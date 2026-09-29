/*
 * gui/draw.c - пиксельные примитивы GUI, шрифт GUI.
 * Часть MyOS; общие объявления - в myos.h.
 */
#include "myos.h"

/*
 * Упаковать R,G,B в 32-битный пиксель под
 * тот PixelFormat, который реально отдаёт GOP.
 * Неизвестные/BLT-only форматы просто трактуем
 * как BGR — это самый частый случай на практике
 * (QEMU/OVMF, VirtualBox).
 */
UINT32 gui_pack(
    EFI_GRAPHICS_PIXEL_FORMAT fmt,
    UINT8 r, UINT8 g, UINT8 b
)
{
    if (fmt == PixelRedGreenBlueReserved8BitPerColor) {

        return (UINT32)r |
               ((UINT32)g << 8) |
               ((UINT32)b << 16);
    }

    return (UINT32)b |
           ((UINT32)g << 8) |
           ((UINT32)r << 16);
}


void gui_fill_rect(
    volatile UINT32 *fb,
    UINT32 stride,
    UINT32 fb_w,
    UINT32 fb_h,
    INTN x, INTN y,
    UINTN w, UINTN h,
    UINT32 color
)
{
    if (x < 0) {

        if ((UINTN)(-x) >= w)
            return;

        w -= (UINTN)(-x);
        x = 0;
    }

    if (y < 0) {

        if ((UINTN)(-y) >= h)
            return;

        h -= (UINTN)(-y);
        y = 0;
    }

    if (x >= (INTN)fb_w || y >= (INTN)fb_h)
        return;

    if ((UINTN)x + w > fb_w)
        w = fb_w - (UINTN)x;

    if ((UINTN)y + h > fb_h)
        h = fb_h - (UINTN)y;

    for (UINTN row = 0; row < h; row++) {

        volatile UINT32 *dst =
            fb +
            (UINTN)(y + (INTN)row) * stride +
            (UINTN)x;

        for (UINTN col = 0; col < w; col++)
            dst[col] = color;
    }
}


/*
 * Нарисовать рамку прямоугольника (только контур,
 * толщиной 1 логический пиксель * scale).
 */
void gui_draw_border(
    volatile UINT32 *fb,
    UINT32 stride,
    UINT32 fb_w,
    UINT32 fb_h,
    INTN x, INTN y,
    UINTN w, UINTN h,
    UINT32 color
)
{
    gui_fill_rect(fb, stride, fb_w, fb_h, x, y, w, 1, color);
    gui_fill_rect(fb, stride, fb_w, fb_h, x, y + (INTN)h - 1, w, 1, color);
    gui_fill_rect(fb, stride, fb_w, fb_h, x, y, 1, h, color);
    gui_fill_rect(fb, stride, fb_w, fb_h, x + (INTN)w - 1, y, 1, h, color);
}


/*
 * Рамка в стиле "объёмных" 3D-кнопок/панелей ранних
 * графических интерфейсов (Win9x и подобных): двойная
 * обводка светлым/тёмным, создающая иллюзию выпуклости
 * (raised == TRUE) или вдавленности (raised == FALSE).
 * Это НЕ копия чужих ассетов, просто тот же общий приём
 * рисования "фаски" сплошными прямоугольниками.
 */
void gui_draw_bevel(
    volatile UINT32 *fb,
    UINT32 stride,
    UINT32 fb_w,
    UINT32 fb_h,
    INTN x, INTN y,
    UINTN w, UINTN h,
    UINT32 c_hi,     /* внешняя светлая грань   */
    UINT32 c_light,  /* внутренняя светлая грань */
    UINT32 c_shadow, /* внутренняя тёмная грань  */
    UINT32 c_dark,   /* внешняя тёмная грань     */
    BOOLEAN raised
)
{
    UINT32 out_tl = raised ? c_hi     : c_dark;
    UINT32 out_br = raised ? c_dark   : c_hi;
    UINT32 in_tl  = raised ? c_light  : c_shadow;
    UINT32 in_br  = raised ? c_shadow : c_light;

    /* внешняя грань */
    gui_fill_rect(fb, stride, fb_w, fb_h, x, y, w, 1, out_tl);
    gui_fill_rect(fb, stride, fb_w, fb_h, x, y, 1, h, out_tl);
    gui_fill_rect(fb, stride, fb_w, fb_h, x, y + (INTN)h - 1, w, 1, out_br);
    gui_fill_rect(fb, stride, fb_w, fb_h, x + (INTN)w - 1, y, 1, h, out_br);

    /* внутренняя грань (только если есть место) */
    if (w > 2 && h > 2) {

        gui_fill_rect(fb, stride, fb_w, fb_h, x + 1, y + 1, w - 2, 1, in_tl);
        gui_fill_rect(fb, stride, fb_w, fb_h, x + 1, y + 1, 1, h - 2, in_tl);
        gui_fill_rect(fb, stride, fb_w, fb_h, x + 1, y + (INTN)h - 2, w - 2, 1, in_br);
        gui_fill_rect(fb, stride, fb_w, fb_h, x + (INTN)w - 2, y + 1, 1, h - 2, in_br);
    }
}

const GUI_GLYPH gui_font[] = {
    { ' ', { 0, 0, 0, 0, 0, 0, 0 } },
    { '!', { 4, 4, 4, 4, 0, 4, 0 } },
    { '\'', { 4, 4, 0, 0, 0, 0, 0 } },
    { '-', { 0, 0, 0, 14, 0, 0, 0 } },
    { '.', { 0, 0, 0, 0, 0, 4, 0 } },
    { '0', { 14, 17, 19, 21, 25, 17, 14 } },
    { '1', { 4, 12, 4, 4, 4, 4, 14 } },
    { '2', { 14, 17, 1, 2, 4, 8, 31 } },
    { '3', { 14, 17, 1, 6, 1, 17, 14 } },
    { '4', { 2, 6, 10, 18, 31, 2, 2 } },
    { '5', { 31, 16, 30, 1, 1, 17, 14 } },
    { '6', { 14, 16, 30, 17, 17, 17, 14 } },
    { '7', { 31, 1, 2, 4, 8, 8, 8 } },
    { '8', { 14, 17, 17, 14, 17, 17, 14 } },
    { '9', { 14, 17, 17, 15, 1, 17, 14 } },
    { ':', { 0, 4, 0, 0, 4, 0, 0 } },
    { ',', { 0, 0, 0, 0, 0, 4, 8 } },
    { '(', { 2, 4, 8, 8, 8, 4, 2 } },
    { ')', { 8, 4, 2, 2, 2, 4, 8 } },
    { '=', { 0, 0, 31, 0, 31, 0, 0 } },
    { '/', { 1, 1, 2, 4, 8, 16, 16 } },
    { '_', { 0, 0, 0, 0, 0, 0, 31 } },
    /* этап 6: знаки для программ в терминале GUI (calc 2*(3+4) ...) */
    { '+', { 0, 4, 4, 31, 4, 4, 0 } },
    { '*', { 0, 21, 14, 31, 14, 21, 0 } },
    { '>', { 16, 8, 4, 2, 4, 8, 16 } },
    { '<', { 1, 2, 4, 8, 4, 2, 1 } },
    { '%', { 25, 26, 2, 4, 8, 11, 19 } },
    { '?', { 14, 17, 1, 2, 4, 0, 4 } },
    { '[', { 14, 8, 8, 8, 8, 8, 14 } },
    { ']', { 14, 2, 2, 2, 2, 2, 14 } },
    { '"', { 10, 10, 20, 0, 0, 0, 0 } },
    { '#', { 10, 10, 31, 10, 31, 10, 10 } },
    { '&', { 12, 18, 20, 8, 21, 18, 13 } },
    { ';', { 0, 12, 12, 0, 12, 4, 8 } },
    { '@', { 14, 17, 23, 21, 23, 16, 14 } },
    { '|', { 4, 4, 4, 4, 4, 4, 4 } },
    { '^', { 4, 10, 17, 0, 0, 0, 0 } },
    { '~', { 0, 0, 8, 21, 2, 0, 0 } },
    { '$', { 4, 15, 20, 14, 5, 30, 4 } },
    { '\\', { 16, 16, 8, 4, 2, 1, 1 } },
    { 'A', { 14, 17, 17, 31, 17, 17, 17 } },
    { 'B', { 30, 17, 17, 30, 17, 17, 30 } },
    { 'C', { 15, 16, 16, 16, 16, 16, 15 } },
    { 'D', { 30, 17, 17, 17, 17, 17, 30 } },
    { 'E', { 31, 16, 16, 30, 16, 16, 31 } },
    { 'F', { 31, 16, 16, 30, 16, 16, 16 } },
    { 'G', { 15, 16, 16, 19, 17, 17, 15 } },
    { 'H', { 17, 17, 17, 31, 17, 17, 17 } },
    { 'I', { 14, 4, 4, 4, 4, 4, 14 } },
    { 'J', { 1, 1, 1, 1, 17, 17, 14 } },
    { 'K', { 17, 18, 20, 24, 20, 18, 17 } },
    { 'L', { 16, 16, 16, 16, 16, 16, 31 } },
    { 'M', { 17, 27, 21, 17, 17, 17, 17 } },
    { 'N', { 17, 25, 21, 19, 17, 17, 17 } },
    { 'O', { 14, 17, 17, 17, 17, 17, 14 } },
    { 'P', { 30, 17, 17, 30, 16, 16, 16 } },
    { 'Q', { 14, 17, 17, 17, 21, 18, 13 } },
    { 'R', { 30, 17, 17, 30, 20, 18, 17 } },
    { 'S', { 15, 16, 16, 14, 1, 1, 30 } },
    { 'T', { 31, 4, 4, 4, 4, 4, 4 } },
    { 'U', { 17, 17, 17, 17, 17, 17, 14 } },
    { 'V', { 17, 17, 17, 17, 17, 10, 4 } },
    { 'W', { 17, 17, 17, 17, 21, 27, 17 } },
    { 'X', { 17, 17, 10, 4, 10, 17, 17 } },
    { 'Y', { 17, 17, 10, 4, 4, 4, 4 } },
    { 'Z', { 31, 1, 2, 4, 8, 16, 31 } },
};


const GUI_GLYPH *gui_find_glyph(char c)
{
    for (UINTN i = 0; i < GUI_FONT_COUNT; i++) {

        if (gui_font[i].ch == c)
            return &gui_font[i];
    }

    return NULL;
}


/*
 * Нарисовать один символ шрифтом 5x7, увеличенным
 * в scale раз. Возвращает ширину символа в пикселях
 * (вместе с межсимвольным интервалом), чтобы вызывающий
 * код мог сдвинуть x для следующего символа.
 */
UINTN gui_draw_char(
    volatile UINT32 *fb,
    UINT32 stride,
    UINT32 fb_w,
    UINT32 fb_h,
    INTN x, INTN y,
    UINTN scale,
    UINT32 color,
    char c
)
{
    const GUI_GLYPH *glyph = gui_find_glyph(c);

    if (glyph != NULL) {

        for (UINTN row = 0; row < 7; row++) {

            UINT8 bits = glyph->rows[row];

            for (UINTN col = 0; col < 5; col++) {

                if (bits & (1 << (4 - col))) {

                    gui_fill_rect(
                        fb, stride, fb_w, fb_h,
                        x + (INTN)(col * scale),
                        y + (INTN)(row * scale),
                        scale, scale,
                        color
                    );
                }
            }
        }
    }

    return (5 * scale) + scale;
}


UINTN gui_draw_text(
    volatile UINT32 *fb,
    UINT32 stride,
    UINT32 fb_w,
    UINT32 fb_h,
    INTN x, INTN y,
    UINTN scale,
    UINT32 color,
    const char *s
)
{
    INTN start_x = x;

    while (*s != '\0') {

        x += (INTN)gui_draw_char(
            fb, stride, fb_w, fb_h,
            x, y, scale, color, *s
        );

        s++;
    }

    return (UINTN)(x - start_x);
}


UINTN gui_text_width(const char *s, UINTN scale)
{
    UINTN w = 0;

    while (*s != '\0') {
        w += (5 * scale) + scale;
        s++;
    }

    return w;
}


/*
 * Перевести число в десятичную строку (без libc).
 * Возвращает длину строки, buf должен вмещать
 * минимум 21 байт (64-битное число + '\0').
 */
UINTN gui_uint_to_str(UINT64 v, char *buf)
{
    char tmp[21];
    UINTN n = 0;

    if (v == 0) {
        buf[0] = '0';
        buf[1] = '\0';
        return 1;
    }

    while (v > 0) {
        tmp[n++] = (char)('0' + (v % 10));
        v /= 10;
    }

    for (UINTN i = 0; i < n; i++)
        buf[i] = tmp[n - 1 - i];

    buf[n] = '\0';

    return n;
}


/*
 * Шестнадцатеричная печать с ведущими нулями до нужной
 * ширины - аналог print_hex(), но пишет в char*-буфер для
 * gui_draw_text() (пиксельный шрифт), а не в ConOut. Нужна
 * именно эта отдельная версия, потому что после
 * ExitBootServices печатать через ConOut уже нельзя, а
 * показать MMIO-адрес найденного xHCI как-то надо.
 */
UINTN gui_hex_to_str(UINT64 v, UINTN digits, char *buf)
{
    static const char hex_chars[] = "0123456789ABCDEF";

    if (digits > 16)
        digits = 16;

    for (UINTN i = 0; i < digits; i++) {

        UINTN shift = (digits - 1 - i) * 4;

        buf[i] = hex_chars[(v >> shift) & 0xF];
    }

    buf[digits] = '\0';

    return digits;
}


/*
 * Дополнить число слева нулём до 2 цифр
 * (для часов/минут/секунд в панели задач).
 */
UINTN gui_uint2_to_str(UINT32 v, char *buf)
{
    if (v > 99)
        v = 99;

    buf[0] = (char)('0' + (v / 10));
    buf[1] = (char)('0' + (v % 10));
    buf[2] = '\0';

    return 2;
}


BOOLEAN gui_point_in_rect(
    INTN px, INTN py,
    INTN x, INTN y, UINTN w, UINTN h
)
{
    return px >= x && px < x + (INTN)w &&
           py >= y && py < y + (INTN)h;
}


/* ============================================================
 * Сапёр: генератор случайных чисел, логика поля, геометрия
 * ============================================================ */

/* Простой LCG. Точная криптостойкость тут не нужна - только
   чтобы мины не лежали каждый раз одинаково. */
UINT32 g_ms_rng = 12345;

UINT32 gui_ms_rand(void)
{
    g_ms_rng = g_ms_rng * 1103515245u + 12345u;
    return (g_ms_rng >> 16) & 0x7fffu;
}
