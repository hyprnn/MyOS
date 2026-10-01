/*
 * gui/aero.c - стиль Frutiger Aero (этап 10, Е).
 * Часть MyOS; общие объявления - в myos.h.
 *
 * Frutiger Aero - облик компьютеров конца 2000-х (Windows Vista и 7,
 * заставки и реклама тех лет): стекло, глянец, небо, трава, вода и
 * пузыри. Здесь - кирпичики этого облика для композитора (gui/wm.c):
 *   * полупрозрачная заливка и вертикальный градиент - "стекло": цвет
 *     смешивается с тем, что уже нарисовано позади (композитор рисует
 *     снизу вверх, так что позади - обои и нижние окна);
 *   * скругление углов со сглаживанием (краевой пиксель - частично);
 *   * мягкая тень под окном;
 *   * глянцевый "шарик" (кнопка Пуск) и блик на стекле;
 *   * обои: небо, солнце, холмы, волна, пузыри - рисуются один раз при
 *     запуске рабочего стола, дальше просто копируются.
 *
 * Дробные числа в ядре можно: регистры SSE сохраняются при каждом
 * прерывании (kx_isr_common), так что потоку композитора они не мешают.
 */
#include "myos.h"

/* ---------------- смешивание ---------------- */

/* цвет c поверх d, непрозрачность a (0..255) */
static inline UINT32 mix(UINT32 d, UINT32 c, UINT32 a)
{
    UINT32 r = (((c >> 16) & 0xFF) * a + ((d >> 16) & 0xFF) * (255 - a)) / 255;
    UINT32 g = (((c >> 8) & 0xFF) * a + ((d >> 8) & 0xFF) * (255 - a)) / 255;
    UINT32 b = ((c & 0xFF) * a + (d & 0xFF) * (255 - a)) / 255;

    return (r << 16) | (g << 8) | b;
}

/* цвет между c0 и c1: t = 0..256 */
static inline UINT32 lerp(UINT32 c0, UINT32 c1, UINT32 t)
{
    UINT32 r = (((c0 >> 16) & 0xFF) * (256 - t) + ((c1 >> 16) & 0xFF) * t) >> 8;
    UINT32 g = (((c0 >> 8) & 0xFF) * (256 - t) + ((c1 >> 8) & 0xFF) * t) >> 8;
    UINT32 b = ((c0 & 0xFF) * (256 - t) + (c1 & 0xFF) * t) >> 8;

    return (r << 16) | (g << 8) | b;
}

static inline void px_blend(GFX *g, INT32 x, INT32 y, UINT32 c, UINT32 a)
{
    if (a == 0 || x < g->cx0 || x >= g->cx1 || y < g->cy0 || y >= g->cy1)
        return;

    UINT32 *p = &g->px[(UINTN)y * g->stride + (UINTN)x];
    *p = (a >= 255) ? c : mix(*p, c, a);
}

/* квадратный корень (Ньютон) - libm в ядре нет */
static double f_sqrt(double v)
{
    if (v <= 0)
        return 0;

    double x = v > 1 ? v : 1;

    for (int i = 0; i < 24; i++)
        x = 0.5 * (x + v / x);

    return x;
}

static double f_sin(double x)
{
    const double PI = 3.14159265358979;

    /* привести к [-pi, pi] */
    while (x > PI)
        x -= 2 * PI;
    while (x < -PI)
        x += 2 * PI;

    /* ряд Тейлора до x^11 - точности хватает с запасом */
    double x2 = x * x, t = x, s = x;

    for (int n = 1; n <= 5; n++) {
        t *= -x2 / (double)((2 * n) * (2 * n + 1));
        s += t;
    }

    return s;
}

/* ---------------- фигуры ---------------- */

/*
 * Сколько пикселей отступить от края в строке row (0 - верхняя) прямо-
 * угольника со скруглением радиуса r, и насколько закрыт краевой
 * пиксель (0..255) - для сглаживания.
 */
static void round_inset(INT32 row, INT32 r, INT32 *inset, UINT32 *edge_a)
{
    if (row >= r) {
        *inset = 0;
        *edge_a = 255;
        return;
    }

    double dy = (double)r - (double)row - 0.5;
    double dx = (double)r - f_sqrt((double)r * r - dy * dy);   /* отступ, дробный */
    INT32 i = (INT32)dx;

    *inset = i;
    *edge_a = (UINT32)((1.0 - (dx - i)) * 255.0);
}

/*
 * Прямоугольник с вертикальным градиентом top -> bot, непрозрачностью a
 * и скруглёнными углами: rt - радиус верхних, rb - нижних (0 - прямые).
 */
void aero_panel(GFX *g, INT32 x, INT32 y, INT32 w, INT32 h,
                UINT32 top, UINT32 bot, UINT32 a, INT32 rt, INT32 rb)
{
    for (INT32 j = 0; j < h; j++) {

        UINT32 c = lerp(top, bot, h > 1 ? (UINT32)(j * 256 / (h - 1)) : 0);
        INT32 in = 0;
        UINT32 ea = 255;

        if (j < rt)
            round_inset(j, rt, &in, &ea);
        else if (h - 1 - j < rb)
            round_inset(h - 1 - j, rb, &in, &ea);

        if (in * 2 >= w)
            continue;

        /* сглаженные краевые пиксели */
        px_blend(g, x + in, y + j, c, a * ea / 255);
        px_blend(g, x + w - 1 - in, y + j, c, a * ea / 255);

        for (INT32 i = in + 1; i < w - 1 - in; i++)
            px_blend(g, x + i, y + j, c, a);
    }
}

/* отступ строки j прямоугольника высотой h: rt - радиус верха, rb - низа */
static INT32 row_inset(INT32 j, INT32 h, INT32 rt, INT32 rb)
{
    INT32 in = 0;
    UINT32 ea;

    if (j < 0 || j >= h)
        return 0;
    if (j < rt)
        round_inset(j, rt, &in, &ea);
    else if (h - 1 - j < rb)
        round_inset(h - 1 - j, rb, &in, &ea);

    return in;
}

/* Контур скруглённого прямоугольника (1 пиксель) */
void aero_outline(GFX *g, INT32 x, INT32 y, INT32 w, INT32 h, UINT32 col, UINT32 a, INT32 rt, INT32 rb)
{
    for (INT32 j = 0; j < h; j++) {

        INT32 in = row_inset(j, h, rt, rb);

        if (j == 0 || j == h - 1) {
            for (INT32 i = in; i < w - in; i++)
                px_blend(g, x + i, y + j, col, a);
            continue;
        }

        /* в углу контур идёт наискось: дотянуть до отступа соседней
           строки, ближней к краю */
        INT32 nb = (j < h / 2) ? row_inset(j - 1, h, rt, rb) : row_inset(j + 1, h, rt, rb);
        INT32 from = (nb < in) ? nb + 1 : in;

        for (INT32 i = from; i <= in; i++) {
            px_blend(g, x + i, y + j, col, a);
            px_blend(g, x + w - 1 - i, y + j, col, a);
        }
    }
}

/*
 * Размыть уже нарисованное в прямоугольнике - "матовое стекло" Vista:
 * сквозь заголовок видно, что позади, но не читается (текст окна позади
 * не мешает заголовку). Размытие "ящиком" радиуса r: сначала по строкам,
 * потом по столбцам, по два прохода (почти как гаусс). Скользящая сумма:
 * на каждый пиксель - одно сложение и одно вычитание, как бы ни был
 * велик радиус.
 */
static void blur_line(UINT32 *p, UINTN step, INT32 n, INT32 r, UINT32 *tmp)
{
    for (INT32 i = 0; i < n; i++)
        tmp[i] = p[(UINTN)i * step];

    UINT32 sr = 0, sg = 0, sb = 0;
    UINT32 cnt = (UINT32)(2 * r + 1);

    /* окно [-r, r] вокруг 0 (края - повтор крайнего пикселя) */
    for (INT32 k = -r; k <= r; k++) {
        UINT32 c = tmp[k < 0 ? 0 : k >= n ? n - 1 : k];
        sr += (c >> 16) & 0xFF;
        sg += (c >> 8) & 0xFF;
        sb += c & 0xFF;
    }

    for (INT32 i = 0; i < n; i++) {

        p[(UINTN)i * step] = ((sr / cnt) << 16) | ((sg / cnt) << 8) | (sb / cnt);

        /* сдвинуть окно: ушёл i-r, пришёл i+r+1 */
        UINT32 out = tmp[i - r < 0 ? 0 : i - r];
        UINT32 in = tmp[i + r + 1 >= n ? n - 1 : i + r + 1];

        sr += ((in >> 16) & 0xFF) - ((out >> 16) & 0xFF);
        sg += ((in >> 8) & 0xFF) - ((out >> 8) & 0xFF);
        sb += (in & 0xFF) - (out & 0xFF);
    }
}

void aero_blur(GFX *g, INT32 x, INT32 y, INT32 w, INT32 h, INT32 r)
{
    static UINT32 tmp[4096];

    /* только внутри отсечения */
    if (x < g->cx0) { w -= g->cx0 - x; x = g->cx0; }
    if (y < g->cy0) { h -= g->cy0 - y; y = g->cy0; }
    if (x + w > g->cx1) w = g->cx1 - x;
    if (y + h > g->cy1) h = g->cy1 - y;
    if (w <= 2 || h <= 2 || w > 4096 || h > 4096)
        return;

    UINT32 *base = &g->px[(UINTN)y * g->stride + (UINTN)x];

    for (int pass = 0; pass < 2; pass++) {
        for (INT32 j = 0; j < h; j++)
            blur_line(base + (UINTN)j * g->stride, 1, w, r, tmp);
        for (INT32 i = 0; i < w; i++)
            blur_line(base + i, g->stride, h, r, tmp);
    }
}

/* Мягкая тень вокруг прямоугольника (под окном) */
void aero_shadow(GFX *g, INT32 x, INT32 y, INT32 w, INT32 h)
{
    const INT32 S = 8;

    for (INT32 k = S; k >= 1; k--) {

        UINT32 a = (UINT32)(10 + (S - k) * 4);  /* ближе к окну - темнее */

        INT32 xx = x - k + 2, yy = y - k + 4, ww = w + 2 * k - 4, hh = h + 2 * k - 4;

        /* только кольцо шириной 1 */
        for (INT32 i = xx; i < xx + ww; i++) {
            px_blend(g, i, yy, 0x000000u, a / 3);
            px_blend(g, i, yy + hh - 1, 0x000000u, a);
        }
        for (INT32 j = yy + 1; j < yy + hh - 1; j++) {
            px_blend(g, xx, j, 0x000000u, a / 2);
            px_blend(g, xx + ww - 1, j, 0x000000u, a);
        }
    }
}

/* Глянцевый шарик: радиальный градиент от светлого верха к цвету base,
   белый блик сверху (кнопка "Пуск") */
void aero_orb(GFX *g, INT32 cx, INT32 cy, INT32 r, UINT32 base, UINT32 glow)
{
    for (INT32 j = -r; j <= r; j++)
        for (INT32 i = -r; i <= r; i++) {

            double d = f_sqrt((double)(i * i + j * j));

            if (d > r + 0.5)
                continue;

            UINT32 a = (d > r - 0.5) ? (UINT32)((r + 0.5 - d) * 255.0) : 255u;

            /* снизу - светлее (отражённый свет), к краю - темнее */
            double t = (double)(j + r) / (2.0 * r);
            UINT32 c = lerp(lerp(base, 0x000000u, 70), lerp(base, glow, 150), (UINT32)(t * t * 256.0));
            c = lerp(c, 0x000000u, (UINT32)(d / r * 60.0));

            px_blend(g, cx + i, cy + j, c, a);

            /* блик: верхний эллипс */
            double hx = i / (r * 0.72), hy = (j + r * 0.45) / (r * 0.48);
            double hd = hx * hx + hy * hy;
            if (hd < 1.0)
                px_blend(g, cx + i, cy + j, 0xFFFFFFu, (UINT32)((1.0 - hd) * 170.0));
        }
}

/* Блик на стекле: белая полоса, тающая сверху вниз */
void aero_gloss(GFX *g, INT32 x, INT32 y, INT32 w, INT32 h, UINT32 a_top)
{
    for (INT32 j = 0; j < h; j++) {
        UINT32 a = a_top * (UINT32)(h - j) / (UINT32)h;
        for (INT32 i = 0; i < w; i++)
            px_blend(g, x + i, y + j, 0xFFFFFFu, a);
    }
}

/* Надпись "со свечением": сначала мягкий ореол, потом сам текст */
void aero_text_glow(GFX *g, INT32 x, INT32 y, const char *s, UINT32 col, UINT32 glow)
{
    static const INT32 off[8][2] = {
        { -1, 0 }, { 1, 0 }, { 0, -1 }, { 0, 1 }, { -1, -1 }, { 1, 1 }, { -1, 1 }, { 1, -1 }
    };

    for (UINTN k = 0; k < 8; k++)
        gfx_text(g, x + off[k][0], y + off[k][1], s, glow);

    gfx_text(g, x, y, s, col);
}

/* ---------------- обои ---------------- */

/*
 * Нарисовать обои w x h в buf (0x00RRGGBB): небо от лазури к светлому
 * горизонту, сияние солнца справа сверху, "аква-волна" - прозрачная
 * лента через весь экран, два холма травы с бликом на гребне, пузыри.
 */
void aero_wallpaper(UINT32 *buf, UINT32 w, UINT32 h)
{
    GFX g;
    gfx_init(&g, buf, w, h, w);

    double W = w, H = h;
    double sunx = W * 0.80, suny = H * 0.16, sunr = H * 0.55;

    for (UINT32 y = 0; y < h; y++) {

        /* небо: сверху насыщенная лазурь, к горизонту светлее */
        double t = y / (H * 0.78);
        if (t > 1)
            t = 1;
        UINT32 sky = (t < 0.55) ? lerp(0x1662C4u, 0x4FA8EEu, (UINT32)(t / 0.55 * 256))
                                : lerp(0x4FA8EEu, 0xC9EEFFu, (UINT32)((t - 0.55) / 0.45 * 256));

        for (UINT32 x = 0; x < w; x++) {

            UINT32 c = sky;

            /* сияние солнца */
            double dx = x - sunx, dy = y - suny;
            double d = f_sqrt(dx * dx + dy * dy) / sunr;

            if (d < 1.0) {
                double k = (1.0 - d);
                c = lerp(c, 0xFFFFFFu, (UINT32)(k * k * 200.0));
            }

            buf[(UINTN)y * w + x] = c;
        }
    }

    /* аква-волна: две прозрачные ленты-синусоиды */
    for (UINT32 x = 0; x < w; x++) {

        double fx = x / W;

        for (int band = 0; band < 2; band++) {

            double cy = H * (0.50 + 0.06 * band) +
                        H * 0.07 * f_sin(fx * 6.2832 * (1.1 + 0.25 * band) + 0.8 + band * 1.7);
            double th = H * (0.030 + 0.018 * band);

            for (INT32 j = (INT32)(cy - th * 2); j <= (INT32)(cy + th * 2); j++) {
                if (j < 0 || j >= (INT32)h)
                    continue;
                double q = (j - cy) / th;
                double a = 1.0 / (1.0 + q * q * q * q) * (band ? 70.0 : 95.0);
                px_blend(&g, (INT32)x, j, 0xFFFFFFu, (UINT32)a);
            }
        }
    }

    /* холмы: дальний (светлый) и ближний (сочный), блик на гребне */
    for (int hill = 0; hill < 2; hill++) {

        UINT32 top = hill ? 0x6FD04Cu : 0x9FE07Au;
        UINT32 bottom = hill ? 0x1F7A26u : 0x4E9E3Au;

        for (UINT32 x = 0; x < w; x++) {

            double fx = x / W;
            double crest = hill ? H * (0.80 + 0.05 * f_sin(fx * 4.4 + 2.3) + 0.02 * f_sin(fx * 11.0))
                                : H * (0.70 + 0.06 * f_sin(fx * 3.1 + 0.4) + 0.015 * f_sin(fx * 9.0 + 1.0));

            for (INT32 y = (INT32)crest; y < (INT32)h; y++) {
                if (y < 0)
                    continue;
                double t = (y - crest) / (H - crest + 1);
                UINT32 c = lerp(top, bottom, (UINT32)(t * 256.0));
                /* сглаженный гребень */
                UINT32 a = (y == (INT32)crest) ? (UINT32)((1.0 - (crest - (INT32)crest)) * 255.0) : 255u;
                px_blend(&g, (INT32)x, y, c, a);
                /* блик вдоль гребня */
                if (y - crest < 10)
                    px_blend(&g, (INT32)x, y, 0xFFFFFFu, (UINT32)((10 - (y - crest)) * 9));
            }
        }
    }

    /* пузыри: полупрозрачные шары с ободком и бликом */
    UINT32 seed = 12345;

    for (int n = 0; n < 16; n++) {

        seed = seed * 1103515245u + 12345u;
        double bx = (seed >> 8) % w;
        seed = seed * 1103515245u + 12345u;
        double by = H * 0.08 + (double)((seed >> 8) % (UINT32)(H * 0.62));
        seed = seed * 1103515245u + 12345u;
        double br = H * (0.012 + ((seed >> 8) % 1000) / 1000.0 * 0.045);

        for (INT32 j = (INT32)(by - br - 1); j <= (INT32)(by + br + 1); j++)
            for (INT32 i = (INT32)(bx - br - 1); i <= (INT32)(bx + br + 1); i++) {

                double dx = i - bx, dy = j - by;
                double d = f_sqrt(dx * dx + dy * dy) / br;

                if (d > 1.0)
                    continue;

                /* ободок ярче, середина - лёгкая голубизна */
                double rim = d > 0.82 ? (d - 0.82) / 0.18 : 0.0;
                px_blend(&g, i, j, 0xE8F8FFu, (UINT32)(25 + rim * rim * 150));

                /* блик вверху слева */
                double hx = (dx + br * 0.35) / (br * 0.35), hy = (dy + br * 0.40) / (br * 0.25);
                double hd = hx * hx + hy * hy;
                if (hd < 1.0)
                    px_blend(&g, i, j, 0xFFFFFFu, (UINT32)((1.0 - hd) * 200.0));
            }
    }
}

/* ---------------- обои-картинка ---------------- */

extern const UINT8 g_wallpaper_rgb[], g_wallpaper_rgb_end[];
extern const UINT32 g_wallpaper_w, g_wallpaper_h;

/*
 * Обои-картинка (gui/wallpaper/, вшита в ядро) - растянуть под экран
 * w x h "с заполнением": масштаб такой, чтобы картинка закрыла весь
 * экран, лишнее по краям обрезается (у экрана 16:10 - чуть-чуть слева
 * и справа). Билинейная интерполяция: точка экрана - смесь четырёх
 * соседних точек картинки, без "лесенки". FALSE - картинки нет.
 */
BOOLEAN aero_wallpaper_image(UINT32 *buf, UINT32 w, UINT32 h)
{
    UINT32 iw = g_wallpaper_w, ih = g_wallpaper_h;

    if ((UINTN)(g_wallpaper_rgb_end - g_wallpaper_rgb) < (UINTN)iw * ih * 3u || iw == 0 || ih == 0)
        return FALSE;

    /* масштаб в 16.16: картинка/экран по той оси, где картинки "меньше" */
    UINT64 sx = ((UINT64)iw << 16) / w, sy = ((UINT64)ih << 16) / h;
    UINT64 s = sx < sy ? sx : sy;

    /* сдвиг, чтобы обрезка была поровну с двух сторон */
    UINT64 ox = (((UINT64)iw << 16) - s * w) / 2;
    UINT64 oy = (((UINT64)ih << 16) - s * h) / 2;

    for (UINT32 y = 0; y < h; y++) {

        UINT64 fy = oy + s * y;
        UINT32 y0 = (UINT32)(fy >> 16), ty = (UINT32)((fy >> 8) & 0xFF);
        UINT32 y1 = (y0 + 1 < ih) ? y0 + 1 : y0;

        const UINT8 *r0 = g_wallpaper_rgb + (UINTN)y0 * iw * 3u;
        const UINT8 *r1 = g_wallpaper_rgb + (UINTN)y1 * iw * 3u;

        for (UINT32 x = 0; x < w; x++) {

            UINT64 fx = ox + s * x;
            UINT32 x0 = (UINT32)(fx >> 16), tx = (UINT32)((fx >> 8) & 0xFF);
            UINT32 x1 = (x0 + 1 < iw) ? x0 + 1 : x0;

            UINT32 c[3];

            for (int k = 0; k < 3; k++) {
                UINT32 a = r0[x0 * 3 + k], b = r0[x1 * 3 + k];
                UINT32 cc = r1[x0 * 3 + k], d = r1[x1 * 3 + k];
                UINT32 top = a * (256 - tx) + b * tx;
                UINT32 bot = cc * (256 - tx) + d * tx;
                c[k] = (top * (256 - ty) + bot * ty) >> 16;
            }

            buf[(UINTN)y * w + x] = (c[0] << 16) | (c[1] << 8) | c[2];
        }
    }

    return TRUE;
}

