#!/usr/bin/env python3
"""
Split MyOS main.c (one translation unit, everything static) into modules.

- Every top-level item (preprocessor line, type, global, function, asm block)
  is found by a small C scanner (comments/strings/char literals aware).
- Types, #defines, `extern` decls and `static inline` functions go to myos.h.
- Globals and functions lose `static` and go to the module that owns the
  source range they came from; myos.h gets an `extern` / prototype for each.
"""
import re, sys, os, collections

SRC = sys.argv[1]
OUT = sys.argv[2]

src = open(SRC, encoding='utf-8').read()
N = len(src)

# ---------------------------------------------------------------- scanner
items = []            # dict(kind, start, end, text, lead)
i = 0
gap_start = 0
line_start = True


def skip_comment_or_ws(j):
    """Return index after whitespace/comments starting at j."""
    while j < N:
        c = src[j]
        if c in ' \t\r\n':
            j += 1
        elif src.startswith('/*', j):
            k = src.find('*/', j + 2)
            j = k + 2
        elif src.startswith('//', j):
            k = src.find('\n', j)
            j = N if k < 0 else k + 1
        else:
            break
    return j


def extend_to_eol(j):
    """If the rest of the line after j is only ws/comment, include it."""
    k = j
    while k < N and src[k] in ' \t':
        k += 1
    if k < N and src.startswith('/*', k):
        e = src.find('*/', k + 2)
        if e >= 0 and '\n' not in src[k:e]:
            k = e + 2
            while k < N and src[k] in ' \t':
                k += 1
    elif k < N and src.startswith('//', k):
        e = src.find('\n', k)
        k = N if e < 0 else e
    if k < N and src[k] == '\n':
        return k + 1
    if k >= N:
        return N
    return j


pos = 0
while True:
    start = skip_comment_or_ws(pos)
    if start >= N:
        trailing = src[pos:]
        break
    lead = src[pos:start]
    if src[start] == '#':
        # preprocessor line (with continuations)
        j = start
        while True:
            e = src.find('\n', j)
            if e < 0:
                e = N
                break
            if src[e - 1] == '\\':
                j = e + 1
                continue
            break
        end = min(N, e + 1)
        items.append(dict(kind='pp', start=start, end=end, lead=lead))
        pos = end
        continue
    # C item
    j = start
    depth_b = depth_p = depth_s = 0
    is_func = False
    end = None
    while j < N:
        c = src[j]
        if src.startswith('/*', j):
            j = src.find('*/', j + 2) + 2
            continue
        if src.startswith('//', j):
            k = src.find('\n', j)
            j = N if k < 0 else k
            continue
        if c == '"' or c == "'":
            q = c
            j += 1
            while src[j] != q:
                if src[j] == '\\':
                    j += 2
                else:
                    j += 1
            j += 1
            continue
        if c == '(':
            depth_p += 1
        elif c == ')':
            depth_p -= 1
        elif c == '[':
            depth_s += 1
        elif c == ']':
            depth_s -= 1
        elif c == '{':
            if depth_b == 0 and depth_p == 0:
                head = src[start:j].rstrip()
                head_nocomm = re.sub(r'/\*.*?\*/', ' ', head, flags=re.S).rstrip()
                if head_nocomm.endswith(')') and not re.match(r'\s*(typedef|struct|union|enum)\b', head_nocomm) \
                        and '=' not in re.sub(r'\(.*\)', '', head_nocomm, flags=re.S):
                    is_func = True
            depth_b += 1
        elif c == '}':
            depth_b -= 1
            if depth_b == 0 and is_func:
                end = j + 1
                break
        elif c == ';' and depth_b == 0 and depth_p == 0:
            end = j + 1
            break
        j += 1
    end = extend_to_eol(end)
    items.append(dict(kind='func' if is_func else 'decl', start=start, end=end, lead=lead))
    pos = end

for it in items:
    it['text'] = src[it['start']:it['end']]
    it['line'] = src.count('\n', 0, it['start']) + 1

# ---------------------------------------------------------------- classify
ATTR = re.compile(r'__attribute__\s*\(\((?:[^()]|\([^()]*\))*\)\)')


def strip_attrs(t):
    return ATTR.sub(' ', t)


def code_only(t):
    t = re.sub(r'/\*.*?\*/', ' ', t, flags=re.S)
    t = re.sub(r'//[^\n]*', ' ', t)
    return t


def top_level_eq(t):
    """index of '=' at paren/brace/bracket depth 0, or -1"""
    d = 0
    k = 0
    while k < len(t):
        c = t[k]
        if c in '([{':
            d += 1
        elif c in ')]}':
            d -= 1
        elif c == '"':
            k += 1
            while t[k] != '"':
                k += 2 if t[k] == '\\' else 1
        elif c == '=' and d == 0 and t[k + 1:k + 2] != '=' and t[k - 1:k] not in '!<>=':
            return k
        k += 1
    return -1


for it in items:
    t = code_only(it['text']).strip()
    ts = strip_attrs(t).strip()
    if it['kind'] == 'pp':
        it['cls'] = 'include_efi' if re.match(r'#\s*include\s+"efi\.h"', t) else 'pp'
        continue
    if it['kind'] == 'func':
        head = t[:t.index('{')] if '{' in t else t
        it['cls'] = 'inline' if re.match(r'static\s+inline\b', ts) else 'func'
        m = re.search(r'([A-Za-z_]\w*)\s*\(', strip_attrs(head))
        it['name'] = m.group(1) if m else '?'
        # prototype
        proto = re.sub(r'/\*.*?\*/', '', head, flags=re.S).rstrip()
        proto = re.sub(r'^static\s+', '', proto)
        it['proto'] = proto + ';'
        continue
    # decl
    if re.match(r'typedef\b', ts) or re.match(r'(struct|union|enum)\s+\w*\s*\{', ts):
        it['cls'] = 'type'
    elif re.match(r'extern\b', ts):
        it['cls'] = 'extern'
    elif re.match(r'__asm__\b', ts):
        it['cls'] = 'asm'
    else:
        eq = top_level_eq(ts)
        before = ts[:eq] if eq >= 0 else ts.rstrip(';')
        if '(' in before:
            it['cls'] = 'proto_decl'     # forward prototype - regenerated
        else:
            it['cls'] = 'var'
            eqr = top_level_eq(t)
            decl = (t[:eqr] if eqr >= 0 else t.rstrip().rstrip(';')).strip()
            decl = re.sub(r'^static\s+', '', decl)
            it['extern'] = 'extern ' + decl + ';'
            m = re.search(r'([A-Za-z_]\w*)\s*(\[[^\]]*\]\s*)*$', strip_attrs(decl).strip())
            it['name'] = m.group(1) if m else '?'

# ---------------------------------------------------------------- module map
# (anchor text found at the start of a line/item, module) - in file order.
ANCHORS = [
    ('typedef __SIZE_TYPE__ myos_size_t;', 'lib/libc.c'),
    ('static UINTN g_color', 'shell/console.c'),
    ('#define PCI_CONFIG_ADDRESS', 'drivers/pci.c'),
    (' * xHCI - Capability Registers', 'drivers/xhci_common.c'),
    (' * Basic string helpers', 'lib/string.c'),
    (' * Input\n', 'shell/readline.c'),
    ('static int fs_find(', 'shell/fs.c'),
    (' * Text editor', 'shell/editor.c'),
    (' * Calculator', 'shell/calc.c'),
    (' * Fetch helpers', 'shell/fetch.c'),
    ('static void push_history(', 'shell/history.c'),
    (' * Графическая оболочка ("start")', 'gui/draw.c'),
    ('static UINT32 gui_ms_rand(void)', 'gui/minesweeper.c'),
    ('static BOOLEAN g_gui_draw_cursor', 'gui/desktop.c'),
    ('static int gui_streq(', 'gui/gstring.c'),
    (' * "Поддельный" ConOut для вывода ПОСЛЕ ExitBootServices', 'drivers/ebs_console.c'),
    ('static BOOLEAN gui_term_exec(', 'gui/terminal.c'),
    ('static void gui_draw_minesweeper(', 'gui/minesweeper_draw.c'),
    ('static UINTN gui_open_fileview(', 'gui/gui.c'),
    (' * Опрос Event Ring в поисках события конкретного типа', 'drivers/xhci_demo.c'),
    (' * Разбор настоящего HID Report Descriptor', 'drivers/hid.c'),
    (' * Шаг 6 — Address Device + первый настоящий USB-запрос', 'drivers/xhci_demo.c'),
    ('KERNEL MODE: ОС продолжает жить ПОСЛЕ ExitBootServices', 'kernel/kcon.c'),
    (' * 2. Процессор: порты, MSR, GDT, IDT, исключения', 'kernel/cpu.c'),
    (' * 3. Время: TSC + PIT (калибровка) + Local APIC timer', 'kernel/time.c'),
    (' * 4. Физическая память: карта от GetMemoryMap', 'kernel/pmm.c'),
    (' * 5a. Клавиатура: очередь клавиш', 'drivers/keyboard.c'),
    (' * 5b. PS/2-клавиатура', 'drivers/ps2.c'),
    (' * 5c. Мышь: накопитель движения', 'drivers/keyboard.c'),
    (' * 6. xHCI + USB HID: неблокирующий драйвер', 'drivers/usb.c'),
    (' * 7. "Прокладка": наши ConIn', 'kernel/shim.c'),
    (' * 8. Сам переход: команда "ebs"', 'kernel/enter.c'),
    (' * 9. Команды: kinfo, usb, mem, int3', 'kernel/kcmds.c'),
    (' * Command dispatcher', 'shell/commands.c'),
    (' * UEFI entry point', 'main.c'),
]
anchor_pos = []
last = -1
for a, mod in ANCHORS:
    k = src.find(a, last + 1)
    if k < 0:
        sys.exit('anchor not found: %r' % a)
    anchor_pos.append((k, mod))
    last = k

NAME_OVERRIDE = {
    'g_tsc_hz': 'kernel/time.c', 'tsc_delay_us': 'kernel/time.c',
    'busy_wait_ms': 'kernel/time.c',
    'g_fs': 'shell/fs.c',
    'g_history': 'shell/history.c', 'g_history_count': 'shell/history.c',
    'g_kernel_mode': 'kernel/enter.c', 'g_st': 'kernel/enter.c',
    'g_image_handle': 'main.c',
}


def module_of(it):
    if it.get('name') in NAME_OVERRIDE:
        return NAME_OVERRIDE[it['name']]
    # first item of the file (mem functions)
    mod = 'lib/libc.c'
    for k, m in anchor_pos:
        # anchor may be inside the item's leading comment
        if k < it['start']:
            mod = m
    return mod


# ---------------------------------------------------------------- emit
modules = collections.OrderedDict()
hdr_types = []
hdr_externs = []
hdr_protos = []
hdr_inline = []

for it in items:
    lead = it['lead']
    cls = it['cls']
    if cls == 'include_efi':
        continue
    if cls in ('pp', 'type', 'extern'):
        hdr_types.append(lead + it['text'])
        continue
    if cls == 'inline':
        hdr_inline.append(lead + it['text'])
        continue
    if cls == 'proto_decl':
        continue
    mod = module_of(it)
    text = it['text']
    if cls in ('func', 'var'):
        text = re.sub(r'^static\s+', '', text)
    modules.setdefault(mod, []).append(lead + text)
    if cls == 'var':
        hdr_externs.append(it['extern'])
    elif cls == 'func':
        hdr_protos.append((mod, it['proto']))

# sanity: every prototype name unique
names = [re.search(r'([A-Za-z_]\w*)\s*\(', strip_attrs(p)).group(1) for _, p in hdr_protos]
dups = [n for n, c in collections.Counter(names).items() if c > 1]
if dups:
    sys.exit('duplicate function names: %s' % dups)

os.makedirs(OUT, exist_ok=True)

H = []
H.append('''/*
 * myos.h - общий заголовок MyOS.
 *
 * Раньше вся ОС была одним файлом main.c (~19 000 строк), где всё
 * было static. Теперь она разложена по модулям (lib/, drivers/,
 * kernel/, gui/, shell/), а этот заголовок - то, что модули видят
 * друг у друга: константы (#define), типы, глобальные переменные
 * (extern) и объявления функций. Порядок разделов важен:
 * сначала типы и константы, потом переменные, потом функции,
 * и в самом конце маленькие static inline функции (они
 * пользуются всем, что объявлено выше).
 *
 * Файл сгенерирован скриптом split_main.py из прежнего main.c,
 * дальше правится руками как обычный заголовок.
 */
#ifndef MYOS_H
#define MYOS_H

#include "efi.h"

/*
 * Все символы MyOS - "скрытые" (hidden): модули ссылаются друг на
 * друга напрямую, относительно RIP. Без этого компилятор с -fpic
 * обращался бы к чужим глобальным переменным через GOT (таблицу
 * адресов для динамической линковки), которой в UEFI-образе нет.
 */
#pragma GCC visibility push(hidden)
''')
H.append('\n/* ================================================================\n * Константы и типы\n * ================================================================ */\n')
H.extend(hdr_types)
H.append('\n\n/* ================================================================\n * Глобальные переменные (определены в модулях)\n * ================================================================ */\n\n')
H.extend(e + '\n' for e in hdr_externs)
H.append('\n\n/* ================================================================\n * Функции, по модулям\n * ================================================================ */\n')
cur = None
for mod, p in hdr_protos:
    if mod != cur:
        H.append('\n/* --- %s --- */\n' % mod)
        cur = mod
    H.append(p + '\n')
H.append('\n\n/* ================================================================\n * Маленькие static inline функции (порты, MMIO, TSC, биты)\n * ================================================================ */\n')
H.extend(hdr_inline)
H.append('\n\n#pragma GCC visibility pop\n\n#endif /* MYOS_H */\n')
open(os.path.join(OUT, 'myos.h'), 'w', encoding='utf-8').write(''.join(H))

DESC = {
    'lib/libc.c': 'memcpy/memmove/memset/memcmp для компилятора',
    'lib/string.c': 'строки CHAR16/char, разбор чисел и слов',
    'shell/console.c': 'цвет, прокрутка истории экрана (scrollback), print*',
    'shell/readline.c': 'ввод строки шелла, история стрелками, PageUp/PageDown',
    'shell/fs.c': 'RAM-диск и его команды (ls, cat, write, ...)',
    'shell/editor.c': 'построчный редактор edit',
    'shell/calc.c': 'калькулятор calc',
    'shell/fetch.c': 'fetch (как neofetch)',
    'shell/history.c': 'история команд',
    'shell/commands.c': 'разбор и выполнение команд шелла (run_command)',
    'drivers/pci.c': 'конфигурационное пространство PCI через порты 0xCF8/0xCFC',
    'drivers/xhci_common.c': 'общее для xHCI: регистры, сброс, отключение драйвера прошивки, BIOS handoff',
    'drivers/xhci_demo.c': 'старое пошаговое демо xHCI одной мыши (команда ebsdemo)',
    'drivers/hid.c': 'разбор HID Report Descriptor',
    'drivers/ebs_console.c': 'пиксельная консоль старого демо ebsdemo',
    'drivers/keyboard.c': 'очередь клавиш, перевод HID Usage в UEFI-клавиши, накопитель мыши',
    'drivers/ps2.c': 'PS/2-клавиатура (i8042)',
    'drivers/usb.c': 'неблокирующий xHCI + USB HID драйвер kernel mode',
    'gui/draw.c': 'пиксельные примитивы GUI, шрифт GUI',
    'gui/minesweeper.c': 'Сапёр: логика поля и иконки',
    'gui/minesweeper_draw.c': 'Сапёр: окно',
    'gui/desktop.c': 'курсор, рабочий стол, окна',
    'gui/gstring.c': 'строки GUI (char), терминал GUI - хранение строк',
    'gui/terminal.c': 'терминал внутри GUI',
    'gui/gui.c': 'главный цикл GUI (команда start)',
    'kernel/kcon.c': 'текстовая консоль kernel mode в framebuffer (шрифт Spleen)',
    'kernel/cpu.c': 'GDT, IDT, обработчики прерываний, экран паники, PIC, I/O APIC',
    'kernel/time.c': 'TSC, калибровка по PIT, Local APIC timer, задержки',
    'kernel/pmm.c': 'карта памяти, страничный аллокатор, пул',
    'kernel/shim.c': 'своя EFI_SYSTEM_TABLE для шелла и GUI после ExitBootServices',
    'kernel/enter.c': 'команда ebs: выход из прошивки и запуск всего kernel mode',
    'kernel/kcmds.c': 'команды kinfo, usb, mousetest, mem',
    'main.c': 'точка входа efi_main и главный цикл шелла',
}
for mod, parts in modules.items():
    path = os.path.join(OUT, mod)
    os.makedirs(os.path.dirname(path) or '.', exist_ok=True)
    body = ''.join(parts)
    head = '/*\n * %s - %s.\n * Часть MyOS; общие объявления - в myos.h.\n */\n#include "myos.h"\n' % (mod, DESC.get(mod, ''))
    open(path, 'w', encoding='utf-8').write(head + body + ('\n' if not body.endswith('\n') else ''))

print('items:', len(items))
print('modules:', {m: len(p) for m, p in modules.items()})
print('header: types/pp=%d externs=%d protos=%d inline=%d' % (len(hdr_types), len(hdr_externs), len(hdr_protos), len(hdr_inline)))
