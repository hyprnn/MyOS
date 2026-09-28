/*
 * lib/libc.c - memcpy/memmove/memset/memcmp для компилятора.
 * Часть MyOS; общие объявления - в myos.h.
 */
#include "myos.h"

__attribute__((used, visibility("hidden")))
void *memcpy(void *dst, const void *src, myos_size_t n)
{
    unsigned char *d = (unsigned char *)dst;
    const unsigned char *s = (const unsigned char *)src;

    for (myos_size_t i = 0; i < n; i++)
        d[i] = s[i];

    return dst;
}

__attribute__((used, visibility("hidden")))
void *memmove(void *dst, const void *src, myos_size_t n)
{
    unsigned char *d = (unsigned char *)dst;
    const unsigned char *s = (const unsigned char *)src;

    if (d < s) {
        for (myos_size_t i = 0; i < n; i++)
            d[i] = s[i];
    } else {
        for (myos_size_t i = n; i > 0; i--)
            d[i - 1] = s[i - 1];
    }

    return dst;
}

__attribute__((used, visibility("hidden")))
void *memset(void *dst, int value, myos_size_t n)
{
    unsigned char *d = (unsigned char *)dst;

    for (myos_size_t i = 0; i < n; i++)
        d[i] = (unsigned char)value;

    return dst;
}

__attribute__((used, visibility("hidden")))
int memcmp(const void *a, const void *b, myos_size_t n)
{
    const unsigned char *x = (const unsigned char *)a;
    const unsigned char *y = (const unsigned char *)b;

    for (myos_size_t i = 0; i < n; i++) {
        if (x[i] != y[i])
            return (int)x[i] - (int)y[i];
    }

    return 0;
}

/* Текущий цвет текста */
UINTN g_color = 0x0F;
