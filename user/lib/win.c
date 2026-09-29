/*
 * user/lib/win.c - окна для программ (этап 7): обёртки над системными
 * вызовами и простое рисование в буфер окна.
 *
 * Буфер окна - общий с ядром: программа пишет в него пиксели
 * 0x00RRGGBB, затем win_update показывает их на экране. Адрес буфера
 * ядро отдаёт по формуле MYOS_WIN_ADDR(id).
 */
#include "myos.h"

int win_create(int w, int h, const char *title)
{
    return (int)syscall3(SYS_WIN_CREATE, w, h, (long)title);
}

void *win_pixels(int id)
{
    return (void *)(MYOS_WIN_BASE + (unsigned long long)(id - 1) * MYOS_WIN_SPAN);
}

int win_update(int id)                              { return (int)syscall3(SYS_WIN_UPDATE, id, 0, 0); }
int win_close(int id)                               { return (int)syscall3(SYS_WIN_CLOSE, id, 0, 0); }
int win_title(int id, const char *t)                { return (int)syscall3(SYS_WIN_TITLE, id, (long)t, 0); }
int win_event(int id, struct myos_event *e, int ms) { return (int)syscall3(SYS_WIN_EVENT, id, (long)e, ms); }

/* --- рисование --- */

void gpx(unsigned int *buf, int w, int h, int x, int y, unsigned int col)
{
    if (x >= 0 && y >= 0 && x < w && y < h)
        buf[(long)y * w + x] = col;
}

void gfill(unsigned int *buf, int w, int h, int x, int y, int rw, int rh, unsigned int col)
{
    if (x < 0) { rw += x; x = 0; }
    if (y < 0) { rh += y; y = 0; }
    if (x + rw > w) rw = w - x;
    if (y + rh > h) rh = h - y;

    for (int j = 0; j < rh; j++)
        for (int i = 0; i < rw; i++)
            buf[(long)(y + j) * w + (x + i)] = col;
}

/*
 * Текст: шрифт 6x8, вшитый в программу (font6x8). Латиница, цифры,
 * знаки - для подписей на кнопках и счётчиках. Русский текст удобнее
 * ставить в ЗАГОЛОВОК окна (его рисует ядро полным шрифтом).
 */
extern const unsigned char font6x8[95][8];

static void gglyph(unsigned int *buf, int w, int h, int x, int y, char c, unsigned int col)
{
    if (c < 32 || c > 126)
        c = '?';

    const unsigned char *g = font6x8[c - 32];

    for (int row = 0; row < 8; row++)
        for (int cx = 0; cx < 6; cx++)
            if (g[row] & (0x80 >> cx))
                gpx(buf, w, h, x + cx, y + row, col);
}

void gtext(unsigned int *buf, int w, int h, int x, int y, const char *s, unsigned int col)
{
    for (; *s; s++) {
        gglyph(buf, w, h, x, y, *s, col);
        x += 6;
    }
}

int gtextw(const char *s)
{
    int n = 0;
    while (s[n]) n++;
    return n * 6;
}
