/*
 * life - "Жизнь" Конвея в окне (этап 7). Клетки живут и умирают по
 * простым правилам; клик мышью ставит/убирает клетку, пробел -
 * пауза, 'c' - очистить, 'r' - случайное поле.
 */
#include "myos.h"

#define CW 60
#define CH 44
#define PIX 8
#define W (CW * PIX)
#define H (CH * PIX + 16)

static unsigned char cur[CH][CW], nxt[CH][CW];

static void step(void)
{
    for (int y = 0; y < CH; y++)
        for (int x = 0; x < CW; x++) {
            int n = 0;
            for (int dy = -1; dy <= 1; dy++)
                for (int dx = -1; dx <= 1; dx++) {
                    if (!dx && !dy) continue;
                    int yy = (y + dy + CH) % CH, xx = (x + dx + CW) % CW;
                    n += cur[yy][xx];
                }
            nxt[y][x] = (n == 3 || (n == 2 && cur[y][x])) ? 1 : 0;
        }
    for (int y = 0; y < CH; y++)
        for (int x = 0; x < CW; x++)
            cur[y][x] = nxt[y][x];
}

static void draw(unsigned int *b, int running, unsigned long gen)
{
    gfill(b, W, H, 0, 0, W, H, 0x101010);
    for (int y = 0; y < CH; y++)
        for (int x = 0; x < CW; x++)
            if (cur[y][x])
                gfill(b, W, H, x * PIX, y * PIX, PIX - 1, PIX - 1, 0x40FF40);

    gfill(b, W, H, 0, CH * PIX, W, 16, 0xC0C0C0);
    char s[64];
    snprintf(s, sizeof(s), "%s  gen %lu  [space] [c]lear [r]andom", running ? "RUN " : "STOP", gen);
    gtext(b, W, H, 4, CH * PIX + 4, s, 0x000000);
}

int main(void)
{
    int id = win_create(W, H, "Жизнь Конвея");
    if (id < 0) { printf("no window (start the desktop first)\n"); return 1; }

    unsigned int *b = win_pixels(id);
    int running = 1;
    unsigned long gen = 0;

    srand(uptime_ms());
    for (int y = 0; y < CH; y++)
        for (int x = 0; x < CW; x++)
            cur[y][x] = (rand() % 5 == 0) ? 1 : 0;

    for (;;) {
        struct myos_event e;
        while (win_event(id, &e, 0)) {
            if (e.type == EV_CLOSE) { win_close(id); return 0; }
            if (e.type == EV_DOWN && e.y < CH * PIX) {
                int x = e.x / PIX, y = e.y / PIX;
                if (x >= 0 && x < CW && y >= 0 && y < CH)
                    cur[y][x] ^= 1;
            }
            if (e.type == EV_KEY) {
                if (e.key == ' ') running = !running;
                else if (e.key == 'c') { for (int y=0;y<CH;y++) for(int x=0;x<CW;x++) cur[y][x]=0; gen=0; }
                else if (e.key == 'r') { for (int y=0;y<CH;y++) for(int x=0;x<CW;x++) cur[y][x]=(rand()%5==0); }
            }
        }

        if (running) { step(); gen++; }
        draw(b, running, gen);
        win_update(id);
        sleep_ms(running ? 90 : 40);
    }
}
