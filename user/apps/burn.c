/*
 * burn - просто считает: N миллионов шагов простого генератора
 * случайных чисел (по умолчанию 40). Ни памяти, ни системных вызовов
 * по дороге - чистая работа процессора. Нужна команде smptest: если
 * несколько таких программ идут на разных ядрах процессора, вместе
 * они заканчиваются почти так же быстро, как одна.
 */
#include "myos.h"

int main(int argc, char **argv)
{
    unsigned long millions = (argc > 1) ? (unsigned long)atol(argv[1]) : 40ul;
    unsigned long x = 88172645463325252ul;     /* xorshift64 */
    unsigned long sum = 0;
    unsigned long t0 = uptime_ms();

    for (unsigned long i = 0; i < millions * 1000000ul; i++) {
        x ^= x << 13;
        x ^= x >> 7;
        x ^= x << 17;
        sum += x >> 60;
    }

    printf("burn: %lu million steps in %lu ms (checksum %lu)\n",
           millions, uptime_ms() - t0, sum);
    return 0;
}
