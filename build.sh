#!/bin/bash
# Сборка MyOS. Теперь всё делает Makefile (пересобирает только
# изменённые файлы); этот скрипт оставлен для привычки: ./build.sh
set -e
cd "$(dirname "$0")"
make
echo "Готово: BOOTX64.EFI собран и разложен в esp/EFI/BOOT/BOOTX64.EFI"
