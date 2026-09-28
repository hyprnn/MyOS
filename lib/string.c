/*
 * lib/string.c - строки CHAR16/char, разбор чисел и слов.
 * Часть MyOS; общие объявления - в myos.h.
 */
#include "myos.h"


/* ============================================================
 * Basic string helpers
 * ============================================================ */

UINTN char16_len(const CHAR16 *s)
{
    UINTN n = 0;

    while (s[n])
        n++;

    return n;
}


void char16_copy(
    CHAR16 *dst,
    const CHAR16 *src,
    UINTN max
)
{
    UINTN i = 0;

    if (max == 0)
        return;

    while (src[i] && i < max - 1) {
        dst[i] = src[i];
        i++;
    }

    dst[i] = 0;
}


int char16_eq(
    const CHAR16 *a,
    const CHAR16 *b
)
{
    while (*a && *b) {

        if (*a != *b)
            return 0;

        a++;
        b++;
    }

    return *a == 0 && *b == 0;
}


/* ============================================================
 * Parsing helpers
 * ============================================================ */

int streq(
    const CHAR16 *a,
    const char *b
)
{
    while (*a && *b) {

        if (*a !=
            (CHAR16)(unsigned char)*b)
            return 0;

        a++;
        b++;
    }

    return *a == 0 && *b == 0;
}


int starts_with(
    const CHAR16 *a,
    const char *prefix
)
{
    while (*prefix) {

        if (*a !=
            (CHAR16)(unsigned char)*prefix)
            return 0;

        a++;
        prefix++;
    }

    return 1;
}


UINTN parse_uint(
    const CHAR16 *s
)
{
    while (*s == L' ')
        s++;

    UINTN v = 0;

    while (*s >= L'0' &&
           *s <= L'9') {

        v =
            v * 10 +
            (UINTN)(*s - L'0');

        s++;
    }

    return v;
}


CHAR16 *skip_ws16(
    CHAR16 *s
)
{
    while (*s == L' ')
        s++;

    return s;
}


CHAR16 *take_word(
    CHAR16 *s,
    CHAR16 *out,
    UINTN max
)
{
    UINTN i = 0;

    if (max == 0)
        return s;

    while (*s &&
           *s != L' ' &&
           i < max - 1) {

        out[i++] = *s++;
    }

    out[i] = 0;

    return s;
}
