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
