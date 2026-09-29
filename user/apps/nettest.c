/*
 * nettest - проверка сетевого стека без сети: TCP и UDP через петлю
 * 127.0.0.1. Программа сама себе сервер и клиент: слушает порт,
 * соединяется с ним, гоняет данные с известным узором и сверяет
 * каждый байт.
 *
 *   nettest            1 МиБ
 *   nettest 4096       4096 КиБ
 */
#include "myos.h"

#define CHUNK 8192

static unsigned char pattern(unsigned long i)
{
    return (unsigned char)((i * 7u + (i >> 9)) & 255u);
}

int main(int argc, char **argv)
{
    unsigned long total = (argc > 1 ? (unsigned long)atol(argv[1]) : 1024ul) * 1024ul;
    unsigned int lo = 0x7F000001u;
    int port = 5555;

    /* --- UDP: сообщение туда и обратно --- */
    int u1 = socket(MYOS_SOCK_DGRAM), u2 = socket(MYOS_SOCK_DGRAM);

    if (u1 < 0 || u2 < 0 || bind(u1, 0, 5556) < 0) {
        printf("nettest: UDP sockets: FAILED\n");
        return 1;
    }

    sendto(u2, "hello udp", 9, lo, 5556);
    sock_timeout(u1, 2000);

    char ub[32];
    struct myos_sockaddr from;
    long un = recvfrom(u1, ub, sizeof(ub), &from);

    if (un != 9 || memcmp(ub, "hello udp", 9) != 0) {
        printf("nettest: UDP loopback: FAILED (%ld)\n", un);
        return 1;
    }

    printf("nettest: UDP loopback OK (from port %u)\n", from.port);
    close(u1);
    close(u2);

    /* --- TCP --- */
    int ls = socket(MYOS_SOCK_STREAM);

    if (ls < 0 || bind(ls, lo, port) < 0 || listen(ls, 2) < 0) {
        printf("nettest: TCP listen: FAILED\n");
        return 1;
    }

    int c = socket(MYOS_SOCK_STREAM);
    sock_timeout(c, 5000);

    int r = connect(c, lo, port);

    if (r < 0) {
        printf("nettest: TCP connect: FAILED (%s)\n", strerror(r));
        return 1;
    }

    sock_timeout(ls, 5000);
    int s = accept(ls, NULL);

    if (s < 0) {
        printf("nettest: TCP accept: FAILED (%s)\n", strerror(s));
        return 1;
    }

    sock_timeout(s, 5000);

    unsigned char *out = malloc(CHUNK), *in = malloc(CHUNK);
    unsigned long sent = 0, got = 0, bad = 0;
    unsigned long t0 = uptime_ms();

    /* кусками: отправить кусок, принять его целиком (одна программа -
       и клиент, и сервер; буфер отправки ядра 32 КиБ) */
    while (sent < total) {

        unsigned long n = total - sent < CHUNK ? total - sent : CHUNK;

        for (unsigned long i = 0; i < n; i++)
            out[i] = pattern(sent + i);

        if (send_all(c, out, n) != (long)n) {
            printf("nettest: TCP send: FAILED\n");
            return 1;
        }

        sent += n;

        while (got < sent) {
            long k = recv(s, in, CHUNK);
            if (k <= 0) {
                printf("nettest: TCP recv: FAILED (%s)\n", k < 0 ? strerror((int)k) : "closed");
                return 1;
            }
            for (long i = 0; i < k; i++)
                if (in[i] != pattern(got + (unsigned long)i))
                    bad++;
            got += (unsigned long)k;
        }
    }

    /* закрытие: клиент закрыл - сервер читает 0 (конец потока) */
    close(c);

    long eof = recv(s, in, CHUNK);
    unsigned long ms = uptime_ms() - t0;

    close(s);
    close(ls);

    if (ms == 0)
        ms = 1;

    printf("nettest: TCP loopback %lu KiB in %lu ms (%lu KiB/s), %lu bad bytes, end of stream %s\n",
           got / 1024, ms, got / 1024 * 1000 / ms, bad, eof == 0 ? "OK" : "MISSING");

    if (bad == 0 && got == total && eof == 0) {
        printf("nettest: OK\n");
        return 0;
    }

    printf("nettest: FAILED\n");
    return 1;
}
