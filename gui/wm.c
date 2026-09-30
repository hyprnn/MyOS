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
static UINT32 *g_wall = NULL;       /* обои (aero_wallpaper), рисуются один раз */
static UINT32  g_wall_w, g_wall_h;
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
    { "Браузер",     "web",      "browser"  },
    { "Блокнот",     "notepad",  "notepad"  },
    { "Проводник",   "folder",   "explorer" },
    { "Калькулятор", "calc",     "calc"     },
    { "Часы",        "clock",    "clock"    },
    { "Рисование",   "paint",    "paint"    },
    { "Сапёр",       "mine",     "mine"     },
    { "DOOM",        "doom",     "doom"     },
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

/*
 * Замок оконной системы (этап 10). Список окон меняют не только
 * композитор, но и программы (системные вызовы окон) и потоки
 * терминалов. Код ядра под "большим замком" всё равно может быть
 * прерван (держатель уступает замок ждущим раз в миллисекунду), и без
 * своего замка программа, закрыв окно, освобождала его буфер прямо
 * посреди перерисовки экрана - падение на гонке. Композитор держит
 * этот замок всю итерацию цикла; wm_open/wm_close/wm_set_title берут
 * его сами (замок рекурсивный - композитору можно звать их изнутри).
 */
KMUTEX g_wm_mutex = KMUTEX_INIT("wm");

static WIN *wm_open_locked(const WIN_CLASS *cls, INT32 cw, INT32 ch, const char *title, void *state);

WIN *wm_open(const WIN_CLASS *cls, INT32 cw, INT32 ch, const char *title, void *state)
{
    kmutex_lock(&g_wm_mutex);
    WIN *w = wm_open_locked(cls, cw, ch, title, state);
    kmutex_unlock(&g_wm_mutex);
    return w;
}

static WIN *wm_open_locked(const WIN_CLASS *cls, INT32 cw, INT32 ch, const char *title, void *state)
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

static void wm_close_locked(WIN *w);

void wm_close(WIN *w)
{
    kmutex_lock(&g_wm_mutex);
    wm_close_locked(w);
    kmutex_unlock(&g_wm_mutex);
}

static void wm_close_locked(WIN *w)
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
    kmutex_lock(&g_wm_mutex);

    if (w != NULL && w->used) {
        ksnprintf(w->title, sizeof(w->title), "%s", title);
        g_dirty = TRUE;
    }

    kmutex_unlock(&g_wm_mutex);
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

/*
 * Рамка окна в стиле Frutiger Aero: мягкая тень, стеклянная
 * полупрозрачная рамка (сквозь неё видны обои и окна позади) с
 * градиентом и бликом, скруглённый верх, тёмный контур и светлая
 * внутренняя кромка, красная глянцевая кнопка закрытия. Размеры -
 * прежние (WIN_BORDER, WIN_TITLE_H): программы и автотесты от них
 * зависят.
 */
#define CLOSE_W 30
#define CLOSE_H 17

static void close_rect(WIN *w, INT32 *bx, INT32 *by)
{
    *bx = w->x + WIN_FRAME_W(w->cw) - WIN_BORDER - CLOSE_W - 2;
    *by = w->y + 1;
}

static void draw_window_frame(WIN *w, BOOLEAN active)
{
    GFX *g = &g_screen;
    INT32 fw = WIN_FRAME_W(w->cw);
    INT32 fh = WIN_FRAME_H(w->ch);
    INT32 th = WIN_BORDER + WIN_TITLE_H;

    UINT32 top = active ? 0xBFE3F9u : 0xE3EBF1u;
    UINT32 mid = active ? 0x7DB8E6u : 0xC3D2DEu;
    UINT32 bot = active ? 0x4E8FCBu : 0xA9BCCBu;
    UINT32 alpha = active ? 205u : 185u;

    aero_shadow(g, w->x, w->y, fw, fh);

    /* матовое стекло: то, что позади заголовка и рамки, - размыть */
    aero_blur(g, w->x, w->y, fw, th, 4);
    aero_blur(g, w->x, w->y + th, WIN_BORDER, fh - th, 2);
    aero_blur(g, w->x + fw - WIN_BORDER, w->y + th, WIN_BORDER, fh - th, 2);
    aero_blur(g, w->x, w->y + fh - WIN_BORDER, fw, WIN_BORDER, 2);

    aero_panel(g, w->x, w->y, fw, th, top, mid, alpha, 8, 0);
    aero_panel(g, w->x, w->y + th, fw, fh - th, mid, bot, alpha, 0, 4);
    aero_gloss(g, w->x + 3, w->y + 2, fw - 6, WIN_TITLE_H / 2, active ? 120u : 90u);
    aero_outline(g, w->x, w->y, fw, fh, 0x16283Du, 190, 8, 4);
    aero_outline(g, w->x + 1, w->y + 1, fw - 2, fh - 2, 0xFFFFFFu, 110, 7, 3);

    /* заголовок: тёмный текст с белой "подсветкой" снизу-справа */
    INT32 bx, by;
    close_rect(w, &bx, &by);

    gfx_clip(g, w->x + WIN_BORDER, w->y, bx - w->x - WIN_BORDER - 4, th);
    gfx_text(g, w->x + WIN_BORDER + 7, w->y + WIN_BORDER + 3, w->title, 0xF4FAFFu);
    gfx_text(g, w->x + WIN_BORDER + 6, w->y + WIN_BORDER + 2, w->title,
             active ? 0x0C1D30u : 0x33414Eu);
    gfx_noclip(g);

    /* кнопка закрытия: красное стекло (у неактивного - блёклое) */
    aero_panel(g, bx, by, CLOSE_W, CLOSE_H, active ? 0xF0A090u : 0xE6D4D0u,
               active ? 0xC7372Au : 0xC9B2AEu, 245, 3, 3);
    aero_gloss(g, bx + 1, by + 1, CLOSE_W - 2, CLOSE_H / 2, 110);
    aero_outline(g, bx, by, CLOSE_W, CLOSE_H, 0x5A1A12u, 200, 3, 3);
    gfx_glyph(g, bx + CLOSE_W / 2 - 4, by, 0x00D7 /* × */, 0xFFFFFFu);

    /* содержимое (буфера нет - окно как раз закрывается) */
    if (w->buf != NULL)
        gfx_blit(g, w->x + WIN_BORDER, w->y + WIN_BORDER + WIN_TITLE_H,
                 w->buf, w->cw, w->ch, (UINT32)w->cw);
}

/* "Пилюля" стекла на панели задач (кнопки окон, значки справа) */
static void glass_pill(GFX *g, INT32 x, INT32 y, INT32 w, INT32 h, BOOLEAN lit)
{
    aero_panel(g, x, y, w, h, lit ? 0x8FC8F0u : 0x5A7896u, lit ? 0x2F6FB0u : 0x243A52u,
               lit ? 220u : 150u, 4, 4);
    aero_gloss(g, x + 1, y + 1, w - 2, h / 2, lit ? 90u : 55u);
    aero_outline(g, x, y, w, h, 0x0A1420u, 170, 4, 4);
}

static void draw_taskbar(void)
{
    GFX *g = &g_screen;
    INT32 y = (INT32)SCR_H - TASK_H;

    /* тёмное стекло: сквозь панель видны обои; сверху светлая кромка */
    aero_panel(g, 0, y, (INT32)SCR_W, TASK_H, 0x3A5570u, 0x0B1624u, 200, 0, 0);
    aero_gloss(g, 0, y, (INT32)SCR_W, TASK_H / 2, 45);
    aero_panel(g, 0, y, (INT32)SCR_W, 1, 0xFFFFFFu, 0xFFFFFFu, 120, 0, 0);

    /* кнопка "Пуск": глянцевый шарик со знаком MyOS и надпись
       (нажимается вся полоса 3..3+START_W, как и раньше) */
    aero_orb(g, 17, y + TASK_H / 2, 11, g_menu_open ? 0x2E9E3Eu : 0x1F7FD0u,
             g_menu_open ? 0xB8F5A0u : 0x9FE3FFu);
    gfx_icon(g, 9, y + TASK_H / 2 - 8, gui_icon("logo"), 1, FALSE);
    aero_text_glow(g, 33, y + 6, "Пуск", D_WHITE, 0x0A2340u);

    /* кнопки окон (порядок - по id, устойчивый) */
    INT32 bx = START_W + 10;
    INT32 bw = 150;

    for (UINTN k = 0; k < g_zi_n; k++) {

        WIN *w = &g_windows[g_zi_idx[k]];
        BOOLEAN active = (w == g_focus) && !w->minimized;

        glass_pill(g, bx, y + 3, bw, TASK_H - 6, active);
        gfx_icon(g, bx + 3, y + 5, w->cls ? w->cls->icon : w->icon ? w->icon : gui_icon("app"),
                 1, FALSE);
        gfx_text_fit(g, bx + 22, y + 6, w->title, D_WHITE, 15);

        bx += bw + 4;
        if (bx + bw > (INT32)SCR_W - 280)
            break;
    }

    /* сеть (этап 8): адрес или "нет сети" */
    char net[24];
    net_gui_indicator(net, sizeof(net));
    INT32 nw = gfx_text_width(net);
    glass_pill(g, (INT32)SCR_W - 96 - nw - 12, y + 3, nw + 12, TASK_H - 6, FALSE);
    gfx_text(g, (INT32)SCR_W - 96 - nw - 6, y + 7, net, D_WHITE);

    /* батарея (этап 9, ACPI): значок-"батарейка" и проценты - левее
       сети; у компьютера без батареи (и в QEMU) значка нет. Сами
       числа раз в 15 секунд обновляет поток power (acpi_dev.c). */
    char bat[8];
    BOOLEAN charging = FALSE;
    INT32 right = (INT32)SCR_W - 96 - nw - 12 - 4;    /* правый край следующего значка */

    if (acpi_battery_brief(bat, sizeof(bat), &charging)) {

        INT32 tw = gfx_text_width(bat);
        INT32 bw2 = 22 + 4 + tw + 12;
        INT32 bx2 = right - bw2;

        right = bx2 - 4;
        INT32 iy = y + 9;
        UINT32 pct = 0;

        for (const char *c = bat; *c >= '0' && *c <= '9'; c++)
            pct = pct * 10u + (UINT32)(*c - '0');

        glass_pill(g, bx2, y + 3, bw2, TASK_H - 6, FALSE);

        /* корпус 18x10 с "носиком" справа, внутри - заливка по заряду:
           зелёная, красная если мало (15% и меньше) */
        INT32 ix = bx2 + 6;
        gfx_fill(g, ix, iy, 18, 1, D_WHITE);
        gfx_fill(g, ix, iy + 9, 18, 1, D_WHITE);
        gfx_fill(g, ix, iy, 1, 10, D_WHITE);
        gfx_fill(g, ix + 17, iy, 1, 10, D_WHITE);
        gfx_fill(g, ix + 18, iy + 3, 2, 4, D_WHITE);

        INT32 fill = (INT32)(pct > 100u ? 100u : pct) * 14 / 100;
        if (fill < 1)
            fill = 1;
        gfx_fill(g, ix + 2, iy + 2, fill, 6, pct <= 15u ? 0xD02020u : 0x20A020u);

        /* заряжается - "молния" поверх: жёлтый зигзаг */
        if (charging) {
            gfx_fill(g, ix + 8, iy + 1, 2, 4, 0xF0D000u);
            gfx_fill(g, ix + 7, iy + 4, 4, 2, 0xF0D000u);
            gfx_fill(g, ix + 8, iy + 5, 2, 4, 0xF0D000u);
        }

        gfx_text(g, ix + 22 + 4, y + 7, bat, D_WHITE);
    }

    /* звук (этап 10): динамик и громкость; без звука - красный крест */
    if (g_hda.ok) {

        char vol[8];
        ksnprintf(vol, sizeof(vol), "%u%%", g_hda.volume);

        INT32 tw = gfx_text_width(vol);
        INT32 sw = 6 + 12 + 4 + tw + 6;
        INT32 sx = right - sw;
        INT32 iy = y + 8;

        glass_pill(g, sx, y + 3, sw, TASK_H - 6, FALSE);

        /* динамик: "коробочка" и раструб */
        INT32 ix = sx + 6;
        gfx_fill(g, ix, iy + 4, 3, 5, D_WHITE);
        for (INT32 k = 0; k < 4; k++)
            gfx_fill(g, ix + 3 + k, iy + 3 - k, 1, 7 + 2 * k, D_WHITE);

        if (g_hda.muted || g_hda.volume == 0) {
            for (INT32 k = 0; k < 5; k++) {
                gfx_fill(g, ix + 8 + k, iy + 4 + k, 1, 1, 0xD02020u);
                gfx_fill(g, ix + 12 - k, iy + 4 + k, 1, 1, 0xD02020u);
            }
        } else {
            /* "волны" - по громкости */
            if (g_hda.volume > 0)
                gfx_fill(g, ix + 9, iy + 5, 1, 3, D_WHITE);
            if (g_hda.volume > 50)
                gfx_fill(g, ix + 11, iy + 3, 1, 7, D_WHITE);
        }

        gfx_text(g, ix + 12 + 4, y + 7, vol, g_hda.muted ? 0x9AA8B6u : D_WHITE);
    }

    /* часы справа + индикатор раскладки */
    EFI_TIME t;
    char clock[16];

    if (rtc_read(&t)) {
        tz_to_local(&t);
        ksnprintf(clock, sizeof(clock), "%02u:%02u", t.Hour, t.Minute);
        INT32 cw = gfx_text_width(clock);
        glass_pill(g, (INT32)SCR_W - cw - 16, y + 3, cw + 12, TASK_H - 6, FALSE);
        gfx_text(g, (INT32)SCR_W - cw - 10, y + 7, clock, D_WHITE);
    }

    const char *lang = g_kbd_layout ? "РУ" : "EN";
    INT32 lw = gfx_text_width(lang);
    glass_pill(g, (INT32)SCR_W - 90, y + 3, lw + 12, TASK_H - 6, FALSE);
    gfx_text(g, (INT32)SCR_W - 84, y + 7, lang, D_WHITE);
}

/* "Отпечаток" того, что показывает панель задач справа: сменился -
   панель надо перерисовать */
static UINT64 taskbar_signature(void)
{
    UINT64 h = 1469598103934665603ull;
    char buf[48];
    EFI_TIME t;

#define SIG_MIX(v) (h = (h ^ (UINT64)(v)) * 1099511628211ull)

    if (rtc_read(&t)) {
        SIG_MIX(t.Hour);
        SIG_MIX(t.Minute);
    }

    SIG_MIX(g_hda.volume);
    SIG_MIX(g_hda.muted);
    SIG_MIX(g_kbd_layout);

    net_gui_indicator(buf, sizeof(buf));
    for (const char *c = buf; *c; c++)
        SIG_MIX(*c);

    BOOLEAN ch = FALSE;
    if (acpi_battery_brief(buf, sizeof(buf), &ch)) {
        for (const char *c = buf; *c; c++)
            SIG_MIX(*c);
        SIG_MIX(ch);
    }

#undef SIG_MIX

    return h;
}

static void draw_start_menu(void)
{
    GFX *g = &g_screen;
    INT32 h = (INT32)MENU_N * MENU_ITEM_H + 6;
    INT32 x = 3;
    INT32 y = (INT32)SCR_H - TASK_H - h;

    /* светлое стекло со скруглением, тень */
    aero_shadow(g, x, y, MENU_W, h);
    aero_panel(g, x, y, MENU_W, h, 0xF4FAFFu, 0xCFE4F5u, 238, 7, 7);
    aero_outline(g, x, y, MENU_W, h, 0x1E3550u, 200, 7, 7);
    aero_outline(g, x + 1, y + 1, MENU_W - 2, h - 2, 0xFFFFFFu, 150, 6, 6);

    /* боковая полоса: насыщенная лазурь с вертикальной надписью MyOS */
    aero_panel(g, x + 3, y + 3, MENU_SIDE, h - 6, 0x1F6FC0u, 0x0E3A78u, 245, 4, 4);
    aero_gloss(g, x + 3, y + 3, MENU_SIDE / 2, h - 6, 60);

    const char *word = "MyOS Aero";
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

        /* наведённый пункт - голубая глянцевая подсветка */
        if (hover) {
            aero_panel(g, ix, iy, MENU_W - MENU_SIDE - 8, MENU_ITEM_H, 0xD8EFFFu, 0x8CC6F2u, 230, 3, 3);
            aero_outline(g, ix, iy, MENU_W - MENU_SIDE - 8, MENU_ITEM_H, 0x3C7FC0u, 200, 3, 3);
        }

        gfx_icon(g, ix + 2, iy + 2, gui_icon(g_menu[i].icon), 1, FALSE);
        gfx_text(g, ix + 22, iy + 3, g_menu[i].label, 0x0C1D30u);
    }
}

/* Собрать весь кадр в back_buf */
static void compose(void)
{
    GFX *g = &g_screen;

    gfx_noclip(g);

    if (g_wall != NULL)
        gfx_blit(g, 0, 0, g_wall, (INT32)SCR_W, (INT32)SCR_H, SCR_W);
    else
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
    INT32 bx, by;
    close_rect(w, &bx, &by);

    if (g_mx >= bx && g_mx < bx + CLOSE_W && g_my >= by && g_my < by + CLOSE_H) {
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

    /* открыто меню "Пуск" - подсветка пункта под мышью */
    if (g_menu_open)
        g_dirty = TRUE;

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

    /* модификаторы - как зажаты сейчас (HID: бит 0/4 Ctrl, 1/5 Shift,
       2/6 Alt): программам вроде браузера нужны Ctrl+C/V/A */
    UINT8 hid = (UINT8)(g_kbd_usb_mods | g_kbd_ps2_mods);
    UINT32 mods = ((hid & 0x11u) ? MYOS_MOD_CTRL : 0u) |
                  ((hid & 0x22u) ? MYOS_MOD_SHIFT : 0u) |
                  ((hid & 0x44u) ? MYOS_MOD_ALT : 0u);
    struct myos_event e = { EV_KEY, key->UnicodeChar, key->ScanCode, 0, 0, 0, 0, mods };

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

    /* обои Frutiger Aero - один раз (при следующем start - те же) */
    if (g_wall == NULL || g_wall_w != SCR_W || g_wall_h != SCR_H) {
        if (g_wall)
            kfree(g_wall);
        g_wall = (UINT32 *)kmalloc((UINTN)SCR_W * SCR_H * 4u);
        if (g_wall) {
            UINT64 t0 = g_kticks;
            aero_wallpaper(g_wall, SCR_W, SCR_H);
            g_wall_w = SCR_W;
            g_wall_h = SCR_H;
            klog("wm: wallpaper drawn in %u ms\n", (UINT32)(g_kticks - t0));
        }
    }

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

    /* старые "сырые" клавиши (набранные в консоли) - не окнам */
    {
        UINT8 u;
        BOOLEAN d;
        while (kbd_raw_dequeue(&u, &d))
            ;
    }

    /* первое окно - "О системе" (программа /bin/about, этап 10) */
    app_open_about();

    UINTN idle = 0;

    while (!g_stop) {

        kmutex_lock(&g_wm_mutex);

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

        /* физические клавиши (нажата/отпущена) - окну программы в
           фокусе: игре нужно знать, что клавишу ещё держат */
        UINT8 ru;
        BOOLEAN rdown;

        while (kbd_raw_dequeue(&ru, &rdown)) {
            if (g_focus && g_focus->proc && !g_focus->minimized) {
                struct myos_event e = { EV_RAWKEY, rdown ? 1u : 0u, ru, 0, 0, 0, 0, 0 };
                wm_push_event(g_focus, &e);
            }
            acted = TRUE;
        }

        /* тик раз в ~100 мс (часы, анимации, окна программ) */
        static UINT64 last_tick = 0;

        if (g_kticks - last_tick >= 100) {
            last_tick = g_kticks;
            for (UINTN i = 0; i < WM_MAX_WINDOWS; i++)
                if (g_windows[i].used && g_windows[i].cls && g_windows[i].cls->tick)
                    g_windows[i].cls->tick(&g_windows[i]);

            /* панель задач: перерисовать, только если на ней что-то
               поменялось (минута, звук, сеть, батарея, раскладка) -
               весь кадр со стеклом и тенями 10 раз в секунду "впустую"
               жёг процессор (и батарею ноутбука) */
            static UINT64 last_sig = 0;
            UINT64 sig = taskbar_signature();

            if (sig != last_sig) {
                last_sig = sig;
                g_dirty = TRUE;
            }
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
            kmutex_unlock(&g_wm_mutex);
            sched_sleep_ms(idle > 20 ? 30 : 8);
            continue;
        }

        kmutex_unlock(&g_wm_mutex);
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
