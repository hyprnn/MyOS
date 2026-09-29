#!/usr/bin/env python3
"""
tools/mkfont.py - сделать шрифт MyOS (font8x16.h) из DejaVu Sans Mono.

Этап 7: GUI рисует текст сглаженным шрифтом 8x16 с латиницей и
кириллицей. Для каждого символа храним 8x16 "яркостей" по 4 бита
(16 оттенков, 64 байта на символ): при выводе цвет текста смешивается
с фоном пропорционально яркости - края букв получаются мягкими.

Шрифт DejaVu (свободная лицензия, производная Bitstream Vera) нужен
только здесь, при генерации; готовый font8x16.h лежит в репозитории,
так что для сборки MyOS ни PIL, ни сам шрифт не нужны.

    python3 tools/mkfont.py [путь к DejaVuSansMono.ttf]
"""
import sys
from PIL import Image, ImageDraw, ImageFont

TTF = sys.argv[1] if len(sys.argv) > 1 else '/usr/share/fonts/truetype/dejavu/DejaVuSansMono.ttf'
SIZE, BASE, W, H = 13, 12, 8, 16

cps = list(range(0x20, 0x7F))
cps += [0xA0, 0xA9, 0xAB, 0xB0, 0xB1, 0xB7, 0xBB, 0xD7, 0xF7]
cps += [0x401] + list(range(0x410, 0x450)) + [0x451]
cps += [0x2013, 0x2014, 0x2018, 0x2019, 0x201C, 0x201D, 0x2022, 0x2026, 0x20AC, 0x2116,
        0x2190, 0x2191, 0x2192, 0x2193, 0x2588, 0x25B2, 0x25BC]
cps = sorted(set(cps))

font = ImageFont.truetype(TTF, SIZE)
out = []
out.append('/*\n * font8x16.h - шрифт MyOS: 8x16, 4 бита яркости на точку (сглаженный).\n'
           ' * СГЕНЕРИРОВАН tools/mkfont.py из DejaVu Sans Mono - не править руками.\n'
           ' * Общий для ядра (gui/gfx.c, kernel/kcon.c) и программ (user/lib/gfx.c).\n'
           ' * font_cp[] - коды символов Unicode по возрастанию (для поиска\n'
           ' * делением пополам), font_bits[i] - 16 строк по 4 байта: в каждом байте\n'
           ' * две точки, старшие 4 бита - левая.\n */\n')
out.append('#ifndef MYOS_FONT8X16_H\n#define MYOS_FONT8X16_H\n\n')
out.append('#define FONT_W 8\n#define FONT_H 16\n#define FONT_COUNT %d\n\n' % len(cps))
out.append('static const unsigned short font_cp[FONT_COUNT] = {\n')
for i in range(0, len(cps), 12):
    out.append('    ' + ', '.join('0x%04X' % c for c in cps[i:i+12]) + ',\n')
out.append('};\n\nstatic const unsigned char font_bits[FONT_COUNT][64] = {\n')
for c in cps:
    g = Image.new('L', (W, H), 0)
    d = ImageDraw.Draw(g)
    if c == 0x2588:
        d.rectangle((0, 0, W - 1, H - 1), fill=255)
    else:
        d.text((0, BASE), chr(c), font=font, fill=255, anchor='ls')
    px = g.load()
    b = []
    for y in range(H):
        for x in range(0, W, 2):
            a = (px[x, y] + 8) // 17
            bb = (px[x + 1, y] + 8) // 17
            b.append((min(a, 15) << 4) | min(bb, 15))
    out.append('    { ' + ', '.join('0x%02X' % v for v in b) + ' }, /* U+%04X */\n' % c)
out.append('};\n\n#endif\n')
open('font8x16.h', 'w').write(''.join(out))
print('font8x16.h: %d glyphs' % len(cps))
