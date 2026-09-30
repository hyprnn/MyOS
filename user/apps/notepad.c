/*
 * notepad - Блокнот: правка текста, в том числе по-русски
 * (этап 10, Д4: раньше был кодом ядра в gui/apps.c).
 *
 *   notepad                новый текст (без имени)
 *   notepad ФАЙЛ           открыть файл (Проводник открывает .txt так)
 *
 * Ctrl+S - сохранить, Esc или крестик - закрыть. Стрелки, Home/End,
 * Backspace, Delete, Enter, Tab (4 пробела), колесо мыши - прокрутка.
 *
 * Текст хранится как есть - в UTF-8 (так его пишут и Linux, и MyOS);
 * курсор - смещение в байтах, но двигается по целым символам: у буквы
 * кириллицы в UTF-8 два байта, и встать "между ними" нельзя.
 */
#include "myos.h"

#define W        460
#define H        320
#define NP_MAX   65536          /* 64 КиБ текста - хватит для заметок */

#define C_TEXTBG 0xFFFFFFu

static char   g_path[256];
static char  *g_text;          /* NP_MAX + 1 байт */
static size_t g_len;
static size_t g_cursor;        /* позиция ввода (байтовое смещение) */
static size_t g_top;           /* прокрутка: первая видимая строка */
static int    g_dirty;         /* есть несохранённые правки */
static char   g_status[96];


/* ================================================================
 * Строки текста
 * ================================================================ */

/* начало строки номер line (байтовое смещение); нет такой - конец текста */
static size_t line_start(size_t line)
{
    size_t l = 0;

    if (line == 0)
        return 0;

    for (size_t i = 0; i < g_len; i++)
        if (g_text[i] == '\n') {
            if (++l == line)
                return i + 1;
        }

    return g_len;
}

static size_t cursor_line(void)
{
    size_t l = 0;

    for (size_t i = 0; i < g_cursor && i < g_len; i++)
        if (g_text[i] == '\n')
            l++;

    return l;
}

static size_t line_count(void)
{
    size_t l = 1;

    for (size_t i = 0; i < g_len; i++)
        if (g_text[i] == '\n')
            l++;

    return l;
}


/* ================================================================
 * Файл
 * ================================================================ */

static void load(const char *path)
{
    snprintf(g_path, sizeof(g_path), "%s", path);

    int fd = open(path, O_READ);

    if (fd < 0) {
        /* нет файла - будет новый, сохранится под этим именем */
        if (fd != MYOS_ENOENT)
            snprintf(g_status, sizeof(g_status), "не открыть: %s", strerror(fd));
        return;
    }

    for (;;) {
        long r = read(fd, g_text + g_len, NP_MAX - g_len);
        if (r <= 0)
            break;
        g_len += (size_t)r;
        if (g_len >= NP_MAX) {
            snprintf(g_status, sizeof(g_status), "файл больше 64 КиБ - открыто начало");
            break;
        }
    }

    close(fd);
}

static void save(void)
{
    if (g_path[0] == '\0') {
        snprintf(g_status, sizeof(g_status), "нет имени файла (открой через Проводник)");
        return;
    }

    int fd = open(g_path, O_WRITE | O_CREATE | O_TRUNC);

    if (fd < 0) {
        snprintf(g_status, sizeof(g_status), "не сохранить: %s", strerror(fd));
        return;
    }

    long w = (g_len > 0) ? write(fd, g_text, g_len) : 0;
    close(fd);

    if (w < 0) {
        snprintf(g_status, sizeof(g_status), "не сохранить: %s", strerror((int)w));
    } else if ((size_t)w < g_len) {
        snprintf(g_status, sizeof(g_status), "не сохранить: %s", strerror(MYOS_ENOSPC));
    } else {
        g_dirty = 0;
        snprintf(g_status, sizeof(g_status), "сохранено: %u байт", (unsigned)g_len);
        /* журнал ядра (COM1) - автотест видит, что сохранилось */
        printf("notepad: saved %s, %u bytes\n", g_path, (unsigned)g_len);
    }
}


/* ================================================================
 * Рисование
 * ================================================================ */

static void paint(GFX *g)
{
    gfx_fill(g, 0, 0, g->w, g->h, GFX_FACE);

    /* меню-строка */
    gfx_text(g, 6, 4, "Файл: Ctrl+S — сохранить   Esc — закрыть", GFX_BLACK);

    /* поле текста */
    int tx = 4, ty = 22;
    int tw = g->w - 8, th = g->h - 26 - 16;

    gfx_fill(g, tx, ty, tw, th, C_TEXTBG);
    gfx_bevel(g, tx, ty, tw, th, 0);
    gfx_clip(g, tx + 2, ty + 2, tw - 4, th - 4);

    size_t rows = (size_t)(th - 4) / FONT_H;
    size_t cur = cursor_line();

    /* курсор всегда виден: прокрутить к нему */
    if (cur < g_top)
        g_top = cur;
    if (cur >= g_top + rows)
        g_top = cur - rows + 1;

    int y = ty + 2;
    size_t i = line_start(g_top);

    for (size_t r = 0; r < rows && i <= g_len; r++) {

        int x = tx + 4;
        char ch[5];
        int cn = 0;

        while (i < g_len && g_text[i] != '\n') {

            if (i == g_cursor)
                gfx_fill(g, x, y, 1, FONT_H, GFX_BLACK);

            ch[cn++] = g_text[i++];

            /* один символ UTF-8 целиком */
            if ((g_text[i] & 0xC0) != 0x80 || cn >= 4) {
                ch[cn] = '\0';
                const char *p = ch;
                gfx_glyph(g, x, y, utf8_next(&p), GFX_BLACK);
                x += FONT_W;
                cn = 0;
            }
        }

        if (i == g_cursor)
            gfx_fill(g, x, y, 1, FONT_H, GFX_BLACK);

        if (i < g_len && g_text[i] == '\n')
            i++;
        else if (i >= g_len)
            i = g_len + 1;   /* конец */

        y += FONT_H;
    }

    gfx_noclip(g);

    /* строка состояния */
    gfx_fill(g, 0, g->h - 14, g->w, 14, GFX_FACE);
    gfx_bevel(g, 0, g->h - 14, g->w, 14, 0);

    char st[160];
    snprintf(st, sizeof(st), "%s%s  стр %u  %s", g_path[0] ? g_path : "(без имени)",
             g_dirty ? " *" : "", (unsigned)cur + 1, g_status);
    gfx_text_fit(g, 4, g->h - 13, st, GFX_BLACK, g->w / FONT_W);
}


/* ================================================================
 * Правка
 * ================================================================ */

static void insert(const char *bytes, size_t k)
{
    if (g_len + k > NP_MAX) {
        snprintf(g_status, sizeof(g_status), "больше 64 КиБ нельзя");
        return;
    }

    memmove(g_text + g_cursor + k, g_text + g_cursor, g_len - g_cursor);
    memcpy(g_text + g_cursor, bytes, k);

    g_len += k;
    g_cursor += k;
    g_dirty = 1;
    g_status[0] = '\0';
}

/* убрать k байт с позиции at */
static void cut(size_t at, size_t k)
{
    memmove(g_text + at, g_text + at + k, g_len - at - k);
    g_len -= k;
    g_dirty = 1;
    g_status[0] = '\0';
}

static void backspace(void)
{
    if (g_cursor == 0)
        return;

    /* один символ UTF-8 (несколько байт) */
    size_t k = 1;

    while (g_cursor - k > 0 && (g_text[g_cursor - k] & 0xC0) == 0x80)
        k++;

    g_cursor -= k;
    cut(g_cursor, k);
}

static void del(void)
{
    if (g_cursor >= g_len)
        return;

    size_t k = 1;

    while (g_cursor + k < g_len && (g_text[g_cursor + k] & 0xC0) == 0x80)
        k++;

    cut(g_cursor, k);
}

static void up_down(int down)
{
    size_t line = cursor_line();
    size_t col = g_cursor - line_start(line);

    if (!down && line == 0)
        return;

    size_t target = down ? line + 1 : line - 1;

    if (target >= line_count())
        return;

    size_t ls = line_start(target);
    size_t le = ls;

    while (le < g_len && g_text[le] != '\n')
        le++;

    g_cursor = (ls + col < le) ? ls + col : le;

    /* не встать внутрь буквы */
    while (g_cursor > ls && (g_text[g_cursor] & 0xC0) == 0x80)
        g_cursor--;
}

/* 1 - перерисовать, -1 - закрыть окно, 0 - ничего */
static int on_key(const struct myos_event *e)
{
    unsigned int k = e->key;

    if (e->scan == KEY_ESC)
        return -1;

    /* Ctrl+S: либо символ 0x13 (DC3), либо 's'/'ы' с зажатым Ctrl */
    if (k == 0x13 || ((e->mods & MYOS_MOD_CTRL) &&
                      (k == 's' || k == 'S' || k == 0x44B || k == 0x42B))) {
        save();
        return 1;
    }

    if (e->mods & MYOS_MOD_CTRL)
        return 0;                  /* другие Ctrl+... блокноту не нужны */

    if (k == 8) {
        backspace();
    } else if (k == '\r' || k == '\n') {
        insert("\n", 1);
    } else if (k == '\t') {
        insert("    ", 4);
    } else if (e->scan == KEY_DEL) {
        del();
    } else if (e->scan == KEY_LEFT) {
        if (g_cursor > 0) {
            g_cursor--;
            while (g_cursor > 0 && (g_text[g_cursor] & 0xC0) == 0x80)
                g_cursor--;
        }
    } else if (e->scan == KEY_RIGHT) {
        if (g_cursor < g_len) {
            g_cursor++;
            while (g_cursor < g_len && (g_text[g_cursor] & 0xC0) == 0x80)
                g_cursor++;
        }
    } else if (e->scan == KEY_HOME) {
        g_cursor = line_start(cursor_line());
    } else if (e->scan == KEY_END) {
        while (g_cursor < g_len && g_text[g_cursor] != '\n')
            g_cursor++;
    } else if (e->scan == KEY_UP || e->scan == KEY_DOWN) {
        up_down(e->scan == KEY_DOWN);
    } else if (e->scan == KEY_PGUP || e->scan == KEY_PGDN) {
        for (int n = 0; n < 10; n++)
            up_down(e->scan == KEY_PGDN);
    } else if (k >= 0x20) {
        char buf[4];
        int n = utf8_put(k, buf);
        insert(buf, (size_t)n);
    } else {
        return 0;
    }

    return 1;
}

int main(int argc, char **argv)
{
    g_text = malloc(NP_MAX + 1);

    if (g_text == NULL) {
        printf("notepad: out of memory\n");
        return 1;
    }

    char title[300];

    if (argc > 1) {
        load(argv[1]);
        snprintf(title, sizeof(title), "Блокнот — %s", g_path);
    } else {
        snprintf(title, sizeof(title), "Блокнот — (новый файл)");
    }

    int id = win_create(W, H, title);

    if (id < 0) {
        printf("notepad: %s\n", strerror(id));
        return 1;
    }

    GFX g;
    gfx_init(&g, win_pixels(id), W, H);
    paint(&g);
    win_update(id);

    for (;;) {

        struct myos_event e;

        if (!win_event(id, &e, 60000))
            continue;

        int r = 0;

        if (e.type == EV_CLOSE)
            r = -1;
        else if (e.type == EV_KEY)
            r = on_key(&e);
        else if (e.type == EV_WHEEL) {
            /* колесо - курсор на 3 строки: прокрутка идёт за ним */
            for (int n = 0; n < 3; n++)
                up_down(e.wheel < 0);
            r = 1;
        }

        if (r < 0) {
            win_close(id);
            return 0;
        }

        if (r > 0) {
            paint(&g);
            win_update(id);
        }
    }
}
