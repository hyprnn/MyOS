/*
 * gui/apps.c - родные приложения рабочего стола (этап 7).
 * Часть MyOS; общие объявления - в myos.h.
 *
 * Каждое приложение - это WIN_CLASS: функция paint (нарисовать
 * содержимое окна) и функция event (клавиша, клик). Композитор
 * (gui/wm.c) вызывает их сам. Здесь: О системе, Блокнот (правка
 * текста, в том числе по-русски), Терминал (с запуском программ),
 * Проводник, Сапёр, плюс ярлыки на рабочем столе и меню "Пуск".
 */
#include "myos.h"

#define C_FACE   0xC0C0C0u
#define C_WHITE  0xFFFFFFu
#define C_BLACK  0x000000u
#define C_NAVY   0x000080u
#define C_GRAY   0x808080u
#define C_TEXTBG 0xFFFFFFu


/* ================================================================
 * О системе
 * ================================================================ */

static void about_paint(WIN *w, GFX *g)
{
    (void)w;

    gfx_fill(g, 0, 0, g->w, g->h, C_FACE);

    gfx_icon(g, 16, 18, gui_icon("logo"), 3, FALSE);

    INT32 x = 80, y = 16;

    gfx_text_bold(g, x, y, "MyOS", 0x000080u); y += 22;
    gfx_text(g, x, y, "Операционная система", C_BLACK); y += 16;
    gfx_text(g, x, y, "с нуля, без Linux и Windows.", C_BLACK); y += 24;
    gfx_text(g, 16, y, "Своё ядро: память, потоки, диски (FAT),", C_BLACK); y += 16;
    gfx_text(g, 16, y, "программы в кольце 3 и эта оконная", C_BLACK); y += 16;
    gfx_text(g, 16, y, "система с композитором; сеть TCP/IP.", C_BLACK); y += 24;

    char net[48], line[64];
    net_status_line(net, sizeof(net));
    ksnprintf(line, sizeof(line), "Сеть: %s", net);
    gfx_text(g, 16, y, line, C_BLACK); y += 20;

    gfx_text(g, 16, y, "Меню «Пуск» → программы и игры.", C_GRAY);
}

static void about_event(WIN *w, struct myos_event *e)
{
    if (e->type == EV_KEY && e->scan == KEY_ESC)
        wm_close(w);
}

static const WIN_CLASS g_about_class = {
    "О системе", NULL, about_paint, about_event, NULL, NULL
};

/* значок задаётся отдельно, т.к. gui_icon возвращает не-const-указатель */
void app_open_about(void)
{
    static WIN_CLASS cls;
    cls = g_about_class;
    cls.icon = gui_icon("logo");
    wm_open(&cls, 380, 230, "О системе — MyOS", NULL);
}


/* ================================================================
 * Блокнот: правка текста (в том числе кириллицей)
 * ================================================================ */

#define NP_MAX  8192

typedef struct {
    char   path[VFS_PATH_MAX];
    char   text[NP_MAX + 1];
    UINTN  len;
    UINTN  cursor;          /* позиция ввода (байтовое смещение) */
    UINTN  top_line;        /* прокрутка: первая видимая строка */
    BOOLEAN dirty;          /* есть несохранённые правки */
    char   status[64];
} NP;

/* строки: начало каждой строки (байтовое смещение) */
static UINTN np_line_start(NP *n, UINTN line)
{
    UINTN l = 0;

    if (line == 0)
        return 0;

    for (UINTN i = 0; i < n->len; i++)
        if (n->text[i] == '\n') {
            if (++l == line)
                return i + 1;
        }

    return n->len;
}

static UINTN np_cursor_line(NP *n)
{
    UINTN l = 0;

    for (UINTN i = 0; i < n->cursor && i < n->len; i++)
        if (n->text[i] == '\n')
            l++;

    return l;
}

static void np_save(WIN *w, NP *n)
{
    if (n->path[0] == '\0') {
        ksnprintf(n->status, sizeof(n->status), "нет имени файла (открой через Проводник)");
        return;
    }

    INTN r = vfs_write_file(n->path, n->text, n->len, FALSE);

    if (r == VFS_OK) {
        n->dirty = FALSE;
        ksnprintf(n->status, sizeof(n->status), "сохранено: %u байт", (UINT32)n->len);
    } else {
        ksnprintf(n->status, sizeof(n->status), "не сохранить: %s", vfs_strerror(r));
    }

    wm_invalidate(w);
}

static void np_paint(WIN *w, GFX *g)
{
    NP *n = (NP *)w->state;

    gfx_fill(g, 0, 0, g->w, g->h, C_FACE);

    /* меню-строка */
    gfx_text(g, 6, 4, "Файл: Ctrl+S — сохранить   Esc — закрыть", C_BLACK);

    /* поле текста */
    INT32 tx = 4, ty = 22;
    INT32 tw = (INT32)g->w - 8, th = (INT32)g->h - 26 - 16;

    gfx_fill(g, tx, ty, tw, th, C_TEXTBG);
    gfx_bevel(g, tx, ty, tw, th, FALSE);
    gfx_clip(g, tx + 2, ty + 2, tw - 4, th - 4);

    UINTN rows = (UINTN)(th - 4) / FONT_H;
    UINTN cur_line = np_cursor_line(n);

    if (cur_line < n->top_line)
        n->top_line = cur_line;
    if (cur_line >= n->top_line + rows)
        n->top_line = cur_line - rows + 1;

    INT32 y = ty + 2;
    UINTN i = np_line_start(n, n->top_line);
    UINTN line = n->top_line;

    for (UINTN r = 0; r < rows && i <= n->len; r++, line++) {

        INT32 x = tx + 4;
        char ch[5];
        UINTN cn = 0;

        while (i < n->len && n->text[i] != '\n') {
            /* курсор */
            if (i == n->cursor)
                gfx_fill(g, x, y, 1, FONT_H, C_BLACK);

            ch[cn++] = n->text[i++];

            /* один символ UTF-8 целиком */
            if ((n->text[i] & 0xC0) != 0x80 || cn >= 4) {
                ch[cn] = '\0';
                const char *p = ch;
                gfx_glyph(g, x, y, utf8_next(&p), C_BLACK);
                x += FONT_W;
                cn = 0;
            }
        }

        if (i == n->cursor)
            gfx_fill(g, x, y, 1, FONT_H, C_BLACK);

        if (i < n->len && n->text[i] == '\n')
            i++;
        else if (i >= n->len)
            i = n->len + 1;   /* конец */

        y += FONT_H;
    }

    gfx_noclip(g);

    /* строка состояния */
    gfx_fill(g, 0, (INT32)g->h - 14, g->w, 14, C_FACE);
    gfx_bevel(g, 0, (INT32)g->h - 14, g->w, 14, FALSE);

    char st[96];
    ksnprintf(st, sizeof(st), "%s%s  стр %u  %s", n->path[0] ? n->path : "(без имени)",
              n->dirty ? " *" : "", (UINT32)cur_line + 1, n->status);
    gfx_text_fit(g, 4, (INT32)g->h - 13, st, C_BLACK, (UINTN)(g->w / FONT_W));
}

static void np_insert(NP *n, const char *bytes, UINTN k)
{
    if (n->len + k > NP_MAX)
        return;

    for (UINTN i = n->len; i > n->cursor; i--)
        n->text[i + k - 1] = n->text[i - 1];

    for (UINTN i = 0; i < k; i++)
        n->text[n->cursor + i] = bytes[i];

    n->len += k;
    n->cursor += k;
    n->dirty = TRUE;
    n->status[0] = '\0';
}

static void np_backspace(NP *n)
{
    if (n->cursor == 0)
        return;

    /* убрать один символ UTF-8 (несколько байт) */
    UINTN k = 1;

    while (n->cursor - k > 0 && (n->text[n->cursor - k] & 0xC0) == 0x80)
        k++;

    for (UINTN i = n->cursor - k; i + k < n->len; i++)
        n->text[i] = n->text[i + k];

    n->len -= k;
    n->cursor -= k;
    n->dirty = TRUE;
    n->status[0] = '\0';
}

static void np_event(WIN *w, struct myos_event *e)
{
    NP *n = (NP *)w->state;

    if (e->type != EV_KEY)
        return;

    if (e->key == 3) {                 /* Ctrl+C не нужен - это блокнот */
        return;
    }

    if (e->scan == KEY_ESC) {
        wm_close(w);
        return;
    }

    /* Ctrl+S: UnicodeChar 0x13 (DC3) приходит от Ctrl+S */
    if (e->key == 0x13) {
        np_save(w, n);
        return;
    }

    if (e->key == CHAR_BACKSPACE) {
        np_backspace(n);
    } else if (e->key == CHAR_CARRIAGE_RETURN) {
        np_insert(n, "\n", 1);
    } else if (e->key == 0x09) {
        np_insert(n, "    ", 4);
    } else if (e->scan == KEY_LEFT) {
        if (n->cursor > 0) {
            n->cursor--;
            while (n->cursor > 0 && (n->text[n->cursor] & 0xC0) == 0x80)
                n->cursor--;
        }
    } else if (e->scan == KEY_RIGHT) {
        if (n->cursor < n->len) {
            n->cursor++;
            while (n->cursor < n->len && (n->text[n->cursor] & 0xC0) == 0x80)
                n->cursor++;
        }
    } else if (e->scan == KEY_HOME) {
        n->cursor = np_line_start(n, np_cursor_line(n));
    } else if (e->scan == KEY_END) {
        while (n->cursor < n->len && n->text[n->cursor] != '\n')
            n->cursor++;
    } else if (e->scan == KEY_UP || e->scan == KEY_DOWN) {
        UINTN line = np_cursor_line(n);
        UINTN col = n->cursor - np_line_start(n, line);
        UINTN target = (e->scan == KEY_UP) ? (line ? line - 1 : 0) : line + 1;
        UINTN ls = np_line_start(n, target);
        if (ls <= n->len) {
            UINTN le = ls;
            while (le < n->len && n->text[le] != '\n')
                le++;
            n->cursor = (ls + col < le) ? ls + col : le;
        }
    } else if (e->key >= 0x20) {
        char buf[4];
        UINTN k = utf8_put(e->key, buf);
        np_insert(n, buf, k);
    } else {
        return;
    }

    wm_invalidate(w);
}

static const WIN_CLASS g_np_class = {
    "Блокнот", NULL, np_paint, np_event, NULL, NULL
};

void app_open_notepad(const char *path)
{
    NP *n = (NP *)kmalloc(sizeof(NP));

    if (n == NULL)
        return;

    raw_zero_mem((volatile UINT8 *)n, sizeof(*n));

    char title[VFS_PATH_MAX + 16];

    if (path && path[0]) {
        ksnprintf(n->path, sizeof(n->path), "%s", path);
        UINTN got = 0;
        vfs_read_file(path, n->text, NP_MAX, &got);
        n->len = got;
        ksnprintf(title, sizeof(title), "Блокнот — %s", path);
    } else {
        ksnprintf(title, sizeof(title), "Блокнот — (новый файл)");
    }

    static WIN_CLASS cls;
    cls = g_np_class;
    cls.icon = gui_icon("notepad");

    WIN *w = wm_open(&cls, 460, 320, title, n);

    if (w == NULL)
        kfree(n);
}


/* ================================================================
 * Терминал: команды и запуск программ
 * ================================================================ */

#define T_COLS   72
#define T_ROWS   60
#define T_LINELEN 256

typedef struct {
    char    lines[T_ROWS][T_LINELEN];
    UINTN   count;
    UINTN   top;
    char    input[T_LINELEN];
    UINTN   inlen;
    KPROC  *proc;            /* запущенная программа (ждёт ввод / выводит) */
    KTHREAD *job;
    UINT32  jtid;
    char    cmd[T_LINELEN];  /* что запустить (для потока) */
    WIN    *win;
} TERM;

static TERM *g_active_term = NULL;    /* куда программа шлёт вывод */

static void term_push(TERM *t, const char *line)
{
    if (t->count == T_ROWS) {
        for (UINTN i = 1; i < T_ROWS; i++)
            memcpy(t->lines[i - 1], t->lines[i], T_LINELEN);
        t->count--;
    }

    UINTN k = 0;

    while (line[k] && k + 1 < T_LINELEN) {
        t->lines[t->count][k] = line[k];
        k++;
    }

    t->lines[t->count][k] = '\0';
    t->count++;

    /* автопрокрутка вниз */
    UINTN vis = 20;
    t->top = (t->count > vis) ? t->count - vis : 0;
}

/* строка вывода программы -> в активный терминал */
static void term_program_sink(const char *line)
{
    if (g_active_term) {
        term_push(g_active_term, line);
        if (g_active_term->win)
            wm_invalidate(g_active_term->win);
    }
    klog("term: %s\n", line);
}

static void term_paint(WIN *w, GFX *g)
{
    TERM *t = (TERM *)w->state;

    gfx_fill(g, 0, 0, g->w, g->h, 0x0C0C0Cu);

    UINTN rows = (UINTN)(g->h - 8) / FONT_H;

    if (rows < 2)
        rows = 2;

    UINTN vis = rows - 1;
    UINTN first = (t->count > vis) ? t->count - vis : 0;

    if (t->top < first || t->top > t->count)
        t->top = first;

    INT32 y = 4;

    for (UINTN i = t->top; i < t->count && i < t->top + vis; i++) {
        gfx_text(g, 4, y, t->lines[i], 0xDCDCDCu);
        y += FONT_H;
    }

    /* строка ввода */
    char prompt[T_LINELEN + 8];

    if (t->proc)
        ksnprintf(prompt, sizeof(prompt), "%s", t->input);
    else
        ksnprintf(prompt, sizeof(prompt), "> %s", t->input);

    INT32 tx = gfx_text(g, 4, y, prompt, 0x00E078u);
    gfx_fill(g, 4 + tx, y, FONT_W, FONT_H, 0x00E078u);  /* курсор */
}

/* поток: запустить программу, дождаться, прибрать */
static void term_job(void *arg)
{
    TERM *t = (TERM *)arg;
    INTN err;
    char buf[160];

    g_proc_gui_sink = term_program_sink;
    g_active_term = t;

    char path[VFS_PATH_MAX];
    char name[64];
    UINTN k = 0;

    while (t->cmd[k] && t->cmd[k] != ' ' && k + 1 < sizeof(name)) {
        name[k] = t->cmd[k];
        k++;
    }
    name[k] = '\0';

    const char *args = t->cmd + k;
    while (*args == ' ') args++;

    if (!proc_find_program(name, path, sizeof(path))) {
        ksnprintf(buf, sizeof(buf), "нет такой программы: %s", name);
        term_program_sink(buf);
        t->proc = NULL;
        t->job = NULL;
        wm_invalidate(t->win);
        return;
    }

    KPROC *p = proc_spawn(path, args, PROC_IO_GUI, &err);

    if (p == NULL) {
        ksnprintf(buf, sizeof(buf), "не запустить %s: %s%s%s", name, vfs_strerror(err),
                  g_proc_last_error[0] ? " — " : "", g_proc_last_error);
        g_proc_last_error = "";
        term_program_sink(buf);
        t->proc = NULL;
        t->job = NULL;
        wm_invalidate(t->win);
        return;
    }

    t->proc = p;
    g_fg_proc = p;

    INT64 code = proc_wait(p);

    g_fg_proc = NULL;
    t->proc = NULL;
    t->job = NULL;

    if (code == -1 && p->why[0]) {
        ksnprintf(buf, sizeof(buf), "*** %s остановлена: %s", name, p->why);
        term_program_sink(buf);
    } else if (code != 0) {
        ksnprintf(buf, sizeof(buf), "[%s: код возврата %lld]", name, code);
        term_program_sink(buf);
    }

    wm_invalidate(t->win);
}

static void term_run_builtin(TERM *t, const char *cmd);

static void term_submit(TERM *t)
{
    char echo[T_LINELEN + 4];

    /* если работает программа - строка ей на ввод */
    if (t->proc) {
        ksnprintf(echo, sizeof(echo), "%s", t->input);
        term_push(t, echo);
        proc_gui_input(t->proc, t->input);
        t->input[0] = '\0';
        t->inlen = 0;
        return;
    }

    ksnprintf(echo, sizeof(echo), "> %s", t->input);
    term_push(t, echo);

    if (t->input[0] != '\0')
        term_run_builtin(t, t->input);

    t->input[0] = '\0';
    t->inlen = 0;
}

/* маленький набор встроенных команд; всё остальное - программа /bin */
static void term_run_builtin(TERM *t, const char *cmd)
{
    char out[T_LINELEN];

    if (kstreq(cmd, "help")) {
        term_push(t, "команды: help clear ver time ls cd pwd cat echo");
        term_push(t, "программы: hello calc guess primes crash (и свои из /bin)");
        return;
    }

    if (kstreq(cmd, "clear") || kstreq(cmd, "cls")) {
        t->count = 0;
        t->top = 0;
        return;
    }

    if (kstreq(cmd, "ver")) {
        term_push(t, "MyOS, этап 7");
        return;
    }

    if (kstreq(cmd, "time") || kstreq(cmd, "date")) {
        EFI_TIME tm;
        if (rtc_read(&tm)) {
            tz_to_local(&tm);
            ksnprintf(out, sizeof(out), "%04u-%02u-%02u %02u:%02u:%02u",
                      tm.Year, tm.Month, tm.Day, tm.Hour, tm.Minute, tm.Second);
            term_push(t, out);
        }
        return;
    }

    if (kstreq(cmd, "pwd")) {
        term_push(t, g_cwd);
        return;
    }

    if (cmd[0]=='e' && cmd[1]=='c' && cmd[2]=='h' && cmd[3]=='o' && cmd[4]==' ') {
        term_push(t, cmd + 5);
        return;
    }

    /* запустить программу из /bin в отдельном потоке */
    ksnprintf(t->cmd, sizeof(t->cmd), "%s", cmd);
    t->job = kthread_create("term", term_job, t, 16);

    if (t->job == NULL)
        term_push(t, "нет потоков для запуска программы");
    else
        t->jtid = t->job->tid;
}

static void term_event(WIN *w, struct myos_event *e)
{
    TERM *t = (TERM *)w->state;

    if (e->type == EV_WHEEL) {
        if (e->wheel > 0 && t->top > 0)
            t->top--;
        else if (e->wheel < 0 && t->top + 1 < t->count)
            t->top++;
        wm_invalidate(w);
        return;
    }

    if (e->type != EV_KEY)
        return;

    if (e->scan == KEY_ESC && !t->proc) {
        wm_close(w);
        return;
    }

    /* Ctrl+C останавливает программу */
    if (e->key == 3 && t->proc) {
        proc_kill(t->proc);
        return;
    }

    if (e->key == CHAR_CARRIAGE_RETURN) {
        term_submit(t);
    } else if (e->key == CHAR_BACKSPACE) {
        while (t->inlen > 0 && (t->input[t->inlen - 1] & 0xC0) == 0x80)
            t->inlen--;
        if (t->inlen > 0)
            t->inlen--;
        t->input[t->inlen] = '\0';
    } else if (e->key >= 0x20) {
        char buf[4];
        UINTN k = utf8_put(e->key, buf);
        if (t->inlen + k + 1 < T_LINELEN) {
            for (UINTN i = 0; i < k; i++)
                t->input[t->inlen++] = buf[i];
            t->input[t->inlen] = '\0';
        }
    } else {
        return;
    }

    wm_invalidate(w);
}

static void term_close(WIN *w)
{
    TERM *t = (TERM *)w->state;

    if (t->proc)
        proc_kill(t->proc);

    if (g_active_term == t)
        g_active_term = NULL;
}

static const WIN_CLASS g_term_class = {
    "Терминал", NULL, term_paint, term_event, NULL, term_close
};

void app_open_terminal(void)
{
    TERM *t = (TERM *)kmalloc(sizeof(TERM));

    if (t == NULL)
        return;

    raw_zero_mem((volatile UINT8 *)t, sizeof(*t));

    static WIN_CLASS cls;
    cls = g_term_class;
    cls.icon = gui_icon("terminal");

    WIN *w = wm_open(&cls, T_COLS * FONT_W + 8, 22 * FONT_H, "Терминал", t);

    if (w == NULL) {
        kfree(t);
        return;
    }

    t->win = w;
    term_push(t, "MyOS терминал. Наберите help. Программы работают в кольце 3.");
}

/* запустить графическую программу без окна терминала: её вывод (если
   есть) идёт только в журнал COM1, а рисует она в своё окно */
typedef struct { char cmd[128]; } BARE;

static void bare_job(void *arg)
{
    BARE *b = (BARE *)arg;
    INTN err;
    char path[VFS_PATH_MAX], name[64];
    UINTN k = 0;

    while (b->cmd[k] && b->cmd[k] != ' ' && k + 1 < sizeof(name)) { name[k] = b->cmd[k]; k++; }
    name[k] = '\0';
    const char *args = b->cmd + k;
    while (*args == ' ') args++;

    if (proc_find_program(name, path, sizeof(path))) {
        g_proc_gui_sink = NULL;      /* вывод в COM1 (klog), окна рисует сама */
        KPROC *p = proc_spawn(path, args, PROC_IO_GUI, &err);
        if (p != NULL)
            proc_wait(p);
        else
            klog("wm: cannot start %s: %s\n", name, vfs_strerror(err));
    }

    kfree(b);
}

void app_run_bare(const char *name)
{
    BARE *b = (BARE *)kmalloc(sizeof(BARE));

    if (b == NULL)
        return;

    ksnprintf(b->cmd, sizeof(b->cmd), "%s", name);

    if (kthread_create("prog", bare_job, b, 16) == NULL)
        kfree(b);
}

/* запустить программу /bin в новом окне терминала */
void app_open_program(const char *name)
{
    app_open_terminal();

    /* найти только что открытый терминал (верхнее окно) и запустить */
    WIN *w = wm_focused();

    if (w && w->cls == NULL)
        return;

    TERM *t = (w && w->state) ? (TERM *)w->state : NULL;

    if (t != NULL) {
        char echo[T_LINELEN];
        ksnprintf(echo, sizeof(echo), "> %s", name);
        term_push(t, echo);
        ksnprintf(t->cmd, sizeof(t->cmd), "%s", name);
        t->job = kthread_create("term", term_job, t, 16);
        wm_invalidate(w);
    }
}


/* ================================================================
 * Проводник
 * ================================================================ */

typedef struct {
    char  path[VFS_PATH_MAX];
    char  names[64][VFS_NAME_MAX];
    UINT8 isdir[64];
    UINT64 size[64];
    UINTN count;
    UINTN top;
    WIN  *win;
} EXP;

typedef struct { EXP *e; } EXP_CTX;

static INTN exp_cb(void *ctx, const VFS_DIRENT *d)
{
    EXP *e = ((EXP_CTX *)ctx)->e;

    if (e->count >= 64)
        return 1;

    ksnprintf(e->names[e->count], VFS_NAME_MAX, "%s", d->name);
    e->isdir[e->count] = d->node.is_dir ? 1 : 0;
    e->size[e->count] = d->node.size;
    e->count++;

    return 0;
}

static void exp_reload(EXP *e)
{
    e->count = 0;
    e->top = 0;

    if (e->path[1] == '\0') {
        /* корень: тома */
        kmutex_lock(&g_vfs_mutex);
        for (UINTN i = 0; i < VFS_MAX_MOUNTS; i++)
            if (g_mounts[i].used && e->count < 64) {
                ksnprintf(e->names[e->count], VFS_NAME_MAX, "%s", g_mounts[i].name);
                e->isdir[e->count] = 1;
                e->size[e->count] = 0;
                e->count++;
            }
        kmutex_unlock(&g_vfs_mutex);
        return;
    }

    EXP_CTX c = { e };
    vfs_list(e->path, exp_cb, &c);
}

static void exp_paint(WIN *w, GFX *g)
{
    EXP *e = (EXP *)w->state;

    gfx_fill(g, 0, 0, g->w, g->h, C_FACE);

    /* адресная строка */
    gfx_fill(g, 4, 4, (INT32)g->w - 8, 16, C_WHITE);
    gfx_bevel(g, 4, 4, (INT32)g->w - 8, 16, FALSE);
    gfx_text_fit(g, 8, 6, e->path, C_BLACK, (UINTN)((g->w - 16) / FONT_W));

    /* список */
    INT32 ly = 24;
    INT32 lh = (INT32)g->h - ly - 4;

    gfx_fill(g, 4, ly, (INT32)g->w - 8, lh, C_WHITE);
    gfx_bevel(g, 4, ly, (INT32)g->w - 8, lh, FALSE);
    gfx_clip(g, 6, ly + 2, (INT32)g->w - 12, lh - 4);

    UINTN rows = (UINTN)(lh - 4) / 18;
    INT32 y = ly + 2;

    /* ".." для входа наверх */
    UINTN shown = 0;
    UINTN idx = e->top;

    if (e->top == 0 && e->path[1] != '\0') {
        gfx_icon(g, 8, y + 1, gui_icon("folder"), 1, FALSE);
        gfx_text(g, 28, y + 2, "..", C_BLACK);
        y += 18;
        shown++;
    }

    for (; idx < e->count && shown < rows; idx++, shown++) {

        const char *ic = e->isdir[idx] ? "folder" : "file";

        gfx_icon(g, 8, y + 1, gui_icon(ic), 1, FALSE);

        char row[128];
        if (e->isdir[idx])
            ksnprintf(row, sizeof(row), "%s", e->names[idx]);
        else
            ksnprintf(row, sizeof(row), "%s  (%llu Б)", e->names[idx], e->size[idx]);

        gfx_text_fit(g, 28, y + 2, row, C_BLACK, (UINTN)((g->w - 36) / FONT_W));
        y += 18;
    }

    gfx_noclip(g);
}

static void exp_open_index(WIN *w, EXP *e, INTN row)
{
    /* row учитывает ".." первой строкой */
    if (e->path[1] != '\0' && row == 0) {
        char np[VFS_PATH_MAX];
        vfs_normalize("..", np, sizeof(np));  /* от g_cwd - не то; сделаем вручную */
        /* убрать последний элемент */
        UINTN k = 0;
        for (UINTN i = 0; e->path[i]; i++)
            if (e->path[i] == '/')
                k = i;
        e->path[k ? k : 1] = '\0';
        exp_reload(e);
        wm_invalidate(w);
        return;
    }

    UINTN i = (UINTN)row - (e->path[1] != '\0' ? 1 : 0);

    if (i >= e->count)
        return;

    char full[VFS_PATH_MAX];

    if (e->path[1] == '\0')
        ksnprintf(full, sizeof(full), "/%s", e->names[i]);
    else
        ksnprintf(full, sizeof(full), "%s/%s", e->path, e->names[i]);

    char norm[VFS_PATH_MAX];
    vfs_normalize(full, norm, sizeof(norm));

    if (e->isdir[i]) {
        ksnprintf(e->path, sizeof(e->path), "%s", norm);
        exp_reload(e);
        wm_invalidate(w);
        return;
    }

    /* файл: ELF -> запустить, иначе открыть в блокноте */
    char magic[4];
    UINTN got = 0;
    vfs_read_file(norm, magic, 4, &got);

    if (got == 4 && magic[0] == 0x7F && magic[1] == 'E' && magic[2] == 'L' && magic[3] == 'F') {
        app_open_program(norm);
    } else {
        app_open_notepad(norm);
    }
}

static void exp_event(WIN *w, struct myos_event *e)
{
    EXP *ex = (EXP *)w->state;

    if (e->type == EV_WHEEL) {
        if (e->wheel > 0 && ex->top > 0) ex->top--;
        else if (e->wheel < 0 && ex->top + 1 < ex->count) ex->top++;
        wm_invalidate(w);
        return;
    }

    if (e->type == EV_DOWN) {
        INT32 ly = 24;
        if (e->y >= ly + 2) {
            INTN row = (INTN)((e->y - ly - 2) / 18) + (INTN)ex->top;
            exp_open_index(w, ex, row);
        }
        return;
    }

    if (e->type == EV_KEY && e->scan == KEY_ESC)
        wm_close(w);
}

static const WIN_CLASS g_exp_class = {
    "Проводник", NULL, exp_paint, exp_event, NULL, NULL
};

void app_open_explorer(const char *path)
{
    EXP *e = (EXP *)kmalloc(sizeof(EXP));

    if (e == NULL)
        return;

    raw_zero_mem((volatile UINT8 *)e, sizeof(*e));
    ksnprintf(e->path, sizeof(e->path), "%s", (path && path[0]) ? path : "/");

    static WIN_CLASS cls;
    cls = g_exp_class;
    cls.icon = gui_icon("folder");

    WIN *w = wm_open(&cls, 380, 300, "Проводник", e);

    if (w == NULL) {
        kfree(e);
        return;
    }

    e->win = w;
    exp_reload(e);
}


/* ================================================================
 * Сапёр (логика игры - в gui/minesweeper.c, здесь только окно)
 * ================================================================ */

#define MS_CELL 18

static void ms_paint(WIN *w, GFX *g)
{
    GUI_MS_STATE *ms = (GUI_MS_STATE *)w->state;

    gfx_fill(g, 0, 0, g->w, g->h, C_FACE);

    /* верхняя панель: флаги, смайлик, таймер */
    INT32 top = 6;

    char n1[8], n2[8];
    ksnprintf(n1, sizeof(n1), "%03u", (UINT32)(GUI_MS_MINES - (ms->flags_used > GUI_MS_MINES ? GUI_MS_MINES : ms->flags_used)));
    ksnprintf(n2, sizeof(n2), "%03u", (UINT32)(ms->timer > 999 ? 999 : ms->timer));

    gfx_fill(g, 8, top, 34, 18, C_BLACK);
    gfx_text(g, 10, top + 1, n1, 0xFF0000u);
    gfx_fill(g, (INT32)g->w - 42, top, 34, 18, C_BLACK);
    gfx_text(g, (INT32)g->w - 40, top + 1, n2, 0xFF0000u);

    INT32 sx = (INT32)g->w / 2 - 11;
    gfx_button(g, sx, top, 22, 22, FALSE);
    const char *face = ms->over ? (ms->won ? ":)" : ":(") : ":|";
    gfx_text(g, sx + 3, top + 4, face, C_BLACK);

    /* поле */
    INT32 gx = ((INT32)g->w - GUI_MS_COLS * MS_CELL) / 2;
    INT32 gy = 34;

    gfx_bevel(g, gx - 2, gy - 2, GUI_MS_COLS * MS_CELL + 4, GUI_MS_ROWS * MS_CELL + 4, FALSE);

    for (INT32 r = 0; r < GUI_MS_ROWS; r++)
        for (INT32 c = 0; c < GUI_MS_COLS; c++) {

            INT32 x = gx + c * MS_CELL, y = gy + r * MS_CELL;

            if (ms->revealed[r][c]) {
                gfx_fill(g, x, y, MS_CELL, MS_CELL, C_FACE);
                gfx_fill(g, x, y, MS_CELL, 1, C_GRAY);
                gfx_fill(g, x, y, 1, MS_CELL, C_GRAY);
                if (ms->mine[r][c]) {
                    gfx_fill(g, x + 4, y + 4, MS_CELL - 8, MS_CELL - 8, C_BLACK);
                    if (ms->boom_r == r && ms->boom_c == c)
                        gfx_fill(g, x, y, MS_CELL, MS_CELL, 0xFF0000u);
                } else if (ms->adj[r][c]) {
                    static const UINT32 nc[9] = { 0, 0x0000FF, 0x008000, 0xFF0000,
                        0x000080, 0x800000, 0x008080, 0x000000, 0x808080 };
                    char d[2] = { (char)('0' + ms->adj[r][c]), 0 };
                    gfx_text(g, x + 5, y + 1, d, nc[ms->adj[r][c]]);
                }
            } else {
                gfx_button(g, x, y, MS_CELL, MS_CELL, FALSE);
                if (ms->flagged[r][c])
                    gfx_text(g, x + 5, y + 1, "!", 0xFF0000u);
            }
        }
}

static void ms_event(WIN *w, struct myos_event *e)
{
    GUI_MS_STATE *ms = (GUI_MS_STATE *)w->state;

    if (e->type == EV_KEY && e->scan == KEY_ESC) {
        wm_close(w);
        return;
    }

    if (e->type != EV_DOWN)
        return;

    INT32 sw = w->cw;
    INT32 sx = sw / 2 - 11;

    /* смайлик - новая игра */
    if (e->y >= 6 && e->y < 28 && e->x >= sx && e->x < sx + 22) {
        gui_ms_reset(ms);
        wm_invalidate(w);
        return;
    }

    INT32 gx = (sw - GUI_MS_COLS * MS_CELL) / 2;
    INT32 gy = 34;
    INT32 c = (e->x - gx) / MS_CELL;
    INT32 r = (e->y - gy) / MS_CELL;

    if (r < 0 || r >= GUI_MS_ROWS || c < 0 || c >= GUI_MS_COLS)
        return;

    if (e->buttons & 2u)
        gui_ms_toggle_flag(ms, r, c);
    else
        gui_ms_reveal(ms, r, c);

    wm_invalidate(w);
}

static void ms_tick(WIN *w)
{
    GUI_MS_STATE *ms = (GUI_MS_STATE *)w->state;
    static UINT64 acc = 0;

    if (ms->generated && !ms->over) {
        if (++acc >= 10) {          /* тик 100 мс -> секунда */
            acc = 0;
            ms->timer++;
            wm_invalidate(w);
        }
    }
}

static const WIN_CLASS g_ms_class = {
    "Сапёр", NULL, ms_paint, ms_event, ms_tick, NULL
};

void app_open_minesweeper(void)
{
    GUI_MS_STATE *ms = (GUI_MS_STATE *)kmalloc(sizeof(GUI_MS_STATE));

    if (ms == NULL)
        return;

    gui_ms_seed(g_st);
    gui_ms_reset(ms);

    static WIN_CLASS cls;
    cls = g_ms_class;
    cls.icon = gui_icon("mine");

    WIN *w = wm_open(&cls, GUI_MS_COLS * MS_CELL + 20, GUI_MS_ROWS * MS_CELL + 40, "Сапёр", ms);

    if (w == NULL)
        kfree(ms);
}


/* ================================================================
 * Ярлыки рабочего стола
 * ================================================================ */

static const struct { const char *label; const char *icon; const char *cmd; } g_desk[] = {
    { "Мой компьютер", "computer", "explorer" },
    { "Терминал",      "terminal", "terminal" },
    { "Браузер",       "web",      "browser"  },
    { "Блокнот",       "notepad",  "notepad"  },
    { "Часы",          "clock",    "clock"    },
    { "Рисование",     "paint",    "paint"    },
    { "Сапёр",         "mine",     "mine"     },
};
#define DESK_N (sizeof(g_desk) / sizeof(g_desk[0]))
#define DESK_CELL_W 90
#define DESK_CELL_H 70
#define DESK_ICON   32
#define DESK_LEFT   12
#define DESK_TOP    36

static INTN g_desk_sel = -1;

static void desk_cell(UINTN i, INT32 *x, INT32 *y)
{
    *x = DESK_LEFT;
    *y = DESK_TOP + (INT32)(i * DESK_CELL_H);
}

void apps_draw_shortcuts(GFX *g)
{
    for (UINTN i = 0; i < DESK_N; i++) {

        INT32 cx, cy;
        desk_cell(i, &cx, &cy);

        INT32 ix = cx + (DESK_CELL_W - DESK_ICON) / 2;

        gfx_icon(g, ix, cy, gui_icon(g_desk[i].icon), 2, (INTN)i == g_desk_sel);

        INT32 tw = gfx_text_width(g_desk[i].label);
        INT32 tx = cx + (DESK_CELL_W - tw) / 2;
        INT32 ty = cy + DESK_ICON + 2;

        if ((INTN)i == g_desk_sel)
            gfx_fill(g, tx - 2, ty, tw + 4, FONT_H, C_NAVY);

        gfx_text(g, tx, ty, g_desk[i].label, C_WHITE);
    }
}

static INTN desk_at(INT32 x, INT32 y)
{
    for (UINTN i = 0; i < DESK_N; i++) {
        INT32 cx, cy;
        desk_cell(i, &cx, &cy);
        if (x >= cx && x < cx + DESK_CELL_W && y >= cy && y < cy + DESK_CELL_H)
            return (INTN)i;
    }
    return -1;
}

void apps_shortcut_click(INT32 x, INT32 y)
{
    INTN i = desk_at(x, y);

    if (i < 0) {
        g_desk_sel = -1;
        wm_invalidate_desktop();
        return;
    }

    if (i != g_desk_sel) {
        g_desk_sel = i;
        wm_invalidate_desktop();
        return;
    }

    /* второй щелчок - открыть */
    g_desk_sel = -1;
    apps_desktop_launch(g_desk[i].cmd);
}


/* ================================================================
 * Запуск по имени (меню "Пуск" и ярлыки)
 * ================================================================ */

void apps_desktop_launch(const char *what)
{
    klog("wm: launch %s\n", what);

    if (kstreq(what, "terminal"))
        app_open_terminal();
    else if (kstreq(what, "notepad"))
        app_open_notepad(NULL);
    else if (kstreq(what, "explorer"))
        app_open_explorer("/");
    else if (kstreq(what, "mine"))
        app_open_minesweeper();
    else if (kstreq(what, "about"))
        app_open_about();
    else if (kstreq(what, "exit"))
        wm_request_stop();
    else if (kstreq(what, "calc") || kstreq(what, "guess") ||
             kstreq(what, "primes") || kstreq(what, "hello") || kstreq(what, "crash"))
        app_open_program(what);
    else if (kstreq(what, "clock") || kstreq(what, "paint") || kstreq(what, "life") ||
             kstreq(what, "browser"))
        app_run_bare(what);          /* графическая программа - без терминала */
    else
        app_open_program(what);
}
