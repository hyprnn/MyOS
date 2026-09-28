# MyOS - сборка.
#
#   make            собрать BOOTX64.EFI (пересобираются только изменённые файлы)
#   make iso        + загрузочный MyOS.iso (нужны mtools и xorriso)
#   make run        собрать ISO и запустить в QEMU; лог ОС (COM1) - прямо в терминал
#   make run-tablet то же, но мышь как планшет (удобнее в окне QEMU: нет "стенок")
#   make test       автотест в QEMU без окна: загрузка, ebs, kinfo, usb, mem, int3
#   make clean      удалить всё собранное
#
# Путь к прошивке OVMF можно переопределить: make run OVMF=/путь/к/OVMF.fd

CC      := gcc
LD      := ld
OVMF    ?= /usr/share/edk2/x64/OVMF.4m.fd
QEMU    ?= qemu-system-x86_64

# -O2                               оптимизация
# -fno-strict-aliasing              код смотрит на одну память через разные типы
# -fno-tree-loop-distribute-patterns не превращать циклы в вызовы memset/memcpy
# -ffreestanding                    нет libc, нет "обычной" среды выполнения
# -fshort-wchar                     wchar_t/L"..." = 16 бит, как CHAR16 в UEFI
# -mno-red-zone                     обязательно для кода, который прерывают прерывания
# -fpic -fvisibility=hidden         позиционно-независимый код без GOT (см. myos.h)
CFLAGS  := -O2 -fno-strict-aliasing -fno-tree-loop-distribute-patterns \
           -ffreestanding -fno-stack-protector -fno-stack-check \
           -fshort-wchar -mno-red-zone -fpic -fvisibility=hidden \
           -maccumulate-outgoing-args -fno-ident \
           -Wall -Wextra -I.

LDFLAGS := -nostdlib -shared -Bsymbolic -m i386pep --subsystem 10 -e efi_main

SRCS    := main.c $(sort $(wildcard lib/*.c drivers/*.c kernel/*.c gui/*.c shell/*.c))
OBJS    := $(SRCS:%.c=build/%.o)
DEPS    := $(OBJS:.o=.d)

QEMU_DEV := -device qemu-xhci -device usb-mouse -device usb-kbd

.PHONY: all iso run run-tablet test clean

all: BOOTX64.EFI

build/%.o: %.c
	@mkdir -p $(dir $@)
	@echo "  CC  $<"
	@$(CC) $(CFLAGS) -MMD -MP -c $< -o $@

BOOTX64.EFI: $(OBJS)
	@echo "  LD  $@"
	@$(LD) $(LDFLAGS) -o $@ $(OBJS)
	@mkdir -p esp/EFI/BOOT
	@cp $@ esp/EFI/BOOT/BOOTX64.EFI

iso: MyOS.iso

MyOS.iso: BOOTX64.EFI
	./build_iso.sh

run: MyOS.iso
	$(QEMU) -bios $(OVMF) -cdrom MyOS.iso -m 256M $(QEMU_DEV) -serial stdio

run-tablet: MyOS.iso
	$(QEMU) -bios $(OVMF) -cdrom MyOS.iso -m 256M \
	    -device qemu-xhci -device usb-tablet -device usb-kbd -serial stdio

test: BOOTX64.EFI
	python3 tools/autotest.py --efi BOOTX64.EFI --ovmf $(OVMF) --qemu $(QEMU)

clean:
	rm -rf build BOOTX64.EFI MyOS.iso fat.img iso_root esp

-include $(DEPS)
