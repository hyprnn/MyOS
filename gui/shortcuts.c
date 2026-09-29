/*
 * gui/shortcuts.c - ярлыки на рабочем столе в духе Windows 95 (этап 6).
 * Часть MyOS; общие объявления - в myos.h.
 *
 * Каждый ярлык - значок 16x16 "пикселей", нарисованный буквами
 * (одна буква - один цвет из 16-цветной палитры Windows 95), и
 * подпись под ним. На экране значок увеличен вдвое (32x32) - так
 * пиксели видны, как на старых мониторах.
 *
 * Как в Windows 95: первый щелчок выделяет ярлык (значок темнеет
 * "сеточкой", подпись - на синем), второй щелчок по выделенному -
 * открывает. Ярлыки программ (HELLO, CALC...) открывают терминал и
 * запускают там программу из /bin в ring 3.
 */
#include "myos.h"

/* Палитра: буква -> цвет (как в стандартных значках Win95) */
static UINT32 sc_color(EFI_GRAPHICS_PIXEL_FORMAT fmt, char c, BOOLEAN *transparent)
{
    *transparent = FALSE;

    switch (c) {
    case 'k': return gui_pack(fmt, 0, 0, 0);
    case 'w': return gui_pack(fmt, 255, 255, 255);
    case 'g': return gui_pack(fmt, 192, 192, 192);
    case 'd': return gui_pack(fmt, 128, 128, 128);
    case 'b': return gui_pack(fmt, 0, 0, 128);
    case 'B': return gui_pack(fmt, 0, 0, 255);
    case 't': return gui_pack(fmt, 0, 128, 128);
    case 'c': return gui_pack(fmt, 0, 255, 255);
    case 'y': return gui_pack(fmt, 255, 255, 0);
    case 'o': return gui_pack(fmt, 128, 128, 0);
    case 'r': return gui_pack(fmt, 255, 0, 0);
    case 'm': return gui_pack(fmt, 128, 0, 0);
    case 'G': return gui_pack(fmt, 0, 255, 0);
    case 'n': return gui_pack(fmt, 0, 128, 0);
    default:
        *transparent = TRUE;
        return 0;
    }
}

/* ---- значки (16x16) ---- */

static const char *const ic_computer[16] = {
    "................",
    "..kkkkkkkkkkkk..",
    "..kwwwwwwwwwwk..",
    "..kwddddddddgk..",
    "..kwdttttttwgk..",
    "..kwdtcttttwgk..",
    "..kwdtttttttwk..",
    "..kwdttttttwgk..",
    "..kwdwwwwwwwgk..",
    "..kwggggggnggk..",
    "..kkkkkkkkkkkk..",
    "......kddk......",
    "...kkkkkkkkkk...",
    "..kwwwwwwwwwwk..",
    "..kgddddddddgk..",
    "..kkkkkkkkkkkk.."
};
static const char *const ic_terminal[16] = {
    "kkkkkkkkkkkkkkkk",
    "kbbbbbbbbbbgkgkk",
    "kbbbbbbbbbbkkkkk",
    "kkkkkkkkkkkkkkkk",
    "kgkkkkkkkkkkkkgk",
    "kgkwwkkkkkkkkkgk",
    "kgwkkkwkwkkkkkgk",
    "kgwkkkkkkwkkkkgk",
    "kgwkkkwkkkwkkkgk",
    "kgkwwkkkkkkwkkgk",
    "kgkkkkkkkkkkkkgk",
    "kgkkkwwwwkkkkkgk",
    "kgkkkkkkkkkkkkgk",
    "kggggggggggggggk",
    "kkkkkkkkkkkkkkkk",
    "................"
};
static const char *const ic_notepad[16] = {
    "...d.d.d.d.d....",
    "..kdkdkdkdkdk...",
    "..kwwwwwwwwwwk..",
    "..kwbbbbbbbwwk..",
    "..kwwwwwwwwwwk..",
    "..kwbbbbbbwwwk..",
    "..kwwwwwwwwwwk..",
    "..kwbbbbbbbbwk..",
    "..kwwwwwwwwwwk..",
    "..kwbbbbbwwwwk..",
    "..kwwwwwwwwwwk..",
    "..kwbbbbbbbwwk..",
    "..kwwwwwwwwwwk..",
    "..kwwwwwwwwwdk..",
    "..kkkkkkkkkkkk..",
    "................"
};
static const char *const ic_mine[16] = {
    "................",
    ".......k........",
    "...k...k...k....",
    "....k.kkk.k.....",
    ".....kkkkk......",
    "....kwwkkkk.....",
    "..kkkwwkkkkkk...",
    "....kkkkkkk.....",
    ".....kkkkk......",
    "....k.kkk.k.....",
    "...k...k...k....",
    ".......k........",
    "................",
    "................",
    "................",
    "................"
};
static const char *const ic_app[16] = {
    "................",
    ".kkkkkkkkkkkkkk.",
    ".kbbbbbbbbbbbgk.",
    ".kbwwwwbbbbbbgk.",
    ".kkkkkkkkkkkkkk.",
    ".kwwwwwwwwwwwgk.",
    ".kwwyyyywwwwwgk.",
    ".kwyykkyywwwwgk.",
    ".kwyyyyyywwwwgk.",
    ".kwyykkyywwwwgk.",
    ".kwwyyyywwwwwgk.",
    ".kwwwwwwwwwwwgk.",
    ".kwwwwwwwwwwwgk.",
    ".kgggggggggggdk.",
    ".kkkkkkkkkkkkkk.",
    "................"
};
static const char *const ic_calc[16] = {
    "...kkkkkkkkkk...",
    "...kggggggggk...",
    "...kgkkkkkkgk...",
    "...kgkGGGGkgk...",
    "...kgkkkkkkgk...",
    "...kggggggggk...",
    "...kgwdwdwdgk...",
    "...kgddddddgk...",
    "...kgwdwdwdgk...",
    "...kgddddddgk...",
    "...kgwdwdrdgk...",
    "...kgddddddgk...",
    "...kgwdwdrdgk...",
    "...kgddddddgk...",
    "...kggggggggk...",
    "...kkkkkkkkkk..."
};
static const char *const ic_guess[16] = {
    "................",
    "....kkkkkkkk....",
    "...kyyyyyyyyk...",
    "..kyyykkkkyyyk..",
    "..kyykkyykkyyk..",
    "..kyyyyyykkyyk..",
    "..kyyyyykkyyyk..",
    "..kyyyykkyyyyk..",
    "..kyyyykkyyyyk..",
    "..kyyyyyyyyyyk..",
    "..kyyyykkyyyyk..",
    "...kyyykkyyyk...",
    "....kyyyyyyk....",
    ".....kkkkkk.....",
    "................",
    "................"
};
static const char *const ic_primes[16] = {
    "................",
    "...kkkkkkkkkk...",
    "..kgggggggggdk..",
    "..kgkkkkkkkkdk..",
    "..kgkttttttkdk..",
    "..kgktkkkktkdk..",
    "..kgktkttktkdk..",
    "..kgktkttktkdk..",
    "..kgktkkkktkdk..",
    "..kgkttttttkdk..",
    "..kgkkkkkkkkdk..",
    "..kdddddddddddk.",
    "...kkkkkkkkkkk..",
    ".k.k.k.k.k.k....",
    "................",
    "................"
};

static const GUI_SHORTCUT g_shortcuts[] = {
    { "MY COMPUTER", ic_computer, GUI_ACT_EXPLORER,    NULL     },
    { "TERMINAL",    ic_terminal, GUI_ACT_TERMINAL,    NULL     },
    { "NOTEPAD",     ic_notepad,  GUI_ACT_NOTEPAD,     NULL     },
    { "MINESWEEPER", ic_mine,     GUI_ACT_MINESWEEPER, NULL     },
    { "HELLO",       ic_app,      GUI_ACT_PROGRAM,     "hello"  },
    { "CALC",        ic_calc,     GUI_ACT_PROGRAM,     "calc"   },
    { "GUESS",       ic_guess,    GUI_ACT_PROGRAM,     "guess"  },
    { "PRIMES",      ic_primes,   GUI_ACT_PROGRAM,     "primes" },
};

#define SC_COUNT   (sizeof(g_shortcuts) / sizeof(g_shortcuts[0]))
#define SC_CELL_W  84          /* ширина места под ярлык */
#define SC_CELL_H  66          /* высота: значок 32 + подпись */
#define SC_ICON    32
#define SC_LEFT    10
#define SC_TOP     ((INTN)GUI_TASKBAR_H + 10)

UINTN gui_shortcut_count(void)
{
    return SC_COUNT;
}

const GUI_SHORTCUT *gui_shortcut(UINTN i)
{
    return (i < SC_COUNT) ? &g_shortcuts[i] : NULL;
}

/* Где ярлык i: колонкой сверху вниз, как на столе Win95 */
static void sc_cell(UINTN i, INTN *x, INTN *y)
{
    *x = SC_LEFT;
    *y = SC_TOP + (INTN)(i * SC_CELL_H);
}

/* Ярлык под точкой (x, y) или -1 */
INTN gui_shortcut_at(INTN x, INTN y)
{
    for (UINTN i = 0; i < SC_COUNT; i++) {

        INTN cx, cy;

        sc_cell(i, &cx, &cy);

        /* значок и подпись (не весь прямоугольник - как в Win95) */
        INTN ix = cx + (SC_CELL_W - SC_ICON) / 2;

        if (gui_point_in_rect(x, y, ix, cy, SC_ICON, SC_ICON))
            return (INTN)i;

        UINTN tw = gui_text_width(g_shortcuts[i].label, 1) + 4;
        INTN tx = cx + (SC_CELL_W - (INTN)tw) / 2;

        if (gui_point_in_rect(x, y, tx, cy + SC_ICON + 2, tw, 12))
            return (INTN)i;
    }

    return -1;
}

void gui_draw_shortcuts(
    volatile UINT32 *fb, UINT32 stride, UINT32 fb_w, UINT32 fb_h,
    EFI_GRAPHICS_PIXEL_FORMAT fmt, INTN selected
)
{
    UINT32 navy = gui_pack(fmt, 0, 0, 128);
    UINT32 white = gui_pack(fmt, 255, 255, 255);

    for (UINTN i = 0; i < SC_COUNT; i++) {

        const GUI_SHORTCUT *s = &g_shortcuts[i];
        BOOLEAN sel = ((INTN)i == selected);
        INTN cx, cy;

        sc_cell(i, &cx, &cy);

        /* значок не должен вылезти за низ экрана */
        if (cy + SC_CELL_H > (INTN)fb_h)
            break;

        INTN ix = cx + (SC_CELL_W - SC_ICON) / 2;

        for (UINTN r = 0; r < 16; r++) {
            for (UINTN c = 0; c < 16; c++) {

                BOOLEAN tr;
                UINT32 col = sc_color(fmt, s->icon[r][c], &tr);

                if (tr)
                    continue;

                for (UINTN dy = 0; dy < 2; dy++)
                    for (UINTN dx = 0; dx < 2; dx++) {

                        UINT32 px = col;

                        /* выделенный значок - "сеточкой" синего,
                           как в Windows 95 */
                        if (sel && (((c * 2 + dx) + (r * 2 + dy)) & 1u))
                            px = navy;

                        gui_fill_rect(fb, stride, fb_w, fb_h,
                                      ix + (INTN)(c * 2 + dx), cy + (INTN)(r * 2 + dy),
                                      1, 1, px);
                    }
            }
        }

        /* подпись: белая на бирюзе; выделенная - на синем */
        UINTN tw = gui_text_width(s->label, 1) + 4;
        INTN tx = cx + (SC_CELL_W - (INTN)tw) / 2;
        INTN ty = cy + SC_ICON + 2;

        if (sel)
            gui_fill_rect(fb, stride, fb_w, fb_h, tx, ty, tw, 11, navy);

        gui_draw_text(fb, stride, fb_w, fb_h, tx + 2, ty + 2, 1, white, s->label);
    }
}
