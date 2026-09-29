/*
 * net/dhcp.c - DHCP: получить IP-адрес от роутера. Часть MyOS;
 * общие объявления - в net/net.h.
 *
 * Только что подключённый компьютер не знает о сети ничего: ни
 * своего адреса, ни шлюза, ни DNS. Он кричит всем (широковещательно,
 * с адреса 0.0.0.0):
 *   DISCOVER  "есть тут DHCP-сервер? мне нужен адрес"
 *   OFFER     сервер: "возьми 192.168.1.37" (+ маска, шлюз, DNS)
 *   REQUEST   "беру 192.168.1.37 у сервера такого-то"
 *   ACK       сервер: "он твой на столько-то секунд" (аренда)
 * В половине срока аренды адрес продлевается (REQUEST/ACK). NAK -
 * "нельзя" - начинаем сначала. Роутер дома, раздача интернета с
 * телефона и QEMU (-netdev user) - все работают так.
 *
 * Пакет DHCP (он же BOOTP): 236 байт фиксированных полей + "magic
 * cookie" 63 82 53 63 + опции вида [код, длина, данные].
 */
#include "net.h"

#define DHCP_DISCOVER  1
#define DHCP_OFFER     2
#define DHCP_REQUEST   3
#define DHCP_ACK       5
#define DHCP_NAK       6

const char *dhcp_state_name(UINT8 s)
{
    switch (s) {
    case DHCP_OFF:        return "off";
    case DHCP_SELECTING:  return "looking for a DHCP server";
    case DHCP_REQUESTING: return "requesting the address";
    case DHCP_BOUND:      return "address received";
    case DHCP_RENEWING:   return "renewing the lease";
    case DHCP_FAILED:     return "no DHCP server answered (will retry)";
    default:              return "?";
    }
}

/* Собрать и отправить DISCOVER или REQUEST */
static void dhcp_send(NETIF *nif, UINT8 type)
{
    static UINT8 m[300];
    DHCP_STATE *d = &nif->dhcp;

    memset(m, 0, sizeof(m));

    m[0] = 1;                       /* запрос клиента */
    m[1] = 1;                       /* Ethernet */
    m[2] = 6;                       /* длина MAC */
    net_put32(m + 4, d->xid);
    net_put16(m + 10, 0x8000);      /* "ответьте всем" - адреса ещё нет */

    if (d->state == DHCP_RENEWING)
        net_put32(m + 12, nif->ip); /* ciaddr - продлеваем свой */

    memcpy(m + 28, nif->mac, 6);    /* chaddr */

    UINT8 *o = m + 236;

    o[0] = 0x63; o[1] = 0x82; o[2] = 0x53; o[3] = 0x63;
    o += 4;

    *o++ = 53; *o++ = 1; *o++ = type;              /* тип сообщения */

    *o++ = 61; *o++ = 7; *o++ = 1;                 /* client id = MAC */
    memcpy(o, nif->mac, 6);
    o += 6;

    if (type == DHCP_REQUEST && d->state == DHCP_REQUESTING) {
        *o++ = 50; *o++ = 4;                       /* нужный адрес */
        net_put32(o, d->offered_ip);
        o += 4;
        *o++ = 54; *o++ = 4;                       /* у этого сервера */
        net_put32(o, d->server);
        o += 4;
    }

    *o++ = 12; *o++ = 4;                           /* имя компьютера */
    *o++ = 'm'; *o++ = 'y'; *o++ = 'o'; *o++ = 's';

    *o++ = 55; *o++ = 4;                           /* что хотим знать: */
    *o++ = 1;                                      /*   маска */
    *o++ = 3;                                      /*   шлюз */
    *o++ = 6;                                      /*   DNS */
    *o++ = 51;                                     /*   срок аренды */

    *o++ = 255;                                    /* конец */

    UINTN len = (UINTN)(o - m);

    if (len < 300)
        len = 300;                                 /* некоторые серверы
                                                      не любят короткие */

    udp_send_if(nif, 0, 68, 0xFFFFFFFFu, 67, m, len);
}

void dhcp_start(NETIF *nif)
{
    DHCP_STATE *d = &nif->dhcp;

    d->state = DHCP_SELECTING;
    d->xid = net_random();
    d->tries = 0;
    d->next_ms = net_now_ms();     /* сразу */
    d->note = "sending DISCOVER";
}

void dhcp_stop(NETIF *nif)
{
    nif->dhcp.state = DHCP_OFF;
    nif->dhcp.note = NULL;
}

/* Раз в 10 мс для каждого интерфейса */
void dhcp_timer(NETIF *nif)
{
    DHCP_STATE *d = &nif->dhcp;
    UINT64 now = net_now_ms();

    if (d->state == DHCP_OFF || now < d->next_ms)
        return;

    switch (d->state) {

    case DHCP_SELECTING:
    case DHCP_REQUESTING:

        if (d->tries >= 6) {
            /* ~1 минута тишины: подождём и начнём заново */
            d->state = DHCP_FAILED;
            d->next_ms = now + 30000u;
            d->note = "no answer from a DHCP server";
            klog("dhcp: %s: no answer, retry in 30 s\n", nif->name);
            return;
        }

        if (d->state == DHCP_REQUESTING && d->tries >= 3) {
            /* REQUEST без ответа - снова искать сервер */
            dhcp_start(nif);
            return;
        }

        dhcp_send(nif, d->state == DHCP_SELECTING ? DHCP_DISCOVER : DHCP_REQUEST);

        /* повтор через 2, 4, 8... с (не больше 16) */
        UINT64 wait = 2000ull << (d->tries < 3 ? d->tries : 3);
        d->next_ms = now + wait;
        d->tries++;
        return;

    case DHCP_BOUND:

        /* половина аренды прошла - продлить */
        d->state = DHCP_RENEWING;
        d->xid = net_random();
        d->tries = 0;
        d->note = "renewing";
        /* fallthrough */

    case DHCP_RENEWING:

        if (d->tries >= 4) {
            /* сервер молчит: аренда вот-вот кончится - начать заново */
            nif->up = FALSE;
            arp_forget_if(nif);
            dhcp_start(nif);
            return;
        }

        dhcp_send(nif, DHCP_REQUEST);
        d->next_ms = now + 5000u;
        d->tries++;
        return;

    case DHCP_FAILED:
        dhcp_start(nif);
        return;
    }
}

void dhcp_input(NETIF *nif, const UINT8 *m, UINTN len)
{
    DHCP_STATE *d = &nif->dhcp;

    if (nif->loopback || d->state == DHCP_OFF || d->state == DHCP_BOUND ||
        d->state == DHCP_FAILED)
        return;

    if (len < 240 || m[0] != 2 || net_get32(m + 4) != d->xid ||
        memcmp(m + 28, nif->mac, 6) != 0 || net_get32(m + 236) != 0x63825363u)
        return;

    UINT32 yiaddr = net_get32(m + 16);
    UINT8 type = 0;
    UINT32 mask = 0, router = 0, dns = 0, dns2 = 0, server = 0, lease = 0;

    /* опции */
    UINTN i = 240;

    while (i < len) {

        UINT8 code = m[i];

        if (code == 255)
            break;

        if (code == 0) {
            i++;
            continue;
        }

        if (i + 1 >= len)
            break;

        UINT8 ol = m[i + 1];
        const UINT8 *v = m + i + 2;

        if (i + 2u + ol > len)
            break;

        if (code == 53 && ol >= 1) type = v[0];
        if (code == 1 && ol >= 4) mask = net_get32(v);
        if (code == 3 && ol >= 4) router = net_get32(v);
        if (code == 6 && ol >= 4) dns = net_get32(v);
        if (code == 6 && ol >= 8) dns2 = net_get32(v + 4);
        if (code == 54 && ol >= 4) server = net_get32(v);
        if (code == 51 && ol >= 4) lease = net_get32(v);

        i += 2u + ol;
    }

    if (type == DHCP_OFFER && d->state == DHCP_SELECTING) {

        d->offered_ip = yiaddr;
        d->server = server;
        d->state = DHCP_REQUESTING;
        d->tries = 0;
        d->next_ms = net_now_ms();       /* REQUEST - сразу */
        d->note = "got an offer, requesting";

        char a[16];
        net_fmt_ip(a, sizeof(a), yiaddr);
        klog("dhcp: %s: offered %s\n", nif->name, a);
        return;
    }

    if (type == DHCP_NAK) {
        klog("dhcp: %s: NAK - starting over\n", nif->name);
        nif->up = FALSE;
        arp_forget_if(nif);
        dhcp_start(nif);
        return;
    }

    if (type != DHCP_ACK)
        return;

    if (mask == 0) {
        /* маски нет - по классу адреса (так делали до CIDR) */
        mask = (yiaddr >> 24) < 128 ? 0xFF000000u :
               (yiaddr >> 24) < 192 ? 0xFFFF0000u : 0xFFFFFF00u;
    }

    if (lease == 0 || lease > 7u * 24u * 3600u)
        lease = 24u * 3600u;

    BOOLEAN changed = !nif->up || nif->ip != yiaddr;

    nif->ip = yiaddr;
    nif->mask = mask;
    nif->gw = router;
    nif->dns = dns;
    nif->dns2 = dns2;
    nif->up = TRUE;

    d->server = server ? server : d->server;
    d->lease_s = lease;
    d->lease_start_ms = net_now_ms();
    if (changed)
        d->bound_ms = d->lease_start_ms;
    d->state = DHCP_BOUND;
    d->tries = 0;
    d->next_ms = d->lease_start_ms + (UINT64)lease * 500u;    /* половина */
    d->note = "bound";

    char a[16], g[16], n[16], s[16];
    net_fmt_ip(a, sizeof(a), yiaddr);
    net_fmt_ip(g, sizeof(g), router);
    net_fmt_ip(n, sizeof(n), dns);
    net_fmt_ip(s, sizeof(s), mask);
    klog("dhcp: %s: address %s mask %s gateway %s DNS %s, lease %u s\n",
         nif->name, a, s, g, n, lease);

    if (changed)
        arp_announce(nif);
}
