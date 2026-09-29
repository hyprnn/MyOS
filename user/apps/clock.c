/*
 * clock - аналоговые часы в своём окне (этап 7): показывает, что
 * программа умеет рисовать в окне и получать время у ядра.
 */
#include "myos.h"

#define W 200
#define H 220

static void line(unsigned int *b, int x0, int y0, int x1, int y1, unsigned int col)
{
    int dx = x1 - x0, dy = y1 - y0;
    int n = (dx < 0 ? -dx : dx) > (dy < 0 ? -dy : dy) ? (dx < 0 ? -dx : dx) : (dy < 0 ? -dy : dy);
    if (n == 0) n = 1;
    for (int i = 0; i <= n; i++) {
        int x = x0 + dx * i / n, y = y0 + dy * i / n;
        gpx(b, W, H, x, y, col);
        gpx(b, W, H, x + 1, y, col);
    }
}

/* грубый синус/косинус по таблице (0..59 -> градусы стрелки) */
static const signed char sintab[60] = {
    0,10,21,31,41,50,59,67,74,81,87,92,96,99,100,100,99,96,92,87,
    81,74,67,59,50,41,31,21,10,0,-10,-21,-31,-41,-50,-59,-67,-74,-81,-87,
    -92,-96,-99,-100,-100,-99,-96,-92,-87,-81,-74,-67,-59,-50,-41,-31,-21,-10,0,0
};
static int si(int t) { return sintab[((t % 60) + 60) % 60]; }
static int co(int t) { return sintab[((t + 15) % 60 + 60) % 60]; }

int main(void)
{
    int id = win_create(W, H, "Часы");
    if (id < 0) { printf("no window (start the desktop first)\n"); return 1; }

    unsigned int *b = win_pixels(id);
    int cx = W / 2, cy = 100, R = 90;

    for (;;) {
        struct myos_event e;
        while (win_event(id, &e, 0)) {
            if (e.type == EV_CLOSE) { win_close(id); return 0; }
        }

        struct myos_time t;
        gettime(&t);

        gfill(b, W, H, 0, 0, W, H, 0xC0C0C0);
        /* циферблат */
        for (int a = 0; a < 60; a += 5)
            gfill(b, W, H, cx + co(a) * R / 100 - 1, cy + si(a) * R / 100 - 1, 3, 3, 0x000000);

        int h = t.hour % 12, m = t.minute, s = t.second;
        int hp = h * 5 + m / 12;
        line(b, cx, cy, cx + co(hp) * (R * 5 / 10) / 100, cy + si(hp) * (R * 5 / 10) / 100, 0x000080);
        line(b, cx, cy, cx + co(m) * (R * 8 / 10) / 100, cy + si(m) * (R * 8 / 10) / 100, 0x000000);
        line(b, cx, cy, cx + co(s) * (R * 9 / 10) / 100, cy + si(s) * (R * 9 / 10) / 100, 0xFF0000);

        char buf[16];
        snprintf(buf, sizeof(buf), "%02u:%02u:%02u", t.hour, t.minute, t.second);
        gtext(b, W, H, cx - gtextw(buf) / 2, 200, buf, 0x000000);

        win_update(id);
        sleep_ms(200);
    }
}
