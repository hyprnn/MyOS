# Чужой код в MyOS

## bearssl/ — BearSSL 0.6 (TLS для HTTPS)

* Автор: Thomas Pornin, https://bearssl.org/ — лицензия MIT (`bearssl/LICENSE.txt`).
* Откуда: исходный пакет Ubuntu `bearssl_0.6+dfsg.1` (тот же код, что
  bearssl-0.6.tar.gz; убран только готовый `T0Comp.exe`). Взяты `src/` и
  `inc/` без изменений; файлы `.t0` (исходники генератора) не нужны — сгенерированные
  `.c` уже в `src/`.
* Своё: `bearssl/myos/string.h` — заглушка системного `<string.h>`.
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
  `_POSIX_MONOTONIC_CLOCK` и `_POSIX_TIMERS` (как для RTEMS).
* Привязка к ядру MyOS — свой файл `user/posix/os.c` (read/write/open/lseek/
  fstat/sbrk/время/opendir...). Программы на ней — `user/posix/apps/`.
