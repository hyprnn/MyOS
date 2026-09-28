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
