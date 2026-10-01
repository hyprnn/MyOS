/*
 * gui/tty.c - терминал рабочего стола: окно, в котором работает
 * шелл-программа /bin/sh (этап 10, Д2). Часть MyOS; общие объявления -
 * в myos.h.
 *
 * Раньше у окна "Терминал" был свой маленький набор команд внутри
 * ядра (help, ls, cat... - восемь штук). Теперь окно - только
 * "экран и клавиатура" (как xterm или консоль Windows), а команды
 * понимает /bin/sh - тот же, что в текстовой консоли. Значит, в окне
 * работает всё: команды ядра (battery, wifi, cpu...), программы,
 * фон "&", вывод в файл.
 *
 * Что здесь есть:
 *   * экран - буфер символов TTY_COLS x TTY_LINES (с историей для
 *     прокрутки колесом и PageUp/PageDown), курсор; вывод понимает
 *     '\r' (в начало строки), '\n', '\b' (на символ влево) и UTF-8 -
 *     кириллицу шрифт рабочего стола рисует;
 *   * клавиатура - очередь клавиш окна. Шелл читает её по одной
 *     (SYS_READKEY); обычная программа (calc, guess) читает строку -
 *     тогда эхо и Backspace делает сам терминал (tty_read_line);
 *   * вывод встроенных команд ядра (SYS_KCMD) - через свой
 *     SIMPLE_TEXT_OUTPUT_INTERFACE прямо в этот буфер;
 *   * "передний план": программа, которую шелл ждёт. Ctrl+C в окне
 *     останавливает её; если на переднем плане сам шелл - ему клавиша 3.
 *
 * Жизнь терминала: его держат окно и все программы, которые в нём
 * запущены (refs). Закрыли окно - программам "стоп"; память - когда
 * уйдёт последняя.
 */
#include "myos.h"

#define TTY_BG      0x101418u
#define TTY_FG      0xDCDCDCu
#define TTY_CURSOR  0x00E078u

struct KTTY {
    SIMPLE_TEXT_OUTPUT_INTERFACE out;   /* первым: указатель на out = на KTTY */
    UINT32  cells[TTY_LINES][TTY_COLS]; /* кольцо строк: символы Юникода */
    UINT32  base;                       /* строка 0 - cells[base] */
    UINT32  nlines;                     /* сколько строк занято (>= 1) */
    UINT32  row, col;                   /* курсор: строка (от 0) и колонка */
    UINT32  scroll;                     /* прокрутка: строк от низа */
    UINT32  u8_need, u8_cp;             /* разбор UTF-8 */

    INT32   keys[TTY_KEYS];             /* очередь клавиш окна */
    volatile UINT32 khead, ktail;

    struct KPROC *fg;                   /* на переднем плане */
    WIN    *win;                        /* NULL - окно закрыто */
    UINT32  refs;                       /* программ, которые его держат */

    char    logline[160];               /* строка для журнала (klog) */
    UINT32  loglen;
    char    cmd[128];                   /* первая команда шелла (ярлык) */

    /* управляющие последовательности ANSI/VT100 (этап 11: программы
       Linux - шелл с редактором строки, vi, цветной ls) */
    UINT8   esc;                        /* 0 - текст, 1 - после ESC, 2 - CSI
                                           "ESC [", 3 - OSC "ESC ]", 4 - "ESC (" */
    UINT32  par[8];                     /* числа CSI */
    UINT32  npar;
    BOOLEAN par_q;                      /* "ESC [ ?" */
    UINT32  fgc, bgc;                   /* цвета: 0 - обычный, 1..16 - палитра */
    BOOLEAN bold, rev;
    UINT32  srow, scol;                 /* сохранённый курсор (ESC 7) */
    UINT32  rtop, rbot;                 /* область прокрутки (строки экрана);
                                           rbot == 0 - весь экран */
    BOOLEAN hide_cursor;
};

/* Ячейка экрана: символ Юникода (21 бит) + цвета (по 5 бит):
   0 - цвет терминала, 1..16 - палитра VT100, 17/18 - "цвет текста"/
   "цвет фона" терминала (нужны для инверсии) */
#define CELL_CH(c)    ((c) & 0x1FFFFFu)
#define CELL_FG(c)    (((c) >> 21) & 31u)
#define CELL_BG(c)    (((c) >> 26) & 31u)
#define CELL(ch, fg, bg)  ((ch) | ((UINT32)(fg) << 21) | ((UINT32)(bg) << 26))

/* окно хранит не сам терминал (его освобождает wm_close), а ссылку */
typedef struct {
    KTTY *t;
} TTY_WIN;

static void tty_say(KTTY *t, const char *msg)
{
    UINTN n = 0;

    while (msg[n])
        n++;

    tty_write(t, msg, n);
}

/* ---------------- память и ссылки ---------------- */

static void tty_free_if_unused(KTTY *t)
{
    if (t->win == NULL && t->refs == 0)
        kfree(t);
}

void tty_ref(KTTY *t)
{
    if (t != NULL)
        t->refs++;
}

void tty_unref(KTTY *t)
{
    if (t == NULL)
        return;

    if (t->refs > 0)
        t->refs--;

    if (t->fg != NULL && t->refs == 0)
        t->fg = NULL;

    tty_free_if_unused(t);
}

/* ---------------- экран ---------------- */

static UINT32 *tty_line(KTTY *t, UINT32 row)
{
    return t->cells[(t->base + row) % TTY_LINES];
}

static void tty_clear_line(UINT32 *l)
{
    for (UINTN i = 0; i < TTY_COLS; i++)
        l[i] = ' ';
}

/* Цвета ячейки по текущему режиму (жирный - яркий цвет, инверсия) */
static UINT32 tty_attr(KTTY *t)
{
    UINT32 fg = t->fgc, bg = t->bgc;

    if (t->bold && fg >= 1 && fg <= 8)
        fg += 8;

    if (t->rev) {
        UINT32 f2 = bg ? bg : 18u, b2 = fg ? fg : 17u;
        fg = f2;
        bg = b2;
    }

    return ((fg & 31u) << 21) | ((bg & 31u) << 26);
}

/* Пустая ячейка с текущим фоном (так стирает VT100) */
static UINT32 tty_blank(KTTY *t)
{
    return ' ' | (tty_attr(t) & (31u << 26));
}

static UINT32 tty_screen_rows(KTTY *t)
{
    UINT32 r = tty_rows(t);

    if (r > TTY_LINES / 2)
        r = TTY_LINES / 2;

    return r ? r : 24;
}

/*
 * Экран - последние rows строк буфера. Для перемещения курсора нужен
 * "полный" экран: строк в буфере не меньше rows (добавляем пустые
 * снизу - то, что уже видно, не сдвигается). Возвращает номер строки
 * буфера, где верх экрана.
 */
static UINT32 tty_screen_top(KTTY *t)
{
    UINT32 rows = tty_screen_rows(t);

    while (t->nlines < rows) {
        t->nlines++;
        tty_clear_line(tty_line(t, t->nlines - 1));
    }

    return t->nlines - rows;
}

/* Сдвинуть строки экрана [a, b] вверх на одну (b - пустая) или вниз */
static void tty_scroll_region(KTTY *t, UINT32 a, UINT32 b, BOOLEAN up)
{
    UINT32 top = tty_screen_top(t);
    UINT32 blank = tty_blank(t);

    if (up) {
        for (UINT32 r = a; r < b; r++)
            memcpy(tty_line(t, top + r), tty_line(t, top + r + 1), sizeof(UINT32) * TTY_COLS);
        for (UINTN i = 0; i < TTY_COLS; i++)
            tty_line(t, top + b)[i] = blank;
    } else {
        for (UINT32 r = b; r > a; r--)
            memcpy(tty_line(t, top + r), tty_line(t, top + r - 1), sizeof(UINT32) * TTY_COLS);
        for (UINTN i = 0; i < TTY_COLS; i++)
            tty_line(t, top + a)[i] = blank;
    }
}

/* Область прокрутки задана и курсор на её нижней строке? */
static BOOLEAN tty_in_region_bottom(KTTY *t)
{
    if (t->rbot == 0)
        return FALSE;

    UINT32 top = tty_screen_top(t);
    return t->row >= top && t->row - top == t->rbot;
}

static void tty_newline(KTTY *t)
{
    /* строка готова - в журнал (автотест смотрит вывод программ там) */
    t->logline[t->loglen] = '\0';
    klog("term: %s\n", t->logline);
    t->loglen = 0;

    t->col = 0;

    /* внизу области прокрутки (vi, less) - сдвинуть только её */
    if (tty_in_region_bottom(t)) {
        tty_scroll_region(t, t->rtop, t->rbot, TRUE);
        return;
    }

    if (t->row + 1 < t->nlines) {
        t->row++;
        return;
    }

    if (t->nlines < TTY_LINES) {
        t->nlines++;
    } else {
        /* кольцо полно - самая старая строка уходит */
        t->base = (t->base + 1) % TTY_LINES;
    }

    t->row = t->nlines - 1;
    tty_clear_line(tty_line(t, t->row));
}

static void tty_push_key(KTTY *t, INT32 k);

/* 256 цветов xterm (и 24-битные) -> ближайший из 16 */
static UINT32 tty_rgb16(UINT32 r, UINT32 g, UINT32 b)
{
    UINT32 mx = r > g ? (r > b ? r : b) : (g > b ? g : b);
    UINT32 c = (r > mx / 2 && r > 60 ? 1u : 0u) | (g > mx / 2 && g > 60 ? 2u : 0u) |
               (b > mx / 2 && b > 60 ? 4u : 0u);

    if (mx < 60)
        return 1;                       /* чёрный */

    return c + (mx > 200 ? 9u : 1u);
}

static UINT32 tty_xterm256(UINT32 n)
{
    if (n < 16)
        return n + 1;

    if (n >= 232) {
        UINT32 v = (n - 232) * 10 + 8;
        return (v < 64) ? 1 : (v < 128) ? 9 : (v < 200) ? 8 : 16;
    }

    n -= 16;
    return tty_rgb16((n / 36) * 51, ((n / 6) % 6) * 51, (n % 6) * 51);
}

static void tty_sgr(KTTY *t)
{
    if (t->npar == 0) {
        t->fgc = t->bgc = 0;
        t->bold = t->rev = FALSE;
        return;
    }

    for (UINT32 i = 0; i < t->npar; i++) {

        UINT32 v = t->par[i];

        if (v == 0) {
            t->fgc = t->bgc = 0;
            t->bold = t->rev = FALSE;
        } else if (v == 1) {
            t->bold = TRUE;
        } else if (v == 22) {
            t->bold = FALSE;
        } else if (v == 7) {
            t->rev = TRUE;
        } else if (v == 27) {
            t->rev = FALSE;
        } else if (v >= 30 && v <= 37) {
            t->fgc = v - 30 + 1;
        } else if (v == 39) {
            t->fgc = 0;
        } else if (v >= 40 && v <= 47) {
            t->bgc = v - 40 + 1;
        } else if (v == 49) {
            t->bgc = 0;
        } else if (v >= 90 && v <= 97) {
            t->fgc = v - 90 + 9;
        } else if (v >= 100 && v <= 107) {
            t->bgc = v - 100 + 9;
        } else if ((v == 38 || v == 48) && i + 2 < t->npar && t->par[i + 1] == 5) {
            UINT32 c = tty_xterm256(t->par[i + 2]);
            if (v == 38) t->fgc = c; else t->bgc = c;
            i += 2;
        } else if ((v == 38 || v == 48) && i + 4 < t->npar && t->par[i + 1] == 2) {
            UINT32 c = tty_rgb16(t->par[i + 2], t->par[i + 3], t->par[i + 4]);
            if (v == 38) t->fgc = c; else t->bgc = c;
            i += 4;
        }
    }
}

/* Конец последовательности CSI: выполнить команду fin */
static void tty_csi(KTTY *t, UINT32 fin)
{
    UINT32 rows = tty_screen_rows(t);
    UINT32 top = tty_screen_top(t);
    UINT32 n = (t->npar > 0 && t->par[0] > 0) ? t->par[0] : 1;
    UINT32 srow = (t->row >= top) ? t->row - top : 0;
    UINT32 blank = tty_blank(t);

    if (t->col >= TTY_COLS)
        t->col = TTY_COLS - 1;

    switch (fin) {

    case 'A':                                   /* курсор вверх */
        srow = (srow > n) ? srow - n : 0;
        t->row = top + srow;
        break;

    case 'B':                                   /* вниз */
        srow = (srow + n < rows) ? srow + n : rows - 1;
        t->row = top + srow;
        break;

    case 'C':                                   /* вправо */
        t->col = (t->col + n < TTY_COLS) ? t->col + n : TTY_COLS - 1;
        break;

    case 'D':                                   /* влево */
        t->col = (t->col > n) ? t->col - n : 0;
        break;

    case 'E': case 'F':                         /* на n строк вниз/вверх, в начало */
        srow = (fin == 'E') ? ((srow + n < rows) ? srow + n : rows - 1) : (srow > n ? srow - n : 0);
        t->row = top + srow;
        t->col = 0;
        break;

    case 'G': case '`':                         /* колонка */
        t->col = (n <= TTY_COLS) ? n - 1 : TTY_COLS - 1;
        break;

    case 'd':                                   /* строка */
        t->row = top + ((n <= rows) ? n - 1 : rows - 1);
        break;

    case 'H': case 'f': {                       /* строка;колонка (с 1) */
        UINT32 r = (t->npar > 0 && t->par[0] > 0) ? t->par[0] : 1;
        UINT32 c = (t->npar > 1 && t->par[1] > 0) ? t->par[1] : 1;
        t->row = top + ((r <= rows) ? r - 1 : rows - 1);
        t->col = (c <= TTY_COLS) ? c - 1 : TTY_COLS - 1;
        break;
    }

    case 'J': {                                 /* стереть экран */
        UINT32 mode = t->npar ? t->par[0] : 0;
        for (UINT32 r = 0; r < rows; r++) {
            UINT32 *l = tty_line(t, top + r);
            for (UINT32 c = 0; c < TTY_COLS; c++) {
                BOOLEAN erase = (mode >= 2) ||
                                (mode == 0 && (r > srow || (r == srow && c >= t->col))) ||
                                (mode == 1 && (r < srow || (r == srow && c <= t->col)));
                if (erase)
                    l[c] = blank;
            }
        }
        break;
    }

    case 'K': {                                 /* стереть строку */
        UINT32 mode = t->npar ? t->par[0] : 0;
        UINT32 *l = tty_line(t, t->row);
        for (UINT32 c = 0; c < TTY_COLS; c++)
            if (mode == 2 || (mode == 0 && c >= t->col) || (mode == 1 && c <= t->col))
                l[c] = blank;
        break;
    }

    case 'X': {                                 /* стереть n символов */
        UINT32 *l = tty_line(t, t->row);
        for (UINT32 c = t->col; c < TTY_COLS && c < t->col + n; c++)
            l[c] = blank;
        break;
    }

    case 'P': {                                 /* удалить n символов */
        UINT32 *l = tty_line(t, t->row);
        for (UINT32 c = t->col; c < TTY_COLS; c++)
            l[c] = (c + n < TTY_COLS) ? l[c + n] : blank;
        break;
    }

    case '@': {                                 /* вставить n пробелов */
        UINT32 *l = tty_line(t, t->row);
        for (UINT32 c = TTY_COLS; c-- > t->col;)
            l[c] = (c >= t->col + n) ? l[c - n] : blank;
        break;
    }

    case 'L': case 'M': {                       /* вставить / удалить строки */
        UINT32 bot = t->rbot ? t->rbot : rows - 1;
        if (srow < (t->rbot ? t->rtop : 0) || srow > bot)
            break;
        for (UINT32 i = 0; i < n && i <= bot - srow; i++)
            tty_scroll_region(t, srow, bot, fin == 'M');
        t->col = 0;
        break;
    }

    case 'S': case 'T': {                       /* прокрутить экран */
        UINT32 a = t->rbot ? t->rtop : 0, b = t->rbot ? t->rbot : rows - 1;
        for (UINT32 i = 0; i < n && i <= b - a; i++)
            tty_scroll_region(t, a, b, fin == 'S');
        break;
    }

    case 'r': {                                 /* область прокрутки */
        UINT32 a = (t->npar > 0 && t->par[0] > 0) ? t->par[0] - 1 : 0;
        UINT32 b = (t->npar > 1 && t->par[1] > 0) ? t->par[1] - 1 : rows - 1;
        if (b >= rows)
            b = rows - 1;
        if (a < b && !(a == 0 && b == rows - 1)) {
            t->rtop = a;
            t->rbot = b;
        } else {
            t->rtop = t->rbot = 0;
        }
        t->row = top;
        t->col = 0;
        break;
    }

    case 'm':
        tty_sgr(t);
        break;

    case 's':
        t->srow = srow;
        t->scol = t->col;
        break;

    case 'u':
        t->row = top + ((t->srow < rows) ? t->srow : rows - 1);
        t->col = t->scol;
        break;

    case 'h': case 'l':                         /* режимы */
        if (t->par_q && t->npar > 0) {
            if (t->par[0] == 25)
                t->hide_cursor = (fin == 'l');
            /* 1049/47/1047 - "второй экран" (vi, less): просто чистый */
            if ((t->par[0] == 1049 || t->par[0] == 47 || t->par[0] == 1047) && fin == 'h') {
                for (UINT32 r = 0; r < rows; r++)
                    tty_clear_line(tty_line(t, top + r));
                t->row = top;
                t->col = 0;
            }
        }
        break;

    case 'n':                                   /* где курсор? - ответ клавишами */
        if (t->npar > 0 && t->par[0] == 6) {
            char rep[24];
            ksnprintf(rep, sizeof(rep), "\x1b[%u;%uR", srow + 1, t->col + 1);
            for (UINTN i = 0; rep[i]; i++)
                tty_push_key(t, (UINT8)rep[i] == 0x1B ? 0x1B : (INT32)(UINT8)rep[i]);
        }
        break;
    }
}

/* Байт в режиме "после ESC": начало последовательности или короткая
   команда. TRUE - символ съеден. */
static BOOLEAN tty_escape(KTTY *t, UINT32 c)
{
    if (t->esc == 0) {
        if (c != 0x1B)
            return FALSE;
        t->esc = 1;
        return TRUE;
    }

    if (t->esc == 1) {

        t->esc = 0;

        switch (c) {
        case '[':
            t->esc = 2;
            t->npar = 0;
            t->par[0] = 0;
            t->par_q = FALSE;
            break;
        case ']':
            t->esc = 3;
            break;
        case '(': case ')':
            t->esc = 4;
            break;
        case '7':
            t->srow = t->row - tty_screen_top(t);
            t->scol = t->col;
            break;
        case '8':
            t->row = tty_screen_top(t) + t->srow;
            t->col = t->scol;
            break;
        case 'M': {                             /* вверх, у верха - прокрутка вниз */
            UINT32 top = tty_screen_top(t);
            UINT32 a = t->rbot ? t->rtop : 0;
            if (t->row - top <= a)
                tty_scroll_region(t, a, t->rbot ? t->rbot : tty_screen_rows(t) - 1, FALSE);
            else
                t->row--;
            break;
        }
        case 'D':
            tty_newline(t);
            break;
        case 'c':                               /* сброс */
            t->fgc = t->bgc = 0;
            t->bold = t->rev = FALSE;
            t->rtop = t->rbot = 0;
            break;
        }

        return TRUE;
    }

    if (t->esc == 2) {

        if (c >= '0' && c <= '9') {
            if (t->npar == 0)
                t->npar = 1;
            t->par[t->npar - 1] = t->par[t->npar - 1] * 10u + (c - '0');
            return TRUE;
        }

        if (c == ';') {
            if (t->npar == 0)
                t->npar = 1;
            if (t->npar < 8)
                t->par[t->npar++] = 0;
            return TRUE;
        }

        if (c == '?' || c == '>' || c == '=') {
            t->par_q = TRUE;
            return TRUE;
        }

        t->esc = 0;

        if (c >= 0x40 && c <= 0x7E)
            tty_csi(t, c);

        return TRUE;
    }

    if (t->esc == 3) {                          /* OSC: до BEL или ESC \ */
        if (c == 7)
            t->esc = 0;
        else if (c == 0x1B)
            t->esc = 1;
        return TRUE;
    }

    t->esc = 0;                                 /* "ESC ( B" - набор символов */
    return TRUE;
}

void tty_putc(KTTY *t, UINT32 c)
{
    if (tty_escape(t, c))
        return;

    if (c == '\r') {
        t->col = 0;
        return;
    }

    if (c == '\n') {
        tty_newline(t);
        return;
    }

    if (c == '\b') {
        if (t->col > 0)
            t->col--;
        return;
    }

    if (c == '\t') {
        do {
            tty_putc(t, ' ');
        } while (t->col % 8 != 0);
        return;
    }

    if (c == 7 || c < 32)
        return;

    if (t->col >= TTY_COLS)
        tty_newline(t);

    tty_line(t, t->row)[t->col++] = CELL(c & 0x1FFFFFu, 0, 0) | tty_attr(t);

    /* в журнал - как есть (UTF-8) */
    if (t->loglen + 4 < sizeof(t->logline))
        t->loglen += (UINT32)utf8_put(c, &t->logline[t->loglen]);
}

static void tty_changed(KTTY *t)
{
    t->scroll = 0;              /* новый вывод - вниз */
    if (t->win != NULL)
        wm_invalidate(t->win);
}

/* Вывод программы: байты UTF-8 */
void tty_write(KTTY *t, const char *s, UINTN n)
{
    for (UINTN i = 0; i < n; i++) {

        UINT8 b = (UINT8)s[i];

        if (t->u8_need > 0) {
            if ((b & 0xC0) == 0x80) {
                t->u8_cp = (t->u8_cp << 6) | (b & 0x3Fu);
                if (--t->u8_need == 0)
                    tty_putc(t, t->u8_cp);
                continue;
            }
            t->u8_need = 0;          /* оборванная последовательность */
        }

        if (b < 0x80) {
            tty_putc(t, b);
        } else if ((b & 0xE0) == 0xC0) {
            t->u8_cp = b & 0x1Fu;
            t->u8_need = 1;
        } else if ((b & 0xF0) == 0xE0) {
            t->u8_cp = b & 0x0Fu;
            t->u8_need = 2;
        } else if ((b & 0xF8) == 0xF0) {
            t->u8_cp = b & 0x07u;
            t->u8_need = 3;
        }
    }

    tty_changed(t);
}

/* Вывод команды ядра (SYS_KCMD): print() шлёт сюда по символу CHAR16 */
static EFI_STATUS EFIAPI tty_out_string(SIMPLE_TEXT_OUTPUT_INTERFACE *this, CHAR16 *s)
{
    KTTY *t = (KTTY *)this;

    for (; *s; s++)
        tty_putc(t, *s);

    tty_changed(t);
    return EFI_SUCCESS;
}

static EFI_STATUS EFIAPI tty_out_attr(SIMPLE_TEXT_OUTPUT_INTERFACE *this, UINTN a)
{
    (void)this;
    (void)a;
    return EFI_SUCCESS;
}

static EFI_STATUS EFIAPI tty_out_clear(SIMPLE_TEXT_OUTPUT_INTERFACE *this)
{
    KTTY *t = (KTTY *)this;

    /* clear - начать с чистого экрана */
    t->base = 0;
    t->nlines = 1;
    t->row = 0;
    t->col = 0;
    tty_clear_line(tty_line(t, 0));
    tty_changed(t);

    return EFI_SUCCESS;
}

SIMPLE_TEXT_OUTPUT_INTERFACE *tty_output(KTTY *t)
{
    return &t->out;
}

/* ---------------- клавиатура ---------------- */

static void tty_push_key(KTTY *t, INT32 k)
{
    UINT32 next = (t->ktail + 1) % TTY_KEYS;

    if (next == t->khead)
        return;                     /* очередь полна - клавиша теряется */

    t->keys[t->ktail] = k;
    t->ktail = next;

    sched_wake_all(t);
    lx_poll_wake();         /* программа Linux могла ждать в poll (этап 11) */
}

/* Есть ли клавиша в очереди (poll программы Linux), не забирая её */
BOOLEAN tty_key_ready(KTTY *t)
{
    return t->khead != t->ktail;
}

/* Сколько строк текста видно в окне (ioctl TIOCGWINSZ) */
UINT32 tty_rows(KTTY *t)
{
    if (t->win == NULL || t->win->ch < 8 + FONT_H)
        return 25;

    return (UINT32)((t->win->ch - 8) / FONT_H);
}

/*
 * Клавиша для программы (SYS_READKEY): символ Юникода или
 * MYOS_KEY_SPECIAL | скан-код. timeout_ms < 0 - ждать сколько угодно.
 * -1 - нет клавиши (время вышло, программу останавливают, окно закрыто).
 */
INT32 tty_getkey(KTTY *t, struct KPROC *p, INT64 timeout_ms)
{
    UINT64 t0 = g_kticks;
    UINT64 fl = kx_irq_save();
    INT32 k = -1;

    for (;;) {

        if (t->khead != t->ktail) {
            k = t->keys[t->khead];
            t->khead = (t->khead + 1) % TTY_KEYS;
            break;
        }

        if (p->killed || t->win == NULL)
            break;

        if (timeout_ms >= 0 && g_kticks - t0 >= (UINT64)timeout_ms)
            break;

        sched_block(t, "terminal key", timeout_ms >= 0 ? 10 : 100);
    }

    kx_irq_restore(fl);
    return k;
}

/*
 * Строка для обычной программы (read с fd 0): эхо, Backspace, Enter -
 * как у консоли ядра (proc_read_console), только в окне.
 */
INTN tty_read_line(KTTY *t, struct KPROC *p, char *dst, UINTN n)
{
    char line[PROC_IN_MAX];
    UINTN len = 0;

    for (;;) {

        INT32 k = tty_getkey(t, p, -1);

        if (k < 0) {
            if (p->killed || t->win == NULL)
                return 0;
            continue;
        }

        if (k == '\r' || k == '\n') {
            tty_write(t, "\n", 1);
            line[len++] = '\n';
            break;
        }

        if (k == '\b') {
            if (len > 0) {
                /* стереть весь символ UTF-8 */
                while (len > 0 && ((UINT8)line[len - 1] & 0xC0) == 0x80)
                    len--;
                if (len > 0)
                    len--;
                tty_write(t, "\b \b", 3);
            }
            continue;
        }

        if (k >= 32 && !(k & MYOS_KEY_SPECIAL) && len + 5 < sizeof(line)) {
            char u[4];
            UINTN m = utf8_put((UINT32)k, u);
            for (UINTN i = 0; i < m; i++)
                line[len++] = u[i];
            tty_write(t, u, m);
        }
    }

    if (len > n)
        len = n;

    for (UINTN i = 0; i < len; i++)
        dst[i] = line[i];

    return (INTN)len;
}

/* ---------------- окно ---------------- */

/* Цвет ячейки: палитра VT100 (как у xterm) */
static UINT32 tty_color(UINT32 c, BOOLEAN fg)
{
    static const UINT32 pal[16] = {
        0x000000, 0xCD3131, 0x0DBC79, 0xE5E510, 0x2472C8, 0xBC3FBC, 0x11A8CD, 0xE5E5E5,
        0x666666, 0xF14C4C, 0x23D18B, 0xF5F543, 0x3B8EEA, 0xD670D6, 0x29B8DB, 0xFFFFFF,
    };

    if (c >= 1 && c <= 16)
        return pal[c - 1];
    if (c == 17)
        return TTY_FG;
    if (c == 18)
        return TTY_BG;

    return fg ? TTY_FG : TTY_BG;
}

static void tty_paint(WIN *w, GFX *g)
{
    KTTY *t = ((TTY_WIN *)w->state)->t;

    gfx_fill(g, 0, 0, g->w, g->h, TTY_BG);

    UINT32 vis = (UINT32)((g->h - 8) / FONT_H);

    if (vis < 1)
        vis = 1;

    /* нижняя видимая строка: строка курсора (или последняя), минус
       прокрутка */
    UINT32 last = t->nlines - 1;

    if (t->scroll > last)
        t->scroll = last;

    UINT32 bottom = last - t->scroll;
    UINT32 top = (bottom + 1 > vis) ? bottom + 1 - vis : 0;

    char buf[TTY_COLS * 4 + 1];
    INT32 y = 4;

    for (UINT32 r = top; r <= bottom; r++) {

        UINT32 *l = tty_line(t, r);
        UINTN used = TTY_COLS;

        while (used > 0 && l[used - 1] == ' ')
            used--;

        /* кусками одного цвета: фон - прямоугольником, потом текст */
        UINTN i = 0;

        while (i < used) {

            UINT32 fg = CELL_FG(l[i]), bg = CELL_BG(l[i]);
            UINTN j = i, k = 0;

            while (j < used && CELL_FG(l[j]) == fg && CELL_BG(l[j]) == bg) {
                k += utf8_put(CELL_CH(l[j]), &buf[k]);
                j++;
            }

            buf[k] = '\0';

            INT32 x = 4 + (INT32)i * FONT_W;

            if (bg != 0)
                gfx_fill(g, x, y, (INT32)(j - i) * FONT_W, FONT_H, tty_color(bg, FALSE));

            gfx_text(g, x, y, buf, tty_color(fg, TRUE));
            i = j;
        }

        if (r == t->row && t->scroll == 0 && !t->hide_cursor) {
            UINT32 cc = (t->col < TTY_COLS) ? t->col : TTY_COLS - 1;
            gfx_fill(g, 4 + (INT32)cc * FONT_W, y + FONT_H - 3, FONT_W, 3, TTY_CURSOR);
        }

        y += FONT_H;
    }
}

static void tty_event(WIN *w, struct myos_event *e)
{
    KTTY *t = ((TTY_WIN *)w->state)->t;

    if (e->type == EV_WHEEL) {
        if (e->wheel > 0)
            t->scroll += 3;
        else
            t->scroll = (t->scroll > 3) ? t->scroll - 3 : 0;
        wm_invalidate(w);
        return;
    }

    if (e->type != EV_KEY)
        return;

    if (e->key == 0 && (e->scan == KEY_PGUP || e->scan == KEY_PGDN)) {
        UINT32 page = (UINT32)((w->ch - 8) / FONT_H) - 1;
        if (e->scan == KEY_PGUP)
            t->scroll += page;
        else
            t->scroll = (t->scroll > page) ? t->scroll - page : 0;
        wm_invalidate(w);
        return;
    }

    /* Ctrl+C: остановить программу на переднем плане; шеллу (читает
       клавиши сам) - просто клавиша 3 */
    if (e->key == 3) {
        KPROC *f = t->fg;
        /* программа Linux: SIGINT (если она сама не читает Ctrl+C) */
        if (f != NULL && f->is_linux && !f->exited) {
            if (lx_term_isig(f)) {
                lx_ctrl_c(f);
                return;
            }
        } else if (f != NULL && !f->raw_keys && !f->exited) {
            ksnprintf(f->why, sizeof(f->why), "stopped with Ctrl+C");
            proc_kill(f);
            return;
        }
    }

    if (e->key != 0)
        tty_push_key(t, (INT32)e->key);
    else
        tty_push_key(t, (INT32)(MYOS_KEY_SPECIAL | e->scan));
}

static void tty_close(WIN *w)
{
    KTTY *t = ((TTY_WIN *)w->state)->t;

    t->win = NULL;

    /* всем программам в этом окне - "стоп" */
    for (UINTN i = 0; i < PROC_MAX; i++)
        if (g_procs[i].used && g_procs[i].tty == t && !g_procs[i].exited) {
            ksnprintf(g_procs[i].why, sizeof(g_procs[i].why), "its terminal window was closed");
            proc_kill(&g_procs[i]);
        }

    sched_wake_all(t);
    tty_free_if_unused(t);
}

static const WIN_CLASS g_tty_class = {
    "Терминал", NULL, tty_paint, tty_event, NULL, tty_close
};

/* Поток окна: запустить /bin/sh, дождаться; шелл закрыли (exit) -
   закрыть и окно */
static void tty_shell_job(void *arg)
{
    KTTY *t = (KTTY *)arg;
    INTN err = VFS_OK;

    tty_ref(t);                 /* пока поток работает - не освобождать */

    KPROC *p = proc_spawn_ex("/bin/sh", t->cmd, PROC_IO_TTY, NULL, NULL, -1, t, &err);

    if (p == NULL) {
        tty_say(t, "Cannot start /bin/sh\n");
        tty_unref(t);
        return;
    }

    t->fg = p;

    proc_wait(p);

    if (t->win != NULL)
        wm_close(t->win);

    tty_unref(t);
}

/*
 * Открыть окно терминала с шеллом. cmd - первая команда шелла (ярлык
 * программы на рабочем столе: "calc") или "" - просто приглашение.
 */
void tty_open_window(const char *cmd)
{
    KTTY *t = (KTTY *)kmalloc(sizeof(KTTY));
    TTY_WIN *tw = (TTY_WIN *)kmalloc(sizeof(TTY_WIN));

    if (t == NULL || tw == NULL) {
        if (t) kfree(t);
        if (tw) kfree(tw);
        return;
    }

    raw_zero_mem((volatile UINT8 *)t, sizeof(*t));

    t->out = *g_st->ConOut;
    t->out.OutputString = tty_out_string;
    t->out.SetAttribute = tty_out_attr;
    t->out.ClearScreen = tty_out_clear;

    t->nlines = 1;
    tty_clear_line(tty_line(t, 0));
    ksnprintf(t->cmd, sizeof(t->cmd), "%s", cmd ? cmd : "");

    tw->t = t;

    static WIN_CLASS cls;
    cls = g_tty_class;
    cls.icon = gui_icon("terminal");

    WIN *w = wm_open(&cls, TTY_COLS * FONT_W + 8, 24 * FONT_H + 8, "Терминал", tw);

    if (w == NULL) {
        kfree(t);
        kfree(tw);
        return;
    }

    t->win = w;

    if (kthread_create("term", tty_shell_job, t, 8) == NULL) {
        tty_say(t, "No threads left to start the shell\n");
    }
}

/* На переднем плане терминала - программа (или NULL) */
void tty_set_fg(KTTY *t, struct KPROC *p)
{
    if (t != NULL)
        t->fg = p;
}

struct KPROC *tty_get_fg(KTTY *t)
{
    return t ? t->fg : NULL;
}
