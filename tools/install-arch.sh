#!/bin/bash
# tools/install-arch.sh - поставить MyOS на диск рядом с Arch (этап 9).
#
# Запускать ИЗ ARCH (или другого Linux) от root:
#
#   sudo tools/install-arch.sh                 поставить (файлы из esp/ после make)
#   sudo tools/install-arch.sh --from /run/media/user/STICK
#                                              взять файлы с флешки MyOS
#   sudo tools/install-arch.sh --menu          + показывать меню загрузки 3 секунды
#   sudo tools/install-arch.sh --remove        убрать MyOS с диска
#   tools/install-arch.sh --esp /tmp/esp ...   другой раздел EFI (для проверки)
#
# ЧТО ДЕЛАЕТ (и ничего больше):
#   * кладёт BOOTX64.EFI и KERNEL.ELF в <раздел EFI>/EFI/MyOS/ - своя
#     папка, файлы Arch, Kali и systemd-boot не трогаются. Загрузчик MyOS
#     сам ищет ядро в \EFI\MYOS\;
#   * если загрузчик - systemd-boot (есть <раздел EFI>/loader/entries/),
#     добавляет пункт меню loader/entries/myos.conf - один новый файл;
#   * --menu: у тебя меню скрыто (timeout 0). Скрипт не правит
#     loader.conf, а просит systemd-boot показывать меню 3 секунды
#     через переменную EFI ("bootctl set-timeout 3"). Вернуть как было:
#     "sudo bootctl set-timeout ''". Без --menu меню открывается, если
#     при включении держать Пробел.
#   * --remove удаляет ровно эту папку и этот файл.
#
# Раздел EFI у HP 250 G7 с Arch - nvme0n1p1, в Arch он смонтирован в
# /boot (FAT32, ~1 ГБ, занято ~30%). MyOS нужно ~10 МБ.

set -e

ESP=""
FROM=""
MENU=0
REMOVE=0

while [ $# -gt 0 ]; do
    case "$1" in
        --esp)    ESP="$2"; shift 2 ;;
        --from)   FROM="$2"; shift 2 ;;
        --menu)   MENU=1; shift ;;
        --remove) REMOVE=1; shift ;;
        -h|--help)
            sed -n '2,30p' "$0" | sed 's/^# \{0,1\}//'
            exit 0 ;;
        *) echo "непонятный параметр: $1 (см. --help)"; exit 1 ;;
    esac
done

say() { echo "  $*"; }

# ---- где раздел EFI ----
if [ -z "$ESP" ]; then
    if command -v bootctl >/dev/null 2>&1; then
        ESP="$(bootctl --print-esp-path 2>/dev/null || true)"
    fi
    [ -z "$ESP" ] && [ -d /boot/EFI ] && ESP=/boot
    [ -z "$ESP" ] && [ -d /efi/EFI ] && ESP=/efi
    [ -z "$ESP" ] && [ -d /boot/efi/EFI ] && ESP=/boot/efi
fi

if [ -z "$ESP" ] || [ ! -d "$ESP/EFI" ]; then
    echo "Не нашёл раздел EFI (ищу /boot, /efi, /boot/efi). Укажи: --esp /путь"
    exit 1
fi

if [ ! -w "$ESP" ]; then
    echo "Нет прав писать в $ESP - запусти через sudo."
    exit 1
fi

DEST="$ESP/EFI/MyOS"
ENTRY="$ESP/loader/entries/myos.conf"

echo "Раздел EFI: $ESP"

# ---- удалить ----
if [ "$REMOVE" = 1 ]; then
    [ -d "$DEST" ]  && { rm -rf "$DEST";  say "удалена папка $DEST"; }
    [ -f "$ENTRY" ] && { rm -f "$ENTRY"; say "удалён пункт меню $ENTRY"; }
    echo "MyOS убрана. Меню загрузки (если включали --menu) вернуть:"
    echo "  sudo bootctl set-timeout ''"
    exit 0
fi

# ---- откуда брать файлы ----
if [ -z "$FROM" ]; then
    FROM="$(cd "$(dirname "$0")/.." && pwd)/esp"
fi

SRC_EFI=""
SRC_KERNEL=""
for d in "$FROM/EFI/BOOT" "$FROM/EFI/MyOS" "$FROM/EFI/MYOS" "$FROM"; do
    for f in BOOTX64.EFI bootx64.efi; do
        [ -z "$SRC_EFI" ] && [ -f "$d/$f" ] && SRC_EFI="$d/$f"
    done
    for f in KERNEL.ELF kernel.elf; do
        [ -z "$SRC_KERNEL" ] && [ -f "$d/$f" ] && SRC_KERNEL="$d/$f"
    done
done

if [ -z "$SRC_EFI" ] || [ -z "$SRC_KERNEL" ]; then
    echo "Не нашёл BOOTX64.EFI и KERNEL.ELF в $FROM"
    echo "(сначала make - файлы появятся в esp/EFI/BOOT/, или --from <флешка MyOS>)"
    exit 1
fi

# проверка, что это правда MyOS, а не чужой BOOTX64.EFI (например, Ventoy)
if ! grep -q "MyOS" "$SRC_EFI" 2>/dev/null; then
    echo "$SRC_EFI не похож на загрузчик MyOS - остановился."
    exit 1
fi

# ---- место ----
NEED_KB=$(( ( $(stat -c %s "$SRC_EFI") + $(stat -c %s "$SRC_KERNEL") ) / 1024 + 1024 ))
FREE_KB=$(df -Pk "$ESP" | awk 'NR==2 {print $4}')
if [ -n "$FREE_KB" ] && [ "$FREE_KB" -lt "$NEED_KB" ]; then
    echo "На $ESP мало места: свободно ${FREE_KB} КБ, нужно ${NEED_KB} КБ."
    exit 1
fi

# ---- копировать ----
mkdir -p "$DEST"
cp "$SRC_EFI" "$DEST/BOOTX64.EFI"
cp "$SRC_KERNEL" "$DEST/KERNEL.ELF"
sync
say "загрузчик -> $DEST/BOOTX64.EFI"
say "ядро      -> $DEST/KERNEL.ELF ($(( $(stat -c %s "$DEST/KERNEL.ELF") / 1024 )) КБ)"

# DOOM (бесплатная shareware-версия, 4 МБ) - если есть рядом и хватает места
WAD=""
for f in DOOM1.WAD doom1.wad; do
    [ -z "$WAD" ] && [ -f "$FROM/$f" ] && WAD="$FROM/$f"
done
if [ -n "$WAD" ]; then
    FREE_KB=$(df -Pk "$ESP" | awk 'NR==2 {print $4}')
    if [ -z "$FREE_KB" ] || [ "$FREE_KB" -gt 8192 ]; then
        cp "$WAD" "$DEST/DOOM1.WAD" && sync
        say "DOOM      -> $DEST/DOOM1.WAD"
    else
        say "DOOM1.WAD не скопирован: на $ESP меньше 8 МБ свободно"
    fi
fi

# ---- пункт меню ----
if [ -d "$ESP/loader/entries" ]; then
    cat > "$ENTRY" <<'EOF'
# MyOS - добавлено tools/install-arch.sh; убрать: sudo tools/install-arch.sh --remove
title   MyOS
efi     /EFI/MyOS/BOOTX64.EFI
sort-key zz-myos
EOF
    say "пункт меню systemd-boot -> $ENTRY"

    if [ "$MENU" = 1 ]; then
        if command -v bootctl >/dev/null 2>&1 && bootctl set-timeout 3 2>/dev/null; then
            say "меню загрузки будет показываться 3 секунды (bootctl set-timeout 3)"
        else
            say "не получилось включить меню через bootctl - держи Пробел при включении"
        fi
    else
        echo
        echo "Меню systemd-boot у тебя, скорее всего, скрыто (timeout 0):"
        echo "  при включении держи Пробел - появится меню, выбери MyOS;"
        echo "  или запусти ещё раз с --menu, чтобы меню показывалось само."
    fi
elif [ -d "$ESP/EFI/grub" ] || [ -d /boot/grub ]; then
    echo
    echo "Похоже, загрузчик - GRUB. Добавь в /etc/grub.d/40_custom:"
    echo "  menuentry 'MyOS' { insmod part_gpt; insmod fat; search --file --set=root /EFI/MyOS/BOOTX64.EFI; chainloader /EFI/MyOS/BOOTX64.EFI }"
    echo "и выполни: sudo grub-mkconfig -o /boot/grub/grub.cfg"
else
    echo
    echo "Не нашёл systemd-boot. Пункт в меню прошивки можно добавить так:"
    echo "  sudo efibootmgr --create --disk /dev/nvme0n1 --part 1 --label MyOS --loader '\\EFI\\MyOS\\BOOTX64.EFI'"
fi

echo
echo "Готово. MyOS стоит в $DEST. Убрать: sudo $0 --remove"
