/*
 * gui/desktop.c - курсор, рабочий стол, окна.
 * Часть MyOS; общие объявления - в myos.h.
 */
#include "myos.h"

void gui_draw_cursor_at(
    volatile UINT32 *fb,
    UINT32 stride, UINT32 fb_w, UINT32 fb_h,
    EFI_GRAPHICS_PIXEL_FORMAT fmt,
    INTN x, INTN y
)
{
    /* те же цвета, что у курсора внутри функций кадра:
       чёрная рамка + белая заливка */
    gui_fill_rect(
        fb, stride, fb_w, fb_h,
        x, y, GUI_CURSOR_SIZE, GUI_CURSOR_SIZE,
        gui_pack(fmt, 0, 0, 0)
    );

    gui_fill_rect(
        fb, stride, fb_w, fb_h,
        x + 2, y + 2, GUI_CURSOR_SIZE - 4, GUI_CURSOR_SIZE - 4,
        gui_pack(fmt, 255, 255, 255)
    );
}

/* Скопировать прямоугольник из заднего буфера в видеопамять */
void gui_blit_rect(
    volatile UINT32 *dst,
    volatile UINT32 *src,
    UINT32 stride, UINT32 fb_w, UINT32 fb_h,
    INTN x, INTN y, UINTN w, UINTN h
)
{
    if (x < 0) { if ((UINTN)(-x) >= w) return; w -= (UINTN)(-x); x = 0; }
    if (y < 0) { if ((UINTN)(-y) >= h) return; h -= (UINTN)(-y); y = 0; }

    if (x >= (INTN)fb_w || y >= (INTN)fb_h)
        return;

    if ((UINTN)x + w > fb_w)
        w = fb_w - (UINTN)x;

    if ((UINTN)y + h > fb_h)
        h = fb_h - (UINTN)y;

    for (UINTN row = 0; row < h; row++) {

        UINTN base = (UINTN)(y + (INTN)row) * stride + (UINTN)x;

        for (UINTN col = 0; col < w; col++)
            dst[base + col] = src[base + col];
    }
}


void gui_draw_desktop(
    volatile UINT32 *fb,
    UINT32 stride,
    UINT32 fb_w,
    UINT32 fb_h,
    EFI_GRAPHICS_PIXEL_FORMAT fmt,
    const GUI_ICON *icons,
    UINTN icon_count,
    BOOLEAN menu_open,
    INTN btn_x, INTN btn_y, UINTN btn_w, UINTN btn_h,
    INTN cur_x, INTN cur_y,
    const char *clock_text
)
{
    /* Классическая "объёмная" серо-бирюзовая палитра */
    UINT32 col_bg       = gui_pack(fmt, 0, 128, 128);   /* бирюза рабочего стола */
    UINT32 col_border   = gui_pack(fmt, 0, 0, 0);
    UINT32 col_face     = gui_pack(fmt, 192, 192, 192); /* серая "поверхность" */
    UINT32 col_hi       = gui_pack(fmt, 255, 255, 255);
    UINT32 col_light    = gui_pack(fmt, 223, 223, 223);
    UINT32 col_shadow   = gui_pack(fmt, 128, 128, 128);
    UINT32 col_sel      = gui_pack(fmt, 0, 0, 128);     /* тёмно-синее выделение */
    UINT32 col_text     = gui_pack(fmt, 0, 0, 0);
    UINT32 col_text_sel = gui_pack(fmt, 255, 255, 255);
    UINT32 col_cursor   = gui_pack(fmt, 255, 255, 255);

    INTN cx = cur_x + GUI_CURSOR_SIZE / 2;
    INTN cy = cur_y + GUI_CURSOR_SIZE / 2;

    /* Рабочий стол: теперь пустой, все программы
       запускаются из меню "Start", как на референсе. */
    gui_fill_rect(
        fb, stride, fb_w, fb_h,
        0, 0, fb_w, fb_h,
        col_bg
    );

    /* Панель задач: приподнятая серая панель во всю
       ширину экрана СВЕРХУ, слева - кнопка "Start",
       справа - вдавленные "часы". */
    gui_fill_rect(
        fb, stride, fb_w, fb_h,
        0, 0,
        fb_w, GUI_TASKBAR_H,
        col_face
    );

    gui_draw_bevel(
        fb, stride, fb_w, fb_h,
        0, 0,
        fb_w, GUI_TASKBAR_H,
        col_hi, col_light, col_shadow, col_border,
        TRUE
    );

    {
        /* Пока меню открыто, кнопка выглядит "вдавленной" -
           так же, как в Windows 95. */
        gui_fill_rect(
            fb, stride, fb_w, fb_h,
            btn_x, btn_y, btn_w, btn_h,
            col_face
        );

        gui_draw_bevel(
            fb, stride, fb_w, fb_h,
            btn_x, btn_y, btn_w, btn_h,
            col_hi, col_light, col_shadow, col_border,
            !menu_open
        );

        INTN txt_x = btn_x + (menu_open ? 8 : 7);

        /* имитация "жирного" текста двойной прорисовкой */
        gui_draw_text(
            fb, stride, fb_w, fb_h,
            txt_x, btn_y + (INTN)btn_h / 2 - 3,
            1, col_text,
            "START"
        );

        gui_draw_text(
            fb, stride, fb_w, fb_h,
            txt_x + 1, btn_y + (INTN)btn_h / 2 - 3,
            1, col_text,
            "START"
        );
    }

    {
        UINTN clock_w = gui_text_width(clock_text, 1) + 12;
        UINTN clock_h = (UINTN)GUI_TASKBAR_H - 6;
        INTN  clock_x = (INTN)fb_w - (INTN)clock_w - 3;
        INTN  clock_y = 3;

        gui_fill_rect(
            fb, stride, fb_w, fb_h,
            clock_x, clock_y, clock_w, clock_h,
            col_face
        );

        gui_draw_bevel(
            fb, stride, fb_w, fb_h,
            clock_x, clock_y, clock_w, clock_h,
            col_hi, col_light, col_shadow, col_border,
            FALSE
        );

        gui_draw_text(
            fb, stride, fb_w, fb_h,
            clock_x + 6, clock_y + (INTN)clock_h / 2 - 3,
            1, col_text,
            clock_text
        );
    }

    /* Меню "Start": выпадающая панель со списком программ,
       раскрывается вниз прямо под кнопкой. Каждый пункт -
       строка с подсветкой под курсором, как на референсе. */
    if (menu_open && icon_count > 0) {

        INTN  menu_x = icons[0].x;
        INTN  menu_y = icons[0].y;
        UINTN menu_w = icons[0].w;
        UINTN menu_h = icon_count * icons[0].h;

        gui_fill_rect(
            fb, stride, fb_w, fb_h,
            menu_x, menu_y, menu_w, menu_h,
            col_face
        );

        gui_draw_bevel(
            fb, stride, fb_w, fb_h,
            menu_x, menu_y, menu_w, menu_h,
            col_hi, col_light, col_shadow, col_border,
            TRUE
        );

        for (UINTN i = 0; i < icon_count; i++) {

            const GUI_ICON *ic = &icons[i];

            BOOLEAN hover = gui_point_in_rect(
                cx, cy, ic->x, ic->y, ic->w, ic->h
            );

            if (hover) {
                gui_fill_rect(
                    fb, stride, fb_w, fb_h,
                    ic->x + 2, ic->y + 1,
                    ic->w - 4, ic->h - 2,
                    col_sel
                );
            }

            gui_draw_text(
                fb, stride, fb_w, fb_h,
                ic->x + 8, ic->y + (INTN)ic->h / 2 - 3,
                1, hover ? col_text_sel : col_text,
                ic->label
            );
        }
    }

    /* Курсор: чёрная рамка + белая заливка */
    if (g_gui_draw_cursor) {

        gui_fill_rect(
            fb, stride, fb_w, fb_h,
            cur_x, cur_y,
            GUI_CURSOR_SIZE, GUI_CURSOR_SIZE,
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
 * Окно приложения: заголовок, кнопка закрытия "X",
 * несколько строк текста и курсор поверх всего.
 */
void gui_draw_window(
    volatile UINT32 *fb,
    UINT32 stride,
    UINT32 fb_w,
    UINT32 fb_h,
    EFI_GRAPHICS_PIXEL_FORMAT fmt,
    INTN win_x, INTN win_y,
    UINTN win_w, UINTN win_h,
    INTN btn_x, INTN btn_y,
    UINTN btn_size,
    const GUI_WINDOW_CONTENT *content,
    INTN cur_x, INTN cur_y
)
{
    UINT32 col_bg      = gui_pack(fmt, 0, 128, 128);
    UINT32 col_border  = gui_pack(fmt, 0, 0, 0);
    UINT32 col_hi      = gui_pack(fmt, 255, 255, 255);
    UINT32 col_light   = gui_pack(fmt, 223, 223, 223);
    UINT32 col_shadow  = gui_pack(fmt, 128, 128, 128);
    UINT32 col_title   = gui_pack(fmt, 0, 0, 128);   /* тёмно-синий заголовок */
    UINT32 col_ttext   = gui_pack(fmt, 255, 255, 255);
    UINT32 col_face    = gui_pack(fmt, 192, 192, 192);
    UINT32 col_win     = gui_pack(fmt, 255, 255, 255); /* белая "бумага" внутри */
    UINT32 col_text    = gui_pack(fmt, 0, 0, 0);
    UINT32 col_btn_tx  = gui_pack(fmt, 0, 0, 0);
    UINT32 col_cursor  = gui_pack(fmt, 255, 255, 255);

    /* Рабочий стол под окном */
    gui_fill_rect(
        fb, stride, fb_w, fb_h,
        0, 0, fb_w, fb_h,
        col_bg
    );

    /* Корпус окна: серая объёмная рамка */
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
        content->title
    );

    /* Строка меню под заголовком: плоская серая полоса
       с пунктами и "протравленной" (etched) линией-разделителем
       снизу - тем самым характерным приёмом Win9x, когда тонкая
       тёмная линия сразу сопровождается тонкой светлой под ней.
       Пункты декоративные (без реального меню), но оформлены
       в общем стиле остального интерфейса. */
    gui_fill_rect(
        fb, stride, fb_w, fb_h,
        win_x + 3, win_y + 25,
        win_w - 6, GUI_MENU_H,
        col_face
    );

    {
        static const char *menu_items[] = { "FILE", "EDIT", "VIEW", "HELP" };
        INTN mx = win_x + 9;

        for (UINTN i = 0; i < sizeof(menu_items) / sizeof(menu_items[0]); i++) {

            UINTN mw = gui_text_width(menu_items[i], 1);

            gui_draw_text(
                fb, stride, fb_w, fb_h,
                mx, win_y + 25 + (INTN)(GUI_MENU_H - 7) / 2,
                1, col_text,
                menu_items[i]
            );

            mx += (INTN)mw + 10;
        }
    }

    gui_fill_rect(
        fb, stride, fb_w, fb_h,
        win_x + 3, win_y + 25 + (INTN)GUI_MENU_H - 2,
        win_w - 6, 1,
        col_shadow
    );

    gui_fill_rect(
        fb, stride, fb_w, fb_h,
        win_x + 3, win_y + 25 + (INTN)GUI_MENU_H - 1,
        win_w - 6, 1,
        col_hi
    );

    /* Тело окна: серое поле со вдавленной "бумагой" */
    gui_fill_rect(
        fb, stride, fb_w, fb_h,
        win_x + 3, win_y + 25 + (INTN)GUI_MENU_H,
        win_w - 6, win_h - 30 - GUI_MENU_H,
        col_face
    );

    gui_fill_rect(
        fb, stride, fb_w, fb_h,
        win_x + 9, win_y + 31 + (INTN)GUI_MENU_H,
        win_w - 18, win_h - 42 - GUI_MENU_H,
        col_win
    );

    gui_draw_bevel(
        fb, stride, fb_w, fb_h,
        win_x + 9, win_y + 31 + (INTN)GUI_MENU_H,
        win_w - 18, win_h - 42 - GUI_MENU_H,
        col_hi, col_light, col_shadow, col_border,
        FALSE
    );

    INTN line_y = win_y + 40 + (INTN)GUI_MENU_H;

    for (UINTN i = 0; i < content->line_count; i++) {

        gui_draw_text(
            fb, stride, fb_w, fb_h,
            win_x + 16, line_y,
            1, col_text,
            content->lines[i]
        );

        line_y += 14;
    }

    /* Кнопка закрытия "X": серая объёмная кнопка,
       как у остальных элементов интерфейса, а не
       отдельная "иконка крестика" стороннего стиля. */
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

    /* Курсор: чёрная рамка + белая заливка */
    if (g_gui_draw_cursor) {

        gui_fill_rect(
            fb, stride, fb_w, fb_h,
            cur_x, cur_y,
            GUI_CURSOR_SIZE, GUI_CURSOR_SIZE,
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
 * Сравнение двух обычных char-строк (не CHAR16, как
 * основной streq()) - нужно для команд, набранных
 * прямо в GUI-терминале.
 */
int gui_streq(const char *a, const char *b)
{
    while (*a && *b) {

        if (*a != *b)
            return 0;

        a++;
        b++;
    }

    return *a == 0 && *b == 0;
}
