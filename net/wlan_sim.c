/*
 * net/wlan_sim.c - программная точка доступа Wi-Fi "MyOS-Test" для
 * автотеста. Часть MyOS.
 *
 * В QEMU нет эмуляции радиочипа, поэтому 802.11-часть MyOS (net/wlan.c,
 * net/wpa.c) проверяем так: вместо драйвера чипа - "адаптер", внутри
 * которого живёт точка доступа, как настоящий роутер:
 *   * на канале 6 шлёт маяки и отвечает на probe (WPA2-PSK, CCMP);
 *   * проходит authentication и association;
 *   * ведёт 4-стороннее рукопожатие WPA2 со стороны точки (пароль
 *     "myos-wifi-test"): неверный пароль - не принимает подпись 2/4,
 *     повторяет 1/4 и в конце "выгоняет" (deauth, причина 15);
 *   * шифрует и расшифровывает кадры данных CCMP, широковещательные -
 *     общим ключом GTK;
 *   * за ней "роутер" 192.168.77.1: DHCP (выдаёт 192.168.77.2), ARP и
 *     ping; после выдачи адреса спрашивает широковещательно "кто
 *     192.168.77.2?" (клиент должен расшифровать это общим ключом GTK и
 *     ответить); широковещательные кадры клиента отражает обратно (как
 *     настоящая точка) - клиент должен их узнать и выбросить.
 * Включается командой "wifi sim" (только если настоящего адаптера нет).
 */
#include "net.h"
#include "wifi.h"
#include "wlan.h"

#define SIM_CH      6
#define SIM_PASS    "myos-wifi-test"
#define SIM_SSID    "MyOS-Test"
#define SIM_QN      24

static const UINT8 g_sim_bssid[6] = { 0x02, 0x00, 0x5E, 0x57, 0x49, 0x46 };
static const UINT8 g_sim_mac[6]   = { 0x02, 0x4D, 0x59, 0x57, 0x4C, 0x01 };
static const UINT8 g_bcast[6]     = { 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF };

/* RSN IE точки: WPA2, CCMP/CCMP, PSK */
static const UINT8 g_sim_rsn[22] = {
    0x30, 20, 1, 0, 0x00, 0x0F, 0xAC, 4, 1, 0, 0x00, 0x0F, 0xAC, 4,
    1, 0, 0x00, 0x0F, 0xAC, 2, 0, 0
};

typedef struct {
    UINT16 len;
    UINT8  data[1700];
} SIM_FRAME;

static struct {
    WLAN_HW   hw;
    BOOLEAN   up;
    UINT8     ch;

    SIM_FRAME q[SIM_QN];              /* кадры "в эфире" к клиенту */
    UINT32    qh, qt;
    UINT64    next_beacon_ms;
    UINT64    beacons;

    /* точка */
    UINT8     pmk[32];
    BOOLEAN   assoc;
    UINT8     anonce[32];
    UINT64    replay;
    UINT8     ptk[48];
    BOOLEAN   ptk_ok;                 /* 2/4 с верной подписью */
    BOOLEAN   keys;                   /* 4/4 получено - ключи в деле */
    UINT8     gtk[16];
    UINT64    pn_tx, pn_gtk;
    UINT32    m1_count;
    UINT64    m1_ms;
    UINT16    seq;

    UINT64    tx_frames, bad_mic, dhcp, pings, echoes, arp_asked, arp_replies;
} g_sim;

/* ================================================================
 * "Эфир": очередь кадров к клиенту
 * ================================================================ */

static void sim_air(const UINT8 *f, UINTN len)
{
    UINT32 next = (g_sim.qh + 1u) % SIM_QN;

    if (next == g_sim.qt || len > sizeof(g_sim.q[0].data))
        return;

    g_sim.q[g_sim.qh].len = (UINT16)len;
    memcpy(g_sim.q[g_sim.qh].data, f, len);
    g_sim.qh = next;
}

static UINTN sim_hdr(UINT8 *f, UINT8 fc0, UINT8 fc1, const UINT8 *a1, const UINT8 *a2,
                     const UINT8 *a3)
{
    memset(f, 0, 24);
    f[0] = fc0;
    f[1] = fc1;
    memcpy(f + 4, a1, 6);
    memcpy(f + 10, a2, 6);
    memcpy(f + 16, a3, 6);
    f[22] = (UINT8)(g_sim.seq << 4);
    f[23] = (UINT8)(g_sim.seq >> 4);
    g_sim.seq = (UINT16)((g_sim.seq + 1u) & 0xFFFu);
    return 24;
}

static UINTN sim_ies(UINT8 *p)
{
    static const UINT8 rates[] = { 1, 8, 0x82, 0x84, 0x8B, 0x96, 0x0C, 0x12, 0x18, 0x24 };
    static const UINT8 ext[] = { 50, 4, 0x30, 0x48, 0x60, 0x6C };
    UINTN n = 0;

    p[n++] = 0;
    p[n++] = sizeof(SIM_SSID) - 1;
    memcpy(p + n, SIM_SSID, sizeof(SIM_SSID) - 1);
    n += sizeof(SIM_SSID) - 1;
    memcpy(p + n, rates, sizeof(rates));
    n += sizeof(rates);
    p[n++] = 3;
    p[n++] = 1;
    p[n++] = SIM_CH;
    memcpy(p + n, ext, sizeof(ext));
    n += sizeof(ext);
    memcpy(p + n, g_sim_rsn, sizeof(g_sim_rsn));
    n += sizeof(g_sim_rsn);

    return n;
}

/* маяк (subtype 8) или ответ на probe (5) */
static void sim_beacon(UINT8 subtype, const UINT8 *da)
{
    UINT8 f[160];
    UINTN n = sim_hdr(f, (UINT8)(subtype << 4), 0, da, g_sim_bssid, g_sim_bssid);

    memset(f + n, 0, 8);                          /* время */
    n += 8;
    f[n++] = 100;                                 /* интервал маяков: 100 TU */
    f[n++] = 0;
    f[n++] = 0x11;                                /* ESS | Privacy */
    f[n++] = 0x04;                                /* короткий слот */
    n += sim_ies(f + n);

    sim_air(f, n);
}

/* Кадр данных клиенту: от точки (FromDS); шифруем, если ключи есть */
static void sim_send_data(const UINT8 *da, const UINT8 *sa, UINT16 type, const UINT8 *data,
                          UINTN len)
{
    static UINT8 body[1600], f[1700];
    UINT8 h[26];
    BOOLEAN group = (da[0] & 1u) != 0;
    BOOLEAN qos = !group;                         /* личные - кадрами QoS (TID 0) */
    UINTN hl;

    if (len + 8 > sizeof(body))
        return;

    hl = sim_hdr(h, qos ? 0x88 : 0x08, 0x02, da, g_sim_bssid, sa);
    if (qos) {
        h[24] = 0;
        h[25] = 0;
        hl = 26;
    }

    memcpy(body, "\xAA\xAA\x03\x00\x00\x00", 6);
    body[6] = (UINT8)(type >> 8);
    body[7] = (UINT8)type;
    memcpy(body + 8, data, len);

    UINTN n;

    if (g_sim.keys && group) {
        n = ccmp_encrypt(g_sim.gtk, h, hl, ++g_sim.pn_gtk, 1, body, len + 8, f);
    } else if (g_sim.keys) {
        n = ccmp_encrypt(g_sim.ptk + 32, h, hl, ++g_sim.pn_tx, 0, body, len + 8, f);
    } else {
        memcpy(f, h, hl);
        memcpy(f + hl, body, len + 8);
        n = hl + len + 8;
    }

    sim_air(f, n);
}

/* ================================================================
 * WPA2: сторона точки
 * ================================================================ */

static UINTN sim_eapol(UINT8 *f, UINT16 info, const UINT8 *kd, UINTN kdlen)
{
    UINTN n = 99 + kdlen;

    memset(f, 0, n);
    f[0] = 2;
    f[1] = 3;
    f[2] = (UINT8)((n - 4) >> 8);
    f[3] = (UINT8)(n - 4);
    f[4] = 2;
    f[5] = (UINT8)(info >> 8);
    f[6] = (UINT8)info;
    f[8] = 16;
    for (UINTN i = 0; i < 8; i++)
        f[9 + i] = (UINT8)(g_sim.replay >> (56u - 8u * i));
    memcpy(f + 17, g_sim.anonce, 32);
    f[97] = (UINT8)(kdlen >> 8);
    f[98] = (UINT8)kdlen;
    memcpy(f + 99, kd, kdlen);

    if (info & 0x0100u) {                         /* подпись KCK */
        UINT8 hm[20];
        hmac_sha1(g_sim.ptk, 16, f, n, hm);
        memcpy(f + 81, hm, 16);
    }

    return n;
}

static void sim_send_m1(void)
{
    UINT8 f[128];

    g_sim.replay++;
    sim_send_data(g_sim_mac, g_sim_bssid, 0x888E, f, sim_eapol(f, 0x008A, NULL, 0));
    g_sim.m1_count++;
    g_sim.m1_ms = net_now_ms();
}

static void sim_send_m3(void)
{
    UINT8 kd[64], wk[72], f[256];
    UINTN kl = 0;

    memcpy(kd, g_sim_rsn, sizeof(g_sim_rsn));
    kl = sizeof(g_sim_rsn);
    kd[kl++] = 0xDD;
    kd[kl++] = 22;
    kd[kl++] = 0x00; kd[kl++] = 0x0F; kd[kl++] = 0xAC; kd[kl++] = 0x01;
    kd[kl++] = 0x01;                              /* номер ключа 1 */
    kd[kl++] = 0x00;
    memcpy(kd + kl, g_sim.gtk, 16);
    kl += 16;
    if (kl % 8) {
        kd[kl++] = 0xDD;
        while (kl % 8)
            kd[kl++] = 0;
    }

    aes_wrap(g_sim.ptk + 16, kl / 8, kd, wk);

    g_sim.replay++;
    sim_send_data(g_sim_mac, g_sim_bssid, 0x888E, f,
                  sim_eapol(f, 0x13CA, wk, kl + 8));
}

static BOOLEAN sim_mic_ok(const UINT8 *f, UINTN n)
{
    static UINT8 c[512];
    UINT8 hm[20];

    if (n > sizeof(c) || n < 99)
        return FALSE;

    memcpy(c, f, n);
    memset(c + 81, 0, 16);
    hmac_sha1(g_sim.ptk, 16, c, n, hm);

    return memcmp(hm, f + 81, 16) == 0;
}

static void sim_rx_eapol(const UINT8 *f, UINTN n)
{
    if (n < 99 || f[1] != 3)
        return;

    UINT16 info = (UINT16)((f[5] << 8) | f[6]);

    if ((info & 0x0008u) && (info & 0x0100u) && !(info & 0x0200u)) {

        /* 2/4: SNonce клиента -> наш PTK; подпись должна сойтись */
        wpa_derive_ptk(g_sim.pmk, g_sim_bssid, g_sim_mac, g_sim.anonce, f + 17, g_sim.ptk);

        if (!sim_mic_ok(f, n)) {
            g_sim.bad_mic++;                      /* пароль неверный - молчим */
            return;
        }

        g_sim.ptk_ok = TRUE;
        sim_send_m3();
        return;
    }

    if ((info & 0x0008u) && (info & 0x0200u) && g_sim.ptk_ok) {

        /* 4/4 - ключи в деле */
        if (sim_mic_ok(f, n)) {
            g_sim.keys = TRUE;
            g_sim.pn_tx = 0;
        }
    }
}

/* ================================================================
 * "Роутер" за точкой: DHCP, ARP, ping
 * ================================================================ */

#define SIM_GW   NET_IP(192, 168, 77, 1)
#define SIM_IP   NET_IP(192, 168, 77, 2)

static void sim_ip_send(const UINT8 *da, UINT8 proto, UINT32 dst, const UINT8 *payload,
                        UINTN len)
{
    static UINT8 p[1600];

    if (len + 20 > sizeof(p))
        return;

    memset(p, 0, 20);
    p[0] = 0x45;
    net_put16(p + 2, (UINT16)(len + 20));
    p[8] = 64;
    p[9] = proto;
    net_put32(p + 12, SIM_GW);
    net_put32(p + 16, dst);
    net_put16(p + 10, net_csum(p, 20, 0));
    memcpy(p + 20, payload, len);

    sim_send_data(da, g_sim_bssid, 0x0800, p, len + 20);
}

static void sim_dhcp(const UINT8 *d, UINTN len)
{
    if (len < 240 || d[0] != 1)
        return;

    UINT8 type = 0;

    for (UINTN i = 240; i + 2 <= len && d[i] != 255; ) {
        if (d[i] == 0) {
            i++;
            continue;
        }
        if (d[i] == 53 && d[i + 1] >= 1)
            type = d[i + 2];
        i += 2u + d[i + 1];
    }

    if (type != 1 && type != 3)                   /* DISCOVER, REQUEST */
        return;

    static UINT8 u[8 + 300];
    UINT8 *m = u + 8;

    memset(u, 0, sizeof(u));
    m[0] = 2;
    m[1] = 1;
    m[2] = 6;
    memcpy(m + 4, d + 4, 4);                      /* xid */
    net_put32(m + 16, SIM_IP);                    /* yiaddr */
    net_put32(m + 20, SIM_GW);                    /* siaddr */
    memcpy(m + 28, d + 28, 16);                   /* chaddr */
    m[236] = 99; m[237] = 130; m[238] = 83; m[239] = 99;

    UINTN o = 240;
    m[o++] = 53; m[o++] = 1; m[o++] = (type == 1) ? 2 : 5;       /* OFFER / ACK */
    m[o++] = 54; m[o++] = 4; net_put32(m + o, SIM_GW); o += 4;
    m[o++] = 51; m[o++] = 4; net_put32(m + o, 3600); o += 4;
    m[o++] = 1;  m[o++] = 4; net_put32(m + o, NET_IP(255, 255, 255, 0)); o += 4;
    m[o++] = 3;  m[o++] = 4; net_put32(m + o, SIM_GW); o += 4;
    m[o++] = 6;  m[o++] = 4; net_put32(m + o, SIM_GW); o += 4;
    m[o++] = 255;

    UINTN ulen = 8 + o;

    net_put16(u, 67);
    net_put16(u + 2, 68);
    net_put16(u + 4, (UINT16)ulen);
    net_put16(u + 6, 0);                          /* без контрольной суммы UDP */

    g_sim.dhcp++;

    /* клиент просил ответить всем (флаг 0x8000) - широковещательно, иначе
       лично на его MAC (так MyOS делает на Wi-Fi) */
    if (d[10] & 0x80u)
        sim_ip_send(g_bcast, 17, 0xFFFFFFFFu, u, ulen);
    else
        sim_ip_send(d + 28, 17, SIM_IP, u, ulen);

    /* адрес выдан - "роутер" спрашивает, кто такой 192.168.77.2
       (широковещательно - клиент расшифрует это общим ключом GTK) */
    if (type == 3) {
        UINT8 a[28];
        net_put16(a, 1);
        net_put16(a + 2, 0x0800);
        a[4] = 6;
        a[5] = 4;
        net_put16(a + 6, 1);
        memcpy(a + 8, g_sim_bssid, 6);
        net_put32(a + 14, SIM_GW);
        memset(a + 18, 0, 6);
        net_put32(a + 24, SIM_IP);
        g_sim.arp_asked++;
        sim_send_data(g_bcast, g_sim_bssid, 0x0806, a, 28);
    }
}

static void sim_rx_ip(const UINT8 *p, UINTN len)
{
    if (len < 20 || (p[0] >> 4) != 4)
        return;

    UINTN ihl = (UINTN)(p[0] & 15u) * 4u;
    UINTN tot = net_get16(p + 2);

    if (tot > len || ihl < 20 || tot < ihl)
        return;

    UINT8 proto = p[9];
    UINT32 src = net_get32(p + 12), dst = net_get32(p + 16);
    const UINT8 *d = p + ihl;
    UINTN dl = tot - ihl;

    if (proto == 17 && dl >= 8 && net_get16(d + 2) == 67) {
        sim_dhcp(d + 8, dl - 8);
        return;
    }

    if (proto == 1 && dst == SIM_GW && dl >= 8 && d[0] == 8) {

        static UINT8 r[1500];

        memcpy(r, d, dl);
        r[0] = 0;                                 /* echo reply */
        r[2] = r[3] = 0;
        net_put16(r + 2, net_csum(r, dl, 0));
        g_sim.pings++;
        sim_ip_send(g_sim_mac, 1, src, r, dl);
    }
}

static void sim_rx_arp(const UINT8 *p, UINTN len)
{
    /* ответ клиента на наш широковещательный вопрос */
    if (len >= 28 && net_get16(p + 6) == 2 && net_get32(p + 14) == SIM_IP) {
        g_sim.arp_replies++;
        return;
    }

    if (len < 28 || net_get16(p + 6) != 1 || net_get32(p + 24) != SIM_GW)
        return;

    UINT8 r[28];

    memcpy(r, p, 8);
    net_put16(r + 6, 2);                          /* ответ */
    memcpy(r + 8, g_sim_bssid, 6);                /* "MAC роутера" = MAC точки */
    net_put32(r + 14, SIM_GW);
    memcpy(r + 18, p + 8, 10);                    /* кто спрашивал */

    sim_send_data(g_sim_mac, g_sim_bssid, 0x0806, r, 28);
}

/* ================================================================
 * Кадры от клиента (hw->tx)
 * ================================================================ */

static void sim_rx_data(const UINT8 *f, UINTN len)
{
    static UINT8 plain[1700];
    UINT16 fc = (UINT16)(f[0] | (f[1] << 8));

    if ((fc & 0x0300u) != 0x0100u || !g_sim.assoc || memcmp(f + 4, g_sim_bssid, 6) != 0)
        return;

    const UINT8 *p = f + 24;
    UINTN n = len - 24;

    if (fc & 0x4000u) {
        if (!g_sim.ptk_ok)
            return;
        INTN k = ccmp_decrypt(g_sim.ptk + 32, f, len, 24, plain, NULL);
        if (k < 0)
            return;
        p = plain;
        n = (UINTN)k;
    }

    if (n < 8 || memcmp(p, "\xAA\xAA\x03\x00\x00\x00", 6) != 0)
        return;

    UINT16 type = net_get16(p + 6);
    const UINT8 *da = f + 16;

    /* широковещательное от клиента: настоящая точка разошлёт его всем,
       в том числе обратно отправителю */
    if ((da[0] & 1u) && type != 0x888E && g_sim.keys) {
        g_sim.echoes++;
        sim_send_data(g_bcast, g_sim_mac, type, p + 8, n - 8);
    }

    if (type == 0x888E)
        sim_rx_eapol(p + 8, n - 8);
    else if (type == 0x0806 && g_sim.keys)
        sim_rx_arp(p + 8, n - 8);
    else if (type == 0x0800 && g_sim.keys)
        sim_rx_ip(p + 8, n - 8);
}

static BOOLEAN sim_tx(WLAN_HW *hw, const UINT8 *f, UINTN len, UINT32 flags)
{
    (void)hw;
    (void)flags;

    g_sim.tx_frames++;

    if (g_sim.ch != SIM_CH || len < 24)
        return TRUE;                              /* на этом канале никого */

    UINT16 fc = (UINT16)(f[0] | (f[1] << 8));
    UINT8 type = (UINT8)((fc >> 2) & 3u), sub = (UINT8)((fc >> 4) & 0xFu);

    if (type == 2) {
        sim_rx_data(f, len);
        return TRUE;
    }

    if (type != 0)
        return TRUE;

    BOOLEAN to_ap = memcmp(f + 4, g_sim_bssid, 6) == 0;

    if (sub == 4) {                               /* probe request */
        UINT8 sl = len > 25 ? f[25] : 0;
        if (sl == 0 || (sl == sizeof(SIM_SSID) - 1 && len >= 26u + sl &&
                        memcmp(f + 26, SIM_SSID, sl) == 0))
            sim_beacon(5, f + 10);
    } else if (sub == 11 && to_ap) {              /* authentication */
        UINT8 r[30];
        sim_hdr(r, 0xB0, 0, f + 10, g_sim_bssid, g_sim_bssid);
        r[24] = 0; r[25] = 0; r[26] = 2; r[27] = 0; r[28] = 0; r[29] = 0;
        sim_air(r, sizeof(r));
    } else if (sub == 0 && to_ap) {               /* association request */
        UINT8 r[64];
        UINTN n = sim_hdr(r, 0x10, 0, f + 10, g_sim_bssid, g_sim_bssid);
        r[n++] = 0x11; r[n++] = 0x04;             /* capability */
        r[n++] = 0; r[n++] = 0;                   /* статус: успех */
        r[n++] = 0x01; r[n++] = 0xC0;             /* AID 1 */
        r[n++] = 1; r[n++] = 4; r[n++] = 0x82; r[n++] = 0x84; r[n++] = 0x8B; r[n++] = 0x96;
        sim_air(r, n);

        g_sim.assoc = TRUE;
        g_sim.keys = FALSE;
        g_sim.ptk_ok = FALSE;
        g_sim.m1_count = 0;
        for (UINTN i = 0; i < 32; i += 4) {
            UINT32 x = net_random();
            memcpy(g_sim.anonce + i, &x, 4);
        }
        sim_send_m1();                            /* сразу за ответом - как hostapd */
    } else if (sub == 12 && to_ap) {              /* deauthentication */
        g_sim.assoc = FALSE;
        g_sim.keys = FALSE;
    }

    return TRUE;
}

/* ================================================================
 * Остальное "железо"
 * ================================================================ */

static void sim_poll(WLAN_HW *hw)
{
    UINT64 now = net_now_ms();

    /* маяки - раз в ~100 мс */
    if (g_sim.ch == SIM_CH && now >= g_sim.next_beacon_ms) {
        g_sim.next_beacon_ms = now + 102;
        g_sim.beacons++;
        sim_beacon(8, g_bcast);
    }

    /* рукопожатие не идёт: повторить 1/4, после трёх - выгнать (как hostapd) */
    if (g_sim.assoc && !g_sim.ptk_ok && now - g_sim.m1_ms > 1000) {
        if (g_sim.m1_count < 3) {
            sim_send_m1();
        } else {
            UINT8 r[26];
            sim_hdr(r, 0xC0, 0, g_sim_mac, g_sim_bssid, g_sim_bssid);
            r[24] = 15;                           /* 4-way handshake timeout */
            r[25] = 0;
            sim_air(r, sizeof(r));
            g_sim.assoc = FALSE;
        }
    }

    /* всё, что "в эфире", - клиенту */
    while (g_sim.qt != g_sim.qh) {
        SIM_FRAME *fr = &g_sim.q[g_sim.qt];
        g_sim.qt = (g_sim.qt + 1u) % SIM_QN;
        wlan_rx(hw, fr->data, fr->len, -45, g_sim.ch);
    }
}

static BOOLEAN sim_set_channel(WLAN_HW *hw, UINT8 ch)
{
    (void)hw;
    g_sim.ch = ch;
    return TRUE;
}

static void sim_set_scan(WLAN_HW *hw, BOOLEAN on)             { (void)hw; (void)on; }
static void sim_set_bssid(WLAN_HW *hw, const UINT8 *bssid)    { (void)hw; (void)bssid; }
static void sim_report_rssi(WLAN_HW *hw, INT32 rssi)          { (void)hw; (void)rssi; }

static void sim_set_link(WLAN_HW *hw, BOOLEAN up, UINT16 aid, UINT32 rates)
{
    (void)hw;
    (void)aid;
    (void)rates;
    if (!up)
        g_sim.keys = g_sim.keys && g_sim.assoc;
}

static void sim_info(WLAN_HW *hw, SIMPLE_TEXT_OUTPUT_INTERFACE *out, BOOLEAN debug)
{
    (void)hw;

    kprintf(out, "  simulated access point \"%s\" (channel %u, password %s)\n", SIM_SSID, SIM_CH,
            SIM_PASS);
    if (debug)
        kprintf(out, "  sim: beacons %llu, frames from client %llu, bad 2/4 MIC %llu, "
                     "DHCP %llu, pings %llu, echoed broadcasts %llu, group-key ARP %llu/%llu, "
                     "keys %s\n",
                g_sim.beacons, g_sim.tx_frames, g_sim.bad_mic, g_sim.dhcp, g_sim.pings,
                g_sim.echoes, g_sim.arp_replies, g_sim.arp_asked, g_sim.keys ? "yes" : "no");
}

BOOLEAN wlan_sim_attach(SIMPLE_TEXT_OUTPUT_INTERFACE *out, WLAN_HW **hw_out)
{
    if (!g_sim.up) {
        memset(&g_sim, 0, sizeof(g_sim));
        pbkdf2_sha1((const UINT8 *)SIM_PASS, sizeof(SIM_PASS) - 1, (const UINT8 *)SIM_SSID,
                    sizeof(SIM_SSID) - 1, 4096, g_sim.pmk, 32);
        for (UINTN i = 0; i < 16; i++)
            g_sim.gtk[i] = (UINT8)(0x5A ^ (i * 17u));
        g_sim.ch = 1;

        g_sim.hw.driver = "wifi-sim";
        g_sim.hw.model = "simulated Wi-Fi (test access point)";
        memcpy(g_sim.hw.mac, g_sim_mac, 6);
        g_sim.hw.set_channel = sim_set_channel;
        g_sim.hw.tx = sim_tx;
        g_sim.hw.set_scan = sim_set_scan;
        g_sim.hw.set_bssid = sim_set_bssid;
        g_sim.hw.set_link = sim_set_link;
        g_sim.hw.poll = sim_poll;
        g_sim.hw.report_rssi = sim_report_rssi;
        g_sim.hw.info = sim_info;
        g_sim.up = TRUE;
    }

    kprintf(out, "Simulated Wi-Fi: access point \"%s\" on channel %u, password \"%s\"\n",
            SIM_SSID, SIM_CH, SIM_PASS);
    *hw_out = &g_sim.hw;
    return TRUE;
}
