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

# Чужой код (third_party: picolibc, curl, NetSurf... - больше двух тысяч
# файлов) по одному не печатаем - только готовые библиотеки (AR).
# make V=1 - печатать и его.
ifeq ($(V),1)
TP_SAY  := @echo
else
TP_SAY  := @true
endif
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

# exFAT - библиотека FatFs (third_party/fatfs, BSD-1-clause) в ядре; свои
# флаги: чужой код без -Wextra, вместо <string.h> glibc - своя заглушка
FATFS_SRCS := third_party/fatfs/ff.c third_party/fatfs/ffunicode.c
FATFS_CFLAGS := $(KCFLAGS) -U_FORTIFY_SOURCE -Wno-extra -Wno-unused-parameter \
             -Ithird_party/fatfs/myos -Ithird_party/fatfs -DFFCONF_H='"ffconf.h"'

# ACPI-байт-код (AML): библиотека uACPI (third_party/uacpi, MIT) в ядре -
# батарея, кнопка питания, крышка, выключение (этап 9). Свои функции ядра
# для неё - kernel/acpi_os.c; её настройки - флаги ниже.
UACPI_SRCS := $(sort $(wildcard third_party/uacpi/source/*.c))
UACPI_DEFS := -DUACPI_NATIVE_ALLOC_ZEROED
UACPI_CFLAGS := $(KCFLAGS) -U_FORTIFY_SOURCE -Ithird_party/uacpi/include $(UACPI_DEFS)

LOBJS   := $(LSRCS:%.c=build/loader/%.o)
KOBJS   := $(KSRCS:%.c=build/kernel/%.o) build/kernel/apps.o build/kernel/firmware.o \
           $(FATFS_SRCS:%.c=build/kernel/%.o) $(UACPI_SRCS:%.c=build/kernel/%.o)
DEPS    := $(LOBJS:.o=.d) $(KSRCS:%.c=build/kernel/%.d) $(UACPI_SRCS:%.c=build/kernel/%.d)

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
# Мини-libc - архивом: компоновщик берёт из него только те файлы, что
# нужны программе (ls не тянет шрифт окон, hello - команды файлов)
ULIB_A  := build/user/libmyos.a
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

# Полная libc для программ (этап 9): picolibc 1.8.12 (third_party/picolibc,
# BSD) - printf с дробями, FILE *, math.h, time.h, regex, UTF-8... Нужна
# для переноса чужих программ (браузер). Свои флаги - как у сборки самой
# picolibc (meson); список файлов - third_party/picolibc/files.mk.
# Привязка к ядру MyOS (read/write/open/lseek/sbrk/время) - user/posix/os.c.
# Программы на ней - user/posix/apps/*.c (старые, user/apps/, - на мини-libc).
# -nostdinc: заголовки - только picolibc и gcc (stddef.h...), не glibc хоста.
PICO_DIR   := third_party/picolibc
include $(PICO_DIR)/files.mk
GCC_INC    := $(shell $(CC) -print-file-name=include)
PICO_INC   := -nostdinc -isystem $(PICO_DIR)/include -isystem $(GCC_INC) -D__myos__
PICO_CFLAGS:= -m64 -fno-pic -fno-pie -ffreestanding -std=c18 -O2 -D_FILE_OFFSET_BITS=64 \
              -ffunction-sections -fno-common -frounding-math -fsignaling-nans \
              -fstrict-flex-arrays=3 -fno-builtin-copysignl -fno-stack-protector \
              -fno-asynchronous-unwind-tables -fno-ident -D_LIBC -U_FORTIFY_SOURCE \
              -Wall -Wextra -Werror=vla -Warray-bounds -Werror=double-promotion \
              -Wno-missing-braces -Wno-return-type -Wmissing-prototypes \
              -Wmissing-declarations -Werror=implicit-fallthrough=5 \
              -Werror=implicit-function-declaration -Wold-style-definition -Wno-implicit-int \
              -I$(PICO_DIR)/src/libm/common -I$(PICO_DIR)/src/libc/machine/x86 \
              -I$(PICO_DIR)/src/libc/stdio -I$(PICO_DIR)/src/libc/locale \
              -I$(PICO_DIR)/src/libc/string -I$(PICO_DIR)/src -I$(PICO_DIR)/src/libc/include \
              $(PICO_INC)
PICO_OBJS  := $(patsubst $(PICO_DIR)/src/%,build/user/picolibc/%.o,$(PICO_SRCS))
PICO_LIB   := build/user/picolibc/libc.a
LIBGCC     := $(shell $(CC) -print-libgcc-file-name)

# Программы на picolibc
PCFLAGS    := -O2 -fno-stack-protector -fno-stack-check -fno-pic -fno-pie \
              -fno-asynchronous-unwind-tables -fno-ident -D_FILE_OFFSET_BITS=64 \
              -Wall -Wextra -Iuser/posix/include $(PICO_INC)
PLDFLAGS   := -nostdlib -static -z noexecstack -z max-page-size=0x1000 --gc-sections \
              -T user/posix/posix.ld
PAPPS      := $(sort $(basename $(notdir $(wildcard user/posix/apps/*.c))))
# crt0 - всегда; остальное - из архива: программа без сети не тащит socket.o
POSIX_CRT0 := build/user/posix/crt0.o
POSIX_LIB  := build/user/posix/libposix.a
POSIX_OBJS := build/user/posix/os.o build/user/posix/socket.o build/user/posix/fs.o
# Сеть для программ на полной libc (этап 9):
# * BearSSL ещё раз - с picolibc: время - time(), случайные числа -
#   getentropy() (своя правка sysrng.c), AES-NI/SSE2 включены;
# * zlib 1.3.1 (third_party/zlib, лицензия zlib) - сжатие gzip;
# * curl 8.14.1 (third_party/curl, лицензия curl/MIT) - HTTP/HTTPS для
#   браузера; настройки - third_party/curl/myos/curl_config.h. Своя правка:
#   без файла корней curl верит встроенным корням MyOS (user/tls/roots.c).
#   Заодно - программа curl.
PBSSL_CFLAGS:= -O2 -fno-stack-protector -fno-pic -fno-pie -fno-asynchronous-unwind-tables \
              -fno-ident $(PICO_INC) -I$(BSSL_DIR)/inc -I$(BSSL_DIR)/src \
              -DBR_USE_UNIX_TIME=1 -DBR_USE_WIN32_TIME=0 -DBR_USE_URANDOM=0 \
              -DBR_USE_WIN32_RAND=0 -DBR_USE_GETENTROPY=1 -DBR_RDRAND=0 -DBR_AES_X86NI=1 \
              -DBR_SSE2=1 -DBR_POWER8=0 -DBR_64=1 -DBR_LE_UNALIGNED=1
PBSSL_OBJS := $(BSSL_SRCS:$(BSSL_DIR)/src/%.c=build/user/posix/bearssl/%.o) \
              build/user/posix/bearssl/roots.o
PBSSL_LIB  := build/user/posix/libbearssl.a

ZLIB_DIR   := third_party/zlib
ZLIB_SRCS  := $(addprefix $(ZLIB_DIR)/,adler32.c crc32.c deflate.c infback.c inffast.c \
              inflate.c inftrees.c trees.c zutil.c compress.c uncompr.c gzclose.c gzlib.c \
              gzread.c gzwrite.c)
ZLIB_OBJS  := $(ZLIB_SRCS:$(ZLIB_DIR)/%.c=build/user/zlib/%.o)
ZLIB_LIB   := build/user/libz.a
ZLIB_CFLAGS:= $(PCFLAGS) -DHAVE_UNISTD_H -DHAVE_STDARG_H

CURL_DIR   := third_party/curl
include $(CURL_DIR)/files.mk
CURL_BASE  := -O2 -fno-stack-protector -fno-pic -fno-pie -fno-asynchronous-unwind-tables \
              -fno-ident -DHAVE_CONFIG_H -D_GNU_SOURCE -DCURL_STATICLIB \
              -I$(CURL_DIR)/include -I$(CURL_DIR)/myos -I$(CURL_DIR)/lib \
              -Iuser/posix/include $(PICO_INC) -I$(BSSL_DIR)/inc -I$(ZLIB_DIR)
CURL_LIB_CFLAGS := $(CURL_BASE) -DBUILDING_LIBCURL
CURL_TOOL_CFLAGS:= $(CURL_BASE) -I$(CURL_DIR)/lib/curlx -I$(CURL_DIR)/src
CURL_LIB_OBJS := $(CURL_LIB_SRCS:$(CURL_DIR)/%.c=build/user/curlobj/%.o)
CURL_TOOL_OBJS:= $(CURL_TOOL_SRCS:$(CURL_DIR)/%.c=build/user/curlobj/%.o)
CURL_LIB   := build/user/libcurl.a
# всё сетевое для программы на полной libc - в таком порядке при линковке
NET_LIBS   := $(CURL_LIB) $(PBSSL_LIB) $(ZLIB_LIB)

# Браузер NetSurf (этап 9): third_party/netsurf (GPL-2 и MIT), FreeType,
# libpng, libjpeg-turbo, utf8proc - только нужные файлы; правила сборки -
# сгенерированный third_party/netsurf/build.mk (флаги - как у их
# собственной сборки). Ресурсы браузера (тексты, стили, шрифты DejaVu)
# вшиваются в программу и видны ей как файлы /embed/... (user/posix/os.c).
TP         := third_party
NS_OBJ     := build/user/ns
NS_CFLAGS  := -O2 -fno-stack-protector -fno-stack-check -fno-pic -fno-pie \
              -fno-asynchronous-unwind-tables -fno-ident -D_FILE_OFFSET_BITS=64 \
              -Iuser/posix/include $(PICO_INC)
include $(TP)/netsurf/build.mk
.DEFAULT_GOAL := all
# libnsfb - не архивом, а целиком: её "поверхности" (окно MyOS) регистрируются
# конструкторами, на них никто не ссылается - из архива их бы выкинули
NS_LIBS    := $(foreach c,$(filter-out netsurf libnsfb,$(NS_COMPONENTS)),$(NS_OBJ)/lib-$(c).a)
NS_RES     := $(TP)/netsurf/netsurf/resources
NS_FONTS   := $(TP)/fonts/dejavu
# что вшить: имя_в_/embed=файл
NS_EMBED   := Messages=$(NS_RES)/Messages welcome.html=$(NS_RES)/welcome.html \
              credits.html=$(NS_RES)/credits.html licence.html=$(NS_RES)/licence.html \
              default.css=$(NS_RES)/default.css internal.css=$(NS_RES)/internal.css \
              quirks.css=$(NS_RES)/quirks.css adblock.css=$(NS_RES)/adblock.css \
              netsurf.png=$(NS_RES)/netsurf.png favicon.png=$(NS_RES)/favicon.png \
              SearchEngines=$(NS_RES)/SearchEngines \
              $(foreach f,DejaVuSans DejaVuSans-Bold DejaVuSerif DejaVuSerif-Bold \
                DejaVuSansMono DejaVuSansMono-Bold,fonts/$(f).ttf=$(NS_FONTS)/$(f).ttf)

ALL_APPS   := $(sort $(APPS) $(PAPPS) curl browser)
ALL_ELFS   := $(ALL_APPS:%=build/user/%)

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
	@for a in $(ALL_APPS); do cp build/user/$$a esp/APPS/$$(echo $$a | tr a-z A-Z); done

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
	$(TP_SAY) "  CC  [bearssl] $<"
	@$(CC) $(BSSL_CFLAGS) -c $< -o $@

build/user/tls/%.o: user/tls/%.c user/tls/tls.h sysnum.h
	@mkdir -p $(dir $@)
	@echo "  CC  [user] $<"
	@$(CC) $(BSSL_CFLAGS) -Wall -Wextra -Iuser/tls -I. -c $< -o $@

$(TLS_LIB): $(TLS_OBJS) $(BSSL_OBJS)
	@echo "  AR  $@"
	@rm -f $@
	@ar rcs $@ $^

$(ULIB_A): $(filter-out build/user/lib/crt0.o,$(ULIB))
	@echo "  AR  $@"
	@rm -f $@
	@ar rcs $@ $^

build/user/%: build/user/apps/%.o build/user/lib/crt0.o $(ULIB_A) $(TLS_LIB) user/user.ld
	@echo "  LD  [user] $@"
	@$(LD) $(ULDFLAGS) -o $@ build/user/lib/crt0.o $< $(TLS_LIB) $(ULIB_A)

build/user/picolibc/%.o: $(PICO_DIR)/src/%
	@mkdir -p $(dir $@)
	$(TP_SAY) "  CC  [picolibc] $<"
	@$(CC) $(PICO_CFLAGS) -c $< -o $@

$(PICO_LIB): $(PICO_OBJS)
	@echo "  AR  $@"
	@rm -f $@
	@ar rcs $@ $^

build/user/posix/%.o: user/posix/%.c $(wildcard user/posix/include/*.h user/posix/include/*/*.h) sysnum.h
	@mkdir -p $(dir $@)
	@echo "  CC  [posix] $<"
	@$(CC) $(PCFLAGS) -c $< -o $@

build/user/posix/%.o: user/posix/%.S
	@mkdir -p $(dir $@)
	@echo "  AS  [posix] $<"
	@$(CC) $(PCFLAGS) -c $< -o $@

build/user/posix/apps/%.o: user/posix/apps/%.c user/posix/include/myos_sys.h sysnum.h
	@mkdir -p $(dir $@)
	@echo "  CC  [posix] $<"
	@$(CC) $(PCFLAGS) -c $< -o $@

$(POSIX_LIB): $(POSIX_OBJS)
	@echo "  AR  $@"
	@rm -f $@
	@ar rcs $@ $^

$(PAPPS:%=build/user/%): build/user/%: build/user/posix/apps/%.o $(POSIX_CRT0) $(POSIX_LIB) $(PICO_LIB) user/posix/posix.ld
	@echo "  LD  [posix] $@"
	@$(LD) $(PLDFLAGS) -o $@ $(POSIX_CRT0) $< $(POSIX_LIB) $(PICO_LIB) $(LIBGCC) $(POSIX_LIB) $(PICO_LIB)

build/user/posix/bearssl/%.o: $(BSSL_DIR)/src/%.c
	@mkdir -p $(dir $@)
	$(TP_SAY) "  CC  [bearssl-posix] $<"
	@$(CC) $(PBSSL_CFLAGS) -c $< -o $@

build/user/posix/bearssl/roots.o: user/tls/roots.c user/tls/tls.h
	@mkdir -p $(dir $@)
	$(TP_SAY) "  CC  [bearssl-posix] $<"
	@$(CC) $(PBSSL_CFLAGS) -Iuser/tls -I. -c $< -o $@

$(PBSSL_LIB): $(PBSSL_OBJS)
	@echo "  AR  $@"
	@rm -f $@
	@ar rcs $@ $^

build/user/zlib/%.o: $(ZLIB_DIR)/%.c
	@mkdir -p $(dir $@)
	$(TP_SAY) "  CC  [zlib] $<"
	@$(CC) $(ZLIB_CFLAGS) -c $< -o $@

$(ZLIB_LIB): $(ZLIB_OBJS)
	@echo "  AR  $@"
	@rm -f $@
	@ar rcs $@ $^

build/user/curlobj/lib/%.o: $(CURL_DIR)/lib/%.c $(CURL_DIR)/myos/curl_config.h
	@mkdir -p $(dir $@)
	$(TP_SAY) "  CC  [curl] $<"
	@$(CC) $(CURL_LIB_CFLAGS) -c $< -o $@

build/user/curlobj/src/%.o: $(CURL_DIR)/src/%.c $(CURL_DIR)/myos/curl_config.h
	@mkdir -p $(dir $@)
	$(TP_SAY) "  CC  [curl] $<"
	@$(CC) $(CURL_TOOL_CFLAGS) -c $< -o $@

$(CURL_LIB): $(CURL_LIB_OBJS)
	@echo "  AR  $@"
	@rm -f $@
	@ar rcs $@ $^

build/user/curl: $(CURL_TOOL_OBJS) $(NET_LIBS) $(POSIX_CRT0) $(POSIX_LIB) $(PICO_LIB) user/posix/posix.ld
	@echo "  LD  [posix] $@"
	@$(LD) $(PLDFLAGS) -o $@ $(POSIX_CRT0) $(CURL_TOOL_OBJS) $(NET_LIBS) $(POSIX_LIB) $(PICO_LIB) $(LIBGCC) $(POSIX_LIB) $(PICO_LIB)

# NetSurf: библиотеки - по архиву на каждую
define NS_LIB_RULE
$(NS_OBJ)/lib-$(1).a: $$(NS_OBJS_$(1))
	@echo "  AR  $$@"
	@rm -f $$@
	@ar rcs $$@ $$^
endef
$(foreach c,$(NS_COMPONENTS),$(eval $(call NS_LIB_RULE,$(c))))

# вшитые файлы браузера: таблица __myos_embedded_files (myos_sys.h)
$(NS_OBJ)/embed.S: Makefile $(foreach e,$(NS_EMBED),$(lastword $(subst =, ,$(e))))
	@mkdir -p $(dir $@)
	@echo "  GEN $@"
	@{ echo '/* generated by make: files built into the browser (/embed/...) */'; \
	   echo '.section .rodata'; i=0; \
	   for e in $(NS_EMBED); do n=$${e%%=*}; f=$${e#*=}; \
	     echo ".balign 16"; echo "ef_$$i: .incbin \"$$f\""; echo "ef_$${i}_end:"; \
	     echo "en_$$i: .asciz \"$$n\""; i=$$((i+1)); done; \
	   echo '.balign 8'; echo '.globl __myos_embedded_files'; echo '__myos_embedded_files:'; \
	   i=0; for e in $(NS_EMBED); do echo "  .quad en_$$i, ef_$$i, ef_$${i}_end - ef_$$i"; i=$$((i+1)); done; \
	   echo '  .quad 0, 0, 0'; echo '.section .note.GNU-stack,"",@progbits'; } > $@

$(NS_OBJ)/embed.o: $(NS_OBJ)/embed.S
	@echo "  AS  $<"
	@$(CC) -c $< -o $@

# сам браузер (без отладочных символов: -s - он и так большой)
build/user/browser: $(NS_OBJS_netsurf) $(NS_OBJS_libnsfb) $(NS_OBJ)/embed.o $(NS_LIBS) $(NET_LIBS) $(POSIX_CRT0) \
                    $(POSIX_LIB) $(PICO_LIB) user/posix/posix.ld
	@echo "  LD  [posix] $@"
	@$(LD) $(PLDFLAGS) -s -o $@ $(POSIX_CRT0) $(NS_OBJS_netsurf) $(NS_OBJS_libnsfb) $(NS_OBJ)/embed.o \
	    --start-group $(NS_LIBS) $(NET_LIBS) $(POSIX_LIB) $(PICO_LIB) $(LIBGCC) --end-group

# Вклеить программы в ядро: таблица {имя, начало, конец}
build/apps.S: $(ALL_ELFS) Makefile
	@echo "  GEN $@"
	@{ echo '/* generated by make: built-in programs for /bin */'; \
	   echo '.section .rodata'; \
	   for a in $(ALL_APPS); do echo ".balign 16"; echo "app_$$a: .incbin \"build/user/$$a\""; echo "app_$${a}_end:"; \
	     echo "name_$$a: .asciz \"$$a\""; done; \
	   echo '.balign 8'; echo '.globl g_app_table'; echo 'g_app_table:'; \
	   for a in $(ALL_APPS); do echo "  .quad name_$$a, app_$$a, app_$${a}_end"; done; \
	   echo '  .quad 0, 0, 0'; echo '.section .note.GNU-stack,"",@progbits'; } > $@

build/kernel/apps.o: build/apps.S
	@mkdir -p $(dir $@)
	@echo "  AS  $<"
	@$(CC) $(KCFLAGS) -c $< -o $@

# Прошивки устройств (firmware/): вклеиваются в ядро как есть
build/kernel/firmware.o: firmware/firmware.S $(wildcard firmware/*/*.bin)
	@mkdir -p $(dir $@)
	@echo "  AS  $<"
	@$(CC) $(KCFLAGS) -c $< -o $@

build/loader/%.o: %.c
	@mkdir -p $(dir $@)
	@echo "  CC  [loader] $<"
	@$(CC) $(LCFLAGS) -MMD -MP -c $< -o $@

build/kernel/third_party/fatfs/%.o: third_party/fatfs/%.c third_party/fatfs/ffconf.h
	@mkdir -p $(dir $@)
	@echo "  CC  [fatfs] $<"
	@$(CC) $(FATFS_CFLAGS) -c $< -o $@

build/kernel/third_party/uacpi/%.o: third_party/uacpi/%.c
	@mkdir -p $(dir $@)
	$(TP_SAY) "  CC  [uacpi] $<"
	@$(CC) $(UACPI_CFLAGS) -MMD -MP -c $< -o $@

# файлы ядра, которые говорят с uACPI, видят её заголовки
build/kernel/kernel/acpi_os.o build/kernel/kernel/acpi_dev.o build/kernel/kernel/backlight.o: KCFLAGS += -Ithird_party/uacpi/include $(UACPI_DEFS)

build/kernel/fs/exfat.o: fs/exfat.c
	@mkdir -p $(dir $@)
	@echo "  CC  $<"
	@$(CC) $(KCFLAGS) -Ithird_party/fatfs -DFFCONF_H='"ffconf.h"' -MMD -MP -c $< -o $@

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
