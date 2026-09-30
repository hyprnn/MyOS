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
    (kpanic write / kpanic stack): экран паники тоже должен работать.

Выход: 0 - все проверки прошли, 1 - что-то не так (лог сохранён).
"""
import argparse, base64, json, os, re, socket, struct, subprocess, sys, tempfile, threading, time, zlib
import http.server, urllib.request
from shutil import which as shutil_which
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import fatimg

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
            '=': 'equal', ',': 'comma', '*': 'shift-8', '_': 'shift-minus', ':': 'shift-semicolon',
            '"': 'shift-apostrophe', "'": 'apostrophe', '~': 'shift-grave_accent',
            '(': 'shift-9', ')': 'shift-0', '+': 'shift-equal', '!': 'shift-1',
            '?': 'shift-slash', '%': 'shift-5'}

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


BIG_DATA = bytes((i * 7) & 255 for i in range(300000))


def make_test_disk(path, mib, bits, scheme, label, extra=None):
    """Диск с FAT и файлами "как с Linux": короткое и длинное имя,
    папка, большой файл (300 000 байт, много кластеров)"""
    b = fatimg.FatBuilder(mib * 2048 - 4096, bits, label)
    b.add_file(b.root, 'HOST.TXT', b'Hello from the host!\n')
    b.add_file(b.root, 'Long File Name From Linux.txt', b'long name ok\n')
    d = b.mkdir(b.root, 'docs')
    b.add_file(d, 'readme.md', b'# readme\n')
    b.add_file(b.root, 'BIG.BIN', BIG_DATA)
    if os.path.exists('build/user/hello'):
        a = b.mkdir(b.root, 'apps')
        b.add_file(a, 'hello', open('build/user/hello', 'rb').read())
    for name, data in (extra or {}).items():
        b.add_file(b.root, name, data)
    fatimg.make_disk(path, mib, b.build(), scheme)


# ------------------------------------------------------------ exFAT (как флешка Ventoy)

EXFAT_P1 = 2048                 # раздел 1 (exFAT) - с сектора 2048
EXFAT_P1_SIZE = 96 * 2048       # 96 МиБ


def build_exfattool(work):
    """tools/exfattool.c + FatFs -> утилита хоста (класть/доставать
    файлы в образе exFAT). None - нет gcc."""
    root = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
    out = os.path.join(work, 'exfattool')
    r = subprocess.run(['gcc', '-O2', '-I' + os.path.join(root, 'third_party/fatfs'),
                        '-DFFCONF_H="ffconf.h"', '-o', out, os.path.join(root, 'tools/exfattool.c'),
                        os.path.join(root, 'third_party/fatfs/ff.c'),
                        os.path.join(root, 'third_party/fatfs/ffunicode.c')],
                       capture_output=True)
    return out if r.returncode == 0 else None


def make_ventoy_disk(path, tool, work):
    """Флешка как у Ventoy: MBR, раздел 1 exFAT (тип 0x07) с файлами,
    раздел 2 - маленький FAT16 (тип 0xEF). Нужен mkfs.exfat."""
    p1 = os.path.join(work, 'exfat-p1.img')
    open(p1, 'wb').truncate(EXFAT_P1_SIZE * 512)
    subprocess.run(['mkfs.exfat', '-L', 'Ventoy', p1], check=True, capture_output=True)
    for name, data in (('/HOST.TXT', b'Hello from exFAT!\n'), ('/ISO/big.bin', BIG_DATA)):
        src = os.path.join(work, 'exfat-src.bin')
        open(src, 'wb').write(data)
        if name.startswith('/ISO/'):
            subprocess.run([tool, p1, '0', 'mkdir', '/ISO'], capture_output=True)
        subprocess.run([tool, p1, '0', 'put', src, name], check=True)
    p2_start = EXFAT_P1 + EXFAT_P1_SIZE
    p2_size = 16 * 2048
    b = fatimg.FatBuilder(p2_size - 4096, 16, 'VTOYEFI')
    b.add_file(b.root, 'ventoy.txt', b'ventoy efi\n')
    fat = b.build()
    img = bytearray((p2_start + p2_size) * 512)
    img[EXFAT_P1 * 512:(EXFAT_P1 + EXFAT_P1_SIZE) * 512] = open(p1, 'rb').read()
    img[p2_start * 512:p2_start * 512 + len(fat)] = fat
    for i, (t, st, n) in enumerate(((0x07, EXFAT_P1, EXFAT_P1_SIZE), (0xEF, p2_start, p2_size))):
        e = 446 + 16 * i
        img[e + 4] = t
        struct.pack_into('<II', img, e + 8, st, n)
    img[510:512] = b'\x55\xaa'
    open(path, 'wb').write(img)


def check_exfat(path, tool, work, expect):
    """fsck.exfat (независимая проверка) + содержимое файлов"""
    problems = []
    p1 = os.path.join(work, 'exfat-check.img')
    data = open(path, 'rb').read()
    open(p1, 'wb').write(data[EXFAT_P1 * 512:(EXFAT_P1 + EXFAT_P1_SIZE) * 512])
    r = subprocess.run(['fsck.exfat', '-n', p1], capture_output=True, text=True)
    if r.returncode != 0:
        problems.append('fsck.exfat: ' + (r.stdout + r.stderr).strip()[-300:])
    for name, want in expect.items():
        g = subprocess.run([tool, path, str(EXFAT_P1), 'get', name], capture_output=True)
        got = g.stdout if g.returncode == 0 else None
        if want is None and got is not None:
            problems.append('%s should be gone' % name)
        elif want is not None and got != want:
            problems.append('%s: wrong content (%s bytes)' % (name, len(got) if got else 'no'))
    return problems


def check_disk(path, expect):
    """Проверить образ НЕЗАВИСИМЫМ читателем FAT (как это сделал бы
    Linux): целостность + ожидаемые файлы. expect: {путь: байты или
    None (файла быть не должно)}"""
    img = bytearray(open(path, 'rb').read())
    parts = fatimg.partitions(img)
    if not parts:
        return ['no FAT volume found']
    r = fatimg.FatReader(img, parts[0][0])
    problems = r.fsck()
    for p, want in expect.items():
        got = r.read(p)
        if want is None:
            if got is not None:
                problems.append('%s should be gone' % p)
        elif got != want:
            problems.append('%s: expected %r..., got %r...' % (p, want[:30], (got or b'')[:30]))
        elif p.rsplit('/', 1)[-1] not in [e[0] for e in r.listdir(r.lookup(p.rsplit('/', 1)[0] or '/')[2])]:
            problems.append('%s: the exact name (case) is not in the folder' % p)
    return problems


# ------------------------------------------------------------ сеть (этап 8)

HOST_HELLO = b'Hello from the host over HTTP!\n'


def free_port():
    s = socket.socket()
    s.bind(('127.0.0.1', 0))
    port = s.getsockname()[1]
    s.close()
    return port


# Тестовая страница для браузера (этап 9): заголовок нужного цвета,
# картинки PNG (жёлтый круг) и JPEG (малиновый прямоугольник), русский
# текст. Автотест потом ищет эти цвета на снимке экрана.
BROWSER_H1 = (0x1a, 0x5f, 0xb4)
BROWSER_SUN = (255, 200, 0)
BROWSER_JPEG = (200, 30, 160)
BROWSER_JPEG_DATA = base64.b64decode('''
/9j/4AAQSkZJRgABAQAAAQABAAD/2wBDAAMCAgMCAgMDAwMEAwMEBQgFBQQEBQoHBwYIDAoMDAsK
CwsNDhIQDQ4RDgsLEBYQERMUFRUVDA8XGBYUGBIUFRT/2wBDAQMEBAUEBQkFBQkUDQsNFBQUFBQU
FBQUFBQUFBQUFBQUFBQUFBQUFBQUFBQUFBQUFBQUFBQUFBQUFBQUFBQUFBT/wAARCAA8AFADASIA
AhEBAxEB/8QAHwAAAQUBAQEBAQEAAAAAAAAAAAECAwQFBgcICQoL/8QAtRAAAgEDAwIEAwUFBAQA
AAF9AQIDAAQRBRIhMUEGE1FhByJxFDKBkaEII0KxwRVS0fAkM2JyggkKFhcYGRolJicoKSo0NTY3
ODk6Q0RFRkdISUpTVFVWV1hZWmNkZWZnaGlqc3R1dnd4eXqDhIWGh4iJipKTlJWWl5iZmqKjpKWm
p6ipqrKztLW2t7i5usLDxMXGx8jJytLT1NXW19jZ2uHi4+Tl5ufo6erx8vP09fb3+Pn6/8QAHwEA
AwEBAQEBAQEBAQAAAAAAAAECAwQFBgcICQoL/8QAtREAAgECBAQDBAcFBAQAAQJ3AAECAxEEBSEx
BhJBUQdhcRMiMoEIFEKRobHBCSMzUvAVYnLRChYkNOEl8RcYGRomJygpKjU2Nzg5OkNERUZHSElK
U1RVVldYWVpjZGVmZ2hpanN0dXZ3eHl6goOEhYaHiImKkpOUlZaXmJmaoqOkpaanqKmqsrO0tba3
uLm6wsPExcbHyMnK0tPU1dbX2Nna4uPk5ebn6Onq8vP09fb3+Pn6/9oADAMBAAIRAxEAPwDyqiii
v2A/sUKKKKACiiigAooooAKKKKACiiigAooooAKKKKACiiigAooooAKKKKACiiigAooooAKKKKAC
iiigAooooAKKKKACiiigAooooAKKKKAP/9k=
''')
BROWSER_PAGE = ("<!DOCTYPE html><html><head><meta charset='utf-8'><title>Тест MyOS</title>"
                "<style>body{background:#fff;font-family:sans-serif}h1{color:#1a5fb4;font-size:40px}"
                "</style></head><body><h1>Браузер MyOS: Привет!</h1>"
                "<p>Русский текст, <b>жирный</b>, ссылка <a href='hello.txt'>hello</a>.</p>"
                "<p><img src='sun.png' width='120' height='120'> <img src='box.jpg'></p>"
                "</body></html>").encode('utf-8')


def make_png(w, h, pixel):
    """PNG без сторонних библиотек: pixel(x, y) -> (r, g, b)"""
    raw = b''.join(b'\0' + bytes(c for x in range(w) for c in pixel(x, y)) for y in range(h))

    def chunk(t, d):
        return struct.pack('>I', len(d)) + t + d + struct.pack('>I', zlib.crc32(t + d) & 0xffffffff)
    return (b'\x89PNG\r\n\x1a\n' + chunk(b'IHDR', struct.pack('>IIBBBBB', w, h, 8, 2, 0, 0, 0)) +
            chunk(b'IDAT', zlib.compress(raw)) + chunk(b'IEND', b''))


def start_host_http(folder):
    """HTTP-сервер на хосте: из QEMU (-netdev user) он виден как
    10.0.2.2:порт. Раздаёт hello.txt и big.bin (BIG_DATA), страницу для
    браузера (page.html, sun.png, box.jpg)."""
    os.makedirs(folder, exist_ok=True)
    open(os.path.join(folder, 'hello.txt'), 'wb').write(HOST_HELLO)
    open(os.path.join(folder, 'big.bin'), 'wb').write(BIG_DATA)
    open(os.path.join(folder, 'page.html'), 'wb').write(BROWSER_PAGE)
    open(os.path.join(folder, 'box.jpg'), 'wb').write(BROWSER_JPEG_DATA)
    open(os.path.join(folder, 'sun.png'), 'wb').write(make_png(
        120, 120, lambda x, y: BROWSER_SUN if (x - 60) ** 2 + (y - 60) ** 2 < 50 ** 2
        else (255, 255, 255)))

    class Quiet(http.server.SimpleHTTPRequestHandler):
        def __init__(self, *args, **kw):
            super().__init__(*args, directory=folder, **kw)

        def log_message(self, *args):
            pass

        def do_GET(self):
            # /to-https -> переадресация на HTTPS-сервер (как делают сайты)
            if self.path == '/to-https':
                self.send_response(301)
                self.send_header('Location', 'https://10.0.2.2:%d/hello.txt' % HTTPS_PORT[0])
                self.send_header('Content-Length', '0')
                self.end_headers()
                return
            super().do_GET()

    srv = http.server.ThreadingHTTPServer(('127.0.0.1', 0), Quiet)
    threading.Thread(target=srv.serve_forever, daemon=True).start()
    return srv.server_address[1]


HTTPS_PORT = [0]
TLS_DIR = os.path.join(os.path.dirname(os.path.abspath(__file__)), 'tls-test')


def start_host_https(folder):
    """HTTPS-сервер на хосте с тестовым сертификатом (tools/tls-test,
    ECDSA, для 10.0.2.2) - те же файлы, что у HTTP-сервера. На него
    ведёт переадресация /to-https HTTP-сервера."""
    import ssl

    class Quiet(http.server.SimpleHTTPRequestHandler):
        def __init__(self, *args, **kw):
            super().__init__(*args, directory=folder, **kw)

        def log_message(self, *args):
            pass

    srv = http.server.ThreadingHTTPServer(('127.0.0.1', 0), Quiet)
    ctx = ssl.SSLContext(ssl.PROTOCOL_TLS_SERVER)
    ctx.load_cert_chain(os.path.join(TLS_DIR, 'server.pem'), os.path.join(TLS_DIR, 'server.key'))
    srv.socket = ctx.wrap_socket(srv.socket, server_side=True)
    threading.Thread(target=srv.serve_forever, daemon=True).start()
    return srv.server_address[1]


DEFAULT_DEVICES = ['qemu-xhci', 'usb-mouse', 'usb-kbd']


def count_colors(ppm, colors, tols):
    """Сколько точек снимка (PPM P6 от QEMU) близки к каждому из цветов"""
    data = open(ppm, 'rb').read()
    parts = data.split(b'\n', 3)          # P6, "w h", 255, пиксели
    w, h = (int(v) for v in parts[1].split())
    px = parts[3]
    counts = [0] * len(colors)
    for i in range(0, w * h * 3, 3):
        r, g, b = px[i], px[i + 1], px[i + 2]
        for k, (cr, cg, cb) in enumerate(colors):
            t = tols[k]
            if abs(r - cr) <= t and abs(g - cg) <= t and abs(b - cb) <= t:
                counts[k] += 1
    return counts


# прогоны со своим загрузочным диском: имя прогона -> файл образа
CUSTOM_BOOT_DISKS = {}

SYSTEMD_BOOT = '/usr/lib/systemd/boot/efi/systemd-bootx64.efi'


def make_install_disk(a, work):
    """Диск "как у ноутбука с Arch" (этап 9): раздел EFI с systemd-boot
    (меню скрыто, по умолчанию - MyOS), MyOS поставлена туда скриптом
    tools/install-arch.sh - в EFI/MyOS/ и loader/entries/myos.conf.
    Нужны systemd-boot и mtools; нет - None (прогон пропускается)."""
    if not (os.path.exists(SYSTEMD_BOOT) and shutil_which('mformat') and shutil_which('mcopy')):
        return None
    root = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
    esp = os.path.join(work, 'install-esp')
    src = os.path.join(work, 'install-src')
    for d in (esp + '/EFI/BOOT', esp + '/EFI/systemd', esp + '/loader/entries', src):
        os.makedirs(d, exist_ok=True)
    boot = open(SYSTEMD_BOOT, 'rb').read()
    open(esp + '/EFI/BOOT/BOOTX64.EFI', 'wb').write(boot)
    open(esp + '/EFI/systemd/systemd-bootx64.efi', 'wb').write(boot)
    open(esp + '/loader/loader.conf', 'w').write('default myos.conf\ntimeout 0\n')
    open(esp + '/loader/entries/arch.conf', 'w').write('title Arch Linux\nlinux /vmlinuz-linux\n')
    open(src + '/BOOTX64.EFI', 'wb').write(open(a.efi, 'rb').read())
    open(src + '/KERNEL.ELF', 'wb').write(open(a.kernel, 'rb').read())
    r = subprocess.run([os.path.join(root, 'tools/install-arch.sh'), '--esp', esp, '--from', src],
                       capture_output=True, text=True)
    if r.returncode != 0:
        print(r.stdout, r.stderr)
        return None
    img = os.path.join(work, 'install.img')
    with open(img, 'wb') as f:
        f.truncate(64 * 1024 * 1024)
    subprocess.run(['mformat', '-i', img, '-F', '-T', str(64 * 2048), '-h', '64', '-s', '32', '::'],
                   check=True)
    subprocess.run(['mcopy', '-s', '-i', img, esp + '/EFI', esp + '/loader', '::/'], check=True)
    return img


def run_steps(a, work, steps, name, extra=(), devices=None):
    """Один запуск ВМ: пройти шаги, вернуть (ok, текст лога)."""
    disk = os.path.join(work, name + '.img')
    if name in CUSTOM_BOOT_DISKS:
        disk = CUSTOM_BOOT_DISKS[name]      # свой загрузочный диск (install)
    else:
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
            if 'goto' in keys:
                # курсор GUI - в точку экрана: сначала в угол, потом
                # шагами (мышь QEMU относительная, 1 шаг = 1 пиксель)
                for _ in range(40):
                    vm.mouse_move(-50, -50)
                    time.sleep(0.02)
                x, y = keys['goto']
                while x > 0 or y > 0:
                    dx, dy = min(x, 20), min(y, 20)
                    vm.mouse_move(dx, dy)
                    x -= dx
                    y -= dy
                    time.sleep(0.02)
                time.sleep(0.3)
                label = '[goto %d,%d]' % keys['goto']
            if 'click' in keys:
                for _ in range(keys['click']):
                    for d in (True, False):
                        vm.cmd('input-send-event', events=[
                            {'type': 'btn', 'data': {'down': d, 'button': 'left'}}])
                        time.sleep(0.15)
                    time.sleep(0.5)
                label = '[click x%d]' % keys['click']
            if 'mouse' in keys:
                for _ in range(keys['mouse']):
                    vm.mouse_move(5, 3)
                    time.sleep(0.03)
                label = '[mouse]'
            if 'screen' in keys:
                # снимок экрана QEMU: есть ли на нём нужные цвета (цвет,
                # допуск, сколько точек минимум) - ждём до timeout секунд
                want = keys['screen']
                label = '[screen]'
                shot = os.path.join(vmdir, 'screen.ppm')
                t0, good, counts = time.time(), False, []
                while time.time() - t0 < timeout:
                    vm.cmd('screendump', filename=shot)
                    time.sleep(0.5)
                    counts = count_colors(shot, [c for c, t, n in want], [t for c, t, n in want])
                    if all(counts[i] >= want[i][2] for i in range(len(want))):
                        good = True
                        break
                    time.sleep(1)
                print('%-4s %-14s -> %s' % ('PASS' if good else 'FAIL', label,
                      ', '.join('#%02x%02x%02x x%d (need %d)' % (c + (counts[i] if counts else 0, n))
                                for i, (c, t, n) in enumerate(want))))
                if not good:
                    ok = False
                    break
                continue
            if 'http' in keys:
                # хост скачивает страницу у httpd внутри MyOS (hostfwd)
                port, path, want = keys['http']
                label = '[host GET %s]' % path
                try:
                    got = urllib.request.urlopen('http://127.0.0.1:%d%s' % (port, path),
                                                 timeout=60).read()
                except Exception as e:
                    got = ('error: %s' % e).encode()
                good = (got == want) if len(want) > 64 else (want in got)
                if not good:
                    print('FAIL %-14s -> got %d bytes: %r' % (label, len(got), got[:60]))
                    ok = False
                    break
        elif keys:
            vm.type(keys)
            label = keys.strip() or '  ...'
        # проверку ищем в новом выводе; первую - во всём логе;
        # пустая проверка - просто действие
        if expect == '':
            time.sleep(timeout)
            found = True
        elif expect == '@exit':
            # машина должна выключиться сама (QEMU завершается)
            t0 = time.time()
            while vm.p.poll() is None and time.time() - t0 < timeout:
                time.sleep(0.2)
            found = vm.p.poll() is not None
            expect = '(QEMU powered off)'

        else:
            found = vm.wait_for(expect, timeout, since=mark if keys else 0)
        print('%-4s %-14s -> %s' % ('PASS' if found else 'FAIL', label,
                                    expect.replace('\r', '\\r').replace('\n', '\\n')))
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
    ap.add_argument('--only', default='', help='run only these runs (comma-separated names)')
    ap.add_argument('--internet', action='store_true',
                    help='also check real DNS and HTTP (needs Internet on the host)')
    a = ap.parse_args()

    work = tempfile.mkdtemp(prefix='myos-test-')
    exfat_tool = None

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
        ('ls /\n', 'RAM disk', 15),
        ('write memo.txt hi\n', 'memo.txt is now 3 bytes', 15),
        ('cat memo.txt\n', 'hi', 15),
        ('', 'USB controller (xHCI)', 5),
        # этап 4: потоки
        ('ps\n', 're:idle +(ready|running)', 15),
        ('', 're:usb +waiting .*usb events', 5),
        ('threadtest\n', 'threadtest: OK', 40),
        ('', 'the timer shared the CPU fairly', 5),
        ('sleep 1\n', 'Woke up.', 15),
        ('spin 1\n', 're:switched threads [1-9]', 15),
        # этап 6: программы в ring 3
        ('ls /bin\n', 'primes', 15),
        ('hello one two\n', 'Hello from ring 3!', 15),
        ('', 'My arguments: [one] [two]', 5),
        ('calc 2*(3+4)\n', '14', 15),
        ('crash\n', 're:crash \\(pid [0-9]+\\) was stopped: Page Fault: READ of 0x0+ ', 15),
        ('crash kernel\n', 'tried to touch KERNEL memory', 15),
        ('crash cli\n', 'only the kernel may do that', 15),
        ('crash div\n', 'Division', 15),
        ('crash stack\n', 'was stopped: Page Fault: WRITE', 20),
        ('crash loop\n', 'press Ctrl+C', 15),
        ({'key': 'ctrl-c'}, 'stopped with Ctrl+C', 10),
        ('primes 100000\n', '9592 primes', 30),
        ('hello\n', 'Hello from ring 3!', 15),
        ('mem\n', 'freed: OK', 15),
        # этап 9: полная libc (picolibc) - printf с дробями, FILE *, math...
        ('libctest\n', 're:libctest: [0-9]+/[0-9]+ OK', 30),
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
            ('disk read 2048\n', 'FAT boot sector', 20),
            ('disk read 0\n', 'this is an MBR', 20),
            ('cat /usb0p1/host.txt\n', 'Hello from the host!', 20),
            ('libctest /usb0p1\n', 're:libctest: [0-9]+/[0-9]+ OK', 40),
            ({'qmp': ('device_add', {'driver': 'usb-mouse', 'bus': 'xhci.0',
                                     'port': '2.3', 'id': 'hm'})},
             're:usb event: .*connected 0627:0001 - HID, active', 20),
            ({'mouse': 20}, '', 1),
            ('usb\n', 're:reports [1-9]', 15),
            ('', 'hot-plug: 1 connected', 5),
            ({'qmp': ('device_del', {'id': 'hm'})},
             're:usb event: .*disconnected 0627:0001', 20),
            ('usb\n', '1 removed', 15),
        ], ['-drive', 'if=none,id=stick,format=raw,file=@WORK@/tree.img'],
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
        # Рабочий стол (этап 7): композитор, окна, окно программы.
        # Экран теста 1280x800; панель задача снизу, кнопка "Пуск" слева.
        # Меню "Пуск" открывается вверх; пункт "Часы" - 6-й (clock),
        # запускает программу, у неё своё окно (winproc в COM1).
        runs.append(('desktop', [
            (None, "Type 'help'", 90),
            ('start\n', 're:wm: started', 20),
            ('', "re:wm: window 1 .* opened", 5),
            # открыть меню "Пуск"
            ({'goto': (20, 786)}, '', 0.3),
            ({'click': 1}, '', 0.4),
            # пункт "Терминал" (1-й, y=549..569): курсор в (60,559)
            ({'goto': (60, 559)}, '', 0.3),
            ({'click': 1}, "re:wm: window .* opened", 8),
            # снова меню -> "Часы" (6-й пункт, y=549+5*20=649..669)
            ({'goto': (20, 786)}, '', 0.3),
            ({'click': 1}, '', 0.4),
            ({'goto': (60, 655)}, '', 0.3),
            ({'click': 1}, 're:wm: launch clock', 8),
            ('', 're:winproc: pid [0-9]+ got window', 10),
            # мышь двигается - композитор жив
            ({'mouse': 15}, '', 1),
            # выход из рабочего стола: меню "Пуск" -> "Выход" (11-й, y=549+10*20=749..769)
            ({'goto': (20, 786)}, '', 0.3),
            ({'click': 1}, '', 0.4),
            ({'goto': (60, 755)}, '', 0.3),
            ({'click': 1}, 'Left the desktop', 10),
        ]))
        # диски и файлы (этап 5): флешка (MBR+FAT32), SATA-диск через
        # AHCI (GPT+FAT16), NVMe (GPT+FAT32); файлы с длинными именами,
        # папки, копирование между дисками, горячее подключение флешки
        disks = ['-drive', 'if=none,id=stick,format=raw,file=@WORK@/stick.img',
                 '-drive', 'if=none,id=stick2,format=raw,file=@WORK@/stick2.img',
                 '-device', 'ahci,id=ahci0',
                 '-drive', 'if=none,id=sd,format=raw,file=@WORK@/sata.img',
                 '-device', 'ide-hd,drive=sd,bus=ahci0.0',
                 '-drive', 'if=none,id=nv,format=raw,file=@WORK@/nvme.img',
                 '-device', 'nvme,serial=MYOS1,drive=nv']
        ddev = ['qemu-xhci,id=xhci', 'usb-kbd,bus=xhci.0,port=1',
                'usb-storage,bus=xhci.0,port=2,drive=stick']
        runs.append(('storage', [
            (None, "Type 'help'", 90),
            ('', 'mounted /usb0p1: FAT32, label "STICK"', 5),
            ('', 'mounted /sata0p1: FAT16, label "SATADISK"', 5),
            ('', 'mounted /nvme0p1: FAT32, label "NVME"', 5),
            ('disk\n', 'sata0p1', 15),
            ('ls /\n', 're:/usb0p1 +FAT32 STICK', 15),
            ('cd /usb0p1\n', '/usb0p1>', 10),
            ('ls\n', 'Long File Name From Linux.txt', 15),
            ('cat host.txt\n', 'Hello from the host!', 15),
            ('cat "Long File Name From Linux.txt"\n', 'long name ok', 15),
            ('mkdir myos\n', '/usb0p1>', 10),
            ('cd myos\n', '/usb0p1/myos>', 10),
            ('write hello.txt Hello from MyOS\n', 'is now 16 bytes', 15),
            ('append hello.txt second line\n', 'is now 28 bytes', 15),
            ('cat hello.txt\n', 'second line', 15),
            ('write "Mixed Case Name.txt" mixed\n', 'is now 6 bytes', 15),
            ('ls\n', 'Mixed Case Name.txt', 15),
            ('cp /usb0p1/BIG.BIN /nvme0p1/copy.bin\n', 'Copied, 300000 bytes', 30),
            ('cp hello.txt /sata0p1/\n', 'Copied, 28 bytes', 15),
            ('mkdir /nvme0p1/a\n', '>', 10),
            ('mkdir /nvme0p1/a/b\n', '>', 10),
            ('mv /nvme0p1/copy.bin /nvme0p1/a/b\n', 'Moved.', 15),
            ('mv /sata0p1/hello.txt /sata0p1/renamed.txt\n', 'Moved.', 15),
            ('mv /usb0p1/docs/readme.md /nvme0p1/\n', 'Moved to another disk', 15),
            ('rmdir /usb0p1/docs\n', 'Deleted.', 15),
            ('rm /usb0p1/host.txt\n', 'Deleted.', 15),
            ('write /ram/r.txt ram\n', 'is now 4 bytes', 15),
            ('cp /ram/r.txt /usb0p1/fromram.txt\n', 'Copied, 4 bytes', 15),
            ('df\n', 're:/nvme0p1 +FAT32', 15),
            ('mkdir /nvme0p1/x\n', '>', 10),
            ('write /nvme0p1/x/inner.txt deep\n', 'is now 5 bytes', 15),
            ('mv /nvme0p1/x /nvme0p1/a\n', 'Moved.', 15),
            ('cd /nvme0p1/a/x/../b\n', '/nvme0p1/a/b>', 10),
            ('edit /usb0p1/notes.txt\n', 'Editing', 15),
            ('line one\n', ': ', 5),
            ('line two\n', ': ', 5),
            ('.\n', 'Saved: 2 line(s), 18 bytes', 15),
            ('ls /sata0p1/nope\n', 'no such file or folder', 15),
            # программа - файлом на флешке (этап 6)
            ('/usb0p1/apps/hello from-disk\n', 'Hello from ring 3!', 15),
            ('', '[from-disk]', 5),
            # вторая флешка - на лету: без таблицы разделов (FAT16 с сектора 0)
            ({'qmp': ('device_add', {'driver': 'usb-storage', 'bus': 'xhci.0', 'port': '4',
                                     'drive': 'stick2', 'id': 's2'})},
             're:usb event: .*connected', 20),
            ('ls /\n', 're:/usb1 +FAT16 STICK2', 20),
            ('cd /usb1\n', '/usb1>', 10),
            ('cat host.txt\n', 'Hello from the host!', 15),
            ({'qmp': ('device_del', {'id': 's2'})}, 're:usb event: .*disconnected', 20),
            ('ls /\n', '/usb1 is gone', 15),
            ('pwd\n', 're:\n/\r?\n', 10),
        ], disks, ddev))
        # "перезагрузка": тот же диск - файлы, записанные в прошлый раз,
        # на месте; потом Проводник GUI видит флешку
        runs.append(('storage-reboot', [
            (None, "Type 'help'", 90),
            ('cat /usb0p1/myos/hello.txt\n', 'Hello from MyOS', 20),
            ('ls /nvme0p1/a/b\n', 'copy.bin', 15),
            # рабочий стол видит флешку: открыть Проводник и зайти в /usb0p1
            ('start\n', 're:wm: started', 20),
            ({'goto': (20, 786)}, '', 0.3),
            ({'click': 1}, '', 0.4),
            ({'goto': (60, 615)}, '', 0.3),
            ({'click': 1}, 're:wm: launch explorer', 8),
        ], disks, ddev))
        # чипсет q35: PCIe через ECAM (MCFG), перезагрузка через FADT
        runs.append(('q35', [
            (None, "Type 'help'", 90),
            ('', 'PCIe config space via ECAM', 5),
            ('usb\n', 'mouse (boot protocol)', 15),
            ('acpi\n', 'MyOS uses it', 15),
            # на q35 загрузочный диск - на AHCI: FAT16 без таблицы разделов
            ('ls /\n', 're:/sata0 +FAT16', 15),
            ('ls /sata0/efi/boot\n', 'KERNEL.ELF', 15),
            ('', "used by 'reboot'", 5),
        ], ['-machine', 'q35']))
        # сеть (этап 8): e1000 (QEMU pc по умолчанию), DHCP от QEMU
        # (-netdev user: шлюз 10.0.2.2 = хост, DNS 10.0.2.3), ping,
        # TCP/UDP через петлю, wget с HTTP-сервера на хосте (в том числе
        # с потерей каждого 10-го кадра), скачивание на флешку (потом
        # файл проверяется), httpd внутри MyOS (хост скачивает у него),
        # адрес вручную и снова DHCP, самопроверка WPA2
        hp = start_host_http(os.path.join(work, 'www'))
        HTTPS_PORT[0] = hsp = start_host_https(os.path.join(work, 'www'))
        fwd = free_port()
        crc = 'CRC-32 %08x' % zlib.crc32(BIG_DATA)
        url = 'http://10.0.2.2:%d' % hp
        net_steps = [
            (None, "Type 'help'", 90),
            ('', 'eth0: Intel 82540EM (e1000)', 5),
            ('', 'dhcp: eth0: address 10.0.2.15', 20),
            ('ifconfig\n', 'inet 10.0.2.15/24  gateway 10.0.2.2', 15),
            ('ping -c 3 10.0.2.2\n', '3 packets transmitted, 3 received', 20),
            ('ping -c 1 127.0.0.1\n', '1 packets transmitted, 1 received', 15),
            ('nettest\n', 'nettest: OK', 40),
            ('nslookup localhost\n', 'Address: 127.0.0.1', 15),
            ('wget -O - %s/hello.txt\n' % url, 'Hello from the host over HTTP!', 20),
            ('wget -O /usb0p1/dl.bin %s/big.bin\n' % url, crc, 40),
            # HTTPS (TLS на BearSSL): чужой сертификат - отказ; с корнем
            # тестового центра - проверка проходит, файл - на флешку;
            # переадресация http -> https; режим без проверки
            ('wget -O - https://10.0.2.2:%d/hello.txt\n' % hsp,
             'not signed by a known authority', 30),
            ('wget --ca-certificate /usb0p1/ca.pem -O /usb0p1/tls.bin https://10.0.2.2:%d/big.bin\n'
             % hsp, crc, 60),
            ('', 're:TLS 1\\.2, ECDHE-ECDSA', 5),
            ('wget --ca-certificate /usb0p1/ca.pem -O - %s/to-https\n' % url,
             'Hello from the host over HTTP!', 60),
            ('', 'Redirected to https://', 5),
            ('wget --no-check-certificate -O - https://10.0.2.2:%d/hello.txt\n' % hsp,
             'Hello from the host over HTTP!', 60),
            # запись не удалась посреди HTTPS (RAM-диск мал) - wget обязан
            # сразу завершиться (на ноутбуке он зависал в br_sslio_close)
            ('wget --ca-certificate /usb0p1/ca.pem -O /ram/big.bin https://10.0.2.2:%d/big.bin\n'
             % hsp, 'keeps only small files', 60),
            ('', 'wget exited with code 1', 10),
            ('wget -0 - https://10.0.2.2:%d/hello.txt\n' % hsp, 'did you mean -O', 15),
            # этап 9: curl (libcurl на picolibc: сокеты BSD, poll, неблокирующий
            # connect, BearSSL) - HTTP, файл на флешку, HTTPS: чужой сертификат
            # против встроенных корней MyOS, свой корень, без проверки,
            # переадресация на https
            ('curl -s %s/hello.txt\n' % url, 'Hello from the host over HTTP!', 30),
            ('curl -s -o /usb0p1/c.bin %s/big.bin\n' % url, 'exited with code 0', 60),
            ('ls /usb0p1\n', 're:%d +c\\.bin' % len(BIG_DATA), 15),
            # (curl с BearSSL сверяет сертификат только с именем, не с
            # IP-адресом - поэтому имя из сертификата через --resolve)
            ('curl -sS --resolve myos-test.local:%d:10.0.2.2 https://myos-test.local:%d/hello.txt\n'
             % (hsp, hsp), 'curl: (60)', 30),
            ('curl -s --cacert /usb0p1/ca.pem --resolve myos-test.local:%d:10.0.2.2 '
             'https://myos-test.local:%d/hello.txt\n' % (hsp, hsp),
             'Hello from the host over HTTP!', 30),
            ('curl -sk https://10.0.2.2:%d/hello.txt\n' % hsp, 'Hello from the host over HTTP!', 30),
            ('curl -sLk %s/to-https\n' % url, 'Hello from the host over HTTP!', 30),
            ('net drop 10\n', 'every 10-th received frame', 10),
            ('wget -O null %s/big.bin\n' % url, crc, 90),
            ('net drop 0\n', 'Test mode off', 10),
            ('httpd 80 /usb0p1 -n 2\n', 'httpd: serving', 15),
            ({'http': (fwd, '/', b'BIG.BIN')}, 'httpd: #1', 20),
            ({'http': (fwd, '/BIG.BIN', BIG_DATA)}, 'httpd: #2', 60),
            ('', 'exited with code 0', 10),
            ('ifconfig eth0 10.0.2.77/24 gw 10.0.2.2 dns 10.0.2.3\n', 'eth0: done', 15),
            ('ifconfig eth0\n', 'inet 10.0.2.77/24', 15),
            ('ping -c 1 10.0.2.2\n', '1 packets transmitted, 1 received', 15),
            ('ifconfig eth0 dhcp\n', 're:got 10\\.0\\.2\\.', 25),
            ('lspci\n', 'network (Ethernet)', 15),
            ('wifi selftest\n', 'wifi selftest: OK', 90),
            ('net\n', 'TCP connections', 15),
        ]
        if a.internet:
            net_steps += [
                ('nslookup example.com\n', 're:Address: +[0-9]+\\.', 20),
                ('wget -O null http://example.com/\n', 'Done:', 40),
                ('wget -O null https://example.com/\n', 'Done:', 60),
                ('curl -sI https://example.com/\n', 're:HTTP/1\\.1 200', 60),
            ]
        runs.append(('network', net_steps,
                     ['-netdev', 'user,id=n0,hostfwd=tcp:127.0.0.1:%d-:80' % fwd,
                      '-device', 'e1000,netdev=n0',
                      '-drive', 'if=none,id=nstick,format=raw,file=@WORK@/netstick.img'],
                     ['qemu-xhci', 'usb-kbd', 'usb-storage,drive=nstick']))
        # все ядра процессора (этап 10): 4 ядра - все проснулись
        runs.append(('smp', [
            (None, 'CPU cores: 4 of 4 running', 90),
            ('', "Type 'help'", 30),
            ('cpu\n', 'cpu3: APIC id 3, online', 15),
            # 4 программы сразу - хотя бы вдвое быстрее, чем по очереди
            ('smptest 100\n', 're:4 programs at once: [0-9]+ ms -> ([2-9]|[1-9][0-9])\\.[0-9]+ times faster', 300),
            ('threadtest\n', 'threadtest: OK', 60),
            ('cpu\n', 'Big kernel lock', 15),
            ('ps\n', 'idle3', 15),
        ], ['-smp', '4']))
        runs.append(('smp-q35', [
            (None, 'CPU cores: 4 of 4 running', 90),
            ('', "Type 'help'", 30),
        ], ['-smp', '4', '-machine', 'q35']))

        # ACPI через uACPI (этап 9): поддельная батарея из своей таблицы
        # SSDT (tools/test-battery.asl), потом "нажать кнопку питания"
        # (system_powerdown) - MyOS должна выключиться сама (_PTS, S5)
        bat = os.path.join(os.path.dirname(os.path.abspath(__file__)), 'test-battery.aml')
        runs.append(('acpi-power', [
            (None, "Type 'help'", 90),
            ('', 'uACPI: AML loaded', 5),
            ('battery\n', 'Battery BAT0: 80%, discharging, 3:12 left', 20),
            ('', 'health: 95% of design', 3),
            ('', 'AC adapter: unplugged', 3),
            ('', 'Lid: open', 3),
            ('', 'Power button: fixed event', 3),
            # яркость через ACPI _BCM (запасной путь; у QEMU нет ШИМ Intel)
            ('brightness\n', 'Brightness: 100% (ACPI _BCM', 10),
            ('brightness 40\n', 'Brightness: 40%', 10),
            ('brightness -\n', 'Brightness: 30%', 10),
            ('brightness 1\n', 'Brightness: 10%', 10),
            # значок батареи на панели задач: зелёная заливка 80%
            ('start\n', 're:wm: started', 20),
            ({'screen': [((0x20, 0xA0, 0x20), 8, 40)]}, '', 15),
            # кнопка питания - прямо с рабочего стола
            ({'qmp': ('system_powerdown', {})}, 'power button pressed - shutting down', 15),
            ('', '@exit', 20),
        ], ['-acpitable', 'file=' + bat]))

        # установка рядом с Arch (этап 9): диск с systemd-boot, куда MyOS
        # поставлена tools/install-arch.sh; systemd-boot запускает
        # \EFI\MyOS\BOOTX64.EFI, тот находит ядро рядом
        inst = make_install_disk(a, work)
        if inst:
            CUSTOM_BOOT_DISKS['install'] = inst
            runs.append(('install', [
                (None, "Type 'help'", 90),
                ('boot\n', re.escape('Kernel file: \\EFI\\MYOS\\KERNEL.ELF').join(['re:', '']), 15),
            ]))
        else:
            print('(no systemd-boot or mtools on this machine - install run skipped)')
        # браузер NetSurf (этап 9): рабочий стол -> терминал -> browser;
        # страница с хоста (UTF-8, CSS, PNG, JPEG) - на снимке экрана
        # должны быть цвет заголовка, жёлтый круг из PNG и малиновый
        # прямоугольник из JPEG
        runs.append(('browser', [
            (None, "Type 'help'", 90),
            ('', 'dhcp: eth0: address 10.0.2.15', 20),
            ('start\n', 're:wm: started', 20),
            ({'goto': (20, 786)}, '', 0.3),
            ({'click': 1}, '', 0.4),
            ({'goto': (60, 559)}, '', 0.3),
            ({'click': 1}, "re:wm: window .* opened", 8),
            ('browser %s/page.html\n' % url, 're:winproc: pid [0-9]+ got window', 40),
            ({'screen': [(BROWSER_H1, 8, 300), (BROWSER_SUN, 4, 5000),
                         (BROWSER_JPEG, 12, 3000)]}, '', 60),
        ], ['-netdev', 'user,id=n0', '-device', 'e1000,netdev=n0']))
        # Realtek в режиме C+ (тот же механизм колец, что у RTL8111/8168)
        runs.append(('net-rtl8139', [
            (None, "Type 'help'", 90),
            ('', 'eth0: Realtek RTL8139C+', 5),
            ('', 'dhcp: eth0: address 10.0.2.15', 20),
            ('ping -c 2 10.0.2.2\n', '2 packets transmitted, 2 received', 20),
            ('wget -O null %s/big.bin\n' % url, crc, 40),
        ], ['-netdev', 'user,id=n0', '-device', 'rtl8139,netdev=n0']))
        # e1000e (82574L) на q35: прерывания MSI
        runs.append(('net-e1000e', [
            (None, "Type 'help'", 90),
            ('', 're:eth0: Intel 82574L \\(e1000e\\).*MSI', 5),
            ('', 'dhcp: eth0: address 10.0.2.15', 20),
            ('wget -O null %s/big.bin\n' % url, crc, 40),
        ], ['-machine', 'q35']))
        # USB-модем ("раздача интернета с телефона"): QEMU usb-net умеет
        # и RNDIS (как Android), и CDC-ECM; подключаем на лету, качаем,
        # выдёргиваем, подключаем другим протоколом
        runs.append(('usb-tether', [
            (None, "Type 'help'", 90),
            ('', 'no wired network card found', 5),
            ('net usb rndis\n', 'prefer RNDIS', 10),
            ({'qmp': ('device_add', {'driver': 'usb-net', 'netdev': 'n0', 'bus': 'xhci.0',
                                     'port': '3', 'id': 'phone'})},
             're:net: usb0 - USB modem \\(RNDIS\\)', 20),
            ('', 'dhcp: usb0: address 10.0.2.15', 20),
            ('ping -c 2 10.0.2.2\n', '2 packets transmitted, 2 received', 20),
            ('wget -O null %s/big.bin\n' % url, crc, 40),
            ({'qmp': ('device_del', {'id': 'phone'})}, 'net: usb0 removed', 20),
            ('net usb auto\n', 'prefer CDC-ECM', 10),
            ({'qmp': ('device_add', {'driver': 'usb-net', 'netdev': 'n0', 'bus': 'xhci.0',
                                     'port': '3', 'id': 'phone2'})},
             're:net: usb0 - USB Ethernet \\(CDC-ECM\\)', 20),
            ('', 're:dhcp: usb0: address 10\\.0\\.2\\.', 20),
            ('wget -O null %s/big.bin\n' % url, crc, 40),
            ('usb\n', 're:network usb-ecm: .*-> usb0', 15),
        ], ['-netdev', 'user,id=n0'], ['qemu-xhci,id=xhci', 'usb-kbd,bus=xhci.0,port=1']))
        # exFAT (через FatFs): флешка "как у Ventoy" - большой раздел
        # exFAT + маленький FAT16; файлы, папки, wget прямо на exFAT,
        # потом повторная загрузка - всё на месте; в конце fsck.exfat
        exfat_tool = build_exfattool(work) if shutil_which('mkfs.exfat') and \
            shutil_which('fsck.exfat') else None
        if exfat_tool:
            make_ventoy_disk(os.path.join(work, 'ventoy.img'), exfat_tool, work)
            vdisk = ['-netdev', 'user,id=n0', '-device', 'e1000,netdev=n0',
                     '-drive', 'if=none,id=vs,format=raw,file=@WORK@/ventoy.img']
            vdev = ['qemu-xhci', 'usb-kbd', 'usb-storage,drive=vs']
            runs.append(('exfat', [
                (None, "Type 'help'", 90),
                ('', 'dhcp: eth0: address', 20),
                ('ls /\n', 're:/usb0p1 +exFAT Ventoy', 15),
                ('', 're:/usb0p2 +FAT16 VTOYEFI', 5),
                ('cat /usb0p1/host.txt\n', 'Hello from exFAT!', 15),
                ('cp /usb0p1/ISO/big.bin /usb0p1/copy.bin\n', 'Copied, 300000 bytes', 30),
                ('mkdir /usb0p1/myos\n', '>', 10),
                ('write /usb0p1/myos/note.txt Hello exFAT from MyOS\n', 'is now 22 bytes', 15),
                ('append /usb0p1/myos/note.txt second line\n', 'is now 34 bytes', 15),
                ('mv /usb0p1/copy.bin /usb0p1/myos/moved.bin\n', 'Moved.', 15),
                ('wget -O /usb0p1/myos/dl.bin %s/big.bin\n' % url, crc, 40),
                ('rm /usb0p1/host.txt\n', 'Deleted.', 15),
                ('df\n', 're:/usb0p1 +exFAT', 15),
                ('wget -O /usb0p9/x.htm %s/hello.txt\n' % url, 'disks and folders you can save to', 30),
            ], vdisk, vdev))
            runs.append(('exfat-reboot', [
                (None, "Type 'help'", 90),
                ('cat /usb0p1/myos/note.txt\n', 'second line', 15),
                ('ls /usb0p1/myos\n', 'dl.bin', 15),
                ('ls /usb0p1\n', 'ISO/', 15),
            ], vdisk, vdev))
        else:
            print('(no mkfs.exfat/fsck.exfat or gcc on this machine - exFAT runs skipped;'
                  ' install exfatprogs)')
        # Wi-Fi: 802.11 + WPA2 с программной точкой доступа "MyOS-Test"
        # (net/wlan_sim.c): поиск, неверный пароль, подключение, ключи,
        # DHCP (ответ - широковещательный, общим ключом), ping через
        # зашифрованный канал, отключение
        runs.append(('wifi-sim', [
            (None, "Type 'help'", 90),
            ('wifi sim\n', 'interface wlan0 ready', 20),
            ('wifi scan\n', 're:WPA2 +MyOS-Test', 30),
            ('wifi connect MyOS-Test wrong-pass\n', 'WRONG PASSWORD', 60),
            ('wifi connect MyOS-Test myos-wifi-test\n',
             'Wi-Fi is up: wlan0 address 192.168.77.2', 60),
            ('ping -c 3 192.168.77.1\n', '3 packets transmitted, 3 received', 20),
            ('ifconfig wlan0\n', 'inet 192.168.77.2/24  gateway 192.168.77.1', 15),
            ('wifi debug\n', 're:DHCP [1-9]', 15),
            ('', 're:echoed broadcasts [1-9]', 5),
            ('', 're:group-key ARP [1-9]', 5),
            ('wifi disconnect\n', 'Wi-Fi disconnected', 10),
            ('wifi\n', 'not connected', 10),
        ]))
        # запомнить сеть (этап 9): wifi save пишет EFI/MyOS/wifi.cfg на
        # диск загрузки (q35: он виден как /sata0); после "перезагрузки"
        # с того же диска сеть известна, 'wifi connect' без имени
        # подключается к ней; wifi forget стирает файл
        runs.append(('wifi-save', [
            (None, "Type 'help'", 90),
            ('wifi sim\n', 'interface wlan0 ready', 20),
            ('wifi save\n', 'Connect first', 10),
            ('wifi connect MyOS-Test myos-wifi-test\n',
             'Wi-Fi is up: wlan0 address 192.168.77.2', 60),
            # диск загрузки - "внутренний" (только чтение, как на ноутбуке):
            # без подтверждения не пишем; с ним - только EFI/MyOS/wifi.cfg
            ('disk protect sata0\n', 'sata0 is read-only now', 10),
            ('wifi save\n', 'To allow that, type:  wifi save confirm', 15),
            ('wifi save confirm\n', "Saved network 'MyOS-Test' to /sata0/EFI/MyOS/wifi.cfg", 15),
            ('cat /sata0/efi/myos/wifi.cfg\n', 're:psk=[0-9a-f]{64}', 15),
            ('cd /sata0\n', '', 1),
            ('write other.txt hi\n', 're:(?i)read-only', 15),
        ], ['-machine', 'q35']))
        CUSTOM_BOOT_DISKS['wifi-save-reboot'] = os.path.join(work, 'wifi-save.img')
        runs.append(('wifi-save-reboot', [
            (None, "saved Wi-Fi network 'MyOS-Test' - but no Wi-Fi adapter", 90),
            ('', "Type 'help'", 30),
            ('wifi sim\n', 'interface wlan0 ready', 20),
            ('wifi\n', "Saved network: 'MyOS-Test'", 15),
            ('wifi connect\n', 'Wi-Fi is up: wlan0 address 192.168.77.2', 60),
            ('disk protect sata0\n', 'sata0 is read-only now', 10),
            ('wifi forget\n', 'Forgot the saved network', 15),
            ('wifi connect\n', 'usage: wifi connect', 15),
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
            ('kpanic write\n', 'Page Fault: WRITE', 15),
            ('', 'READ-ONLY', 5),
            ('', 'kernel CODE', 5),
        ]))
        runs.append(('crash-stack', [
            (None, "Type 'help'", 90),
            ('kpanic stack\n', 'KERNEL STACK OVERFLOW', 20),
        ]))
        runs.append(('crash-null', [
            (None, "Type 'help'", 90),
            ('kpanic null\n', 'NULL pointer', 15),
        ]))

    ok = True
    last_log = ''
    make_test_disk(os.path.join(work, 'tree.img'), 64, 32, 'mbr', 'TREE')
    make_test_disk(os.path.join(work, 'stick.img'), 64, 32, 'mbr', 'STICK')
    make_test_disk(os.path.join(work, 'stick2.img'), 40, 16, 'none', 'STICK2')
    make_test_disk(os.path.join(work, 'sata.img'), 40, 16, 'gpt', 'SATADISK')
    make_test_disk(os.path.join(work, 'nvme.img'), 80, 32, 'gpt', 'NVME')
    make_test_disk(os.path.join(work, 'netstick.img'), 64, 32, 'mbr', 'NETSTICK',
                   {'ca.pem': open(os.path.join(TLS_DIR, 'ca.pem'), 'rb').read()})

    if a.only:
        runs = [r for r in runs if r[0] in a.only.split(',')]

    for run in runs:
        name, steps = run[0], run[1]
        extra = run[2] if len(run) > 2 else []
        devices = run[3] if len(run) > 3 else None
        print('--- run: %s' % name)
        r, last_log = run_steps(a, work, steps, name, extra, devices)
        if not r:
            ok = False
            break

    # диски после запусков storage: проверка "как в Linux"
    if ok and not a.quick and not a.only:
        for img, expect in (
                ('stick.img', {'/myos/hello.txt': b'Hello from MyOS\nsecond line\n',
                               '/myos/Mixed Case Name.txt': b'mixed\n',
                               '/fromram.txt': b'ram\n',
                               '/notes.txt': b'line one\nline two\n',
                               '/HOST.TXT': None, '/docs': None,
                               '/BIG.BIN': BIG_DATA}),
                ('sata.img', {'/renamed.txt': b'Hello from MyOS\nsecond line\n',
                              '/hello.txt': None}),
                ('nvme.img', {'/a/b/copy.bin': BIG_DATA, '/readme.md': b'# readme\n',
                              '/a/x/inner.txt': b'deep\n', '/x': None,
                              '/BIG.BIN': BIG_DATA}),
                ('stick2.img', {'/HOST.TXT': b'Hello from the host!\n'}),
                ('netstick.img', {'/dl.bin': BIG_DATA, '/tls.bin': BIG_DATA})):
            problems = check_disk(os.path.join(work, img), expect)
            print('%-4s %-14s -> %s' % ('PASS' if not problems else 'FAIL', '[fsck ' + img + ']',
                                        'consistent, files as expected' if not problems else ''))
            for p in problems:
                print('       ', p)
            if problems:
                ok = False

    if ok and not a.quick and not a.only and exfat_tool:
        problems = check_exfat(os.path.join(work, 'ventoy.img'), exfat_tool, work, {
            '/myos/note.txt': b'Hello exFAT from MyOS\nsecond line\n',
            '/myos/moved.bin': BIG_DATA, '/myos/dl.bin': BIG_DATA,
            '/ISO/big.bin': BIG_DATA, '/HOST.TXT': None, '/copy.bin': None})
        print('%-4s %-14s -> %s' % ('PASS' if not problems else 'FAIL', '[fsck ventoy.img]',
                                    'fsck.exfat clean, files as expected' if not problems else ''))
        for p in problems:
            print('       ', p)
        if problems:
            ok = False

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
