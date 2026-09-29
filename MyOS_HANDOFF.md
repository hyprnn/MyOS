# Контекст для продолжения: MyOS — hobby UEFI OS

Вставь это первым сообщением новому ИИ и **приложи весь `MyOS.zip`**
(вместе с папкой `.git` — там история).

## Что это за проект

`MyOS` — самодельная 64-битная ОС на чистом C, без GNU-EFI и сторонних
библиотек. Два файла: загрузчик `BOOTX64.EFI` (UEFI-программа, PE)
собирает «паспорт загрузки» (`bootinfo.h`), читает `\EFI\BOOT\KERNEL.ELF`,
строит временные таблицы страниц, вызывает ExitBootServices и прыгает
в ядро `kernel.elf` (ELF, `0xFFFFFFFF80000000`). Ядро о прошивке не
знает ничего: свои таблицы страниц, GDT/IDT/TSS, таймер, память,
USB/PS2-ввод, консоль, шелл и GUI. Команды `ebs` больше нет (печатает
пояснение).

Пользователь **не программист в этой теме**: код и решения пишет ИИ,
пользователь собирает, запускает в QEMU и на ноутбуке (HP 250 G7, Arch
Linux, fish; беспроводная мышь Onikuma через USB-донгл) и присылает
скриншоты/видео. Просит «делай максимум», минимум вопросов. Общение
по-русски.

Есть документ-план «MyOS: план пути к настоящей ОС» (10 этапов).
**Этапы 0 (порядок в коде), 1 (загрузчик + ядро, виртуальная память)
2 (ACPI), 3 (прерывания, USB-хабы, горячее подключение, флешки) и
4 (потоки ядра, планировщик с вытеснением, мьютексы, `ps`) и 5
(диски: USB/AHCI/NVMe, MBR/GPT, FAT16/32 чтение+запись, VFS, кэш,
команды файлов, Проводник) и 6 (программы в ring 3: syscall, свои
адресные пространства, ELF, мини-libc, /bin; ярлыки рабочего стола
в стиле Win95) выполнены.** Следующий по плану — этап 7 (оконная
система: композитор, несколько окон, окна у программ, кириллица).
Из этапа 6 не сделано: шелл как программа и Сапёр как программа (им
нужен протокол окон этапа 7).
Не сделаны: SMP (этап 4, «позже»), virtio-blk (не нужен: QEMU-тесты
идут через AHCI и NVMe), exFAT/NTFS/ext4 (не монтируются). AML (байт-код
DSDT/SSDT) не исполняется: решение — взять библиотеку uACPI, когда
понадобятся батарея/крышка/кнопка питания (этап 9).

## Сборка, запуск, тест

```bash
make            # BOOTX64.EFI + kernel.elf, инкрементально; оба -> esp/EFI/BOOT/
make run        # ISO + QEMU, лог COM1 в терминал (-serial stdio)
make run-tablet # мышь-планшет (без «стенок» в окне QEMU)
make test       # tools/autotest.py: 11 запусков QEMU (~6 мин): основной
                # (-smp 2, загрузка CPU, ps, threadtest, spin), usb-tree
                # (хаб, флешка, горячее подключение по QMP), ps2
                # (IRQ 1/12), gui-threads (SLEEP/SPIN в потоке, пока GUI
                # живой; проверка по строкам "gui: ..." в COM1),
                # storage (флешка MBR+FAT32, AHCI GPT+FAT16, NVMe
                # GPT+FAT32; ls/cd/mkdir/write/cp/mv/rm/edit, флешка
                # на лету), storage-reboot (те же образы: файлы на
                # месте, Проводник), в конце - образы проверяются
                # tools/fatimg.py (независимый "fsck" + содержимое), q35
                # (ECAM, сброс через FADT), зимние часы
                # (-rtc base=2026-01-15: МСК 13:00 / Иерусалим 12:00) и
                # crash write / crash stack / crash null (экраны паники)
```
Опции автотеста: `--quick` (без crash-запусков), `--mem 5G`,
`--extra "-machine q35"` / `"-cpu qemu64,-nx,-pat"` — всё это проверено.
`./build.sh` вызывает `make`. OVMF по умолчанию
`/usr/share/edk2/x64/OVMF.4m.fd`. Два набора флагов (см. Makefile):
загрузчик — `-fpic -fvisibility=hidden -DMYOS_LOADER`, линковка
`-m i386pep`; ядро — `-fno-pic -mcmodel=kernel`, линковка по
`kernel/kernel.ld`. Общие: `-O2 -fno-strict-aliasing
-fno-tree-loop-distribute-patterns -ffreestanding -fshort-wchar
-mno-red-zone`. Предупреждений быть не должно (сейчас ноль).

## Структура

| Где | Что |
| --- | --- |
| `loader/loader.c` | загрузчик: GOP, время, RSDP, TSC по Stall, чтение ядра, ELF, отъём xHCI у прошивки, временные таблицы (1:1 + HHDM + ядро), EBS, прыжок |
| `bootinfo.h` | паспорт загрузки `MYOS_BOOT_INFO` + раскладка адресов: HHDM `0xFFFF800000000000`, стеки `0xFFFFFE8000000000`, ядро `0xFFFFFFFF80000000`; свои типы памяти `MYOS_MEM_KERNEL/LOADER_TEMP` |
| `myos.h` | общий заголовок; `P2V()`/`V2P()` (физ. адрес <-> указатель прямого отображения), `mmio_read32/write32` берут ФИЗИЧЕСКИЙ адрес и сами переводят |
| `lib/` | `libc.c` (memcpy/memset), `string.c` (+`kstreq`), `kprintf.c` (`kprintf`, `ksnprintf`, `klog` — только COM1; `%S` = CHAR16*), `serial.c` |
| `kernel/` | `kmain.c` порядок запуска; `kernel.ld`; `vmm.c` таблицы страниц (код RX, rodata R, данные RW+NX, RAM WB, не-RAM ниже 4 ГиБ UC, экран WC через PAT, нижняя половина пуста), стеки с защитными страницами; `pmm.c` страницы (свободны также BootServices*/Loader*); `kmalloc.c` слабы 16..1024 + крупные страницами; `cpu.c` GDT+TSS (IST1 #DF, IST2 NMI, IST3 #MC), IDT, экран паники с разбором #PF/#DF; `acpi.c` разбор ACPI (RSDP→XSDT/RSDT, контрольные суммы, MADT: ядра/I/O APIC/переназначения IRQ, FADT: порты PM, таймер PM, регистр сброса, век RTC, MCFG: ECAM, HPET: запуск и замер TSC; команда `acpi`); `tz.c` часовые пояса: CMOS хранит UTC, `krt_get_time` отдаёт местное время Москвы (UTC+3) или Иерусалима (UTC+2/+3, израильские правила летнего времени; проверено против zoneinfo на 2015–2035), переключение `tz msk|jer|toggle`, клик по часам в GUI или T; по умолчанию Москва, выбор не сохраняется (диска нет); `power.c` CMOS-часы (век из FADT), reboot (регистр FADT → 0xCF9 → 8042 → triple fault), shutdown (`_S5_` из DSDT/SSDT, QEMU-порты); `shim.c` таблица `g_kst` для шелла/GUI (свои BootServices/RuntimeServices, без прошивки); `kcon.c`, `time.c`, `kcmds.c` (kinfo/usb/mousetest/mem/boot/vm/crash) |
| `drivers/` | `pci.c` (порты или ECAM после `pci_use_ecam` со сверкой), `xhci_common.c` (общие с загрузчиком), `usb.c` (ядро xHCI: кольца, `kx_pump`, синхронные операции под замком, перечисление с route string/TT, горячее подключение, `kx_service`, MSI), `usbhid.c` (клавиатуры/мыши, трубы прерываний, светодиоды), `usbhub.c` (хабы USB2/USB3), `usbmsd.c` (флешки: Bulk-Only + SCSI, `usb_disk_read`, команда `disk`), `hid.c`, `keyboard.c` (+NumLock/ScrollLock), `ps2.c` (IRQ 1/12, PS/2-мышь/тачпад с колесом, светодиоды) |
| `gui/` | `gui.c` цикл `start`, `desktop.c` (вывод кадра без мигания: `gui_present_frame`/`gui_present_cursor`), `draw.c`, `minesweeper*.c`, `terminal.c` (+`gui_term_exec`) |
| `shell/` | `commands.c`, `console.c`, `readline.c`, `fs.c`, `fetch.c`, `editor.c`, `calc.c`, `history.c` |
| `tools/` | `autotest.py` (свой FAT16-образ с обоими файлами, QMP, проверки по COM1), `split_main.py` |

Новые команды шелла: в `shell/commands.c`, `else if (streq(line,
"имя"))` + строка в `help`. Новые функции/переменные модуля — объявить
в `myos.h` в секции своего модуля.

## Ядро — кратко

Порядок запуска (`kernel/kmain.c`): COM1 → копия паспорта → свои
GDT/IDT (старые — в памяти прошивки, которую сейчас отдадим) → карта
памяти + pmm → vmm (свои таблицы, CR3, сброс глобальных TLB) → отдать
таблицы загрузчика → стек 256 КиБ с защитной страницей → TSS/IST →
консоль → PIC → ACPI (таблицы; I/O APIC из MADT маскируются; ECAM;
подготовка shutdown) → TSC по HPET / PIT / таймеру PM (берётся первый,
согласный с контролем — замером загрузчика по Stall) → LAPIC timer
1 кГц → CMOS → PS/2 → xHCI
(BAR отображается UC через `vmm_map_mmio`) → `g_kst` → шелл.

Правило: **физический адрес никогда не приводить к указателю
напрямую** — только `P2V()`. Нижняя половина пуста, забытый `P2V` =
Page Fault с текстом «lower half: nothing is mapped there». Новые
устройства выше 4 ГиБ — `vmm_map_mmio(phys, size, VMM_UC)`.

Без ACPI (проверено подменой) ядро откатывается на 0xFEC00000, порты
PCI и PIT. OVMF с `-no-acpi` сам не грузится — так не проверить.

Команды для проверки: `usb` (дерево устройств, журнал подключений), `disk`/`disk read N`, `cpu` (загрузка, прерывания), `acpi` (таблицы, ядра, I/O APIC, ECAM, HPET, FADT), `vm` (раскладка, стеки, проверка перевода
адресов), `mem` (карта, pmm, куча, самотесты), `boot` (что сделал
загрузчик + его журнал), `kpanic`/`kpanic null`/`kpanic write`/`kpanic stack`.

## Состояние на реальном ноутбуке

Мышь (донгл Onikuma) работает (boot protocol + GET_PROTOCOL). Курсор в
GUI больше не моргает (теневой буфер + вклейка курсора) —
**подтверждено пользователем**. Этап 1 (загрузчик + ядро) проверен
только в QEMU (OVMF; 256 МиБ и 5 ГиБ; pc и q35; с NX/PAT и без) —
**на ноутбуке ещё не запускался**. Если не стартует: фото экрана
загрузчика (он пишет, что делает) или экрана паники (там RIP, CR2 и
объяснение). На флешке должны быть ОБА файла в `\EFI\BOOT\`.

## Прерывания и USB (этап 3) — как устроено

`kernel/irq.c`: таблица обработчиков векторов (`kx_irq_register`),
I/O APIC (`kx_ioapic_route`, IRQ→GSI через переназначения MADT),
MSI/MSI-X (`kx_pci_enable_msi`), учёт загрузки (время в `hlt` через
`kx_idle_hlt`, пересчёт раз в секунду из таймера; команда `cpu`).
Векторы: 0x21 PS/2-клавиатура, 0x2C PS/2-мышь, 0x40 таймер, 0x50 xHCI.

USB — два контекста: обработчик прерывания (`kx_usb_irq` → `kx_pump`:
отчёты HID, сообщения хабов, смена портов — только ЗАПОМИНАЕТ) и
основной код (`kernel_poll_input` → `kx_service`: подключение,
отключение, хабы, светодиоды — всё, что ждёт). Основной код держит
замок `kx_lock` (= cli, вложенный); во время ожиданий
`kx_relax`/`kx_msleep` ненадолго включают прерывания; событие, которого
ждёт основной код, обработчик кладёт в «ящик» `g_kx.wait_*`. В ISR
никогда ничего не ждать. Фоновые сообщения — `kx_out(NULL, ...)` (только
COM1: в GUI писать в консоль нельзя) и журнал `kx_event_log` (виден в
`usb`). Без MSI драйвер работает опросом (irq_mode "polling").

Не проверено в QEMU (нет таких устройств): HS-хаб с Transaction
Translator (LS/FS-мышь за USB2-хабом — обычный случай на железе!),
USB3-хабы, флешки SuperSpeed. Проверено: FS-хаб, мышь/клавиатура/
флешка за ним, горячее подключение и отключение (QMP device_add/del),
MSI-X, PS/2 по IRQ, PS/2-мышь.

## Потоки (этап 4) — как устроено

`kernel/sched.c`. Поток = `KTHREAD` (таблица `g_kthreads[32]`, без
списков): свой стек (`vmm_alloc_stack`, защитная страница), rsp,
состояние (READY/RUNNING/SLEEPING/BLOCKED/DEAD), учёт времени.
Слот 0 — поток, выполнявший kmain (потом «shell»; в GUI
переименовывается в «gui»). `g_kcur` валиден с первой инструкции.

* Переключение — `kx_switch` (асм): push rbp/rbx/r12–r15/rflags,
  смена rsp, pop, ret. Новый поток стартует через `kt_trampoline`
  (sti → fn(arg) → `kthread_exit`). Стек и слот мёртвого потока
  переиспользуются.
* Вытеснение: таймер 1 кГц → `sched_tick` (будит спящих, квант 10 мс)
  → в конце `kx_isr_dispatch` (после EOI) `sched_isr_exit` переключает
  прямо из прерывания. `g_kx_isr_depth` — «мы в обработчике»: там
  спать нельзя (`sched_can_block()`).
* Очередь: FIFO по `ready_seq`; только что проснувшиеся (`boost`) —
  первыми (шелл/GUI/usb отзываются сразу); вытесненный проснувшимся
  сохраняет место и остаток кванта. `threadtest` проверяет 33/33/33.
* Сон: `sched_sleep_ms`; `kx_sleep_us` (Stall) и `kx_msleep` спят по-
  настоящему (≥1 мс). Ожидание: `sched_block(obj, what, timeout)` +
  `sched_wake_all/one(obj)`; проверять условие и засыпать — при
  `kx_irq_save()`.
* Замки: `kx_lock` (cli, вложенный, счётчик **у каждого потока** —
  поток может уснуть внутри kx_lock, флаги сохраняет kx_switch);
  `KMUTEX` (рекурсивный, спящий; нельзя в ISR); `KSPINLOCK` (irqsave +
  атомарный флаг, на будущее SMP; им защищён вывод в COM1).
* Поток **usb**: `kx_service` в цикле, будится из `kx_usb_irq`
  (any_change / хабы / светодиоды) или раз в 50 мс. `g_usb_mutex`
  — синхронные операции с контроллером (ящик `g_kx.wait_*` один):
  его берут `kx_service`, `usb_disk_read`, `disk`, `usb`.
  `kernel_poll_input` зовёт `kx_service` только без потока usb.
* GUI: `SLEEP N`/`SPIN N` в терминале GUI — поток «term-job»
  (`g_term_job`, gui/terminal.c); строки — под `g_term_mutex`; в
  заголовке окна «RUNNING: ...» и часы; выход из GUI отменяет
  задание (term_lines живут в стеке gui_start). `C` на рабочем
  столе открывает терминал.
* Без работающего таймера потоков нет: всё работает по-старому.
* Команды: `ps`, `threadtest`, `spin N`; в `cpu`/`kinfo` — строка про
  потоки.

## Диски и файлы (этап 5) — как устроено

* `drivers/blk.c`: `g_blk[24]` — диски и разделы (`usb0`, `usb0p1`,
  `sata0`, `nvme0p1`). `blk_register_disk` → `blk_scan_partitions`
  (FAT без таблицы / MBR / GPT по GUID) → `fat_probe` →
  `vfs_mount_dev` (том называется как раздел). `blk_read/write`:
  раздел → диск + смещение; кэш 512 секторов по 512 Б (LRU, только
  одиночные секторы, запись сквозная). Запись разрешена только USB и
  моделям "QEMU..." (внутренний диск ноутбука — Arch!).
* Флешки: поток usb их НЕ регистрирует (порядок замков: g_vfs_mutex →
  g_usb_mutex, никогда наоборот) — `blk_sync_usb()` сверяет
  `g_kx_msd` (поле `serial` — паспорт подключения) при каждой
  операции VFS. `usb_msd_rw` = SCSI READ(10)/WRITE(10) по 4 КиБ.
* `drivers/ahci.c` (слот 0, один PRDT, 64 КиБ, READ/WRITE DMA EXT,
  опрос `blk_wait`), `drivers/nvme.c` (admin + одна пара I/O-очередей
  по 16, PRP1/PRP2, 8 КиБ за команду). DMA-буферы ниже 4 ГиБ.
* `fs/fat.c`: FAT16/32, тип по числу кластеров; длинные имена
  (чтение и запись, псевдонимы `NAME~N.EXT`, флаги строчных букв
  NT); запись в обе копии FAT; FSInfo при первой записи → "не
  знаю" (0xFFFFFFFF); каталоги растут; `..` правится при переносе
  папки. Узел = parent_cluster + entry_index + lfn_count.
* `fs/vfs.c`: корень "/" — список томов; `vfs_open/read/write/
  close` (16 fd), mkdir/remove/rename (в пределах тома; между
  томами — `mv` копирует и удаляет), `g_cwd`; `/ram` — старый
  RAM-диск (`g_fs`) через VFS_OPS. Всё под `g_vfs_mutex`.
* Шелл: `shell/fs.c` (`fs_shell_command`), `editor.c` (edit по
  пути); приглашение показывает текущую папку. GUI: Проводник
  (`gui/explorer.c`) ходит по VFS, `E` на рабочем столе открывает
  его. Терминал GUI по-прежнему работает только с RAM-диском.
* `tools/fatimg.py`: FatBuilder (mkfs + файлы с LFN), make_disk
  (MBR/GPT/без таблицы), FatReader.fsck — независимая проверка.

## Программы в ring 3 (этап 6) — как устроено

* GDT: 0x08/0x10 ядро, 0x18 (пустышка для SYSRET), 0x20 данные
  программ (0x23), 0x28 код программ (0x2B), 0x30 TSS.
* `kernel/proc.c`: `KPROC` (12 шт.): своя PML4 (верх 256 записей
  копируется из ядра при каждом входе — `uvm_sync_kernel_half`),
  ELF64 ET_EXEC (PT_LOAD, права страниц по флагам, NX), стек 256 КиБ
  под 0x7FFFFFFF0000 с argc/argv по System V, куча — `sbrk`.
  Поток ядра `proc_thread_main` ставит CR3 и `iretq` в ring 3.
  `proc_switch_hook` (из sched.c): TSS.rsp0 = `g_sc_kstack` = стек
  потока, CR3. Исключение в ring 3 → `kx_user_fault` (cpu.c) →
  `killed`; в конце `kx_isr_dispatch` и syscall `proc_check_kill` →
  `proc_exit_current` → `kthread_exit`. Память освобождает ждущий:
  `proc_wait` → `proc_reap`. Ctrl+C (keyboard.c) → `g_fg_proc`.
  SMAP выключается (ядро читает буферы программ напрямую после
  `uptr_ok`).
* `kernel/syscall.c`: MSR STAR/LSTAR/FMASK, вход `kx_syscall_entry`
  (свой стек, fxsave, sti, `kx_syscall_dispatch`, sysretq).
  17 вызовов (`sysnum.h`): exit write read open close sleep uptime
  sbrk getpid time readdir mkdir unlink rename yield stat getkey.
  fd 0/1/2 — консоль шелла (`PROC_IO_CONSOLE`) или терминал GUI
  (`PROC_IO_GUI`: вывод строками через `g_proc_gui_sink`, ввод —
  `proc_gui_input`), 3+ — файлы VFS.
* `fs/binfs.c`: том `/bin` из `build/apps.S` (.incbin всех
  `build/user/*`). make кладёт копии и в `esp/APPS/`.
* Шелл: неизвестная команда → `proc_shell_try` (ищет /bin/имя, путь
  или ELF в текущей папке); `run путь аргументы`. Встроенные `calc`
  и `edit` удалены (стали программами). Старая `crash` ядра →
  `kpanic`.
* GUI: `gui/shortcuts.c` — 8 ярлыков, значки 16x16 буквами-цветами
  (палитра Win95), рисуются x2; выделение «сеточкой»; второй щелчок
  по выделенному открывает. Терминал GUI запускает программы
  (`GUI_JOB_PROC`), ввод строк идёт программе, Esc завершает её.
  В шрифт GUI добавлены + * > < % ? [ ] " # & ; @ | ^ ~ $ \\.
* Тесты: main (hello/calc/crash*/Ctrl+C/primes/mem), gui-threads
  (программы в терминале, ярлык CALC мышью), storage (программа с
  флешки /usb0p1/apps/hello).

## Ранний запуск на железе (диагностика без COM-порта)

После ExitBootServices загрузчик рисует серую полосу по верху
экрана. Ядро первым делом грузит свои GDT/IDT (исключение = наш
экран паники, а не вечный цикл обработчика прошивки) и рисует внизу
синюю строку «MyOS kernel: step N/7 - ...» на каждом раннем шаге;
дальше консоль, и каждый раздел `[...]` сразу выводится на экран.
Разные сборки загрузчика и ядра (MYOS_BOOT_VERSION) — красная
надпись вверху. Загрузчик больше не использует свои типы памяти
UEFI: всё — EfiLoaderData, а что ядру не трогать, передаётся
списком `g_boot.reserved` (KEEP / TEMP).

Сентябрь 2026: на HP 250 G7 ядро после прыжка ничего не нарисовало
(фото: последний экран загрузчика). Вероятная причина — на флешке
загрузчик и ядро из разных сборок (версия паспорта менялась), ядро
тогда молча останавливалось. Ждём новое фото с новой сборкой.

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
  запускается свежий образ (`strings kernel.elf | grep <строка>`) и
  что загрузчик и ядро из одной сборки (иначе ядро скажет «bad boot
  info» в COM1).
- Меняешь `MYOS_BOOT_INFO` — увеличь `MYOS_BOOT_VERSION`.
- QEMU с `-bios OVMF.fd` без pflash пишет `NvVars` на загрузочный
  диск; `autotest.py` каждый раз делает свежий образ.
