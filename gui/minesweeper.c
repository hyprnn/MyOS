/*
 * gui/minesweeper.c - Сапёр: логика поля и иконки.
 * Часть MyOS; общие объявления - в myos.h.
 */
#include "myos.h"

/* Затравка от текущего времени (RTC), чтобы расклад мин
   отличался между запусками, а не только между партиями
   внутри одного запуска. */
void gui_ms_seed(EFI_SYSTEM_TABLE *st)
{
    UINT32 seed = 0x9e3779b9u;

    EFI_TIME t;

    if (
        st->RuntimeServices->GetTime &&
        st->RuntimeServices->GetTime(&t, NULL) == EFI_SUCCESS
    ) {
        seed ^= ((UINT32)t.Year     << 20) ^
                ((UINT32)t.Month    << 16) ^
                ((UINT32)t.Day      << 11) ^
                ((UINT32)t.Hour     << 22) ^
                ((UINT32)t.Minute   <<  6) ^
                ((UINT32)t.Second)         ^
                t.Nanosecond;
    }

    if (seed == 0)
        seed = 12345;

    g_ms_rng = seed;
}


void gui_ms_reset(GUI_MS_STATE *ms)
{
    for (UINTN r = 0; r < GUI_MS_ROWS; r++) {
        for (UINTN c = 0; c < GUI_MS_COLS; c++) {
            ms->mine[r][c]     = 0;
            ms->adj[r][c]      = 0;
            ms->revealed[r][c] = 0;
            ms->flagged[r][c]  = 0;
        }
    }

    ms->generated      = FALSE;
    ms->over           = FALSE;
    ms->won            = FALSE;
    ms->flags_used     = 0;
    ms->revealed_count = 0;
    ms->timer          = 0;
    ms->boom_r         = -1;
    ms->boom_c         = -1;
}


/*
 * Расставить мины, избегая клетки первого клика и её
 * соседей (классическое поведение "первый клик всегда
 * безопасен"), затем посчитать числа-подсказки.
 */
void gui_ms_generate(
    GUI_MS_STATE *ms, int safe_r, int safe_c
)
{
    UINTN placed = 0;

    while (placed < GUI_MS_MINES) {

        UINT32 rv = gui_ms_rand();

        int r = (int)(rv % GUI_MS_ROWS);
        int c = (int)((rv / GUI_MS_ROWS) % GUI_MS_COLS);

        int dr = r - safe_r;
        int dc = c - safe_c;

        if (dr < 0) dr = -dr;
        if (dc < 0) dc = -dc;

        if (dr <= 1 && dc <= 1)
            continue;

        if (ms->mine[r][c])
            continue;

        ms->mine[r][c] = 1;
        placed++;
    }

    for (int r = 0; r < GUI_MS_ROWS; r++) {

        for (int c = 0; c < GUI_MS_COLS; c++) {

            if (ms->mine[r][c])
                continue;

            UINTN n = 0;

            for (int dr = -1; dr <= 1; dr++) {

                for (int dc = -1; dc <= 1; dc++) {

                    if (dr == 0 && dc == 0)
                        continue;

                    int rr = r + dr;
                    int cc = c + dc;

                    if (rr < 0 || rr >= GUI_MS_ROWS ||
                        cc < 0 || cc >= GUI_MS_COLS)
                        continue;

                    if (ms->mine[rr][cc])
                        n++;
                }
            }

            ms->adj[r][c] = (UINT8)n;
        }
    }

    ms->generated = TRUE;
}


/*
 * Открыть клетку (r,c). Мины ещё нет на поле до первого
 * клика - она расставляется прямо тут, в первый раз.
 * Клетка с 0 соседних мин "заливает" соседей рекурсивно
 * (итеративно, через явный стек - без настоящей рекурсии).
 */
void gui_ms_reveal(GUI_MS_STATE *ms, int start_r, int start_c)
{
    if (ms->over)
        return;

    if (!ms->generated)
        gui_ms_generate(ms, start_r, start_c);

    if (ms->flagged[start_r][start_c] ||
        ms->revealed[start_r][start_c])
        return;

    if (ms->mine[start_r][start_c]) {

        ms->revealed[start_r][start_c] = 1;
        ms->over   = TRUE;
        ms->won    = FALSE;
        ms->boom_r = start_r;
        ms->boom_c = start_c;

        /* Проигрыш: показать все мины на поле. */
        for (int r = 0; r < GUI_MS_ROWS; r++)
            for (int c = 0; c < GUI_MS_COLS; c++)
                if (ms->mine[r][c])
                    ms->revealed[r][c] = 1;

        return;
    }

    UINT8 queued[GUI_MS_ROWS][GUI_MS_COLS];

    for (UINTN r = 0; r < GUI_MS_ROWS; r++)
        for (UINTN c = 0; c < GUI_MS_COLS; c++)
            queued[r][c] = 0;

    int stack_r[GUI_MS_ROWS * GUI_MS_COLS];
    int stack_c[GUI_MS_ROWS * GUI_MS_COLS];
    int sp = 0;

    stack_r[sp] = start_r;
    stack_c[sp] = start_c;
    sp++;
    queued[start_r][start_c] = 1;

    while (sp > 0) {

        sp--;

        int r = stack_r[sp];
        int c = stack_c[sp];

        if (ms->revealed[r][c] || ms->flagged[r][c])
            continue;

        ms->revealed[r][c] = 1;
        ms->revealed_count++;

        if (ms->adj[r][c] != 0)
            continue;

        for (int dr = -1; dr <= 1; dr++) {

            for (int dc = -1; dc <= 1; dc++) {

                if (dr == 0 && dc == 0)
                    continue;

                int rr = r + dr;
                int cc = c + dc;

                if (rr < 0 || rr >= GUI_MS_ROWS ||
                    cc < 0 || cc >= GUI_MS_COLS)
                    continue;

                if (ms->revealed[rr][cc] ||
                    ms->flagged[rr][cc] ||
                    ms->mine[rr][cc] ||
                    queued[rr][cc])
                    continue;

                queued[rr][cc] = 1;
                stack_r[sp] = rr;
                stack_c[sp] = cc;
                sp++;
            }
        }
    }

    if (
        ms->revealed_count ==
        (UINTN)(GUI_MS_ROWS * GUI_MS_COLS) - GUI_MS_MINES
    ) {
        ms->won  = TRUE;
        ms->over = TRUE;

        /* Красиво доставить флажки на оставшиеся мины. */
        for (int r = 0; r < GUI_MS_ROWS; r++) {
            for (int c = 0; c < GUI_MS_COLS; c++) {

                if (ms->mine[r][c] && !ms->flagged[r][c]) {
                    ms->flagged[r][c] = 1;
                    ms->flags_used++;
                }
            }
        }
    }
}


void gui_ms_toggle_flag(GUI_MS_STATE *ms, int r, int c)
{
    if (ms->over || ms->revealed[r][c])
        return;

    if (ms->flagged[r][c]) {

        ms->flagged[r][c] = 0;
        ms->flags_used--;

    } else {

        if (ms->flags_used >= GUI_MS_MINES)
            return;

        ms->flagged[r][c] = 1;
        ms->flags_used++;
    }
}


/*
 * Единственное место, где считается геометрия окна Сапёра -
 * и отрисовка, и обработка кликов берут клетки/кнопки строго
 * отсюда, чтобы никогда не разъехаться друг с другом.
 */
void gui_ms_compute_layout(
    INTN win_x, INTN win_y, UINTN win_w, UINTN win_h,
    GUI_MS_LAYOUT *L
)
{
    L->field_x = win_x + 9;
    L->field_y = win_y + 31 + (INTN)GUI_MENU_H;
    L->field_w = win_w - 18;
    L->field_h = win_h - 42 - GUI_MENU_H;

    L->head_x = L->field_x + 6;
    L->head_y = L->field_y + 6;
    L->head_w = (L->field_w > 12) ? L->field_w - 12 : L->field_w;
    L->head_h = 32;

    L->led_w = 40;
    L->led_h = 20;

    L->led1_x = L->head_x + 6;
    L->led1_y = L->head_y + ((INTN)L->head_h - (INTN)L->led_h) / 2;

    L->led2_x = L->head_x + (INTN)L->head_w - 6 - (INTN)L->led_w;
    L->led2_y = L->led1_y;

    L->smile_size = 24;
    L->smile_x = L->head_x + (INTN)L->head_w / 2 - (INTN)L->smile_size / 2;
    L->smile_y = L->head_y + ((INTN)L->head_h - (INTN)L->smile_size) / 2;

    INTN  grid_area_x = L->head_x;
    INTN  grid_area_y = L->head_y + (INTN)L->head_h + 9;
    UINTN grid_area_w = L->head_w;

    INTN grid_area_bottom = L->field_y + (INTN)L->field_h - 6;
    UINTN grid_area_h =
        (grid_area_bottom > grid_area_y)
            ? (UINTN)(grid_area_bottom - grid_area_y)
            : 1;

    UINTN cell_w = grid_area_w / GUI_MS_COLS;
    UINTN cell_h = grid_area_h / GUI_MS_ROWS;
    UINTN cell   = (cell_w < cell_h) ? cell_w : cell_h;

    if (cell > 24)
        cell = 24;

    if (cell < 10)
        cell = 10;

    L->cell = cell;

    UINTN grid_w_px = GUI_MS_COLS * cell;
    UINTN grid_h_px = GUI_MS_ROWS * cell;

    L->grid_x = grid_area_x + (INTN)(grid_area_w - grid_w_px) / 2;
    L->grid_y = grid_area_y + (INTN)(grid_area_h - grid_h_px) / 2;
}


/* Цвет цифры-подсказки - как в оригинальном Сапёре
   (1 синий, 2 зелёный, 3 красный, 4 тёмно-синий, ...). */
UINT32 gui_ms_number_color(
    EFI_GRAPHICS_PIXEL_FORMAT fmt, UINT8 n
)
{
    switch (n) {
        case 1: return gui_pack(fmt,   0,   0, 255);
        case 2: return gui_pack(fmt,   0, 128,   0);
        case 3: return gui_pack(fmt, 255,   0,   0);
        case 4: return gui_pack(fmt,   0,   0, 128);
        case 5: return gui_pack(fmt, 128,   0,   0);
        case 6: return gui_pack(fmt,   0, 128, 128);
        case 7: return gui_pack(fmt,   0,   0,   0);
        default: return gui_pack(fmt, 128, 128, 128);
    }
}


/*
 * Значок мины: закрашенный кружок с 8 "усиками" и белым
 * бликом, посчитанный от расстояния до центра - не нужно
 * хранить готовый битмап руками.
 */
void gui_draw_icon_mine(
    volatile UINT32 *fb,
    UINT32 stride, UINT32 fb_w, UINT32 fb_h,
    INTN x, INTN y,
    UINT32 body, UINT32 hi
)
{
    const INTN c0 = 5;

    for (INTN r = 0; r < 11; r++) {

        for (INTN c = 0; c < 11; c++) {

            INTN dx = c - c0;
            INTN dy = r - c0;
            INTN ax = (dx < 0) ? -dx : dx;
            INTN ay = (dy < 0) ? -dy : dy;

            BOOLEAN on = FALSE;

            if (dx * dx + dy * dy <= 9)
                on = TRUE;
            else if (dx == 0 && ay >= 4)
                on = TRUE;
            else if (dy == 0 && ax >= 4)
                on = TRUE;
            else if (ax == 4 && ay == 4)
                on = TRUE;

            if (!on)
                continue;

            UINT32 col = (dx == -1 && dy == -1) ? hi : body;

            gui_fill_rect(
                fb, stride, fb_w, fb_h,
                x + c, y + r, 1, 1, col
            );
        }
    }
}


/* Значок флажка: флагшток + треугольный флаг + подставка. */
void gui_draw_icon_flag(
    volatile UINT32 *fb,
    UINT32 stride, UINT32 fb_w, UINT32 fb_h,
    INTN x, INTN y,
    UINT32 pole, UINT32 flag, UINT32 base
)
{
    gui_fill_rect(fb, stride, fb_w, fb_h, x + 5, y + 1, 1, 8, pole);

    gui_fill_rect(fb, stride, fb_w, fb_h, x + 6, y + 1, 4, 1, flag);
    gui_fill_rect(fb, stride, fb_w, fb_h, x + 6, y + 2, 3, 1, flag);
    gui_fill_rect(fb, stride, fb_w, fb_h, x + 6, y + 3, 2, 1, flag);
    gui_fill_rect(fb, stride, fb_w, fb_h, x + 6, y + 4, 1, 1, flag);

    gui_fill_rect(fb, stride, fb_w, fb_h, x + 3, y + 9, 5, 1, base);
}


/*
 * Лицо кнопки "рестарт": mode 0 = обычное, 1 = победа
 * (тёмные очки), 2 = проигрыш (крестики вместо глаз).
 */
void gui_draw_face(
    volatile UINT32 *fb,
    UINT32 stride, UINT32 fb_w, UINT32 fb_h,
    EFI_GRAPHICS_PIXEL_FORMAT fmt,
    INTN x, INTN y, int mode
)
{
    UINT32 yellow = gui_pack(fmt, 255, 216, 0);
    UINT32 black  = gui_pack(fmt,   0,   0, 0);

    const INTN c0 = 8;
    const INTN R  = 8;

    for (INTN r = 0; r < 17; r++) {

        for (INTN c = 0; c < 17; c++) {

            INTN dx = c - c0;
            INTN dy = r - c0;
            INTN d2 = dx * dx + dy * dy;

            if (d2 > R * R)
                continue;

            UINT32 col =
                (d2 >= (R - 1) * (R - 1)) ? black : yellow;

            gui_fill_rect(
                fb, stride, fb_w, fb_h,
                x + c, y + r, 1, 1, col
            );
        }
    }

    if (mode == 2) {

        /* проигрыш: крестики вместо глаз */
        gui_fill_rect(fb, stride, fb_w, fb_h, x + 4, y + 5, 1, 1, black);
        gui_fill_rect(fb, stride, fb_w, fb_h, x + 6, y + 5, 1, 1, black);
        gui_fill_rect(fb, stride, fb_w, fb_h, x + 5, y + 6, 1, 1, black);
        gui_fill_rect(fb, stride, fb_w, fb_h, x + 4, y + 7, 1, 1, black);
        gui_fill_rect(fb, stride, fb_w, fb_h, x + 6, y + 7, 1, 1, black);

        gui_fill_rect(fb, stride, fb_w, fb_h, x + 10, y + 5, 1, 1, black);
        gui_fill_rect(fb, stride, fb_w, fb_h, x + 12, y + 5, 1, 1, black);
        gui_fill_rect(fb, stride, fb_w, fb_h, x + 11, y + 6, 1, 1, black);
        gui_fill_rect(fb, stride, fb_w, fb_h, x + 10, y + 7, 1, 1, black);
        gui_fill_rect(fb, stride, fb_w, fb_h, x + 12, y + 7, 1, 1, black);

        gui_fill_rect(fb, stride, fb_w, fb_h, x + 5, y + 12, 7, 1, black);

    } else if (mode == 1) {

        /* победа: тёмные очки */
        gui_fill_rect(fb, stride, fb_w, fb_h, x + 3, y + 6, 4, 2, black);
        gui_fill_rect(fb, stride, fb_w, fb_h, x + 10, y + 6, 4, 2, black);
        gui_fill_rect(fb, stride, fb_w, fb_h, x + 7, y + 6, 3, 1, black);

        gui_fill_rect(fb, stride, fb_w, fb_h, x + 4, y + 11, 9, 2, black);

    } else {

        /* обычное лицо: два глаза-точки и улыбка */
        gui_fill_rect(fb, stride, fb_w, fb_h, x + 5, y + 6, 2, 2, black);
        gui_fill_rect(fb, stride, fb_w, fb_h, x + 10, y + 6, 2, 2, black);

        gui_fill_rect(fb, stride, fb_w, fb_h, x + 4, y + 10, 1, 1, black);
        gui_fill_rect(fb, stride, fb_w, fb_h, x + 12, y + 10, 1, 1, black);
        gui_fill_rect(fb, stride, fb_w, fb_h, x + 5, y + 11, 7, 1, black);
    }
}


/*
 * Экран рабочего стола: фон, иконки (с подсветкой
 * той, над которой сейчас курсор), панель задач
 * с часами и курсор поверх всего.
 */
/*
 * Курсор мыши рисуется ОТДЕЛЬНО от остальной картинки.
 *
 * Раньше любое движение мыши означало "перерисовать весь экран":
 * весь рабочий стол/окно заново в задний буфер (около миллиона
 * пикселей) и потом весь буфер в видеопамять (ещё миллион). Мышь
 * присылает десятки-сотни движений в секунду, а такой кадр
 * стоит миллисекунды - отсюда ощущение "5-15 FPS на мышке".
 *
 * Теперь задний буфер хранит картинку БЕЗ курсора, а курсор
 * рисуется только в видеопамяти, поверх. Когда сдвинулся ТОЛЬКО
 * курсор (и под ним ничего не должно поменяться - например,
 * подсветка пункта меню), достаточно: вернуть из заднего буфера
 * кусочек 12x12 там, где курсор был, и нарисовать курсор на новом
 * месте. Это ~300 пикселей вместо двух миллионов.
 *
 * g_gui_draw_cursor = TRUE - старое поведение (курсор рисуют сами
 * функции кадра); используется, если заднего буфера нет вообще.
 */
BOOLEAN g_gui_draw_cursor = TRUE;
