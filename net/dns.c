/*
 * net/dns.c - DNS: имя -> IP-адрес ("example.com" -> 93.184.215.14).
 * Часть MyOS; общие объявления - в net/net.h.
 *
 * Вопрос серверу DNS (его адрес дал DHCP) - один пакет UDP на порт
 * 53: заголовок 12 байт (номер вопроса, флаги, счётчики) + вопрос
 * (имя по частям: 7 example 3 com 0; тип A = IPv4; класс IN).
 * Ответ - тот же вопрос + записи ответов; имена в ответе бывают
 * "сжаты" - ссылкой на место, где это имя уже написано (два
 * старших бита 11). Записи CNAME ("это другое имя") пропускаем:
 * серверы присылают следом и A-запись для него.
 *
 * Ответы запоминаются (кэш) на столько, сколько разрешил сервер (TTL).
 */
#include "net.h"

#define DNS_CACHE 16

typedef struct {
    BOOLEAN used;
    char    name[64];
    UINT32  ip;
    UINT64  until_ms;
} DNS_ENTRY;

static DNS_ENTRY g_dns_cache[DNS_CACHE];
static UINT16 g_dns_id = 0;
static UINT64 g_dns_queries = 0, g_dns_answers = 0;

static char dns_lower(char c)
{
    return (c >= 'A' && c <= 'Z') ? (char)(c + 32) : c;
}

static BOOLEAN dns_name_eq(const char *a, const char *b)
{
    while (*a && *b) {
        if (dns_lower(*a) != dns_lower(*b))
            return FALSE;
        a++;
        b++;
    }

    return *a == *b;
}

static BOOLEAN dns_cache_get(const char *name, UINT32 *ip)
{
    UINT64 now = net_now_ms();

    for (UINTN i = 0; i < DNS_CACHE; i++) {
        DNS_ENTRY *e = &g_dns_cache[i];
        if (e->used && now < e->until_ms && dns_name_eq(e->name, name)) {
            *ip = e->ip;
            return TRUE;
        }
    }

    return FALSE;
}

static void dns_cache_put(const char *name, UINT32 ip, UINT32 ttl)
{
    DNS_ENTRY *e = &g_dns_cache[0];

    for (UINTN i = 0; i < DNS_CACHE; i++) {
        if (!g_dns_cache[i].used || dns_name_eq(g_dns_cache[i].name, name)) {
            e = &g_dns_cache[i];
            break;
        }
        if (g_dns_cache[i].until_ms < e->until_ms)
            e = &g_dns_cache[i];
    }

    if (ttl < 10) ttl = 10;
    if (ttl > 3600) ttl = 3600;

    ksnprintf(e->name, sizeof(e->name), "%s", name);
    e->ip = ip;
    e->until_ms = net_now_ms() + (UINT64)ttl * 1000u;
    e->used = TRUE;
}

/* Собрать вопрос "A-запись для name". Возвращает длину или 0 */
static UINTN dns_build_query(UINT8 *q, UINTN cap, UINT16 id, const char *name)
{
    UINTN o = 12;

    memset(q, 0, 12);
    net_put16(q + 0, id);
    net_put16(q + 2, 0x0100);        /* RD: "спроси у других, если сам не знаешь" */
    net_put16(q + 4, 1);             /* один вопрос */

    const char *p = name;

    while (*p) {

        const char *dot = p;
        while (*dot && *dot != '.')
            dot++;

        UINTN l = (UINTN)(dot - p);

        if (l == 0 || l > 63 || o + l + 2 >= cap)
            return 0;

        q[o++] = (UINT8)l;
        memcpy(q + o, p, l);
        o += l;

        p = (*dot == '.') ? dot + 1 : dot;
    }

    if (o + 5 > cap)
        return 0;

    q[o++] = 0;
    net_put16(q + o, 1);             /* тип A */
    net_put16(q + o + 2, 1);         /* класс IN */

    return o + 4;
}

/* Пропустить имя (возможно, сжатое) - позиция после него */
static UINTN dns_skip_name(const UINT8 *m, UINTN len, UINTN o)
{
    while (o < len) {

        UINT8 l = m[o];

        if (l == 0)
            return o + 1;

        if ((l & 0xC0u) == 0xC0u)
            return o + 2;            /* ссылка - конец имени */

        o += 1u + l;
    }

    return len + 1;                   /* битый пакет */
}

/* Разобрать ответ: первый адрес IPv4. 0 - нашли; иначе ошибка */
static INTN dns_parse(const UINT8 *m, UINTN len, UINT16 id, UINT32 *ip, UINT32 *ttl)
{
    if (len < 12 || net_get16(m) != id)
        return MYOS_EAGAIN;                /* не тот ответ */

    UINT16 flags = net_get16(m + 2);

    if (!(flags & 0x8000u))
        return MYOS_EAGAIN;

    if ((flags & 0xFu) == 3)
        return MYOS_EHOSTNOTFOUND;         /* NXDOMAIN - такого имени нет */

    if ((flags & 0xFu) != 0)
        return MYOS_EIO;

    UINTN qd = net_get16(m + 4);
    UINTN an = net_get16(m + 6);
    UINTN o = 12;

    for (UINTN i = 0; i < qd; i++) {
        o = dns_skip_name(m, len, o);
        o += 4;
    }

    for (UINTN i = 0; i < an && o < len; i++) {

        o = dns_skip_name(m, len, o);

        if (o + 10 > len)
            break;

        UINT16 type = net_get16(m + o);
        UINT16 cls = net_get16(m + o + 2);
        UINT32 t = net_get32(m + o + 4);
        UINT16 rdl = net_get16(m + o + 8);

        o += 10;

        if (o + rdl > len)
            break;

        if (type == 1 && cls == 1 && rdl == 4) {
            *ip = net_get32(m + o);
            *ttl = t;
            return 0;
        }

        o += rdl;
    }

    return MYOS_EHOSTNOTFOUND;             /* ответ без A-записи */
}

/* Серверы DNS: из DHCP (или заданные вручную) */
static UINTN dns_servers(UINT32 *out, UINTN cap)
{
    UINTN n = 0;

    for (UINTN i = 0; i < NET_MAX_IF && n < cap; i++) {

        NETIF *f = &g_netifs[i];

        if (!f->used || !f->up || f->loopback)
            continue;

        if (f->dns && n < cap) out[n++] = f->dns;
        if (f->dns2 && n < cap) out[n++] = f->dns2;
    }

    return n;
}

/*
 * Узнать адрес имени. "1.2.3.4" - просто разбирается; localhost -
 * 127.0.0.1. Возвращает 0 и *ip или ошибку (MYOS_EHOSTNOTFOUND,
 * MYOS_ETIMEDOUT, MYOS_ENETUNREACH).
 */
INTN dns_resolve(const char *name, UINT32 *ip, UINT64 timeout_ms)
{
    if (net_parse_ip(name, ip))
        return 0;

    if (dns_name_eq(name, "localhost")) {
        *ip = NET_IP(127, 0, 0, 1);
        return 0;
    }

    if (name[0] == '\0')
        return MYOS_EINVAL;

    kmutex_lock(&g_net_mutex);

    BOOLEAN hit = dns_cache_get(name, ip);
    UINT32 servers[4];
    UINTN ns = dns_servers(servers, 4);

    kmutex_unlock(&g_net_mutex);

    if (hit)
        return 0;

    if (ns == 0)
        return MYOS_ENETUNREACH;

    INTN s = sock_create(MYOS_SOCK_DGRAM, 0);

    if (s < 0)
        return s;

    sock_bind(s, 0, 0);

    UINT8 q[300], a[512];
    UINT16 id = (UINT16)(net_random() ^ ++g_dns_id);
    UINTN ql = dns_build_query(q, sizeof(q), id, name);
    INTN r = MYOS_ETIMEDOUT;

    if (ql == 0) {
        sock_close(s);
        return MYOS_EINVAL;
    }

    if (timeout_ms == 0)
        timeout_ms = 6000;

    UINT64 end = net_now_ms() + timeout_ms;
    UINT64 per = timeout_ms / 3u;

    if (per < 500) per = 500;

    for (UINTN attempt = 0; attempt < 6 && net_now_ms() < end; attempt++) {

        UINT32 srv = servers[attempt % ns];

        g_dns_queries++;
        if (sock_sendto(s, q, ql, srv, 53) < 0) {
            r = MYOS_ENETUNREACH;
            continue;
        }

        UINT64 left = end - net_now_ms();
        sock_setopt(s, MYOS_SO_TIMEOUT, left < per ? left : per);

        for (;;) {

            UINT32 from;
            UINT16 fport;
            INTN n = sock_recvfrom(s, a, sizeof(a), &from, &fport, NULL);

            if (n < 0) {
                r = (n == MYOS_EAGAIN) ? MYOS_ETIMEDOUT : n;
                break;
            }

            UINT32 got = 0, ttl = 0;
            INTN pr = dns_parse(a, (UINTN)n, id, &got, &ttl);

            if (pr == MYOS_EAGAIN)
                continue;           /* чужой/старый ответ - ждём дальше */

            r = pr;

            if (pr == 0) {
                *ip = got;
                g_dns_answers++;
                kmutex_lock(&g_net_mutex);
                dns_cache_put(name, got, ttl);
                kmutex_unlock(&g_net_mutex);
            }
            break;
        }

        if (r == 0 || r == MYOS_EHOSTNOTFOUND || r == MYOS_EINTR)
            break;
    }

    sock_close(s);

    return r;
}

void dns_print_cache(SIMPLE_TEXT_OUTPUT_INTERFACE *out)
{
    UINT64 now = net_now_ms();
    UINTN n = 0;

    kprintf(out, "DNS: %llu questions, %llu answers; remembered names:\n",
            g_dns_queries, g_dns_answers);

    for (UINTN i = 0; i < DNS_CACHE; i++) {

        DNS_ENTRY *e = &g_dns_cache[i];

        if (!e->used || now >= e->until_ms)
            continue;

        char ip[16];
        net_fmt_ip(ip, sizeof(ip), e->ip);
        kprintf(out, "  %-30s %-15s %llu s left\n", e->name, ip, (e->until_ms - now) / 1000u);
        n++;
    }

    if (n == 0)
        print(out, "  (empty)\n");
}
