#!/usr/bin/env python3
"""
MyOS autotest: загрузить ОС в QEMU без окна, "понажимать клавиши"
и проверить, что ОС ответила как надо.

    make test
    python3 tools/autotest.py --efi BOOTX64.EFI --ovmf /usr/share/edk2/x64/OVMF.4m.fd

Как это устроено:
  * загрузочный диск (FAT16 с /EFI/BOOT/BOOTX64.EFI) собирается прямо
    здесь, на Python - mtools/xorriso не нужны;
  * QEMU запускается с -display none, управляется через QMP (JSON-
    протокол QEMU): через него "нажимаются" клавиши;
  * всё, что ОС пишет в COM1, попадает в файл serial.log - по нему
    и проверяется результат. В режиме прошивки туда же пишет сама
    OVMF (её консоль), после `ebs` - наша ОС (lib/serial.c).

Выход: 0 - все проверки прошли, 1 - что-то не так (лог сохранён).
"""
import argparse, json, os, socket, struct, subprocess, sys, tempfile, time

# ------------------------------------------------------------ FAT16 image


def make_fat_image(efi_path, out_path, total_sectors=65536):
    efi = open(efi_path, 'rb').read()
    bps, spc, reserved, nfats, root_entries, fat_secs = 512, 4, 1, 2, 512, 64
    root_secs = root_entries * 32 // bps
    data_start = reserved + nfats * fat_secs + root_secs
    csize = bps * spc
    img = bytearray(total_sectors * bps)
    bs = bytearray(512)
    bs[0:3] = b'\xEB\x3C\x90'
    bs[3:11] = b'MYOSTEST'
    struct.pack_into('<HBHBHHBHHHII', bs, 11, bps, spc, reserved, nfats,
                     root_entries, 0, 0xF8, fat_secs, 63, 255, 0, total_sectors)
    struct.pack_into('<BBBI11s8s', bs, 36, 0x80, 0, 0x29, 0x12345678,
                     b'MYOS       ', b'FAT16   ')
    bs[510:512] = b'\x55\xAA'
    img[0:512] = bs
    fat = [0] * (fat_secs * bps // 2)
    fat[0], fat[1] = 0xFFF8, 0xFFFF
    nxt = [2]

    def alloc(nbytes):
        n = max(1, (nbytes + csize - 1) // csize)
        first = nxt[0]
        for i in range(n):
            fat[first + i] = first + i + 1 if i < n - 1 else 0xFFFF
        nxt[0] += n
        return first

    def off(c):
        return (data_start + (c - 2) * spc) * bps

    def ent(name, attr, cl, size):
        e = bytearray(32)
        e[0:11] = name
        e[11] = attr
        struct.pack_into('<HI', e, 26, cl, size)
        return e

    d_efi, d_boot, f_cl = alloc(csize), alloc(csize), alloc(len(efi))
    root = (reserved + nfats * fat_secs) * bps
    img[root:root + 32] = ent(b'EFI        ', 0x10, d_efi, 0)
    o = off(d_efi)
    img[o:o + 96] = ent(b'.          ', 0x10, d_efi, 0) + ent(b'..         ', 0x10, 0, 0) + \
        ent(b'BOOT       ', 0x10, d_boot, 0)
    o = off(d_boot)
    img[o:o + 96] = ent(b'.          ', 0x10, d_boot, 0) + ent(b'..         ', 0x10, d_efi, 0) + \
        ent(b'BOOTX64 EFI', 0x20, f_cl, len(efi))
    o = off(f_cl)
    img[o:o + len(efi)] = efi
    fatb = struct.pack('<%dH' % len(fat), *fat)
    for i in range(nfats):
        p = (reserved + i * fat_secs) * bps
        img[p:p + len(fatb)] = fatb
    open(out_path, 'wb').write(img)

# ------------------------------------------------------------ QEMU + QMP


class VM:
    def __init__(self, qemu, ovmf, disk, workdir, devices):
        self.serial = os.path.join(workdir, 'serial.log')
        self.sock = os.path.join(workdir, 'qmp.sock')
        args = [qemu, '-bios', ovmf, '-m', '256M', '-display', 'none',
                '-drive', 'format=raw,file=%s,if=ide' % disk,
                '-serial', 'file:' + self.serial,
                '-qmp', 'unix:%s,server=on,wait=off' % self.sock,
                '-no-reboot']
        for d in devices:
            args += ['-device', d]
        self.p = subprocess.Popen(args, stdout=subprocess.DEVNULL,
                                  stderr=subprocess.PIPE)
        for _ in range(100):
            try:
                self.s = socket.socket(socket.AF_UNIX)
                self.s.connect(self.sock)
                break
            except OSError:
                time.sleep(0.1)
        else:
            raise RuntimeError('QEMU did not start: %s' % self.p.stderr.read().decode())
        self.f = self.s.makefile('rw')
        self.f.readline()
        self.cmd('qmp_capabilities')

    def cmd(self, name, **args):
        self.f.write(json.dumps({'execute': name, 'arguments': args}) + '\n')
        self.f.flush()
        while True:
            r = json.loads(self.f.readline())
            if 'return' in r or 'error' in r:
                return r

    KEYS = {' ': 'spc', '\n': 'ret', '-': 'minus', '.': 'dot', '/': 'slash',
            '=': 'equal', ',': 'comma', '*': 'shift-8', '_': 'shift-minus', ':': 'shift-semicolon'}

    def type(self, text):
        for ch in text:
            k = self.KEYS.get(ch)
            if k is None:
                k = ('shift-' + ch.lower()) if ch.isupper() else ch
            self.cmd('human-monitor-command', **{'command-line': 'sendkey %s 40' % k})
            time.sleep(0.07)

    def log(self):
        try:
            return open(self.serial, 'rb').read().decode('latin-1')
        except FileNotFoundError:
            return ''

    def wait_for(self, text, timeout, since=0):
        t0 = time.time()
        while time.time() - t0 < timeout:
            if text in self.log()[since:]:
                return True
            if self.p.poll() is not None:
                return False
            time.sleep(0.2)
        return False

    def quit(self):
        try:
            self.cmd('quit')
        except Exception:
            pass
        try:
            self.p.wait(3)
        except subprocess.TimeoutExpired:
            self.p.kill()

# ------------------------------------------------------------ the test


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('--efi', default='BOOTX64.EFI')
    ap.add_argument('--ovmf', default='/usr/share/edk2/x64/OVMF.4m.fd')
    ap.add_argument('--qemu', default='qemu-system-x86_64')
    ap.add_argument('--keep', action='store_true', help='keep the work dir')
    a = ap.parse_args()

    work = tempfile.mkdtemp(prefix='myos-test-')
    disk = os.path.join(work, 'disk.img')
    make_fat_image(a.efi, disk)

    vm = VM(a.qemu, a.ovmf, disk, work,
            ['qemu-xhci', 'usb-mouse', 'usb-kbd'])

    # (что набрать, чего ждать в логе, таймаут в секундах)
    steps = [
        (None, "Type 'help'", 60),
        ('ebs\n', 'Back to the shell', 90),
        ('kinfo\n', 'LAPIC timer: running', 15),
        ('usb\n', 'keyboard (boot protocol)', 15),
        ('', 'mouse (boot protocol)', 5),
        ('mem\n', 'freed: OK', 15),
        ('int3\n', 'handler ran and returned', 15),
        ('calc 6 * 7\n', '42', 15),
    ]

    ok = True
    for keys, expect, timeout in steps:
        mark = len(vm.log())
        if keys:
            vm.type(keys)
        # проверку ищем в новом выводе; первую - во всём логе
        found = vm.wait_for(expect, timeout, since=mark if keys else 0)
        label = '(boot)' if keys is None else (keys.strip() or '  ...')
        print('%-4s %-14s -> %s' % ('PASS' if found else 'FAIL', label, expect))
        if not found:
            ok = False
            break

    vm.quit()

    log_copy = os.path.join(work, 'serial.log')
    if ok:
        print('\nALL TESTS PASSED')
        if not a.keep:
            for f in os.listdir(work):
                os.unlink(os.path.join(work, f))
            os.rmdir(work)
    else:
        print('\nFAILED - serial log: %s' % log_copy)
        tail = vm.log()[-2000:]
        print('--- last part of the log ---')
        print(tail)
    sys.exit(0 if ok else 1)


if __name__ == '__main__':
    main()
