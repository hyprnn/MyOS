/*
 * gui/wallpaper/wallpaper.S - обои рабочего стола, вклеенные в ядро
 * (этап 10). wallpaper.rgb - 1366x768, по 3 байта R, G, B на точку;
 * делает его tools/mkwallpaper.py из любой картинки. Рабочий стол
 * (gui/wm.c) растягивает обои под свой экран.
 */
.section .rodata
.balign 16
.globl g_wallpaper_rgb
.globl g_wallpaper_rgb_end
.globl g_wallpaper_w
.globl g_wallpaper_h
g_wallpaper_w: .long 1366
g_wallpaper_h: .long 768
.balign 16
g_wallpaper_rgb:
.incbin "gui/wallpaper/wallpaper.rgb"
g_wallpaper_rgb_end:
.section .note.GNU-stack,"",@progbits
