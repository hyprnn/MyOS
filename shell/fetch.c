/*
 * shell/fetch.c - fetch (как neofetch).
 * Часть MyOS; общие объявления - в myos.h.
 */
#include "myos.h"


/* ============================================================
 * Fetch helpers
 * ============================================================ */

void print_label(
    SIMPLE_TEXT_OUTPUT_INTERFACE *out,
    const char *label
)
{
    set_color(
        out,
        0x0B
    );

    print(
        out,
        label
    );

    set_color(
        out,
        0x0F
    );
}


void print_separator(
    SIMPLE_TEXT_OUTPUT_INTERFACE *out
)
{
    set_color(
        out,
        0x08
    );

    print(
        out,
        "----------------------------------------\n"
    );

    set_color(
        out,
        0x0F
    );
}


void print_bool(
    SIMPLE_TEXT_OUTPUT_INTERFACE *out,
    BOOLEAN value
)
{
    set_color(
        out,
        value ? 0x0A : 0x0C
    );

    print(
        out,
        value ? "Yes" : "No"
    );

    set_color(
        out,
        0x0F
    );
}


/* ============================================================
 * Fetch / neofetch
 * ============================================================ */

void cmd_fetch(
    EFI_SYSTEM_TABLE *st
)
{
    SIMPLE_TEXT_OUTPUT_INTERFACE *out =
        st->ConOut;

    UINTN saved =
        g_color;


    /* --------------------------------------------------------
     * ASCII logo
     * -------------------------------------------------------- */

    const char *logo[] = {

        " /$$      /$$            /$$$$$$   /$$$$$$ ",
        "| $$$    /$$$           /$$__  $$ /$$__  $$",
        "| $$$$  /$$$$ /$$   /$$| $$  \\ $$| $$  \\__/",
        "| $$ $$/$$ $$| $$  | $$| $$  | $$|  $$$$$$ ",
        "| $$  $$$| $$| $$  | $$| $$  | $$ \\____  $$",
        "| $$\\  $ | $$| $$  | $$| $$  | $$ /$$  \\ $$",
        "| $$ \\/  | $$|  $$$$$$$|  $$$$$$/|  $$$$$$/",
        "|__/     |__/ \\____  $$ \\______/  \\______/ ",
        "              /$$  | $$                    ",
        "             |  $$$$$$/                    ",
        "              \\______/                     "
    };


    UINTN logo_colors[] = {

        0x0B,
        0x0B,
        0x0B,

        0x03,
        0x03,
        0x03,

        0x01,
        0x01,
        0x01,

        0x09,
        0x09
    };


    for (UINTN i = 0;
         i < 11;
         i++) {

        set_color(
            out,
            logo_colors[i]
        );

        print(
            out,
            logo[i]
        );

        print(
            out,
            "\n"
        );
    }


    set_color(
        out,
        0x0F
    );

    print(
        out,
        "\n"
    );


    /* --------------------------------------------------------
     * Header
     * -------------------------------------------------------- */

    set_color(
        out,
        0x0B
    );

    print(
        out,
        "MyOS"
    );

    set_color(
        out,
        0x08
    );

    print(
        out,
        " @ "
    );

    set_color(
        out,
        0x0F
    );

    print(
        out,
        "UEFI bare-metal environment\n"
    );


    print_separator(out);


    /* --------------------------------------------------------
     * Operating system
     * -------------------------------------------------------- */

    print_label(
        out,
        "OS"
    );

    print(
        out,
        "        : MyOS 0.1"
    );

    print(
        out,
        " (x86_64, bare-metal UEFI)\n"
    );


    print_label(
        out,
        "Kernel"
    );

    print(
        out,
        "    : MyOS kernel.elf"
    );

    print(
        out,
        " (higher half, own page tables)\n"
    );


    print_label(
        out,
        "Shell"
    );

    print(
        out,
        "     : myos-shell"
    );

    print(
        out,
        " (built-in command loop)\n"
    );


    print_label(
        out,
        "Architecture"
    );

    print(
        out,
        " : x86_64\n"
    );


    print_label(
        out,
        "CPU cores"
    );

    if (g_acpi.have_madt)
        kprintf(out, "    : %u (from ACPI), MyOS uses 1 for now\n",
                (UINT32)g_acpi.ncpus_enabled);
    else
        print(out, "    : unknown (no ACPI MADT)\n");


    print_label(
        out,
        "Boot Mode"
    );

    print(
        out,
        "    : UEFI -> MyOS loader -> kernel\n"
    );


    /* --------------------------------------------------------
     * Firmware
     * -------------------------------------------------------- */

    print_separator(out);


    print_label(
        out,
        "Firmware"
    );

    print(
        out,
        "  : "
    );


    if (st->FirmwareVendor)
        print16(
            out,
            st->FirmwareVendor
        );
    else
        print(
            out,
            "Unknown"
        );


    print(
        out,
        " rev "
    );


    print_uint(
        out,
        st->FirmwareRevision
    );


    print(
        out,
        "\n"
    );


    print_label(
        out,
        "UEFI"
    );

    print(
        out,
        "       : "
    );


    print_uint(
        out,
        (st->Hdr.Revision >> 16) &
        0xFFFF
    );


    print(
        out,
        "."
    );


    print_uint(
        out,
        st->Hdr.Revision &
        0xFFFF
    );


    print(
        out,
        "\n"
    );


    /* --------------------------------------------------------
     * Secure Boot
     * -------------------------------------------------------- */

    UINT8 secure_boot = 0;

    UINTN secure_boot_size =
        sizeof(secure_boot);


    EFI_GUID global_variable =
    {
        0x8BE4DF61,
        0x93CA,
        0x11D2,
        {
            0xAA,
            0x0D,
            0x00,
            0xE0,
            0x98,
            0x03,
            0x2B,
            0x8C
        }
    };


    EFI_STATUS sb_status =
        st->RuntimeServices->GetVariable(
            L"SecureBoot",
            &global_variable,
            NULL,
            &secure_boot_size,
            &secure_boot
        );


    print_label(
        out,
        "Secure Boot"
    );

    print(
        out,
        " : "
    );


    if (sb_status == EFI_SUCCESS) {

        print_bool(
            out,
            secure_boot != 0
        );

    } else {

        set_color(
            out,
            0x08
        );

        print(
            out,
            "Unknown"
        );

        set_color(
            out,
            0x0F
        );
    }


    print(
        out,
        "\n"
    );


    /* --------------------------------------------------------
     * Boot time / uptime
     * -------------------------------------------------------- */

    if (g_have_boot_time) {

        EFI_TIME now;


        if (st->RuntimeServices->GetTime &&
            st->RuntimeServices->GetTime(
                &now,
                NULL
            ) == EFI_SUCCESS) {


            INT64 now_secs =
                (INT64)now.Hour * 3600 +
                (INT64)now.Minute * 60 +
                now.Second;


            INT64 boot_secs =
                (INT64)g_boot_time.Hour * 3600 +
                (INT64)g_boot_time.Minute * 60 +
                g_boot_time.Second;


            /* время работы - по счётчику ядра (TSC), а не разностью
               показаний часов: так оно верно и после суток работы, и
               после смены часового пояса */
            INT64 secs = (INT64)(kx_uptime_us() / 1000000u);

            (void)now_secs;
            (void)boot_secs;


            print_label(
                out,
                "Uptime"
            );

            print(
                out,
                "      : "
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
     * Network (этап 8)
     * -------------------------------------------------------- */

    {
        char net[48];

        net_status_line(net, sizeof(net));
        print_label(out, "Network");
        kprintf(out, "     : %s\n", net);
    }


    /* --------------------------------------------------------
     * Current date/time
     * -------------------------------------------------------- */

    {
        EFI_TIME now;


        if (st->RuntimeServices->GetTime &&
            st->RuntimeServices->GetTime(
                &now,
                NULL
            ) == EFI_SUCCESS) {


            print_label(
                out,
                "Date"
            );

            print(
                out,
                "        : "
            );


            print_uint(
                out,
                now.Day
            );

            print(
                out,
                "."
            );


            print_uint(
                out,
                now.Month
            );

            print(
                out,
                "."
            );


            print_uint(
                out,
                now.Year
            );


            print(
                out,
                " "
            );


            if (now.Hour < 10)
                print(
                    out,
                    "0"
                );


            print_uint(
                out,
                now.Hour
            );


            print(
                out,
                ":"
            );


            if (now.Minute < 10)
                print(
                    out,
                    "0"
                );


            print_uint(
                out,
                now.Minute
            );


            print(
                out,
                ":"
            );


            if (now.Second < 10)
                print(
                    out,
                    "0"
                );


            print_uint(
                out,
                now.Second
            );


            print(
                out,
                "\n"
            );
        }
    }


    /* --------------------------------------------------------
     * GOP
     * -------------------------------------------------------- */

    EFI_GRAPHICS_OUTPUT_PROTOCOL *gop =
        NULL;


    EFI_GUID gop_guid =
        EFI_GRAPHICS_OUTPUT_PROTOCOL_GUID;


    EFI_STATUS gop_status =
        st->BootServices->LocateProtocol(
            &gop_guid,
            NULL,
            (void **)&gop
        );


    if (gop_status == EFI_SUCCESS &&
        gop != NULL) {


        print_separator(out);


        print_label(
            out,
            "GPU"
        );

        print(
            out,
            "        : EFI Graphics Output Protocol\n"
        );


        print_label(
            out,
            "Resolution"
        );

        print(
            out,
            " : "
        );


        if (gop->Mode &&
            gop->Mode->Info) {

            print_uint(
                out,
                gop->Mode->Info->
                    HorizontalResolution
            );


            print(
                out,
                "x"
            );


            print_uint(
                out,
                gop->Mode->Info->
                    VerticalResolution
            );


            print(
                out,
                "\n"
            );
        }


        print_label(
            out,
            "Pixel Format"
        );

        print(
            out,
            " : "
        );


        if (gop->Mode &&
            gop->Mode->Info) {


            switch (
                gop->Mode->Info->PixelFormat
            ) {

                case PixelRedGreenBlueReserved8BitPerColor:

                    print(
                        out,
                        "RGB"
                    );

                    break;


                case PixelBlueGreenRedReserved8BitPerColor:

                    print(
                        out,
                        "BGR"
                    );

                    break;


                case PixelBitMask:

                    print(
                        out,
                        "BitMask"
                    );

                    break;


                case PixelBltOnly:

                    print(
                        out,
                        "BltOnly"
                    );

                    break;


                default:

                    print(
                        out,
                        "Unknown"
                    );

                    break;
            }


            print(
                out,
                "\n"
            );
        }
    }


    /* --------------------------------------------------------
     * Console
     * -------------------------------------------------------- */

    print_separator(out);


    print_label(
        out,
        "Terminal"
    );

    print(
        out,
        g_kernel_mode ? "    : MyOS framebuffer console (8x16 font)\n"
                      : "    : EFI_SIMPLE_TEXT_OUTPUT\n"
    );


    print_label(
        out,
        "Input"
    );

    print(
        out,
        g_kernel_mode ? "       : MyOS USB HID + PS/2 drivers\n"
                      : "       : EFI_SIMPLE_TEXT_INPUT\n"
    );


    print_label(
        out,
        "Text Mode"
    );

    print(
        out,
        "   : "
    );


    if (out->Mode) {

        print_uint(
            out,
            out->Mode->Mode
        );


        print(
            out,
            " / "
        );


        if (out->Mode->MaxMode > 0)
            print_uint(
                out,
                out->Mode->MaxMode - 1
            );
        else
            print_uint(
                out,
                0
            );
    }


    print(
        out,
        "\n"
    );


    /* --------------------------------------------------------
     * Прошивка: после загрузчика её нет вообще - всё своё
     * -------------------------------------------------------- */

    print_label(
        out,
        "Firmware use"
    );

    print(
        out,
        " : none after the loader (own drivers)\n"
    );


    /* --------------------------------------------------------
     * Memory
     * -------------------------------------------------------- */

    print_separator(out);


    print_label(
        out,
        "Memory"
    );

    print(
        out,
        "      : "
    );

    kprintf(
        out,
        "%llu MiB free of %llu MiB (see 'mem')\n",
        (g_kmm_free_pages * 4u) / 1024u,
        (g_kmm_usable_pages * 4u) / 1024u
    );


    print_label(
        out,
        "Allocator"
    );

    print(
        out,
        "   : MyOS page bitmap + kmalloc slabs\n"
    );


    /* --------------------------------------------------------
     * Palette
     *
     * Используем ### вместо "███".
     *
     * Причина:
     * print() работает с char* и не является UTF-8
     * декодером. Символ █ в UTF-8 занимает несколько
     * байтов и раньше мог превращаться в мусор.
     * -------------------------------------------------------- */

    print(
        out,
        "\n"
    );


    print_label(
        out,
        "Colors"
    );

    print(
        out,
        "      : "
    );


    for (UINTN i = 0;
         i < 8;
         i++) {

        set_color(
            out,
            i
        );

        print(
            out,
            "###"
        );
    }


    set_color(
        out,
        0x0F
    );


    print(
        out,
        "\n"
    );


    print_label(
        out,
        "Bright"
    );

    print(
        out,
        "      : "
    );


    for (UINTN i = 8;
         i < 16;
         i++) {

        set_color(
            out,
            i
        );

        print(
            out,
            "###"
        );
    }


    set_color(
        out,
        saved
    );


    print(
        out,
        "\n"
    );


    print_separator(out);


    /* --------------------------------------------------------
     * Footer
     * -------------------------------------------------------- */

    set_color(
        out,
        0x08
    );


    print(
        out,
        "MyOS 0.1 | x86_64 | UEFI | bare-metal"
    );


    set_color(
        out,
        0x0F
    );


    print(
        out,
        "\n\n"
    );
}


/* ============================================================
 * Command history
 * ============================================================ */

void push_history(
    CHAR16 *line
)
{
    if (char16_len(line) == 0)
        return;


    char16_copy(
        g_history[
            g_history_count % HIST_MAX
        ],
        line,
        LINE_MAX
    );


    g_history_count++;
}
