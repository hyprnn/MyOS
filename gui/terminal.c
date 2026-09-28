/*
 * gui/terminal.c - терминал внутри GUI.
 * Часть MyOS; общие объявления - в myos.h.
 */
#include "myos.h"


/*
 * Окно терминала: то же самое оформление рамки/шапки,
 * что и у остальных программ, но внутри - чёрный
 * viewport с живым вводом, как у отдельного
 * приложения-терминала (kitty и подобные), а не
 * встроенная в ОС консоль поверх всего экрана.
 */
void gui_draw_terminal(
    volatile UINT32 *fb,
    UINT32 stride,
    UINT32 fb_w,
    UINT32 fb_h,
    EFI_GRAPHICS_PIXEL_FORMAT fmt,
    INTN win_x, INTN win_y,
    UINTN win_w, UINTN win_h,
    INTN btn_x, INTN btn_y,
    UINTN btn_size,
    const char lines[][GUI_TERM_LINE_LEN + 1],
    UINTN line_count,
    const char *input
)
{
    UINT32 col_bg      = gui_pack(fmt, 0, 128, 128);
    UINT32 col_border  = gui_pack(fmt, 0, 0, 0);
    UINT32 col_hi      = gui_pack(fmt, 255, 255, 255);
    UINT32 col_light   = gui_pack(fmt, 223, 223, 223);
    UINT32 col_shadow  = gui_pack(fmt, 128, 128, 128);
    UINT32 col_title   = gui_pack(fmt, 0, 0, 128);
    UINT32 col_ttext   = gui_pack(fmt, 255, 255, 255);
    UINT32 col_face    = gui_pack(fmt, 192, 192, 192);
    UINT32 col_btn_tx  = gui_pack(fmt, 0, 0, 0);

    /* Тёмный "стеклянный" фон терминала - как у kitty
       и большинства отдельных терминальных приложений,
       а не белая "бумага" остальных окон. */
    UINT32 col_term_bg = gui_pack(fmt, 12, 12, 12);
    UINT32 col_term_fg = gui_pack(fmt, 220, 220, 220);
    UINT32 col_prompt  = gui_pack(fmt, 0, 220, 120);

    /* Рабочий стол под окном */
    gui_fill_rect(
        fb, stride, fb_w, fb_h,
        0, 0, fb_w, fb_h,
        col_bg
    );

    /* Корпус окна: та же серая объёмная рамка, что и
       у остальных программ - терминал выглядит своим
       окном на общем столе, а не отдельным экраном. */
    gui_fill_rect(
        fb, stride, fb_w, fb_h,
        win_x, win_y, win_w, win_h,
        col_face
    );

    gui_draw_bevel(
        fb, stride, fb_w, fb_h,
        win_x, win_y, win_w, win_h,
        col_hi, col_light, col_shadow, col_border,
        TRUE
    );

    /* Заголовок окна */
    gui_fill_rect(
        fb, stride, fb_w, fb_h,
        win_x + 3, win_y + 3,
        win_w - 6, 22,
        col_title
    );

    gui_draw_text(
        fb, stride, fb_w, fb_h,
        win_x + 9, win_y + 9,
        1, col_ttext,
        "TERMINAL"
    );

    /* Чёрный viewport вместо белой "бумаги" - без
       декоративного меню FILE/EDIT/VIEW, которое
       настоящему терминалу не нужно. */
    INTN  term_x = win_x + 9;
    INTN  term_y = win_y + 31;
    UINTN term_w = win_w - 18;
    UINTN term_h = win_h - 42;

    gui_fill_rect(
        fb, stride, fb_w, fb_h,
        term_x, term_y, term_w, term_h,
        col_term_bg
    );

    gui_draw_bevel(
        fb, stride, fb_w, fb_h,
        term_x, term_y, term_w, term_h,
        col_hi, col_light, col_shadow, col_border,
        FALSE
    );

    UINTN row_h = 9;
    UINTN max_rows = (UINTN)term_h / row_h;

    if (max_rows < 2)
        max_rows = 2;

    UINTN visible =
        (line_count < max_rows - 1) ? line_count : max_rows - 1;

    UINTN first = line_count - visible;

    INTN line_y = term_y + 4;

    for (UINTN i = 0; i < visible; i++) {

        gui_draw_text(
            fb, stride, fb_w, fb_h,
            term_x + 4, line_y,
            1, col_term_fg,
            lines[first + i]
        );

        line_y += (INTN)row_h;
    }

    /* Строка ввода внизу viewport'а: зелёное
       приглашение, набранный текст и блочный курсор -
       как в настоящем терминале. */
    INTN prompt_y =
        term_y + (INTN)term_h - (INTN)row_h - 3;

    INTN px = term_x + 4;

    px += (INTN)gui_draw_text(
        fb, stride, fb_w, fb_h,
        px, prompt_y,
        1, col_prompt,
        "> "
    );

    px += (INTN)gui_draw_text(
        fb, stride, fb_w, fb_h,
        px, prompt_y,
        1, col_term_fg,
        input
    );

    gui_fill_rect(
        fb, stride, fb_w, fb_h,
        px, prompt_y, 6, 7,
        col_term_fg
    );

    /* Кнопка закрытия "X" - того же стиля, что у
       остальных окон (закрывается также по Esc). */
    gui_fill_rect(
        fb, stride, fb_w, fb_h,
        btn_x, btn_y, btn_size, btn_size,
        col_face
    );

    gui_draw_bevel(
        fb, stride, fb_w, fb_h,
        btn_x, btn_y, btn_size, btn_size,
        col_hi, col_light, col_shadow, col_border,
        TRUE
    );

    gui_draw_text(
        fb, stride, fb_w, fb_h,
        btn_x + 6, btn_y + 5,
        1, col_btn_tx,
        "X"
    );
}


/*
 * Окно "Сапёра": та же рамка/заголовок/крестик, что и у
 * остальных программ, а внутри - панель со счётчиком мин,
 * улыбающейся кнопкой рестарта, таймером и самим полем.
 */
void gui_draw_minesweeper(
    volatile UINT32 *fb,
    UINT32 stride,
    UINT32 fb_w,
    UINT32 fb_h,
    EFI_GRAPHICS_PIXEL_FORMAT fmt,
    INTN win_x, INTN win_y,
    UINTN win_w, UINTN win_h,
    INTN btn_x, INTN btn_y,
    UINTN btn_size,
    const GUI_MS_STATE *ms,
    INTN cur_x, INTN cur_y
)
{
    UINT32 col_bg      = gui_pack(fmt, 0, 128, 128);
    UINT32 col_border  = gui_pack(fmt, 0, 0, 0);
    UINT32 col_hi      = gui_pack(fmt, 255, 255, 255);
    UINT32 col_light   = gui_pack(fmt, 223, 223, 223);
    UINT32 col_shadow  = gui_pack(fmt, 128, 128, 128);
    UINT32 col_title   = gui_pack(fmt, 0, 0, 128);
    UINT32 col_ttext   = gui_pack(fmt, 255, 255, 255);
    UINT32 col_face    = gui_pack(fmt, 192, 192, 192);
    UINT32 col_text    = gui_pack(fmt, 0, 0, 0);
    UINT32 col_led_bg  = gui_pack(fmt, 20, 20, 20);
    UINT32 col_led_fg  = gui_pack(fmt, 220, 0, 0);
    UINT32 col_sel     = gui_pack(fmt, 0, 0, 128);
    UINT32 col_red_bg  = gui_pack(fmt, 255, 0, 0);
    UINT32 col_flag    = gui_pack(fmt, 200, 0, 0);
    UINT32 col_cursor  = gui_pack(fmt, 255, 255, 255);

    GUI_MS_LAYOUT L;
    gui_ms_compute_layout(win_x, win_y, win_w, win_h, &L);

    /* Рабочий стол под окном */
    gui_fill_rect(fb, stride, fb_w, fb_h, 0, 0, fb_w, fb_h, col_bg);

    /* Корпус окна */
    gui_fill_rect(fb, stride, fb_w, fb_h, win_x, win_y, win_w, win_h, col_face);

    gui_draw_bevel(
        fb, stride, fb_w, fb_h,
        win_x, win_y, win_w, win_h,
        col_hi, col_light, col_shadow, col_border,
        TRUE
    );

    /* Заголовок */
    gui_fill_rect(
        fb, stride, fb_w, fb_h,
        win_x + 3, win_y + 3, win_w - 6, 22,
        col_title
    );

    gui_draw_text(
        fb, stride, fb_w, fb_h,
        win_x + 9, win_y + 9,
        1, col_ttext,
        "MINESWEEPER"
    );

    /* Строка "меню" GAME / HELP - декоративная, как на
       референсе, в общем стиле остальных окон программы. */
    gui_fill_rect(
        fb, stride, fb_w, fb_h,
        win_x + 3, win_y + 25, win_w - 6, GUI_MENU_H,
        col_face
    );

    {
        static const char *ms_menu_items[] = { "GAME", "HELP" };
        INTN mx = win_x + 9;

        for (UINTN i = 0; i < 2; i++) {

            UINTN mw = gui_text_width(ms_menu_items[i], 1);

            gui_draw_text(
                fb, stride, fb_w, fb_h,
                mx, win_y + 25 + (INTN)(GUI_MENU_H - 7) / 2,
                1, col_text,
                ms_menu_items[i]
            );

            mx += (INTN)mw + 10;
        }
    }

    gui_fill_rect(
        fb, stride, fb_w, fb_h,
        win_x + 3, win_y + 25 + (INTN)GUI_MENU_H - 2,
        win_w - 6, 1, col_shadow
    );

    gui_fill_rect(
        fb, stride, fb_w, fb_h,
        win_x + 3, win_y + 25 + (INTN)GUI_MENU_H - 1,
        win_w - 6, 1, col_hi
    );

    /* Вдавленная общая рамка игрового поля */
    gui_fill_rect(
        fb, stride, fb_w, fb_h,
        L.field_x, L.field_y, L.field_w, L.field_h,
        col_face
    );

    gui_draw_bevel(
        fb, stride, fb_w, fb_h,
        L.field_x, L.field_y, L.field_w, L.field_h,
        col_hi, col_light, col_shadow, col_border,
        FALSE
    );

    /* Панель шапки: счётчик мин / смайлик / таймер */
    gui_fill_rect(
        fb, stride, fb_w, fb_h,
        L.head_x, L.head_y, L.head_w, L.head_h,
        col_face
    );

    gui_draw_bevel(
        fb, stride, fb_w, fb_h,
        L.head_x, L.head_y, L.head_w, L.head_h,
        col_hi, col_light, col_shadow, col_border,
        FALSE
    );

    /* Счётчик оставшихся мин (слева) */
    gui_fill_rect(
        fb, stride, fb_w, fb_h,
        L.led1_x, L.led1_y, L.led_w, L.led_h,
        col_led_bg
    );

    gui_draw_bevel(
        fb, stride, fb_w, fb_h,
        L.led1_x, L.led1_y, L.led_w, L.led_h,
        col_shadow, col_border, col_hi, col_light,
        FALSE
    );

    {
        UINTN left =
            (ms->flags_used >= GUI_MS_MINES)
                ? 0 : GUI_MS_MINES - ms->flags_used;

        if (left > 999)
            left = 999;

        char buf[4];
        buf[0] = (char)('0' + (left / 100) % 10);
        buf[1] = (char)('0' + (left / 10) % 10);
        buf[2] = (char)('0' + left % 10);
        buf[3] = '\0';

        gui_draw_text(
            fb, stride, fb_w, fb_h,
            L.led1_x + 5, L.led1_y + (INTN)(L.led_h - 14) / 2,
            2, col_led_fg, buf
        );
    }

    /* Таймер (справа) */
    gui_fill_rect(
        fb, stride, fb_w, fb_h,
        L.led2_x, L.led2_y, L.led_w, L.led_h,
        col_led_bg
    );

    gui_draw_bevel(
        fb, stride, fb_w, fb_h,
        L.led2_x, L.led2_y, L.led_w, L.led_h,
        col_shadow, col_border, col_hi, col_light,
        FALSE
    );

    {
        UINTN t = ms->timer;

        if (t > 999)
            t = 999;

        char buf[4];
        buf[0] = (char)('0' + (t / 100) % 10);
        buf[1] = (char)('0' + (t / 10) % 10);
        buf[2] = (char)('0' + t % 10);
        buf[3] = '\0';

        gui_draw_text(
            fb, stride, fb_w, fb_h,
            L.led2_x + 5, L.led2_y + (INTN)(L.led_h - 14) / 2,
            2, col_led_fg, buf
        );
    }

    /* Кнопка-смайлик (рестарт) */
    gui_fill_rect(
        fb, stride, fb_w, fb_h,
        L.smile_x, L.smile_y, L.smile_size, L.smile_size,
        col_face
    );

    gui_draw_bevel(
        fb, stride, fb_w, fb_h,
        L.smile_x, L.smile_y, L.smile_size, L.smile_size,
        col_hi, col_light, col_shadow, col_border,
        TRUE
    );

    {
        int mode = ms->won ? 1 : (ms->over ? 2 : 0);

        gui_draw_face(
            fb, stride, fb_w, fb_h, fmt,
            L.smile_x + 3, L.smile_y + 3, mode
        );
    }

    /* Вдавленная рамка вокруг сетки клеток */
    gui_draw_bevel(
        fb, stride, fb_w, fb_h,
        L.grid_x - 3, L.grid_y - 3,
        GUI_MS_COLS * L.cell + 6, GUI_MS_ROWS * L.cell + 6,
        col_hi, col_light, col_shadow, col_border,
        FALSE
    );

    INTN cx = cur_x + GUI_CURSOR_SIZE / 2;
    INTN cy = cur_y + GUI_CURSOR_SIZE / 2;

    for (int r = 0; r < GUI_MS_ROWS; r++) {

        for (int c = 0; c < GUI_MS_COLS; c++) {

            INTN cell_x = L.grid_x + (INTN)((UINTN)c * L.cell);
            INTN cell_y = L.grid_y + (INTN)((UINTN)r * L.cell);

            BOOLEAN hovered =
                !ms->over &&
                gui_point_in_rect(
                    cx, cy, cell_x, cell_y, L.cell, L.cell
                );

            if (ms->revealed[r][c]) {

                UINT32 bg =
                    (ms->mine[r][c] &&
                     r == ms->boom_r && c == ms->boom_c)
                        ? col_red_bg : col_face;

                gui_fill_rect(
                    fb, stride, fb_w, fb_h,
                    cell_x, cell_y, L.cell, L.cell, bg
                );

                gui_draw_border(
                    fb, stride, fb_w, fb_h,
                    cell_x, cell_y, L.cell, L.cell, col_shadow
                );

                if (ms->mine[r][c]) {

                    gui_draw_icon_mine(
                        fb, stride, fb_w, fb_h,
                        cell_x + ((INTN)L.cell - 11) / 2,
                        cell_y + ((INTN)L.cell - 11) / 2,
                        col_border, col_hi
                    );

                } else if (ms->adj[r][c] > 0) {

                    char buf[2];
                    buf[0] = (char)('0' + ms->adj[r][c]);
                    buf[1] = '\0';

                    UINT32 numcol =
                        gui_ms_number_color(fmt, ms->adj[r][c]);

                    gui_draw_text(
                        fb, stride, fb_w, fb_h,
                        cell_x + ((INTN)L.cell - 6) / 2,
                        cell_y + ((INTN)L.cell - 7) / 2,
                        1, numcol, buf
                    );
                }

            } else {

                gui_fill_rect(
                    fb, stride, fb_w, fb_h,
                    cell_x + 1, cell_y + 1,
                    L.cell - 2, L.cell - 2,
                    col_face
                );

                gui_draw_bevel(
                    fb, stride, fb_w, fb_h,
                    cell_x + 1, cell_y + 1,
                    L.cell - 2, L.cell - 2,
                    col_hi, col_light, col_shadow, col_border,
                    TRUE
                );

                if (ms->flagged[r][c]) {

                    gui_draw_icon_flag(
                        fb, stride, fb_w, fb_h,
                        cell_x + ((INTN)L.cell - 11) / 2,
                        cell_y + ((INTN)L.cell - 11) / 2,
                        col_border, col_flag, col_border
                    );
                }

                if (hovered) {

                    gui_draw_border(
                        fb, stride, fb_w, fb_h,
                        cell_x, cell_y, L.cell, L.cell, col_sel
                    );
                }
            }
        }
    }

    /* Кнопка закрытия "X" */
    gui_fill_rect(
        fb, stride, fb_w, fb_h,
        btn_x, btn_y, btn_size, btn_size,
        col_face
    );

    gui_draw_bevel(
        fb, stride, fb_w, fb_h,
        btn_x, btn_y, btn_size, btn_size,
        col_hi, col_light, col_shadow, col_border,
        TRUE
    );

    gui_draw_text(
        fb, stride, fb_w, fb_h,
        btn_x + 6, btn_y + 5,
        1, col_text,
        "X"
    );

    /* Курсор поверх всего */
    if (g_gui_draw_cursor) {

        gui_fill_rect(
            fb, stride, fb_w, fb_h,
            cur_x, cur_y, GUI_CURSOR_SIZE, GUI_CURSOR_SIZE,
            col_border
        );

        gui_fill_rect(
            fb, stride, fb_w, fb_h,
            cur_x + 2, cur_y + 2,
            GUI_CURSOR_SIZE - 4, GUI_CURSOR_SIZE - 4,
            col_cursor
        );
    }
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
