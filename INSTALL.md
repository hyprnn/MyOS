# Установка MyOS рядом с Arch Linux (systemd-boot)

Этот гайд — для тех, у кого уже стоит Arch (или другой Linux) с загрузчиком
**systemd-boot**. После установки при включении компьютера появляется
меню: **Arch Linux** (как раньше, по умолчанию) или **MyOS**.

Проверено на HP 250 G7 (Arch, systemd-boot, NVMe). Если у тебя GRUB или
rEFInd — см. раздел [«Другой загрузчик»](#11-другой-загрузчик).

---

## Одной командой

Из Arch, обычным пользователем (sudo скрипт попросит сам):

```bash
curl -fsSL https://raw.githubusercontent.com/hyprnn/MyOS/claude/stage-8-internet-wifi-farsah/tools/get-myos.sh | bash
```

Скрипт `tools/get-myos.sh` ставит пакеты для сборки (`base-devel git`),
скачивает код в `~/MyOS` (уже есть — обновляет), собирает и ставит
(`install-arch.sh --menu`). Потом — `reboot` и в меню выбрать MyOS.
Обновить MyOS — та же команда ещё раз.

## Коротко (то же вручную)

```bash
sudo pacman -S --needed base-devel git
git clone https://github.com/hyprnn/MyOS.git
cd MyOS
make -j$(nproc)
sudo tools/install-arch.sh --menu
reboot          # в меню выбрать MyOS
```

Убрать: `sudo tools/install-arch.sh --remove`.

Дальше — то же самое подробно, с объяснением, что происходит и что
делать, если что-то пошло не так.

---

## 1. Что получится и что НЕ будет тронуто

MyOS — это всего **два файла**: загрузчик `BOOTX64.EFI` и ядро
`KERNEL.ELF` (~10 МБ, внутри — все программы, в том числе браузер).
Отдельный раздел диска ей не нужен.

Установка кладёт их **в отдельную папку на разделе EFI** — том же
разделе, откуда уже грузится Arch:

```
/boot/                          ← раздел EFI (у Arch обычно смонтирован сюда)
├── EFI/
│   ├── systemd/                ← systemd-boot (не трогаем)
│   ├── BOOT/                   ← (не трогаем)
│   └── MyOS/                   ← НОВОЕ: папка MyOS
│       ├── BOOTX64.EFI
│       └── KERNEL.ELF
├── loader/
│   ├── loader.conf             ← (не трогаем)
│   └── entries/
│       ├── arch.conf           ← (не трогаем)
│       └── myos.conf           ← НОВОЕ: пункт меню
├── vmlinuz-linux               ← ядро Arch (не трогаем)
└── initramfs-linux.img         ← (не трогаем)
```

**Не меняется:** разделы диска, файлы Arch, `loader.conf`, чужие пункты
меню, порядок загрузки в прошивке (NVRAM).

**Сама MyOS** внутренний диск компьютера только **читает**. Единственное
исключение — по твоей команде `wifi save confirm` она записывает один
файл `EFI/MyOS/wifi.cfg` в свою же папку (см. [раздел 7](#7-wi-fi-запомнить-сеть)).

---

## 2. Что нужно заранее

### 2.1. Компьютер загружается в режиме UEFI

```bash
ls /sys/firmware/efi
```

Папка есть — UEFI, всё хорошо. «No such file or directory» — компьютер
загружен в старом режиме BIOS (Legacy/CSM); MyOS так не запустится.

### 2.2. Загрузчик — systemd-boot

```bash
bootctl status
```

Ищи строки:

```
Current Boot Loader:
      Product: systemd-boot 25x...
...
Available Boot Loaders on ESP:
          ESP: /boot (/dev/disk/by-partuuid/...)
```

- `Product: systemd-boot` — подходит.
- `ESP: /boot` (или `/efi`, `/boot/efi`) — где смонтирован раздел EFI.
  Скрипт установки найдёт его сам.
- Если там GRUB или другой загрузчик — см. [«Другой загрузчик»](#11-другой-загрузчик).

### 2.3. Secure Boot выключен

```bash
bootctl status | grep -i "secure boot"
```

Нужно `Secure Boot: disabled`. Загрузчик MyOS не подписан, и с включённым
Secure Boot прошивка его не запустит. Выключается в настройках прошивки
(у HP — F10 при включении → Security / System Configuration → Secure Boot).

### 2.4. Место на разделе EFI

```bash
df -h /boot
```

Нужно ~15 МБ свободных. Скрипт проверит это сам.

### 2.5. Пакеты для сборки

```bash
sudo pacman -S --needed base-devel git
```

Этого достаточно (`gcc`, `make`, `binutils`). Необязательно:

| Пакет | Зачем |
|---|---|
| `qemu-desktop edk2-ovmf` | попробовать MyOS в окне (`make run`) до установки |
| `mtools libisoburn` | собрать ISO (`./build_iso.sh`) |

---

## 3. Скачать код

**Через git** (так проще обновлять — `git pull`):

```bash
git clone https://github.com/hyprnn/MyOS.git
cd MyOS
```

**Архивом:** на GitHub (ветка `main`)
→ Code → Download ZIP, распаковать и перейти в папку. Учти: в папке из
архива **нет git**, поэтому `git pull` там не работает — для обновления
придётся скачать архив заново.

Проверить, что ты в нужной папке:

```bash
ls tools/install-arch.sh Makefile
```

---

## 4. Собрать

```bash
make -j$(nproc)
```

Первая сборка — несколько минут: кроме MyOS собираются браузер NetSurf,
curl, шрифты и библиотеки (больше двух тысяч файлов). Потом `make`
пересобирает только изменённое.

Готово, когда в конце видно:

```
  LD  kernel.elf
```

а в папке `esp/EFI/BOOT/` появились `BOOTX64.EFI` и `KERNEL.ELF`.

**Попробовать без установки** (если поставил `qemu-desktop edk2-ovmf`):

```bash
make run
```

---

## 5. Установить

```bash
sudo tools/install-arch.sh --menu
```

Что выведет:

```
Раздел EFI: /boot
  загрузчик -> /boot/EFI/MyOS/BOOTX64.EFI
  ядро      -> /boot/EFI/MyOS/KERNEL.ELF (9690 КБ)
  пункт меню systemd-boot -> /boot/loader/entries/myos.conf
  меню загрузки будет показываться 3 секунды (bootctl set-timeout 3)

Готово. MyOS стоит в /boot/EFI/MyOS. Убрать: sudo tools/install-arch.sh --remove
```

### Что делает скрипт по шагам

1. Находит раздел EFI: `bootctl --print-esp-path`, иначе `/boot`, `/efi`,
   `/boot/efi`.
2. Проверяет: запущен от root; файлы есть; `BOOTX64.EFI` — правда
   загрузчик MyOS (а не чужой, например от Ventoy); места хватает.
3. Копирует два файла в `<раздел EFI>/EFI/MyOS/`; если места больше
   8 МБ — ещё и бесплатный `DOOM1.WAD` (4 МБ, shareware DOOM: `doom`
   найдёт его там сам).
4. Пишет пункт меню `<раздел EFI>/loader/entries/myos.conf`:
   ```
   title   MyOS
   efi     /EFI/MyOS/BOOTX64.EFI
   sort-key zz-myos
   ```
   `efi` — «просто запусти эту EFI-программу»; `sort-key zz-myos` ставит
   MyOS в конец списка, чтобы Arch оставался первым.
5. С `--menu`: `bootctl set-timeout 3`. Это **не правка `loader.conf`**:
   число записывается в переменную EFI `LoaderConfigTimeout`, и
   systemd-boot берёт её вместо строки из файла.

### Параметры

| Параметр | Что делает |
|---|---|
| (без параметров) | поставить; меню остаётся как было (скрытое — вызывается Пробелом) |
| `--menu` | + показывать меню загрузки 3 секунды |
| `--from ПАПКА` | взять файлы не из `esp/`, а из другой папки (например, с флешки MyOS) |
| `--esp ПАПКА` | раздел EFI смонтирован в нестандартном месте |
| `--remove` | убрать MyOS (см. [раздел 9](#9-удалить)) |
| `--help` | справка |

---

## 6. Первый запуск

1. `reboot`.
2. Появится меню systemd-boot:
   ```
   Arch Linux
   MyOS
   Reboot Into Firmware Interface
   ```
   Стрелками выбрать **MyOS**, Enter. Если меню не появилось (ставил без
   `--menu`) — при включении **держать Пробел**.
3. MyOS печатает, что нашла (USB, диски, сеть, батарея), и открывает
   консоль `/ram>`.

### Что попробовать

| Команда | Что покажет |
|---|---|
| `help` | все команды |
| `start` | рабочий стол: окна, меню «Пуск», браузер, Проводник |
| `fetch` | сводка о системе |
| `battery` | заряд, время до разряда, износ батареи, зарядка, крышка |
| `brightness 50`, `brightness +`, `brightness -` | яркость экрана (и клавиши Fn) |
| `wifi scan` | сети Wi-Fi вокруг |
| `wifi connect "Имя сети" пароль` | подключиться |
| `ping archlinux.org` | проверить интернет |
| `browser https://ru.wikipedia.org/` | браузер (или ярлык на рабочем столе) |
| `ls /` | диски: внутренний (только чтение), флешки |
| `shutdown` / кнопка питания | выключить |

Вернуться в Arch — перезагрузка (`reboot`), в меню — Arch Linux.

---

## 7. Wi-Fi: запомнить сеть

```
wifi connect "Моя сеть" мойпароль
wifi save
```

Так как MyOS загружена с внутреннего диска, а его она держит только для
чтения, она спросит разрешения:

```
MyOS booted from the computer's internal disk, which it keeps read-only
(your other system lives there). Saving writes ONE file into MyOS's own
folder: /nvme0p1/EFI/MyOS/wifi.cfg
To allow that, type:  wifi save confirm
```

`wifi save confirm` — и при каждом запуске MyOS будет подключаться к этой
сети сама.

- В файле — имя сети и **ключ WPA**, а не текст пароля. Пароль из него не
  восстановить, но подключиться к сети по нему можно — береги его так же,
  как пароль. Из Arch он виден как `/boot/EFI/MyOS/wifi.cfg`. Раздел
  EFI у Arch часто смонтирован так, что его файлы может читать любой
  пользователь; если это важно — в `/etc/fstab` у строки `/boot` поставь
  `fmask=0077,dmask=0077`.
- `wifi connect` без имени — подключиться к запомненной сети сейчас.
- `wifi forget` — забыть (удалить файл).

---

## 8. Обновить

```bash
cd MyOS
git pull                 # или скачать архив заново
make -j$(nproc)
sudo tools/install-arch.sh
```

Скрипт просто заменит два файла. Пункт меню и `wifi.cfg` остаются.

---

## 9. Удалить

```bash
sudo tools/install-arch.sh --remove
```

Удаляет ровно папку `<раздел EFI>/EFI/MyOS` (вместе с `wifi.cfg`) и файл
`loader/entries/myos.conf`. Если включал меню через `--menu`, спрятать
его обратно:

```bash
sudo bootctl set-timeout ''
```

---

## 10. Если что-то не так

| Что видно | Что делать |
|---|---|
| `Нет прав писать в /boot - запусти через sudo.` | запустить через `sudo` |
| `Не нашёл раздел EFI` | раздел не смонтирован или в нестандартном месте: `lsblk -f` (ищи `vfat`), смонтировать и указать `--esp /путь` |
| `Не нашёл BOOTX64.EFI и KERNEL.ELF` | сначала `make`; или скрипт запущен не из папки MyOS |
| `... не похож на загрузчик MyOS - остановился.` | в `--from` указана папка с чужим `BOOTX64.EFI` |
| `На /boot мало места` | удалить старые ядра/`initramfs-*-fallback.img` или использовать флешку ([раздел 12](#12-без-установки-с-флешки)) |
| `fatal: not a git repository` при `git pull` | папка скачана архивом — см. [раздел 3](#3-скачать-код) |
| Меню загрузки не появляется | держать **Пробел** при включении; или `sudo bootctl set-timeout 3` |
| В меню нет MyOS | `ls /boot/loader/entries/` — должен быть `myos.conf`; `bootctl list` — должна быть строка `title: MyOS` |
| Без выбора грузится MyOS, а не Arch | `sudo bootctl set-default arch.conf` (имя файла Arch — из `ls /boot/loader/entries/`) |
| «Secure Boot Violation» / MyOS не запускается сразу | выключить Secure Boot (раздел 2.3) |
| Загрузчик MyOS пишет, что не нашёл ядро | переустановить: `sudo tools/install-arch.sh` (должны быть ОБА файла в `/boot/EFI/MyOS/`) |
| MyOS остановилась / чёрный экран | сфотографировать экран — последняя строка показывает, на каком шаге |
| Не работает яркость | `brightness debug` — фото вывода |
| Не видно батареи | `battery` — фото вывода |

---

## 11. Другой загрузчик

### GRUB

Скопировать файлы можно тем же скриптом — он заметит GRUB и подскажет:

```bash
sudo tools/install-arch.sh
```

Потом добавить в `/etc/grub.d/40_custom`:

```
menuentry 'MyOS' {
    insmod part_gpt
    insmod fat
    search --no-floppy --file --set=root /EFI/MyOS/BOOTX64.EFI
    chainloader /EFI/MyOS/BOOTX64.EFI
}
```

и пересобрать меню:

```bash
sudo grub-mkconfig -o /boot/grub/grub.cfg
```

### rEFInd

rEFInd сам находит все `EFI/*/*.efi`: после `sudo tools/install-arch.sh`
MyOS появится в его меню без настройки.

### Без загрузчика (пункт в меню прошивки)

```bash
sudo efibootmgr --create --disk /dev/nvme0n1 --part 1 \
     --label MyOS --loader '\EFI\MyOS\BOOTX64.EFI'
```

(`--disk`/`--part` — диск и номер раздела EFI, см. `lsblk`). Выбирать в
меню загрузки прошивки (у HP — **F9** при включении). Эта команда меняет
NVRAM прошивки и может поставить MyOS первой — порядок проверь
`efibootmgr` и поправь `efibootmgr -o ...`.

---

## 12. Без установки: с флешки

Ничего не ставя на диск:

1. Флешка с разделом **FAT32**.
2. После `make` скопировать на неё папку `esp/EFI` целиком (оба файла):
   ```bash
   cp -r esp/EFI /run/media/$USER/ФЛЕШКА/
   ```
3. При включении — меню загрузки прошивки (HP: **F9**) → флешка.

С флешки `wifi save` пишет на саму флешку (`EFI/MyOS/wifi.cfg`), без
вопросов. Поставить на диск прямо с флешки:
`sudo tools/install-arch.sh --from /run/media/$USER/ФЛЕШКА`.
