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
в стиле Win95) и 7 (оконная система: композитор, перекрывающиеся
окна, окна у программ, сглаженный шрифт с кириллицей, раскладка
EN/РУ) и 8 (сеть: свой TCP/IP, e1000/e1000e, Realtek, USB-модемы,
программы ping/wget/ifconfig/nslookup/nc/httpd, HTTPS на BearSSL, exFAT,
Wi-Fi: WPA2-клиент 802.11 + драйвер Realtek RTL8821CE — последний пока
не проверен на живом чипе) выполнены.** На ноутбуке
(HP 250 G7) проверено: USB-модем телефона даёт интернет (ping
archlinux.org), Wi-Fi — Realtek RTL8821CE (10ec:c821). Следующий по плану — этап 9
(реальное железо и повседневность). Из этапа 7
не сделано: шелл и Сапёр как отдельные программы (сейчас Сапёр,
Терминал и т.п. - родные окна ядра, а не программы; для «всё окно -
программа» нужен вынос шелла в ring 3, отложено).
Не сделаны: SMP (этап 4, «позже»), virtio-blk (не нужен: QEMU-тесты
идут через AHCI и NVMe), exFAT/NTFS/ext4 (не монтируются). AML (байт-код
DSDT/SSDT) исполняет библиотека uACPI (этап 9): батарея, зарядка, крышка,
кнопка питания, выключение через S5.

## Сборка, запуск, тест

```bash
make            # BOOTX64.EFI + kernel.elf, инкрементально; оба -> esp/EFI/BOOT/
make run        # ISO + QEMU, лог COM1 в терминал (-serial stdio)
make run-tablet # мышь-планшет (без «стенок» в окне QEMU)
make test       # tools/autotest.py: 17 запусков QEMU (~6 мин): основной
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
                # (-rtc base=2026-01-15: МСК 13:00 / Иерусалим 12:00),
                # network (e1000: DHCP, ping, nettest, wget с HTTP-сервера
                # автотеста на хосте, в т.ч. на флешку и с потерей каждого
                # 10-го кадра; httpd внутри MyOS, хост качает у него через
                # hostfwd; ручной адрес; wifi selftest), net-rtl8139,
                # net-e1000e (q35, MSI), usb-tether (usb-net: RNDIS и ECM,
                # подключение/отключение на лету) и crash write / crash
                # stack / crash null (экраны паники), exfat/exfat-reboot,
                # wifi-sim (802.11 + WPA2 с программной точкой доступа)
```
`--internet` - ещё и настоящий DNS + `wget http://example.com/`.
Опции автотеста: `--quick` (без crash-запусков), `--only wifi-sim,network`
(только эти запуски), `--mem 5G`,
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
| `drivers/` | `pci.c` (порты или ECAM после `pci_use_ecam` со сверкой), `xhci_common.c` (общие с загрузчиком), `usb.c` (ядро xHCI: кольца, `kx_pump`, синхронные операции под замком, перечисление с route string/TT, горячее подключение, `kx_service`, MSI), `usbhid.c` (клавиатуры/мыши, трубы прерываний, светодиоды), `usbhub.c` (хабы USB2/USB3), `usbmsd.c` (флешки: Bulk-Only + SCSI, `usb_disk_read`, команда `disk`), `hid.c`, `keyboard.c` (+NumLock/ScrollLock), `ps2.c` (IRQ 1/12, PS/2-мышь/тачпад с колесом, светодиоды); сеть: `e1000.c`, `rtl8169.c`, `usbnet.c`; Wi-Fi: `rtw8821c.c` + `rtw8821c_table.c` (Realtek RTL8821CE) |
| `firmware/` | прошивки, вклеенные в ядро (`firmware.S`): `rtw88/rtw8821c_fw.bin` + `LICENCE.rtlwifi_firmware.txt` |
| `gui/` | `gui.c` цикл `start`, `desktop.c` (вывод кадра без мигания: `gui_present_frame`/`gui_present_cursor`), `draw.c`, `minesweeper*.c`, `terminal.c` (+`gui_term_exec`) |
| `shell/` | `commands.c`, `console.c`, `readline.c`, `fs.c`, `fetch.c`, `editor.c`, `calc.c`, `history.c` |
| `net/` | этап 8: `net.h` (типы стека, NETIF), `net.c` (интерфейсы, очередь приёма, поток net, прерывания карт, `lo`), `arp.c`, `ip.c` (+ICMP), `udp.c`, `tcp.c`/`tcp.h`, `dhcp.c`, `dns.c`, `socket.c`, `netcmd.c` (`net`, SYS_NETINFO/NETCTL, индикатор GUI), `wpa.c`/`wifi.h` (WPA2), `wifi.c` (`wifi`), `wlan.c`/`wlan.h` (802.11-клиент, `WLAN_HW`), `wlan_sim.c` (программная точка доступа) |
| `drivers/e1000.c`, `rtl8169.c`, `usbnet.c` | сетевые карты: Intel, Realtek, USB-модемы |
| `tools/` | `autotest.py` (свой FAT16-образ с обоими файлами, QMP, проверки по COM1, HTTP-сервер на хосте для сетевых тестов), `split_main.py` |

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

## Оконная система (этап 7) — как устроено

* `gui/gfx.c`: поверхность GFX (пиксели 0x00RRGGBB + отсечение),
  gfx_fill/blit/bevel/button/text/icon; сглаженный шрифт 8x16
  (`font8x16.h`, 16 градаций, латиница+кириллица, генератор
  `tools/mkfont.py` из DejaVu Sans Mono - нужен только при
  регенерации). UTF-8 (utf8_next/put/len). `gui/icons.c` - значки
  16x16 буквами-цветами.
* `gui/wm.c`: композитор. `g_windows[16]`, z-порядок, фокус,
  перетаскивание. Кадр собирается в back_buf (RAM), на экран идут
  только изменившиеся точки (shadow) + курсор-стрелка поверх -
  без мигания. Ввод: мышь (перетаскивание заголовка, крестик,
  фокус, ярлыки, панель задач, меню «Пуск»), клавиши - активному
  окну. Цикл `wm_start` спит между событиями (sched_sleep_ms).
  `start` теперь зовёт `wm_start` (старый `gui_start`/`gui/*.c`
  остались в дереве, но не используются).
* `gui/apps.c`: родные окна (WIN_CLASS = paint+event+tick+close):
  Терминал (свой поток запускает /bin, вывод через
  `g_proc_gui_sink`), Блокнот (правка UTF-8, Ctrl+S), Проводник
  (VFS), Сапёр (логика в gui/minesweeper.c), О системе; ярлыки и
  меню «Пуск». Графические программы (clock/paint/life)
  запускаются без окна терминала (`app_run_bare`).
* Окна программ: `kernel/winproc.c` + 5 системных вызовов
  (`SYS_WIN_*`). Буфер окна - общие физические страницы,
  отображённые и ядру (P2V), и программе (`proc_map_shared`,
  адрес `MYOS_WIN_ADDR(id)`, PTE помечены UPTE_SHARED, чтобы
  uvm_free их не освободил). События - в очередь окна
  (`win_event`, ждёт с таймаутом). Программа ушла -> окна помечены
  `dead`, закрывает их поток композитора. `user/lib/win.c` +
  `font6x8.c` - API и мелкий шрифт для программ.
* Клавиатура: `g_kbd_layout` (EN/РУ, ЙЦУКЕН), переключение
  `Ctrl+Space`; индикатор на панели задач.
* Тест `desktop`: мышью открыть «Пуск» -> Терминал, «Пуск» ->
  Часы (окно программы, `winproc` в COM1), выйти.

## Сеть (этап 8) — как устроено

* **Стек свой**, не lwIP (`net/`). Адреса IPv4 внутри — `UINT32` в
  порядке процессора (10.0.2.15 = 0x0A00020F); в пакеты — `net_put32`.
  Всё состояние — под одним спящим замком `g_net_mutex`. **Порядок
  замков: g_net_mutex → g_usb_mutex** (отправка через USB-модем);
  поток usb никогда не берёт g_net_mutex (USB-модем регистрируется в
  стеке потоком net: `usbnet_sync`).
* **Приём**: драйвер (обычно в прерывании) → `net_rx_frame` (копия в
  кольцо на 128 кадров, спин-замок, будит поток) → поток **net**
  (`net_thread`): `arp_input` / `ip_input` → `icmp`/`udp`/`tcp_input`
  → сокет → `net_wake(сокет)`. Таймеры (ARP, DHCP, повторы TCP) — там
  же, раз в 10 мс; в простое поток спит 250 мс (`net_idle_ms`).
* **Отправка**: `ip_send` → `ip_route` (своя подсеть / шлюз / `lo`)
  → `arp_send_ip` (MAC известен — сразу, нет — ARP-запрос, пакет ждёт
  в записи) → `net_send_eth` → `NETIF.tx` драйвера.
* **NETIF** (`net.h`): имя (`lo`, `eth0`, `usb0`), MAC, link, tx,
  poll (сторож/опрос), `poll_fast` (карта без прерываний — будить поток
  каждые 2 мс), адрес/маска/шлюз/DNS, `cfg` (DHCP/вручную), состояние DHCP.
* **Прерывания карт** (`net_pci_irq_setup`): MSI/MSI-X, иначе INTx
  через I/O APIC по Interrupt Line из PCI (+ флаги из MADT; без
  переназначения — по уровню, активный низкий) — и драйвер ОБЯЗАН
  проверить, что прерывание приходит: e1000 — сам вызывает его (ICS),
  Realtek — сторож в `rtl_poll` (кадр ждёт, прерываний нет дважды за
  200 мс → опрос). Защита от «шторма» общей линии: 5000 чужих
  прерываний подряд → линия маскируется (`kx_ioapic_mask`), опрос.
  Векторы 0x60–0x65. Перед запуском карта будится из D3 (`net_pci_wake`).
* **TCP** (`tcp.c`): все состояния RFC 793, буферы 32 КиБ, MSS 1460,
  RTO по RFC 6298 (мин. 200 мс), Карн, быстрый повтор по 3 дубль-ACK,
  cwnd (медленный старт/избегание), пробы нулевого окна, **очередь
  сегментов не по порядку** (32 шт.). Важная тонкость: в повторных ACK
  окно не увеличивается (`last_adv_ack`), иначе отправитель (BSD/slirp)
  не считает их повторными и быстрый повтор не срабатывает. TIME_WAIT
  2 с. Брошенные программой соединения умирают через 60 с. Нет SACK и
  масштабирования окна.
* **Сокеты** (`socket.c`): 32 шт., TCP/UDP/PING; ожидание — `net_wait`
  (отпускает g_net_mutex целиком, спит кусками по 100 мс — чтобы
  заметить Ctrl+C: `MYOS_EINTR`); `MYOS_SO_TIMEOUT`. В программе сокет
  — fd со флагом `PROC_FD_SOCK` в `p->fds` (read/write/close работают);
  при завершении программы сокеты закрывает `proc_reap`.
  Системные вызовы 22–32 (`sysnum.h`), обёртки — `user/lib/net.c`
  (`syscall4`: 4-й аргумент в r10).
* **DHCP** (`dhcp.c`): DISCOVER/OFFER/REQUEST/ACK, повторы 2–16 с,
  продление в половине аренды; запускается сам, когда у интерфейса
  появилась связь. **DNS** (`dns.c`): A-записи, сжатые имена, кэш 16
  имён по TTL; `localhost` и «1.2.3.4» — без сервера.
* **Драйверы**: `e1000.c` (82540EM/82545EM/82574L проверены; I21x —
  без сброса CTRL.RST, вслепую), `rtl8169.c` (RTL8139C+ в QEMU;
  RTL8111/8168/8106E — вслепую: RxConfig по поколению чипа (XID из
  TxConfig), снятие RXDV_GATED_EN у 8168G+, TxPoll 0x38 против 0xD9 у
  8139), `usbnet.c` (USB: в `kx_enum_device`, если нет HID/флешки,
  `kx_net_probe` перебирает ВСЕ конфигурации; ECM > RNDIS > NCM, `net
  usb rndis` — RNDIS раньше ECM для проверки; приём — труба
  `KX_ROLE_NET` на 4 КиБ из usbhid.c, разбор в прерывании
  `kx_net_report`; отправка — синхронный `kx_bulk` под g_usb_mutex;
  добивка байтом при длине кратной пакету — у RNDIS входит в
  MessageLength, иначе QEMU/устройство сбивается). NCM — только по
  спецификации.
* **Wi-Fi**: `wifi.c` находит адаптер (PCI класс 0x0280 и известные
  USB-свистки, таблица с драйвером Linux для ориентира) и разбирает
  команду `wifi` (scan / connect / disconnect / debug / sim / selftest /
  psk). `wpa.c` — SHA-1, HMAC, PBKDF2, PRF-512, AES-128 (+расшифровка),
  Key Wrap, AES-CCM, CCMP (AAD/nonce по 802.11), клиент 4-стороннего
  рукопожатия и группового (смена GTK) (`wpa_supp_rx`; свой RSN IE в
  `WPA_SUPP.rsn_ie` — тот же, что в association). `wifi selftest` — 12
  проверок (FIPS 180/197, RFC 2202/3394/3610, IEEE 802.11i H.3/H.4,
  рукопожатие с программной AP, смена GTK, неверный пароль).
  * **802.11-клиент** `net/wlan.c` (не зависит от чипа, интерфейс к
    драйверу — `WLAN_HW` в `net/wlan.h`: set_channel, tx кадра 802.11,
    set_scan, set_bssid, set_link, poll, info). Поток `wlan` делает всё,
    что ждёт ответа (поиск: каналы 1–13, 36–165, на DFS 52–144 только
    слушаем; подключение: auth open → assoc (скорости 802.11a/g, RSN IE
    с шифром группы как у точки) → 4-way; переподключение, если 10 с нет
    маяков или пришёл deauth), кадры разбирает поток net (`wlan_rx` из
    `hw->poll`). Ждёт через `net_wait` (замок отпущен). Данные: Ethernet
    ↔ 802.11 + LLC/SNAP, CCMP программно (`ccmp_encrypt/decrypt`), своя
    защита от повторов (PN), A-MSDU, отражённые точкой свои
    широковещательные кадры выбрасываются. Команда `wifi connect`
    печатает журнал попытки (`g_wl.log`) и ждёт адрес DHCP. Неверный
    пароль = 1/4 пришло, 3/4 нет → не повторять. Нет: SAE (WPA3-only),
    PMF (сеть с MFPR отказываемся), TKIP (общий ключ TKIP у старых
    роутеров — широковещательные не расшифруем), 802.11n/ac.
  * **Драйвер RTL8821CE** `drivers/rtw8821c.c` — перенос rtw88
    (BSD-3-Clause), таблицы `rtw8821c_table.c` без изменений чисел,
    прошивка `firmware/rtw88/rtw8821c_fw.bin` (24.11) вклеена в ядро
    (`firmware/firmware.S`, `g_fw_rtw8821c`). Включается по первой
    команде `wifi scan/connect/debug` и печатает шаги 1/6..6/6 (PCI и
    версия → питание (card_enable_flow) → прошивка (куски по 4 КиБ через
    маячную очередь в буфер чипа + DDMA в IMEM/DMEM/EMEM, ждать
    FW_READY 0xC078) → eFuse (MAC, RFE, калибровка мощности) → MAC
    (очереди, LLT, H2C) + таблицы MAC/BB/AGC/RF с условиями по RFE →
    wlan0). Кольца: 8 очередей отправки (используются MGMT, BE, H2C,
    BCN), приём 64×12 КиБ; прерываний нет — опрос каждые 2 мс
    (`poll_fast`). Прошивке: RA info (скорости точки; скорость данных
    выбирает она), media status, RSSI, IQK перед подключением. DIG по
    ложным тревогам раз в 2 с. Bluetooth-часть не используется: антенна
    всегда у Wi-Fi (GNT_WL=1, GNT_BT=0 через LTE-coex 0x38, DPDT по RFE).
    Мощность: база eFuse + таблица PG, не выше самого строгого предела
    LMT из всех стран. Ширина канала только 20 МГц. **На ноутбуке
    проверено:** чип RTL8821C cut B, RFE 2, прошивка 24.11 стартует за
    16 мс, `wifi scan` находит 26 сетей 2,4/5 ГГц, `wifi connect` к
    домашней WPA2-сети — DHCP и ping в Интернет работают. `wifi debug` печатает
    регистры для фото.
  * **Программная точка доступа** `net/wlan_sim.c` (`wifi sim`, только
    если настоящего адаптера нет): "MyOS-Test" на канале 6, пароль
    `myos-wifi-test`, сторона точки WPA2 (hostapd-подобно: неверный
    пароль → 3 раза 1/4 → deauth 15), DHCP 192.168.77.2 (ответ
    широковещательный — через GTK), ARP и ping 192.168.77.1; отражает
    широковещательные кадры клиента. Автотест `wifi-sim`.
* **HTTPS / TLS** (после этапа 8; пользователь разрешил брать готовые
  библиотеки, «чтобы не мучиться»): BearSSL 0.6 в `third_party/bearssl`
  (из пакета Ubuntu, MIT; `third_party/README.md`), собирается для
  ring 3 со своими флагами `BSSL_CFLAGS` (все OS-зависимости выключены,
  своя заглушка `<string.h>`) и вместе с `user/tls/tls.c` (обёртка:
  сокет ↔ BearSSL, время UTC, случайные числа, загрузка PEM, режим без
  проверки, понятные ошибки) и `user/tls/roots.c` (корни Mozilla,
  `tools/mkroots.sh`) в `build/user/libtls.a` — компонуется ко всем
  программам, но берётся только нужное (растёт лишь wget, ~240 КиБ).
  `tls.c` НЕ включает `user/include/myos.h` (разные uint64_t). Ядро:
  `SYS_GETRANDOM` (kernel/random.c: SHA-1 от пула + RDRAND + TSC,
  подмешиваются кадры сети и клавиши), `SYS_TIME` с a2 = 1 — UTC.
  wget: `https://`, переадресации http↔https, `--ca-certificate`,
  `--no-check-certificate`, HTTP/1.1 + `Connection: close`. Только TLS
  1.0–1.2 (в BearSSL нет 1.3). Проверено: автотест (свой HTTPS-сервер
  с `tools/tls-test/`), и из песочницы — настоящий pypi.org через TLS-
  шлюз песочницы (с его корнем).
* **exFAT** (после этапа 8, по просьбе: флешка пользователя - Ventoy, её
  большой раздел exFAT): библиотека FatFs (ooFatFs R0.13c из
  MicroPython, BSD-1) в ядре, `third_party/fatfs` + своя `ffconf.h`;
  `fs/exfat.c` - прослойка к VFS: FatFs работает с путями, поэтому у
  VFS_NODE появилось поле `fpath` (путь внутри тома), у VFS_OPS -
  необязательный `close` (vfs_close дописывает файл), у VFS_MOUNT -
  `xfs` (том FatFs). Открытый файл тома кэшируется (FIL) между read/
  write, закрывается при vfs_close или любой другой операции тома;
  чтение каталога продолжается с того же места. Монтирование:
  `vfs_mount_dev` - сначала свой FAT, иначе `exfat_mount`
  ("EXFAT   " в секторе 0). `vfs_is_disk` / `vfs_fs_name` вместо
  сравнений с g_fat_ops. Проверки: автотест `exfat` / `exfat-reboot`
  (образ "как Ventoy": MBR, exFAT + FAT16), затем `fsck.exfat -n` и
  сверка файлов утилитой `tools/exfattool.c` (нужен exfatprogs, иначе
  запуски пропускаются). Консоль ядра показывает не-ASCII имена как '?'
  (шрифт консоли - только ASCII; в GUI кириллица есть).
* **Отладка**: `net` (всё сразу), `net drop N` (терять каждый N-й
  принятый кадр), `klog` пишет DHCP/подключения в COM1.
* GUI: адрес на панели задач (`net_gui_indicator`), строка «Сеть» в
  «О системе»; `fetch` — строка Network; `lspci` помечает сетевые карты.
* Проверено только в QEMU. На ноутбуке без кабеля — USB-модем телефона.

## Полная libc и браузер (этап 9) — как устроено

* **picolibc** (`third_party/picolibc`, собирается make по `files.mk`) — libc
  для программ `user/posix/apps/*.c` (и перенесённых). Однопоточная, без TLS;
  errno глобальный. Привязка к ядру — `user/posix/`: `os.c` (read/write/open/
  lseek/fstat/stat/sbrk/время/сигналы-эмуляция/opendir, коды MYOS_E* -> errno,
  TZ из ядра), `socket.c` (сокеты BSD поверх SYS_SOCKET.., MSG_PEEK хранится
  в программе, poll/select, getaddrinfo через SYS_RESOLVE), `fs.c` (realpath,
  scandir, mmap = malloc+read, *at через dirfd). Линкуются архивом
  `libposix.a`: сокеты подтягиваются только если нужны (слабые ссылки).
  Свои заголовки (`sys/socket.h`, `netdb.h`...) — `user/posix/include`,
  перед заголовками picolibc; `-nostdinc`, чтобы не попали заголовки glibc.
* **Ядро для неё**: SYS_SEEK/FSTAT/GETCWD/CHDIR, SYS_POLL (ждёт
  `g_net_any_event`, который будит каждый `net_wake`; проверка и сон под
  g_net_mutex — событие не теряется), неблокирующие сокеты (`MYOS_SO_NONBLOCK`:
  sock_wait сразу MYOS_EAGAIN, connect — MYOS_EINPROGRESS, итог —
  `MYOS_SO_ERROR`), `MYOS_SO_LOCALADDR`. 32 fd на программу, куча до 512 МБ,
  стек 1 МБ, ELF до 32 МБ. События окна: поле `mods` (Ctrl/Shift/Alt);
  Ctrl+C не убивает программу, если в фокусе её собственное окно.
* **Встроенные файлы**: таблица `__myos_embedded_files` (myos_sys.h) в
  программе -> файлы только для чтения `/embed/...` (и папки) в open/read/
  lseek/stat/fstat. Браузер так носит шрифты, стили, `Messages`.
* **curl 8.14.1** (последний с BearSSL) + zlib: `third_party/curl`, настройки
  `curl/myos/curl_config.h` (создан CMake с тулчейном MyOS). BearSSL собран
  второй раз для picolibc (`PBSSL_CFLAGS`: time(), getentropy). Без файла
  корней curl верит `user/tls/roots.c`. Имя в сертификате — только DNS (не IP).
* **NetSurf**: `third_party/netsurf/` — NetSurf + 11 библиотек; FreeType,
  libpng, libjpeg-turbo, utf8proc рядом. `netsurf/build.mk` — правила,
  сгенерированные из журнала их родной сборки (скрипт переноса: собрать всё
  их buildsystem с обёрткой `x86_64-myos-gcc` [gcc + picolibc + user/posix],
  взять команды компиляции, gcc -M -> список файлов). Окно — поверхность
  libnsfb `src/surface/myos.c` (буфер окна = пиксели XRGB8888 libnsfb,
  win_event -> события nsfb; символы Юникода кодом 0x10000+c, правка в
  `fbtk/event.c`). libparserutils — без iconv (свои кодеки: UTF-8/16,
  ISO-8859, Windows-125x); iconv picolibc знает только UTF-8. Нет:
  JavaScript, SVG, WebP; курсивных шрифтов (курсив — обычным).
* **Проверки**: `libctest` (main, usb-tree), curl (network), прогон
  `browser`: рабочий стол -> терминал -> `browser URL`, затем снимок экрана
  QEMU и подсчёт точек трёх цветов страницы (`{'screen': ...}` в autotest.py).

* **ACPI через uACPI** (`third_party/uacpi`, MIT; этап 9). Ядро даёт ей
  функции `uacpi_kernel_*` — `kernel/acpi_os.c`: память (`P2V`, страницы
  вне RAM — `vmm_ensure_writable` c UC), порты, конфигурация PCI (сегмент 0,
  256 байт), свои мьютекс/событие с таймаутом на `sched_block`, спин-замки
  (`KSPINLOCK`), SCI на вектор `KX_VEC_ACPI` (0x29; режим линии — из ISO
  MADT, по умолчанию уровень/низкий), очередь работы из прерываний →
  поток `acpi`. `kernel/acpi_dev.c`: порядок `uacpi_initialize` →
  `namespace_load` → `\_PIC(1)` → EC → `namespace_initialize` → поиск
  устройств → фиксированное событие кнопки → `finalize_gpe_initialization`
  (в `kmain` — раздел `[power]`, после сети). Драйвер EC (PNP0C09, порты из
  `_CRS` или ECDT; команды 0x80/0x81/0x84, `_GLK` → глобальный замок; GPE
  из `_GPE` → запрос → `_Qxx` в потоке acpi). Батарея PNP0C0A: `_STA`,
  `_BIX`/`_BIF`, `_BST`; блок питания ACPI0003 `_PSR`; крышка PNP0C0D
  `_LID`; кнопка — фиксированное событие или PNP0C0C (Notify 0x80). Поток
  `power`: опрос раз в 15 с и сразу после Notify; кнопка питания →
  `kx_shutdown`, который сначала пробует `acpi_dev_poweroff` (`_PTS` + S5
  через uACPI), затем старый путь power.c. Команда `battery` (`power`),
  значок на панели задач (`acpi_battery_brief`). Проверка: прогон
  `acpi-power` — QEMU с лишней SSDT `tools/test-battery.aml` (поддельные
  BAT0/ADP1/LID0; исходник `.asl`, собирается `iasl`), `battery`, снимок
  панели (зелёная заливка), `system_powerdown` → QEMU выключается сам.
  У QEMU нет EC — драйвер EC проверяется только на ноутбуке.
* **Яркость** (`kernel/backlight.c`): видеокарта Intel 0:2.0, BAR0 +
  0xC8250 — CTL1 (бит 31 вкл., бит 29 инверсия); PCH: 0xC8254 = период
  [31:16] | доля [15:0]; BXT (id 0x0A84/0x1A84/0x1A85/0x5A84/0x5A85/0x3184/
  0x3185): 0xC8254 период, 0xC8258 доля. Отображается одна страница
  0xC8000 (UC). Вид регистров: BXT/GLK по id видеокарты, иначе по
  семейству чипсета (LPC 0:1F.0, id & 0xFF80): LPT/WPT/SPT/KBP — старый
  (PCH), Cannon Point и новее — BXT (как `cnp_*` в i915); если числа не
  сходятся — пробуется другой. На HP 250 G7 первая версия ошиблась
  видом (ушла в ACPI `_BCM`, который у HP без драйвера видеокарты ничего
  не делает). `brightness debug` — сырые регистры. Нет ШИМ (выключен или нули) — первый узел AML с `_BCM`
  (уровни из `_BCL` без двух первых, текущий — `_BQC`). Notify 0x86/0x87
  на узле с `_BCM` (обработчик на корне) — шаг ±10% / соседний уровень.
  Минимум 5%. Проверка: `LCD0` в `tools/test-battery.asl`, прогон
  `acpi-power`.
* **Настройки** (`kernel/settings.c`): том загрузки = диск FAT, где по пути
  `g_boot.kernel_path` лежит файл размером `g_boot.kernel_file_size`;
  настройки — в его `EFI/MyOS/` (папка создаётся при первой записи).
  Wi-Fi (`net/wifi.c`): `wifi save` → `wifi.cfg` (`ssid=`, `psk=` 64 hex
  PMK или `open`), `wifi forget`, `wifi connect` без имени;
  `wifi_boot_autoconnect` в `kmain` после `net_init`: есть адаптер →
  `wlan_cmd_connect_key(..., wait=FALSE)` (поток wlan пробует сам). Пишется
  только по команде. Проверка: прогоны `wifi-save` и `wifi-save-reboot`
  (q35 — диск загрузки виден как `/sata0`; второй прогон грузится с того же
  образа).
* **Установка на диск** (`tools/install-arch.sh`, запуск из Arch): копирует
  загрузчик и ядро в `<ESP>/EFI/MyOS/`, пишет `<ESP>/loader/entries/myos.conf`
  (`efi /EFI/MyOS/BOOTX64.EFI`); `--menu` = `bootctl set-timeout 3`
  (переменная EFI, loader.conf не правится), `--remove`. Загрузчик ищет ядро
  в `\EFI\BOOT\`, в корне, затем в `\EFI\MYOS\` (`l_kernel_paths`).
  Прогон autotest `install`: образ FAT32 64 МБ с настоящим systemd-boot
  (default myos.conf) + скрипт установки -> команда `boot` должна показать
  `Kernel file: \EFI\MYOS\KERNEL.ELF`. Нужны пакеты systemd-boot-efi и
  mtools, иначе прогон пропускается.

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
