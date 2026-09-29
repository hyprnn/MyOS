/*
 * gui/gfx.c - рисование для оконной системы (этап 7).
 * Часть MyOS; общие объявления - в myos.h.
 *
 * "Поверхность" (GFX) - прямоугольник пикселей в памяти: буфер окна,
 * задний буфер экрана. Цвет пикселя всегда 0x00RRGGBB (красный в
 * старших битах) - в какой порядок байт его переложить для
 * видеокарты, решает только вывод на экран (wm.c).
 *
 * У поверхности есть "отсечение" (clip) - прямоугольник, за который
 * рисование не выходит. Композитор перерисовывает экран кусками: для
 * каждого изменившегося прямоугольника ставит clip и рисует рабочий
 * стол, окна, панель задач - всё, что вылезло бы за кусок, отсекается.
 *
 * Текст - в UTF-8 (так пишутся строки в исходниках и в файлах), шрифт
 * 8x16 сглаженный (font8x16.h: 16 оттенков на точку, цвет смешивается
 * с фоном - края букв мягкие). Латиница и кириллица.
 */
#include "myos.h"
#include "font8x16.h"


/* ================================================================
 * Поверхность и отсечение
 * ================================================================ */

void gfx_init(GFX *g, UINT32 *px, UINT32 w, UINT32 h, UINT32 stride)
{
    g->px = px;
    g->w = w;
    g->h = h;
    g->stride = stride;
    gfx_noclip(g);
}

void gfx_noclip(GFX *g)
{
    g->cx0 = 0;
    g->cy0 = 0;
    g->cx1 = (INT32)g->w;
    g->cy1 = (INT32)g->h;
}

/* Отсечение = пересечение прямоугольника с поверхностью */
void gfx_clip(GFX *g, INT32 x, INT32 y, INT32 w, INT32 h)
{
    g->cx0 = (x < 0) ? 0 : x;
    g->cy0 = (y < 0) ? 0 : y;
    g->cx1 = x + w;
    g->cy1 = y + h;

    if (g->cx1 > (INT32)g->w) g->cx1 = (INT32)g->w;
    if (g->cy1 > (INT32)g->h) g->cy1 = (INT32)g->h;
    if (g->cx1 < g->cx0) g->cx1 = g->cx0;
    if (g->cy1 < g->cy0) g->cy1 = g->cy0;
}

void gfx_fill(GFX *g, INT32 x, INT32 y, INT32 w, INT32 h, UINT32 col)
{
    INT32 x0 = x, y0 = y, x1 = x + w, y1 = y + h;

    if (x0 < g->cx0) x0 = g->cx0;
    if (y0 < g->cy0) y0 = g->cy0;
    if (x1 > g->cx1) x1 = g->cx1;
    if (y1 > g->cy1) y1 = g->cy1;

    if (x0 >= x1 || y0 >= y1)
        return;

    for (INT32 yy = y0; yy < y1; yy++) {
        UINT32 *row = g->px + (UINTN)yy * g->stride;
        for (INT32 xx = x0; xx < x1; xx++)
            row[xx] = col;
    }
}

void gfx_pixel(GFX *g, INT32 x, INT32 y, UINT32 col)
{
    if (x >= g->cx0 && x < g->cx1 && y >= g->cy0 && y < g->cy1)
        g->px[(UINTN)y * g->stride + (UINTN)x] = col;
}

/* Смешать цвет c поверх пикселя с прозрачностью a (0..15) */
static void gfx_blend(GFX *g, INT32 x, INT32 y, UINT32 c, UINT32 a)
{
    if (a == 0 || x < g->cx0 || x >= g->cx1 || y < g->cy0 || y >= g->cy1)
        return;

    UINT32 *p = &g->px[(UINTN)y * g->stride + (UINTN)x];

    if (a >= 15) {
        *p = c;
        return;
    }

    UINT32 d = *p;
    UINT32 r = (((c >> 16) & 0xFF) * a + ((d >> 16) & 0xFF) * (15 - a)) / 15;
    UINT32 gg = (((c >> 8) & 0xFF) * a + ((d >> 8) & 0xFF) * (15 - a)) / 15;
    UINT32 b = ((c & 0xFF) * a + (d & 0xFF) * (15 - a)) / 15;

    *p = (r << 16) | (gg << 8) | b;
}

/* Прямоугольник другой картинки (w x h, строка src_stride) в (x, y) */
void gfx_blit(GFX *g, INT32 x, INT32 y, const UINT32 *src, INT32 w, INT32 h, UINT32 src_stride)
{
    INT32 x0 = x, y0 = y, x1 = x + w, y1 = y + h;

    if (x0 < g->cx0) x0 = g->cx0;
    if (y0 < g->cy0) y0 = g->cy0;
    if (x1 > g->cx1) x1 = g->cx1;
    if (y1 > g->cy1) y1 = g->cy1;

    if (x0 >= x1 || y0 >= y1)
        return;

    for (INT32 yy = y0; yy < y1; yy++) {
        const UINT32 *s = src + (UINTN)(yy - y) * src_stride + (UINTN)(x0 - x);
        UINT32 *d = g->px + (UINTN)yy * g->stride + (UINTN)x0;
        for (INT32 k = 0; k < x1 - x0; k++)
            d[k] = s[k];
    }
}


/* ================================================================
 * Объёмные рамки в духе Windows 95
 * ================================================================ */

#define C_WHITE  0xFFFFFFu
#define C_LIGHT  0xDFDFDFu
#define C_FACE   0xC0C0C0u
#define C_SHADOW 0x808080u
#define C_BLACK  0x000000u

/*
 * Рамка в 2 пикселя: выпуклая (raised) - светлый верх-лево, тёмный
 * низ-право; вдавленная - наоборот. Так в 1995 году рисовалась любая
 * кнопка и окно.
 */
void gfx_bevel(GFX *g, INT32 x, INT32 y, INT32 w, INT32 h, BOOLEAN raised)
{
    UINT32 tl1 = raised ? C_LIGHT : C_SHADOW;
    UINT32 tl2 = raised ? C_WHITE : C_BLACK;
    UINT32 br1 = raised ? C_BLACK : C_WHITE;
    UINT32 br2 = raised ? C_SHADOW : C_LIGHT;

    gfx_fill(g, x, y, w, 1, tl1);
    gfx_fill(g, x, y, 1, h, tl1);
    gfx_fill(g, x + 1, y + 1, w - 2, 1, tl2);
    gfx_fill(g, x + 1, y + 1, 1, h - 2, tl2);
    gfx_fill(g, x, y + h - 1, w, 1, br1);
    gfx_fill(g, x + w - 1, y, 1, h, br1);
    gfx_fill(g, x + 1, y + h - 2, w - 2, 1, br2);
    gfx_fill(g, x + w - 2, y + 1, 1, h - 2, br2);
}

/* Серая кнопка: заливка + рамка */
void gfx_button(GFX *g, INT32 x, INT32 y, INT32 w, INT32 h, BOOLEAN pressed)
{
    gfx_fill(g, x, y, w, h, C_FACE);
    gfx_bevel(g, x, y, w, h, !pressed);
}


/* ================================================================
 * Текст
 * ================================================================ */

/* Следующий символ Unicode из строки UTF-8 (сдвигает *s) */
UINT32 utf8_next(const char **s)
{
    const UINT8 *p = (const UINT8 *)*s;
    UINT32 c = p[0];

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
        return ((c & 0x0F) << 12) | ((UINT32)(p[1] & 0x3F) << 6) | (p[2] & 0x3F);
    }

    /* 4 байта (эмодзи и т.п.) или мусор - пропустить байт */
    *s += 1;
    return '?';
}

/* Символ -> UTF-8 (в out, до 3 байт + 0); вернуть длину */
UINTN utf8_put(UINT32 c, char *out)
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
UINTN utf8_len(const char *s)
{
    UINTN n = 0;

    while (utf8_next(&s) != 0)
        n++;

    return n;
}

/* Номер символа в шрифте (поиск делением пополам); нет - '?' */
INTN font_index(UINT32 cp)
{
    INTN lo = 0, hi = FONT_COUNT - 1;

    while (lo <= hi) {
        INTN mid = (lo + hi) / 2;
        if (font_cp[mid] == cp)
            return mid;
        if (font_cp[mid] < cp)
            lo = mid + 1;
        else
            hi = mid - 1;
    }

    return (cp == '?') ? -1 : font_index('?');
}

/* Маска символа: яркость точки (x, y) 0..15 */
UINT32 font_alpha(INTN idx, UINT32 x, UINT32 y)
{
    UINT8 b = font_bits[idx][y * 4u + x / 2u];

    return (x & 1u) ? (b & 0x0Fu) : (b >> 4);
}

void gfx_glyph(GFX *g, INT32 x, INT32 y, UINT32 cp, UINT32 col)
{
    if (cp == ' ')
        return;

    INTN idx = font_index(cp);

    if (idx < 0)
        return;

    for (UINT32 yy = 0; yy < FONT_H; yy++)
        for (UINT32 xx = 0; xx < FONT_W; xx++)
            gfx_blend(g, x + (INT32)xx, y + (INT32)yy, col, font_alpha(idx, xx, yy));
}

/* Символ, повёрнутый на 90 градусов против часовой (для боковой
   полосы меню "Пуск" - надпись снизу вверх) */
void gfx_glyph_up(GFX *g, INT32 x, INT32 y, UINT32 cp, UINT32 col)
{
    INTN idx = font_index(cp);

    if (idx < 0 || cp == ' ')
        return;

    for (UINT32 yy = 0; yy < FONT_H; yy++)
        for (UINT32 xx = 0; xx < FONT_W; xx++)
            gfx_blend(g, x + (INT32)yy, y - (INT32)xx, col, font_alpha(idx, xx, yy));
}

/* Строка UTF-8; вернуть ширину в пикселях */
INT32 gfx_text(GFX *g, INT32 x, INT32 y, const char *s, UINT32 col)
{
    INT32 x0 = x;
    UINT32 c;

    while ((c = utf8_next(&s)) != 0) {
        gfx_glyph(g, x, y, c, col);
        x += FONT_W;
    }

    return x - x0;
}

/* То же, но не шире max_chars символов (длинное - с "…") */
INT32 gfx_text_fit(GFX *g, INT32 x, INT32 y, const char *s, UINT32 col, UINTN max_chars)
{
    UINTN n = utf8_len(s);
    INT32 x0 = x;
    UINT32 c;

    if (max_chars == 0)
        return 0;

    for (UINTN i = 0; (c = utf8_next(&s)) != 0; i++) {

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
INT32 gfx_text_bold(GFX *g, INT32 x, INT32 y, const char *s, UINT32 col)
{
    gfx_text(g, x + 1, y, s, col);
    return gfx_text(g, x, y, s, col) + 1;
}

INT32 gfx_text_width(const char *s)
{
    return (INT32)utf8_len(s) * FONT_W;
}


/* ================================================================
 * Значки 16x16 (буквы-цвета, палитра Windows 95)
 * ================================================================ */

static BOOLEAN icon_color(char c, UINT32 *col)
{
    switch (c) {
    case 'k': *col = 0x000000; return TRUE;
    case 'w': *col = 0xFFFFFF; return TRUE;
    case 'g': *col = 0xC0C0C0; return TRUE;
    case 'd': *col = 0x808080; return TRUE;
    case 'b': *col = 0x000080; return TRUE;
    case 'B': *col = 0x0000FF; return TRUE;
    case 't': *col = 0x008080; return TRUE;
    case 'c': *col = 0x00FFFF; return TRUE;
    case 'y': *col = 0xFFFF00; return TRUE;
    case 'o': *col = 0x808000; return TRUE;
    case 'r': *col = 0xFF0000; return TRUE;
    case 'm': *col = 0x800000; return TRUE;
    case 'G': *col = 0x00FF00; return TRUE;
    case 'n': *col = 0x008000; return TRUE;
    case 'p': *col = 0x800080; return TRUE;
    case 'P': *col = 0xFF00FF; return TRUE;
    default:  return FALSE;          /* '.' - прозрачно */
    }
}

/*
 * Значок в (x, y), каждый "пиксель" значка - квадрат scale x scale.
 * selected - "сеточка" тёмно-синего поверх (выделенный значок в Win95).
 */
void gfx_icon(GFX *g, INT32 x, INT32 y, const char *const *rows, UINT32 scale, BOOLEAN selected)
{
    for (UINT32 r = 0; r < 16; r++) {
        for (UINT32 c = 0; c < 16; c++) {

            UINT32 col;

            if (!icon_color(rows[r][c], &col))
                continue;

            for (UINT32 dy = 0; dy < scale; dy++)
                for (UINT32 dx = 0; dx < scale; dx++) {
                    UINT32 px = col;
                    if (selected && (((c * scale + dx) + (r * scale + dy)) & 1u))
                        px = 0x000080;
                    gfx_pixel(g, x + (INT32)(c * scale + dx), y + (INT32)(r * scale + dy), px);
                }
        }
    }
}
