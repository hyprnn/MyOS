/*
 * gui/apps.c - родные приложения рабочего стола (этап 7).
 * Часть MyOS; общие объявления - в myos.h.
 *
 * Каждое приложение - это WIN_CLASS: функция paint (нарисовать
 * содержимое окна) и функция event (клавиша, клик). Композитор
 * (gui/wm.c) вызывает их сам. Здесь: Терминал (окно с /bin/sh,
 * gui/tty.c), Проводник, плюс ярлыки на рабочем столе и меню "Пуск".
 * О системе, Блокнот и Сапёр - уже программы в /bin (этап 10, Д4):
 * отсюда их только запускают.
 */
#include "myos.h"

#define C_FACE   0xC0C0C0u
#define C_WHITE  0xFFFFFFu
#define C_BLACK  0x000000u
#define C_NAVY   0x000080u
#define C_GRAY   0x808080u
#define C_TEXTBG 0xFFFFFFu


/* ================================================================
 * О системе, Блокнот, Сапёр - программы /bin/about, /bin/notepad,
 * /bin/mines (этап 10, Д4): раньше их окна были кодом ядра здесь.
 * Теперь ядро только запускает их - рисуют они сами (user/lib/gfx.c,
 * тот же шрифт и значки).
 * ================================================================ */

void app_open_about(void)
{
    app_run_bare("about");
}

/* path - файл для правки (NULL - новый текст). В кавычках: в имени
   могут быть пробелы, а ядро делит аргументы программы по пробелам */
void app_open_notepad(const char *path)
{
    char cmd[VFS_PATH_MAX + 16];

    if (path && path[0])
        ksnprintf(cmd, sizeof(cmd), "notepad \"%s\"", path);
    else
        ksnprintf(cmd, sizeof(cmd), "notepad");

    app_run_bare(cmd);
}

void app_open_minesweeper(void)
{
    app_run_bare("mines");
}


/* ================================================================
 * Терминал: окно с шеллом /bin/sh (gui/tty.c, этап 10)
 * ================================================================ */

void app_open_terminal(void)
{
    tty_open_window("");
}

/* запустить графическую программу без окна терминала: её вывод (если
   есть) идёт только в журнал ядра (COM1, PROC_IO_GUI), ввода с
   клавиатуры у неё нет - клавиши приходят событиями её окна */
typedef struct { char cmd[VFS_PATH_MAX + 16]; } BARE;

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

/* запустить программу /bin в новом окне терминала: шелл выполнит её
   первой командой, потом - обычное приглашение */
void app_open_program(const char *name)
{
    tty_open_window(name);
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
             kstreq(what, "browser") || kstreq(what, "mines"))
        app_run_bare(what);          /* графическая программа - без терминала */
    else
        app_open_program(what);
}
