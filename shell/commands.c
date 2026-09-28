/*
 * shell/commands.c - разбор и выполнение команд шелла (run_command).
 * Часть MyOS; общие объявления - в myos.h.
 */
#include "myos.h"


/* ============================================================
 * Command dispatcher
 * ============================================================ */

void run_command(
    EFI_SYSTEM_TABLE *st,
    CHAR16 *line
)
{
    SIMPLE_TEXT_OUTPUT_INTERFACE *out =
        st->ConOut;


    push_history(line);


    /* --------------------------------------------------------
     * Команды, которым нужна прошивка (Boot Services) или
     * которые сбросили бы xHCI-контроллер из-под нашего же
     * работающего USB-драйвера - в kernel mode запрещены.
     * -------------------------------------------------------- */

    if (
        g_kernel_mode &&
        (streq(line, "xhci") || streq(line, "ebsdemo"))
    ) {

        print(
            out,
            "Not available in kernel mode: this command needs UEFI\n"
            "Boot Services, or would reset the USB controller that\n"
            "MyOS's own driver is using right now. See 'usb'.\n"
        );

        return;
    }


    /* --------------------------------------------------------
     * Empty
     * -------------------------------------------------------- */

    if (streq(line, "")) {

        return;


    /* --------------------------------------------------------
     * kinfo / usb / mem / int3 - состояние "ядра" (см. блок
     * KERNEL MODE выше)
     * -------------------------------------------------------- */

    } else if (streq(line, "kinfo")) {

        kernel_cmd_kinfo(out);

    } else if (streq(line, "usb")) {

        kernel_cmd_usb(out);

    } else if (streq(line, "mousetest")) {

        kernel_cmd_mousetest(out);

    } else if (streq(line, "mem")) {

        kernel_cmd_mem(st, out);

    } else if (streq(line, "crash")) {

        if (!g_kernel_mode) {

            print(
                out,
                "Only in kernel mode (after 'ebs') - our panic screen\n"
                "is not installed while the firmware is in charge.\n"
            );

        } else {

            print(out, "Executing an invalid instruction (ud2) on purpose...\n");

            /* пиксели консоли - на экран до того, как упадём */
            kcon_flush();

            /* ud2 - "гарантированно неверная инструкция",
               процессор бросает #UD (вектор 6) -> kx_panic */
            __asm__ __volatile__("ud2");
        }

    } else if (streq(line, "int3")) {

        if (!g_kernel_mode) {

            /* обработчик #BP сейчас - прошивочный: в OVMF он
               печатает дамп и вешает машину */
            print(
                out,
                "Only in kernel mode (after 'ebs') - right now the\n"
                "firmware's exception handler is active.\n"
            );

        } else {

            UINT64 before = g_kbreakpoints;

            __asm__ __volatile__("int3");

            if (g_kbreakpoints == before + 1) {

                print(out, "int3 -> our IDT vector 3 handler ran and returned.\n");
                print(out, "Return address (RIP) = 0x");
                print_hex(out, g_kbp_rip, 16);
                print(out, "\n");

            } else {

                print(out, "int3 did not reach our handler?!\n");
            }
        }


    /* --------------------------------------------------------
     * help
     * -------------------------------------------------------- */

    } else if (streq(line, "help")) {

        print(
            out,
            "Available commands:\n"
        );

        print(
            out,
            "  help          - show this list\n"
        );

        print(
            out,
            "  fetch         - show system info (like neofetch/fastfetch)\n"
        );

        print(
            out,
            "  about         - short info about MyOS\n"
        );

        print(
            out,
            "  ver           - print MyOS version\n"
        );

        print(
            out,
            "  banner        - reprint the startup banner\n"
        );

        print(
            out,
            "  time          - show current UEFI time\n"
        );

        print(
            out,
            "  date          - show current UEFI date\n"
        );

        print(
            out,
            "  uptime        - time elapsed since boot\n"
        );

        print(
            out,
            "  echo <text>   - print text back\n"
        );

        print(
            out,
            "  calc <a> <op> <b> - basic calculator (+ - * /)\n"
        );

        print(
            out,
            "  color <0-15>  - change text color (0=black..15=white)\n"
        );

        print(
            out,
            "  clear         - clear the screen\n"
        );

        print(
            out,
            "  ebs           - leave UEFI firmware for good and keep running\n"
            "                  on MyOS drivers (timer, memory, USB/PS2 input)\n"
        );

        print(
            out,
            "  ebsdemo       - old step-by-step xHCI mouse demo (reboots after)\n"
        );

        print(
            out,
            "  kinfo         - kernel status: timer, interrupts, input\n"
        );

        print(
            out,
            "  usb           - USB devices found by MyOS's own driver\n"
        );

        print(
            out,
            "  mousetest     - live USB mouse/keyboard diagnostics (kernel mode)\n"
        );

        print(
            out,
            "  mem           - memory map and page allocator status\n"
        );

        print(
            out,
            "  int3          - test our interrupt table (kernel mode)\n"
        );

        print(
            out,
            "  crash         - trigger a CPU exception -> panic screen\n"
            "                  (kernel mode; the machine halts after it)\n"
        );

        print(
            out,
            "  lspci         - scan PCI bus via raw ports, find USB controllers\n"
        );

        print(
            out,
            "  xhci          - xHCI: reset, Enable Slot, Address Device,\n"
            "                  GET_DESCRIPTOR (real USB VID/PID)\n"
        );

        print(
            out,
            "  history       - show recently run commands\n"
        );

        print(
            out,
            "  whoami        - print current user\n"
        );

        print(
            out,
            "  sleep <sec>   - pause for N seconds\n"
        );

        print(
            out,
            "  --- files (RAM disk, cleared on reboot) ---\n"
        );

        print(
            out,
            "  ls            - list files\n"
        );

        print(
            out,
            "  touch <name>  - create an empty file\n"
        );

        print(
            out,
            "  cat <name>    - print file contents\n"
        );

        print(
            out,
            "  write <n> <t> - overwrite file with text\n"
        );

        print(
            out,
            "  append <n> <t>- append text to file\n"
        );

        print(
            out,
            "  edit <name>   - multi-line editor, end with '.'\n"
        );

        print(
            out,
            "  rm <name>     - delete a file\n"
        );

        print(
            out,
            "  mv <a> <b>    - rename a file\n"
        );

        print(
            out,
            "  cp <a> <b>    - copy a file\n"
        );

        print(
            out,
            "  size <name>   - show file size in bytes\n"
        );

        print(
            out,
            "  start         - launch GUI desktop (icons, taskbar clock, arrows+Enter)\n"
        );

        print(
            out,
            "  reboot        - cold reboot\n"
        );

        print(
            out,
            "  shutdown      - power off the machine\n"
        );

        print(
            out,
            "  exit          - same as shutdown\n"
        );


    /* --------------------------------------------------------
     * whoami
     * -------------------------------------------------------- */

    } else if (streq(line, "whoami")) {

        print(
            out,
            "root@myos\n"
        );


    /* --------------------------------------------------------
     * history
     * -------------------------------------------------------- */

    } else if (streq(line, "history")) {

        cmd_history(st);


    /* --------------------------------------------------------
     * sleep
     * -------------------------------------------------------- */

    } else if (starts_with(line, "sleep ")) {

        UINTN secs =
            parse_uint(
                line + 6
            );


        for (UINTN i = 0;
             i < secs;
             i++) {

            st->BootServices->Stall(
                1000000
            );
        }


        print(
            out,
            "Woke up.\n"
        );


    /* --------------------------------------------------------
     * calc
     * -------------------------------------------------------- */

    } else if (starts_with(line, "calc ")) {

        CHAR16 tmp[LINE_MAX];


        char16_copy(
            tmp,
            line + 5,
            LINE_MAX
        );


        cmd_calc(
            st,
            tmp
        );


    /* --------------------------------------------------------
     * ls
     * -------------------------------------------------------- */

    } else if (
        streq(line, "ls") ||
        streq(line, "dir") ||
        streq(line, "files")
    ) {

        cmd_ls(st);


    /* --------------------------------------------------------
     * touch
     * -------------------------------------------------------- */

    } else if (starts_with(line, "touch ")) {

        CHAR16 name[FS_NAME_MAX];


        take_word(
            skip_ws16(line + 6),
            name,
            FS_NAME_MAX
        );


        cmd_touch(
            st,
            name
        );


    /* --------------------------------------------------------
     * cat
     * -------------------------------------------------------- */

    } else if (starts_with(line, "cat ")) {

        CHAR16 name[FS_NAME_MAX];


        take_word(
            skip_ws16(line + 4),
            name,
            FS_NAME_MAX
        );


        cmd_cat(
            st,
            name
        );


    /* --------------------------------------------------------
     * write
     * -------------------------------------------------------- */

    } else if (starts_with(line, "write ")) {

        CHAR16 name[FS_NAME_MAX];


        CHAR16 *rest =
            take_word(
                skip_ws16(line + 6),
                name,
                FS_NAME_MAX
            );


        rest =
            skip_ws16(rest);


        cmd_write(
            st,
            name,
            rest
        );


    /* --------------------------------------------------------
     * append
     * -------------------------------------------------------- */

    } else if (starts_with(line, "append ")) {

        CHAR16 name[FS_NAME_MAX];


        CHAR16 *rest =
            take_word(
                skip_ws16(line + 7),
                name,
                FS_NAME_MAX
            );


        rest =
            skip_ws16(rest);


        cmd_append(
            st,
            name,
            rest
        );


    /* --------------------------------------------------------
     * edit
     * -------------------------------------------------------- */

    } else if (starts_with(line, "edit ")) {

        CHAR16 name[FS_NAME_MAX];


        take_word(
            skip_ws16(line + 5),
            name,
            FS_NAME_MAX
        );


        cmd_edit(
            st,
            name
        );


    /* --------------------------------------------------------
     * rm
     * -------------------------------------------------------- */

    } else if (starts_with(line, "rm ")) {

        CHAR16 name[FS_NAME_MAX];


        take_word(
            skip_ws16(line + 3),
            name,
            FS_NAME_MAX
        );


        cmd_rm(
            st,
            name
        );


    /* --------------------------------------------------------
     * mv
     * -------------------------------------------------------- */

    } else if (starts_with(line, "mv ")) {

        CHAR16 a[FS_NAME_MAX];
        CHAR16 b[FS_NAME_MAX];


        CHAR16 *rest =
            take_word(
                skip_ws16(line + 3),
                a,
                FS_NAME_MAX
            );


        rest =
            skip_ws16(rest);


        take_word(
            rest,
            b,
            FS_NAME_MAX
        );


        cmd_mv(
            st,
            a,
            b
        );


    /* --------------------------------------------------------
     * cp
     * -------------------------------------------------------- */

    } else if (starts_with(line, "cp ")) {

        CHAR16 a[FS_NAME_MAX];
        CHAR16 b[FS_NAME_MAX];


        CHAR16 *rest =
            take_word(
                skip_ws16(line + 3),
                a,
                FS_NAME_MAX
            );


        rest =
            skip_ws16(rest);


        take_word(
            rest,
            b,
            FS_NAME_MAX
        );


        cmd_cp(
            st,
            a,
            b
        );


    /* --------------------------------------------------------
     * size
     * -------------------------------------------------------- */

    } else if (starts_with(line, "size ")) {

        CHAR16 name[FS_NAME_MAX];


        take_word(
            skip_ws16(line + 5),
            name,
            FS_NAME_MAX
        );


        int idx =
            fs_find(name);


        if (idx < 0) {

            print(
                out,
                "No such file.\n"
            );

        } else {

            print_uint(
                out,
                g_fs[idx].size
            );

            print(
                out,
                " bytes\n"
            );
        }


    /* --------------------------------------------------------
     * fetch
     * -------------------------------------------------------- */

    } else if (
        streq(line, "fetch") ||
        streq(line, "neofetch") ||
        streq(line, "fastfetch")
    ) {

        cmd_fetch(st);


    /* --------------------------------------------------------
     * about
     * -------------------------------------------------------- */

    } else if (streq(line, "about")) {

        print(
            out,
            "MyOS 0.1 - a minimal 64-bit UEFI OS built from scratch.\n"
        );

        print(
            out,
            "No Linux, no Windows, no GNU-EFI/EDK2 - just gcc + ld.\n"
        );

        print(
            out,
            "Type 'fetch' for a system summary or 'help' for commands.\n"
        );


    /* --------------------------------------------------------
     * version
     * -------------------------------------------------------- */

    } else if (
        streq(line, "ver") ||
        streq(line, "version")
    ) {

        print(
            out,
            "MyOS 0.1\n"
        );


    /* --------------------------------------------------------
     * banner
     * -------------------------------------------------------- */

    } else if (streq(line, "banner")) {

        print(
            out,
            "================================\n"
        );

        print(
            out,
            "   MyOS 0.1 - kernel base\n"
        );

        print(
            out,
            "   64-bit UEFI, no Linux/Windows\n"
        );

        print(
            out,
            "================================\n"
        );


    /* --------------------------------------------------------
     * time
     * -------------------------------------------------------- */

    } else if (streq(line, "time")) {

        EFI_TIME now;


        if (
            st->RuntimeServices->GetTime &&
            st->RuntimeServices->GetTime(
                &now,
                NULL
            ) == EFI_SUCCESS
        ) {

            print_uint2(
                out,
                now.Hour
            );

            print(
                out,
                ":"
            );

            print_uint2(
                out,
                now.Minute
            );

            print(
                out,
                ":"
            );

            print_uint2(
                out,
                now.Second
            );

            print(
                out,
                "\n"
            );

        } else {

            print(
                out,
                "Time service unavailable.\n"
            );
        }


    /* --------------------------------------------------------
     * date
     * -------------------------------------------------------- */

    } else if (streq(line, "date")) {

        EFI_TIME now;


        if (
            st->RuntimeServices->GetTime &&
            st->RuntimeServices->GetTime(
                &now,
                NULL
            ) == EFI_SUCCESS
        ) {

            print_uint2(
                out,
                now.Day
            );

            print(
                out,
                "-"
            );

            print_uint2(
                out,
                now.Month
            );

            print(
                out,
                "-"
            );

            print_uint(
                out,
                now.Year
            );

            print(
                out,
                "\n"
            );

        } else {

            print(
                out,
                "Date service unavailable.\n"
            );
        }


    /* --------------------------------------------------------
     * uptime
     * -------------------------------------------------------- */

    } else if (streq(line, "uptime")) {

        if (
            !g_have_boot_time ||
            !st->RuntimeServices->GetTime
        ) {

            print(
                out,
                "Uptime unavailable.\n"
            );

        } else {

            EFI_TIME now;


            if (
                st->RuntimeServices->GetTime(
                    &now,
                    NULL
                ) == EFI_SUCCESS
            ) {

                INT64 secs =
                    (INT64)now.Hour * 3600 +
                    (INT64)now.Minute * 60 +
                    now.Second
                    -
                    (
                        (INT64)g_boot_time.Hour * 3600 +
                        (INT64)g_boot_time.Minute * 60 +
                        g_boot_time.Second
                    );


                if (secs < 0)
                    secs += 86400;


                print(
                    out,
                    "up "
                );


                print_uint(
                    out,
                    (UINT64)secs / 3600
                );


                print(
                    out,
                    "h "
                );


                print_uint(
                    out,
                    ((UINT64)secs / 60) % 60
                );


                print(
                    out,
                    "m "
                );


                print_uint(
                    out,
                    (UINT64)secs % 60
                );


                print(
                    out,
                    "s\n"
                );
            }
        }


    /* --------------------------------------------------------
     * echo
     * -------------------------------------------------------- */

    } else if (starts_with(line, "echo ")) {

        /*
         * print16(), чтобы echo попадал
         * в scrollback.
         */
        print16(
            out,
            line + 5
        );

        print(
            out,
            "\n"
        );


    } else if (streq(line, "echo")) {

        print(
            out,
            "\n"
        );


    /* --------------------------------------------------------
     * color
     * -------------------------------------------------------- */

    } else if (starts_with(line, "color ")) {

        UINTN c =
            parse_uint(
                line + 6
            );


        if (c > 15) {

            print(
                out,
                "Usage: color <0-15>\n"
            );

        } else {

            g_color = c;


            set_color(
                out,
                g_color
            );


            print(
                out,
                "Color changed.\n"
            );
        }


    /* --------------------------------------------------------
     * clear
     * -------------------------------------------------------- */

    } else if (
        streq(line, "clear") ||
        streq(line, "cls")
    ) {

        out->ClearScreen(out);


    /* --------------------------------------------------------
     * lspci - Шаг 2: своими руками, через порты 0xCF8/0xCFC,
     * найти на шине PCI все устройства и, отдельно, USB-
     * контроллер(ы). Не использует ни одного UEFI-протокола -
     * тот же самый код будет годиться и после ExitBootServices
     * (см. команду "ebs", которая его туда и подключает).
     * -------------------------------------------------------- */

    } else if (streq(line, "lspci")) {

        UINTN lspci_found = 0;
        UINTN lspci_usb    = 0;

        for (UINTN bus = 0; bus < 256; bus++) {

            for (UINTN dev = 0; dev < 32; dev++) {

                UINT32 id0 =
                    pci_config_read32(
                        (UINT8)bus, (UINT8)dev, 0, 0x00
                    );

                if ((id0 & 0xFFFF) == 0xFFFF)
                    continue;

                UINT32 hdr_dword =
                    pci_config_read32(
                        (UINT8)bus, (UINT8)dev, 0, 0x0C
                    );

                BOOLEAN multi_func =
                    (((hdr_dword >> 16) & 0x80) != 0);

                UINTN max_func = multi_func ? 8 : 1;

                for (UINTN func = 0; func < max_func; func++) {

                    UINT32 id =
                        (func == 0)
                            ? id0
                            : pci_config_read32(
                                  (UINT8)bus, (UINT8)dev,
                                  (UINT8)func, 0x00
                              );

                    if ((id & 0xFFFF) == 0xFFFF)
                        continue;

                    lspci_found++;

                    UINT32 class_dword =
                        pci_config_read32(
                            (UINT8)bus, (UINT8)dev,
                            (UINT8)func, 0x08
                        );

                    UINT8 base_class =
                        PCI_CLASS_DWORD_BASE_CLASS(class_dword);
                    UINT8 sub_class =
                        PCI_CLASS_DWORD_SUB_CLASS(class_dword);
                    UINT8 prog_if =
                        PCI_CLASS_DWORD_PROG_IF(class_dword);

                    print_uint(out, bus);
                    print(out, ":");
                    print_uint(out, dev);
                    print(out, ".");
                    print_uint(out, func);
                    print(out, "  vendor=");
                    print_hex(out, id & 0xFFFF, 4);
                    print(out, " device=");
                    print_hex(out, (id >> 16) & 0xFFFF, 4);
                    print(out, "  class=");
                    print_hex(out, base_class, 2);
                    print(out, " sub=");
                    print_hex(out, sub_class, 2);
                    print(out, " progif=");
                    print_hex(out, prog_if, 2);

                    if (
                        base_class == PCI_CLASS_SERIAL_BUS &&
                        sub_class  == PCI_SUBCLASS_USB
                    ) {

                        lspci_usb++;

                        UINT64 bar =
                            pci_read_bar_address(
                                (UINT8)bus, (UINT8)dev,
                                (UINT8)func, 0x10
                            );

                        if (prog_if == PCI_PROGIF_XHCI) {
                            print(out, "  <-- XHCI (USB3), MMIO=0x");
                        } else if (prog_if == PCI_PROGIF_EHCI) {
                            print(out, "  <-- EHCI (USB2), MMIO=0x");
                        } else if (prog_if == PCI_PROGIF_OHCI) {
                            print(out, "  <-- OHCI (USB1.1), MMIO=0x");
                        } else if (prog_if == PCI_PROGIF_UHCI) {
                            print(out, "  <-- UHCI (USB1.1), MMIO=0x");
                        } else {
                            print(out, "  <-- USB controller, MMIO=0x");
                        }

                        print_hex(out, bar, 16);
                    }

                    print(out, "\n");
                }
            }
        }

        print(out, "\n");
        print_uint(out, lspci_found);
        print(out, " device(s) found, ");
        print_uint(out, lspci_usb);
        print(out, " USB controller(s).\n");

        if (lspci_usb == 0) {

            print(
                out,
                "No USB controller on the PCI bus at all - "
                "this VM/machine has none configured "
                "(in QEMU add e.g. -device qemu-xhci).\n"
            );
        }


    /* --------------------------------------------------------
     * xhci - Шаг 3: не просто найти контроллер на шине PCI
     * (это уже делает lspci), а заговорить с ним самим -
     * включить Memory Space + Bus Master в его PCI Command
     * регистре и прочитать его собственные Capability
     * Registers через MMIO. Отсюда, в частности, берётся
     * MaxPorts - число портов корневого хаба, то есть куда
     * физически может быть воткнута мышь.
     * -------------------------------------------------------- */

    } else if (streq(line, "porttest")) {

        /*
         * ВРЕМЕННАЯ диагностическая команда (отладка
         * зависания в "xhci"): повторяет ТОЧНО ТУ ЖЕ
         * последовательность печати, что и порт-сканер
         * в "xhci" (префикс + hex-значение + "empty"),
         * восемь раз подряд, но БЕЗ единого обращения
         * к PCI/MMIO - только вымышленные, зашитые в
         * код значения.
         *
         * Если это тоже зависнет на 2-3 итерации - значит
         * дело в самой печати (ConOut/scrollback), никак
         * не связано с xHCI. Если пройдёт всё 8 раз без
         * проблем - значит зависание вызывают именно
         * PCI/MMIO-операции xHCI-кода (скорее всего порча
         * памяти где-то в DCBAA/Command Ring/Event Ring
         * страницах).
         */

        print(out, "porttest: repeating print pattern "
                    "8 times, no PCI/MMIO involved\n\n");

        for (UINTN p = 1; p <= 8; p++) {

            print(out, "[DBG] reading port ");
            print_uint(out, p);
            print(out, "\n");

            UINT32 fake_portsc = 0x000202A0;

            print(out, "  Port ");
            print_uint(out, p);
            print(out, ": PORTSC=0x");
            print_hex(out, fake_portsc, 8);
            print(out, "  empty\n");
        }

        print(out, "\nporttest: done, all 8 iterations "
                    "completed.\n");

    } else if (streq(line, "xhci")) {

        /*
         * Эта команда теперь делает только "предполётную"
         * проверку - находит контроллер, включает его в PCI
         * Command register, отбирает его у собственного
         * драйвера прошивки (если он там есть) и показывает
         * Capability Registers. Все эти шаги ОБЯЗАНЫ выполняться
         * до ExitBootServices - для отключения родного драйвера
         * прошивки (xhci_disconnect_firmware_driver) нужны
         * живые Boot Services (LocateHandleBuffer/HandleProtocol/
         * DisconnectController), без них это в принципе
         * невозможно.
         *
         * Весь остальной путь - сброс контроллера, запуск,
         * поиск порта, Enable Slot, Address Device,
         * GET_DESCRIPTOR, SET_CONFIGURATION и опрос отчётов
         * мыши - теперь выполняется ЦЕЛИКОМ ПОСЛЕ
         * ExitBootServices, автоматически, как часть команды
         * "ebs" (см. ниже) - без единого обращения к прошивке.
         */

        UINT8  xbus = 0, xdev = 0, xfunc = 0;
        UINT64 xmmio = 0;

        if (!pci_find_xhci(&xbus, &xdev, &xfunc, &xmmio)) {

            print(
                out,
                "No xHCI controller found on the PCI bus "
                "(run 'lspci' to see what is there).\n"
            );

        } else if (xmmio == 0) {

            print(
                out,
                "xHCI found, but its BAR0 is an I/O-port BAR, "
                "not memory-mapped - this driver only handles "
                "the memory-mapped case.\n"
            );

        } else {

            print(out, "xHCI at ");
            print_uint(out, xbus);
            print(out, ":");
            print_uint(out, xdev);
            print(out, ".");
            print_uint(out, xfunc);
            print(out, ", MMIO base=0x");
            print_hex(out, xmmio, 16);
            print(out, "\n");

            pci_enable_device(xbus, xdev, xfunc);

            print(
                out,
                "Memory Space + Bus Master enabled in PCI "
                "Command register.\n\n"
            );

            print(
                out,
                "--- disconnecting firmware's own driver "
                "from this device (if any) ---\n"
            );

            xhci_disconnect_firmware_driver(
                st, out, xbus, xdev, xfunc
            );

            print(out, "\n");

            XHCI_CAP_INFO cap;
            xhci_read_cap_regs(xmmio, &cap);

            print(out, "CapLength   = ");
            print_uint(out, cap.CapLength);
            print(out, " bytes\n");

            print(out, "HCI Version = ");
            print_hex(out, (cap.HciVersion >> 8) & 0xFF, 2);
            print(out, ".");
            print_hex(out, cap.HciVersion & 0xFF, 2);
            print(out, "  (raw 0x");
            print_hex(out, cap.HciVersion, 4);
            print(out, ")\n");

            print(out, "MaxSlots    = ");
            print_uint(out, cap.MaxSlots);
            print(out, "  (device slots the controller can track)\n");

            print(out, "MaxIntrs    = ");
            print_uint(out, cap.MaxIntrs);
            print(out, "  (interrupter lines)\n");

            print(out, "MaxPorts    = ");
            print_uint(out, cap.MaxPorts);
            print(out, "  (root hub ports - where devices plug in)\n");

            print(out, "DbOff       = 0x");
            print_hex(out, cap.DbOff, 8);
            print(out, "  (Doorbell registers offset)\n");

            print(out, "RtsOff      = 0x");
            print_hex(out, cap.RtsOff, 8);
            print(out, "  (Runtime registers offset)\n");

            print(out, "\n--- USB Legacy Support handoff ---\n");
            xhci_bios_handoff(
                st, xmmio, cap.ExtCapOff, out
            );

            if (cap.CapLength == 0 && cap.HciVersion == 0) {

                print(
                    out,
                    "\nAll zeros - MMIO is not actually "
                    "responding (wrong address, device not "
                    "enabled, or firmware did not map this "
                    "region). This is the next thing to fix, "
                    "not a working controller yet.\n"
                );

            } else {

                print(
                    out,
                    "\nController looks alive. The full "
                    "reset + init + mouse-polling pipeline no "
                    "longer runs from here - it now runs "
                    "automatically after ExitBootServices, as "
                    "part of the 'ebs' command, with zero "
                    "dependency on firmware from that point on. "
                    "Run 'ebs' to see it.\n"
                );
            }
        }


    /* --------------------------------------------------------
     * start (GUI)
     * -------------------------------------------------------- */

    } else if (streq(line, "start")) {

        gui_start(st);


    /* --------------------------------------------------------
     * ebs - Шаг 1 эксперимента "жизнь без Boot Services".
     *
     * Кэширует framebuffer, по-настоящему вызывает
     * ExitBootServices, и доказывает, что ОС жива после этого,
     * рисуя прямо в видеопамять собственным пиксельным шрифтом -
     * без ConOut, без ConIn, без Stall, без LocateProtocol.
     * Назад в этот шелл дороги нет (клавиатура всё равно не
     * будет работать), поэтому демо завершается своей же
     * перезагрузкой через RuntimeServices.
     * -------------------------------------------------------- */

    } else if (streq(line, "ebs")) {

        /* Новый "ebs": выйти из прошивки НАСОВСЕМ и продолжить
           работать - шелл, GUI, мышь, клавиатура уже на своих
           драйверах (см. блок KERNEL MODE выше) */
        kernel_ebs_and_enter(st);


    /* --------------------------------------------------------
     * ebsdemo - старое демо (бывший "ebs"): выход из Boot
     * Services, пошаговый лог xHCI-драйвера одной мыши, 200
     * отчётов, перезагрузка. Оставлено как подробная
     * диагностика - удобно, если новый драйвер на каком-то
     * железе поведёт себя не так.
     * -------------------------------------------------------- */

    } else if (streq(line, "ebsdemo")) {

        print(out, "=== ExitBootServices demo ===\n");
        print(out, "This will permanently exit UEFI Boot Services.\n");
        print(out, "After that: no more keyboard, no more mouse\n");
        print(out, "protocol, no more Stall() - this shell will\n");
        print(out, "not come back. If a USB mouse controller is\n");
        print(out, "found, its ENTIRE driver (reset, init, Enable\n");
        print(out, "Slot, Address Device, GET_DESCRIPTOR, report\n");
        print(out, "polling) now runs AFTER ExitBootServices too -\n");
        print(out, "with zero dependency on firmware from that\n");
        print(out, "point on. Then the machine reboots itself.\n");
        print(out, "Starting in 3 seconds...\n");

        if (st->BootServices->Stall)
            st->BootServices->Stall(3000000);

        /* 1. Найти GOP и закэшировать всё, что понадобится, -
              ПОСЛЕ выхода LocateProtocol звать уже нельзя. */

        EFI_GUID ebs_gop_guid = EFI_GRAPHICS_OUTPUT_PROTOCOL_GUID;
        EFI_GRAPHICS_OUTPUT_PROTOCOL *ebs_gop = NULL;

        if (
            !st->BootServices->LocateProtocol ||
            st->BootServices->LocateProtocol(
                &ebs_gop_guid, NULL, (VOID **)&ebs_gop
            ) != EFI_SUCCESS ||
            !ebs_gop || !ebs_gop->Mode || !ebs_gop->Mode->Info
        ) {

            print(
                out,
                "No Graphics Output Protocol - aborting, "
                "nothing to draw with after the exit.\n"
            );

            return;
        }

        volatile UINT32 *ebs_fb =
            (volatile UINT32 *)ebs_gop->Mode->FrameBufferBase;

        UINT32 ebs_fb_w = ebs_gop->Mode->Info->HorizontalResolution;
        UINT32 ebs_fb_h = ebs_gop->Mode->Info->VerticalResolution;
        UINT32 ebs_stride = ebs_gop->Mode->Info->PixelsPerScanLine;

        EFI_GRAPHICS_PIXEL_FORMAT ebs_fmt =
            ebs_gop->Mode->Info->PixelFormat;

        /*
         * 1.5. Подготовка xHCI, ПОКА Boot Services ещё живы.
         *
         * Две вещи здесь принципиально обязаны случиться ДО
         * ExitBootServices и никак иначе:
         *
         *   - xhci_disconnect_firmware_driver() пользуется
         *     самими Boot Services (LocateHandleBuffer/
         *     HandleProtocol/DisconnectController), чтобы
         *     попросить прошивку отпустить устройство - без
         *     живых Boot Services это в принципе невозможно
         *     сделать (и было бы поздно уже после выхода -
         *     родной драйвер прошивки к тому моменту либо
         *     мёртв, либо не важен, но ПОКА мы ещё не вышли,
         *     он может конфликтовать с нами за одни и те же
         *     регистры, отсюда и сам смысл этого шага);
         *
         *   - AllocatePages() - Boot Service, после выхода
         *     недоступен вообще. Поэтому все страницы, которые
         *     понадобятся ПОСЛЕ выхода (DCBAA, Command Ring,
         *     Event Ring, ERST, Input/Device Context, EP0 ring,
         *     буфер под дескрипторы, Transfer Ring под
         *     Interrupt IN endpoint - 9 страниц), выделяются
         *     здесь заранее и передаются дальше уже готовыми
         *     физическими адресами.
         *
         * Сам сброс контроллера, запуск, сканирование портов,
         * Enable Slot, Address Device и опрос отчётов мыши -
         * ничего из этого Boot Services не требует (чистый
         * MMIO/порты PCI), поэтому выполняется уже ПОСЛЕ
         * ExitBootServices, через xhci_run_post_ebs() ниже.
         */

        UINT8  ebs_xbus = 0, ebs_xdev = 0, ebs_xfunc = 0;
        UINT64 ebs_xmmio = 0;

        BOOLEAN ebs_xhci_ready = FALSE;
        XHCI_CAP_INFO ebs_cap;

        UINT64 ebs_dcbaa_phys = 0;
        UINT64 ebs_cmdring_phys = 0;
        UINT64 ebs_evring_phys = 0;
        UINT64 ebs_erst_phys = 0;
        UINT64 ebs_input_ctx_phys = 0;
        UINT64 ebs_dev_ctx_phys = 0;
        UINT64 ebs_ep0_ring_phys = 0;
        UINT64 ebs_desc_buf_phys = 0;
        UINT64 ebs_int_ring_phys = 0;

        BOOLEAN ebs_xhci_found =
            pci_find_xhci(
                &ebs_xbus, &ebs_xdev, &ebs_xfunc, &ebs_xmmio
            );

        if (ebs_xhci_found && ebs_xmmio != 0) {

            print(out, "\nxHCI at ");
            print_uint(out, ebs_xbus);
            print(out, ":");
            print_uint(out, ebs_xdev);
            print(out, ".");
            print_uint(out, ebs_xfunc);
            print(out, " - preparing driver before exit...\n");

            pci_enable_device(ebs_xbus, ebs_xdev, ebs_xfunc);

            xhci_disconnect_firmware_driver(
                st, out, ebs_xbus, ebs_xdev, ebs_xfunc
            );

            xhci_read_cap_regs(ebs_xmmio, &ebs_cap);
            xhci_bios_handoff(
                st, ebs_xmmio, ebs_cap.ExtCapOff, out
            );

            GUI_ALLOCATE_PAGES ebs_alloc_pages =
                (GUI_ALLOCATE_PAGES)
                    st->BootServices->AllocatePages;

            if (
                ebs_cap.CapLength == 0 &&
                ebs_cap.HciVersion == 0
            ) {

                print(
                    out,
                    "MMIO is not responding - skipping the "
                    "mouse driver.\n"
                );

            } else if (!ebs_alloc_pages) {

                print(
                    out,
                    "AllocatePages is not available - "
                    "skipping the mouse driver.\n"
                );

            } else if (
                ebs_alloc_pages(
                    GUI_ALLOCATE_ANY_PAGES,
                    GUI_EFI_BOOT_SERVICES_DATA,
                    1, &ebs_dcbaa_phys
                ) != EFI_SUCCESS ||
                ebs_alloc_pages(
                    GUI_ALLOCATE_ANY_PAGES,
                    GUI_EFI_BOOT_SERVICES_DATA,
                    1, &ebs_cmdring_phys
                ) != EFI_SUCCESS ||
                ebs_alloc_pages(
                    GUI_ALLOCATE_ANY_PAGES,
                    GUI_EFI_BOOT_SERVICES_DATA,
                    1, &ebs_evring_phys
                ) != EFI_SUCCESS ||
                ebs_alloc_pages(
                    GUI_ALLOCATE_ANY_PAGES,
                    GUI_EFI_BOOT_SERVICES_DATA,
                    1, &ebs_erst_phys
                ) != EFI_SUCCESS ||
                ebs_alloc_pages(
                    GUI_ALLOCATE_ANY_PAGES,
                    GUI_EFI_BOOT_SERVICES_DATA,
                    1, &ebs_input_ctx_phys
                ) != EFI_SUCCESS ||
                ebs_alloc_pages(
                    GUI_ALLOCATE_ANY_PAGES,
                    GUI_EFI_BOOT_SERVICES_DATA,
                    1, &ebs_dev_ctx_phys
                ) != EFI_SUCCESS ||
                ebs_alloc_pages(
                    GUI_ALLOCATE_ANY_PAGES,
                    GUI_EFI_BOOT_SERVICES_DATA,
                    1, &ebs_ep0_ring_phys
                ) != EFI_SUCCESS ||
                ebs_alloc_pages(
                    GUI_ALLOCATE_ANY_PAGES,
                    GUI_EFI_BOOT_SERVICES_DATA,
                    1, &ebs_desc_buf_phys
                ) != EFI_SUCCESS ||
                ebs_alloc_pages(
                    GUI_ALLOCATE_ANY_PAGES,
                    GUI_EFI_BOOT_SERVICES_DATA,
                    1, &ebs_int_ring_phys
                ) != EFI_SUCCESS
            ) {

                print(
                    out,
                    "Could not allocate the pages the driver "
                    "will need after the exit - skipping the "
                    "mouse driver.\n"
                );

            } else {

                print(
                    out,
                    "All pages pre-allocated. Driver will run "
                    "fully after ExitBootServices.\n"
                );

                ebs_xhci_ready = TRUE;
            }

        } else {

            print(
                out,
                "\nNo xHCI controller found - continuing "
                "without the mouse driver.\n"
            );
        }

        /* 2. Получить карту памяти и вызвать ExitBootServices.
              MapKey может устареть между двумя вызовами (если
              что-то ещё меняет карту памяти между ними), поэтому
              - стандартный из спецификации UEFI цикл повтора. */

        GUI_GET_MEMORY_MAP ebs_get_map =
            (GUI_GET_MEMORY_MAP)st->BootServices->GetMemoryMap;

        GUI_ALLOCATE_POOL ebs_alloc =
            (GUI_ALLOCATE_POOL)st->BootServices->AllocatePool;

        GUI_EXIT_BOOT_SERVICES ebs_exit =
            (GUI_EXIT_BOOT_SERVICES)st->BootServices->ExitBootServices;

        if (!ebs_get_map || !ebs_alloc || !ebs_exit) {

            print(
                out,
                "Firmware is missing required Boot Services "
                "functions - aborting.\n"
            );

            return;
        }

        UINTN ebs_map_size = 0;
        UINTN ebs_map_key  = 0;
        UINTN ebs_desc_size = 0;
        UINT32 ebs_desc_ver = 0;

        /* Первый вызов - только чтобы узнать нужный размер. */
        ebs_get_map(
            &ebs_map_size, NULL, &ebs_map_key,
            &ebs_desc_size, &ebs_desc_ver
        );

        /* Запас на случай, если сам AllocatePool ниже добавит
           в карту памяти новую запись под свою аллокацию. */
        ebs_map_size += ebs_desc_size * 8;

        VOID *ebs_map_buf = NULL;

        if (
            ebs_alloc(
                GUI_EFI_BOOT_SERVICES_DATA,
                ebs_map_size,
                &ebs_map_buf
            ) != EFI_SUCCESS
        ) {

            print(
                out,
                "Could not allocate memory map buffer - aborting.\n"
            );

            return;
        }

        BOOLEAN ebs_ok = FALSE;

        for (UINTN ebs_try = 0; ebs_try < 4; ebs_try++) {

            UINTN this_size = ebs_map_size;

            if (
                ebs_get_map(
                    &this_size, ebs_map_buf, &ebs_map_key,
                    &ebs_desc_size, &ebs_desc_ver
                ) != EFI_SUCCESS
            ) {
                break;
            }

            if (ebs_exit(g_image_handle, ebs_map_key) == EFI_SUCCESS) {
                ebs_ok = TRUE;
                break;
            }

            /* MapKey успел устареть - прошивка поменяла карту
               памяти между двумя вызовами выше. Берём карту
               заново и пробуем ещё раз. */
        }

        if (!ebs_ok) {

            print(
                out,
                "ExitBootServices failed after several attempts - "
                "aborting. Boot Services are still active, it is "
                "safe to keep using the shell.\n"
            );

            return;
        }

        /*
         * === Дальше Boot Services больше нет. ===
         *
         * Никаких LocateProtocol/AllocatePool/Stall/ConIn/ConOut -
         * они держались на коде прошивки, который теперь либо
         * освобождён, либо не гарантированно работает. Есть
         * только: ebs_fb/ebs_stride/... (закэшированы ДО выхода),
         * RuntimeServices (GetTime, ResetSystem и т.п. - по
         * спецификации обязаны работать и после ExitBootServices),
         * и собственный код (gui_fill_rect/gui_draw_text - чистая
         * работа с пикселями в буфере, без обращений к прошивке).
         */

        /*
         * Строка со статусом xHCI-контроллера - результат
         * pci_find_xhci(), выполненного ЕЩЁ ДО ExitBootServices
         * (см. выше, шаг 1.5). Повторно искать контроллер здесь
         * не нужно: сам факт того, что весь драйвер ниже
         * (xhci_run_post_ebs) успешно работает с уже найденным
         * ebs_xmmio ПОСЛЕ выхода из Boot Services, и есть живое
         * доказательство независимости от прошивки - искать
         * заново ничего не доказывает дополнительно, только
         * дублирует код.
         */
        char ebs_xhci_line[64];

        if (ebs_xhci_found) {

            UINTN p = 0;
            const char *seg;

            seg = "XHCI: BUS ";
            for (UINTN i = 0; seg[i] != '\0'; i++)
                ebs_xhci_line[p++] = seg[i];

            p += gui_uint_to_str(ebs_xbus, ebs_xhci_line + p);

            seg = " DEV ";
            for (UINTN i = 0; seg[i] != '\0'; i++)
                ebs_xhci_line[p++] = seg[i];

            p += gui_uint_to_str(ebs_xdev, ebs_xhci_line + p);

            seg = " FUNC ";
            for (UINTN i = 0; seg[i] != '\0'; i++)
                ebs_xhci_line[p++] = seg[i];

            p += gui_uint_to_str(ebs_xfunc, ebs_xhci_line + p);

            seg = " MMIO=0x";
            for (UINTN i = 0; seg[i] != '\0'; i++)
                ebs_xhci_line[p++] = seg[i];

            p += gui_hex_to_str(ebs_xmmio, 16, ebs_xhci_line + p);

            ebs_xhci_line[p] = '\0';

        } else {

            gui_str_copy8(
                ebs_xhci_line,
                "XHCI: NOT FOUND ON PCI BUS.",
                sizeof(ebs_xhci_line)
            );
        }

        UINT32 ebs_bg  = gui_pack(ebs_fmt, 10, 10, 30);
        UINT32 ebs_fg  = gui_pack(ebs_fmt, 80, 220, 120);
        UINT32 ebs_hl  = gui_pack(ebs_fmt, 255, 210, 90);
        UINT32 ebs_box = gui_pack(ebs_fmt, 220, 160, 40);

        gui_fill_rect(
            ebs_fb, ebs_stride, ebs_fb_w, ebs_fb_h,
            0, 0, ebs_fb_w, ebs_fb_h, ebs_bg
        );

        gui_draw_text(
            ebs_fb, ebs_stride, ebs_fb_w, ebs_fb_h,
            30, 30, 3, ebs_fg,
            "EXITBOOTSERVICES: OK"
        );

        gui_draw_text(
            ebs_fb, ebs_stride, ebs_fb_w, ebs_fb_h,
            30, 70, 2, ebs_fg,
            "NO CONIN / CONOUT / STALL FROM HERE ON."
        );

        gui_draw_text(
            ebs_fb, ebs_stride, ebs_fb_w, ebs_fb_h,
            30, 95, 2, ebs_fg,
            "THIS TEXT IS DRAWN WITH OUR OWN PIXEL FONT."
        );

        gui_draw_text(
            ebs_fb, ebs_stride, ebs_fb_w, ebs_fb_h,
            30, 148, 2, ebs_hl,
            ebs_xhci_line
        );

        /* Короткая бегающая коробочка - быстрое живое
           доказательство, что кадры рисуются сами, без Stall
           (свой busy-wait) и вообще без какой-либо прошивки,
           перед тем как перейти к самому интересному - запуску
           xHCI-драйвера. */

        INTN ebs_bx = 30;
        INTN ebs_by = 180;
        INTN ebs_dx = 3;
        INTN ebs_dy = 2;

        for (UINTN ebs_frame = 0; ebs_frame < EBS_STEPS / 4; ebs_frame++) {

            gui_fill_rect(
                ebs_fb, ebs_stride, ebs_fb_w, ebs_fb_h,
                ebs_bx, ebs_by, EBS_BOX_SIZE, EBS_BOX_SIZE, ebs_bg
            );

            ebs_bx += ebs_dx;
            ebs_by += ebs_dy;

            if (ebs_bx < 0 || ebs_bx > (INTN)ebs_fb_w - EBS_BOX_SIZE)
                ebs_dx = -ebs_dx;

            if (
                ebs_by < 180 ||
                ebs_by > (INTN)ebs_fb_h - EBS_BOX_SIZE
            )
                ebs_dy = -ebs_dy;

            gui_fill_rect(
                ebs_fb, ebs_stride, ebs_fb_w, ebs_fb_h,
                ebs_bx, ebs_by, EBS_BOX_SIZE, EBS_BOX_SIZE, ebs_box
            );

            /* Собственный busy-wait вместо BootServices->Stall -
               обычный volatile-счётчик, чтобы компилятор не
               выкинул пустой цикл. Грубо и без калибровки под
               частоту CPU - точный таймер (PIT/HPET/TSC) это уже
               отдельный, следующий шаг. */
            for (
                volatile UINT64 ebs_spin = 0;
                ebs_spin < EBS_SPIN_PER_STEP;
                ebs_spin++
            ) { }
        }

        /*
         * Дальше - сам xHCI-драйвер, целиком после
         * ExitBootServices, без единого обращения к прошивке.
         * Он печатает через "поддельный" ConOut
         * (ebs_console_start/g_ebs_pixel_out, см. выше) - тот же
         * самый print()/print_uint()/print_hex(), что и раньше,
         * только вместо st->ConOut->OutputString рисует символы
         * прямо в framebuffer нашим пиксельным шрифтом.
         */

        if (ebs_xhci_ready) {

            ebs_console_start(
                ebs_fb, ebs_stride, ebs_fb_w, ebs_fb_h,
                ebs_fg, ebs_bg
            );

            xhci_run_post_ebs(
                &g_ebs_pixel_out, ebs_xmmio, &ebs_cap,
                ebs_dcbaa_phys, ebs_cmdring_phys,
                ebs_evring_phys, ebs_erst_phys,
                ebs_input_ctx_phys, ebs_dev_ctx_phys,
                ebs_ep0_ring_phys, ebs_desc_buf_phys,
                ebs_int_ring_phys
            );

            print(&g_ebs_pixel_out, "\nRebooting shortly...\n");

            busy_wait_ms(3000);
        }

        /* Обычный return сюда не годится: клавиатуры для шелла
           всё равно больше нет. RuntimeServices, в отличие от
           BootServices, обязаны работать и после выхода - поэтому
           ResetSystem ниже безопасен и является штатным финалом
           демо, а не костылём. */
        st->RuntimeServices->ResetSystem(
            EfiResetCold, EFI_SUCCESS, 0, NULL
        );

        /* На случай, если прошивка почему-то не перезагрузила
           сразу же - зависаем тут, а не проваливаемся в код,
           который ждёт клавиатурный ввод, которого уже нет. */
        for (;;) { }


    /* --------------------------------------------------------
     * reboot
     * -------------------------------------------------------- */

    } else if (streq(line, "reboot")) {

        print(
            out,
            "Rebooting...\n"
        );


        st->RuntimeServices->ResetSystem(
            EfiResetCold,
            EFI_SUCCESS,
            0,
            NULL
        );


    /* --------------------------------------------------------
     * shutdown
     * -------------------------------------------------------- */

    } else if (
        streq(line, "shutdown") ||
        streq(line, "exit")
    ) {

        print(
            out,
            "Shutting down...\n"
        );


        st->RuntimeServices->ResetSystem(
            EfiResetShutdown,
            EFI_SUCCESS,
            0,
            NULL
        );


    /* --------------------------------------------------------
     * unknown command
     * -------------------------------------------------------- */

    } else {

        print(
            out,
            "Unknown command: "
        );


        /*
         * Используем print16(), чтобы этот вывод
         * тоже попал в scrollback.
         */
        print16(
            out,
            line
        );


        print(
            out,
            "\n(type 'help')\n"
        );
    }
}
