/*
 * libnsfb: поверхность MyOS - окно на рабочем столе MyOS.
 * Часть MyOS (этап 9), лицензия MIT - как у libnsfb.
 *
 * Буфер окна MyOS - общая память программы и композитора по адресу
 * MYOS_WIN_ADDR(номер), пиксели 0x00RRGGBB (это NSFB_FMT_XRGB8888),
 * поэтому libnsfb рисует прямо в него, а update() только говорит
 * композитору "этот прямоугольник поменялся". События окна (мышь,
 * клавиши, крестик) - через системный вызов win_event, переводим их в
 * события libnsfb.
 *
 * Клавиши: MyOS даёт готовый символ Юникода (с учётом Shift и русской
 * раскладки), а libnsfb - "коды клавиш" раскладки US. Поэтому символ
 * отдаём кодом 0x10000 + символ (фронтенд NetSurf, fbtk/event.c, с
 * правкой MyOS вернёт его как есть), а особые клавиши - их кодами.
 * Одно событие MyOS даёт несколько событий libnsfb (нажата/отпущена,
 * Ctrl) - они ждут в маленькой очереди. Зажат ли Ctrl - поле mods
 * события (MYOS_MOD_CTRL).
 *
 * Курсор мыши рисует сам MyOS - libnsfb свой не рисует.
 */
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>

#include "libnsfb.h"
#include "libnsfb_plot.h"
#include "libnsfb_event.h"

#include "nsfb.h"
#include "surface.h"
#include "plot.h"

#include "myos_sys.h"

#define MYOS_UNICODE_KEY 0x10000   /* код "готовый символ" для fbtk */

typedef struct {
    int           win;             /* номер окна MyOS */
    nsfb_event_t  q[8];            /* события, ждущие выдачи */
    int           qh, qn;
    bool          left_down;
} myos_priv_t;

static myos_priv_t g_priv;

static void q_push(myos_priv_t *p, enum nsfb_event_type_e type, int code)
{
    if (p->qn >= 8)
        return;

    nsfb_event_t *e = &p->q[(p->qh + p->qn) % 8];

    memset(e, 0, sizeof(*e));
    e->type = type;
    e->value.keycode = code;
    p->qn++;
}

/* клавиша: нажата и сразу отпущена (MyOS сообщает только нажатие) */
static void q_key(myos_priv_t *p, int code)
{
    q_push(p, NSFB_EVENT_KEY_DOWN, code);
    q_push(p, NSFB_EVENT_KEY_UP, code);
}

static int myos_defaults(nsfb_t *nsfb)
{
    nsfb->width = 1000;
    nsfb->height = 700;
    nsfb->format = NSFB_FMT_XRGB8888;
    select_plotters(nsfb);
    return 0;
}

static int myos_initialise(nsfb_t *nsfb)
{
    myos_priv_t *p = &g_priv;

    if (nsfb->width > MYOS_WIN_MAX_W)
        nsfb->width = MYOS_WIN_MAX_W;
    if (nsfb->height > MYOS_WIN_MAX_H)
        nsfb->height = MYOS_WIN_MAX_H;

    long id = myos_syscall3(SYS_WIN_CREATE, nsfb->width, nsfb->height, (long)"NetSurf");

    if (id <= 0)
        return -1;             /* нет графики ('start') или окон слишком много */

    memset(p, 0, sizeof(*p));
    p->win = (int)id;

    nsfb->format = NSFB_FMT_XRGB8888;
    nsfb->bpp = 32;
    nsfb->ptr = (uint8_t *)(unsigned long)MYOS_WIN_ADDR(id);
    nsfb->linelen = nsfb->width * 4;
    nsfb->surface_priv = p;
    select_plotters(nsfb);

    return 0;
}

static int myos_finalise(nsfb_t *nsfb)
{
    myos_priv_t *p = nsfb->surface_priv;

    if (p != NULL && p->win > 0) {
        myos_syscall3(SYS_WIN_CLOSE, p->win, 0, 0);
        p->win = 0;
    }

    return 0;
}

/* размер окна MyOS не меняет - только тот, что задали до initialise */
static int myos_set_geometry(nsfb_t *nsfb, int width, int height, enum nsfb_format_e format)
{
    if (nsfb->surface_priv != NULL)
        return -1;

    if (width > 0)
        nsfb->width = width;
    if (height > 0)
        nsfb->height = height;
    if (format != NSFB_FMT_ANY && format != NSFB_FMT_XRGB8888)
        return -1;

    return 0;
}

/* событие MyOS -> события libnsfb в очередь */
static void translate(myos_priv_t *p, const struct myos_event *e)
{
    switch (e->type) {

    case EV_MOVE: {
        nsfb_event_t *q;
        if (p->qn >= 8)
            break;
        q = &p->q[(p->qh + p->qn) % 8];
        memset(q, 0, sizeof(*q));
        q->type = NSFB_EVENT_MOVE_ABSOLUTE;
        q->value.vector.x = e->x;
        q->value.vector.y = e->y;
        q->value.vector.z = 0;
        p->qn++;
        break;
    }

    case EV_DOWN:
    case EV_UP: {
        /* сначала - где мышь (NetSurf жмёт туда, где курсор) */
        struct myos_event mv = *e;
        mv.type = EV_MOVE;
        translate(p, &mv);
        if (e->type == EV_DOWN && !p->left_down) {
            p->left_down = true;
            q_push(p, NSFB_EVENT_KEY_DOWN, NSFB_KEY_MOUSE_1);
        } else if (e->type == EV_UP && p->left_down) {
            p->left_down = false;
            q_push(p, NSFB_EVENT_KEY_UP, NSFB_KEY_MOUSE_1);
        }
        break;
    }

    case EV_WHEEL:
        /* колесо: у libnsfb это "кнопки" 4 (вверх) и 5 (вниз) */
        q_key(p, (e->wheel > 0) ? NSFB_KEY_MOUSE_4 : NSFB_KEY_MOUSE_5);
        break;

    case EV_CLOSE:
        if (p->qn < 8) {
            nsfb_event_t *q = &p->q[(p->qh + p->qn) % 8];
            memset(q, 0, sizeof(*q));
            q->type = NSFB_EVENT_CONTROL;
            q->value.controlcode = NSFB_CONTROL_QUIT;
            p->qn++;
        }
        break;

    case EV_KEY: {
        unsigned k = e->key;

        if (k == 0) {
            /* особая клавиша: по коду scan */
            int code = 0;
            switch (e->scan) {
            case KEY_UP:    code = NSFB_KEY_UP; break;
            case KEY_DOWN:  code = NSFB_KEY_DOWN; break;
            case KEY_LEFT:  code = NSFB_KEY_LEFT; break;
            case KEY_RIGHT: code = NSFB_KEY_RIGHT; break;
            case KEY_HOME:  code = NSFB_KEY_HOME; break;
            case KEY_END:   code = NSFB_KEY_END; break;
            case KEY_PGUP:  code = NSFB_KEY_PAGEUP; break;
            case KEY_PGDN:  code = NSFB_KEY_PAGEDOWN; break;
            case KEY_DEL:   code = NSFB_KEY_DELETE; break;
            case KEY_ESC:   code = NSFB_KEY_ESCAPE; break;
            default: break;
            }
            if (code)
                q_key(p, code);
        } else if (k == 8 || k == 9 || k == 13 || k == 27 || k == 127) {
            q_key(p, (int)k);            /* Backspace, Tab, Enter, Esc, Del */
        } else if ((e->mods & MYOS_MOD_CTRL) &&
                   ((k >= 'a' && k <= 'z') || (k >= 'A' && k <= 'Z'))) {
            /* Ctrl+буква (копировать, вставить, выделить всё...): Ctrl
               нажат, буква, Ctrl отпущен - так её понимает NetSurf */
            q_push(p, NSFB_EVENT_KEY_DOWN, NSFB_KEY_LCTRL);
            q_key(p, NSFB_KEY_a + (int)((k | 0x20u) - 'a'));
            q_push(p, NSFB_EVENT_KEY_UP, NSFB_KEY_LCTRL);
        } else if (k >= 32) {
            q_key(p, MYOS_UNICODE_KEY + (int)k);
        }
        break;
    }

    default:
        break;
    }
}

static bool myos_input(nsfb_t *nsfb, nsfb_event_t *event, int timeout)
{
    myos_priv_t *p = nsfb->surface_priv;
    struct myos_event e;

    if (p == NULL)
        return false;

    /* timeout < 0 - ждать сколько угодно (но просыпаемся раз в секунду,
       чтобы ядро видело Ctrl+C / закрытие окна) */
    while (p->qn == 0) {
        long wait = (timeout < 0) ? 1000 : timeout;
        long r = myos_syscall3(SYS_WIN_EVENT, p->win, (long)&e, wait);

        if (r == 1)
            translate(p, &e);
        else if (timeout >= 0) {
            event->type = NSFB_EVENT_CONTROL;
            event->value.controlcode = NSFB_CONTROL_TIMEOUT;
            return true;
        }
    }

    *event = p->q[p->qh];
    p->qh = (p->qh + 1) % 8;
    p->qn--;
    return true;
}

static int myos_update(nsfb_t *nsfb, nsfb_bbox_t *box)
{
    myos_priv_t *p = nsfb->surface_priv;
    struct myos_rect r;

    if (p == NULL || p->win <= 0)
        return 0;

    r.x = box->x0;
    r.y = box->y0;
    r.w = box->x1 - box->x0;
    r.h = box->y1 - box->y0;

    myos_syscall3(SYS_WIN_UPDATE, p->win, (long)&r, 0);
    return 0;
}

/* курсор рисует MyOS */
static int myos_cursor(nsfb_t *nsfb, struct nsfb_cursor_s *cursor)
{
    (void)nsfb;
    (void)cursor;
    return true;
}

const nsfb_surface_rtns_t myos_rtns = {
    .defaults = myos_defaults,
    .initialise = myos_initialise,
    .finalise = myos_finalise,
    .input = myos_input,
    .update = myos_update,
    .cursor = myos_cursor,
    .geometry = myos_set_geometry,
};

NSFB_SURFACE_DEF(myos, NSFB_SURFACE_MYOS, &myos_rtns)
