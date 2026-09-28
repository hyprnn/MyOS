# Контекст для продолжения: MyOS — hobby UEFI OS

Вставь это первым сообщением новому ИИ и **приложи весь `MyOS.zip`**
(вместе с папкой `.git` — там история).

## Что это за проект

`MyOS` — самодельная 64-битная ОС на чистом C, без GNU-EFI и сторонних
библиотек. Стартует как UEFI-приложение (`BOOTX64.EFI`); команда `ebs`
вызывает ExitBootServices, и дальше ОС работает на своём коде: консоль,
прерывания, таймер, память, USB/PS2-ввод, тот же шелл и GUI.

Пользователь **не программист в этой теме**: код и решения пишет ИИ,
пользователь собирает, запускает в QEMU и на ноутбуке (HP 250 G7, Arch
Linux, fish; беспроводная мышь Onikuma через USB-донгл) и присылает
скриншоты/видео. Просит «делай максимум», минимум вопросов. Общение
по-русски.

Есть документ-план «MyOS: план пути к настоящей ОС» (10 этапов).
**Этап 0 (порядок в коде) выполнен.** Следующий — этап 1: отдельный
загрузчик + `kernel.elf`, свои таблицы страниц, kmalloc, TSS/IST.

## Сборка, запуск, тест

```bash
make            # BOOTX64.EFI, инкрементально
make run        # ISO + QEMU, лог COM1 в терминал (-serial stdio)
make run-tablet # мышь-планшет (без «стенок» в окне QEMU)
make test       # tools/autotest.py: QEMU без окна, ~10 с
```
`./build.sh` вызывает `make`. OVMF по умолчанию
`/usr/share/edk2/x64/OVMF.4m.fd`. Флаги: `-O2 -fno-strict-aliasing
-fno-tree-loop-distribute-patterns -ffreestanding -fshort-wchar
-mno-red-zone -fpic -fvisibility=hidden ...` — предупреждений быть не
должно (сейчас ноль).

## Структура

| Где | Что |
| --- | --- |
| `main.c` | `efi_main`, главный цикл шелла (берёт таблицу из `g_st` каждую итерацию) |
| `myos.h` | общий заголовок: #define, типы, `extern`, прототипы по модулям, `static inline` (порты, MMIO, rdtsc) в конце; `#pragma GCC visibility push(hidden)` — иначе -fpic даёт ссылки через GOT, которого в PE нет |
| `lib/` | `libc.c` (memcpy/memset — их может вставить компилятор), `string.c`, `kprintf.c` (`kprintf(out,fmt,...)`, `ksnprintf`, `klog` — только в COM1), `serial.c` (COM1 115200, loopback-проверка наличия) |
| `kernel/` | `kcon.c` консоль в framebuffer (Spleen 8x16, отложенная отрисовка `kcon_flush`), `cpu.c` GDT/IDT/ISR/паника/PIC/I/O APIC, `time.c` TSC+PIT+LAPIC timer, `pmm.c` битовая карта страниц + пул, `shim.c` своя EFI_SYSTEM_TABLE `g_kst`, `enter.c` команда `ebs`, `kcmds.c` kinfo/usb/mousetest/mem |
| `drivers/` | `pci.c`, `xhci_common.c` (сброс, disconnect прошивки, BIOS handoff), `usb.c` (неблокирующий xHCI+HID kernel mode), `hid.c` (разбор Report Descriptor), `keyboard.c` (очередь клавиш, HID Usage→UEFI, накопитель мыши), `ps2.c`, `xhci_demo.c` + `ebs_console.c` (старое демо `ebsdemo`) |
| `gui/` | `gui.c` цикл `start`, `desktop.c` (вывод кадра без мигания: задний буфер → теневой буфер → только изменённые пиксели на экран, курсор вклеивается: `gui_present_frame`/`gui_present_cursor`; `gui_hover_key`), `draw.c`, `minesweeper*.c`, `terminal.c`, `gstring.c` |
| `shell/` | `commands.c` (`run_command`), `console.c` (print*, scrollback, дублирование в COM1), `readline.c`, `fs.c` (RAM-диск), `fetch.c`, `editor.c`, `calc.c`, `history.c` |
| `tools/` | `autotest.py` (свой FAT16-образ, QMP, проверки по логу COM1), `split_main.py` (чем резался старый main.c) |

Новые команды шелла: в `shell/commands.c`, `else if (streq(line,
"имя"))` + строка в `help`. Новые функции/переменные модуля — объявить
в `myos.h` в секции своего модуля.

## Kernel mode (после `ebs`) — кратко

GDT (0x08/0x10), IDT 256 векторов (asm-заглушки по 16 байт, fxsave),
экран паники для исключений; PIC перенастроен и замаскирован, I/O APIC
замаскирован; TSC калибруется по PIT (контроль — Stall до выхода);
LAPIC timer 1000 Гц, вектор 0x40, `hlt` в ожидании. PMM — только
EfiConventionalMemory, первый 1 МиБ не выдаётся; DMA для xHCI ниже
4 ГиБ. xHCI: все корневые порты, кольца с Link TRB, общий диспетчер
событий, восстановление после ошибок (ошибки «подряд», cc=21 без
Reset Endpoint, сторож 300 мс), Scratchpad Buffers. HID: клавиатура —
boot protocol; мышь — boot protocol с проверкой GET_PROTOCOL, иначе
разбор дескриптора (несколько Report ID, X/Y через Usage Min/Max,
абсолютные планшеты). `mousetest` — живая диагностика с курсором.

## Состояние мыши на реальном ноутбуке

Мышь (донгл Onikuma) **работает на железе** после перехода на boot
protocol + проверку GET_PROTOCOL. Следом была жалоба: курсор в GUI
периодически моргал и на миг замирал. Причина: часы панели задач
помечали весь кадр изменённым несколько раз в секунду, и весь экран
(~1-2 млн пикселей) копировался в медленную видеопамять, затирая
курсор. Исправлено (`gui/desktop.c`: `gui_present_frame`,
`gui_present_cursor`): теневой буфер в RAM + запись на экран только
изменившихся пикселей, курсор вклеивается в поток записи (каждый
пиксель пишется один раз); часы перерисовываются только при смене
цифры. **Ждёт подтверждения на ноутбуке.**

## Правила работы с кодом

- Многословный явный C, подробные комментарии **на русском**, «почему».
- Нет libc; большие структуры целиком не присваивать без нужды.
- Две системы печати: `print*`/`kprintf` (в консоль через ConOut) и
  `gui_*_to_str` (char* для пиксельного шрифта GUI).
- После изменений: `make` без предупреждений → `make test` → для GUI
  визуальная проверка → отдать весь `MyOS.zip` (с `.git`).
- Коммиты — с осмысленным сообщением; в конце сообщения строки
  соавторства, если их требует окружение.
- Если «не работает» после изменений — сначала проверить, что
  запускается свежий образ (`strings BOOTX64.EFI | grep <строка>`).
- QEMU с `-bios OVMF.fd` без pflash пишет `NvVars` на загрузочный
  диск; `autotest.py` каждый раз делает свежий образ.
