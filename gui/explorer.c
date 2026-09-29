/*
 * gui/explorer.c - Проводник GUI поверх VFS (этап 5).
 * Часть MyOS; общие объявления - в myos.h.
 *
 * Раньше Проводник показывал только RAM-диск. Теперь он ходит по
 * всем томам: в корне "/" - тома (RAM, флешки, разделы дисков),
 * клик по папке - зайти в неё, по ".." - выйти наверх, по файлу -
 * открыть его в окне просмотра.
 *
 * Шрифт GUI знает только заглавные латинские буквы, цифры и немного
 * знаков - поэтому имена показываются ЗАГЛАВНЫМИ, а настоящие имена
 * (для открытия) хранятся отдельно в ex->names.
 */
#include "myos.h"

/* Строка для пиксельного шрифта: заглавные, неизвестное - '_' */
static UINTN ex_put(char *dst, UINTN pos, UINTN cap, const char *src)
{
    for (UINTN i = 0; src[i] && pos + 1 < cap; i++) {
        char c = src[i];
        if (c >= 'a' && c <= 'z')
            c = (char)(c - 'a' + 'A');
        if (gui_find_glyph(c) == NULL && c != ' ')
            c = '_';
        dst[pos++] = c;
    }

    dst[pos] = '\0';
    return pos;
}

typedef struct {
    GUI_EXPLORER *ex;
    UINTN total;
} EX_CTX;

static void ex_row(GUI_EXPLORER *ex, INT8 kind, const char *name, const char *text)
{
    if (ex->count >= GUI_EXPLORER_MAX_ROWS)
        return;

    UINTN r = ex->count++;
    UINTN k = 0;

    for (; name[k] && k + 1 < VFS_NAME_MAX; k++)
        ex->names[r][k] = name[k];

    ex->names[r][k] = '\0';
    ex->kind[r] = kind;
    ex_put(ex->buf[r], 0, GUI_EXPLORER_LINE_LEN, text);
}

static INTN ex_cb(void *ctx, const VFS_DIRENT *e)
{
    EX_CTX *c = (EX_CTX *)ctx;
    char text[GUI_EXPLORER_LINE_LEN + 16];

    c->total++;

    if (e->node.is_dir) {
        ksnprintf(text, sizeof(text), "%s/", e->name);
    } else {
        char nm[30];
        UINTN k = 0;
        for (; e->name[k] && k < 28; k++)
            nm[k] = e->name[k];
        nm[k] = '\0';
        ksnprintf(text, sizeof(text), "%-28s %llu B", nm, e->node.size);
    }

    ex_row(c->ex, e->node.is_dir ? 1 : 0, e->name, text);

    return 0;
}

/* Перечитать открытую папку ex->path в строки окна */
UINTN gui_explorer_fill(GUI_EXPLORER *ex, const char **lines)
{
    EX_CTX c = { ex, 0 };

    ex->count = 0;

    if (ex->path[0] == '\0') {
        ex->path[0] = '/';
        ex->path[1] = '\0';
    }

    /* первая строка - где мы */
    {
        char head[VFS_PATH_MAX + 8];
        ksnprintf(head, sizeof(head), "IN %s", ex->path);
        ex_row(ex, -1, "", head);
    }

    if (ex->path[1] != '\0')
        ex_row(ex, 2, "..", ".. (UP)");

    INTN r = vfs_list(ex->path, ex_cb, &c);

    if (r != VFS_OK) {
        ex_row(ex, -1, "", vfs_strerror(r));
    } else if (c.total == 0) {
        ex_row(ex, -1, "", "(EMPTY)");
    } else if (c.total + 2 > GUI_EXPLORER_MAX_ROWS) {
        ex->count = GUI_EXPLORER_MAX_ROWS - 1;
        ex_row(ex, -1, "", "... MORE - SEE LS IN THE TERMINAL");
    }

    for (UINTN i = 0; i < ex->count; i++) {
        lines[i] = ex->buf[i];
        klog("gui: explorer row: %s\n", ex->buf[i]);
    }

    return ex->count;
}

/*
 * Клик по строке row. Папка или ".." - сменить ex->path (вызывающий
 * перечитает список) и вернуть FALSE; файл - полный путь в file_path
 * и TRUE.
 */
BOOLEAN gui_explorer_click(GUI_EXPLORER *ex, UINTN row, char *file_path, UINTN cap)
{
    if (row >= ex->count || ex->kind[row] < 0)
        return FALSE;

    char next[VFS_PATH_MAX];

    if (ex->kind[row] == 2)
        ksnprintf(next, sizeof(next), "%s/..", ex->path);
    else if (ex->path[1] == '\0')
        ksnprintf(next, sizeof(next), "/%s", ex->names[row]);
    else
        ksnprintf(next, sizeof(next), "%s/%s", ex->path, ex->names[row]);

    char norm[VFS_PATH_MAX];

    if (vfs_normalize(next, norm, sizeof(norm)) != VFS_OK)
        return FALSE;

    if (ex->kind[row] == 0) {
        ksnprintf(file_path, cap, "%s", norm);
        return TRUE;
    }

    ksnprintf(ex->path, sizeof(ex->path), "%s", norm);
    klog("gui: explorer: %s\n", ex->path);

    return FALSE;
}

/* Окно просмотра файла по пути (текст, порезанный на строки) */
UINTN gui_open_fileview_path(const char *path, char fileview_buf[][GUI_FILEVIEW_LINE_LEN],
                             const char **fileview_lines, char *title_buf, UINTN title_cap)
{
    const char *base = path;

    for (const char *p = path; *p; p++)
        if (*p == '/')
            base = p + 1;

    ex_put(title_buf, 0, title_cap, base);

    UINTN cap = GUI_FILEVIEW_MAX_ROWS * GUI_FILEVIEW_LINE_LEN;
    char *body = (char *)kmalloc(cap + 1);
    UINTN got = 0;

    if (body == NULL) {
        gui_str_copy8(fileview_buf[0], "OUT OF MEMORY", GUI_FILEVIEW_LINE_LEN);
        fileview_lines[0] = fileview_buf[0];
        return 1;
    }

    INTN r = vfs_read_file(path, body, cap, &got);

    klog("gui: fileview %s (%u bytes)\n", path, (UINT32)got);

    if (r != VFS_OK || got == 0) {
        ex_put(fileview_buf[0], 0, GUI_FILEVIEW_LINE_LEN,
               r != VFS_OK ? vfs_strerror(r) : "EMPTY FILE");
        fileview_lines[0] = fileview_buf[0];
        kfree(body);
        return 1;
    }

    UINTN rows = 0, p = 0;

    while (p < got && rows < GUI_FILEVIEW_MAX_ROWS) {

        char line[GUI_FILEVIEW_LINE_LEN];
        UINTN n = 0;

        while (p < got && body[p] != '\n' && n < GUI_FILEVIEW_LINE_LEN - 1) {
            char c = body[p++];
            line[n++] = (c == '\t' || c == '\r') ? ' ' : c;
        }

        line[n] = '\0';

        if (p < got && body[p] == '\n')
            p++;

        ex_put(fileview_buf[rows], 0, GUI_FILEVIEW_LINE_LEN, line);
        fileview_lines[rows] = fileview_buf[rows];
        rows++;
    }

    if (p < got && rows == GUI_FILEVIEW_MAX_ROWS) {
        gui_str_copy8(fileview_buf[rows - 1], "...", GUI_FILEVIEW_LINE_LEN);
        fileview_lines[rows - 1] = fileview_buf[rows - 1];
    }

    kfree(body);
    return rows;
}
