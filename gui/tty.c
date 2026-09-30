/*
 * gui/tty.c - терминал рабочего стола: окно, в котором работает
 * шелл-программа /bin/sh (этап 10, Д2). Часть MyOS; общие объявления -
 * в myos.h.
 *
 * Раньше у окна "Терминал" был свой маленький набор команд внутри
 * ядра (help, ls, cat... - восемь штук). Теперь окно - только
 * "экран и клавиатура" (как xterm или консоль Windows), а команды
 * понимает /bin/sh - тот же, что в текстовой консоли. Значит, в окне
 * работает всё: команды ядра (battery, wifi, cpu...), программы,
 * фон "&", вывод в файл.
 *
 * Что здесь есть:
 *   * экран - буфер символов TTY_COLS x TTY_LINES (с историей для
 *     прокрутки колесом и PageUp/PageDown), курсор; вывод понимает
 *     '\r' (в начало строки), '\n', '\b' (на символ влево) и UTF-8 -
 *     кириллицу шрифт рабочего стола рисует;
 *   * клавиатура - очередь клавиш окна. Шелл читает её по одной
 *     (SYS_READKEY); обычная программа (calc, guess) читает строку -
 *     тогда эхо и Backspace делает сам терминал (tty_read_line);
 *   * вывод встроенных команд ядра (SYS_KCMD) - через свой
 *     SIMPLE_TEXT_OUTPUT_INTERFACE прямо в этот буфер;
 *   * "передний план": программа, которую шелл ждёт. Ctrl+C в окне
 *     останавливает её; если на переднем плане сам шелл - ему клавиша 3.
 *
 * Жизнь терминала: его держат окно и все программы, которые в нём
 * запущены (refs). Закрыли окно - программам "стоп"; память - когда
 * уйдёт последняя.
 */
#include "myos.h"

#define TTY_BG      0x101418u
#define TTY_FG      0xDCDCDCu
#define TTY_CURSOR  0x00E078u

struct KTTY {
    SIMPLE_TEXT_OUTPUT_INTERFACE out;   /* первым: указатель на out = на KTTY */
    UINT32  cells[TTY_LINES][TTY_COLS]; /* кольцо строк: символы Юникода */
    UINT32  base;                       /* строка 0 - cells[base] */
    UINT32  nlines;                     /* сколько строк занято (>= 1) */
    UINT32  row, col;                   /* курсор: строка (от 0) и колонка */
    UINT32  scroll;                     /* прокрутка: строк от низа */
    UINT32  u8_need, u8_cp;             /* разбор UTF-8 */

    INT32   keys[TTY_KEYS];             /* очередь клавиш окна */
    volatile UINT32 khead, ktail;

    struct KPROC *fg;                   /* на переднем плане */
    WIN    *win;                        /* NULL - окно закрыто */
    UINT32  refs;                       /* программ, которые его держат */

    char    logline[160];               /* строка для журнала (klog) */
    UINT32  loglen;
    char    cmd[128];                   /* первая команда шелла (ярлык) */
};

/* окно хранит не сам терминал (его освобождает wm_close), а ссылку */
typedef struct {
    KTTY *t;
} TTY_WIN;

static void tty_say(KTTY *t, const char *msg)
{
    UINTN n = 0;

    while (msg[n])
        n++;

    tty_write(t, msg, n);
}

/* ---------------- память и ссылки ---------------- */

static void tty_free_if_unused(KTTY *t)
{
    if (t->win == NULL && t->refs == 0)
        kfree(t);
}

void tty_ref(KTTY *t)
{
    if (t != NULL)
        t->refs++;
}

void tty_unref(KTTY *t)
{
    if (t == NULL)
        return;

    if (t->refs > 0)
        t->refs--;

    if (t->fg != NULL && t->refs == 0)
        t->fg = NULL;

    tty_free_if_unused(t);
}

/* ---------------- экран ---------------- */

static UINT32 *tty_line(KTTY *t, UINT32 row)
{
    return t->cells[(t->base + row) % TTY_LINES];
}

static void tty_clear_line(UINT32 *l)
{
    for (UINTN i = 0; i < TTY_COLS; i++)
        l[i] = ' ';
}

static void tty_newline(KTTY *t)
{
    /* строка готова - в журнал (автотест смотрит вывод программ там) */
    t->logline[t->loglen] = '\0';
    klog("term: %s\n", t->logline);
    t->loglen = 0;

    t->col = 0;

    if (t->row + 1 < t->nlines) {
        t->row++;
        return;
    }

    if (t->nlines < TTY_LINES) {
        t->nlines++;
    } else {
        /* кольцо полно - самая старая строка уходит */
        t->base = (t->base + 1) % TTY_LINES;
    }

    t->row = t->nlines - 1;
    tty_clear_line(tty_line(t, t->row));
}

void tty_putc(KTTY *t, UINT32 c)
{
    if (c == '\r') {
        t->col = 0;
        return;
    }

    if (c == '\n') {
        tty_newline(t);
        return;
    }

    if (c == '\b') {
        if (t->col > 0)
            t->col--;
        return;
    }

    if (c == '\t') {
        do {
            tty_putc(t, ' ');
        } while (t->col % 8 != 0);
        return;
    }

    if (c < 32)
        return;

    if (t->col >= TTY_COLS)
        tty_newline(t);

    tty_line(t, t->row)[t->col++] = c;

    /* в журнал - как есть (UTF-8) */
    if (t->loglen + 4 < sizeof(t->logline))
        t->loglen += (UINT32)utf8_put(c, &t->logline[t->loglen]);
}

static void tty_changed(KTTY *t)
{
    t->scroll = 0;              /* новый вывод - вниз */
    if (t->win != NULL)
        wm_invalidate(t->win);
}

/* Вывод программы: байты UTF-8 */
void tty_write(KTTY *t, const char *s, UINTN n)
{
    for (UINTN i = 0; i < n; i++) {

        UINT8 b = (UINT8)s[i];

        if (t->u8_need > 0) {
            if ((b & 0xC0) == 0x80) {
                t->u8_cp = (t->u8_cp << 6) | (b & 0x3Fu);
                if (--t->u8_need == 0)
                    tty_putc(t, t->u8_cp);
                continue;
            }
            t->u8_need = 0;          /* оборванная последовательность */
        }

        if (b < 0x80) {
            tty_putc(t, b);
        } else if ((b & 0xE0) == 0xC0) {
            t->u8_cp = b & 0x1Fu;
            t->u8_need = 1;
        } else if ((b & 0xF0) == 0xE0) {
            t->u8_cp = b & 0x0Fu;
            t->u8_need = 2;
        } else if ((b & 0xF8) == 0xF0) {
            t->u8_cp = b & 0x07u;
            t->u8_need = 3;
        }
    }

    tty_changed(t);
}

/* Вывод команды ядра (SYS_KCMD): print() шлёт сюда по символу CHAR16 */
static EFI_STATUS EFIAPI tty_out_string(SIMPLE_TEXT_OUTPUT_INTERFACE *this, CHAR16 *s)
{
    KTTY *t = (KTTY *)this;

    for (; *s; s++)
        tty_putc(t, *s);

    tty_changed(t);
    return EFI_SUCCESS;
}

static EFI_STATUS EFIAPI tty_out_attr(SIMPLE_TEXT_OUTPUT_INTERFACE *this, UINTN a)
{
    (void)this;
    (void)a;
    return EFI_SUCCESS;
}

static EFI_STATUS EFIAPI tty_out_clear(SIMPLE_TEXT_OUTPUT_INTERFACE *this)
{
    KTTY *t = (KTTY *)this;

    /* clear - начать с чистого экрана */
    t->base = 0;
    t->nlines = 1;
    t->row = 0;
    t->col = 0;
    tty_clear_line(tty_line(t, 0));
    tty_changed(t);

    return EFI_SUCCESS;
}

SIMPLE_TEXT_OUTPUT_INTERFACE *tty_output(KTTY *t)
{
    return &t->out;
}

/* ---------------- клавиатура ---------------- */

static void tty_push_key(KTTY *t, INT32 k)
{
    UINT32 next = (t->ktail + 1) % TTY_KEYS;

    if (next == t->khead)
        return;                     /* очередь полна - клавиша теряется */

    t->keys[t->ktail] = k;
    t->ktail = next;

    sched_wake_all(t);
}

/*
 * Клавиша для программы (SYS_READKEY): символ Юникода или
 * MYOS_KEY_SPECIAL | скан-код. timeout_ms < 0 - ждать сколько угодно.
 * -1 - нет клавиши (время вышло, программу останавливают, окно закрыто).
 */
INT32 tty_getkey(KTTY *t, struct KPROC *p, INT64 timeout_ms)
{
    UINT64 t0 = g_kticks;
    UINT64 fl = kx_irq_save();
    INT32 k = -1;

    for (;;) {

        if (t->khead != t->ktail) {
            k = t->keys[t->khead];
            t->khead = (t->khead + 1) % TTY_KEYS;
            break;
        }

        if (p->killed || t->win == NULL)
            break;

        if (timeout_ms >= 0 && g_kticks - t0 >= (UINT64)timeout_ms)
            break;

        sched_block(t, "terminal key", timeout_ms >= 0 ? 10 : 100);
    }

    kx_irq_restore(fl);
    return k;
}

/*
 * Строка для обычной программы (read с fd 0): эхо, Backspace, Enter -
 * как у консоли ядра (proc_read_console), только в окне.
 */
INTN tty_read_line(KTTY *t, struct KPROC *p, char *dst, UINTN n)
{
    char line[PROC_IN_MAX];
    UINTN len = 0;

    for (;;) {

        INT32 k = tty_getkey(t, p, -1);

        if (k < 0) {
            if (p->killed || t->win == NULL)
                return 0;
            continue;
        }

        if (k == '\r' || k == '\n') {
            tty_write(t, "\n", 1);
            line[len++] = '\n';
            break;
        }

        if (k == '\b') {
            if (len > 0) {
                /* стереть весь символ UTF-8 */
                while (len > 0 && ((UINT8)line[len - 1] & 0xC0) == 0x80)
                    len--;
                if (len > 0)
                    len--;
                tty_write(t, "\b \b", 3);
            }
            continue;
        }

        if (k >= 32 && !(k & MYOS_KEY_SPECIAL) && len + 5 < sizeof(line)) {
            char u[4];
            UINTN m = utf8_put((UINT32)k, u);
            for (UINTN i = 0; i < m; i++)
                line[len++] = u[i];
            tty_write(t, u, m);
        }
    }

    if (len > n)
        len = n;

    for (UINTN i = 0; i < len; i++)
        dst[i] = line[i];

    return (INTN)len;
}

/* ---------------- окно ---------------- */

static void tty_paint(WIN *w, GFX *g)
{
    KTTY *t = ((TTY_WIN *)w->state)->t;

    gfx_fill(g, 0, 0, g->w, g->h, TTY_BG);

    UINT32 vis = (UINT32)((g->h - 8) / FONT_H);

    if (vis < 1)
        vis = 1;

    /* нижняя видимая строка: строка курсора (или последняя), минус
       прокрутка */
    UINT32 last = t->nlines - 1;

    if (t->scroll > last)
        t->scroll = last;

    UINT32 bottom = last - t->scroll;
    UINT32 top = (bottom + 1 > vis) ? bottom + 1 - vis : 0;

    char buf[TTY_COLS * 3 + 1];
    INT32 y = 4;

    for (UINT32 r = top; r <= bottom; r++) {

        UINT32 *l = tty_line(t, r);
        UINTN k = 0;
        UINTN used = TTY_COLS;

        while (used > 0 && l[used - 1] == ' ')
            used--;

        for (UINTN i = 0; i < used; i++)
            k += utf8_put(l[i], &buf[k]);

        buf[k] = '\0';
        gfx_text(g, 4, y, buf, TTY_FG);

        if (r == t->row && t->scroll == 0)
            gfx_fill(g, 4 + (INT32)t->col * FONT_W, y + FONT_H - 3, FONT_W, 3, TTY_CURSOR);

        y += FONT_H;
    }
}

static void tty_event(WIN *w, struct myos_event *e)
{
    KTTY *t = ((TTY_WIN *)w->state)->t;

    if (e->type == EV_WHEEL) {
        if (e->wheel > 0)
            t->scroll += 3;
        else
            t->scroll = (t->scroll > 3) ? t->scroll - 3 : 0;
        wm_invalidate(w);
        return;
    }

    if (e->type != EV_KEY)
        return;

    if (e->key == 0 && (e->scan == KEY_PGUP || e->scan == KEY_PGDN)) {
        UINT32 page = (UINT32)((w->ch - 8) / FONT_H) - 1;
        if (e->scan == KEY_PGUP)
            t->scroll += page;
        else
            t->scroll = (t->scroll > page) ? t->scroll - page : 0;
        wm_invalidate(w);
        return;
    }

    /* Ctrl+C: остановить программу на переднем плане; шеллу (читает
       клавиши сам) - просто клавиша 3 */
    if (e->key == 3) {
        KPROC *f = t->fg;
        if (f != NULL && !f->raw_keys && !f->exited) {
            ksnprintf(f->why, sizeof(f->why), "stopped with Ctrl+C");
            proc_kill(f);
            return;
        }
    }

    if (e->key != 0)
        tty_push_key(t, (INT32)e->key);
    else
        tty_push_key(t, (INT32)(MYOS_KEY_SPECIAL | e->scan));
}

static void tty_close(WIN *w)
{
    KTTY *t = ((TTY_WIN *)w->state)->t;

    t->win = NULL;

    /* всем программам в этом окне - "стоп" */
    for (UINTN i = 0; i < PROC_MAX; i++)
        if (g_procs[i].used && g_procs[i].tty == t && !g_procs[i].exited) {
            ksnprintf(g_procs[i].why, sizeof(g_procs[i].why), "its terminal window was closed");
            proc_kill(&g_procs[i]);
        }

    sched_wake_all(t);
    tty_free_if_unused(t);
}

static const WIN_CLASS g_tty_class = {
    "Терминал", NULL, tty_paint, tty_event, NULL, tty_close
};

/* Поток окна: запустить /bin/sh, дождаться; шелл закрыли (exit) -
   закрыть и окно */
static void tty_shell_job(void *arg)
{
    KTTY *t = (KTTY *)arg;
    INTN err = VFS_OK;

    tty_ref(t);                 /* пока поток работает - не освобождать */

    KPROC *p = proc_spawn_ex("/bin/sh", t->cmd, PROC_IO_TTY, NULL, NULL, -1, t, &err);

    if (p == NULL) {
        tty_say(t, "Cannot start /bin/sh\n");
        tty_unref(t);
        return;
    }

    t->fg = p;

    proc_wait(p);

    if (t->win != NULL)
        wm_close(t->win);

    tty_unref(t);
}

/*
 * Открыть окно терминала с шеллом. cmd - первая команда шелла (ярлык
 * программы на рабочем столе: "calc") или "" - просто приглашение.
 */
void tty_open_window(const char *cmd)
{
    KTTY *t = (KTTY *)kmalloc(sizeof(KTTY));
    TTY_WIN *tw = (TTY_WIN *)kmalloc(sizeof(TTY_WIN));

    if (t == NULL || tw == NULL) {
        if (t) kfree(t);
        if (tw) kfree(tw);
        return;
    }

    raw_zero_mem((volatile UINT8 *)t, sizeof(*t));

    t->out = *g_st->ConOut;
    t->out.OutputString = tty_out_string;
    t->out.SetAttribute = tty_out_attr;
    t->out.ClearScreen = tty_out_clear;

    t->nlines = 1;
    tty_clear_line(tty_line(t, 0));
    ksnprintf(t->cmd, sizeof(t->cmd), "%s", cmd ? cmd : "");

    tw->t = t;

    static WIN_CLASS cls;
    cls = g_tty_class;
    cls.icon = gui_icon("terminal");

    WIN *w = wm_open(&cls, TTY_COLS * FONT_W + 8, 24 * FONT_H + 8, "Терминал", tw);

    if (w == NULL) {
        kfree(t);
        kfree(tw);
        return;
    }

    t->win = w;

    if (kthread_create("term", tty_shell_job, t, 8) == NULL) {
        tty_say(t, "No threads left to start the shell\n");
    }
}

/* На переднем плане терминала - программа (или NULL) */
void tty_set_fg(KTTY *t, struct KPROC *p)
{
    if (t != NULL)
        t->fg = p;
}

struct KPROC *tty_get_fg(KTTY *t)
{
    return t ? t->fg : NULL;
}
