/*
 * gui/minesweeper_draw.c - Сапёр: окно.
 * Часть MyOS; общие объявления - в myos.h.
 */
#include "myos.h"


/*
 * Собрать окно "просмотра файла" (открывается кликом
 * по строке в Проводнике): заголовок = имя файла,
 * тело = содержимое, порезанное на строки под ширину
 * окна. idx - индекс файла в g_fs.
 */
UINTN gui_open_fileview(
    int idx,
    char fileview_buf[][GUI_FILEVIEW_LINE_LEN],
    const char **fileview_lines,
    char *title_buf
)
{
    gui_char16_to_char(
        g_fs[idx].name, title_buf, FS_NAME_MAX + 1
    );

    if (g_fs[idx].size == 0) {

        gui_str_copy8(
            fileview_buf[0], "EMPTY FILE", GUI_FILEVIEW_LINE_LEN
        );

        fileview_lines[0] = fileview_buf[0];

        return 1;
    }

    char body[FS_DATA_MAX];
    UINTN blen =
        gui_char16_to_char(
            g_fs[idx].data, body, FS_DATA_MAX
        );

    UINTN row_count = 0;
    UINTN p = 0;

    while (p < blen && row_count < GUI_FILEVIEW_MAX_ROWS) {

        char *row = fileview_buf[row_count];
        UINTN n = 0;

        while (
            p < blen &&
            body[p] != '\n' &&
            n < GUI_FILEVIEW_LINE_LEN - 1
        ) {
            row[n++] = body[p++];
        }

        row[n] = '\0';

        if (p < blen && body[p] == '\n')
            p++;

        fileview_lines[row_count] = row;
        row_count++;
    }

    if (p < blen && row_count == GUI_FILEVIEW_MAX_ROWS) {

        /* Не влезло целиком - подменяем последнюю
           показанную строку отметкой обрезки. */
        gui_str_copy8(
            fileview_buf[row_count - 1],
            "...",
            GUI_FILEVIEW_LINE_LEN
        );

        fileview_lines[row_count - 1] =
            fileview_buf[row_count - 1];
    }

    return row_count;
}
