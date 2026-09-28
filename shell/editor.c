/*
 * shell/editor.c - построчный редактор edit (с этапа 5 - для любого
 * файла через VFS: и на RAM-диске, и на флешке).
 * Часть MyOS; общие объявления - в myos.h.
 *
 * Показывает, что в файле было, потом принимает новые строки до
 * строки из одной точки "." - и записывает их в файл ВМЕСТО старого
 * текста. Пустая правка (сразу ".") ничего не меняет.
 */
#include "myos.h"

#define EDIT_MAX 16384

void cmd_edit(EFI_SYSTEM_TABLE *st, const char *path)
{
    SIMPLE_TEXT_OUTPUT_INTERFACE *out = st->ConOut;
    char *buf = (char *)kmalloc(EDIT_MAX + 1);

    if (buf == NULL) {
        print(out, "Out of memory.\n");
        return;
    }

    /* что было */
    UINTN got = 0;
    VFS_DIRENT e;

    if (vfs_stat(path, &e) == VFS_OK) {

        if (e.node.is_dir) {
            print(out, "That is a folder.\n");
            kfree(buf);
            return;
        }

        vfs_read_file(path, buf, EDIT_MAX, &got);
        buf[got] = '\0';

        if (got > 0) {
            print(out, "--- current text ---\n");
            for (UINTN i = 0; i < got; i++)
                if ((UINT8)buf[i] < 32 && buf[i] != '\n')
                    buf[i] = '?';
            print(out, buf);
            if (buf[got - 1] != '\n')
                print(out, "\n");
            print(out, "--------------------\n");
        }
    }

    kprintf(out, "Editing '%s'. Type the new text line by line; a line with just '.'\n"
                 "finishes (the old text is replaced; '.' at once - keep it as is).\n", path);

    UINTN pos = 0;
    UINTN lines = 0;
    CHAR16 ln[LINE_MAX];

    for (;;) {

        print(out, ": ");
        read_line(st, ln, LINE_MAX);

        if (ln[0] == L'.' && ln[1] == 0)
            break;

        for (UINTN i = 0; ln[i] != 0 && pos + 2 < EDIT_MAX; i++)
            buf[pos++] = (ln[i] < 128) ? (char)ln[i] : '?';

        if (pos + 1 < EDIT_MAX)
            buf[pos++] = '\n';

        lines++;
    }

    if (lines == 0) {
        print(out, "Nothing typed - the file is unchanged.\n");
        kfree(buf);
        return;
    }

    INTN r = vfs_write_file(path, buf, pos, FALSE);

    if (r != VFS_OK)
        kprintf(out, "Could not save %s: %s\n", path, vfs_strerror(r));
    else
        kprintf(out, "Saved: %u line(s), %u bytes.\n", (UINT32)lines, (UINT32)pos);

    kfree(buf);
}
