/*
 * mines - Сапёр (этап 10, Д4: раньше был кодом ядра - gui/minesweeper.c
 * и окно в gui/apps.c). Поле 9x9, 10 мин, как в Windows.
 *
 *   левая кнопка  - открыть клетку (первая всегда без мины)
 *   правая кнопка - флажок
 *   смайлик или N - новая игра, Esc или крестик - закрыть
 *
 * Слева - сколько мин осталось без флажка, справа - секунды игры.
 */
#include "myos.h"

#define COLS   9
#define ROWS   9
#define MINES  10
#define CELL   18
#define TOP    34                    /* поле - ниже панели со смайликом */
#define W      (COLS * CELL + 20)
#define H      (ROWS * CELL + 40)

static unsigned char g_mine[ROWS][COLS];
static unsigned char g_adj[ROWS][COLS];      /* мин вокруг клетки */
static unsigned char g_open[ROWS][COLS];
static unsigned char g_flag[ROWS][COLS];
static int g_generated;      /* мины расставлены? (лениво, после 1-го клика) */
static int g_over, g_won;
static int g_flags, g_opened;
static int g_boom_r = -1, g_boom_c = -1;     /* какая мина взорвалась */
static unsigned long g_start_ms;             /* когда был первый клик */
static int g_seconds;


/* ================================================================
 * Логика поля
 * ================================================================ */

static void reset(void)
{
    memset(g_mine, 0, sizeof(g_mine));
    memset(g_adj, 0, sizeof(g_adj));
    memset(g_open, 0, sizeof(g_open));
    memset(g_flag, 0, sizeof(g_flag));
    g_generated = g_over = g_won = 0;
    g_flags = g_opened = 0;
    g_boom_r = g_boom_c = -1;
    g_seconds = 0;
}

/*
 * Расставить мины, обходя клетку первого клика и её соседей
 * (классическое "первый клик всегда безопасен"), и посчитать
 * числа-подсказки. Случайные числа - rand() мини-libc; затравку
 * main берёт из getrandom ядра, чтобы расклад менялся от запуска
 * к запуску.
 */
static void generate(int safe_r, int safe_c)
{
    int placed = 0;

    while (placed < MINES) {

        unsigned int rv = rand();
        int r = (int)(rv % ROWS);
        int c = (int)((rv / ROWS) % COLS);
        int dr = r - safe_r, dc = c - safe_c;

        if (dr >= -1 && dr <= 1 && dc >= -1 && dc <= 1)
            continue;

        if (g_mine[r][c])
            continue;

        g_mine[r][c] = 1;
        placed++;
    }

    for (int r = 0; r < ROWS; r++)
        for (int c = 0; c < COLS; c++) {
            int n = 0;
            for (int dr = -1; dr <= 1; dr++)
                for (int dc = -1; dc <= 1; dc++) {
                    int rr = r + dr, cc = c + dc;
                    if ((dr || dc) && rr >= 0 && rr < ROWS && cc >= 0 && cc < COLS)
                        n += g_mine[rr][cc];
                }
            g_adj[r][c] = (unsigned char)n;
        }

    g_generated = 1;
    g_start_ms = uptime_ms();
}

/*
 * Открыть клетку. Клетка без мин вокруг "заливает" соседей - через
 * явный стек, без рекурсии (стек поля - не больше числа клеток).
 */
static void reveal(int r0, int c0)
{
    if (g_over)
        return;

    if (!g_generated)
        generate(r0, c0);

    if (g_flag[r0][c0] || g_open[r0][c0])
        return;

    if (g_mine[r0][c0]) {
        g_over = 1;
        g_boom_r = r0;
        g_boom_c = c0;
        /* проигрыш: показать все мины */
        for (int r = 0; r < ROWS; r++)
            for (int c = 0; c < COLS; c++)
                if (g_mine[r][c])
                    g_open[r][c] = 1;
        return;
    }

    unsigned char queued[ROWS][COLS];
    int sr[ROWS * COLS], sc[ROWS * COLS];
    int sp = 0;

    memset(queued, 0, sizeof(queued));
    sr[sp] = r0;
    sc[sp] = c0;
    sp++;
    queued[r0][c0] = 1;

    while (sp > 0) {

        sp--;
        int r = sr[sp], c = sc[sp];

        if (g_open[r][c] || g_flag[r][c])
            continue;

        g_open[r][c] = 1;
        g_opened++;

        if (g_adj[r][c] != 0)
            continue;

        for (int dr = -1; dr <= 1; dr++)
            for (int dc = -1; dc <= 1; dc++) {
                int rr = r + dr, cc = c + dc;
                if ((dr == 0 && dc == 0) || rr < 0 || rr >= ROWS || cc < 0 || cc >= COLS)
                    continue;
                if (g_open[rr][cc] || g_flag[rr][cc] || g_mine[rr][cc] || queued[rr][cc])
                    continue;
                queued[rr][cc] = 1;
                sr[sp] = rr;
                sc[sp] = cc;
                sp++;
            }
    }

    if (g_opened == ROWS * COLS - MINES) {
        g_won = g_over = 1;
        /* красиво доставить флажки на оставшиеся мины */
        for (int r = 0; r < ROWS; r++)
            for (int c = 0; c < COLS; c++)
                if (g_mine[r][c] && !g_flag[r][c]) {
                    g_flag[r][c] = 1;
                    g_flags++;
                }
        printf("mines: won in %d s\n", g_seconds);
    }
}

static void toggle_flag(int r, int c)
{
    if (g_over || g_open[r][c])
        return;

    if (g_flag[r][c]) {
        g_flag[r][c] = 0;
        g_flags--;
    } else if (g_flags < MINES) {
        g_flag[r][c] = 1;
        g_flags++;
    }
}


/* ================================================================
 * Окно
 * ================================================================ */

static int grid_x(void) { return (W - COLS * CELL) / 2; }
static int face_x(void) { return W / 2 - 11; }

static void paint(GFX *g)
{
    gfx_fill(g, 0, 0, g->w, g->h, GFX_FACE);

    /* верхняя панель: мины без флажка, смайлик, секунды */
    int top = 6;
    char n1[8], n2[8];
    int left = MINES - (g_flags > MINES ? MINES : g_flags);

    snprintf(n1, sizeof(n1), "%03d", left);
    snprintf(n2, sizeof(n2), "%03d", g_seconds > 999 ? 999 : g_seconds);

    gfx_fill(g, 8, top, 34, 18, GFX_BLACK);
    gfx_text(g, 10, top + 1, n1, 0xFF0000u);
    gfx_fill(g, W - 42, top, 34, 18, GFX_BLACK);
    gfx_text(g, W - 40, top + 1, n2, 0xFF0000u);

    gfx_button(g, face_x(), top, 22, 22, 0);
    gfx_text(g, face_x() + 3, top + 4, g_over ? (g_won ? ":)" : ":(") : ":|", GFX_BLACK);

    /* поле */
    int gx = grid_x();

    gfx_bevel(g, gx - 2, TOP - 2, COLS * CELL + 4, ROWS * CELL + 4, 0);

    static const unsigned int num_col[9] = {
        0, 0x0000FF, 0x008000, 0xFF0000, 0x000080, 0x800000, 0x008080, 0x000000, 0x808080
    };

    for (int r = 0; r < ROWS; r++)
        for (int c = 0; c < COLS; c++) {

            int x = gx + c * CELL, y = TOP + r * CELL;

            if (g_open[r][c]) {
                gfx_fill(g, x, y, CELL, CELL, GFX_FACE);
                gfx_fill(g, x, y, CELL, 1, GFX_SHADOW);
                gfx_fill(g, x, y, 1, CELL, GFX_SHADOW);
                if (g_mine[r][c]) {
                    if (r == g_boom_r && c == g_boom_c)
                        gfx_fill(g, x + 1, y + 1, CELL - 1, CELL - 1, 0xFF0000u);
                    gfx_fill(g, x + 5, y + 5, CELL - 9, CELL - 9, GFX_BLACK);
                } else if (g_adj[r][c]) {
                    char d[2] = { (char)('0' + g_adj[r][c]), 0 };
                    gfx_text(g, x + 5, y + 1, d, num_col[g_adj[r][c]]);
                }
            } else {
                gfx_button(g, x, y, CELL, CELL, 0);
                if (g_flag[r][c])
                    gfx_text(g, x + 5, y + 1, "!", 0xFF0000u);
            }
        }
}

/* 1 - перерисовать, -1 - закрыть */
static int on_event(const struct myos_event *e)
{
    if (e->type == EV_CLOSE)
        return -1;

    if (e->type == EV_KEY) {
        if (e->scan == KEY_ESC)
            return -1;
        if (e->key == 'n' || e->key == 'N' || e->key == 0x442 || e->key == 0x422) {
            reset();             /* N (или Т - та же клавиша по-русски) */
            return 1;
        }
        return 0;
    }

    if (e->type != EV_DOWN)
        return 0;

    /* смайлик - новая игра */
    if (e->y >= 6 && e->y < 28 && e->x >= face_x() && e->x < face_x() + 22) {
        reset();
        return 1;
    }

    int c = (e->x - grid_x()) / CELL;
    int r = (e->y - TOP) / CELL;

    if (e->x < grid_x() || e->y < TOP || r >= ROWS || c >= COLS)
        return 0;

    if (e->buttons & 2u)
        toggle_flag(r, c);
    else
        reveal(r, c);

    return 1;
}

int main(void)
{
    unsigned int seed;

    if (getrandom(&seed, sizeof(seed)) != (long)sizeof(seed))
        seed = (unsigned int)uptime_ms();
    srand(seed);
    reset();

    int id = win_create(W, H, "Сапёр");

    if (id < 0) {
        printf("mines: %s\n", strerror(id));
        return 1;
    }

    GFX g;
    gfx_init(&g, win_pixels(id), W, H);
    paint(&g);
    win_update(id);

    for (;;) {

        struct myos_event e;
        int redraw = 0;

        /* пока идёт игра - просыпаться каждые 200 мс ради секундомера */
        if (win_event(id, &e, 200)) {
            int r = on_event(&e);
            if (r < 0) {
                win_close(id);
                return 0;
            }
            redraw = r;
        }

        if (g_generated && !g_over) {
            int s = (int)((uptime_ms() - g_start_ms) / 1000);
            if (s != g_seconds) {
                g_seconds = s;
                redraw = 1;
            }
        }

        if (redraw) {
            paint(&g);
            win_update(id);
        }
    }
}
