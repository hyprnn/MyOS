/*
 * about - окно "О системе" (этап 10, Д4: раньше было кодом ядра в
 * gui/apps.c). Знак MyOS, пара слов о системе и состояние сети.
 * Esc или крестик - закрыть.
 *
 * Сеть узнаём системным вызовом netinfo (как ifconfig): первая
 * карта с адресом - "eth0 10.0.2.15", без адреса - "нет адреса"
 * или "нет кабеля". Окно раз в 2 секунды обновляет эту строку -
 * DHCP мог выдать адрес уже после того, как окно открыли.
 */
#include "myos.h"

#define W 380
#define H 230

/* "Сеть: ..." - как net_status_line в ядре */
static void net_line(char *buf, size_t cap)
{
    struct myos_netif ni;
    char ip[16];
    int any = 0;

    for (int i = 0; netinfo(i, &ni) == 1; i++) {

        if (strcmp(ni.name, "lo") == 0)
            continue;

        if (ni.up) {
            snprintf(buf, cap, "%s %s", ni.name, ip_to_str(ni.ip, ip));
            return;
        }

        if (!any) {
            snprintf(buf, cap, "%s: %s", ni.name, ni.link ? "no address yet" : "no link");
            any = 1;
        }
    }

    if (!any)
        snprintf(buf, cap, "no network");
}

static void paint(GFX *g)
{
    gfx_fill(g, 0, 0, g->w, g->h, GFX_FACE);

    gfx_icon(g, 16, 18, gfx_icon_rows("logo"), 3, 0);

    int x = 80, y = 16;

    gfx_text_bold(g, x, y, "MyOS", GFX_NAVY); y += 22;
    gfx_text(g, x, y, "Операционная система", GFX_BLACK); y += 16;
    gfx_text(g, x, y, "с нуля, без Linux и Windows.", GFX_BLACK); y += 24;
    gfx_text(g, 16, y, "Своё ядро: память, потоки, диски (FAT),", GFX_BLACK); y += 16;
    gfx_text(g, 16, y, "программы в кольце 3 и эта оконная", GFX_BLACK); y += 16;
    gfx_text(g, 16, y, "система с композитором; сеть TCP/IP.", GFX_BLACK); y += 24;

    char net[64], line[80];
    net_line(net, sizeof(net));
    snprintf(line, sizeof(line), "Сеть: %s", net);
    gfx_text(g, 16, y, line, GFX_BLACK); y += 20;

    gfx_text(g, 16, y, "Меню «Пуск» → программы и игры.", GFX_SHADOW);
}

int main(void)
{
    int id = win_create(W, H, "О системе — MyOS");

    if (id < 0) {
        printf("about: %s\n", strerror(id));
        return 1;
    }

    GFX g;
    gfx_init(&g, win_pixels(id), W, H);

    for (;;) {

        paint(&g);
        win_update(id);

        /* события - или раз в 2 с перерисовать (строка сети) */
        struct myos_event e;

        while (win_event(id, &e, 2000)) {
            if (e.type == EV_CLOSE || (e.type == EV_KEY && e.scan == KEY_ESC)) {
                win_close(id);
                return 0;
            }
        }
    }
}
