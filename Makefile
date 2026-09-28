# MyOS - сборка.
#
# Собираются ДВА файла:
#   BOOTX64.EFI  - загрузчик (UEFI-программа, формат PE): loader/
#   kernel.elf   - ядро (формат ELF, адрес 0xFFFFFFFF80000000): всё остальное
# Оба кладутся в esp/EFI/BOOT/ - прошивка запускает загрузчик, загрузчик
# читает ядро из той же папки.
#
#   make            собрать оба (пересобираются только изменённые файлы)
#   make iso        + загрузочный MyOS.iso (нужны mtools и xorriso)
#   make run        собрать ISO и запустить в QEMU; лог ядра (COM1) - прямо в терминал
#   make run-tablet то же, но мышь как планшет (удобнее в окне QEMU: нет "стенок")
#   make test       автотест в QEMU без окна
#   make clean      удалить всё собранное
#
# Путь к прошивке OVMF можно переопределить: make run OVMF=/путь/к/OVMF.fd

CC      := gcc
LD      := ld
OVMF    ?= /usr/share/edk2/x64/OVMF.4m.fd
QEMU    ?= qemu-system-x86_64

# Общее для обоих:
# -O2                               оптимизация
# -fno-strict-aliasing              код смотрит на одну память через разные типы
# -fno-tree-loop-distribute-patterns не превращать циклы в вызовы memset/memcpy
# -ffreestanding                    нет libc, нет "обычной" среды выполнения
# -fshort-wchar                     wchar_t/L"..." = 16 бит, как CHAR16 в UEFI
# -mno-red-zone                     обязательно для кода, который прерывают прерывания
COMMON  := -O2 -fno-strict-aliasing -fno-tree-loop-distribute-patterns \
           -ffreestanding -fno-stack-protector -fno-stack-check \
           -fshort-wchar -mno-red-zone -fno-ident \
           -Wall -Wextra -I.

# Загрузчик: позиционно-независимый код без GOT (см. myos.h), формат PE.
# -DMYOS_LOADER: память ещё отображена прошивкой 1:1 (P2V = просто приведение)
LCFLAGS := $(COMMON) -DMYOS_LOADER -fpic -fvisibility=hidden -maccumulate-outgoing-args
LLDFLAGS:= -nostdlib -shared -Bsymbolic -m i386pep --subsystem 10 -e efi_main

# Ядро: обычный (не PIC) код для верхних 2 ГиБ адресов (-mcmodel=kernel),
# раскладка - kernel/kernel.ld
KCFLAGS := $(COMMON) -fno-pic -mcmodel=kernel -fno-omit-frame-pointer \
           -fno-asynchronous-unwind-tables
KLDFLAGS:= -nostdlib -static -z max-page-size=0x1000 -T kernel/kernel.ld

# Загрузчику из общего кода нужны только поиск xHCI в PCI и "отъём"
# контроллера у прошивки
LSRCS   := loader/loader.c drivers/pci.c drivers/xhci_common.c lib/libc.c
KSRCS   := $(sort $(wildcard lib/*.c drivers/*.c kernel/*.c gui/*.c shell/*.c))

LOBJS   := $(LSRCS:%.c=build/loader/%.o)
KOBJS   := $(KSRCS:%.c=build/kernel/%.o)
DEPS    := $(LOBJS:.o=.d) $(KOBJS:.o=.d)

QEMU_DEV := -device qemu-xhci -device usb-mouse -device usb-kbd

.PHONY: all iso run run-tablet test clean

all: BOOTX64.EFI kernel.elf
	@mkdir -p esp/EFI/BOOT
	@cp BOOTX64.EFI esp/EFI/BOOT/BOOTX64.EFI
	@cp kernel.elf esp/EFI/BOOT/KERNEL.ELF

build/loader/%.o: %.c
	@mkdir -p $(dir $@)
	@echo "  CC  [loader] $<"
	@$(CC) $(LCFLAGS) -MMD -MP -c $< -o $@

build/kernel/%.o: %.c
	@mkdir -p $(dir $@)
	@echo "  CC  $<"
	@$(CC) $(KCFLAGS) -MMD -MP -c $< -o $@

BOOTX64.EFI: $(LOBJS)
	@echo "  LD  $@"
	@$(LD) $(LLDFLAGS) -o $@ $(LOBJS)

kernel.elf: $(KOBJS) kernel/kernel.ld
	@echo "  LD  $@"
	@$(LD) $(KLDFLAGS) -o $@ $(KOBJS)

iso: MyOS.iso

MyOS.iso: BOOTX64.EFI kernel.elf
	./build_iso.sh

run: MyOS.iso
	$(QEMU) -bios $(OVMF) -cdrom MyOS.iso -m 256M $(QEMU_DEV) -serial stdio

run-tablet: MyOS.iso
	$(QEMU) -bios $(OVMF) -cdrom MyOS.iso -m 256M \
	    -device qemu-xhci -device usb-tablet -device usb-kbd -serial stdio

test: all
	python3 tools/autotest.py --efi BOOTX64.EFI --kernel kernel.elf \
	    --ovmf $(OVMF) --qemu $(QEMU)

clean:
	rm -rf build BOOTX64.EFI kernel.elf MyOS.iso fat.img iso_root esp

-include $(DEPS)
