/*
 * shell/editor.c - построчный редактор edit.
 * Часть MyOS; общие объявления - в myos.h.
 */
#include "myos.h"


/* ============================================================
 * Text editor
 * ============================================================ */

void cmd_edit(
    EFI_SYSTEM_TABLE *st,
    CHAR16 *name
)
{
    SIMPLE_TEXT_OUTPUT_INTERFACE *out =
        st->ConOut;

    if (char16_len(name) == 0) {

        print(
            out,
            "Usage: edit <name>\n"
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
    }


    print(
        out,
        "Editing '"
    );

    print16(
        out,
        name
    );

    print(
        out,
        "'. Type lines, finish with a line containing just '.'\n"
    );


    CHAR16 buf[FS_DATA_MAX];

    UINTN pos = 0;

    buf[0] = 0;


    CHAR16 ln[LINE_MAX];


    for (;;) {

        print(
            out,
            ": "
        );


        read_line(
            st,
            ln,
            LINE_MAX
        );


        if (ln[0] == L'.' &&
            ln[1] == 0)
            break;


        UINTN llen =
            char16_len(ln);


        if (pos > 0 &&
            pos + 1 < FS_DATA_MAX) {

            buf[pos++] =
                L'\n';
        }


        for (UINTN i = 0;
             i < llen &&
             pos < FS_DATA_MAX - 1;
             i++) {

            buf[pos++] =
                ln[i];
        }


        buf[pos] = 0;


        if (pos >= FS_DATA_MAX - 1) {

            print(
                out,
                "(file full, stopping edit)\n"
            );

            break;
        }
    }


    char16_copy(
        g_fs[idx].data,
        buf,
        FS_DATA_MAX
    );


    g_fs[idx].size =
        char16_len(
            g_fs[idx].data
        );


    print(
        out,
        "Saved ("
    );

    print_uint(
        out,
        g_fs[idx].size
    );

    print(
        out,
        " bytes).\n"
    );
}
