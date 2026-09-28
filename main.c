/*
 * main.c - точка входа efi_main и главный цикл шелла.
 * Часть MyOS; общие объявления - в myos.h.
 */
#include "myos.h"

/*
 * Хэндл образа, полученный efi_main(). Нужен позже для
 * ExitBootServices(ImageHandle, MapKey) - без него прошивка
 * не сможет проверить, что выход из Boot Services запрашивает
 * именно наше приложение, поэтому храним его глобально, а не
 * просто отбрасываем, как раньше.
 */
EFI_HANDLE g_image_handle = NULL;


/* ============================================================
 * UEFI entry point
 * ============================================================ */

EFI_STATUS EFIAPI efi_main(
    EFI_HANDLE ImageHandle,
    EFI_SYSTEM_TABLE *SystemTable
)
{
    g_image_handle = ImageHandle;

    /* Таблица, через которую шелл получает ввод/вывод. После
       команды "ebs" она подменяется на нашу собственную (g_kst),
       поэтому главный цикл ниже берёт её заново на каждой
       итерации, а не держит SystemTable/out из начала функции -
       настоящий ConOut прошивки после ExitBootServices равен
       NULL. */
    g_st = SystemTable;


    SIMPLE_TEXT_OUTPUT_INTERFACE *out =
        SystemTable->ConOut;


    out->Reset(
        out,
        FALSE
    );


    out->ClearScreen(out);


    set_color(
        out,
        g_color
    );


    /* --------------------------------------------------------
     * Save boot time
     * -------------------------------------------------------- */

    if (
        SystemTable->RuntimeServices->GetTime &&
        SystemTable->RuntimeServices->GetTime(
            &g_boot_time,
            NULL
        ) == EFI_SUCCESS
    ) {

        g_have_boot_time = TRUE;
    }


    /* --------------------------------------------------------
     * Startup banner
     * -------------------------------------------------------- */

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
        "================================\n\n"
    );


    print(
        out,
        "Boot services: OK\n"
    );

    print(
        out,
        "Text output:   OK\n\n"
    );


    print(
        out,
        "Type 'help' for the list of commands, or 'fetch' for a system summary.\n\n"
    );


    /* --------------------------------------------------------
     * Main shell loop
     * -------------------------------------------------------- */

    CHAR16 line[LINE_MAX];


    for (;;) {

        out = g_st->ConOut;

        set_color(
            out,
            g_color
        );


        print(
            out,
            "> "
        );


        read_line(
            g_st,
            line,
            LINE_MAX
        );


        run_command(
            g_st,
            line
        );
    }


    return EFI_SUCCESS;
}
