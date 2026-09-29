/*
 * ping - "ты тут?" другому компьютеру: эхо-запросы ICMP.
 *
 *   ping 10.0.2.2          4 запроса, по одному в секунду
 *   ping -c 10 example.com 10 запросов (имя - через DNS)
 *   ping -t 8.8.8.8        без конца (Ctrl+C - стоп)
 *
 * Каждый ответ: сколько байт, от кого, номер, TTL (сколько
 * "прыжков" пакету ещё оставалось) и время туда-обратно.
 */
#include "myos.h"

int main(int argc, char **argv)
{
    int count = 4;
    const char *host = NULL;

    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "-c") == 0 && i + 1 < argc)
            count = atoi(argv[++i]);
        else if (strcmp(argv[i], "-t") == 0)
            count = 0;
        else
            host = argv[i];
    }

    if (host == NULL) {
        printf("usage: ping [-c N | -t] <address or name>\n");
        return 1;
    }

    unsigned int ip;
    char ips[16];
    int r = resolve(host, &ip);

    if (r < 0) {
        printf("ping: %s: %s\n", host, strerror(r));
        return 1;
    }

    int s = socket(MYOS_SOCK_PING);

    if (s < 0) {
        printf("ping: no socket: %s\n", strerror(s));
        return 1;
    }

    printf("PING %s (%s): 56 data bytes\n", host, ip_to_str(ip, ips));

    int sent = 0, got = 0;
    unsigned long tmin = ~0ul, tmax = 0, tsum = 0;

    for (int seq = 1; count == 0 || seq <= count; seq++) {

        unsigned char pkt[64];

        memset(pkt, 0, sizeof(pkt));
        pkt[6] = (unsigned char)(seq >> 8);          /* номер - в заголовке ICMP */
        pkt[7] = (unsigned char)seq;
        for (int k = 8; k < 64; k++)
            pkt[k] = (unsigned char)k;

        unsigned long t0 = uptime_ms();

        r = (int)sendto(s, pkt, sizeof(pkt), ip, 0);

        if (r < 0) {
            printf("ping: cannot send: %s\n", strerror(r));
            return 1;
        }

        sent++;

        /* ждём ответ с этим номером не дольше секунды */
        int answered = 0;

        for (;;) {

            unsigned long el = uptime_ms() - t0;

            if (el >= 1000)
                break;

            sock_timeout(s, (unsigned)(1000 - el));

            unsigned char in[128];
            struct myos_sockaddr from;
            long n = recvfrom(s, in, sizeof(in), &from);

            if (n < 0)
                break;

            if (n >= 8 && in[0] == 0 && ((in[6] << 8) | in[7]) == seq) {
                unsigned long dt = uptime_ms() - t0;
                got++;
                answered = 1;
                tsum += dt;
                if (dt < tmin) tmin = dt;
                if (dt > tmax) tmax = dt;
                if (dt == 0)
                    printf("%ld bytes from %s: icmp_seq=%d ttl=%u time<1 ms\n",
                           n, ip_to_str(from.ip, ips), seq, from.ttl);
                else
                    printf("%ld bytes from %s: icmp_seq=%d ttl=%u time=%lu ms\n",
                           n, ip_to_str(from.ip, ips), seq, from.ttl, dt);
                break;
            }

            if (n >= 8 && (in[0] == 3 || in[0] == 11)) {
                printf("from %s: icmp_seq=%d %s\n", ip_to_str(from.ip, ips), seq,
                       in[0] == 3 ? "destination unreachable" : "time to live exceeded");
                answered = 1;
                break;
            }
        }

        if (!answered)
            printf("no answer for icmp_seq=%d\n", seq);

        /* до следующего запроса - секунда от начала этого */
        if (count == 0 || seq < count) {
            unsigned long el = uptime_ms() - t0;
            if (el < 1000)
                sleep_ms(1000 - el);
        }
    }

    close(s);

    printf("--- %s ping statistics ---\n", host);
    printf("%d packets transmitted, %d received, %d%% packet loss\n", sent, got,
           sent ? (sent - got) * 100 / sent : 0);

    if (got > 0)
        printf("round-trip min/avg/max = %lu/%lu/%lu ms\n", tmin, tsum / (unsigned long)got, tmax);

    return got > 0 ? 0 : 1;
}
