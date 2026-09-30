/*
 * user/lib/gfx.c - рисование в окне программы (этап 10, Д4).
 *
 * То же, что gui/gfx.c в ядре, но для программ: Блокнот, Сапёр и
 * "О системе" раньше жили в ядре и рисовали его функциями; теперь это
 * программы, и рисуют они сами в буфер своего окна (win_pixels) - а
 * ядро только показывает готовые пиксели. Имена и поведение функций
 * те же, что у ядра, поэтому код окон перенесён почти без изменений.
 *
 * Шрифт - тот же 8x16 сглаженный с кириллицей (font8x16.h общий для
 * ядра и программ): 16 оттенков на точку, цвет смешивается с фоном.
 * Значки 16x16 - тоже общие (icons16.h).
 */
#include "myos.h"
#include "../../font8x16.h"
#include "../../icons16.h"


/* ================================================================
 * Поверхность и отсечение
 * ================================================================ */

void gfx_init(GFX *g, unsigned int *px, int w, int h)
{
    g->px = px;
    g->w = w;
    g->h = h;
    g->stride = w;
    gfx_noclip(g);
}

void gfx_noclip(GFX *g)
{
    g->cx0 = 0;
    g->cy0 = 0;
    g->cx1 = g->w;
    g->cy1 = g->h;
}

/* Отсечение = пересечение прямоугольника с поверхностью: всё, что
   рисуется дальше, не вылезет за него (поле текста в Блокноте) */
void gfx_clip(GFX *g, int x, int y, int w, int h)
{
    g->cx0 = (x < 0) ? 0 : x;
    g->cy0 = (y < 0) ? 0 : y;
    g->cx1 = x + w;
    g->cy1 = y + h;

    if (g->cx1 > g->w) g->cx1 = g->w;
    if (g->cy1 > g->h) g->cy1 = g->h;
    if (g->cx1 < g->cx0) g->cx1 = g->cx0;
    if (g->cy1 < g->cy0) g->cy1 = g->cy0;
}

void gfx_fill(GFX *g, int x, int y, int w, int h, unsigned int col)
{
    int x0 = x, y0 = y, x1 = x + w, y1 = y + h;

    if (x0 < g->cx0) x0 = g->cx0;
    if (y0 < g->cy0) y0 = g->cy0;
    if (x1 > g->cx1) x1 = g->cx1;
    if (y1 > g->cy1) y1 = g->cy1;

    if (x0 >= x1 || y0 >= y1)
        return;

    for (int yy = y0; yy < y1; yy++) {
        unsigned int *row = g->px + (long)yy * g->stride;
        for (int xx = x0; xx < x1; xx++)
            row[xx] = col;
    }
}

void gfx_pixel(GFX *g, int x, int y, unsigned int col)
{
    if (x >= g->cx0 && x < g->cx1 && y >= g->cy0 && y < g->cy1)
        g->px[(long)y * g->stride + x] = col;
}

/* Смешать цвет c поверх пикселя с прозрачностью a (0..15) */
static void gfx_blend(GFX *g, int x, int y, unsigned int c, unsigned int a)
{
    if (a == 0 || x < g->cx0 || x >= g->cx1 || y < g->cy0 || y >= g->cy1)
        return;

    unsigned int *p = &g->px[(long)y * g->stride + x];

    if (a >= 15) {
        *p = c;
        return;
    }

    unsigned int d = *p;
    unsigned int r = (((c >> 16) & 0xFF) * a + ((d >> 16) & 0xFF) * (15 - a)) / 15;
    unsigned int gg = (((c >> 8) & 0xFF) * a + ((d >> 8) & 0xFF) * (15 - a)) / 15;
    unsigned int b = ((c & 0xFF) * a + (d & 0xFF) * (15 - a)) / 15;

    *p = (r << 16) | (gg << 8) | b;
}


/* ================================================================
 * Объёмные рамки в духе Windows 95 (как у окон ядра)
 * ================================================================ */

static unsigned int lerp(unsigned int c0, unsigned int c1, unsigned int t)   /* t: 0..256 */
{
    unsigned int r = (((c0 >> 16) & 0xFF) * (256 - t) + ((c1 >> 16) & 0xFF) * t) >> 8;
    unsigned int g = (((c0 >> 8) & 0xFF) * (256 - t) + ((c1 >> 8) & 0xFF) * t) >> 8;
    unsigned int b = ((c0 & 0xFF) * (256 - t) + (c1 & 0xFF) * t) >> 8;
    return (r << 16) | (g << 8) | b;
}

/* контур со срезанными угловыми пикселями (скругление в 1 пиксель) */
static void soft_frame(GFX *g, int x, int y, int w, int h, unsigned int col)
{
    gfx_fill(g, x + 1, y, w - 2, 1, col);
    gfx_fill(g, x + 1, y + h - 1, w - 2, 1, col);
    gfx_fill(g, x, y + 1, 1, h - 2, col);
    gfx_fill(g, x + w - 1, y + 1, 1, h - 2, col);
}

/*
 * Рамки и кнопки в стиле Frutiger Aero (как у ядра, gui/gfx.c):
 * выпуклая - светлая кромка, "вдавленное" поле - голубовато-серый
 * контур, как у полей ввода Vista.
 */
void gfx_bevel(GFX *g, int x, int y, int w, int h, int raised)
{
    if (w < 3 || h < 3)
        return;

    if (raised) {
        soft_frame(g, x, y, w, h, 0x9DB1C6u);
        soft_frame(g, x + 1, y + 1, w - 2, h - 2, GFX_WHITE);
    } else {
        soft_frame(g, x, y, w, h, 0x7F9DB9u);
        gfx_fill(g, x + 1, y + 1, w - 2, 1, 0xDCE4EDu);
    }
}

/* Кнопка: градиент почти белый -> светло-серо-голубой; нажатая - голубая */
void gfx_button(GFX *g, int x, int y, int w, int h, int pressed)
{
    unsigned int top = pressed ? 0xC9E4F8u : 0xFCFDFEu;
    unsigned int bot = pressed ? 0x8DC0EAu : 0xD9E3EDu;

    for (int j = 0; j < h; j++)
        gfx_fill(g, x, y + j, w, 1, lerp(top, bot, h > 1 ? (unsigned int)(j * 256 / (h - 1)) : 0));

    if (!pressed && h > 6)
        gfx_fill(g, x + 1, y + 1, w - 2, 1, GFX_WHITE);

    soft_frame(g, x, y, w, h, pressed ? 0x3C7FB1u : 0x8397ACu);
}


/* ================================================================
 * Текст (UTF-8)
 * ================================================================ */

/* Следующий символ Unicode из строки UTF-8 (сдвигает *s) */
unsigned int utf8_next(const char **s)
{
    const unsigned char *p = (const unsigned char *)*s;
    unsigned int c = p[0];

    if (c == 0)
        return 0;

    if (c < 0x80) {
        *s += 1;
        return c;
    }

    if ((c & 0xE0) == 0xC0 && (p[1] & 0xC0) == 0x80) {
        *s += 2;
        return ((c & 0x1F) << 6) | (p[1] & 0x3F);
    }

    if ((c & 0xF0) == 0xE0 && (p[1] & 0xC0) == 0x80 && (p[2] & 0xC0) == 0x80) {
        *s += 3;
        return ((c & 0x0F) << 12) | ((unsigned int)(p[1] & 0x3F) << 6) | (p[2] & 0x3F);
    }

    /* 4 байта (эмодзи и т.п.) или мусор - пропустить байт */
    *s += 1;
    return '?';
}

/* Символ -> UTF-8 (в out, до 3 байт + 0); вернуть длину */
int utf8_put(unsigned int c, char *out)
{
    if (c < 0x80) {
        out[0] = (char)c;
        out[1] = 0;
        return 1;
    }

    if (c < 0x800) {
        out[0] = (char)(0xC0 | (c >> 6));
        out[1] = (char)(0x80 | (c & 0x3F));
        out[2] = 0;
        return 2;
    }

    out[0] = (char)(0xE0 | (c >> 12));
    out[1] = (char)(0x80 | ((c >> 6) & 0x3F));
    out[2] = (char)(0x80 | (c & 0x3F));
    out[3] = 0;
    return 3;
}

/* Сколько символов (не байт) в строке UTF-8 */
int utf8_len(const char *s)
{
    int n = 0;

    while (utf8_next(&s) != 0)
        n++;

    return n;
}

/* Номер символа в шрифте (поиск делением пополам); нет - '?' */
static int font_index(unsigned int cp)
{
    int lo = 0, hi = FONT_COUNT - 1;

    while (lo <= hi) {
        int mid = (lo + hi) / 2;
        if (font_cp[mid] == cp)
            return mid;
        if (font_cp[mid] < cp)
            lo = mid + 1;
        else
            hi = mid - 1;
    }

    return (cp == '?') ? -1 : font_index('?');
}

void gfx_glyph(GFX *g, int x, int y, unsigned int cp, unsigned int col)
{
    if (cp == ' ')
        return;

    int idx = font_index(cp);

    if (idx < 0)
        return;

    /* в байте две точки: старшие 4 бита - левая */
    for (int yy = 0; yy < FONT_H; yy++)
        for (int xx = 0; xx < FONT_W; xx++) {
            unsigned char b = font_bits[idx][yy * 4 + xx / 2];
            unsigned int a = (xx & 1) ? (b & 0x0Fu) : (unsigned int)(b >> 4);
            gfx_blend(g, x + xx, y + yy, col, a);
        }
}

/* Строка UTF-8; вернуть ширину в пикселях */
int gfx_text(GFX *g, int x, int y, const char *s, unsigned int col)
{
    int x0 = x;
    unsigned int c;

    while ((c = utf8_next(&s)) != 0) {
        gfx_glyph(g, x, y, c, col);
        x += FONT_W;
    }

    return x - x0;
}

/* То же, но не шире max_chars символов (длинное - с "…") */
int gfx_text_fit(GFX *g, int x, int y, const char *s, unsigned int col, int max_chars)
{
    int n = utf8_len(s);
    int x0 = x;
    unsigned int c;

    if (max_chars <= 0)
        return 0;

    for (int i = 0; (c = utf8_next(&s)) != 0; i++) {

        if (n > max_chars && i == max_chars - 1) {
            gfx_glyph(g, x, y, 0x2026, col);
            x += FONT_W;
            break;
        }

        gfx_glyph(g, x, y, c, col);
        x += FONT_W;
    }

    return x - x0;
}

/* "Жирный" текст - дважды со сдвигом на пиксель (как заголовки Win95) */
int gfx_text_bold(GFX *g, int x, int y, const char *s, unsigned int col)
{
    gfx_text(g, x + 1, y, s, col);
    return gfx_text(g, x, y, s, col) + 1;
}

int gfx_text_width(const char *s)
{
    return utf8_len(s) * FONT_W;
}


/* ================================================================
 * Значки 16x16 (буквы-цвета, палитра Windows 95; icons16.h)
 * ================================================================ */

const char *const *gfx_icon_rows(const char *name)
{
    return icon16_find(name);
}

/*
 * Значок в (x, y), каждый "пиксель" значка - квадрат scale x scale.
 * selected - "сеточка" тёмно-синего поверх (выделенный значок в Win95).
 */
void gfx_icon(GFX *g, int x, int y, const char *const *rows, int scale, int selected)
{
    for (int r = 0; r < 16; r++) {
        for (int c = 0; c < 16; c++) {

            unsigned int col;

            if (!icon16_color(rows[r][c], &col))
                continue;

            for (int dy = 0; dy < scale; dy++)
                for (int dx = 0; dx < scale; dx++) {
                    unsigned int px = col;
                    if (selected && (((c * scale + dx) + (r * scale + dy)) & 1))
                        px = 0x000080;
                    gfx_pixel(g, x + c * scale + dx, y + r * scale + dy, px);
                }
        }
    }
}
