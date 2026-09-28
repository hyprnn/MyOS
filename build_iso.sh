#!/bin/bash
# Сборка загрузочного UEFI ISO-образа MyOS.
#
# Нужны: mtools, xorriso.
#
#   Debian/Ubuntu: sudo apt install mtools xorriso
#   Fedora:        sudo dnf install mtools xorriso
#   Arch:          sudo pacman -S mtools libisoburn
#
# Запускать после ./build.sh (BOOTX64.EFI должен
# уже существовать).
#
# Схема ниже — стандартный, проверенный вариант
# (см. OSDev Wiki "UEFI Bare Bones"): FAT-образ
# размером с 1.44 МБ дискету, положенный внутрь
# ISO и подключённый через El Torito "no emulation".
# Именно эта комбинация корректно распознаётся
# прошивками QEMU/OVMF, VirtualBox и реальным
# железом — в отличие от варианта с
# -eltorito-alt-boot/-isohybrid-gpt-basdat,
# который VirtualBox не всегда понимает.

set -e

if [ ! -f BOOTX64.EFI ]; then
    echo "Сначала соберите проект: ./build.sh"
    exit 1
fi

FAT_IMG=fat.img
ISO_DIR=iso_root
OUT_ISO=MyOS.iso

rm -rf "$FAT_IMG" "$ISO_DIR" "$OUT_ISO"

# ------------------------------------------------------------
# 1. FAT-образ размером 1.44 МБ с EFI/BOOT/BOOTX64.EFI внутри.
# ------------------------------------------------------------
dd if=/dev/zero of="$FAT_IMG" bs=1024 count=1440

mformat -i "$FAT_IMG" -f 1440 ::

mmd -i "$FAT_IMG" ::/EFI
mmd -i "$FAT_IMG" ::/EFI/BOOT
mcopy -i "$FAT_IMG" BOOTX64.EFI ::/EFI/BOOT/BOOTX64.EFI

# ------------------------------------------------------------
# 2. Кладём образ в дерево ISO.
#
#    ВАЖНО: помимо самого fat.img (который
#    используется El Torito для загрузки),
#    нужно продублировать /EFI/BOOT/BOOTX64.EFI
#    и в файловой системе ISO9660 напрямую.
#    Без этого xorriso честно предупреждает
#    ("no directory /EFI/BOOT will emerge in
#    the ISO filesystem"), а прошивка VirtualBox
#    в некоторых версиях ищет файл именно там
#    и не находит его — отсюда "Not Found".
# ------------------------------------------------------------
mkdir -p "$ISO_DIR/EFI/BOOT"
cp "$FAT_IMG" "$ISO_DIR/"
cp BOOTX64.EFI "$ISO_DIR/EFI/BOOT/BOOTX64.EFI"

# ------------------------------------------------------------
# 3. Собираем чисто UEFI-загрузочный ISO.
# ------------------------------------------------------------
xorriso -as mkisofs \
    -R -f \
    -e "$FAT_IMG" \
    -no-emul-boot \
    -o "$OUT_ISO" \
    "$ISO_DIR"

echo "Готово: $OUT_ISO"
