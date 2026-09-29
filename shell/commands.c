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

    } else if (streq(line, "cpu")) {

        kernel_cmd_cpu(out);

    } else if (streq(line, "ps")) {

        kernel_cmd_ps(out);

    } else if (streq(line, "threadtest")) {

        kernel_cmd_threadtest(out);

    } else if (starts_with(line, "spin ")) {

        /* Занять процессор на N секунд, НЕ отдавая его: ни сна, ни
           ожидания клавиш. Раньше это "вешало" машину целиком; с
           вытеснением поток usb по-прежнему подключает устройства,
           а поток idle честно получает 0%. */
        UINTN secs = parse_uint(line + 5);
        UINT64 end = rdtsc() + g_tsc_hz * (UINT64)secs;
        UINT64 loops = 0;
        UINT64 sw0 = g_sched_switches;

        kprintf(out, "Spinning for %u s without giving the CPU away...\n", (UINT32)secs);
        kcon_flush();

        while (rdtsc() < end)
            loops++;

        kprintf(out, "Done: %llu million loops; meanwhile the scheduler switched "
                     "threads %llu times.\n",
                loops / 1000000u, g_sched_switches - sw0);

    } else if (streq(line, "disk") || starts_with(line, "disk ")) {

        char arg[32];
        UINTN k = 0;

        if (line[4] == ' ')
            for (CHAR16 *c = line + 5; *c && k + 1 < sizeof(arg); c++)
                arg[k++] = (*c < 128) ? (char)*c : '?';

        arg[k] = '\0';
        kernel_cmd_disk(out, arg);

    } else if (streq(line, "acpi")) {

        kernel_cmd_acpi(out);

    /* сеть (этап 8): состояние стека; Wi-Fi - что за адаптер */
    } else if (streq(line, "net") || streq(line, "netstat")) {

        kernel_cmd_net(out, "");

    } else if (starts_with(line, "net ")) {

        char arg[32];
        UINTN k = 0;

        for (CHAR16 *c = line + 4; *c && k + 1 < sizeof(arg); c++)
            arg[k++] = (*c < 128) ? (char)*c : '?';

        arg[k] = '\0';
        kernel_cmd_net(out, arg);

    } else if (streq(line, "wifi") || starts_with(line, "wifi ")) {

        char arg[48];
        UINTN k = 0;

        if (line[4] == ' ')
            for (CHAR16 *c = line + 5; *c && k + 1 < sizeof(arg); c++)
                arg[k++] = (*c < 128) ? (char)*c : '?';

        arg[k] = '\0';
        kernel_cmd_wifi(out, arg);

    } else if (streq(line, "boot")) {

        kernel_cmd_boot(out);

    } else if (streq(line, "vm")) {

        kernel_cmd_vm(out);

    /* kpanic - сломать САМО ЯДРО (экран паники, машина стоп).
       "crash" теперь - программа /bin/crash: ломается только она */
    } else if (streq(line, "kpanic")) {

        kernel_cmd_crash(out, "");

    } else if (starts_with(line, "kpanic ")) {

        /* line - CHAR16; аргумент переводим в ASCII */
        char arg[16];
        UINTN k = 0;

        for (CHAR16 *c = line + 7; *c && k + 1 < sizeof(arg); c++)
            arg[k++] = (*c < 128) ? (char)*c : '?';

        arg[k] = '\0';
        kernel_cmd_crash(out, arg);

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
            "  time          - show current time (CMOS clock)\n"
        );

        print(
            out,
            "  date          - show current date (CMOS clock)\n"
            "  tz [msk|jer]  - time zone: Moscow or Jerusalem (GUI: click the clock)\n"
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
            "  --- programs (ring 3, files in /bin; also run <path>) ---\n"
            "  hello         - greeting from user mode\n"
            "  calc [expr]   - calculator: calc 2*(3+4)\n"
            "  edit <file>   - type text into a file, end with '.'\n"
            "  guess         - guess-the-number game\n"
            "  primes [N]    - count prime numbers (a long job)\n"
            "  crash [how]   - a program that breaks on purpose (null, write,\n"
            "                  kernel, cli, div, stack, loop); Ctrl+C stops a program\n"
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
            "  acpi          - ACPI tables: CPU cores, I/O APIC, HPET, PCIe, power\n"
            "  cpu           - CPU load and interrupt counters\n"
            "  ps            - threads: state, CPU share, stack, what they wait for\n"
            "  threadtest    - live test: preemption, fair sharing, mutex\n"
            "  spin <sec>    - keep the CPU 100% busy (other threads still run)\n"
            "  disk [read N] - USB flash drives: list, show a sector\n"
        );

        print(
            out,
            "  boot          - what the loader did: kernel file, firmware, log\n"
        );

        print(
            out,
            "  vm            - virtual memory: page tables, stacks, heap\n"
        );

        print(
            out,
            "  kinfo         - kernel status: timer, interrupts, input\n"
        );

        print(
            out,
            "  usb           - USB devices (tree with hubs), plug/unplug log\n"
        );

        print(
            out,
            "  mousetest     - live USB mouse/keyboard diagnostics\n"
        );

        print(
            out,
            "  mem           - memory map and page allocator status\n"
        );

        print(
            out,
            "  int3          - test our interrupt table\n"
        );

        print(
            out,
            "  kpanic [null|write|stack] - break the KERNEL itself on purpose\n"
            "                  -> panic screen (the machine halts after it)\n"
        );

        print(
            out,
            "  lspci         - scan PCI bus via raw ports, find USB controllers\n"
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
            "  --- files and folders (/ram - RAM disk, /usb0p1 - flash drive...) ---\n"
            "  ls [path]     - list a folder ('ls /' - all volumes)\n"
            "  cd <path>     - go to a folder (cd /usb0p1, cd .., cd /)\n"
            "  pwd           - where am I\n"
            "  mkdir <name>  - new folder;  rmdir <name> - delete an empty one\n"
            "  touch <name>  - create an empty file\n"
            "  cat <name>    - print a file\n"
            "  write <n> <t> - write a line of text into a file (replaces it)\n"
            "  append <n> <t>- add a line of text to the end\n"
            "  rm <name>     - delete a file\n"
            "  mv <a> <b>    - rename / move;  cp <a> <b> - copy (also between disks)\n"
            "  size <name>   - file size;  df - volumes, size and free space\n"
            "  disk          - disks and partitions;  disk read N [name] - raw sector\n"
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
     * файлы и папки: ls, cd, pwd, mkdir, rmdir, touch, cat, write,
     * append, edit, rm, mv, cp, size, df - см. shell/fs.c (через
     * VFS: RAM-диск /ram, флешки, разделы дисков)
     * -------------------------------------------------------- */

    } else if (fs_shell_command(st, line)) {

        /* уже выполнено */

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

            /* какой пояс - чтобы было видно, чьё это время */
            char tzd[64];
            tz_describe(tzd, sizeof(tzd));
            kprintf(out, "  %s\n", tzd);

        } else {

            print(
                out,
                "Time service unavailable.\n"
            );
        }


    /* --------------------------------------------------------
     * tz - часовой пояс: Москва / Иерусалим (kernel/tz.c)
     * -------------------------------------------------------- */

    } else if (streq(line, "tz")) {

        kernel_cmd_tz(out, "");

    } else if (starts_with(line, "tz ")) {

        char arg[16];
        UINTN k = 0;

        for (CHAR16 *c = line + 3; *c && k + 1 < sizeof(arg); c++) {
            CHAR16 ch = *c;
            if (ch >= 'A' && ch <= 'Z')
                ch = (CHAR16)(ch - 'A' + 'a');
            arg[k++] = (ch < 128) ? (char)ch : '?';
        }

        arg[k] = '\0';
        kernel_cmd_tz(out, arg);


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

        /* Раньше считалось как "часы сейчас минус часы при
           загрузке" - ломалось после суток работы и теперь сбилось
           бы при смене часового пояса. Ядро само знает, сколько
           прошло с запуска (TSC), - берём это. */
        UINT64 secs = kx_uptime_us() / 1000000u;

        kprintf(out, "up %llud %lluh %llum %llus\n",
                secs / 86400u, (secs / 3600u) % 24u,
                (secs / 60u) % 60u, secs % 60u);


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
     * start (GUI)
     * -------------------------------------------------------- */

    } else if (streq(line, "start")) {

        wm_start(st);


    /* --------------------------------------------------------
     * ebs - раньше: выйти из прошивки. Теперь MyOS сразу
     * стартует своим ядром (загрузчик выходит из прошивки сам).
     * -------------------------------------------------------- */

    } else if (streq(line, "ebs")) {

        print(out, "Nothing to do: MyOS now boots straight into its own kernel.\n");
        print(out, "The loader (BOOTX64.EFI) leaves the firmware before the kernel\n");
        print(out, "starts - see 'boot' for what it did.\n");


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
     * программа из /bin (или путь к файлу программы) - этап 6:
     * она работает в ring 3 в своей памяти, шелл её ждёт
     * -------------------------------------------------------- */

    } else if (proc_shell_try(st, line)) {

        /* выполнено */

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
