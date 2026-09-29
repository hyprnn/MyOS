/*
 * nslookup - узнать IP-адрес по имени (спросить DNS).
 *
 *   nslookup example.com
 */
#include "myos.h"

int main(int argc, char **argv)
{
    if (argc < 2) {
        printf("usage: nslookup <name>\n");
        return 1;
    }

    /* какой сервер спрашиваем - первый DNS из настроек интерфейсов */
    struct myos_netif n;
    char a[16];

    for (int i = 0; netinfo(i, &n) == 1; i++)
        if (n.up && n.dns) {
            printf("Server:  %s (%s)\n", ip_to_str(n.dns, a), n.name);
            break;
        }

    for (int i = 1; i < argc; i++) {

        unsigned int ip;
        unsigned long t0 = uptime_ms();
        int r = resolve(argv[i], &ip);

        if (r < 0) {
            printf("%s: %s\n", argv[i], strerror(r));
            continue;
        }

        printf("Name:    %s\nAddress: %s  (%lu ms)\n", argv[i], ip_to_str(ip, a), uptime_ms() - t0);
    }

    return 0;
}
