/*
 * net/ip.c - IPv4 и ICMP. Часть MyOS; общие объявления - в net/net.h.
 *
 * IP-пакет = заголовок 20 байт (версия, длина, TTL, протокол,
 * контрольная сумма, адреса отправителя и получателя) + данные
 * протокола выше (ICMP, UDP, TCP).
 *
 * Маршрут (ip_route): получатель в нашей подсети (адрес & маска
 * совпадает) - шлём ему напрямую; иначе - шлюзу (роутеру), он
 * передаст дальше, в Интернет. 127.x.x.x - на петлю lo.
 *
 * Фрагменты (большие пакеты, разрезанные по дороге) не собираются:
 * TCP договаривается о размере сегмента (MSS), а мы всегда ставим
 * "не фрагментировать" (DF) - на практике их почти не бывает.
 *
 * ICMP: отвечаем на ping (Echo Request -> Echo Reply), ответы на
 * наш ping отдаём сокету ping (net/socket.c).
 */
#include "net.h"

static UINT16 g_ip_id = 1;
static UINT64 g_ip_bad = 0, g_ip_frags = 0, g_ip_notme = 0;

/* Наш ли это адрес (любого интерфейса) */
BOOLEAN ip_is_local(UINT32 ip)
{
    if ((ip >> 24) == 127)
        return TRUE;

    for (UINTN i = 0; i < NET_MAX_IF; i++)
        if (g_netifs[i].used && g_netifs[i].up && g_netifs[i].ip == ip)
            return TRUE;

    return FALSE;
}

/*
 * Куда слать пакет для dst: интерфейс и следующий "прыжок" (сам
 * получатель или шлюз). NULL - маршрута нет (нет адреса / шлюза).
 */
NETIF *ip_route(UINT32 dst, UINT32 *next_hop)
{
    NETIF *lo = NULL;

    for (UINTN i = 0; i < NET_MAX_IF; i++)
        if (g_netifs[i].used && g_netifs[i].loopback)
            lo = &g_netifs[i];

    /* себе самому - через петлю */
    if (ip_is_local(dst) && lo != NULL) {
        *next_hop = dst;
        return lo;
    }

    /* своя подсеть */
    for (UINTN i = 0; i < NET_MAX_IF; i++) {

        NETIF *n = &g_netifs[i];

        if (!n->used || !n->up || n->loopback || !n->link)
            continue;

        if ((dst & n->mask) == (n->ip & n->mask) || dst == 0xFFFFFFFFu) {
            *next_hop = dst;
            return n;
        }
    }

    /* шлюз: первый интерфейс, у которого он есть */
    for (UINTN i = 0; i < NET_MAX_IF; i++) {

        NETIF *n = &g_netifs[i];

        if (n->used && n->up && !n->loopback && n->link && n->gw != 0) {
            *next_hop = n->gw;
            return n;
        }
    }

    return NULL;
}

/* С какого нашего адреса отправлять пакет для dst */
UINT32 ip_src_for(UINT32 dst)
{
    UINT32 hop;
    NETIF *n = ip_route(dst, &hop);

    if (n == NULL)
        return 0;

    if (n->loopback)
        return ip_is_local(dst) && (dst >> 24) != 127 ? dst : NET_IP(127, 0, 0, 1);

    return n->ip;
}

/* Сумма "псевдозаголовка" для UDP/TCP: адреса, протокол, длина */
UINT32 ip_pseudo_sum(UINT32 src, UINT32 dst, UINT8 proto, UINTN len)
{
    UINT8 ph[12];

    net_put32(ph, src);
    net_put32(ph + 4, dst);
    ph[8] = 0;
    ph[9] = proto;
    net_put16(ph + 10, (UINT16)len);

    return net_csum_add(ph, 12, 0);
}

/* Отправить через конкретный интерфейс (DHCP - когда адреса ещё нет) */
INTN ip_send_if(NETIF *nif, UINT32 src, UINT32 dst, UINT8 proto,
                const UINT8 *payload, UINTN len)
{
    static UINT8 pkt[NET_MTU];      /* под g_net_mutex */

    if (len + IP_HLEN > NET_MTU)
        return -1;

    pkt[0] = 0x45;                   /* версия 4, заголовок 5*4 байт */
    pkt[1] = 0;                      /* тип обслуживания */
    net_put16(pkt + 2, (UINT16)(IP_HLEN + len));
    net_put16(pkt + 4, g_ip_id++);
    net_put16(pkt + 6, 0x4000);      /* DF - не фрагментировать */
    pkt[8] = 64;                     /* TTL - сколько роутеров пройдёт */
    pkt[9] = proto;
    net_put16(pkt + 10, 0);
    net_put32(pkt + 12, src);
    net_put32(pkt + 16, dst);
    net_put16(pkt + 10, net_csum(pkt, IP_HLEN, 0));

    memcpy(pkt + IP_HLEN, payload, len);

    UINT32 hop = dst;

    if (!nif->loopback && dst != 0xFFFFFFFFu && nif->up &&
        (dst & nif->mask) != (nif->ip & nif->mask) && nif->gw != 0)
        hop = nif->gw;

    return arp_send_ip(nif, hop, pkt, IP_HLEN + len) ? 0 : -1;
}

/* Отправить пакет по маршруту. src == 0 - адрес выбрать самим.
   0 - ушёл (или ждёт ответа ARP), -1 - маршрута нет */
INTN ip_send(UINT32 src, UINT32 dst, UINT8 proto, const UINT8 *payload, UINTN len)
{
    UINT32 hop;
    NETIF *nif = ip_route(dst, &hop);

    if (nif == NULL)
        return -1;

    if (src == 0)
        src = ip_src_for(dst);

    return ip_send_if(nif, src, dst, proto, payload, len);
}

void ip_input(NETIF *nif, const UINT8 *p, UINTN len)
{
    if (len < IP_HLEN || (p[0] >> 4) != 4) {
        g_ip_bad++;
        return;
    }

    UINTN hl = (UINTN)(p[0] & 0xFu) * 4u;
    UINTN total = net_get16(p + 2);

    if (hl < IP_HLEN || total < hl || total > len || net_csum(p, hl, 0) != 0) {
        g_ip_bad++;
        return;
    }

    /* фрагмент (MF или смещение != 0) - не собираем */
    if (net_get16(p + 6) & 0x3FFFu) {
        g_ip_frags++;
        return;
    }

    UINT8 ttl = p[8];
    UINT8 proto = p[9];
    UINT32 src = net_get32(p + 12);
    UINT32 dst = net_get32(p + 16);

    /* наш адрес, широковещательный - или адреса ещё нет (ответ
       DHCP приходит на предложенный адрес или всем) */
    BOOLEAN mine = (nif->up && dst == nif->ip) || dst == 0xFFFFFFFFu ||
                   (nif->up && nif->mask != 0 && (dst & ~nif->mask) == ~nif->mask &&
                    (dst & nif->mask) == (nif->ip & nif->mask)) ||
                   (nif->loopback && ip_is_local(dst)) ||
                   (!nif->up && proto == IP_PROTO_UDP);

    if (!mine) {
        g_ip_notme++;
        return;
    }

    const UINT8 *pl = p + hl;
    UINTN plen = total - hl;

    if (proto == IP_PROTO_ICMP)
        icmp_input(nif, src, dst, ttl, pl, plen);
    else if (proto == IP_PROTO_UDP)
        udp_input(nif, src, dst, pl, plen);
    else if (proto == IP_PROTO_TCP)
        tcp_input(nif, src, dst, pl, plen);
}

/* ================================================================
 * ICMP
 * ================================================================ */

static UINT64 g_icmp_echo_answered = 0;

void icmp_input(NETIF *nif, UINT32 src, UINT32 dst, UINT8 ttl, const UINT8 *p, UINTN len)
{
    (void)nif;

    if (len < 8 || net_csum(p, len, 0) != 0)
        return;

    UINT8 type = p[0];

    if (type == 8) {

        /* Echo Request -> Echo Reply: те же данные, тип 0 */
        static UINT8 reply[NET_MTU];

        if (len > sizeof(reply) - IP_HLEN)
            return;

        memcpy(reply, p, len);
        reply[0] = 0;
        net_put16(reply + 2, 0);
        net_put16(reply + 2, net_csum(reply, len, 0));

        /* отвечаем с того адреса, на который спросили (если он не
           широковещательный) */
        UINT32 from = ip_is_local(dst) ? dst : 0;

        ip_send(from, src, IP_PROTO_ICMP, reply, len);
        g_icmp_echo_answered++;
        return;
    }

    if (type == 0) {
        sock_icmp_input(src, ttl, p, len);
        return;
    }

    /* Destination Unreachable (3), Time Exceeded (11): пусть знает и
       сокет ping (traceroute-подобные сообщения) */
    if (type == 3 || type == 11)
        sock_icmp_input(src, ttl, p, len);
}

void ip_print_stats(SIMPLE_TEXT_OUTPUT_INTERFACE *out)
{
    kprintf(out, "IP: %llu bad packets, %llu fragments dropped, %llu not for us; "
                 "answered %llu pings\n",
            g_ip_bad, g_ip_frags, g_ip_notme, g_icmp_echo_answered);
}
