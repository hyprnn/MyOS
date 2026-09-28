# Контекст для продолжения: MyOS — hobby UEFI OS

Вставь это первым сообщением новому ИИ и **приложи весь `MyOS.zip`**
(без исходника это только описание, а не рабочий контекст).

## Что это за проект

`MyOS` — самодельная 64-битная UEFI-ОС на чистом C, без GNU-EFI и
сторонних библиотек. Всё в одном `main.c` (~19 100 строк) + свой
`efi.h`. Сборка: `./build.sh` (gcc + ld → `BOOTX64.EFI`),
`./build_iso.sh` (mtools + xorriso → `MyOS.iso`). Пользователь на Arch
Linux (fish), тестирует в QEMU:

```bash
qemu-system-x86_64 -bios /usr/share/edk2/x64/OVMF.4m.fd -cdrom MyOS.iso -m 256M \
    -device qemu-xhci -device usb-mouse -device usb-kbd
```

Пользователь **не программист в этой теме**: код и решения пишет ИИ,
пользователь собирает, запускает и присылает скриншоты. Просит
«делай максимум», минимум вопросов. Общение по-русски.

## Текущее состояние (всё проверено в QEMU)

Два режима работы:

1. **Режим прошивки** (как раньше): шелл и GUI (`start`) поверх UEFI
   Boot Services.
2. **Kernel mode** — команда `ebs`. Вызывает ExitBootServices и
   продолжает работать на своём коде: тот же шелл, тот же GUI,
   мышь/клавиатура через свои драйверы. Блок `KERNEL MODE` в main.c
   (ищи `KERNEL MODE: ОС продолжает жить ПОСЛЕ ExitBootServices`):
   - `kcon_*` — текстовая консоль в framebuffer, шрифт Spleen 8x16
     (BSD-2, лицензия в комментарии), 16 цветов UEFI, прокрутка,
     курсор, **отложенная отрисовка** (`kcon_flush` при ожидании
     ввода/Stall и не реже раза в 30 мс);
   - `kx_load_gdt/kx_load_idt` — своя GDT (0x08 код, 0x10 данные), IDT
     на 256 векторов (asm-заглушки по 16 байт, общий `kx_isr_common`
     с fxsave), экран паники `kx_panic` для исключений 0–31, `int3`
     возвращается;
   - 8259 PIC перенастроен на 0x20–0x2F и замаскирован, все записи
     I/O APIC (0xFEC00000) замаскированы;
   - TSC калибруется по PIT канал 2 (контроль — замер через Stall до
     выхода; если PIT мёртв — берётся замер Stall); Local APIC timer
     (xAPIC или x2APIC) периодический 1000 Гц, вектор 0x40; `hlt` в
     ожидании;
   - `pmm_*` — битовая карта страниц по итоговой карте памяти
     (только EfiConventionalMemory, первый 1 МиБ не выдаётся);
     `kpool_*` — пул поверх страниц (под AllocatePool);
   - `kx_*` — неблокирующий xHCI-драйвер: все корневые порты, кольца
     с Link TRB (`kx_ring_push`), общий диспетчер событий
     (`kx_handle_async_event`), ожидание без потери чужих событий
     (`kx_wait_event`), восстановление после ошибок (Reset Endpoint +
     Set TR Dequeue), Scratchpad Buffers, EP0 MPS через Evaluate
     Context. HID: клавиатура — boot protocol; мышь — по Report
     Descriptor (`hid_parse_report_descriptor`, теперь с раздельными
     смещениями на каждый Report ID), запасной вариант boot protocol;
     абсолютные устройства (usb-tablet) поддержаны;
   - `kbd_*` — очередь EFI_INPUT_KEY, перевод HID Usage → UEFI-клавиши
     (US раскладка), программный автоповтор для USB; `ps2_*` — PS/2
     клавиатура (i8042, опрос, Set 1);
   - `kx_install_shims` — своя `EFI_SYSTEM_TABLE g_kst`: ConIn/ConOut/
     BootServices (Stall, Allocate/FreePool, Allocate/FreePages,
     LocateProtocol → свой GOP и свой SimplePointer) — шелл и GUI
     работают без правок. `g_st` в efi_main переключается на `g_kst`.

Команды: `ebs`, `mousetest` (живая диагностика USB-ввода: отчёты, ошибки,
состояние конечной точки у драйвера и у контроллера, сырые байты
последнего отчёта, USBSTS), `ebsdemo` (старое пошаговое демо одной мыши, потом
перезагрузка), `kinfo`, `usb`, `mem`, `int3`, `crash` (ud2 → экран
паники). В kernel mode `xhci` и `ebsdemo` запрещены.

## Известные ограничения / следующие шаги

- USB-хабы и горячее подключение не поддерживаются (события порта
  только считаются); светодиоды CapsLock не зажигаются.
- Нет своих таблиц страниц: работаем на identity-mapping прошивки,
  поэтому память BootServicesCode/Data ещё не забрана в аллокатор.
- Нет TSS (IST для double fault) и нет ACPI (адрес I/O APIC взят
  стандартный, а не из MADT).
- Тачпады ноутбуков (I2C HID / PS/2 aux) не поддерживаются.
- `erase_input_line defined but not used` — старое безобидное
  предупреждение, других быть не должно.

## Производительность GUI

- `build.sh` собирает с `-O2 -fno-strict-aliasing
  -fno-tree-loop-distribute-patterns`; свои `memcpy/memmove/memset/
  memcmp` в начале main.c (компилятор может вставить их вызовы).
- В GUI курсор рисуется отдельно от кадра (`g_gui_draw_cursor`,
  `gui_draw_cursor_at`, `gui_blit_rect`, `gui_hover_key`): задний
  буфер без курсора, при движении мыши перерисовываются только два
  квадрата 12x12; полный кадр - только если меняется то, что под
  курсором (подсветка пункта меню, клетка Сапёра).

## Правила работы с кодом

- Стиль: многословный явный C, подробные комментарии **на русском**,
  объясняющие «почему». Никакого libc; не присваивать большие
  структуры целиком (компилятор может вставить memcpy, которого нет).
- Две системы печати: `print/print_uint/print_hex` (CHAR16 через
  `out->OutputString`) и `gui_*_to_str` (char* для пиксельного шрифта
  GUI). Консоль kernel mode — это тоже `out`, печатать через print.
- Новые команды — в `run_command()` как `else if (streq(line, ...))`
  + строка в `help`.
- Проверка сборки: `gcc -O2 -fno-strict-aliasing -fno-tree-loop-distribute-patterns -ffreestanding -fno-stack-protector
  -fno-stack-check -fshort-wchar -mno-red-zone -fpic
  -maccumulate-outgoing-args -fno-ident -Wall -Wextra -c main.c`,
  потом `bash build.sh`, отдать пользователю весь `MyOS.zip`.
- Если «не работает» после изменений — сначала попросить проверить,
  что запускается свежий образ
  (`strings BOOTX64.EFI | grep <уникальная строка>`).
- Тестовый стенд: QEMU с `-bios OVMF.fd` без pflash пишет файл
  `NvVars` на загрузочный диск; при смене `-machine` старый NvVars
  может отправить загрузку в UEFI Shell — нужен свежий образ.

## Открытая проблема (на момент передачи)

На реальном ноутбуке пользователя (HP 250 G7, беспроводная мышь
Onikuma через USB-донгл) после `ebs` мышь «чуть двигается и
останавливается»; в QEMU такого не воспроизводится (проверено
>700 отчётов подряд в шелле и в GUI). Сделано вслепую:
ошибки конечной точки теперь считаются «подряд», а не «за всё
время» (раньше после 200 ошибок мышь выключалась навсегда),
Missed Service Error (cc=21) не вызывает Reset Endpoint, сторож
на зависшее восстановление, парсер понимает X/Y через Usage
Minimum/Maximum. Если не помогло — нужен скриншот `mousetest`
в момент, когда мышь встала. В QEMU «упирание в стенку» — это
относительная мышь без захвата окна; для ВМ рекомендован
`-device usb-tablet`.

Обновление: по видео с ноутбука курсор в GUI стоял на месте. Теперь
мыши с boot protocol (subclass 1, protocol 2) переводятся
SET_PROTOCOL(Boot) и читаются в фиксированном формате (кнопки, dX, dY,
колесо) - разбор Report Descriptor только для устройств без boot
protocol (планшеты). В `usb`/`mousetest` добавлен счётчик "ignored"
(отчёты, не подошедшие под формат). GUI без `ebs` на HP мыши не
видит (прошивка не публикует Simple Pointer) - выводится подсказка.
