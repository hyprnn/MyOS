/*
 * user/doom/w_file_myos.c - файл игры (WAD) целиком в памяти (этап 10).
 *
 * Замена w_file_stdc.c из doomgeneric (его Makefile не собирает): тот
 * на каждый кусок WAD делал fseek+fread, а при загрузке уровня кусков -
 * тысячи; с флешки каждый такой запрос - поход к диску, и заставка
 * висела 13 секунд. Здесь WAD читается один раз большими кусками, и
 * DOOM берёт данные прямо из памяти (поле mapped: без копирования).
 * doom1.wad - 4 МиБ, doom2.wad - 14 МиБ: памяти хватает с запасом.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "m_misc.h"
#include "w_file.h"
#include "z_zone.h"

extern wad_file_class_t stdc_wad_file;

static wad_file_t *W_Mem_OpenFile(char *path)
{
    FILE *f = fopen(path, "rb");

    if (f == NULL)
        return NULL;

    long len = M_FileLength(f);
    unsigned char *data = (len > 0) ? malloc((size_t)len) : NULL;

    if (data == NULL) {
        fclose(f);
        return NULL;
    }

    /* большими кусками: по 1 МиБ */
    long got = 0;

    while (got < len) {
        size_t want = (size_t)(len - got) > (1u << 20) ? (1u << 20) : (size_t)(len - got);
        size_t r = fread(data + got, 1, want, f);
        if (r == 0)
            break;
        got += (long)r;
    }

    fclose(f);

    if (got != len) {
        free(data);
        return NULL;
    }

    wad_file_t *w = Z_Malloc(sizeof(wad_file_t), PU_STATIC, 0);
    w->file_class = &stdc_wad_file;
    w->mapped = data;
    w->length = (unsigned int)len;
    return w;
}

static void W_Mem_CloseFile(wad_file_t *w)
{
    free(w->mapped);
    Z_Free(w);
}

static size_t W_Mem_Read(wad_file_t *w, unsigned int offset, void *buf, size_t n)
{
    if (offset >= w->length)
        return 0;

    if (n > w->length - offset)
        n = w->length - offset;

    memcpy(buf, w->mapped + offset, n);
    return n;
}

wad_file_class_t stdc_wad_file = {
    W_Mem_OpenFile,
    W_Mem_CloseFile,
    W_Mem_Read,
};
