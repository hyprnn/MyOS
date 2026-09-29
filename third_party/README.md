# Чужой код в MyOS

## bearssl/ — BearSSL 0.6 (TLS для HTTPS)

* Автор: Thomas Pornin, https://bearssl.org/ — лицензия MIT (`bearssl/LICENSE.txt`).
* Откуда: исходный пакет Ubuntu `bearssl_0.6+dfsg.1` (тот же код, что
  bearssl-0.6.tar.gz; убран только готовый `T0Comp.exe`). Взяты `src/` и
  `inc/` без изменений; файлы `.t0` (исходники генератора) не нужны — сгенерированные
  `.c` уже в `src/`.
* Своё: `bearssl/myos/string.h` — заглушка системного `<string.h>`.
* Изменено (этап 9): `src/rand/sysrng.c` — источник случайных чисел
  `getentropy()` при `BR_USE_GETENTROPY=1` (для второй сборки BearSSL — с
  picolibc, для curl; см. `PBSSL_CFLAGS` в `Makefile`).
* Собирается в `build/user/libtls.a` вместе с `user/tls/` (обёртка MyOS и
  корневые сертификаты) — только для программ, которым нужен TLS (`wget`).
  Настройки сборки (без времени/случайных чисел ОС, без AES-NI/SSE2-интринсиков)
  — `BSSL_CFLAGS` в `Makefile`.

## fatfs/ — FatFs (ooFatFs R0.13c): exFAT

* Автор FatFs: ChaN, http://elm-chan.org/fsw/ff/ — лицензия BSD-1-clause
  (текст — в начале `ff.c`). ooFatFs — вариант из MicroPython
  (https://github.com/micropython/oofatfs): у каждого тома свой указатель
  на диск (`FATFS.drv`), без таблицы дисков.
* Откуда: исходный пакет Ubuntu `micropython_1.22.1+ds`, `lib/oofatfs/`:
  `ff.c`, `ff.h`, `diskio.h`, `ffunicode.c` — без изменений.
* Своё: `fatfs/ffconf.h` (exFAT, длинные имена UTF-8, время от MyOS),
  `fatfs/myos/string.h`. Собирается в ядро (`FATFS_CFLAGS` в Makefile);
  прослойка к VFS — `fs/exfat.c`. Утилита хоста для автотеста —
  `tools/exfattool.c` (та же FatFs).

## Драйвер Wi-Fi rtw88 (Linux) и прошивка Realtek

* `drivers/rtw8821c.c` — перенос частей драйвера Linux rtw88
  (`drivers/net/wireless/realtek/rtw88`: pci.c, mac.c, fw.c, efuse.c,
  phy.c, coex.c, rtw8821c.c), Copyright © 2018–2019 Realtek Corporation,
  двойная лицензия **GPL-2.0 OR BSD-3-Clause** — MyOS пользуется
  BSD-3-Clause. Код переписан под MyOS (не копия), порядок регистров и
  числа — как у Realtek.
* `drivers/rtw8821c_table.c` — таблицы `rtw8821c_table.c` из того же
  драйвера (та же лицензия), без изменений чисел; заменены только имена
  типов. Взято из ветки master ядра Linux (сентябрь 2026).
* Текст BSD-3-Clause для этих файлов:

  > Redistribution and use in source and binary forms, with or without
  > modification, are permitted provided that the following conditions are
  > met: (1) Redistributions of source code must retain the above copyright
  > notice, this list of conditions and the following disclaimer.
  > (2) Redistributions in binary form must reproduce the above copyright
  > notice, this list of conditions and the following disclaimer in the
  > documentation and/or other materials provided with the distribution.
  > (3) Neither the name of the copyright holder nor the names of its
  > contributors may be used to endorse or promote products derived from
  > this software without specific prior written permission.
  > THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS
  > IS" AND ANY EXPRESS OR IMPLIED WARRANTIES ... ARE DISCLAIMED.

* `firmware/rtw88/rtw8821c_fw.bin` — прошивка процессора внутри чипа
  (версия 24.11) из linux-firmware (gitlab.com/kernel-firmware, файл
  `rtw88/rtw8821c_fw.bin`, sha256 2ef409bc…32f3f0). Realtek разрешает
  распространять её только в двоичном виде и без изменений; условия —
  `firmware/LICENCE.rtlwifi_firmware.txt` (копия LICENCE.rtlwifi_firmware.txt
  из linux-firmware). Вклеивается в ядро файлом `firmware/firmware.S`.

## picolibc/ — picolibc 1.8.12 (полная libc для программ)

* Автор: Keith Packard и проект newlib, https://github.com/picolibc/picolibc
  (тег `1.8.12`). Лицензии — BSD-подобные (BSD-2/BSD-3 и лицензии newlib для
  отдельных файлов; сводка — `picolibc/COPYING.picolibc`, текст лицензии —
  в начале каждого файла).
* Как получено: сборка meson под x86_64 без ОС (`-Dthread-local-storage=false
  -Dsingle-thread=true -Dposix-console=true -Dmb-capable=true
  -Dio-long-long=true -Dformat-default=double -Dos-fallback=false
  -Dstdio-exit-flush=true`), затем в `src/` скопированы ровно те исходники и
  заголовки, что попали в `libc.a` (по журналу зависимостей ninja), а в
  `include/` — установленные заголовки (с готовым `picolibc.h`). Список
  файлов — `picolibc/files.mk`; собирает `Makefile` (meson не нужен).
* Не взято: заглушки ОС (`libos/`), TLS (`tls.c`, `tcb.S`, `inittls.c`),
  `popen`/`system`/`exec*`/`getpass` — в MyOS программы не порождают процессов.
* Изменено: `include/sys/features.h` — при `__myos__` объявлены
  `_POSIX_MONOTONIC_CLOCK` и `_POSIX_TIMERS` (как для RTEMS);
  `include/sys/cdefs.h` — запасной `__has_extension` (у gcc его нет, без него
  заголовки не собираются в режиме `-std=c99`).
* Привязка к ядру MyOS — свой файл `user/posix/os.c` (read/write/open/lseek/
  fstat/sbrk/время/opendir...). Программы на ней — `user/posix/apps/`.

## zlib/ — zlib 1.3.1 (сжатие gzip для curl и браузера)

* Авторы: Jean-loup Gailly и Mark Adler, https://zlib.net/ — лицензия zlib
  (`zlib/LICENSE`). Откуда: https://github.com/madler/zlib, тег `v1.3.1`;
  взяты файлы библиотеки (`*.c` из списка `ZLIB_SRCS` в `Makefile` и все `*.h`)
  без изменений.

## curl/ — curl 8.14.1 (HTTP/HTTPS для программ)

* Автор: Daniel Stenberg и участники, https://curl.se/ — лицензия curl
  (MIT-подобная, `curl/COPYING`). Откуда: https://github.com/curl/curl, тег
  `curl-8_14_1` — последняя версия с BearSSL (в 8.15 его убрали).
* Как получено: CMake-сборка под MyOS (picolibc + `user/posix`; только HTTP и
  HTTPS, BearSSL, zlib, без IPv6, потоков, cookies-файлов, HSTS, alt-svc);
  скопированы ровно попавшие в сборку исходники и заголовки библиотеки
  (`lib/`) и программы `curl` (`src/`), список — `curl/files.mk`.
  `curl/myos/curl_config.h` — созданный CMake файл настроек (с правками,
  описанными в его начале).
* Изменено: `lib/vtls/bearssl.c` — если файла корней не дали, верить
  встроенным корням Mozilla из `user/tls/roots.c` (как `wget`).
* Особенность BearSSL-части curl: сертификат сверяется только с именем
  сервера, не с IP-адресом (`https://1.2.3.4/` с проверкой не пройдёт).

## netsurf/ — браузер NetSurf и его библиотеки (этап 9)

* NetSurf — https://www.netsurf-browser.org/, лицензия **GPL-2.0**
  (`netsurf/netsurf/COPYING`; отдельные файлы — MIT, см. их начало). Это
  отдельная программа `/bin/browser`; ядро MyOS под неё не попадает.
* Библиотеки проекта NetSurf — лицензия **MIT** (`netsurf/<имя>/COPYING`):
  libwapcaplet, libparserutils, libhubbub (HTML5-разбор), libdom, libcss,
  libnsutils, libnsgif, libnsbmp, libnspsl, libnslog, libnsfb (рисование).
* Откуда: https://github.com/netsurf-browser/…, ветка master (сентябрь 2026):
  netsurf `39da3c3a40af`, libwapcaplet `c7c128d3eb32`, libparserutils
  `6b0cbf086ca8`, libhubbub `6651b8cf87a4`, libdom `f69781e1f062`, libcss
  `499f1c4601ad`, libnsutils `0bd39060740b`, libnsgif `22e99eb6818b`, libnsbmp
  `ea063c9f46ac`, libnspsl `82815c2bc7fd`, libnslog `bedff2146270`, libnsfb
  `b701cdce7241`.
* Как получено: сборка их собственной системой сборки (buildsystem) под MyOS
  (компилятор-обёртка с picolibc и `user/posix`; NetSurf: цель framebuffer,
  шрифты FreeType, без JavaScript, SVG, WebP, PDF), затем скопированы ровно те
  исходники и заголовки, что попали в сборку (по `gcc -M`), вместе со
  сгенерированными файлами (папки `gen/`: таблицы libcss/hubbub/parserutils,
  разборщик фильтров libnslog (bison/flex), картинки кнопок NetSurf в C,
  `testament.h`) — поэтому perl, gperf, bison не нужны. Правила сборки —
  сгенерированный `netsurf/build.mk` (флаги — как у их сборки), подключается
  из `Makefile`. `netsurf/include/` — два заголовка libdom под тем путём, по
  которому их подключают (`dom/bindings/hubbub/`).
* Ресурсы браузера — `netsurf/netsurf/resources/` (тексты `Messages` —
  английский набор, собранный их скриптом из `FatMessages`; стили, страницы
  about:, картинки). Вшиваются в программу (`/embed/...`, см. `NS_EMBED`).
* Изменено (помечено «MyOS» в тексте):
  * libnsfb: новая поверхность `src/surface/myos.c` (окно рабочего стола MyOS,
    события мыши и клавиатуры) и `NSFB_SURFACE_MYOS` в `include/libnsfb.h`;
  * NetSurf `frontends/framebuffer/gui.c` — по умолчанию окно MyOS 1000x700;
    `fbtk/event.c` — готовый символ Юникода от MyOS (русская раскладка);
    `fbtk/text.c` — строка адреса хранит UTF-8 (кириллица; Backspace и
    стрелки — по целым символам), Ctrl+A / Ctrl+L очищают её;
    `content/fetchers/curl.c` и `frontends/framebuffer/schedule.c` — две
    ошибки типов (int вместо long), найденные предупреждениями gcc;
  * libparserutils собрана с `WITHOUT_ICONV_FILTER`: кодировки страниц
    (UTF-8, UTF-16, ISO-8859-*, Windows-125*) переводит она сама, а не iconv.

## freetype/, libpng/, libjpeg-turbo/, utf8proc/ — шрифты, картинки, Юникод

* FreeType 2.13.3 (тег `VER-2-13-3`, https://freetype.org/) — лицензия FTL
  (`freetype/LICENSE.TXT`, `docs/FTL.TXT`); без zlib/bzip2/png/HarfBuzz/Brotli.
  `freetype/myos/include` — настройки, созданные его CMake.
* libpng 1.6.50 (тег `v1.6.50`) — лицензия libpng (`libpng/LICENSE`);
  настройки — готовый `scripts/pnglibconf.h.prebuilt`.
* libjpeg-turbo 3.1.2 (тег `3.1.2`) — лицензии IJG и BSD (`LICENSE.md`,
  `README.ijg`); без SIMD. `libjpeg-turbo/myos` — созданные CMake `jconfig*.h`.
* utf8proc 2.10.0 (тег `v2.10.0`, JuliaStrings) — лицензия MIT
  (`utf8proc/LICENSE.md`) — нужен NetSurf для международных имён сайтов.

## fonts/dejavu/ — шрифты DejaVu для браузера

* DejaVu Sans / Serif / Sans Mono (обычный и жирный) — лицензия Bitstream
  Vera + public domain (`fonts/dejavu/LICENSE`); из пакета Ubuntu
  `fonts-dejavu-core`. Кириллица есть. Вшиваются в браузер.

## uacpi/ — uACPI: интерпретатор AML (ACPI) в ядре

* Автор: Daniil Tatianin и участники, https://github.com/uACPI/uACPI —
  лицензия MIT (`uacpi/LICENSE`). Коммит — в `uacpi/VERSION`.
* Взяты `source/`, `include/`, `README.md`, `LICENSE` без изменений
  (без тестов и файлов сборки CMake/meson).
* Собирается в ядро (`UACPI_CFLAGS` в `Makefile`, настройка
  `UACPI_NATIVE_ALLOC_ZEROED`). Функции ядра, которые она зовёт
  (`uacpi_kernel_*`), — `kernel/acpi_os.c`; батарея, EC, кнопка питания —
  `kernel/acpi_dev.c` (этап 9).
