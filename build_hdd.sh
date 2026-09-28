#!/bin/bash
# Альтернатива ISO: собрать загрузочный "жёсткий диск"
# (raw-образ, отформатированный в FAT целиком, без
# таблицы разделов — как флешка для UEFI-загрузки).
#
# VirtualBox EFI надёжно грузится именно с такого
# диска, в отличие от ISO/El Torito, с которым у
# него бывают проблемы.
#
# Нужны: mtools (mformat, mmd, mcopy) и VBoxManage
# (идёт в комплекте с VirtualBox).
#
#   Debian/Ubuntu: sudo apt install mtools
#
# Запускать после ./build.sh (BOOTX64.EFI должен
# уже существовать).

set -e

if [ ! -f BOOTX64.EFI ]; then
    echo "Сначала соберите проект: ./build.sh"
    exit 1
fi

RAW_IMG=MyOS_hdd.img
OUT_VDI=MyOS_hdd.vdi

rm -f "$RAW_IMG" "$OUT_VDI"

# ------------------------------------------------------------
# 1. Raw-образ диска, 64 МБ, отформатированный в FAT32
#    ЦЕЛИКОМ (без MBR/GPT) — "superfloppy" формат,
#    который UEFI-прошивки понимают как готовый ESP.
# ------------------------------------------------------------
dd if=/dev/zero of="$RAW_IMG" bs=1M count=64

mformat -i "$RAW_IMG" -F ::

mmd -i "$RAW_IMG" ::/EFI
mmd -i "$RAW_IMG" ::/EFI/BOOT
mcopy -i "$RAW_IMG" BOOTX64.EFI ::/EFI/BOOT/BOOTX64.EFI

# ------------------------------------------------------------
# 2. Конвертируем raw -> VDI, чтобы VirtualBox мог
#    подключить это как обычный виртуальный диск.
# ------------------------------------------------------------
VBoxManage convertfromraw "$RAW_IMG" "$OUT_VDI" --format VDI

echo "Готово: $OUT_VDI"
echo ""
echo "Дальше в VirtualBox:"
echo "  1. Settings -> Storage"
echo "  2. Удали IDE Primary Device 0 (старый MyOS.vdi) и"
echo "     IDE Secondary Device 0 (MyOS.iso) — они больше не нужны."
echo "  3. Add Hard Disk -> выбери $OUT_VDI, подключи к IDE Primary."
echo "  4. Убедись, что в Boot Order стоит Hard Disk (можно снять"
echo "     Floppy и Optical, если не нужны)."
echo "  5. Запусти ВМ."
