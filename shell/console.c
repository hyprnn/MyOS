/*
 * shell/console.c - цвет, прокрутка истории экрана (scrollback), print*.
 * Часть MyOS; общие объявления - в myos.h.
 */
#include "myos.h"

/*
 * Атрибут (цвет), который реально действует
 * на экране ПРЯМО СЕЙЧАС — обновляется в
 * set_color() при каждом вызове SetAttribute.
 *
 * В отличие от g_color (который меняется только
 * командой "color"), это отражает и временные
 * перекраски (лого, заголовки fetch, ошибки и т.д.),
 * поэтому именно g_current_attr пишется в scrollback
 * вместе с каждым символом.
 */
UINTN g_current_attr = 0x0F;

/* Время загрузки */
EFI_TIME g_boot_time;
BOOLEAN  g_have_boot_time = FALSE;


/*
 * Обёртка над SetAttribute(), которая ещё и
 * запоминает текущий цвет в g_current_attr,
 * чтобы scrollback знал, каким цветом был
 * напечатан каждый символ.
 */
void set_color(
    SIMPLE_TEXT_OUTPUT_INTERFACE *out,
    UINTN attr
)
{
    g_current_attr = attr;
    out->SetAttribute(out, attr);
}

/*
 * Сколько строк истории помещается на экран при прокрутке
 * (PageUp/PageDown). В режиме прошивки консоль 80x25 -> 24 строки
 * (последняя под prompt). После "ebs" консоль своя, и строк в ней
 * больше (зависит от разрешения) - kernel mode выставляет это
 * значение заново. Поэтому переменная, а не константа.
 */
UINTN g_scrollback_visible_rows = 24;

CHAR16 g_scrollback[
    SCROLLBACK_MAX_LINES
][SCROLLBACK_LINE_MAX];

/*
 * Цвет (атрибут) каждого символа g_scrollback,
 * индексы синхронизированы 1-в-1.
 */
UINT8 g_scrollback_attr[
    SCROLLBACK_MAX_LINES
][SCROLLBACK_LINE_MAX];

UINTN g_scrollback_count = 0;
UINTN g_scrollback_line_len = 0;

/*
 * 0 = самый низ
 * 1 = на один экран вверх
 * 2 = ещё выше
 */
int g_scrollback_view = 0;

/*
 * TRUE, когда мы просто перерисовываем
 * уже существующий scrollback.
 *
 * В этот момент новые символы в историю
 * записывать нельзя.
 */
BOOLEAN g_scrollback_replaying = FALSE;


/* ============================================================
 * Scrollback internals
 * ============================================================ */

void scrollback_newline(void)
{
    UINTN index =
        g_scrollback_count % SCROLLBACK_MAX_LINES;

    g_scrollback[index][g_scrollback_line_len] = 0;

    g_scrollback_count++;
    g_scrollback_line_len = 0;
}


void scrollback_char(CHAR16 c)
{
    if (g_scrollback_replaying)
        return;

    if (c == L'\r')
        return;

    if (c == L'\n') {
        scrollback_newline();
        return;
    }

    /*
     * Управляющие символы не записываем.
     */
    if (c < 32)
        return;

    if (g_scrollback_line_len <
        SCROLLBACK_LINE_MAX - 1) {

        UINTN index =
            g_scrollback_count %
            SCROLLBACK_MAX_LINES;

        g_scrollback[index][g_scrollback_line_len] = c;

        g_scrollback_attr[index][g_scrollback_line_len] =
            (UINT8)g_current_attr;

        g_scrollback_line_len++;
    }
}


/*
 * Перерисовать prompt + текущую строку ввода.
 *
 * ВАЖНО:
 * здесь НЕ используется print(), иначе prompt
 * снова попадёт в scrollback.
 */
void redraw_input(
    SIMPLE_TEXT_OUTPUT_INTERFACE *out,
    const CHAR16 *line
)
{
    CHAR16 prompt[] = L"> ";

    out->OutputString(out, prompt);

    if (line)
        out->OutputString(out, (CHAR16 *)line);
}


/* ============================================================
 * Scrollback renderer
 *
 * view = 0:
 *     последние строки
 *
 * view > 0:
 *     страницы выше
 * ============================================================ */

void scrollback_render(
    EFI_SYSTEM_TABLE *st,
    int view
)
{
    SIMPLE_TEXT_OUTPUT_INTERFACE *out =
        st->ConOut;

    if (g_scrollback_count == 0)
        return;

    /*
     * Сколько строк реально хранится.
     */
    UINTN total =
        (g_scrollback_count < SCROLLBACK_MAX_LINES)
        ? g_scrollback_count
        : SCROLLBACK_MAX_LINES;

    /*
     * Сколько строк истории помещается,
     * потому что последнюю строку оставляем
     * под prompt.
     */
    UINTN visible =
        SCROLLBACK_VISIBLE_ROWS;

    int max_view;

    if (total > visible)
        max_view =
            (int)total - (int)visible;
    else
        max_view = 0;

    if (view < 0)
        view = 0;

    if (view > max_view)
        view = max_view;

    g_scrollback_view = view;


    /*
     * Самая старая строка ring-buffer.
     */
    UINTN oldest;

    if (g_scrollback_count <=
        SCROLLBACK_MAX_LINES) {

        oldest = 0;

    } else {

        oldest =
            g_scrollback_count %
            SCROLLBACK_MAX_LINES;
    }


    /*
     * Вычисляем начало страницы ОТ КОНЦА.
     *
     * view = 0:
     *
     *     [........][последние 24 строки]
     *
     * view = 1:
     *
     *     [.......][24 строки перед последними]
     */
    int start_offset =
        (int)total -
        (int)visible -
        view;

    if (start_offset < 0)
        start_offset = 0;


    UINTN start =
        (oldest + (UINTN)start_offset)
        % SCROLLBACK_MAX_LINES;


    /*
     * Теперь выводим историю.
     */
    g_scrollback_replaying = TRUE;

    out->ClearScreen(out);

    /*
     * ClearScreen() не гарантирует сохранение
     * текущего атрибута текста на всех
     * реализациях UEFI, поэтому явно
     * восстанавливаем цвет перед отрисовкой
     * истории — иначе при прокрутке вверх
     * текст мог перекраситься в цвет по
     * умолчанию.
     */
    set_color(
        out,
        g_color
    );


    for (UINTN row = 0;
         row < visible;
         row++) {

        UINTN logical =
            (UINTN)start_offset + row;

        if (logical >= total)
            break;


        UINTN index =
            (start + row)
            % SCROLLBACK_MAX_LINES;


        /*
         * Выводим строку посимвольно, меняя
         * атрибут при каждой смене цвета —
         * так сохраняется исходная раскраска
         * (лого, fetch, ошибки и т.д.) даже
         * при прокрутке вверх.
         */
        UINTN line_len = 0;

        while (line_len < SCROLLBACK_LINE_MAX &&
               g_scrollback[index][line_len] != 0)
            line_len++;

        /* заведомо невозможное значение атрибута,
         * чтобы первый символ строки гарантированно
         * выставил цвет */
        UINTN cur_attr = 0xFFFF;

        for (UINTN col = 0;
             col < line_len;
             col++) {

            UINTN attr =
                g_scrollback_attr[index][col];

            if (attr != cur_attr) {
                out->SetAttribute(
                    out,
                    attr
                );
                cur_attr = attr;
            }

            CHAR16 ch[2];

            ch[0] = g_scrollback[index][col];
            ch[1] = 0;

            out->OutputString(
                out,
                ch
            );
        }


        /*
         * UEFI лучше явно получать CRLF.
         */
        CHAR16 nl[2];

        nl[0] = L'\r';
        nl[1] = 0;

        out->OutputString(out, nl);

        nl[0] = L'\n';

        out->OutputString(out, nl);
    }


    /*
     * Возвращаем "живой" цвет — тот, которым
     * реально печатает терминал сейчас, — чтобы
     * промпт после выхода из scrollback рисовался
     * правильно, а не последним цветом истории.
     */
    out->SetAttribute(
        out,
        g_current_attr
    );

    g_scrollback_replaying = FALSE;
}


/* ============================================================
 * Output helpers
 * ============================================================ */

void print(
    SIMPLE_TEXT_OUTPUT_INTERFACE *out,
    const char *s
)
{
    CHAR16 buf[2];

    buf[1] = 0;

    /* В kernel mode весь вывод консоли дублируется в COM1 (см.
       lib/serial.c). При перерисовке истории (PageUp) - нет:
       это старый текст, в логе он уже есть. */
    if (!g_scrollback_replaying)
        serial_puts(s);   /* no-op, пока COM1 не включён */

    /* история экрана (PageUp) - только у текстовой консоли; вывод в
       окно-терминал или в файл (команды шелла, этап 10) - не её */
    BOOLEAN to_console = (out == &g_kcon_out);

    while (*s) {

        if (*s == '\n') {

            if (to_console)
                scrollback_char(L'\n');

            buf[0] = L'\r';
            out->OutputString(out, buf);

            buf[0] = L'\n';
            out->OutputString(out, buf);

        } else {

            CHAR16 c =
                (CHAR16)(unsigned char)*s;

            if (to_console)
                scrollback_char(c);

            buf[0] = c;

            out->OutputString(
                out,
                buf
            );
        }

        s++;
    }

    /*
     * Если мы не перерисовываем scrollback,
     * новый вывод возвращает нас вниз.
     */
    if (!g_scrollback_replaying && to_console)
        g_scrollback_view = 0;
}


/*
 * Вывод CHAR16 строки.
 *
 * Нужен для:
 *   - UEFI строк
 *   - файлов
 *   - пользовательского ввода
 *   - FirmwareVendor
 */
void print16(
    SIMPLE_TEXT_OUTPUT_INTERFACE *out,
    const CHAR16 *s
)
{
    if (!s) {
        print(out, "?");
        return;
    }

    if (!g_scrollback_replaying)
        serial_puts16(s);

    while (*s) {

        scrollback_char(*s);

        CHAR16 buf[2];

        buf[0] = *s;
        buf[1] = 0;

        out->OutputString(
            out,
            buf
        );

        s++;
    }

    if (!g_scrollback_replaying)
        g_scrollback_view = 0;
}


/*
 * Вывод беззнакового числа в десятичном виде.
 *
 * ВАЖНО: раньше эта функция вызывалась по всему файлу
 * (cmd_ls, cmd_touch, cmd_fetch, calc, ...), но нигде
 * не была определена - это ошибка компиляции
 * (implicit declaration of function). Добавлена здесь,
 * как можно раньше, чтобы быть видимой для всех
 * последующих вызовов в файле.
 */
void print_uint(
    SIMPLE_TEXT_OUTPUT_INTERFACE *out,
    UINT64 value
)
{
    CHAR16 digits[21];
    CHAR16 buf[21];
    UINTN  n = 0;

    if (value == 0) {
        print(out, "0");
        return;
    }

    while (value > 0 && n < 20) {
        digits[n++] = (CHAR16)(L'0' + (value % 10));
        value /= 10;
    }

    for (UINTN i = 0; i < n; i++)
        buf[i] = digits[n - 1 - i];

    buf[n] = 0;

    print16(out, buf);
}


/*
 * Печать знакового числа (для дельт мыши dX/dY из HID-отчётов -
 * они приходят как знаковый байт, движение "влево"/"вверх"
 * должно печататься со знаком минус, а не как огромное
 * беззнаковое число). Просто выводит '-' при отрицательном
 * значении и дальше переиспользует print_uint на модуле числа.
 */
void print_int(
    SIMPLE_TEXT_OUTPUT_INTERFACE *out,
    INT32 value
)
{
    if (value < 0) {
        print(out, "-");
        print_uint(out, (UINT64)(UINT32)(-value));
    } else {
        print_uint(out, (UINT64)(UINT32)value);
    }
}


/*
 * То же самое, но всегда минимум 2 цифры
 * (с ведущим нулём) - используется для часов/минут/секунд.
 * Тоже отсутствовала в исходнике - вторая недостающая
 * функция, вызывавшая ошибку компиляции.
 */
void print_uint2(
    SIMPLE_TEXT_OUTPUT_INTERFACE *out,
    UINTN value
)
{
    if (value < 10)
        print(out, "0");

    print_uint(out, (UINT64)value);
}


/*
 * Шестнадцатеричная печать с ведущими нулями до нужной
 * ширины (digits символов) - для адресов MMIO, ID устройств
 * и т.п., где удобнее фиксированная ширина, чем print_uint.
 */
void print_hex(
    SIMPLE_TEXT_OUTPUT_INTERFACE *out,
    UINT64 value,
    UINTN digits
)
{
    static const char hex_chars[] = "0123456789ABCDEF";

    CHAR16 buf[17];

    if (digits > 16)
        digits = 16;

    for (UINTN i = 0; i < digits; i++) {

        UINTN shift = (digits - 1 - i) * 4;

        buf[i] = (CHAR16)hex_chars[(value >> shift) & 0xF];
    }

    buf[digits] = 0;

    print16(out, buf);
}
