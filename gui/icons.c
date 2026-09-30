/*
 * gui/icons.c - значки 16x16 в пиксельном стиле Windows 95 (этапы 6-7).
 * Часть MyOS; общие объявления - в myos.h.
 *
 * Сами значки (16 строк по 16 букв-цветов) - в icons16.h: он общий с
 * программами (user/lib/gfx.c), как шрифт font8x16.h. Рисует gfx_icon
 * (gui/gfx.c), на рабочем столе - вдвое крупнее.
 */
#include "myos.h"
#include "icons16.h"

/* Значок по имени ("folder", "clock"...); нет такого - "app" */
const char *const *gui_icon(const char *name)
{
    return icon16_find(name);
}
