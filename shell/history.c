/*
 * shell/history.c - история команд.
 * Часть MyOS; общие объявления - в myos.h.
 */
#include "myos.h"

CHAR16 g_history[HIST_MAX][LINE_MAX];
UINTN  g_history_count = 0;


void cmd_history(
    EFI_SYSTEM_TABLE *st
)
{
    SIMPLE_TEXT_OUTPUT_INTERFACE *out =
        st->ConOut;


    if (g_history_count == 0) {

        print(
            out,
            "(no history yet)\n"
        );

        return;
    }


    UINTN shown =
        (g_history_count < HIST_MAX)
        ? g_history_count
        : HIST_MAX;


    UINTN start =
        (g_history_count < HIST_MAX)
        ? 0
        : (g_history_count % HIST_MAX);


    for (UINTN i = 0;
         i < shown;
         i++) {


        UINTN idx =
            (start + i) % HIST_MAX;


        print_uint(
            out,
            g_history_count -
            shown +
            i +
            1
        );


        print(
            out,
            "  "
        );


        print16(
            out,
            g_history[idx]
        );


        print(
            out,
            "\n"
        );
    }
}
