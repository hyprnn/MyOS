/*
 * user/lib/malloc.c - выделение памяти для программ.
 *
 * Память берётся у ядра кусками через sbrk (сдвинуть конец "кучи").
 * Внутри - простейший список блоков: у каждого заголовок с размером
 * и отметкой "свободен". free помечает блок свободным и склеивает с
 * соседом справа, malloc ищет первый подходящий свободный.
 */
#include "myos.h"

typedef struct BLK {
    size_t size;            /* байт полезной части */
    int    free;
    struct BLK *next;
} BLK;

static BLK *g_first = NULL;
static BLK *g_last = NULL;

#define ALIGN16(n) (((n) + 15u) & ~(size_t)15u)

void *malloc(size_t n)
{
    if (n == 0)
        return NULL;

    n = ALIGN16(n);

    for (BLK *b = g_first; b; b = b->next) {

        if (!b->free || b->size < n)
            continue;

        /* большой - отрезать хвост отдельным блоком */
        if (b->size >= n + sizeof(BLK) + 32) {
            BLK *t = (BLK *)((char *)(b + 1) + n);
            t->size = b->size - n - sizeof(BLK);
            t->free = 1;
            t->next = b->next;
            b->next = t;
            b->size = n;
            if (g_last == b) g_last = t;
        }

        b->free = 0;
        return b + 1;
    }

    size_t want = sizeof(BLK) + n;
    size_t chunk = (want < 65536) ? 65536 : ALIGN16(want);
    BLK *b = (BLK *)sbrk((long)chunk);

    if (b == (void *)-1)
        return NULL;

    b->size = chunk - sizeof(BLK);
    b->free = 1;
    b->next = NULL;

    if (g_last) g_last->next = b; else g_first = b;
    g_last = b;

    return malloc(n);
}

void free(void *p)
{
    if (!p)
        return;

    BLK *b = (BLK *)p - 1;
    b->free = 1;

    /* склеить подряд идущие свободные */
    for (BLK *x = g_first; x; x = x->next) {
        while (x->free && x->next && x->next->free &&
               (char *)(x + 1) + x->size == (char *)x->next) {
            if (g_last == x->next) g_last = x;
            x->size += sizeof(BLK) + x->next->size;
            x->next = x->next->next;
        }
    }
}

void *calloc(size_t n, size_t sz)
{
    void *p = malloc(n * sz);
    if (p) memset(p, 0, n * sz);
    return p;
}

void *realloc(void *p, size_t n)
{
    if (!p) return malloc(n);
    BLK *b = (BLK *)p - 1;
    if (b->size >= n) return p;
    void *q = malloc(n);
    if (q) { memcpy(q, p, b->size); free(p); }
    return q;
}
