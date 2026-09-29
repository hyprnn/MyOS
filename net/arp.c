/*
 * net/arp.c - ARP: "у кого IP-адрес 192.168.1.1? Скажите свой MAC".
 * Часть MyOS; общие объявления - в net/net.h.
 *
 * В локальной сети кадры Ethernet адресуются MAC-адресами, а
 * программы знают только IP. Прежде чем отправить пакет соседу
 * (или шлюзу - роутеру), нужно узнать его MAC: широковещательный
 * запрос ARP "кто 10.0.2.2?" и ответ "10.0.2.2 - это 52:55:0a:00:02:02".
 * Ответы запоминаются в таблице (кэш) на 10 минут.
 *
 * Пока ответа нет, ОДИН пакет, ради которого спрашивали, ждёт в
 * записи таблицы и уходит, как только MAC станет известен (для
 * первого ping или SYN TCP этого хватает; остальное повторят TCP и
 * программы).
 */
#include "net.h"

#define ARP_ENTRIES   32
#define ARP_TTL_MS    (10u * 60u * 1000u)
#define ARP_RETRY_MS  1000u
#define ARP_TRIES     3

#define ARP_FREE      0
#define ARP_PENDING   1
#define ARP_OK        2

typedef struct {
    UINT8   state;
    NETIF  *nif;
    UINT32  ip;
    UINT8   mac[6];
    UINT64  time_ms;          /* когда узнали / когда спросили */
    UINT8   tries;
    /* пакет, который ждёт ответа */
    UINT8  *wait_pkt;
    UINT16  wait_len;
} ARP_ENTRY;

static ARP_ENTRY g_arp[ARP_ENTRIES];

static const UINT8 g_bcast[6] = { 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF };

static void arp_send(NETIF *nif, UINT16 op, const UINT8 tha[6], UINT32 tpa,
                     const UINT8 eth_dst[6])
{
    UINT8 p[28];

    net_put16(p + 0, 1);              /* оборудование: Ethernet */
    net_put16(p + 2, ETH_TYPE_IP);    /* протокол: IPv4 */
    p[4] = 6;                         /* длина MAC */
    p[5] = 4;                         /* длина IP */
    net_put16(p + 6, op);             /* 1 - запрос, 2 - ответ */
    memcpy(p + 8, nif->mac, 6);
    net_put32(p + 14, nif->up ? nif->ip : 0);
    memcpy(p + 18, tha, 6);
    net_put32(p + 24, tpa);

    net_send_eth(nif, eth_dst, ETH_TYPE_ARP, p, sizeof(p));
}

static ARP_ENTRY *arp_find(NETIF *nif, UINT32 ip)
{
    for (UINTN i = 0; i < ARP_ENTRIES; i++)
        if (g_arp[i].state != ARP_FREE && g_arp[i].nif == nif && g_arp[i].ip == ip)
            return &g_arp[i];

    return NULL;
}

static void arp_drop_wait(ARP_ENTRY *e)
{
    if (e->wait_pkt != NULL) {
        kfree(e->wait_pkt);
        e->wait_pkt = NULL;
        e->wait_len = 0;
    }
}

/* Свободная запись; нет - самая старая */
static ARP_ENTRY *arp_alloc(void)
{
    ARP_ENTRY *old = &g_arp[0];

    for (UINTN i = 0; i < ARP_ENTRIES; i++) {
        if (g_arp[i].state == ARP_FREE)
            return &g_arp[i];
        if (g_arp[i].time_ms < old->time_ms)
            old = &g_arp[i];
    }

    arp_drop_wait(old);
    old->state = ARP_FREE;

    return old;
}

/* Запомнить (или обновить) пару IP - MAC; если кто-то ждал - отправить */
static void arp_learn(NETIF *nif, UINT32 ip, const UINT8 mac[6], BOOLEAN create)
{
    ARP_ENTRY *e = arp_find(nif, ip);

    if (e == NULL) {
        if (!create)
            return;
        e = arp_alloc();
        e->nif = nif;
        e->ip = ip;
        e->wait_pkt = NULL;
    }

    memcpy(e->mac, mac, 6);
    e->state = ARP_OK;
    e->time_ms = net_now_ms();
    e->tries = 0;

    if (e->wait_pkt != NULL) {
        net_send_eth(nif, e->mac, ETH_TYPE_IP, e->wait_pkt, e->wait_len);
        arp_drop_wait(e);
    }
}

void arp_input(NETIF *nif, const UINT8 *p, UINTN len)
{
    if (len < 28 || net_get16(p) != 1 || net_get16(p + 2) != ETH_TYPE_IP ||
        p[4] != 6 || p[5] != 4)
        return;

    UINT16 op = net_get16(p + 6);
    const UINT8 *sha = p + 8;
    UINT32 spa = net_get32(p + 14);
    UINT32 tpa = net_get32(p + 24);
    BOOLEAN for_me = nif->up && tpa == nif->ip;

    if (spa != 0)
        arp_learn(nif, spa, sha, for_me);    /* новых соседей - только если
                                                спрашивали именно нас */

    if (op == 1 && for_me)
        arp_send(nif, 2, sha, spa, sha);     /* "это я" */
}

/*
 * Отправить IP-пакет соседу next_hop (сам получатель или шлюз).
 * MAC известен - сразу; нет - спросить и отложить пакет.
 */
BOOLEAN arp_send_ip(NETIF *nif, UINT32 next_hop, const UINT8 *ip_pkt, UINTN len)
{
    if (nif->loopback) {
        static const UINT8 zero[6] = { 0, 0, 0, 0, 0, 0 };
        return net_send_eth(nif, zero, ETH_TYPE_IP, ip_pkt, len);
    }

    /* широковещательные адреса: всей сети или всей подсети */
    if (next_hop == 0xFFFFFFFFu ||
        (nif->up && nif->mask != 0 && nif->mask != 0xFFFFFFFFu &&
         (next_hop & ~nif->mask) == ~nif->mask))
        return net_send_eth(nif, g_bcast, ETH_TYPE_IP, ip_pkt, len);

    ARP_ENTRY *e = arp_find(nif, next_hop);

    if (e != NULL && e->state == ARP_OK)
        return net_send_eth(nif, e->mac, ETH_TYPE_IP, ip_pkt, len);

    if (e == NULL) {
        e = arp_alloc();
        e->nif = nif;
        e->ip = next_hop;
        e->state = ARP_PENDING;
        e->tries = 0;
        e->wait_pkt = NULL;
        e->time_ms = 0;
    }

    /* отложить пакет (старый ждущий заменяется новым) */
    arp_drop_wait(e);
    e->wait_pkt = (UINT8 *)kmalloc(len);

    if (e->wait_pkt != NULL) {
        memcpy(e->wait_pkt, ip_pkt, len);
        e->wait_len = (UINT16)len;
    }

    UINT64 now = net_now_ms();

    if (e->tries == 0 || now - e->time_ms >= ARP_RETRY_MS) {
        static const UINT8 zero[6] = { 0, 0, 0, 0, 0, 0 };
        arp_send(nif, 1, zero, next_hop, g_bcast);
        e->time_ms = now;
        e->tries++;
    }

    return TRUE;
}

/* Раз в 10 мс: повторить запросы, забыть устаревшее */
void arp_timer(void)
{
    UINT64 now = net_now_ms();

    for (UINTN i = 0; i < ARP_ENTRIES; i++) {

        ARP_ENTRY *e = &g_arp[i];

        if (e->state == ARP_OK && now - e->time_ms > ARP_TTL_MS) {
            e->state = ARP_FREE;
            continue;
        }

        if (e->state != ARP_PENDING || now - e->time_ms < ARP_RETRY_MS)
            continue;

        if (e->tries >= ARP_TRIES) {
            /* сосед не отвечает - пакет не отправить */
            arp_drop_wait(e);
            e->state = ARP_FREE;
            continue;
        }

        static const UINT8 zero[6] = { 0, 0, 0, 0, 0, 0 };
        arp_send(e->nif, 1, zero, e->ip, g_bcast);
        e->time_ms = now;
        e->tries++;
    }
}

BOOLEAN arp_busy(void)
{
    for (UINTN i = 0; i < ARP_ENTRIES; i++)
        if (g_arp[i].state == ARP_PENDING)
            return TRUE;

    return FALSE;
}

void arp_forget_if(NETIF *nif)
{
    for (UINTN i = 0; i < ARP_ENTRIES; i++)
        if (g_arp[i].state != ARP_FREE && g_arp[i].nif == nif) {
            arp_drop_wait(&g_arp[i]);
            g_arp[i].state = ARP_FREE;
        }
}

/* "Я теперь 10.0.2.15" - всем (gratuitous ARP): соседи обновят кэш */
void arp_announce(NETIF *nif)
{
    if (nif->loopback || !nif->up)
        return;

    static const UINT8 zero[6] = { 0, 0, 0, 0, 0, 0 };
    arp_send(nif, 1, zero, nif->ip, g_bcast);
}

void arp_print(SIMPLE_TEXT_OUTPUT_INTERFACE *out)
{
    UINTN n = 0;
    UINT64 now = net_now_ms();

    print(out, "ARP table (IP -> MAC of neighbours in the local network):\n");

    for (UINTN i = 0; i < ARP_ENTRIES; i++) {

        ARP_ENTRY *e = &g_arp[i];

        if (e->state == ARP_FREE)
            continue;

        char ip[16];
        net_fmt_ip(ip, sizeof(ip), e->ip);

        if (e->state == ARP_OK)
            kprintf(out, "  %-15s %02x:%02x:%02x:%02x:%02x:%02x  %s, %llu s ago\n",
                    ip, e->mac[0], e->mac[1], e->mac[2], e->mac[3], e->mac[4], e->mac[5],
                    e->nif->name, (now - e->time_ms) / 1000u);
        else
            kprintf(out, "  %-15s (asking...)        %s\n", ip, e->nif->name);
        n++;
    }

    if (n == 0)
        print(out, "  (empty)\n");
}
