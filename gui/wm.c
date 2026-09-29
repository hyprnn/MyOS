/*
 * gui/wm.c - оконная система: композитор и рабочий стол (этап 7).
 * Часть MyOS; общие объявления - в myos.h.
 *
 * ЧТО ТАКОЕ КОМПОЗИТОР
 * --------------------
 * До этапа 7 GUI показывал одно окно на весь экран за раз. Теперь у
 * каждого окна свой буфер пикселей (его содержимое), а композитор
 * собирает из них КАДР: рабочий стол, потом окна снизу вверх по
 * "высоте" (z), потом панель задач, и наконец курсор-стрелка. Кадр
 * собирается в обычной памяти (back_buf), а на экран (медленная
 * видеопамять) переносятся только те точки, что изменились с
 * прошлого раза (shadow) - поэтому перерисовка дешёвая и без мигания.
 *
 * ОКНА бывают двух видов:
 *   * родные (gui/apps.c): терминал, блокнот, проводник, Сапёр... -
 *     у них есть функции paint (нарисовать) и event (нажатие). Их
 *     рисует ядро в буфер окна;
 *   * окна программ (kernel/winproc.c): программа в ring 3 создаёт
 *     окно системным вызовом, ядро отдаёт ей общий буфер пикселей,
 *     она рисует сама и говорит "перерисуй"; события (клик, клавиша)
 *     ядро складывает ей в очередь.
 *
 * Как в Windows 95: окна перетаскиваются за заголовок, крестик
 * закрывает, щелчок поднимает окно наверх и даёт ему фокус (клавиши
 * идут активному окну), на панели задач снизу - кнопка каждого окна
 * и кнопка "Пуск".
 */
#include "myos.h"

WIN g_windows[WM_MAX_WINDOWS];
volatile BOOLEAN g_wm_running = FALSE;

static UINT32 g_next_win_id = 1;
static UINT32 g_z_top = 1;
static WIN *g_focus = NULL;
static WIN *g_drag = NULL;          /* окно, которое тащат за заголовок */
static INT32 g_drag_dx, g_drag_dy;
static BOOLEAN g_stop = FALSE;

/* экран */
static UINT32 *g_back = NULL;       /* собранный кадр, 0x00RRGGBB */
static UINT32 *g_shadow = NULL;     /* что сейчас на экране (упаковано) */
static UINT32 SCR_W, SCR_H, SCR_STRIDE;
static EFI_GRAPHICS_PIXEL_FORMAT SCR_FMT;
static GFX g_screen;                /* поверхность = back_buf */

/* мышь */
static INT32 g_mx, g_my;
static BOOLEAN g_mouse_left = FALSE;
static BOOLEAN g_dirty = TRUE;

/* порядок кнопок на панели задач: стабильный, по id окна */
static UINT32 g_zi_idx[WM_MAX_WINDOWS];   /* индексы окон в порядке id */
static UINTN  g_zi_n = 0;

static void rebuild_taskbar_order(void)
{
    UINTN n = 0;
    UINT32 last = 0;

    for (UINTN k = 0; k < WM_MAX_WINDOWS; k++) {
        UINT32 best = 0xFFFFFFFFu;
        INTN pick = -1;
        for (UINTN i = 0; i < WM_MAX_WINDOWS; i++)
            if (g_windows[i].used && g_windows[i].id > last && g_windows[i].id < best) {
                best = g_windows[i].id;
                pick = (INTN)i;
            }
        if (pick < 0)
            break;
        g_zi_idx[n++] = (UINT32)pick;
        last = best;
    }

    g_zi_n = n;
}

/* цвета рабочего стола */
#define D_TEAL    0x008080u
#define D_FACE    0xC0C0C0u
#define D_NAVY    0x000080u
#define D_WHITE   0xFFFFFFu
#define D_BLACK   0x000000u
#define D_GRAY    0x808080u

/* ================================================================
 * Панель задач и меню "Пуск"
 * ================================================================ */

#define TASK_H     28
#define START_W    72

static BOOLEAN g_menu_open = FALSE;

/* пункты меню "Пуск": имя, значок, что открыть (apps_desktop_launch) */
static const struct { const char *label; const char *icon; const char *cmd; } g_menu[] = {
    { "Терминал",    "terminal", "terminal" },
    { "Блокнот",     "notepad",  "notepad"  },
    { "Проводник",   "folder",   "explorer" },
    { "Калькулятор", "calc",     "calc"     },
    { "Часы",        "clock",    "clock"    },
    { "Рисование",   "paint",    "paint"    },
    { "Сапёр",       "mine",     "mine"     },
    { "Жизнь",       "app",      "life"     },
    { "О системе",   "logo",     "about"    },
    { "Выход",       "shutdown", "exit"     },
};
#define MENU_N     (sizeof(g_menu) / sizeof(g_menu[0]))
#define MENU_ITEM_H 20
#define MENU_W     160
#define MENU_SIDE  20          /* синяя боковая полоса с надписью MyOS */


/* ================================================================
 * Курсор-стрелка (0 прозр., 1 чёрный контур, 2 белая заливка)
 * ================================================================ */

#define CUR_W 12
#define CUR_H 19
static const UINT8 g_cursor[CUR_H][CUR_W] = {
    {1,0,0,0,0,0,0,0,0,0,0,0},
    {1,1,0,0,0,0,0,0,0,0,0,0},
    {1,2,1,0,0,0,0,0,0,0,0,0},
    {1,2,2,1,0,0,0,0,0,0,0,0},
    {1,2,2,2,1,0,0,0,0,0,0,0},
    {1,2,2,2,2,1,0,0,0,0,0,0},
    {1,2,2,2,2,2,1,0,0,0,0,0},
    {1,2,2,2,2,2,2,1,0,0,0,0},
    {1,2,2,2,2,2,2,2,1,0,0,0},
    {1,2,2,2,2,2,2,2,2,1,0,0},
    {1,2,2,2,2,2,1,1,1,1,1,0},
    {1,2,2,1,2,2,1,0,0,0,0,0},
    {1,2,1,0,1,2,2,1,0,0,0,0},
    {1,1,0,0,1,2,2,1,0,0,0,0},
    {1,0,0,0,0,1,2,2,1,0,0,0},
    {0,0,0,0,0,1,2,2,1,0,0,0},
    {0,0,0,0,0,0,1,2,2,1,0,0},
    {0,0,0,0,0,0,1,2,2,1,0,0},
    {0,0,0,0,0,0,0,1,1,0,0,0},
};


/* ================================================================
 * Список окон
 * ================================================================ */

static WIN *win_top_at(INT32 x, INT32 y);

static UINTN win_count_visible(void)
{
    UINTN n = 0;

    for (UINTN i = 0; i < WM_MAX_WINDOWS; i++)
        if (g_windows[i].used)
            n++;

    return n;
}

/* Освободить буфер окна */
static void win_free_buf(WIN *w)
{
    if (w->buf == NULL)
        return;

    if (w->buf_phys != 0)
        pmm_free_pages(w->buf_phys, w->buf_pages);   /* окно программы */
    else
        kfree(w->buf);

    w->buf = NULL;
    w->buf_phys = 0;
}

WIN *wm_open(const WIN_CLASS *cls, INT32 cw, INT32 ch, const char *title, void *state)
{
    if (!g_wm_running)
        return NULL;

    if (cw < 40) cw = 40;
    if (ch < 30) ch = 30;
    if (cw > (INT32)SCR_W - 8) cw = (INT32)SCR_W - 8;
    if (ch > (INT32)SCR_H - TASK_H - WIN_TITLE_H - 8) ch = (INT32)SCR_H - TASK_H - WIN_TITLE_H - 8;

    WIN *w = NULL;

    for (UINTN i = 0; i < WM_MAX_WINDOWS; i++)
        if (!g_windows[i].used) {
            w = &g_windows[i];
            break;
        }

    if (w == NULL)
        return NULL;

    raw_zero_mem((volatile UINT8 *)w, sizeof(*w));

    w->buf = (UINT32 *)kmalloc((UINTN)cw * (UINTN)ch * 4u);

    if (w->buf == NULL)
        return NULL;

    for (INTN i = 0; i < cw * ch; i++)
        w->buf[i] = D_FACE;

    w->used = TRUE;
    w->id = g_next_win_id++;
    w->cw = cw;
    w->ch = ch;
    w->cls = cls;
    w->state = state;
    w->z = ++g_z_top;
    w->want_redraw = TRUE;

    ksnprintf(w->title, sizeof(w->title), "%s", title ? title : (cls ? cls->name : "Окно"));

    /* каскадом от левого верхнего угла */
    UINTN n = win_count_visible();

    w->x = 24 + (INT32)((n % 6) * 24);
    w->y = TASK_H + 16 + (INT32)((n % 6) * 22);

    if (w->x + WIN_FRAME_W(cw) > (INT32)SCR_W)
        w->x = (INT32)SCR_W - WIN_FRAME_W(cw);
    if (w->y + WIN_FRAME_H(ch) > (INT32)SCR_H - TASK_H)
        w->y = TASK_H + 8;

    wm_focus(w);
    g_dirty = TRUE;

    klog("wm: window %u '%s' (%dx%d) opened\n", w->id, w->title, cw, ch);

    return w;
}

void wm_close(WIN *w)
{
    if (w == NULL || !w->used)
        return;

    if (w->cls && w->cls->close)
        w->cls->close(w);

    if (w->proc != NULL)
        w->proc = NULL;

    win_free_buf(w);

    if (w->state != NULL && w->cls != NULL)
        kfree(w->state);

    w->used = FALSE;

    if (g_focus == w)
        g_focus = NULL;
    if (g_drag == w)
        g_drag = NULL;

    /* фокус - верхнему из оставшихся */
    if (g_focus == NULL) {
        WIN *top = NULL;
        for (UINTN i = 0; i < WM_MAX_WINDOWS; i++)
            if (g_windows[i].used && (top == NULL || g_windows[i].z > top->z))
                top = &g_windows[i];
        if (top)
            wm_focus(top);
    }

    g_dirty = TRUE;
}

void wm_invalidate(WIN *w)
{
    if (w != NULL && w->used) {
        w->want_redraw = TRUE;
        g_dirty = TRUE;
    }
}

void wm_set_title(WIN *w, const char *title)
{
    if (w != NULL && w->used) {
        ksnprintf(w->title, sizeof(w->title), "%s", title);
        g_dirty = TRUE;
    }
}

GFX wm_client_gfx(WIN *w)
{
    GFX g;

    gfx_init(&g, w->buf, (UINT32)w->cw, (UINT32)w->ch, (UINT32)w->cw);
    return g;
}

void wm_focus(WIN *w)
{
    if (w == NULL || !w->used)
        return;

    if (g_focus == w && w->z == g_z_top)
        return;

    if (g_focus != NULL && g_focus != w) {
        struct myos_event e = { EV_FOCUS, 0, 0, 0, 0, 0, 0, 0 };
        if (g_focus->proc)
            wm_push_event(g_focus, &e);
    }

    g_focus = w;
    w->z = ++g_z_top;
    w->minimized = FALSE;

    struct myos_event e = { EV_FOCUS, 1, 0, 0, 0, 0, 0, 0 };
    if (w->proc)
        wm_push_event(w, &e);

    g_dirty = TRUE;
}

WIN *wm_focused(void)
{
    return g_focus;
}

BOOLEAN wm_push_event(WIN *w, struct myos_event *e)
{
    UINTN next = (w->ev_tail + 1u) % 32u;

    if (next == w->ev_head)
        return FALSE;                /* очередь полна */

    w->evq[w->ev_tail] = *e;
    w->ev_tail = next;

    if (w->proc)
        sched_wake_all(&w->evq);

    return TRUE;
}

void wm_request_stop(void)
{
    g_stop = TRUE;
}

void wm_invalidate_desktop(void)
{
    g_dirty = TRUE;
}


/* ================================================================
 * Отрисовка рамки окна и кадра
 * ================================================================ */

static void draw_window_frame(WIN *w, BOOLEAN active)
{
    GFX *g = &g_screen;
    INT32 fw = WIN_FRAME_W(w->cw);
    INT32 fh = WIN_FRAME_H(w->ch);

    gfx_fill(g, w->x, w->y, fw, fh, D_FACE);
    gfx_bevel(g, w->x, w->y, fw, fh, TRUE);

    /* заголовок: активное - синий, неактивное - серое */
    UINT32 bar = active ? D_NAVY : D_GRAY;

    gfx_fill(g, w->x + WIN_BORDER, w->y + WIN_BORDER,
             w->cw, WIN_TITLE_H, bar);

    gfx_text_bold(g, w->x + WIN_BORDER + 4, w->y + WIN_BORDER + 2, w->title,
                  active ? D_WHITE : D_FACE);

    /* кнопка закрытия */
    INT32 bx = w->x + fw - WIN_BORDER - 18;
    INT32 by = w->y + WIN_BORDER + 2;

    gfx_button(g, bx, by, 16, 16, FALSE);
    gfx_glyph(g, bx + 4, by, 0x00D7 /* × */, D_BLACK);

    /* содержимое */
    gfx_blit(g, w->x + WIN_BORDER, w->y + WIN_BORDER + WIN_TITLE_H,
             w->buf, w->cw, w->ch, (UINT32)w->cw);
}

static void draw_taskbar(void)
{
    GFX *g = &g_screen;
    INT32 y = (INT32)SCR_H - TASK_H;

    gfx_fill(g, 0, y, (INT32)SCR_W, TASK_H, D_FACE);
    gfx_bevel(g, 0, y, (INT32)SCR_W, TASK_H, TRUE);

    /* кнопка "Пуск" со значком MyOS */
    gfx_button(g, 3, y + 3, START_W, TASK_H - 6, g_menu_open);
    INT32 sx = g_menu_open ? 6 : 5;
    gfx_icon(g, sx + 2, y + 4, gui_icon("logo"), 1, FALSE);
    gfx_text_bold(g, sx + 22, y + 6, "Пуск", D_BLACK);

    /* кнопки окон (порядок - по id, устойчивый) */
    INT32 bx = START_W + 10;
    INT32 bw = 150;

    for (UINTN k = 0; k < g_zi_n; k++) {

        WIN *w = &g_windows[g_zi_idx[k]];
        BOOLEAN active = (w == g_focus) && !w->minimized;

        gfx_button(g, bx, y + 4, bw, TASK_H - 8, active);
        gfx_icon(g, bx + 3, y + 5, w->cls ? w->cls->icon : gui_icon("app"), 1, FALSE);
        gfx_text_fit(g, bx + 22, y + 7, w->title, D_BLACK, 15);

        bx += bw + 4;
        if (bx + bw > (INT32)SCR_W - 90)
            break;
    }

    /* часы справа + индикатор раскладки */
    EFI_TIME t;
    char clock[16];

    if (rtc_read(&t)) {
        tz_to_local(&t);
        ksnprintf(clock, sizeof(clock), "%02u:%02u", t.Hour, t.Minute);
        INT32 cw = gfx_text_width(clock);
        gfx_button(g, (INT32)SCR_W - cw - 16, y + 4, cw + 12, TASK_H - 8, FALSE);
        gfx_text(g, (INT32)SCR_W - cw - 10, y + 7, clock, D_BLACK);
    }

    const char *lang = g_kbd_layout ? "РУ" : "EN";
    INT32 lw = gfx_text_width(lang);
    gfx_button(g, (INT32)SCR_W - 90, y + 4, lw + 12, TASK_H - 8, FALSE);
    gfx_text(g, (INT32)SCR_W - 84, y + 7, lang, D_BLACK);
}

static void draw_start_menu(void)
{
    GFX *g = &g_screen;
    INT32 h = (INT32)MENU_N * MENU_ITEM_H + 6;
    INT32 x = 3;
    INT32 y = (INT32)SCR_H - TASK_H - h;

    gfx_fill(g, x, y, MENU_W, h, D_FACE);
    gfx_bevel(g, x, y, MENU_W, h, TRUE);

    /* синяя боковая полоса с вертикальной надписью MyOS */
    gfx_fill(g, x + 3, y + 3, MENU_SIDE, h - 6, D_NAVY);

    const char *word = "MyOS 95";
    INT32 ty = y + h - 12;

    for (const char *s = word; *s; s++) {
        gfx_glyph_up(g, x + 5, ty, (UINT8)*s, D_WHITE);
        ty -= FONT_W;
    }

    INT32 ix = x + 3 + MENU_SIDE + 2;

    for (UINTN i = 0; i < MENU_N; i++) {

        INT32 iy = y + 3 + (INT32)i * MENU_ITEM_H;
        BOOLEAN hover = g_mx >= ix && g_mx < x + MENU_W - 3 &&
                        g_my >= iy && g_my < iy + MENU_ITEM_H;

        if (hover)
            gfx_fill(g, ix, iy, MENU_W - MENU_SIDE - 8, MENU_ITEM_H, D_NAVY);

        gfx_icon(g, ix + 2, iy + 2, gui_icon(g_menu[i].icon), 1, FALSE);
        gfx_text(g, ix + 22, iy + 3, g_menu[i].label, hover ? D_WHITE : D_BLACK);
    }
}

/* Собрать весь кадр в back_buf */
static void compose(void)
{
    GFX *g = &g_screen;

    gfx_noclip(g);
    gfx_fill(g, 0, 0, (INT32)SCR_W, (INT32)SCR_H, D_TEAL);

    /* ярлыки рабочего стола */
    apps_draw_shortcuts(g);

    /* окна снизу вверх по z (сортировка индексов пузырьком) */
    UINT32 order[WM_MAX_WINDOWS];
    UINTN nn = 0;

    for (UINTN i = 0; i < WM_MAX_WINDOWS; i++)
        if (g_windows[i].used)
            order[nn++] = (UINT32)i;

    for (UINTN a = 0; a + 1 < nn; a++)
        for (UINTN b = 0; b + 1 < nn - a; b++)
            if (g_windows[order[b]].z > g_windows[order[b + 1]].z) {
                UINT32 t = order[b]; order[b] = order[b + 1]; order[b + 1] = t;
            }

    for (UINTN k = 0; k < nn; k++) {

        WIN *w = &g_windows[order[k]];

        /* родное окно перерисовать в свой буфер, если помечено */
        if (w->cls && w->cls->paint && w->want_redraw) {
            GFX cg = wm_client_gfx(w);
            w->cls->paint(w, &cg);
            w->want_redraw = FALSE;
        }

        gfx_noclip(g);
        draw_window_frame(w, w == g_focus);
    }

    gfx_noclip(g);
    draw_taskbar();

    if (g_menu_open)
        draw_start_menu();
}

/* back_buf -> экран: только изменившиеся точки, курсор поверх */
static void flush(void)
{
    volatile UINT32 *fb = g_kfb;
    BOOLEAN rgb = (SCR_FMT == PixelRedGreenBlueReserved8BitPerColor);

    for (UINT32 y = 0; y < SCR_H; y++) {

        UINTN base = (UINTN)y * SCR_STRIDE;
        INT32 crow = (INT32)y - g_my;

        for (UINT32 x = 0; x < SCR_W; x++) {

            UINT32 v = g_back[base + x];

            /* курсор поверх кадра */
            INT32 ccol = (INT32)x - g_mx;

            if (crow >= 0 && crow < CUR_H && ccol >= 0 && ccol < CUR_W) {
                UINT8 c = g_cursor[crow][ccol];
                if (c == 1) v = 0x000000u;
                else if (c == 2) v = 0xFFFFFFu;
            }

            UINT32 packed = rgb
                ? ((v & 0xFF0000u) >> 16) | (v & 0x00FF00u) | ((v & 0x0000FFu) << 16)
                : v;

            if (g_shadow[base + x] != packed) {
                fb[base + x] = packed;
                g_shadow[base + x] = packed;
            }
        }
    }
}


/* ================================================================
 * Ввод
 * ================================================================ */

static WIN *win_top_at(INT32 x, INT32 y)
{
    WIN *best = NULL;

    for (UINTN i = 0; i < WM_MAX_WINDOWS; i++) {

        WIN *w = &g_windows[i];

        if (!w->used)
            continue;

        if (x >= w->x && x < w->x + WIN_FRAME_W(w->cw) &&
            y >= w->y && y < w->y + WIN_FRAME_H(w->ch))
            if (best == NULL || w->z > best->z)
                best = w;
    }

    return best;
}

/* точка в области содержимого окна -> координаты внутри (client) */
static BOOLEAN win_client_point(WIN *w, INT32 x, INT32 y, INT32 *cx, INT32 *cy)
{
    INT32 ox = w->x + WIN_BORDER;
    INT32 oy = w->y + WIN_BORDER + WIN_TITLE_H;

    if (x >= ox && x < ox + w->cw && y >= oy && y < oy + w->ch) {
        *cx = x - ox;
        *cy = y - oy;
        return TRUE;
    }

    return FALSE;
}

/* доставить событие окну: программе - в очередь, родному - через event */
static void deliver(WIN *w, struct myos_event *e)
{
    if (w == NULL)
        return;

    if (w->proc)
        wm_push_event(w, e);
    else if (w->cls && w->cls->event)
        w->cls->event(w, e);
}

static void on_mouse_down(void)
{
    /* меню "Пуск" открыто */
    if (g_menu_open) {

        INT32 mh = (INT32)MENU_N * MENU_ITEM_H + 6;
        INT32 mx = 3, my0 = (INT32)SCR_H - TASK_H - mh;
        INT32 ix = mx + 3 + MENU_SIDE + 2;

        if (g_mx >= ix && g_mx < mx + MENU_W - 3 && g_my >= my0 + 3 && g_my < my0 + mh - 3) {
            UINTN idx = (UINTN)(g_my - my0 - 3) / MENU_ITEM_H;
            if (idx < MENU_N)
                apps_desktop_launch(g_menu[idx].cmd);
        }

        g_menu_open = FALSE;
        g_dirty = TRUE;
        return;
    }

    INT32 ty = (INT32)SCR_H - TASK_H;

    /* панель задач */
    if (g_my >= ty) {

        if (g_mx >= 3 && g_mx < 3 + START_W) {
            g_menu_open = TRUE;
            g_dirty = TRUE;
            return;
        }

        /* кнопки окон */
        INT32 bx = START_W + 10, bw = 150;

        for (UINTN k = 0; k < g_zi_n; k++) {
            WIN *w = &g_windows[g_zi_idx[k]];
            if (g_mx >= bx && g_mx < bx + bw) {
                if (w == g_focus)
                    w->minimized = !w->minimized;
                else
                    wm_focus(w);
                g_dirty = TRUE;
                return;
            }
            bx += bw + 4;
            if (bx + bw > (INT32)SCR_W - 90) break;
        }

        return;
    }

    WIN *w = win_top_at(g_mx, g_my);

    if (w == NULL) {

        /* ярлык рабочего стола? */
        apps_shortcut_click(g_mx, g_my);
        return;
    }

    wm_focus(w);

    /* крестик */
    INT32 fw = WIN_FRAME_W(w->cw);
    INT32 bx = w->x + fw - WIN_BORDER - 18;
    INT32 by = w->y + WIN_BORDER + 2;

    if (g_mx >= bx && g_mx < bx + 16 && g_my >= by && g_my < by + 16) {
        struct myos_event e = { EV_CLOSE, 0, 0, 0, 0, 0, 0, 0 };
        if (w->proc)
            deliver(w, &e);          /* программа сама решит закрыться */
        else
            wm_close(w);
        return;
    }

    /* заголовок - тащить */
    if (g_my < w->y + WIN_BORDER + WIN_TITLE_H) {
        g_drag = w;
        g_drag_dx = g_mx - w->x;
        g_drag_dy = g_my - w->y;
        return;
    }

    /* содержимое - событие окну */
    INT32 cx, cy;

    if (win_client_point(w, g_mx, g_my, &cx, &cy)) {
        struct myos_event e = { EV_DOWN, 0, 0, cx, cy, g_mouse_left ? 1u : 0u, 0, 0 };
        e.buttons = 1;
        deliver(w, &e);
    }
}

static void on_mouse_up(void)
{
    if (g_drag) {
        g_drag = NULL;
        return;
    }

    WIN *w = win_top_at(g_mx, g_my);
    INT32 cx, cy;

    if (w && win_client_point(w, g_mx, g_my, &cx, &cy)) {
        struct myos_event e = { EV_UP, 0, 0, cx, cy, 0, 0, 0 };
        deliver(w, &e);
    }
}

static void on_mouse_move(void)
{
    if (g_drag) {

        g_drag->x = g_mx - g_drag_dx;
        g_drag->y = g_my - g_drag_dy;

        if (g_drag->y < 0)
            g_drag->y = 0;
        if (g_drag->y > (INT32)SCR_H - TASK_H - WIN_TITLE_H)
            g_drag->y = (INT32)SCR_H - TASK_H - WIN_TITLE_H;

        g_dirty = TRUE;
        return;
    }

    /* движение над содержимым активного окна */
    WIN *w = win_top_at(g_mx, g_my);
    INT32 cx, cy;

    if (w && win_client_point(w, g_mx, g_my, &cx, &cy)) {
        struct myos_event e = { EV_MOVE, 0, 0, cx, cy, g_mouse_left ? 1u : 0u, 0, 0 };
        deliver(w, &e);
    }
}

static void on_key(EFI_INPUT_KEY *key)
{
    /* Esc при открытом меню - закрыть меню */
    if (g_menu_open && key->ScanCode == KEY_ESC) {
        g_menu_open = FALSE;
        g_dirty = TRUE;
        return;
    }

    if (g_focus == NULL)
        return;

    struct myos_event e = { EV_KEY, key->UnicodeChar, key->ScanCode, 0, 0, 0, 0, 0 };

    deliver(g_focus, &e);
}


/* ================================================================
 * Главный цикл
 * ================================================================ */

void wm_start(EFI_SYSTEM_TABLE *st)
{
    (void)st;

    if (g_kfb == NULL) {
        print(g_st->ConOut, "GUI unavailable: no framebuffer.\n");
        return;
    }

    SCR_W = g_kfb_w;
    SCR_H = g_kfb_h;
    SCR_STRIDE = g_kfb_stride;
    SCR_FMT = g_kfb_fmt;

    UINTN npx = (UINTN)SCR_STRIDE * SCR_H;

    g_back = (UINT32 *)kmalloc(npx * 4u);
    g_shadow = (UINT32 *)kmalloc(npx * 4u);

    if (g_back == NULL || g_shadow == NULL) {
        if (g_back) kfree(g_back);
        if (g_shadow) kfree(g_shadow);
        print(g_st->ConOut, "GUI unavailable: not enough memory for the screen buffers.\n");
        return;
    }

    gfx_init(&g_screen, g_back, SCR_W, SCR_H, SCR_STRIDE);

    for (UINTN i = 0; i < npx; i++)
        g_shadow[i] = 0xAA55AA55u;        /* заведомо не совпадёт - первый кадр целиком */

    g_mx = (INT32)SCR_W / 2;
    g_my = (INT32)SCR_H / 2;
    g_wm_running = TRUE;
    g_stop = FALSE;
    g_menu_open = FALSE;
    g_focus = NULL;
    g_drag = NULL;

    klog("wm: started, %ux%u\n", SCR_W, SCR_H);

    /* первое окно - терминал, как в прошлом GUI */
    app_open_about();

    UINTN idle = 0;

    while (!g_stop) {

        kernel_poll_input();

        /* закрыть окна, чьи программы завершились */
        for (UINTN i = 0; i < WM_MAX_WINDOWS; i++)
            if (g_windows[i].used && g_windows[i].dead) {
                g_windows[i].dead = FALSE;
                g_windows[i].proc = NULL;
                wm_close(&g_windows[i]);
            }

        BOOLEAN acted = FALSE;

        /* мышь */
        if (g_kmouse_present) {

            kx_lock();
            INT64 dx = g_kmouse_dx, dy = g_kmouse_dy, dz = g_kmouse_dz;
            UINT32 btn = g_kmouse_buttons;
            g_kmouse_dx = g_kmouse_dy = g_kmouse_dz = 0;
            kx_unlock();

            if (dx || dy) {
                g_mx += (INT32)dx;
                g_my += (INT32)dy;
                if (g_mx < 0) g_mx = 0;
                if (g_my < 0) g_my = 0;
                if (g_mx > (INT32)SCR_W - 1) g_mx = (INT32)SCR_W - 1;
                if (g_my > (INT32)SCR_H - 1) g_my = (INT32)SCR_H - 1;
                on_mouse_move();
                g_dirty = TRUE;
                acted = TRUE;
            }

            BOOLEAN left = (btn & 1u) != 0;

            if (left && !g_mouse_left) {
                g_mouse_left = TRUE;
                on_mouse_down();
                acted = TRUE;
            } else if (!left && g_mouse_left) {
                g_mouse_left = FALSE;
                on_mouse_up();
                acted = TRUE;
            }

            if (dz != 0 && g_focus) {
                struct myos_event e = { EV_WHEEL, 0, 0, 0, 0, 0, (INT32)dz, 0 };
                INT32 cx, cy;
                if (win_client_point(g_focus, g_mx, g_my, &cx, &cy)) {
                    e.x = cx; e.y = cy;
                }
                deliver(g_focus, &e);
                acted = TRUE;
            }
        }

        /* клавиатура */
        EFI_INPUT_KEY key;

        kx_lock();
        BOOLEAN got = kbd_dequeue(&key);
        kx_unlock();

        if (got) {
            on_key(&key);
            acted = TRUE;
            g_dirty = TRUE;        /* обновить и индикатор раскладки */
        }

        /* тик раз в ~100 мс (часы, анимации, окна программ) */
        static UINT64 last_tick = 0;

        if (g_kticks - last_tick >= 100) {
            last_tick = g_kticks;
            for (UINTN i = 0; i < WM_MAX_WINDOWS; i++)
                if (g_windows[i].used && g_windows[i].cls && g_windows[i].cls->tick)
                    g_windows[i].cls->tick(&g_windows[i]);
            g_dirty = TRUE;        /* обновить часы на панели задач */
        }

        if (g_dirty) {
            rebuild_taskbar_order();
            compose();
            flush();
            g_dirty = FALSE;
            idle = 0;
        } else if (acted) {
            flush();               /* курсор сдвинулся */
        } else {
            /* нечего делать - поспать, отдать процессор другим */
            idle++;
            sched_sleep_ms(idle > 20 ? 30 : 8);
        }
    }

    g_wm_running = FALSE;

    /* закрыть окна */
    for (UINTN i = 0; i < WM_MAX_WINDOWS; i++)
        if (g_windows[i].used)
            wm_close(&g_windows[i]);

    kfree(g_back);
    kfree(g_shadow);
    g_back = g_shadow = NULL;

    g_st->ConOut->ClearScreen(g_st->ConOut);
    set_color(g_st->ConOut, g_color);
    print(g_st->ConOut, "Left the desktop. Type 'start' to return.\n");
}
