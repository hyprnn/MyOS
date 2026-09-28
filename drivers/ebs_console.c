/*
 * drivers/ebs_console.c - пиксельная консоль старого демо ebsdemo.
 * Часть MyOS; общие объявления - в myos.h.
 */
#include "myos.h"

volatile UINT32 *g_ebsout_fb = NULL;
UINT32 g_ebsout_stride = 0;
UINT32 g_ebsout_w = 0;
UINT32 g_ebsout_h = 0;
UINT32 g_ebsout_fg = 0;
UINT32 g_ebsout_bg = 0;
INTN   g_ebsout_col = EBS_CONSOLE_MARGIN;
INTN   g_ebsout_row = EBS_CONSOLE_MARGIN;

EFI_STATUS EFIAPI ebs_console_output_string(
    SIMPLE_TEXT_OUTPUT_INTERFACE *this_out,
    CHAR16 *str
)
{
    /* this_out не используется - у нас на весь пиксельный
       "терминал" ровно один экземпляр, весь его реальный
       "объект" - глобальные переменные g_ebsout_* выше */
    (void)this_out;

    if (g_ebsout_fb == NULL || str == NULL)
        return EFI_SUCCESS;

    while (*str != 0) {

        CHAR16 c = *str;

        if (c == L'\r') {

            /* print() всегда шлёт '\r' и '\n' отдельной парой
               символов для перевода строки - сам перевод строки
               делает ветка '\n' ниже, здесь просто игнорируем */
            str++;
            continue;
        }

        if (c == L'\n') {

            g_ebsout_col = EBS_CONSOLE_MARGIN;
            g_ebsout_row += (INTN)EBS_CONSOLE_CHAR_H;

        } else {

            /* Наш пиксельный шрифт (gui_font) знает только ASCII
               (и то не весь набор - строчных букв там вообще
               нет). Символы вне таблицы gui_draw_char просто
               не рисует, но место под них всё равно резервирует
               - крашей не будет, часть текста молча превратится
               в пробелы. */
            /* Наш пиксельный шрифт (gui_font) знает только
               ASCII-цифры, ЗАГЛАВНЫЕ буквы и небольшой набор
               знаков препинания - строчных букв там нет вообще.
               Весь текст драйвера (print()/print_uint()/...)
               написан обычным регистром, поэтому строчные буквы
               без преобразования просто пропадали бы, оставляя
               дыры в тексте. Приводим к верхнему регистру перед
               отрисовкой - для латиницы это просто -32 к коду
               символа. Символы, которых в таблице всё равно нет
               (например большинство остальных знаков
               препинания), gui_draw_char по-прежнему molча
               пропускает - это ожидаемо и не крашится. */
            char ascii = (c < 128) ? (char)c : '?';

            if (ascii >= 'a' && ascii <= 'z')
                ascii = (char)(ascii - 'a' + 'A');

            gui_draw_char(
                g_ebsout_fb, g_ebsout_stride,
                g_ebsout_w, g_ebsout_h,
                g_ebsout_col, g_ebsout_row,
                EBS_CONSOLE_SCALE, g_ebsout_fg, ascii
            );

            g_ebsout_col += (INTN)EBS_CONSOLE_CHAR_W;

            if (
                g_ebsout_col + (INTN)EBS_CONSOLE_CHAR_W >
                (INTN)g_ebsout_w - EBS_CONSOLE_MARGIN
            ) {

                g_ebsout_col = EBS_CONSOLE_MARGIN;
                g_ebsout_row += (INTN)EBS_CONSOLE_CHAR_H;
            }
        }

        /* Дошли до низа экрана - настоящей прокрутки тут нет
           (framebuffer - просто массив пикселей в памяти,
           можно было бы сдвигать его memmove'ом, но это лишняя
           сложность ради истории, которая всё равно уже никому
           не нужна). Вместо этого просто очищаем экран и
           начинаем заново сверху - "листаем страницу". Важен
           только последний результат на экране, не вся история
           вывода. */
        if (
            g_ebsout_row + (INTN)EBS_CONSOLE_CHAR_H >
            (INTN)g_ebsout_h - EBS_CONSOLE_MARGIN
        ) {

            gui_fill_rect(
                g_ebsout_fb, g_ebsout_stride,
                g_ebsout_w, g_ebsout_h,
                0, 0, g_ebsout_w, g_ebsout_h,
                g_ebsout_bg
            );

            g_ebsout_col = EBS_CONSOLE_MARGIN;
            g_ebsout_row = EBS_CONSOLE_MARGIN;
        }

        str++;
    }

    return EFI_SUCCESS;
}

/* Сам "поддельный" объект ConOut - заполняется полями один раз
   в ebs_console_start() ниже. Только OutputString указывает на
   настоящую функцию, остальные указатели - NULL: весь код,
   который через этот out когда-либо проходит (print* семейство),
   их не вызывает. */
SIMPLE_TEXT_OUTPUT_INTERFACE g_ebs_pixel_out;

/*
 * Готовит framebuffer и сам "поддельный" ConOut к работе после
 * ExitBootServices: очищает экран заданным цветом фона и ставит
 * курсор пиксельного "терминала" в левый верхний угол.
 * Параметры fb/stride/w/h должны быть закэшированы ДО
 * ExitBootServices (см. команду "ebs") - после него
 * GraphicsOutputProtocol уже не найти через LocateProtocol,
 * а сам framebuffer как область памяти продолжает работать
 * как обычно.
 */
void ebs_console_start(
    volatile UINT32 *fb,
    UINT32 stride,
    UINT32 w,
    UINT32 h,
    UINT32 fg,
    UINT32 bg
)
{
    g_ebsout_fb = fb;
    g_ebsout_stride = stride;
    g_ebsout_w = w;
    g_ebsout_h = h;
    g_ebsout_fg = fg;
    g_ebsout_bg = bg;
    g_ebsout_col = EBS_CONSOLE_MARGIN;
    g_ebsout_row = EBS_CONSOLE_MARGIN;

    gui_fill_rect(fb, stride, w, h, 0, 0, w, h, bg);

    g_ebs_pixel_out.Reset = NULL;
    g_ebs_pixel_out.OutputString = ebs_console_output_string;
    g_ebs_pixel_out.TestString = NULL;
    g_ebs_pixel_out.QueryMode = NULL;
    g_ebs_pixel_out.SetMode = NULL;
    g_ebs_pixel_out.SetAttribute = NULL;
    g_ebs_pixel_out.ClearScreen = NULL;
    g_ebs_pixel_out.SetCursorPosition = NULL;
    g_ebs_pixel_out.EnableCursor = NULL;
    g_ebs_pixel_out.Mode = NULL;
}


/*
 * Небольшой набор встроенных команд для терминала
 * внутри GUI. Это отдельная, упрощённая реализация:
 * основной run_command() пишет прямо в
 * EFI_SIMPLE_TEXT_OUTPUT, а тут нужен вывод в свой
 * скроллбек-буфер окна. Шрифт GUI умеет только
 * заглавные буквы, поэтому весь ввод здесь тоже
 * в верхнем регистре.
 *
 * Возвращает TRUE, если команда должна закрыть окно
 * терминала (EXIT/CLOSE/QUIT), иначе FALSE.
 */
BOOLEAN gui_term_exec(
    EFI_SYSTEM_TABLE *st,
    const char *cmd,
    char lines[][GUI_TERM_LINE_LEN + 1],
    UINTN *count
)
{
    char prompt[GUI_TERM_LINE_LEN + 1];

    prompt[0] = '>';
    prompt[1] = ' ';

    UINTN i = 0;

    while (cmd[i] != '\0' && i < GUI_TERM_LINE_LEN - 2) {
        prompt[2 + i] = cmd[i];
        i++;
    }

    prompt[2 + i] = '\0';

    gui_term_push(lines, count, prompt);

    if (gui_streq(cmd, "")) {

        return FALSE;

    } else if (
        gui_streq(cmd, "EXIT") ||
        gui_streq(cmd, "CLOSE") ||
        gui_streq(cmd, "QUIT")
    ) {

        return TRUE;

    } else if (gui_streq(cmd, "HELP")) {

        gui_term_push(lines, count, "HELP ABOUT VER TIME DATE UPTIME");
        gui_term_push(lines, count, "WHOAMI CLEAR ECHO TEXT");
        gui_term_push(lines, count, "CALC A OP B");
        gui_term_push(lines, count, "LS TOUCH N CAT N SIZE N RM N");
        gui_term_push(lines, count, "WRITE N TEXT");
        gui_term_push(lines, count, "APPEND N TEXT");
        gui_term_push(lines, count, "MV A B CP A B");
        gui_term_push(lines, count, "REBOOT SHUTDOWN");
        gui_term_push(lines, count, "EXIT CLOSES THIS WINDOW");

    } else if (gui_streq(cmd, "ABOUT")) {

        gui_term_push(
            lines, count,
            "MYOS 0.1 - MINIMAL UEFI OS FROM SCRATCH."
        );

    } else if (gui_streq(cmd, "VER")) {

        gui_term_push(lines, count, "MYOS 0.1");

    } else if (gui_streq(cmd, "WHOAMI")) {

        gui_term_push(lines, count, "ROOT AT MYOS");

    } else if (
        gui_streq(cmd, "CLEAR") ||
        gui_streq(cmd, "CLS")
    ) {

        *count = 0;

    } else if (gui_streq(cmd, "TIME")) {

        EFI_TIME now;

        if (
            st->RuntimeServices->GetTime &&
            st->RuntimeServices->GetTime(
                &now, NULL
            ) == EFI_SUCCESS
        ) {

            char buf[9];

            gui_uint2_to_str(now.Hour, buf);
            buf[2] = ':';
            gui_uint2_to_str(now.Minute, buf + 3);
            buf[5] = ':';
            gui_uint2_to_str(now.Second, buf + 6);

            gui_term_push(lines, count, buf);

        } else {

            gui_term_push(lines, count, "TIME UNAVAILABLE");
        }

    } else if (gui_streq(cmd, "DATE")) {

        EFI_TIME now;

        if (
            st->RuntimeServices->GetTime &&
            st->RuntimeServices->GetTime(
                &now, NULL
            ) == EFI_SUCCESS
        ) {

            char buf[16];
            UINTN n = 0;

            n += gui_uint2_to_str(now.Day, buf + n);
            buf[n++] = '-';
            n += gui_uint2_to_str(now.Month, buf + n);
            buf[n++] = '-';
            n += gui_uint_to_str(now.Year, buf + n);

            gui_term_push(lines, count, buf);

        } else {

            gui_term_push(lines, count, "DATE UNAVAILABLE");
        }

    } else if (gui_streq(cmd, "UPTIME")) {

        if (
            !g_have_boot_time ||
            !st->RuntimeServices->GetTime
        ) {

            gui_term_push(lines, count, "UPTIME UNAVAILABLE");

        } else {

            EFI_TIME now;

            if (
                st->RuntimeServices->GetTime(
                    &now, NULL
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

                char buf[32];
                UINTN n = 0;

                buf[n++] = 'U';
                buf[n++] = 'P';
                buf[n++] = ' ';

                n += gui_uint_to_str(
                    (UINT64)secs / 3600, buf + n
                );
                buf[n++] = 'H';
                buf[n++] = ' ';

                n += gui_uint_to_str(
                    ((UINT64)secs / 60) % 60, buf + n
                );
                buf[n++] = 'M';
                buf[n++] = ' ';

                n += gui_uint_to_str(
                    (UINT64)secs % 60, buf + n
                );
                buf[n++] = 'S';
                buf[n] = '\0';

                gui_term_push(lines, count, buf);
            }
        }

    } else if (
        cmd[0] == 'E' && cmd[1] == 'C' &&
        cmd[2] == 'H' && cmd[3] == 'O' &&
        (cmd[4] == ' ' || cmd[4] == '\0')
    ) {

        gui_term_push(
            lines, count,
            cmd[4] == ' ' ? cmd + 5 : ""
        );

    } else if (
        gui_streq(cmd, "LS") ||
        gui_streq(cmd, "DIR") ||
        gui_streq(cmd, "FILES")
    ) {

        int any = 0;

        for (int fi = 0; fi < FS_MAX_FILES; fi++) {

            if (!g_fs[fi].used)
                continue;

            any = 1;

            char row[GUI_TERM_LINE_LEN + 1];
            UINTN n =
                gui_char16_to_char(
                    g_fs[fi].name, row,
                    GUI_TERM_LINE_LEN - 10
                );

            row[n++] = ' ';
            row[n++] = '-';
            row[n++] = ' ';

            n += gui_uint_to_str(
                g_fs[fi].size, row + n
            );

            row[n++] = 'B';
            row[n] = '\0';

            gui_term_push(lines, count, row);
        }

        if (!any)
            gui_term_push(lines, count, "EMPTY - NO FILES");

    } else if (gui_starts_with(cmd, "TOUCH ")) {

        char name8[FS_NAME_MAX];
        gui_take_word(cmd + 6, name8, sizeof(name8));

        if (name8[0] == '\0') {

            gui_term_push(lines, count, "USAGE: TOUCH NAME");

        } else {

            CHAR16 name16[FS_NAME_MAX];
            gui_char_to_char16(name8, name16, FS_NAME_MAX);

            if (fs_find(name16) >= 0) {

                gui_term_push(lines, count, "FILE ALREADY EXISTS");

            } else {

                int idx = fs_find_free();

                if (idx < 0) {

                    gui_term_push(lines, count, "FILESYSTEM FULL");

                } else {

                    g_fs[idx].used = TRUE;
                    char16_copy(g_fs[idx].name, name16, FS_NAME_MAX);
                    g_fs[idx].data[0] = 0;
                    g_fs[idx].size = 0;

                    gui_term_push(lines, count, "CREATED");
                }
            }
        }

    } else if (gui_starts_with(cmd, "CAT ")) {

        char name8[FS_NAME_MAX];
        gui_take_word(cmd + 4, name8, sizeof(name8));

        if (name8[0] == '\0') {

            gui_term_push(lines, count, "USAGE: CAT NAME");

        } else {

            CHAR16 name16[FS_NAME_MAX];
            gui_char_to_char16(name8, name16, FS_NAME_MAX);

            int idx = fs_find(name16);

            if (idx < 0) {

                gui_term_push(lines, count, "NO SUCH FILE");

            } else if (g_fs[idx].size == 0) {

                gui_term_push(lines, count, "EMPTY FILE");

            } else {

                char body[FS_DATA_MAX];
                UINTN blen =
                    gui_char16_to_char(
                        g_fs[idx].data, body, FS_DATA_MAX
                    );

                UINTN p = 0;

                while (p < blen) {

                    char row[GUI_TERM_LINE_LEN + 1];
                    UINTN n = 0;

                    while (
                        p < blen &&
                        body[p] != '\n' &&
                        n < GUI_TERM_LINE_LEN
                    ) {
                        row[n++] = body[p++];
                    }

                    row[n] = '\0';

                    if (p < blen && body[p] == '\n')
                        p++;

                    gui_term_push(lines, count, row);
                }
            }
        }

    } else if (gui_starts_with(cmd, "SIZE ")) {

        char name8[FS_NAME_MAX];
        gui_take_word(cmd + 5, name8, sizeof(name8));

        if (name8[0] == '\0') {

            gui_term_push(lines, count, "USAGE: SIZE NAME");

        } else {

            CHAR16 name16[FS_NAME_MAX];
            gui_char_to_char16(name8, name16, FS_NAME_MAX);

            int idx = fs_find(name16);

            if (idx < 0) {

                gui_term_push(lines, count, "NO SUCH FILE");

            } else {

                char buf[24];
                UINTN n =
                    gui_uint_to_str(g_fs[idx].size, buf);

                buf[n++] = 'B';
                buf[n] = '\0';

                gui_term_push(lines, count, buf);
            }
        }

    } else if (gui_starts_with(cmd, "RM ")) {

        char name8[FS_NAME_MAX];
        gui_take_word(cmd + 3, name8, sizeof(name8));

        if (name8[0] == '\0') {

            gui_term_push(lines, count, "USAGE: RM NAME");

        } else {

            CHAR16 name16[FS_NAME_MAX];
            gui_char_to_char16(name8, name16, FS_NAME_MAX);

            int idx = fs_find(name16);

            if (idx < 0) {

                gui_term_push(lines, count, "NO SUCH FILE");

            } else {

                g_fs[idx].used = FALSE;
                gui_term_push(lines, count, "DELETED");
            }
        }

    } else if (
        gui_starts_with(cmd, "WRITE ") ||
        gui_starts_with(cmd, "APPEND ")
    ) {
        int is_append = gui_starts_with(cmd, "APPEND ");

        const char *rest =
            cmd + (is_append ? 7 : 6);

        char name8[FS_NAME_MAX];
        UINTN nlen = gui_take_word(rest, name8, sizeof(name8));

        rest += nlen;

        if (*rest == ' ')
            rest++;

        if (name8[0] == '\0') {

            gui_term_push(
                lines, count,
                is_append ? "USAGE: APPEND NAME TEXT"
                          : "USAGE: WRITE NAME TEXT"
            );

        } else {

            CHAR16 name16[FS_NAME_MAX];
            gui_char_to_char16(name8, name16, FS_NAME_MAX);

            int idx = fs_find(name16);

            if (idx < 0)
                idx = fs_find_free();

            if (idx < 0) {

                gui_term_push(lines, count, "FILESYSTEM FULL");

            } else {

                if (!g_fs[idx].used) {

                    g_fs[idx].used = TRUE;
                    char16_copy(g_fs[idx].name, name16, FS_NAME_MAX);
                    g_fs[idx].data[0] = 0;
                    g_fs[idx].size = 0;
                }

                CHAR16 text16[GUI_TERM_LINE_LEN + 1];
                gui_char_to_char16(
                    rest, text16, GUI_TERM_LINE_LEN + 1
                );

                UINTN need = char16_len(text16);

                if (!is_append) {

                    char16_copy(
                        g_fs[idx].data, text16, FS_DATA_MAX
                    );

                    g_fs[idx].size = char16_len(g_fs[idx].data);

                } else {

                    UINTN cur = g_fs[idx].size;
                    UINTN sep = (cur > 0) ? 1 : 0;

                    if (cur + sep + need >= FS_DATA_MAX) {

                        gui_term_push(
                            lines, count,
                            "FILE TOO LARGE"
                        );

                        need = 0;

                    } else {

                        if (sep)
                            g_fs[idx].data[cur++] = L'\n';

                        for (UINTN k = 0; k < need; k++)
                            g_fs[idx].data[cur++] = text16[k];

                        g_fs[idx].data[cur] = 0;
                        g_fs[idx].size = cur;
                    }
                }

                if (need > 0 || !is_append) {

                    char buf[32];
                    UINTN n = 0;

                    buf[n++] = 'O';
                    buf[n++] = 'K';
                    buf[n++] = ' ';
                    buf[n++] = '-';
                    buf[n++] = ' ';

                    n += gui_uint_to_str(
                        g_fs[idx].size, buf + n
                    );

                    buf[n++] = 'B';
                    buf[n] = '\0';

                    gui_term_push(lines, count, buf);
                }
            }
        }

    } else if (
        gui_starts_with(cmd, "MV ") ||
        gui_starts_with(cmd, "CP ")
    ) {
        int is_copy = gui_starts_with(cmd, "CP ");

        const char *rest = cmd + 3;

        char a8[FS_NAME_MAX];
        UINTN alen = gui_take_word(rest, a8, sizeof(a8));

        rest += alen;

        if (*rest == ' ')
            rest++;

        char b8[FS_NAME_MAX];
        gui_take_word(rest, b8, sizeof(b8));

        if (a8[0] == '\0' || b8[0] == '\0') {

            gui_term_push(
                lines, count,
                is_copy ? "USAGE: CP A B" : "USAGE: MV A B"
            );

        } else {

            CHAR16 a16[FS_NAME_MAX];
            CHAR16 b16[FS_NAME_MAX];
            gui_char_to_char16(a8, a16, FS_NAME_MAX);
            gui_char_to_char16(b8, b16, FS_NAME_MAX);

            int ai = fs_find(a16);

            if (ai < 0) {

                gui_term_push(lines, count, "NO SUCH FILE");

            } else if (fs_find(b16) >= 0) {

                gui_term_push(lines, count, "TARGET ALREADY EXISTS");

            } else if (is_copy) {

                int bi = fs_find_free();

                if (bi < 0) {

                    gui_term_push(lines, count, "FILESYSTEM FULL");

                } else {

                    g_fs[bi].used = TRUE;
                    char16_copy(g_fs[bi].name, b16, FS_NAME_MAX);
                    char16_copy(g_fs[bi].data, g_fs[ai].data, FS_DATA_MAX);
                    g_fs[bi].size = g_fs[ai].size;

                    gui_term_push(lines, count, "COPIED");
                }

            } else {

                char16_copy(g_fs[ai].name, b16, FS_NAME_MAX);
                gui_term_push(lines, count, "RENAMED");
            }
        }

    } else if (gui_starts_with(cmd, "CALC ")) {

        UINTN a = 0, b = 0;
        char op = '+';
        UINTN p = 5;

        while (cmd[p] >= '0' && cmd[p] <= '9') {
            a = a * 10 + (UINTN)(cmd[p] - '0');
            p++;
        }

        while (cmd[p] == ' ')
            p++;

        if (
            cmd[p] == '+' || cmd[p] == '-' ||
            cmd[p] == '*' || cmd[p] == '/'
        ) {
            op = cmd[p];
            p++;
        }

        while (cmd[p] == ' ')
            p++;

        while (cmd[p] >= '0' && cmd[p] <= '9') {
            b = b * 10 + (UINTN)(cmd[p] - '0');
            p++;
        }

        BOOLEAN ok = TRUE;
        UINTN res = 0;

        if (op == '+') res = a + b;
        else if (op == '-') res = (a >= b) ? a - b : 0;
        else if (op == '*') res = a * b;
        else if (op == '/') {
            if (b == 0) ok = FALSE;
            else res = a / b;
        }

        if (!ok) {

            gui_term_push(lines, count, "DIVISION BY ZERO");

        } else {

            char buf[24];
            gui_uint_to_str(res, buf);
            gui_term_push(lines, count, buf);
        }

    } else if (gui_streq(cmd, "REBOOT")) {

        gui_term_push(lines, count, "REBOOTING");

        st->RuntimeServices->ResetSystem(
            EfiResetCold, EFI_SUCCESS, 0, NULL
        );

    } else if (gui_streq(cmd, "SHUTDOWN")) {

        gui_term_push(lines, count, "SHUTTING DOWN");

        st->RuntimeServices->ResetSystem(
            EfiResetShutdown, EFI_SUCCESS, 0, NULL
        );

    } else {

        gui_term_push(lines, count, "UNKNOWN COMMAND. TRY HELP.");
    }

    return FALSE;
}
