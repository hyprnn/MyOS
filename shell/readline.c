/*
 * shell/readline.c - ввод строки шелла, история стрелками, PageUp/PageDown.
 * Часть MyOS; общие объявления - в myos.h.
 */
#include "myos.h"


/* ============================================================
 * Input
 * ============================================================ */

void erase_input_line(
    SIMPLE_TEXT_OUTPUT_INTERFACE *out,
    UINTN len
)
{
    for (UINTN i = 0; i < len; i++)
        print(out, "\b \b");
}


/*
 * Чтение строки.
 *
 * PageUp    = scrollback вверх
 * PageDown  = scrollback вниз
 * Home      = самый верх истории
 * End       = самый низ
 *
 * Up/Down   = command history
 */
void read_line(
    EFI_SYSTEM_TABLE *st,
    CHAR16 *line,
    UINTN max
)
{
    SIMPLE_TEXT_OUTPUT_INTERFACE *out =
        st->ConOut;

    SIMPLE_INPUT_INTERFACE *in =
        st->ConIn;

    UINTN len = 0;

    int history_pos = -1;

    line[0] = 0;


    for (;;) {

        EFI_INPUT_KEY key;

        EFI_STATUS status =
            in->ReadKeyStroke(
                in,
                &key
            );


        if (status != EFI_SUCCESS) {

            st->BootServices->Stall(
                10000
            );

            continue;
        }


        /* ====================================================
         * ENTER
         * ==================================================== */

        if (key.UnicodeChar ==
            CHAR_CARRIAGE_RETURN) {

            line[len] = 0;

            /* эхо набранных символов идёт мимо print() (прямо в
               OutputString), поэтому в лог COM1 саму введённую
               команду отправляем отдельно */
            serial_puts16(line);

            print(out, "\n");

            return;
        }


        /* ====================================================
         * BACKSPACE
         * ==================================================== */

        if (key.UnicodeChar ==
            CHAR_BACKSPACE) {

            if (len > 0) {

                len--;

                line[len] = 0;

                print(out, "\b \b");
            }

            continue;
        }


        /* ====================================================
         * SPECIAL KEYS
         * ==================================================== */

        if (key.UnicodeChar == 0) {


            /* =================================================
             * PAGE UP
             * ScanCode 0x09
             * ================================================= */

            if (key.ScanCode == 0x09) {

                int step =
                    SCROLLBACK_VISIBLE_ROWS - 1;

                if (step < 1)
                    step = 1;

                int new_view =
                    g_scrollback_view + step;

                scrollback_render(
                    st,
                    new_view
                );

                redraw_input(
                    out,
                    line
                );

                continue;
            }


            /* =================================================
             * PAGE DOWN
             * ScanCode 0x0A
             * ================================================= */

            if (key.ScanCode == 0x0A) {

                int step =
                    SCROLLBACK_VISIBLE_ROWS - 1;

                if (step < 1)
                    step = 1;

                int new_view =
                    g_scrollback_view - step;

                if (new_view < 0)
                    new_view = 0;

                scrollback_render(
                    st,
                    new_view
                );

                redraw_input(
                    out,
                    line
                );

                continue;
            }


            /* =================================================
             * HOME
             * ScanCode 0x05
             * ================================================= */

            if (key.ScanCode == 0x05) {

                scrollback_render(
                    st,
                    999999
                );

                redraw_input(
                    out,
                    line
                );

                continue;
            }


            /* =================================================
             * END
             * ScanCode 0x06
             * ================================================= */

            if (key.ScanCode == 0x06) {

                scrollback_render(
                    st,
                    0
                );

                redraw_input(
                    out,
                    line
                );

                continue;
            }


            /* =================================================
             * UP — command history
             * ScanCode 1
             * ================================================= */

            if (key.ScanCode == 1) {

                if (g_history_count > 0) {

                    if (history_pos == -1) {

                        history_pos =
                            (int)g_history_count - 1;

                    } else if (history_pos > 0) {

                        history_pos--;
                    }


                    /*
                     * Стираем текущий ввод.
                     */
                    for (UINTN i = 0;
                         i < len;
                         i++) {

                        print(
                            out,
                            "\b \b"
                        );
                    }


                    UINTN index;


                    if (g_history_count <=
                        HIST_MAX) {

                        index =
                            (UINTN)history_pos;

                    } else {

                        UINTN start =
                            g_history_count %
                            HIST_MAX;

                        index =
                            (start +
                             (UINTN)history_pos)
                            % HIST_MAX;
                    }


                    char16_copy(
                        line,
                        g_history[index],
                        max
                    );

                    len =
                        char16_len(line);


                    print16(
                        out,
                        line
                    );
                }

                continue;
            }


            /* =================================================
             * DOWN — command history
             * ScanCode 2
             * ================================================= */

            if (key.ScanCode == 2) {

                if (history_pos != -1) {

                    for (UINTN i = 0;
                         i < len;
                         i++) {

                        print(
                            out,
                            "\b \b"
                        );
                    }


                    history_pos++;


                    if (history_pos >=
                            (int)g_history_count ||
                        history_pos >=
                            (int)HIST_MAX) {

                        history_pos = -1;

                        len = 0;

                        line[0] = 0;

                    } else {

                        UINTN index;


                        if (g_history_count <=
                            HIST_MAX) {

                            index =
                                (UINTN)history_pos;

                        } else {

                            UINTN start =
                                g_history_count %
                                HIST_MAX;

                            index =
                                (start +
                                 (UINTN)history_pos)
                                % HIST_MAX;
                        }


                        char16_copy(
                            line,
                            g_history[index],
                            max
                        );

                        len =
                            char16_len(line);


                        print16(
                            out,
                            line
                        );
                    }
                }

                continue;
            }


            continue;
        }


        /* ====================================================
         * NORMAL CHARACTER
         * ==================================================== */

        if (len < max - 1) {

            line[len++] =
                key.UnicodeChar;

            line[len] = 0;


            CHAR16 echo[2] = {
                key.UnicodeChar,
                0
            };


            out->OutputString(
                out,
                echo
            );


            history_pos = -1;
        }
    }
}


/* ============================================================
 * RAM filesystem
 * ============================================================ */

int fs_find(
    const CHAR16 *name
)
{
    for (int i = 0;
         i < FS_MAX_FILES;
         i++) {

        if (g_fs[i].used &&
            char16_eq(
                g_fs[i].name,
                name
            ))
            return i;
    }

    return -1;
}
