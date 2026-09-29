#!/bin/bash
# Сборка MyOS. Теперь всё делает Makefile (пересобирает только
# изменённые файлы); этот скрипт оставлен для привычки: ./build.sh
set -e
cd "$(dirname "$0")"
make
echo "Готово: BOOTX64.EFI (загрузчик) и kernel.elf (ядро) разложены в esp/EFI/BOOT/"
