#!/usr/bin/env python3
"""
tools/mkwallpaper.py - картинка -> обои рабочего стола MyOS (этап 10).

    python3 tools/mkwallpaper.py картинка.png|jpg

Ядро не умеет PNG/JPEG, поэтому обои вшиваются готовыми пикселями:
gui/wallpaper/wallpaper.rgb - 1366x768 (экран HP 250 G7), по 3 байта
R, G, B на точку, строки сверху вниз. Картинка другой формы
обрезается по краям ("заполнить экран"), как это делают обои в
Windows. Рабочий стол при запуске растягивает их под свой экран
(gui/wm.c). Исходник сохраняется рядом сжатым (source.jpg), чтобы
обои можно было пересобрать. Нужен Pillow (pip install pillow).
"""
import os, sys
from PIL import Image

W, H = 1366, 768
here = os.path.join(os.path.dirname(os.path.abspath(__file__)), '..', 'gui', 'wallpaper')

src = Image.open(sys.argv[1]).convert('RGB')
sw, sh = src.size
scale = max(W / sw, H / sh)                       # заполнить, лишнее - обрезать
nw, nh = round(sw * scale), round(sh * scale)
img = src.resize((nw, nh), Image.LANCZOS)
x0, y0 = (nw - W) // 2, (nh - H) // 2
img = img.crop((x0, y0, x0 + W, y0 + H))

open(os.path.join(here, 'wallpaper.rgb'), 'wb').write(img.tobytes())

# исходник - сжатым, не больше 1920x1080
s = src.copy()
s.thumbnail((1920, 1080), Image.LANCZOS)
s.save(os.path.join(here, 'source.jpg'), quality=90)
print('wallpaper: %dx%d -> %dx%d' % (sw, sh, W, H))
