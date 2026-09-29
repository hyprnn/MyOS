/*
 * kernel/random.c - случайные числа для криптографии (ключи TLS).
 * Часть MyOS; общие объявления - в myos.h.
 *
 * Ключам шифрования нужна непредсказуемость. Источники:
 *   * RDRAND - аппаратный генератор процессора (Intel с 2012 года,
 *     AMD с 2015-го), если он есть (CPUID.1:ECX бит 30);
 *   * "шум" времени: счётчик тактов TSC в моменты прерываний -
 *     приход сетевых кадров, нажатия клавиш (krandom_stir), и в
 *     момент запроса.
 * Всё смешивается в "пул" (20 байт) через SHA-1; каждый выданный блок
 * = SHA-1(пул || счётчик || TSC || RDRAND), и пул тут же обновляется -
 * по выданным числам нельзя восстановить ни пул, ни прошлые числа.
 */
#include "myos.h"
#include "../net/wifi.h"

static UINT8  g_pool[20];
static volatile UINT64 g_stir[4];
static UINT64 g_counter = 0;
static INTN   g_rdrand = -1;           /* -1 - ещё не знаем */

static BOOLEAN have_rdrand(void)
{
    if (g_rdrand < 0) {
        UINT32 a = 1, b, c, d;
        __asm__ __volatile__("cpuid" : "+a"(a), "=b"(b), "=c"(c), "=d"(d) : "c"(0));
        g_rdrand = (c >> 30) & 1u;
    }

    return g_rdrand == 1;
}

static BOOLEAN rdrand64(UINT64 *v)
{
    for (UINTN i = 0; i < 10; i++) {
        UINT8 ok;
        __asm__ __volatile__("rdrand %0; setc %1" : "=r"(*v), "=qm"(ok));
        if (ok)
            return TRUE;
    }

    return FALSE;
}

/* Подмешать событие (из прерываний: время прихода кадра, клавиши) */
void krandom_stir(UINT64 v)
{
    UINT64 t = rdtsc();
    UINTN i = (UINTN)(t & 3u);

    g_stir[i] = (g_stir[i] << 7 | g_stir[i] >> 57) ^ v ^ t;
}

void krandom_fill(void *buf, UINTN n)
{
    UINT8 *out = (UINT8 *)buf;
    UINT64 fl = kx_irq_save();

    while (n > 0) {

        UINT64 extra[8];
        UINT8 h[20];
        SHA1_CTX c;

        extra[0] = ++g_counter;
        extra[1] = rdtsc();
        extra[2] = g_kticks;
        extra[3] = g_stir[0] ^ g_stir[1];
        extra[4] = g_stir[2] ^ g_stir[3];
        extra[5] = extra[6] = 0;
        if (have_rdrand()) {
            rdrand64(&extra[5]);
            rdrand64(&extra[6]);
        }
        extra[7] = rdtsc();

        /* блок на выдачу */
        sha1_init(&c);
        sha1_update(&c, g_pool, sizeof(g_pool));
        sha1_update(&c, "out", 3);
        sha1_update(&c, extra, sizeof(extra));
        sha1_final(&c, h);

        /* новый пул - другой хеш тех же данных */
        sha1_init(&c);
        sha1_update(&c, g_pool, sizeof(g_pool));
        sha1_update(&c, "pool", 4);
        sha1_update(&c, extra, sizeof(extra));
        sha1_final(&c, g_pool);

        UINTN k = (n < 16) ? n : 16;       /* из 20 байт отдаём 16 */

        memcpy(out, h, k);
        out += k;
        n -= k;
    }

    kx_irq_restore(fl);
}

BOOLEAN krandom_hw(void)
{
    return have_rdrand();
}
