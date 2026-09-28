#!/usr/bin/env python3
"""
MyOS autotest: загрузить ОС в QEMU без окна, "понажимать клавиши"
и проверить, что ОС ответила как надо.

    make test
    python3 tools/autotest.py --efi BOOTX64.EFI --kernel kernel.elf \
        --ovmf /usr/share/edk2/x64/OVMF.4m.fd

Как это устроено:
  * загрузочный диск (FAT16 с /EFI/BOOT/BOOTX64.EFI - загрузчик - и
    /EFI/BOOT/KERNEL.ELF - ядро) собирается прямо здесь, на Python -
    mtools/xorriso не нужны;
  * QEMU запускается с -display none, управляется через QMP (JSON-
    протокол QEMU): через него "нажимаются" клавиши;
  * всё, что ОС пишет в COM1, попадает в файл serial.log - по нему
    и проверяется результат. В режиме прошивки туда же пишет сама
    OVMF (её консоль), потом - ядро MyOS (lib/serial.c);
  * в конце - отдельные короткие запуски с нарочными падениями
    (crash write / crash stack): экран паники тоже должен работать.

Выход: 0 - все проверки прошли, 1 - что-то не так (лог сохранён).
"""
import argparse, json, os, re, socket, struct, subprocess, sys, tempfile, time

# ------------------------------------------------------------ FAT16 image


def make_fat_image(efi_path, kernel_path, out_path, total_sectors=65536):
    efi = open(efi_path, 'rb').read()
    kern = open(kernel_path, 'rb').read()
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
    k_cl = alloc(len(kern))
    root = (reserved + nfats * fat_secs) * bps
    img[root:root + 32] = ent(b'EFI        ', 0x10, d_efi, 0)
    o = off(d_efi)
    img[o:o + 96] = ent(b'.          ', 0x10, d_efi, 0) + ent(b'..         ', 0x10, 0, 0) + \
        ent(b'BOOT       ', 0x10, d_boot, 0)
    o = off(d_boot)
    img[o:o + 128] = ent(b'.          ', 0x10, d_boot, 0) + ent(b'..         ', 0x10, d_efi, 0) + \
        ent(b'BOOTX64 EFI', 0x20, f_cl, len(efi)) + ent(b'KERNEL  ELF', 0x20, k_cl, len(kern))
    o = off(f_cl)
    img[o:o + len(efi)] = efi
    o = off(k_cl)
    img[o:o + len(kern)] = kern
    fatb = struct.pack('<%dH' % len(fat), *fat)
    for i in range(nfats):
        p = (reserved + i * fat_secs) * bps
        img[p:p + len(fatb)] = fatb
    open(out_path, 'wb').write(img)

# ------------------------------------------------------------ QEMU + QMP


class VM:
    def __init__(self, qemu, ovmf, disk, workdir, devices, mem='256M', extra=()):
        self.serial = os.path.join(workdir, 'serial.log')
        self.sock = os.path.join(workdir, 'qmp.sock')
        args = [qemu, '-bios', ovmf, '-m', mem, '-display', 'none',
                '-drive', 'format=raw,file=%s,if=ide' % disk,
                '-serial', 'file:' + self.serial,
                '-qmp', 'unix:%s,server=on,wait=off' % self.sock,
                '-no-reboot']
        for d in devices:
            args += ['-device', d]
        args += list(extra)
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

    def mouse_move(self, dx, dy):
        self.cmd('input-send-event', events=[
            {'type': 'rel', 'data': {'axis': 'x', 'value': dx}},
            {'type': 'rel', 'data': {'axis': 'y', 'value': dy}}])

    def log(self):
        try:
            return open(self.serial, 'rb').read().decode('latin-1')
        except FileNotFoundError:
            return ''

    def wait_for(self, text, timeout, since=0):
        """text - подстрока, или 're:...' - регулярное выражение"""
        t0 = time.time()
        while time.time() - t0 < timeout:
            part = self.log()[since:]
            if text.startswith('re:'):
                if re.search(text[3:], part):
                    return True
            elif text in part:
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


def make_stick_image(path):
    """Тестовая "флешка": MBR с разделом FAT32 и метками"""
    img = bytearray(16 * 1024 * 1024)
    img[0:16] = b'MYOS TEST STICK!'
    e = bytearray(16)
    e[4] = 0x0C
    struct.pack_into('<II', e, 8, 2048, 32768 - 2048)
    img[446:462] = e
    img[510], img[511] = 0x55, 0xAA
    img[2048 * 512:2048 * 512 + 11] = b'HELLO MYOS!'
    open(path, 'wb').write(img)


DEFAULT_DEVICES = ['qemu-xhci', 'usb-mouse', 'usb-kbd']


def run_steps(a, work, steps, name, extra=(), devices=None):
    """Один запуск ВМ: пройти шаги, вернуть (ok, текст лога)."""
    disk = os.path.join(work, name + '.img')
    make_fat_image(a.efi, a.kernel, disk)

    vmdir = os.path.join(work, name)
    os.makedirs(vmdir, exist_ok=True)
    extra = [x.replace('@WORK@', work) for x in extra]
    vm = VM(a.qemu, a.ovmf, disk, vmdir, devices or DEFAULT_DEVICES,
            mem=a.mem, extra=(a.extra.split() if a.extra else []) + list(extra))

    ok = True
    for keys, expect, timeout in steps:
        mark = len(vm.log())
        label = '(boot)' if keys is None else '  ...'
        if isinstance(keys, dict):
            # действие вместо клавиш: QMP-команда или движение мыши
            if 'qmp' in keys:
                name, args = keys['qmp']
                vm.cmd(name, **args)
                label = '[qmp %s]' % name
            if 'key' in keys:
                vm.cmd('human-monitor-command',
                       **{'command-line': 'sendkey %s 40' % keys['key']})
                label = '[key %s]' % keys['key']
            if 'mouse' in keys:
                for _ in range(keys['mouse']):
                    vm.mouse_move(5, 3)
                    time.sleep(0.03)
                label = '[mouse]'
        elif keys:
            vm.type(keys)
            label = keys.strip() or '  ...'
        # проверку ищем в новом выводе; первую - во всём логе;
        # пустая проверка - просто действие
        if expect == '':
            time.sleep(timeout)
            found = True
        else:
            found = vm.wait_for(expect, timeout, since=mark if keys else 0)
        print('%-4s %-14s -> %s' % ('PASS' if found else 'FAIL', label, expect))
        if not found:
            ok = False
            break

    log = vm.log()
    vm.quit()
    return ok, log


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('--efi', default='BOOTX64.EFI')
    ap.add_argument('--kernel', default='kernel.elf')
    ap.add_argument('--ovmf', default='/usr/share/edk2/x64/OVMF.4m.fd')
    ap.add_argument('--qemu', default='qemu-system-x86_64')
    ap.add_argument('--keep', action='store_true', help='keep the work dir')
    ap.add_argument('--quick', action='store_true', help='skip the crash runs')
    ap.add_argument('--mem', default='256M', help='RAM for the VM, e.g. 8G')
    ap.add_argument('--extra', default='', help='extra QEMU args, e.g. "-machine q35"')
    a = ap.parse_args()

    work = tempfile.mkdtemp(prefix='myos-test-')

    # (что набрать, чего ждать в логе, таймаут в секундах)
    main_steps = [
        (None, 'entered kmain', 60),
        ('', 'own page tables on', 10),
        ('', "Type 'help'", 30),
        ('', 'all checksums OK', 5),
        ('', 'source: HPET', 5),
        ('acpi\n', 'CPU cores (MADT): 2 enabled', 15),
        ('', 'I/O APIC #0: id', 5),
        ('kinfo\n', 'LAPIC timer: running', 15),
        ('usb\n', 'keyboard (boot protocol)', 15),
        ('', 'mouse (boot protocol)', 5),
        ('mem\n', 'freed: OK', 15),
        ('', 'verified, freed: OK', 5),
        ('vm\n', 'inside the kernel image: OK', 15),
        ('boot\n', 'Loader log', 15),
        ('int3\n', 'handler ran and returned', 15),
        ('calc 6 * 7\n', '42', 15),
        ('time\n', ':', 15),
        ('cpu\n', 're:CPU load \\(last second[^)]*\\): [0-9]\\.[0-9]%', 15),
        ('', 'USB controller (xHCI)', 5),
        # этап 4: потоки
        ('ps\n', 're:idle +(ready|running)', 15),
        ('', 're:usb +waiting .*usb events', 5),
        ('threadtest\n', 'threadtest: OK', 40),
        ('', 'the timer shared the CPU fairly', 5),
        ('sleep 1\n', 'Woke up.', 15),
        ('spin 1\n', 're:switched threads [1-9]', 15),
    ]

    runs = [('main', main_steps, ['-smp', '2'])]

    if not a.quick:
        # USB: хаб, мышь за ним, флешка за ним; потом горячее
        # подключение/отключение через QMP
        runs.append(('usb-tree', [
            (None, "Type 'help'", 90),
            ('', 'USB events: MSI', 5),
            ('usb\n', 'hub: 8 ports', 15),
            ('', 'USB drive, ready', 5),
            ('disk read 2048\n', 'HELLO MYOS!', 20),
            ('disk read 0\n', 'this is an MBR', 20),
            ({'qmp': ('device_add', {'driver': 'usb-mouse', 'bus': 'xhci.0',
                                     'port': '2.3', 'id': 'hm'})},
             're:usb event: .*connected 0627:0001 - HID, active', 20),
            ({'mouse': 20}, '', 1),
            ('usb\n', 're:reports [1-9]', 15),
            ('', 'hot-plug: 1 connected', 5),
            ({'qmp': ('device_del', {'id': 'hm'})},
             're:usb event: .*disconnected 0627:0001', 20),
            ('usb\n', '1 removed', 15),
        ], ['-drive', 'if=none,id=stick,format=raw,file=@WORK@/stick.img'],
           ['qemu-xhci,id=xhci', 'usb-kbd,bus=xhci.0,port=1', 'usb-hub,bus=xhci.0,port=2',
            'usb-storage,bus=xhci.0,port=2.2,drive=stick']))
        # только PS/2: клавиатура по IRQ 1, мышь по IRQ 12
        runs.append(('ps2', [
            (None, "Type 'help'", 90),
            ('', 'keyboard on IRQ 1', 5),
            ('', 'PS/2 mouse/touchpad: found', 5),
            ({'mouse': 20}, '', 1),
            ('cpu\n', 're:PS/2 keyboard \\(IRQ 1\\) +[1-9]', 15),
            ('', 're:PS/2 mouse / touchpad \\(IRQ 12\\) +[1-9]', 5),
        ], [], ['qemu-xhci']))
        # GUI + потоки: долгая команда терминала работает в своём
        # потоке, а GUI живёт (часы идут, мышь двигается)
        runs.append(('gui-threads', [
            (None, "Type 'help'", 90),
            ('start\n', 'Press any key', 15),
            (' ', 'gui: started', 10),
            ('c', 'gui: terminal opened', 10),
            ('sleep 4\n', "gui: job 'SLEEP 4' started", 10),
            ({'mouse': 20}, "gui: mouse moved while 'SLEEP 4' runs", 5),
            ('ps\n', 're:gui: ps: .*TERM-JOB +SLEEPING', 10),
            ('', "re:gui: clock [0-9:]+ [A-Z]+ while 'SLEEP 4' runs", 5),
            ('', 'gui: term: WOKE UP - 4 S', 10),
            ('spin 2\n', "gui: job 'SPIN 2' started", 10),
            ({'mouse': 20}, "gui: mouse moved while 'SPIN 2' runs", 5),
            ('', 're:gui: term: SPUN 2 S', 10),
            ('sleep 30\n', "gui: job 'SLEEP 30' started", 10),
            ({'key': 'esc'}, '', 0.5),
            ({'key': 'esc'}, 'gui: left', 10),
            ('', 'SLEEP CANCELLED', 1),
        ]))
        # чипсет q35: PCIe через ECAM (MCFG), перезагрузка через FADT
        runs.append(('q35', [
            (None, "Type 'help'", 90),
            ('', 'PCIe config space via ECAM', 5),
            ('usb\n', 'mouse (boot protocol)', 15),
            ('acpi\n', 'MyOS uses it', 15),
            ('', "used by 'reboot'", 5),
        ], ['-machine', 'q35']))
        # часовые пояса: часы машины - 15 января 10:00 UTC (зима):
        # Москва 13:00 (UTC+3), Иерусалим 12:00 (IST, UTC+2)
        runs.append(('timezone-winter', [
            (None, "Type 'help'", 90),
            ('tz msk\n', 'Moscow (MSK, UTC+3) - now 13:0', 15),
            ('tz jer\n', 'Jerusalem (IST, UTC+2, winter time) - now 12:0', 15),
            ('time\n', 'Jerusalem (IST', 15),
        ], ['-rtc', 'base=2026-01-15T10:00:00']))
        runs.append(('crash-write', [
            (None, "Type 'help'", 90),
            ('crash write\n', 'Page Fault: WRITE', 15),
            ('', 'READ-ONLY', 5),
            ('', 'kernel CODE', 5),
        ]))
        runs.append(('crash-stack', [
            (None, "Type 'help'", 90),
            ('crash stack\n', 'KERNEL STACK OVERFLOW', 20),
        ]))
        runs.append(('crash-null', [
            (None, "Type 'help'", 90),
            ('crash null\n', 'NULL pointer', 15),
        ]))

    ok = True
    last_log = ''
    make_stick_image(os.path.join(work, 'stick.img'))

    for run in runs:
        name, steps = run[0], run[1]
        extra = run[2] if len(run) > 2 else []
        devices = run[3] if len(run) > 3 else None
        print('--- run: %s' % name)
        r, last_log = run_steps(a, work, steps, name, extra, devices)
        if not r:
            ok = False
            break

    if ok:
        print('\nALL TESTS PASSED')
        if not a.keep:
            import shutil
            shutil.rmtree(work, ignore_errors=True)
    else:
        print('\nFAILED - work dir: %s' % work)
        print('--- last part of the log ---')
        print(last_log[-2500:])
    sys.exit(0 if ok else 1)


if __name__ == '__main__':
    main()
