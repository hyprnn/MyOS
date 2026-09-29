/*
 * net/netcmd.c - сеть для шелла и программ: команда ядра "net"
 * (всё о стеке: интерфейсы, ARP, TCP, сокеты, DNS), сведения об
 * интерфейсах для программы ifconfig (SYS_NETINFO) и настройка
 * адреса (SYS_NETCTL). Часть MyOS; общие объявления - в net/net.h.
 */
#include "net.h"

void ip_print_stats(SIMPLE_TEXT_OUTPUT_INTERFACE *out);
void udp_print_stats(SIMPLE_TEXT_OUTPUT_INTERFACE *out);
UINT64 net_rxq_overflows(void);

static void copy_str(char *dst, UINTN cap, const char *src)
{
    UINTN i = 0;

    for (; src && src[i] && i + 1 < cap; i++)
        dst[i] = src[i];

    dst[i] = '\0';
}

/* Сведения об интерфейсе idx (по порядку занятых) для программы */
INTN net_sys_info(UINTN idx, struct myos_netif *o)
{
    kmutex_lock(&g_net_mutex);

    NETIF *f = NULL;
    UINTN k = 0;

    for (UINTN i = 0; i < NET_MAX_IF; i++) {
        if (!g_netifs[i].used)
            continue;
        if (k++ == idx) {
            f = &g_netifs[i];
            break;
        }
    }

    if (f == NULL) {
        kmutex_unlock(&g_net_mutex);
        return 0;
    }

    memset(o, 0, sizeof(*o));
    copy_str(o->name, sizeof(o->name), f->name);
    copy_str(o->driver, sizeof(o->driver), f->driver);
    copy_str(o->model, sizeof(o->model), f->model);
    copy_str(o->irq, sizeof(o->irq), f->irq_mode);

    if (f->loopback)
        copy_str(o->state, sizeof(o->state), "loopback");
    else if (!f->link)
        copy_str(o->state, sizeof(o->state), "no link (cable unplugged?)");
    else if (f->cfg == NET_CFG_STATIC)
        copy_str(o->state, sizeof(o->state), "address set by hand");
    else if (f->cfg == NET_CFG_DHCP)
        copy_str(o->state, sizeof(o->state), dhcp_state_name(f->dhcp.state));
    else
        copy_str(o->state, sizeof(o->state), "no address");

    memcpy(o->mac, f->mac, 6);
    o->link = f->link ? 1 : 0;
    o->up = f->up ? 1 : 0;
    o->cfg = f->cfg;
    o->ip = f->up ? f->ip : 0;
    o->mask = f->up ? f->mask : 0;
    o->gw = f->up ? f->gw : 0;
    o->dns = f->dns;
    o->dns2 = f->dns2;
    o->speed_mbps = f->speed_mbps;

    if (f->cfg == NET_CFG_DHCP && f->up && f->dhcp.lease_s) {
        UINT64 end = f->dhcp.lease_start_ms + (UINT64)f->dhcp.lease_s * 1000u;
        UINT64 now = net_now_ms();
        o->lease_left_s = (end > now) ? (UINT32)((end - now) / 1000u) : 0;
    }

    o->rx_packets = f->rx_packets;
    o->rx_bytes = f->rx_bytes;
    o->rx_dropped = f->rx_dropped;
    o->tx_packets = f->tx_packets;
    o->tx_bytes = f->tx_bytes;
    o->tx_errors = f->tx_errors;

    kmutex_unlock(&g_net_mutex);

    return 1;
}

/* Настроить адрес интерфейса (ifconfig eth0 dhcp / 10.0.2.99 ...) */
INTN net_sys_ctl(const struct myos_netctl *c)
{
    char name[8];

    copy_str(name, sizeof(name), c->name);

    kmutex_lock(&g_net_mutex);

    NETIF *f = net_if_by_name(name);
    INTN r = 0;

    if (f == NULL) {
        r = MYOS_ENOENT;
    } else if (f->loopback) {
        r = MYOS_EINVAL;
    } else if (c->cmd == MYOS_NETCTL_DHCP) {
        f->up = FALSE;
        arp_forget_if(f);
        f->cfg = NET_CFG_DHCP;
        f->dns = f->dns2 = 0;
        dhcp_stop(f);
        if (f->link)
            dhcp_start(f);
    } else if (c->cmd == MYOS_NETCTL_STATIC) {
        if (c->ip == 0 || c->mask == 0) {
            r = MYOS_EINVAL;
        } else {
            dhcp_stop(f);
            arp_forget_if(f);
            f->cfg = NET_CFG_STATIC;
            f->ip = c->ip;
            f->mask = c->mask;
            f->gw = c->gw;
            if (c->dns)
                f->dns = c->dns;
            f->dns2 = 0;
            f->up = TRUE;
            arp_announce(f);
        }
    } else if (c->cmd == MYOS_NETCTL_DNS) {
        f->dns = c->dns;
        f->dns2 = 0;
    } else if (c->cmd == MYOS_NETCTL_DOWN) {
        dhcp_stop(f);
        arp_forget_if(f);
        f->cfg = NET_CFG_NONE;
        f->up = FALSE;
    } else {
        r = MYOS_EINVAL;
    }

    kmutex_unlock(&g_net_mutex);

    net_kick();

    return r;
}

/* Одна строка о сети - для панели задач GUI и fetch:
   "eth0 10.0.2.15" / "no network". FALSE - сетевых карт нет */
BOOLEAN net_status_line(char *buf, UINTN cap)
{
    for (UINTN i = 0; i < NET_MAX_IF; i++) {

        NETIF *f = &g_netifs[i];

        if (!f->used || f->loopback)
            continue;

        if (f->up) {
            char ip[16];
            net_fmt_ip(ip, sizeof(ip), f->ip);
            ksnprintf(buf, cap, "%s %s", f->name, ip);
            return TRUE;
        }
    }

    for (UINTN i = 0; i < NET_MAX_IF; i++) {
        NETIF *f = &g_netifs[i];
        if (f->used && !f->loopback) {
            ksnprintf(buf, cap, "%s: %s", f->name, f->link ? "no address yet" : "no link");
            return TRUE;
        }
    }

    ksnprintf(buf, cap, "no network");
    return FALSE;
}

static void print_if(SIMPLE_TEXT_OUTPUT_INTERFACE *out, NETIF *f)
{
    char ip[16], mask[16], gw[16], dns[16];

    net_fmt_ip(ip, sizeof(ip), f->ip);
    net_fmt_ip(mask, sizeof(mask), f->mask);
    net_fmt_ip(gw, sizeof(gw), f->gw);
    net_fmt_ip(dns, sizeof(dns), f->dns);

    kprintf(out, "%s: %s [%s]\n", f->name, f->model, f->driver);

    if (!f->loopback)
        kprintf(out, "    MAC %02x:%02x:%02x:%02x:%02x:%02x, link %s", f->mac[0], f->mac[1],
                f->mac[2], f->mac[3], f->mac[4], f->mac[5], f->link ? "UP" : "DOWN");

    if (!f->loopback && f->link && f->speed_mbps)
        kprintf(out, " %u Mbit/s", f->speed_mbps);

    if (!f->loopback)
        kprintf(out, ", interrupts: %s\n", f->irq_mode);

    if (f->up)
        kprintf(out, "    inet %s  mask %s  gateway %s  DNS %s\n", ip, mask, gw, dns);
    else
        print(out, "    no IP address\n");

    if (f->cfg == NET_CFG_DHCP)
        kprintf(out, "    DHCP: %s\n", dhcp_state_name(f->dhcp.state));

    kprintf(out, "    RX %llu packets (%llu bytes, %llu dropped), TX %llu packets (%llu bytes, %llu errors)\n",
            f->rx_packets, f->rx_bytes, f->rx_dropped, f->tx_packets, f->tx_bytes, f->tx_errors);
}

/*
 * Команда ядра "net": что происходит в сети.
 *   net        - интерфейсы, ARP, соединения TCP, сокеты, DNS
 */
void kernel_cmd_net(SIMPLE_TEXT_OUTPUT_INTERFACE *out, const char *arg)
{
    (void)arg;

    if (!net_running()) {
        print(out, "The network is not running (no threads?).\n");
        return;
    }

    kmutex_lock(&g_net_mutex);

    for (UINTN i = 0; i < NET_MAX_IF; i++)
        if (g_netifs[i].used)
            print_if(out, &g_netifs[i]);

    print(out, "\n");
    arp_print(out);
    print(out, "\n");
    tcp_print(out);
    sock_print(out);
    ip_print_stats(out);
    udp_print_stats(out);

    if (net_rxq_overflows())
        kprintf(out, "Receive queue overflows: %llu\n", net_rxq_overflows());

    dns_print_cache(out);

    kmutex_unlock(&g_net_mutex);

    print(out, "\nPrograms: ifconfig, ping, nslookup, wget, nc, httpd (see 'help').\n");
}
