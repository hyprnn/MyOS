/*
 * icons16.h - значки MyOS 16x16 в пиксельном стиле Windows 95.
 * Общие для ядра (gui/icons.c, gui/gfx.c) и программ (user/lib/gfx.c) -
 * как шрифт font8x16.h: Блокнот и Сапёр стали программами (этап 10, Д4),
 * а значки у них должны быть те же, что у ярлыков рабочего стола.
 *
 * Каждый значок - 16 строк по 16 букв, буква - цвет из 16-цветной
 * палитры (k чёрный, w белый, g светло-серый, d тёмно-серый, b синий,
 * t бирюзовый, c голубой, y жёлтый, o оливковый, r красный, G зелёный,
 * n тёмно-зелёный, B ярко-синий, m бордовый, p/P фиолетовый;
 * '.' - прозрачно). Значок "logo" - собственный знак MyOS для кнопки
 * "Пуск". Здесь только данные и две маленькие функции без зависимостей
 * (static: у ядра и у каждой программы - своя копия).
 */
#ifndef MYOS_ICONS16_H
#define MYOS_ICONS16_H

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

/* глобус - браузер (этап 9) */
static const char *const ic_web[16] = {
    ".....kkkkkk.....",
    "...kkBBGGBBkk...",
    "..kBBGGGGBBBBk..",
    ".kBBBGGGBBBBBBk.",
    ".kBBGGGGGBBGGBk.",
    "kBBBBGGGBBBGGGBk",
    "kBBBBBGBBBGGGGBk",
    "kBBBBBBBBBBGGBBk",
    "kBBGGBBBBBBBBBBk",
    "kBGGGGBBBBBBBBBk",
    ".kGGGGGBBBBBBBk.",
    ".kBGGGBBBBBBBBk.",
    "..kBBBBBBBBBBk..",
    "...kkBBBBBBkk...",
    ".....kkkkkk.....",
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

static const struct {
    const char        *name;
    const char *const *rows;
} g_icons16[] = {
    { "computer", ic_computer },
    { "terminal", ic_terminal },
    { "notepad", ic_notepad },
    { "mine", ic_mine },
    { "mines", ic_mine },        /* программа /bin/mines */
    { "about", ic_logo },        /* программа /bin/about */
    { "life", ic_app },
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
    { "web", ic_web },
    { "help", ic_help },
    { "shutdown", ic_shutdown }
};

/* Значок по имени ("folder", "clock"...); нет такого - "app" */
static inline const char *const *icon16_find(const char *name)
{
    for (unsigned long i = 0; i < sizeof(g_icons16) / sizeof(g_icons16[0]); i++) {
        const char *a = g_icons16[i].name, *b = name;
        while (*a && *a == *b) {
            a++;
            b++;
        }
        if (*a == *b)
            return g_icons16[i].rows;
    }

    return ic_app;
}

/* Буква значка -> цвет 0x00RRGGBB; '.' (и неизвестная) - прозрачно */
static inline int icon16_color(char c, unsigned int *col)
{
    switch (c) {
    case 'k': *col = 0x000000; return 1;
    case 'w': *col = 0xFFFFFF; return 1;
    case 'g': *col = 0xC0C0C0; return 1;
    case 'd': *col = 0x808080; return 1;
    case 'b': *col = 0x000080; return 1;
    case 'B': *col = 0x0000FF; return 1;
    case 't': *col = 0x008080; return 1;
    case 'c': *col = 0x00FFFF; return 1;
    case 'y': *col = 0xFFFF00; return 1;
    case 'o': *col = 0x808000; return 1;
    case 'r': *col = 0xFF0000; return 1;
    case 'm': *col = 0x800000; return 1;
    case 'G': *col = 0x00FF00; return 1;
    case 'n': *col = 0x008000; return 1;
    case 'p': *col = 0x800080; return 1;
    case 'P': *col = 0xFF00FF; return 1;
    default:  return 0;
    }
}

#endif
