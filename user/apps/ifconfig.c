/*
 * ifconfig - сетевые интерфейсы: адреса, MAC, связь, счётчики.
 *
 *   ifconfig                          все интерфейсы
 *   ifconfig eth0 dhcp                получить адрес заново от DHCP
 *   ifconfig eth0 192.168.1.50/24 [gw 192.168.1.1] [dns 1.1.1.1]
 *                                     адрес вручную
 *   ifconfig eth0 dns 8.8.8.8         только сервер DNS
 *   ifconfig eth0 down                забыть адрес
 */
#include "myos.h"

static void human(unsigned long long b, char *out)
{
    if (b >= 10ull * 1024 * 1024)
        snprintf(out, 24, "%llu MiB", b / (1024 * 1024));
    else if (b >= 10ull * 1024)
        snprintf(out, 24, "%llu KiB", b / 1024);
    else
        snprintf(out, 24, "%llu B", b);
}

static int prefix_len(unsigned int mask)
{
    int n = 0;
    while (mask & 0x80000000u) {
        n++;
        mask <<= 1;
    }
    return n;
}

static void show(struct myos_netif *n)
{
    char a[16], b[16], c[16], d[16], rx[24], tx[24];

    printf("%s: %s", n->name, n->model);
    if (n->name[0] != 'l')
        printf("  [%s]", n->driver);
    printf("\n");

    if (strcmp(n->driver, "loopback") != 0) {
        printf("    ether %02x:%02x:%02x:%02x:%02x:%02x  link %s", n->mac[0], n->mac[1],
               n->mac[2], n->mac[3], n->mac[4], n->mac[5], n->link ? "UP" : "DOWN");
        if (n->link && n->speed_mbps)
            printf(" %u Mbit/s", n->speed_mbps);
        printf("  (%s)\n", n->irq);
    }

    if (n->up) {
        printf("    inet %s/%d  gateway %s\n", ip_to_str(n->ip, a), prefix_len(n->mask),
               n->gw ? ip_to_str(n->gw, b) : "-");
        if (n->dns)
            printf("    dns %s%s%s\n", ip_to_str(n->dns, c), n->dns2 ? ", " : "",
                   n->dns2 ? ip_to_str(n->dns2, d) : "");
    } else {
        printf("    no address: %s\n", n->state);
    }

    if (n->cfg == MYOS_NETCFG_DHCP && n->up)
        printf("    address from DHCP, lease %u min left\n", n->lease_left_s / 60u);
    else if (n->cfg == MYOS_NETCFG_STATIC && n->name[0] != 'l')
        printf("    address set by hand\n");

    human(n->rx_bytes, rx);
    human(n->tx_bytes, tx);
    printf("    RX %llu packets (%s)", n->rx_packets, rx);
    if (n->rx_dropped)
        printf(", %llu dropped", n->rx_dropped);
    printf("   TX %llu packets (%s)", n->tx_packets, tx);
    if (n->tx_errors)
        printf(", %llu errors", n->tx_errors);
    printf("\n");
}

/* "192.168.1.50/24" -> адрес и маска */
static int parse_cidr(const char *s, unsigned int *ip, unsigned int *mask)
{
    char buf[32];
    int k = 0;

    while (s[k] && s[k] != '/' && k < 31) {
        buf[k] = s[k];
        k++;
    }
    buf[k] = '\0';

    if (!str_to_ip(buf, ip))
        return 0;

    int bits = 24;

    if (s[k] == '/')
        bits = atoi(s + k + 1);

    if (bits < 1 || bits > 32)
        return 0;

    *mask = (bits == 32) ? 0xFFFFFFFFu : ~(0xFFFFFFFFu >> bits);
    return 1;
}

int main(int argc, char **argv)
{
    if (argc <= 1) {
        struct myos_netif n;
        int any = 0;

        for (int i = 0; netinfo(i, &n) == 1; i++) {
            show(&n);
            any = 1;
        }

        if (!any)
            printf("no network interfaces\n");

        return 0;
    }

    struct myos_netctl c;

    memset(&c, 0, sizeof(c));
    strncpy(c.name, argv[1], sizeof(c.name) - 1);

    if (argc == 2) {
        struct myos_netif n;
        for (int i = 0; netinfo(i, &n) == 1; i++)
            if (strcmp(n.name, argv[1]) == 0) {
                show(&n);
                return 0;
            }
        printf("ifconfig: no interface '%s'\n", argv[1]);
        return 1;
    }

    if (strcmp(argv[2], "dhcp") == 0) {
        c.cmd = MYOS_NETCTL_DHCP;
    } else if (strcmp(argv[2], "down") == 0) {
        c.cmd = MYOS_NETCTL_DOWN;
    } else if (strcmp(argv[2], "dns") == 0 && argc > 3) {
        c.cmd = MYOS_NETCTL_DNS;
        if (!str_to_ip(argv[3], &c.dns)) {
            printf("ifconfig: bad address %s\n", argv[3]);
            return 1;
        }
    } else if (parse_cidr(argv[2], &c.ip, &c.mask)) {
        c.cmd = MYOS_NETCTL_STATIC;
        for (int i = 3; i + 1 < argc; i += 2) {
            unsigned int *dst = strcmp(argv[i], "gw") == 0 ? &c.gw :
                                strcmp(argv[i], "dns") == 0 ? &c.dns : NULL;
            if (dst == NULL || !str_to_ip(argv[i + 1], dst)) {
                printf("ifconfig: do not understand '%s %s'\n", argv[i], argv[i + 1]);
                return 1;
            }
        }
    } else {
        printf("usage: ifconfig [name [dhcp | down | dns IP | IP/bits [gw IP] [dns IP]]]\n");
        return 1;
    }

    int r = netctl(&c);

    if (r < 0) {
        printf("ifconfig: %s: %s\n", argv[1], r == MYOS_ENOENT ? "no such interface" : strerror(r));
        return 1;
    }

    if (c.cmd == MYOS_NETCTL_DHCP) {
        /* подождать адрес - до 10 секунд */
        printf("%s: asking DHCP for an address", argv[1]);
        for (int t = 0; t < 40; t++) {
            struct myos_netif n;
            for (int i = 0; netinfo(i, &n) == 1; i++)
                if (strcmp(n.name, argv[1]) == 0 && n.up) {
                    char a[16];
                    printf(" - got %s\n", ip_to_str(n.ip, a));
                    return 0;
                }
            sleep_ms(250);
            if (t % 4 == 3)
                printf(".");
        }
        printf(" - no answer yet (it keeps trying in the background)\n");
        return 0;
    }

    printf("%s: done\n", argv[1]);
    return 0;
}
