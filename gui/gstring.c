/*
 * gui/gstring.c - строки GUI (char), терминал GUI - хранение строк.
 * Часть MyOS; общие объявления - в myos.h.
 */
#include "myos.h"


/*
 * Добавить строку в скроллбек терминала. Если буфер
 * уже заполнен, самая старая строка "уезжает" вверх -
 * обычное поведение прокрутки в любом терминале.
 */
/* Строки терминала GUI пишут два потока: сам GUI (ответы команд)
   и поток долгого задания (SLEEP/SPIN, gui/terminal.c), а читает
   GUI, когда рисует окно. Мьютекс - чтобы рисование не застало
   список посреди сдвига строк. */
KMUTEX g_term_mutex = KMUTEX_INIT("gui terminal");

void gui_term_push(
    char lines[][GUI_TERM_LINE_LEN + 1],
    UINTN *count,
    const char *text
)
{
    kmutex_lock(&g_term_mutex);

    UINTN n = *count;

    if (n >= GUI_TERM_MAX_LINES) {

        for (UINTN i = 1; i < GUI_TERM_MAX_LINES; i++) {

            for (UINTN j = 0; j < GUI_TERM_LINE_LEN + 1; j++)
                lines[i - 1][j] = lines[i][j];
        }

        n = GUI_TERM_MAX_LINES - 1;
    }

    UINTN i = 0;

    while (text[i] != '\0' && i < GUI_TERM_LINE_LEN) {
        lines[n][i] = text[i];
        i++;
    }

    lines[n][i] = '\0';

    *count = n + 1;

    kmutex_unlock(&g_term_mutex);
}

/* Очистить терминал (CLEAR) - тоже под мьютексом */
void gui_term_clear(UINTN *count)
{
    kmutex_lock(&g_term_mutex);
    *count = 0;
    kmutex_unlock(&g_term_mutex);
}


/*
 * Сравнение начала обычной char-строки с префиксом
 * (аналог starts_with(), но для команд терминала).
 */
int gui_starts_with(const char *s, const char *prefix)
{
    while (*prefix) {

        if (*s != *prefix)
            return 0;

        s++;
        prefix++;
    }

    return 1;
}


/*
 * Взять первое "слово" (до пробела или конца строки)
 * из char-строки. Возвращает длину слова.
 */
UINTN gui_take_word(const char *s, char *out, UINTN max)
{
    UINTN i = 0;

    while (
        s[i] != '\0' &&
        s[i] != ' ' &&
        i < max - 1
    ) {
        out[i] = s[i];
        i++;
    }

    out[i] = '\0';

    return i;
}


/*
 * Простая char -> CHAR16 копия (имена файлов в
 * терминале - обычный char, но RAM-FS хранит CHAR16).
 */
void gui_char_to_char16(
    const char *src,
    CHAR16 *dst,
    UINTN max
)
{
    UINTN i = 0;

    while (src[i] != '\0' && i < max - 1) {
        dst[i] = (CHAR16)(unsigned char)src[i];
        i++;
    }

    dst[i] = 0;
}


/*
 * И обратно: CHAR16 -> char, для вывода содержимого
 * файлов и списков в терминал (символы вне ASCII
 * заменяются на '?', как заглушку не-ASCII текста).
 */
UINTN gui_char16_to_char(
    const CHAR16 *src,
    char *dst,
    UINTN max
)
{
    UINTN i = 0;

    while (src[i] != 0 && i < max - 1) {

        CHAR16 wc = src[i];

        dst[i] = (wc < 128) ? (char)wc : '?';
        i++;
    }

    dst[i] = '\0';

    return i;
}


/*
 * Простое копирование обычной char-строки с ограничением
 * по размеру (аналог char16_copy(), но для char).
 */
void gui_str_copy8(
    char *dst,
    const char *src,
    UINTN max
)
{
    UINTN i = 0;

    while (src[i] != '\0' && i < max - 1) {
        dst[i] = src[i];
        i++;
    }

    dst[i] = '\0';
}
