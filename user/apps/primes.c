/*
 * primes - считает простые числа до N (по умолчанию 3 000 000)
 * решетом Эратосфена. Долго и много памяти - удобно смотреть, как
 * программа работает, пока ОС живёт своей жизнью (ps в другом окне,
 * Ctrl+C - остановить).
 */
#include "myos.h"

int main(int argc, char **argv)
{
    unsigned long n = (argc > 1) ? (unsigned long)atol(argv[1]) : 3000000ul;

    if (n < 2 || n > 200000000ul) {
        printf("Usage: primes [N], 2 <= N <= 200000000\n");
        return 1;
    }

    unsigned char *sieve = malloc(n + 1);

    if (!sieve) {
        printf("not enough memory for %lu numbers\n", n);
        return 1;
    }

    unsigned long t0 = uptime_ms();

    memset(sieve, 1, n + 1);
    sieve[0] = sieve[1] = 0;

    for (unsigned long i = 2; i * i <= n; i++)
        if (sieve[i])
            for (unsigned long j = i * i; j <= n; j += i)
                sieve[j] = 0;

    unsigned long count = 0, last = 0;

    for (unsigned long i = 2; i <= n; i++)
        if (sieve[i]) {
            count++;
            last = i;
        }

    printf("%lu primes up to %lu (the biggest: %lu), %lu ms\n",
           count, n, last, uptime_ms() - t0);

    free(sieve);
    return 0;
}
