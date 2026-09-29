/*
 * gui/icons.c - значки 16x16 в пиксельном стиле Windows 95 (этапы 6-7).
 * Часть MyOS; общие объявления - в myos.h.
 *
 * Каждый значок - 16 строк по 16 букв, буква - цвет из 16-цветной
 * палитры (k чёрный, w белый, g светло-серый, d тёмно-серый, b синий,
 * t бирюзовый, c голубой, y жёлтый, o оливковый, r красный, G зелёный,
 * n тёмно-зелёный, B ярко-синий, m бордовый, p/P фиолетовый;
 * '.' - прозрачно). Рисует gfx_icon (gui/gfx.c), на рабочем столе -
 * вдвое крупнее. Значок "logo" - собственный знак MyOS для кнопки
 * "Пуск".
 */
#include "myos.h"

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

static const char *const ic_folder[16] = {
    "................",
    "................",
    "..kkkkk.........",
    ".kyyyyyk........",
    "kyyyyyyykkkkkkk.",
    "kywwwwwwwwwwwwyk",
    "kywyyyyyyyyyyyok",
    "kywyyyyyyyyyyyok",
    "kywyyyyyyyyyyyok",
    "kywyyyyyyyyyyyok",
    "kywyyyyyyyyyyyok",
    "kywyyyyyyyyyyyok",
    "kyoooooooooooook",
    "kkkkkkkkkkkkkkkk",
    "................",
    "................"
};

static const char *const ic_file[16] = {
    "..kkkkkkkk......",
    "..kwwwwwwkwk....",
    "..kwwwwwwkwwk...",
    "..kwwwwwwkkkkk..",
    "..kwwwwwwwwwwk..",
    "..kwddddddwwwk..",
    "..kwwwwwwwwwwk..",
    "..kwdddddddwwk..",
    "..kwwwwwwwwwwk..",
    "..kwddddddddwk..",
    "..kwwwwwwwwwwk..",
    "..kwdddddwwwwk..",
    "..kwwwwwwwwwwk..",
    "..kwwwwwwwwwwk..",
    "..kkkkkkkkkkkk..",
    "................"
};

static const char *const ic_drive[16] = {
    "................",
    "................",
    "................",
    "................",
    "..kkkkkkkkkkkk..",
    ".kggggggggggggk.",
    "kgwwwwwwwwwwwwdk",
    "kgggggggggggggdk",
    "kggggggggggGgddk",
    "kgggggggggggggdk",
    "kdddddddddddddkk",
    ".kkkkkkkkkkkkkk.",
    "................",
    "................",
    "................",
    "................"
};

static const char *const ic_clock[16] = {
    ".....kkkkkk.....",
    "...kkwwwwwwkk...",
    "..kwwwwkwwwwwk..",
    ".kwwwwwwwwwwwwk.",
    ".kwkwwwwkwwwwkk.",
    "kwwwwwwwkwwwwwwk",
    "kwwwwwwwkwwwwwwk",
    "kkwwwwwwkkkkwwkk",
    "kwwwwwwwwwwwwwwk",
    "kwwwwwwwwwwwwwwk",
    ".kwkwwwwwwwwwkk.",
    ".kwwwwwwwwwwwwk.",
    "..kwwwwwkwwwwk..",
    "...kkwwwwwwkk...",
    ".....kkkkkk.....",
    "................"
};

static const char *const ic_paint[16] = {
    "................",
    "....kkkkkkk.....",
    "..kkgggggggkk...",
    ".kggrrggggBBgk..",
    ".kggrrggggBBgk..",
    "kgggggggggggggk.",
    "kgGGgggggggkkk..",
    "kgGGggggggk.....",
    "kggggggggk....kk",
    "kgyyggggggk..kok",
    ".kyyggggggk.kok.",
    "..kkgggggk.kok..",
    "....kkkkk.kok...",
    "..........kk....",
    "................",
    "................"
};

static const char *const ic_logo[16] = {
    "................",
    ".kkkkkkkkkkkkkk.",
    ".kbbbbbbbbbbbbk.",
    ".kbccbbbbbbccbk.",
    ".kbcccbbbbcccbk.",
    ".kbccccbbccccbk.",
    ".kbccbccccbccbk.",
    ".kbccbbccbbccbk.",
    ".kbccbbbbbbccbk.",
    ".kbccbbbbbbccbk.",
    ".kbccbbbbbbccbk.",
    ".kbbbbbbbbbbbbk.",
    ".kbyyyyrrrGGGbk.",
    ".kbbbbbbbbbbbbk.",
    ".kkkkkkkkkkkkkk.",
    "................"
};

static const char *const ic_help[16] = {
    "................",
    "..kkkkkkkkkkkk..",
    "..kbbbbbbbbbbkk.",
    "..kbbwwwwwwbbkwk",
    "..kbbwbbbbwbbkwk",
    "..kbbbbbbbwbbkwk",
    "..kbbbbbwwbbbkwk",
    "..kbbbbbwbbbbkwk",
    "..kbbbbbwbbbbkwk",
    "..kbbbbbbbbbbkwk",
    "..kbbbbbwbbbbkwk",
    "..kbbbbbbbbbbkwk",
    "..kkkkkkkkkkkkwk",
    "...kwwwwwwwwwwwk",
    "....kkkkkkkkkkkk",
    "................"
};

static const char *const ic_shutdown[16] = {
    "................",
    "......kkkk......",
    "....kkrrrrkk....",
    "...krrkkkkrrk...",
    "..krrk.kk.krrk..",
    "..krk..kk..krk..",
    ".krk...kk...krk.",
    ".krk...kk...krk.",
    ".krk........krk.",
    ".krk........krk.",
    "..krk......krk..",
    "..krrk....krrk..",
    "...krrkkkkrrk...",
    "....kkrrrrkk....",
    "......kkkk......",
    "................"
};

typedef struct {
    const char        *name;
    const char *const *rows;
} ICON_ENTRY;

static const ICON_ENTRY g_icons[] = {
    { "computer", ic_computer },
    { "terminal", ic_terminal },
    { "notepad", ic_notepad },
    { "mine", ic_mine },
    { "app", ic_app },
    { "calc", ic_calc },
    { "guess", ic_guess },
    { "primes", ic_primes },
    { "folder", ic_folder },
    { "file", ic_file },
    { "drive", ic_drive },
    { "clock", ic_clock },
    { "paint", ic_paint },
    { "logo", ic_logo },
    { "help", ic_help },
    { "shutdown", ic_shutdown }
};

/* Значок по имени ("folder", "clock"...); нет такого - "app" */
const char *const *gui_icon(const char *name)
{
    for (UINTN i = 0; i < sizeof(g_icons) / sizeof(g_icons[0]); i++)
        if (kstreq(g_icons[i].name, name))
            return g_icons[i].rows;

    return ic_app;
}
