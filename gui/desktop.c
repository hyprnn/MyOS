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

/*
 * ------------------------------------------------------------------
 * Вывод кадра на экран без мигания курсора.
 *
 * Что было раньше и почему курсор "моргал и замирал" на ноутбуке:
 *   1) часы на панели задач раз в ~0.25-1 с помечали ВЕСЬ кадр
 *      как изменившийся (даже если секунда не сменилась);
 *   2) полный кадр копировался в видеопамять целиком - на реальном
 *      железе это ~1-2 млн пикселей в медленную (некэшируемую)
 *      память, десятки миллисекунд, и всё это время мышь никто не
 *      опрашивал - отсюда "остановка";
 *   3) копия затирала курсор, а рисовался он заново только ПОСЛЕ
 *      копии - отсюда "моргание".
 *
 * Теперь:
 *   * есть "теневой" буфер (shadow) в обычной RAM - точная копия
 *     того, что сейчас на экране (без курсора). В видеопамять пишутся
 *     только пиксели, которые действительно отличаются. Смена цифры
 *     на часах - это пара сотен пикселей, а не два миллиона;
 *   * курсор "вклеивается" в поток пикселей прямо при записи: каждый
 *     пиксель экрана пишется один раз и сразу правильным цветом
 *     (картинка или курсор поверх неё). Момента, когда курсора на
 *     экране нет, больше не существует.
 * ------------------------------------------------------------------
 */

/* Цвет пикселя (px,py) экрана: курсор, если пиксель под курсором
   с левым верхним углом (cx,cy), иначе картинка из заднего буфера.
   Форма курсора та же, что в gui_draw_cursor_at: квадрат 12x12,
   чёрная рамка 2 пикселя, белая середина. */
static inline UINT32 gui_composite_px(
    volatile UINT32 *back, UINT32 stride,
    INTN px, INTN py, INTN cx, INTN cy,
    UINT32 black, UINT32 white
)
{
    INTN dx = px - cx;
    INTN dy = py - cy;

    if (dx >= 0 && dy >= 0 && dx < GUI_CURSOR_SIZE && dy < GUI_CURSOR_SIZE) {

        BOOLEAN inner =
            dx >= 2 && dy >= 2 &&
            dx < GUI_CURSOR_SIZE - 2 && dy < GUI_CURSOR_SIZE - 2;

        return inner ? white : black;
    }

    return back[(UINTN)py * stride + (UINTN)px];
}

/* Переписать в видеопамяти прямоугольник (x,y,w,h) "картинка +
   курсор в (cx,cy)". Каждый пиксель пишется ровно один раз. */
static void gui_compose_rect(
    volatile UINT32 *fb, volatile UINT32 *back,
    UINT32 stride, UINT32 fb_w, UINT32 fb_h,
    INTN x, INTN y, INTN w, INTN h,
    INTN cx, INTN cy, UINT32 black, UINT32 white
)
{
    if (x < 0) { w += x; x = 0; }
    if (y < 0) { h += y; y = 0; }
    if (x + w > (INTN)fb_w) w = (INTN)fb_w - x;
    if (y + h > (INTN)fb_h) h = (INTN)fb_h - y;

    if (w <= 0 || h <= 0)
        return;

    for (INTN py = y; py < y + h; py++)
        for (INTN px = x; px < x + w; px++)
            fb[(UINTN)py * stride + (UINTN)px] =
                gui_composite_px(back, stride, px, py, cx, cy, black, white);
}

/* Сдвинуть курсор со старого места (ox,oy) на новое (nx,ny):
   переписываем оба квадратика 12x12 составным цветом. Старый
   квадрат получает картинку (или новый курсор, если перекрываются),
   новый - курсор. Никаких промежуточных состояний на экране. */
void gui_present_cursor(
    volatile UINT32 *fb, volatile UINT32 *back,
    UINT32 stride, UINT32 fb_w, UINT32 fb_h,
    EFI_GRAPHICS_PIXEL_FORMAT fmt,
    INTN ox, INTN oy, INTN nx, INTN ny
)
{
    UINT32 black = gui_pack(fmt, 0, 0, 0);
    UINT32 white = gui_pack(fmt, 255, 255, 255);

    gui_compose_rect(fb, back, stride, fb_w, fb_h,
                     ox, oy, GUI_CURSOR_SIZE, GUI_CURSOR_SIZE,
                     nx, ny, black, white);

    gui_compose_rect(fb, back, stride, fb_w, fb_h,
                     nx, ny, GUI_CURSOR_SIZE, GUI_CURSOR_SIZE,
                     nx, ny, black, white);
}

/*
 * Вывести готовый кадр из заднего буфера back на экран fb.
 *
 * shadow - копия того, что сейчас в видеопамяти (без курсора), или
 * NULL, если под неё не хватило памяти. full = TRUE - переписать
 * экран целиком (первый кадр: что там на экране - неизвестно).
 *
 * Строки сравниваются по 64 бита (2 пикселя за раз) - это обычная
 * RAM, быстро. В видеопамять уходят только отличающиеся пиксели,
 * курсор вклеивается на лету. Старое место курсора (ox,oy) потом
 * дочищает gui_present_cursor - вызывающий делает это сам.
 *
 * Возвращает число записанных в видеопамять пикселей (для отладки).
 */
UINTN gui_present_frame(
    volatile UINT32 *fb, volatile UINT32 *back, UINT32 *shadow,
    UINT32 stride, UINT32 fb_w, UINT32 fb_h,
    EFI_GRAPHICS_PIXEL_FORMAT fmt,
    INTN cx, INTN cy, BOOLEAN full
)
{
    UINT32 black = gui_pack(fmt, 0, 0, 0);
    UINT32 white = gui_pack(fmt, 255, 255, 255);
    UINTN written = 0;

    for (UINTN y = 0; y < fb_h; y++) {

        UINTN row = y * stride;
        BOOLEAN cursor_row =
            (INTN)y >= cy && (INTN)y < cy + GUI_CURSOR_SIZE;

        UINTN x = 0;

        while (x < fb_w) {

            /* быстро пропускаем совпадающие пары пикселей */
            if (!full && shadow != NULL) {

                while (
                    x + 1 < fb_w &&
                    *(volatile UINT64 *)&back[row + x] ==
                        *(UINT64 *)&shadow[row + x]
                )
                    x += 2;

                if (x >= fb_w)
                    break;

                if (back[row + x] == shadow[row + x]) {
                    x++;
                    continue;
                }
            }

            UINT32 v = back[row + x];

            if (shadow != NULL)
                shadow[row + x] = v;

            if (cursor_row && (INTN)x >= cx && (INTN)x < cx + GUI_CURSOR_SIZE)
                v = gui_composite_px(back, stride, (INTN)x, (INTN)y,
                                     cx, cy, black, white);

            fb[row + x] = v;
            written++;
            x++;
        }
    }

    return written;
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
    const char *clock_text,
    INTN shortcut_sel
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

    /* Рабочий стол: бирюза и колонка ярлыков слева (этап 6,
       gui/shortcuts.c); все программы есть и в меню "Start". */
    gui_fill_rect(
        fb, stride, fb_w, fb_h,
        0, 0, fb_w, fb_h,
        col_bg
    );

    gui_draw_shortcuts(fb, stride, fb_w, fb_h, fmt, shortcut_sel);

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

        /* не рисовать ниже окна (длинный список в Проводнике) */
        if (line_y + 10 > win_y + (INTN)win_h - 8)
            break;

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
