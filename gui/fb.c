/*
 * gui/fb.c - рисование прямо в видеопамять, без оконной системы.
 * Часть MyOS; общие объявления - в myos.h.
 *
 * Нужно тем, кто рисует до или вместо рабочего стола: текстовой
 * консоли ядра (kernel/kcon.c), экрану паники (kernel/cpu.c), заставке
 * загрузки (kernel/kmain.c) и `mousetest` (kernel/kcmds.c). Всё прочее
 * рисование - через поверхности GFX (gui/gfx.c) оконной системы.
 *
 * (Этап 10, Д5: здесь осталось то немногое, что ещё было нужно от
 * первого GUI - gui/gui.c, draw.c, desktop.c, terminal.c и другие его
 * файлы удалены: с этапа 7 рабочий стол - это gui/wm.c.)
 */
#include "myos.h"

/*
 * Упаковать R,G,B в 32-битный пиксель под тот PixelFormat, который
 * реально отдаёт GOP. Неизвестные/BLT-only форматы трактуем как BGR -
 * самый частый случай на практике (QEMU/OVMF, VirtualBox, ноутбуки).
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

/* Залить прямоугольник (обрезается по краям экрана) */
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

/* Курсор-квадрат для `mousetest`: чёрная рамка 2 пикселя, белая середина */
void gui_draw_cursor_at(
    volatile UINT32 *fb,
    UINT32 stride, UINT32 fb_w, UINT32 fb_h,
    EFI_GRAPHICS_PIXEL_FORMAT fmt,
    INTN x, INTN y
)
{
    gui_fill_rect(
        fb, stride, fb_w, fb_h,
        x, y, GUI_CURSOR_SIZE, GUI_CURSOR_SIZE,
        gui_pack(fmt, 0, 0, 0)
    );

    gui_fill_rect(
        fb, stride, fb_w, fb_h,
        x + 2, y + 2, GUI_CURSOR_SIZE - 4, GUI_CURSOR_SIZE - 4,
        gui_pack(fmt, 255, 255, 255)
    );
}
