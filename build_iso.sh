#!/bin/bash
# Сборка загрузочного UEFI ISO-образа MyOS.
#
# Нужны: mtools, xorriso.
#
#   Debian/Ubuntu: sudo apt install mtools xorriso
#   Fedora:        sudo dnf install mtools xorriso
#   Arch:          sudo pacman -S mtools libisoburn
#
# Запускать после make (BOOTX64.EFI - загрузчик - и
# kernel.elf - ядро - должны уже существовать). В образ
# они кладутся рядом: /EFI/BOOT/BOOTX64.EFI и
# /EFI/BOOT/KERNEL.ELF - загрузчик ищет ядро там.
#
# Схема ниже — стандартный, проверенный вариант
# (см. OSDev Wiki "UEFI Bare Bones"): FAT-образ
# (размер - под ядро, см. ниже), положенный внутрь
# ISO и подключённый через El Torito "no emulation".
# Именно эта комбинация корректно распознаётся
# прошивками QEMU/OVMF, VirtualBox и реальным
# железом — в отличие от варианта с
# -eltorito-alt-boot/-isohybrid-gpt-basdat,
# который VirtualBox не всегда понимает.

set -e

if [ ! -f BOOTX64.EFI ] || [ ! -f kernel.elf ]; then
    echo "Сначала соберите проект: make"
    exit 1
fi

FAT_IMG=fat.img
ISO_DIR=iso_root
OUT_ISO=MyOS.iso

rm -rf "$FAT_IMG" "$ISO_DIR" "$OUT_ISO"

# ------------------------------------------------------------
# 1. FAT-образ с загрузчиком и ядром внутри. Размер - по ядру:
#    раньше хватало дискеты 2.88 МБ, но с этапа 9 в ядро вклеены
#    программы с браузером (~10 МБ). Берём размер файлов + 2 МБ
#    запаса, не меньше 4 МБ; El Torito допускает до 32 МБ.
# ------------------------------------------------------------
NEED_KB=$(( ( $(stat -c %s BOOTX64.EFI) + $(stat -c %s kernel.elf) ) / 1024 ))
SIZE_MB=$(( NEED_KB / 1024 + 2 ))
[ "$SIZE_MB" -lt 4 ] && SIZE_MB=4
if [ "$SIZE_MB" -gt 32 ]; then
    echo "kernel.elf слишком большой для загрузочного образа ISO ($SIZE_MB МБ > 32 МБ)"
    exit 1
fi

dd if=/dev/zero of="$FAT_IMG" bs=1M count="$SIZE_MB" status=none

# -T - число секторов по 512 байт; -h/-s - условная геометрия диска
mformat -i "$FAT_IMG" -T $(( SIZE_MB * 2048 )) -h 64 -s 32 ::

mmd -i "$FAT_IMG" ::/EFI
mmd -i "$FAT_IMG" ::/EFI/BOOT
mcopy -i "$FAT_IMG" BOOTX64.EFI ::/EFI/BOOT/BOOTX64.EFI
mcopy -i "$FAT_IMG" kernel.elf ::/EFI/BOOT/KERNEL.ELF

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
cp kernel.elf "$ISO_DIR/EFI/BOOT/KERNEL.ELF"

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
