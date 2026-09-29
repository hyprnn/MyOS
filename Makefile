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
KSRCS   := $(sort $(wildcard lib/*.c drivers/*.c kernel/*.c fs/*.c gui/*.c shell/*.c net/*.c))

LOBJS   := $(LSRCS:%.c=build/loader/%.o)
KOBJS   := $(KSRCS:%.c=build/kernel/%.o) build/kernel/apps.o
DEPS    := $(LOBJS:.o=.d) $(KSRCS:%.c=build/kernel/%.d)

# Программы для ring 3 (этап 6): user/apps/*.c + мини-libc user/lib/.
# Каждая - отдельный статический ELF (build/user/<имя>), линкуется
# с 0x400000 (user/user.ld). Копии: в ядро (том /bin, build/apps.S)
# и на флешку (esp/APPS/).
UCFLAGS := -O2 -ffreestanding -fno-stack-protector -fno-stack-check -fno-pic -fno-pie \
           -fno-builtin -fno-asynchronous-unwind-tables -fno-ident -Wall -Wextra \
           -Iuser/include
ULDFLAGS:= -nostdlib -static -z noexecstack -z max-page-size=0x1000 -T user/user.ld
APPS    := $(sort $(basename $(notdir $(wildcard user/apps/*.c))))
ULIB    := $(patsubst user/lib/%,build/user/lib/%.o,$(basename $(wildcard user/lib/*.c user/lib/*.S)))
APP_ELFS:= $(APPS:%=build/user/%)

# TLS (HTTPS) для программ: BearSSL (third_party/bearssl, MIT) + обёртка
# MyOS и корневые сертификаты (user/tls/). Всё - в архив libtls.a:
# компоновщик берёт из него только нужное, поэтому программы без TLS
# не растут. Настройки BearSSL: ни времени, ни случайных чисел ОС (их
# даёт ядро MyOS через системные вызовы), без интринсиков AES-NI/SSE2
# (им нужны заголовки libc хоста).
BSSL_DIR   := third_party/bearssl
BSSL_SRCS  := $(sort $(wildcard $(BSSL_DIR)/src/*.c $(BSSL_DIR)/src/*/*.c))
BSSL_OBJS  := $(BSSL_SRCS:$(BSSL_DIR)/src/%.c=build/user/bearssl/%.o)
BSSL_CFLAGS:= -O2 -ffreestanding -fno-stack-protector -fno-stack-check -fno-pic -fno-pie \
              -fno-builtin -fno-asynchronous-unwind-tables -fno-ident -U_FORTIFY_SOURCE \
              -I$(BSSL_DIR)/myos -I$(BSSL_DIR)/inc -I$(BSSL_DIR)/src \
              -DBR_USE_UNIX_TIME=0 -DBR_USE_WIN32_TIME=0 -DBR_USE_URANDOM=0 \
              -DBR_USE_WIN32_RAND=0 -DBR_RDRAND=0 -DBR_AES_X86NI=0 -DBR_SSE2=0 \
              -DBR_POWER8=0 -DBR_64=1 -DBR_LE_UNALIGNED=1
TLS_OBJS   := build/user/tls/tls.o build/user/tls/roots.o
TLS_LIB    := build/user/libtls.a

QEMU_DEV := -device qemu-xhci -device usb-mouse -device usb-kbd
# сеть в QEMU: по умолчанию карта e1000 + "user"-сеть (DHCP, шлюз 10.0.2.2
# = хост, DNS 10.0.2.3). Например, открыть httpd из браузера хоста:
#   make run QEMU_NET="-netdev user,id=n0,hostfwd=tcp::8080-:80 -device e1000,netdev=n0"
# USB-модем (как телефон): QEMU_NET="-netdev user,id=n0 -device usb-net,netdev=n0"
QEMU_NET ?=

.PHONY: all iso run run-tablet test clean

# промежуточные .o программ не удалять (иначе make каждый раз собирает заново)
.SECONDARY:

all: BOOTX64.EFI kernel.elf
	@mkdir -p esp/EFI/BOOT esp/APPS
	@cp BOOTX64.EFI esp/EFI/BOOT/BOOTX64.EFI
	@cp kernel.elf esp/EFI/BOOT/KERNEL.ELF
	@for a in $(APPS); do cp build/user/$$a esp/APPS/$$(echo $$a | tr a-z A-Z); done

build/user/lib/%.o: user/lib/%.c user/include/myos.h sysnum.h
	@mkdir -p $(dir $@)
	@echo "  CC  [user] $<"
	@$(CC) $(UCFLAGS) -c $< -o $@

build/user/lib/%.o: user/lib/%.S
	@mkdir -p $(dir $@)
	@echo "  AS  [user] $<"
	@$(CC) $(UCFLAGS) -c $< -o $@

build/user/apps/%.o: user/apps/%.c user/include/myos.h sysnum.h
	@mkdir -p $(dir $@)
	@echo "  CC  [user] $<"
	@$(CC) $(UCFLAGS) -c $< -o $@

build/user/bearssl/%.o: $(BSSL_DIR)/src/%.c
	@mkdir -p $(dir $@)
	@echo "  CC  [bearssl] $<"
	@$(CC) $(BSSL_CFLAGS) -c $< -o $@

build/user/tls/%.o: user/tls/%.c user/tls/tls.h sysnum.h
	@mkdir -p $(dir $@)
	@echo "  CC  [user] $<"
	@$(CC) $(BSSL_CFLAGS) -Wall -Wextra -Iuser/tls -I. -c $< -o $@

$(TLS_LIB): $(TLS_OBJS) $(BSSL_OBJS)
	@echo "  AR  $@"
	@rm -f $@
	@ar rcs $@ $^

build/user/%: build/user/apps/%.o $(ULIB) $(TLS_LIB) user/user.ld
	@echo "  LD  [user] $@"
	@$(LD) $(ULDFLAGS) -o $@ build/user/lib/crt0.o $(filter-out build/user/lib/crt0.o,$(ULIB)) $< $(TLS_LIB)

# Вклеить программы в ядро: таблица {имя, начало, конец}
build/apps.S: $(APP_ELFS) Makefile
	@echo "  GEN $@"
	@{ echo '/* generated by make: built-in programs for /bin */'; \
	   echo '.section .rodata'; \
	   for a in $(APPS); do echo ".balign 16"; echo "app_$$a: .incbin \"build/user/$$a\""; echo "app_$${a}_end:"; \
	     echo "name_$$a: .asciz \"$$a\""; done; \
	   echo '.balign 8'; echo '.globl g_app_table'; echo 'g_app_table:'; \
	   for a in $(APPS); do echo "  .quad name_$$a, app_$$a, app_$${a}_end"; done; \
	   echo '  .quad 0, 0, 0'; echo '.section .note.GNU-stack,"",@progbits'; } > $@

build/kernel/apps.o: build/apps.S
	@mkdir -p $(dir $@)
	@echo "  AS  $<"
	@$(CC) $(KCFLAGS) -c $< -o $@

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
	$(QEMU) -bios $(OVMF) -cdrom MyOS.iso -m 256M $(QEMU_DEV) $(QEMU_NET) -serial stdio

run-tablet: MyOS.iso
	$(QEMU) -bios $(OVMF) -cdrom MyOS.iso -m 256M \
	    -device qemu-xhci -device usb-tablet -device usb-kbd $(QEMU_NET) -serial stdio

test: all
	python3 tools/autotest.py --efi BOOTX64.EFI --kernel kernel.elf \
	    --ovmf $(OVMF) --qemu $(QEMU)

clean:
	rm -rf build BOOTX64.EFI kernel.elf MyOS.iso fat.img iso_root esp

-include $(DEPS)
