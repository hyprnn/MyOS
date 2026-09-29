/*
 * paint - рисовалка в окне (этап 7): держи левую кнопку и води мышью.
 * Внизу - палитра цветов; клавиша 'c' очищает лист.
 */
#include "myos.h"

#define W 320
#define H 240
#define BAR 20

static const unsigned int pal[8] = {
    0x000000, 0xFFFFFF, 0xFF0000, 0x00A000,
    0x0000FF, 0xFFFF00, 0x00FFFF, 0xFF00FF
};

int main(void)
{
    int id = win_create(W, H, "Рисование");
    if (id < 0) { printf("no window (start the desktop first)\n"); return 1; }

    unsigned int *b = win_pixels(id);
    unsigned int col = 0x000000;
    int px = -1, py = -1;

    gfill(b, W, H, 0, 0, W, H - BAR, 0xFFFFFF);
    for (int i = 0; i < 8; i++)
        gfill(b, W, H, i * (W / 8), H - BAR, W / 8, BAR, pal[i]);
    win_update(id);

    for (;;) {
        struct myos_event e;
        if (!win_event(id, &e, 200))
            continue;

        if (e.type == EV_CLOSE) { win_close(id); return 0; }

        if (e.type == EV_KEY && e.key == 'c') {
            gfill(b, W, H, 0, 0, W, H - BAR, 0xFFFFFF);
            win_update(id);
        }

        if ((e.type == EV_DOWN || e.type == EV_MOVE) && (e.buttons & 1)) {
            if (e.y >= H - BAR) {
                col = pal[(e.x * 8 / W) & 7];
            } else {
                if (e.type == EV_MOVE && px >= 0) {
                    int dx = e.x - px, dy = e.y - py;
                    int n = (dx < 0 ? -dx : dx) > (dy < 0 ? -dy : dy) ? (dx<0?-dx:dx):(dy<0?-dy:dy);
                    if (n == 0) n = 1;
                    for (int i = 0; i <= n; i++)
                        gfill(b, W, H, px + dx * i / n - 1, py + dy * i / n - 1, 3, 3, col);
                } else {
                    gfill(b, W, H, e.x - 1, e.y - 1, 3, 3, col);
                }
                px = e.x; py = e.y;
            }
            win_update(id);
        }

        if (e.type == EV_UP) { px = py = -1; }
    }
}
