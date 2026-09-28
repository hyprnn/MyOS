/*
 * shell/fs.c - RAM-диск и его команды (ls, cat, write, ...).
 * Часть MyOS; общие объявления - в myos.h.
 */
#include "myos.h"

FS_FILE g_fs[FS_MAX_FILES];


int fs_find_free(void)
{
    for (int i = 0;
         i < FS_MAX_FILES;
         i++) {

        if (!g_fs[i].used)
            return i;
    }

    return -1;
}


void cmd_ls(
    EFI_SYSTEM_TABLE *st
)
{
    SIMPLE_TEXT_OUTPUT_INTERFACE *out =
        st->ConOut;

    int any = 0;

    for (int i = 0;
         i < FS_MAX_FILES;
         i++) {

        if (!g_fs[i].used)
            continue;

        any = 1;

        print16(
            out,
            g_fs[i].name
        );

        print(out, "  (");

        print_uint(
            out,
            g_fs[i].size
        );

        print(out, " bytes)\n");
    }

    if (!any) {

        print(
            out,
            "(empty - no files. use 'touch <name>' or 'write <name> <text>')\n"
        );
    }
}


void cmd_touch(
    EFI_SYSTEM_TABLE *st,
    CHAR16 *name
)
{
    SIMPLE_TEXT_OUTPUT_INTERFACE *out =
        st->ConOut;

    if (char16_len(name) == 0) {

        print(
            out,
            "Usage: touch <name>\n"
        );

        return;
    }


    if (fs_find(name) >= 0) {

        print(
            out,
            "File already exists.\n"
        );

        return;
    }


    int idx =
        fs_find_free();


    if (idx < 0) {

        print(
            out,
            "Filesystem full (max "
        );

        print_uint(
            out,
            FS_MAX_FILES
        );

        print(out, " files).\n");

        return;
    }


    g_fs[idx].used = TRUE;

    char16_copy(
        g_fs[idx].name,
        name,
        FS_NAME_MAX
    );

    g_fs[idx].data[0] = 0;

    g_fs[idx].size = 0;

    print(out, "Created.\n");
}


void cmd_cat(
    EFI_SYSTEM_TABLE *st,
    CHAR16 *name
)
{
    SIMPLE_TEXT_OUTPUT_INTERFACE *out =
        st->ConOut;

    if (char16_len(name) == 0) {

        print(
            out,
            "Usage: cat <name>\n"
        );

        return;
    }


    int idx =
        fs_find(name);


    if (idx < 0) {

        print(
            out,
            "No such file.\n"
        );

        return;
    }


    if (g_fs[idx].size == 0) {

        print(
            out,
            "(empty file)\n"
        );

        return;
    }


    /*
     * Важно: print16(), а не прямой OutputString(),
     * чтобы cat попадал в scrollback.
     */
    print16(
        out,
        g_fs[idx].data
    );

    print(out, "\n");
}


void cmd_write(
    EFI_SYSTEM_TABLE *st,
    CHAR16 *name,
    CHAR16 *text
)
{
    SIMPLE_TEXT_OUTPUT_INTERFACE *out =
        st->ConOut;

    if (char16_len(name) == 0) {

        print(
            out,
            "Usage: write <name> <text>\n"
        );

        return;
    }


    int idx =
        fs_find(name);


    if (idx < 0)
        idx = fs_find_free();


    if (idx < 0) {

        print(
            out,
            "Filesystem full.\n"
        );

        return;
    }


    g_fs[idx].used = TRUE;


    char16_copy(
        g_fs[idx].name,
        name,
        FS_NAME_MAX
    );


    char16_copy(
        g_fs[idx].data,
        text,
        FS_DATA_MAX
    );


    g_fs[idx].size =
        char16_len(
            g_fs[idx].data
        );


    print(out, "Written (");

    print_uint(
        out,
        g_fs[idx].size
    );

    print(out, " bytes).\n");
}


void cmd_append(
    EFI_SYSTEM_TABLE *st,
    CHAR16 *name,
    CHAR16 *text
)
{
    SIMPLE_TEXT_OUTPUT_INTERFACE *out =
        st->ConOut;

    if (char16_len(name) == 0) {

        print(
            out,
            "Usage: append <name> <text>\n"
        );

        return;
    }


    int idx =
        fs_find(name);


    if (idx < 0)
        idx = fs_find_free();


    if (idx < 0) {

        print(
            out,
            "Filesystem full.\n"
        );

        return;
    }


    if (!g_fs[idx].used) {

        g_fs[idx].used = TRUE;

        char16_copy(
            g_fs[idx].name,
            name,
            FS_NAME_MAX
        );

        g_fs[idx].data[0] = 0;

        g_fs[idx].size = 0;
    }


    UINTN cur =
        g_fs[idx].size;

    UINTN need =
        char16_len(text);

    UINTN sep =
        (cur > 0) ? 1 : 0;


    if (cur + sep + need >=
        FS_DATA_MAX) {

        print(
            out,
            "File too large, cannot append that much.\n"
        );

        return;
    }


    if (sep)
        g_fs[idx].data[cur++] =
            L'\n';


    for (UINTN i = 0;
         i < need;
         i++) {

        g_fs[idx].data[cur++] =
            text[i];
    }


    g_fs[idx].data[cur] = 0;

    g_fs[idx].size = cur;


    print(
        out,
        "Appended ("
    );

    print_uint(
        out,
        g_fs[idx].size
    );

    print(
        out,
        " bytes total).\n"
    );
}


void cmd_rm(
    EFI_SYSTEM_TABLE *st,
    CHAR16 *name
)
{
    SIMPLE_TEXT_OUTPUT_INTERFACE *out =
        st->ConOut;

    if (char16_len(name) == 0) {

        print(
            out,
            "Usage: rm <name>\n"
        );

        return;
    }


    int idx =
        fs_find(name);


    if (idx < 0) {

        print(
            out,
            "No such file.\n"
        );

        return;
    }


    g_fs[idx].used = FALSE;

    g_fs[idx].size = 0;


    print(
        out,
        "Deleted.\n"
    );
}


void cmd_mv(
    EFI_SYSTEM_TABLE *st,
    CHAR16 *oldname,
    CHAR16 *newname
)
{
    SIMPLE_TEXT_OUTPUT_INTERFACE *out =
        st->ConOut;

    if (char16_len(oldname) == 0 ||
        char16_len(newname) == 0) {

        print(
            out,
            "Usage: mv <old> <new>\n"
        );

        return;
    }


    int idx =
        fs_find(oldname);


    if (idx < 0) {

        print(
            out,
            "No such file.\n"
        );

        return;
    }


    if (fs_find(newname) >= 0) {

        print(
            out,
            "Target name already exists.\n"
        );

        return;
    }


    char16_copy(
        g_fs[idx].name,
        newname,
        FS_NAME_MAX
    );


    print(
        out,
        "Renamed.\n"
    );
}


void cmd_cp(
    EFI_SYSTEM_TABLE *st,
    CHAR16 *src,
    CHAR16 *dst
)
{
    SIMPLE_TEXT_OUTPUT_INTERFACE *out =
        st->ConOut;

    if (char16_len(src) == 0 ||
        char16_len(dst) == 0) {

        print(
            out,
            "Usage: cp <src> <dst>\n"
        );

        return;
    }


    int si =
        fs_find(src);


    if (si < 0) {

        print(
            out,
            "No such file.\n"
        );

        return;
    }


    if (fs_find(dst) >= 0) {

        print(
            out,
            "Target name already exists.\n"
        );

        return;
    }


    int di =
        fs_find_free();


    if (di < 0) {

        print(
            out,
            "Filesystem full.\n"
        );

        return;
    }


    g_fs[di].used = TRUE;


    char16_copy(
        g_fs[di].name,
        dst,
        FS_NAME_MAX
    );


    char16_copy(
        g_fs[di].data,
        g_fs[si].data,
        FS_DATA_MAX
    );


    g_fs[di].size =
        g_fs[si].size;


    print(
        out,
        "Copied.\n"
    );
}
