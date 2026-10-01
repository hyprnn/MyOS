#!/bin/bash
# tools/get-myos.sh - скачать, собрать и поставить MyOS одной командой
# (из Arch Linux с systemd-boot). Запускать ОБЫЧНЫМ пользователем - sudo
# скрипт попросит сам (для pacman и для записи в раздел EFI):
#
#   curl -fsSL https://raw.githubusercontent.com/hyprnn/MyOS/claude/stage-8-internet-wifi-farsah/tools/get-myos.sh | bash
#
# Что делает (по шагам, с объяснением на экране):
#   1. проверяет: Arch (pacman), загрузка в режиме UEFI;
#   2. ставит пакеты для сборки: base-devel, git (pacman --needed - то,
#      что уже стоит, не трогается);
#   3. скачивает код в ~/MyOS (уже есть - обновляет: git pull);
#   4. собирает (make) - несколько минут;
#   5. ставит: sudo tools/install-arch.sh --menu - два файла MyOS в
#      свою папку EFI/MyOS на разделе EFI и пункт меню systemd-boot
#      (файлы Arch не трогаются; подробно - INSTALL.md).
# Повторный запуск - обновить MyOS до последней версии.
#
# Настройки (переменные окружения):
#   MYOS_DIR=~/src/MyOS     куда скачать (по умолчанию ~/MyOS)
#   MYOS_BRANCH=main        какую ветку (по умолчанию - ветка с программами Linux)
#   MYOS_NO_MENU=1          не включать показ меню загрузки (держать Пробел)
#   MYOS_ESP=/путь          раздел EFI вручную (обычно находится сам)
#   MYOS_REPO=...           откуда скачивать (по умолчанию github.com/hyprnn/MyOS)
#
# Убрать MyOS: sudo ~/MyOS/tools/install-arch.sh --remove

set -e

REPO="${MYOS_REPO:-https://github.com/hyprnn/MyOS.git}"
BRANCH="${MYOS_BRANCH:-claude/stage-8-internet-wifi-farsah}"
DIR="${MYOS_DIR:-$HOME/MyOS}"

step() { printf '\n\033[1;36m==> %s\033[0m\n' "$*"; }
fail() { printf '\n\033[1;31mОшибка: %s\033[0m\n' "$*" >&2; exit 1; }

# ---- 1. проверки ----
step "1/5 Проверка компьютера"

[ "$(id -u)" -eq 0 ] && fail "запусти без sudo, обычным пользователем - sudo скрипт попросит сам"
command -v pacman >/dev/null 2>&1 || fail "это не Arch Linux (нет pacman). Гайд для других систем - INSTALL.md"
[ -d /sys/firmware/efi ] || fail "компьютер загружен в режиме BIOS (Legacy), а MyOS нужен UEFI"
command -v sudo >/dev/null 2>&1 || fail "нет sudo (pacman -S sudo от root)"
echo "  Arch Linux, UEFI - хорошо"

# ---- 2. пакеты ----
step "2/5 Пакеты для сборки (base-devel, git)"
sudo pacman -S --needed --noconfirm base-devel git

# ---- 3. код ----
step "3/5 Код MyOS -> $DIR (ветка $BRANCH)"

if [ -d "$DIR/.git" ]; then
    cd "$DIR"
    if [ -n "$(git status --porcelain --untracked-files=no)" ]; then
        fail "в $DIR есть твои изменения - не трогаю их. Сохрани (git stash) или укажи другую папку: MYOS_DIR=..."
    fi
    git fetch origin "$BRANCH"
    git checkout -q "$BRANCH" 2>/dev/null || git checkout -q -b "$BRANCH" "origin/$BRANCH"
    git merge -q --ff-only "origin/$BRANCH" || fail "не получилось обновить $DIR (ветка разошлась) - удали папку и запусти снова"
elif [ -e "$DIR" ]; then
    fail "$DIR уже есть, но это не код MyOS - укажи другую папку: MYOS_DIR=..."
else
    git clone --branch "$BRANCH" --depth 50 "$REPO" "$DIR"
    cd "$DIR"
fi

echo "  версия: $(git log -1 --format='%h %s')"

# ---- 4. сборка ----
step "4/5 Сборка (make -j$(nproc)) - несколько минут"
make -j"$(nproc)"

[ -f esp/EFI/BOOT/BOOTX64.EFI ] || [ -f BOOTX64.EFI ] || fail "сборка не дала BOOTX64.EFI"

# ---- 5. установка ----
step "5/5 Установка в раздел EFI (нужен sudo)"

ARGS=()
[ -z "$MYOS_NO_MENU" ] && ARGS+=(--menu)
[ -n "$MYOS_ESP" ] && ARGS+=(--esp "$MYOS_ESP")

sudo tools/install-arch.sh "${ARGS[@]}"

cat <<EOF

Готово! Дальше:
  * перезагрузи компьютер (reboot) и в меню выбери MyOS
    (меню не появилось - держи Пробел при включении);
  * в MyOS: start - рабочий стол, в "Терминале" набери bash -
    это bash твоего Arch (раздел Linux MyOS только читает);
  * обновить MyOS - запусти эту же команду ещё раз;
  * убрать MyOS - sudo $DIR/tools/install-arch.sh --remove
EOF
