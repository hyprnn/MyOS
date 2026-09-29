/*
 * net/udp.c - UDP: "письма" без соединения и без гарантий доставки.
 * Часть MyOS; общие объявления - в net/net.h.
 *
 * Заголовок 8 байт: порт отправителя, порт получателя, длина,
 * контрольная сумма (по псевдозаголовку IP + данным). На UDP
 * работают DHCP (порты 67/68) и DNS (порт 53); программы получают
 * UDP через сокеты (net/socket.c).
 */
#include "net.h"

static UINT64 g_udp_in = 0, g_udp_bad = 0, g_udp_noport = 0;

static INTN udp_build_send(NETIF *nif, UINT32 src, UINT16 sport, UINT32 dst,
                           UINT16 dport, const UINT8 *data, UINTN len)
{
    static UINT8 seg[NET_MTU];

    if (len + UDP_HLEN + IP_HLEN > NET_MTU)
        return -1;

    if (src == 0 && nif == NULL)
        src = ip_src_for(dst);

    net_put16(seg + 0, sport);
    net_put16(seg + 2, dport);
    net_put16(seg + 4, (UINT16)(UDP_HLEN + len));
    net_put16(seg + 6, 0);
    memcpy(seg + UDP_HLEN, data, len);

    UINT16 cs = net_csum(seg, UDP_HLEN + len,
                         ip_pseudo_sum(src, dst, IP_PROTO_UDP, UDP_HLEN + len));

    /* 0 в поле суммы значит "суммы нет" - настоящий 0 пишется как FFFF */
    net_put16(seg + 6, cs ? cs : 0xFFFFu);

    if (nif != NULL)
        return ip_send_if(nif, src, dst, IP_PROTO_UDP, seg, UDP_HLEN + len);

    return ip_send(src, dst, IP_PROTO_UDP, seg, UDP_HLEN + len);
}

INTN udp_send(UINT32 src, UINT16 sport, UINT32 dst, UINT16 dport,
              const UINT8 *data, UINTN len)
{
    return udp_build_send(NULL, src, sport, dst, dport, data, len);
}

INTN udp_send_if(NETIF *nif, UINT32 src, UINT16 sport, UINT32 dst, UINT16 dport,
                 const UINT8 *data, UINTN len)
{
    return udp_build_send(nif, src, sport, dst, dport, data, len);
}

/* "Порт недоступен" в ответ на UDP в закрытый порт (так делают
   все ОС - traceroute и nslookup это понимают) */
static void udp_unreachable(UINT32 src, UINT32 dst, const UINT8 *ip_payload, UINTN len)
{
    UINT8 m[8 + IP_HLEN + 8];

    if (dst == 0xFFFFFFFFu || !ip_is_local(dst))
        return;

    memset(m, 0, sizeof(m));
    m[0] = 3;      /* Destination Unreachable */
    m[1] = 3;      /* Port Unreachable */

    /* по правилам - заголовок IP исходного пакета + 8 байт данных;
       заголовок восстанавливаем (его у нас уже нет) */
    UINT8 *h = m + 8;
    h[0] = 0x45;
    net_put16(h + 2, (UINT16)(IP_HLEN + len));
    h[8] = 64;
    h[9] = IP_PROTO_UDP;
    net_put32(h + 12, src);
    net_put32(h + 16, dst);
    net_put16(h + 10, net_csum(h, IP_HLEN, 0));
    memcpy(m + 8 + IP_HLEN, ip_payload, len < 8 ? len : 8);

    net_put16(m + 2, net_csum(m, sizeof(m), 0));
    ip_send(dst, src, IP_PROTO_ICMP, m, sizeof(m));
}

void udp_input(NETIF *nif, UINT32 src, UINT32 dst, const UINT8 *p, UINTN len)
{
    if (len < UDP_HLEN) {
        g_udp_bad++;
        return;
    }

    UINT16 sport = net_get16(p);
    UINT16 dport = net_get16(p + 2);
    UINTN ulen = net_get16(p + 4);

    if (ulen < UDP_HLEN || ulen > len) {
        g_udp_bad++;
        return;
    }

    if (net_get16(p + 6) != 0 &&
        net_csum(p, ulen, ip_pseudo_sum(src, dst, IP_PROTO_UDP, ulen)) != 0) {
        g_udp_bad++;
        return;
    }

    g_udp_in++;

    /* ответ сервера DHCP (с порта 67 на наш 68) */
    if (dport == 68 && sport == 67) {
        dhcp_input(nif, p + UDP_HLEN, ulen - UDP_HLEN);
        return;
    }

    if (!nif->up && !nif->loopback)
        return;       /* без адреса - только DHCP */

    if (sock_udp_input(src, sport, dst, dport, p + UDP_HLEN, ulen - UDP_HLEN))
        return;

    g_udp_noport++;
    udp_unreachable(src, dst, p, ulen);
}

void udp_print_stats(SIMPLE_TEXT_OUTPUT_INTERFACE *out)
{
    kprintf(out, "UDP: %llu received, %llu bad, %llu to closed ports\n",
            g_udp_in, g_udp_bad, g_udp_noport);
}
