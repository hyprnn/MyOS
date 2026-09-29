/*
 * net/wlan.c - Wi-Fi-клиент MyOS: протокол 802.11 поверх драйвера
 * радиочипа (net/wlan.h). Часть MyOS.
 *
 * Что здесь есть:
 *   * поиск сетей (scan): обойти каналы 2,4 и 5 ГГц, на каждом
 *     спросить "кто здесь?" (probe request) и послушать маяки (beacon);
 *   * подключение: authentication (открытая система) -> association
 *     (скорости, RSN IE с WPA2) -> 4-стороннее рукопожатие WPA2
 *     (net/wpa.c) -> ключи;
 *   * шифрование CCMP (AES) программно: чип только передаёт кадры;
 *   * мост в сетевой стек: интерфейс wlan0 - как проводная карта:
 *     кадр Ethernet <-> кадр данных 802.11 (+ заголовок LLC/SNAP);
 *   * присмотр: пропали маяки точки или она нас "выгнала" (deauth) -
 *     переподключиться.
 *
 * Работа с сетью, которой нужно ждать (поиск, подключение), идёт в
 * своём потоке "wlan"; принятые кадры разбирает поток net (wlan_rx).
 * Всё - под g_net_mutex; пока поток wlan ждёт ответа (net_wait), замок
 * отпущен, и поток net успевает принять кадры.
 *
 * Ограничения (честно): только WPA2-PSK (и WPA2/WPA3 в смешанном
 * режиме) с шифром CCMP и открытые сети; WPA3-only (SAE), WEP, WPA1 и
 * корпоративный WPA2 (802.1X) - нет. Скорости 802.11a/b/g (до 54
 * Мбит/с): режимы 802.11n/ac MyOS пока не объявляет.
 */
#include "net.h"
#include "wifi.h"
#include "wlan.h"

/* ================================================================
 * Состояние
 * ================================================================ */

#define WL_MAX_BSS   48

#define SEC_OPEN     0
#define SEC_WEP      1
#define SEC_WPA1     2
#define SEC_WPA2     3        /* PSK, можно подключиться */
#define SEC_WPA23    4        /* WPA2 + WPA3 (переходный режим) - через WPA2 */
#define SEC_WPA3     5        /* только SAE - нельзя */
#define SEC_ENT      6        /* 802.1X (логин/пароль организации) - нельзя */

typedef struct {
    BOOLEAN used;
    UINT8   bssid[6];
    char    ssid[33];
    UINT8   ssid_len;
    UINT8   channel;
    INT32   rssi;             /* дБм */
    UINT16  cap;              /* Capability Information */
    UINT8   sec;
    BOOLEAN group_tkip;       /* общий ключ сети - TKIP (старые роутеры "WPA/WPA2") */
    BOOLEAN mfp_required;     /* требует защиты кадров управления (802.11w) */
    UINT32  rates;            /* маска скоростей (как в wlan.h) */
    UINT8   rsn[64];          /* RSN IE точки, как есть */
    UINT8   rsn_len;
    UINT64  seen_ms;
} WL_BSS;

#define ST_IDLE      0
#define ST_JOIN      1        /* authentication + association */
#define ST_4WAY      2
#define ST_UP        3

#define WL_LOG_N     12

static struct {
    WLAN_HW  *hw;
    NETIF    *nif;
    KTHREAD  *thread;
    BOOLEAN   started;

    WL_BSS    bss[WL_MAX_BSS];

    /* поиск */
    BOOLEAN   scan_req;
    BOOLEAN   scanning;
    UINT32    scan_gen;              /* +1 после каждого поиска */

    /* куда хотим подключиться */
    BOOLEAN   want;
    char      ssid[33];
    UINT8     ssid_len;
    UINT8     pmk[32];
    BOOLEAN   has_pass;
    UINT64    next_try_ms;
    UINT32    tries;

    /* текущая связь */
    UINT8     state;
    WL_BSS    cur;
    UINT16    aid;
    BOOLEAN   got_auth, got_assoc;
    UINT16    auth_status, assoc_status;
    UINT16    kicked;                /* пришёл deauth/disassoc: причина + 1 */
    UINT64    last_beacon_ms;
    INT32     rssi_avg;

    /* ключи */
    WPA_SUPP  wpa;
    BOOLEAN   keys;
    UINT8     tk[16];
    UINT8     gtk[16];
    UINT8     gtk_id;
    BOOLEAN   gtk_ok;
    UINT64    tx_pn;
    UINT64    rx_pn;                 /* защита от повтора: последний номер от точки */
    BOOLEAN   rx_pn_ok;

    /* журнал для команды wifi connect */
    char      log[WL_LOG_N][100];
    UINT32    log_seq;               /* сколько строк записано всего */
    BOOLEAN   fail;                  /* последняя попытка не удалась */
    UINT32    attempt;               /* номер попытки подключения */

    /* счётчики */
    UINT64    rx_mgmt, rx_data, rx_decrypt_fail, rx_replay, rx_no_key, rx_other;
    UINT64    tx_data, tx_fail;
    UINT64    beacons;
} g_wl;

static void wl_log(const char *fmt, ...) __attribute__((format(printf, 1, 2)));

static void wl_log(const char *fmt, ...)
{
    char buf[100];
    va_list ap;

    va_start(ap, fmt);
    kvsnprintf(buf, sizeof(buf), fmt, ap);
    va_end(ap);

    memcpy(g_wl.log[g_wl.log_seq % WL_LOG_N], buf, sizeof(buf));
    g_wl.log_seq++;
    klog("wifi: %s\n", buf);
    net_wake(&g_wl.log_seq);
}

BOOLEAN wlan_present(void)
{
    return g_wl.hw != NULL;
}

static UINTN wl_strlen(const char *s)
{
    UINTN n = 0;

    while (s[n])
        n++;
    return n;
}

static BOOLEAN mac_eq(const UINT8 *a, const UINT8 *b)
{
    return memcmp(a, b, 6) == 0;
}

static const char *sec_name(const WL_BSS *b)
{
    switch (b->sec) {
    case SEC_OPEN:  return "open";
    case SEC_WEP:   return "WEP";
    case SEC_WPA1:  return "WPA";
    case SEC_WPA2:  return b->group_tkip ? "WPA2 (TKIP group)" : "WPA2";
    case SEC_WPA23: return "WPA2/WPA3";
    case SEC_WPA3:  return "WPA3";
    case SEC_ENT:   return "WPA2-Enterprise";
    default:        return "?";
    }
}

/* ================================================================
 * Разбор маяков и ответов на probe
 * ================================================================ */

/* скорость (в единицах 500 кбит/с) -> бит маски */
static UINT32 rate_bit(UINT8 r)
{
    switch (r & 0x7Fu) {
    case 2:   return 1u << 0;
    case 4:   return 1u << 1;
    case 11:  return 1u << 2;
    case 22:  return 1u << 3;
    case 12:  return 1u << 4;
    case 18:  return 1u << 5;
    case 24:  return 1u << 6;
    case 36:  return 1u << 7;
    case 48:  return 1u << 8;
    case 72:  return 1u << 9;
    case 96:  return 1u << 10;
    case 108: return 1u << 11;
    default:  return 0;
    }
}

static BOOLEAN suite_is(const UINT8 *p, UINT8 type)
{
    return p[0] == 0x00 && p[1] == 0x0F && p[2] == 0xAC && p[3] == type;
}

/* RSN IE: шифры и способ входа */
static void parse_rsn(WL_BSS *b, const UINT8 *ie, UINT8 len)
{
    BOOLEAN psk = FALSE, sae = FALSE, dot1x = FALSE;

    b->sec = SEC_WPA2;
    b->group_tkip = FALSE;
    b->mfp_required = FALSE;
    b->rsn_len = (UINT8)(len + 2u > sizeof(b->rsn) ? 0 : len + 2u);
    if (b->rsn_len)
        memcpy(b->rsn, ie - 2, b->rsn_len);

    if (len < 8)
        return;

    b->group_tkip = suite_is(ie + 2, 2);

    UINTN i = 6;
    UINTN pc = (UINTN)(ie[i] | (ie[i + 1] << 8));

    i += 2 + pc * 4;
    if (i + 2 > len)
        return;

    UINTN ac = (UINTN)(ie[i] | (ie[i + 1] << 8));

    i += 2;
    for (UINTN k = 0; k < ac && i + 4 <= len; k++, i += 4) {
        if (suite_is(ie + i, 2))
            psk = TRUE;                           /* PSK-SHA256 (6) - другой MIC, нет */
        else if (suite_is(ie + i, 8))
            sae = TRUE;
        else if (suite_is(ie + i, 1) || suite_is(ie + i, 5))
            dot1x = TRUE;
    }

    if (i + 2 <= len)
        b->mfp_required = (ie[i] & 0x40u) != 0;     /* RSN capabilities: MFPR */

    if (psk && sae)
        b->sec = SEC_WPA23;
    else if (psk)
        b->sec = SEC_WPA2;
    else if (sae)
        b->sec = SEC_WPA3;
    else if (dot1x)
        b->sec = SEC_ENT;
}

/* Маяк или ответ на probe: body - после 24 байт заголовка */
static void wl_rx_beacon(const UINT8 *bssid, const UINT8 *body, UINTN len, INT32 rssi,
                         UINT8 rx_ch)
{
    if (len < 12)
        return;

    WL_BSS n;

    memset(&n, 0, sizeof(n));
    memcpy(n.bssid, bssid, 6);
    n.cap = (UINT16)(body[10] | (body[11] << 8));
    n.channel = rx_ch;
    n.rssi = rssi;
    n.sec = (n.cap & 0x10u) ? SEC_WEP : SEC_OPEN;

    BOOLEAN have_rsn = FALSE, have_wpa1 = FALSE;

    for (UINTN i = 12; i + 2 <= len; ) {

        UINT8 id = body[i], l = body[i + 1];
        const UINT8 *v = body + i + 2;

        if (i + 2u + l > len)
            break;

        switch (id) {
        case 0:                                         /* SSID */
            if (l <= 32) {
                memcpy(n.ssid, v, l);
                n.ssid[l] = '\0';
                n.ssid_len = l;
            }
            break;
        case 1:                                         /* скорости */
        case 50:                                        /* ещё скорости */
            for (UINTN k = 0; k < l; k++)
                n.rates |= rate_bit(v[k]);
            break;
        case 3:                                         /* канал (2,4 ГГц) */
            if (l >= 1)
                n.channel = v[0];
            break;
        case 61:                                        /* HT operation: основной канал */
            if (l >= 1 && rx_ch > 14)
                n.channel = v[0];
            break;
        case 48:
            have_rsn = TRUE;
            parse_rsn(&n, v, l);
            break;
        case 221:
            if (l >= 4 && v[0] == 0x00 && v[1] == 0x50 && v[2] == 0xF2 && v[3] == 1)
                have_wpa1 = TRUE;
            break;
        }

        i += 2u + l;
    }

    if (!have_rsn && have_wpa1)
        n.sec = SEC_WPA1;

    /* своя точка: маяк - "жива", сила сигнала */
    if (g_wl.state != ST_IDLE && mac_eq(bssid, g_wl.cur.bssid)) {
        g_wl.last_beacon_ms = net_now_ms();
        g_wl.beacons++;
        g_wl.rssi_avg = (g_wl.rssi_avg * 7 + rssi) / 8;
        if (g_wl.hw->report_rssi)
            g_wl.hw->report_rssi(g_wl.hw, g_wl.rssi_avg);
    }

    /* в таблицу найденных сетей */
    WL_BSS *slot = NULL, *oldest = &g_wl.bss[0];

    for (UINTN i = 0; i < WL_MAX_BSS; i++) {

        WL_BSS *b = &g_wl.bss[i];

        if (b->used && mac_eq(b->bssid, bssid)) {
            slot = b;
            break;
        }
        if (!b->used && slot == NULL)
            slot = b;
        if (b->seen_ms < oldest->seen_ms)
            oldest = b;
    }

    if (slot == NULL)
        slot = oldest;

    /* скрытая сеть: имя в маяке пустое, в ответе на probe - есть */
    if (slot->used && mac_eq(slot->bssid, bssid) && n.ssid_len == 0 && slot->ssid_len != 0) {
        memcpy(n.ssid, slot->ssid, sizeof(n.ssid));
        n.ssid_len = slot->ssid_len;
    }

    n.used = TRUE;
    n.seen_ms = net_now_ms();
    *slot = n;
}

/* ================================================================
 * Отправка кадров управления
 * ================================================================ */

static void mgmt_hdr(UINT8 *f, UINT8 subtype, const UINT8 *da, const UINT8 *bssid)
{
    memset(f, 0, 24);
    f[0] = (UINT8)(subtype << 4);                 /* тип 0 - управление */
    memcpy(f + 4, da, 6);
    memcpy(f + 10, g_wl.hw->mac, 6);
    memcpy(f + 16, bssid, 6);

    if (!(da[0] & 1u)) {
        /* длительность: SIFS + ACK на базовой скорости */
        UINT16 dur = g_wl.cur.channel > 14 ? 44 : 314;
        f[2] = (UINT8)dur;
        f[3] = (UINT8)(dur >> 8);
    }
}

/* Поддерживаемые скорости: 2,4 ГГц - 1..54, 5 ГГц - 6..54 */
static UINTN put_rates(UINT8 *p, UINT8 ch)
{
    static const UINT8 g[12] = { 0x82, 0x84, 0x8B, 0x96, 0x0C, 0x12, 0x18, 0x24,
                                 0x30, 0x48, 0x60, 0x6C };
    static const UINT8 a[8] = { 0x8C, 0x12, 0x98, 0x24, 0xB0, 0x48, 0x60, 0x6C };
    UINTN n = 0;

    if (ch <= 14) {
        p[n++] = 1;
        p[n++] = 8;
        memcpy(p + n, g, 8);
        n += 8;
        p[n++] = 50;
        p[n++] = 4;
        memcpy(p + n, g + 8, 4);
        n += 4;
    } else {
        p[n++] = 1;
        p[n++] = 8;
        memcpy(p + n, a, 8);
        n += 8;
    }

    return n;
}

static void wl_send_probe(UINT8 ch, const char *ssid, UINT8 ssid_len)
{
    static const UINT8 bcast[6] = { 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF };
    UINT8 f[128];
    UINTN n = 24;

    mgmt_hdr(f, 4, bcast, bcast);

    f[n++] = 0;
    f[n++] = ssid_len;
    memcpy(f + n, ssid, ssid_len);
    n += ssid_len;
    n += put_rates(f + n, ch);

    if (ch <= 14) {
        f[n++] = 3;
        f[n++] = 1;
        f[n++] = ch;
    }

    g_wl.hw->tx(g_wl.hw, f, n, WLAN_TX_MGMT);
}

static void wl_send_auth(void)
{
    UINT8 f[30];

    mgmt_hdr(f, 11, g_wl.cur.bssid, g_wl.cur.bssid);
    f[24] = 0; f[25] = 0;                    /* алгоритм: открытая система */
    f[26] = 1; f[27] = 0;                    /* шаг 1 */
    f[28] = 0; f[29] = 0;                    /* статус */

    g_wl.hw->tx(g_wl.hw, f, sizeof(f), WLAN_TX_MGMT);
}

/* Наш RSN IE: те же шифр группы (CCMP или TKIP), что у точки; для
   себя - CCMP; вход по паролю (PSK) */
static UINTN wl_rsn_ie(UINT8 *p)
{
    memcpy(p, g_wpa_rsn_ie, sizeof(g_wpa_rsn_ie));
    if (g_wl.cur.group_tkip)
        p[7] = 2;
    return sizeof(g_wpa_rsn_ie);
}

static void wl_send_assoc(void)
{
    UINT8 f[256];
    UINTN n = 24;
    UINT16 cap = 0x0001;                               /* ESS */

    mgmt_hdr(f, 0, g_wl.cur.bssid, g_wl.cur.bssid);

    if (g_wl.cur.sec != SEC_OPEN)
        cap |= 0x0010;                                 /* Privacy */
    if (g_wl.cur.channel <= 14)
        cap |= 0x0020;                                 /* короткая преамбула */
    cap |= (UINT16)(g_wl.cur.cap & 0x0400u);           /* короткий слот - как у точки */

    f[n++] = (UINT8)cap;
    f[n++] = (UINT8)(cap >> 8);
    f[n++] = 10;                                       /* listen interval */
    f[n++] = 0;

    f[n++] = 0;
    f[n++] = g_wl.cur.ssid_len;
    memcpy(f + n, g_wl.cur.ssid, g_wl.cur.ssid_len);
    n += g_wl.cur.ssid_len;

    n += put_rates(f + n, g_wl.cur.channel);

    if (g_wl.cur.sec == SEC_WPA2 || g_wl.cur.sec == SEC_WPA23)
        n += wl_rsn_ie(f + n);

    g_wl.hw->tx(g_wl.hw, f, n, WLAN_TX_MGMT);
}

static void wl_send_deauth(UINT16 reason)
{
    UINT8 f[26];

    mgmt_hdr(f, 12, g_wl.cur.bssid, g_wl.cur.bssid);
    f[24] = (UINT8)reason;
    f[25] = (UINT8)(reason >> 8);

    g_wl.hw->tx(g_wl.hw, f, sizeof(f), WLAN_TX_MGMT);
}

/* ================================================================
 * Данные: 802.11 <-> Ethernet
 * ================================================================ */

static const UINT8 g_snap[6] = { 0xAA, 0xAA, 0x03, 0x00, 0x00, 0x00 };

/* Кадр данных точке: заголовок (к точке, ToDS), LLC/SNAP + тип + данные.
   Ключи есть - шифруем CCMP */
static BOOLEAN wl_send_data(const UINT8 *da, UINT16 type, const UINT8 *data, UINTN len,
                            UINT32 flags)
{
    static UINT8 body[NET_FRAME_MAX + 16];
    static UINT8 f[NET_FRAME_MAX + 64];
    UINT8 h[24];

    if (len + 8 > sizeof(body) - 16)
        return FALSE;

    UINT16 dur = g_wl.cur.channel > 14 ? 44 : 314;   /* SIFS + ACK */

    memset(h, 0, sizeof(h));
    h[0] = 0x08;                                  /* данные */
    h[1] = 0x01;                                  /* ToDS */
    h[2] = (UINT8)dur;
    h[3] = (UINT8)(dur >> 8);
    memcpy(h + 4, g_wl.cur.bssid, 6);
    memcpy(h + 10, g_wl.hw->mac, 6);
    memcpy(h + 16, da, 6);

    memcpy(body, g_snap, 6);
    body[6] = (UINT8)(type >> 8);
    body[7] = (UINT8)type;
    memcpy(body + 8, data, len);

    UINTN blen = len + 8, flen;

    if (g_wl.keys) {
        g_wl.tx_pn++;
        flen = ccmp_encrypt(g_wl.tk, h, 24, g_wl.tx_pn, 0, body, blen, f);
    } else {
        memcpy(f, h, 24);
        memcpy(f + 24, body, blen);
        flen = 24 + blen;
    }

    if (!g_wl.hw->tx(g_wl.hw, f, flen, flags)) {
        g_wl.tx_fail++;
        return FALSE;
    }

    g_wl.tx_data++;
    return TRUE;
}

/* NETIF.tx: кадр Ethernet от стека -> в эфир */
static BOOLEAN wl_eth_tx(NETIF *nif, const UINT8 *frame, UINTN len)
{
    (void)nif;

    if (g_wl.state != ST_UP || len < ETH_HLEN)
        return FALSE;

    return wl_send_data(frame, net_get16(frame + 12), frame + ETH_HLEN, len - ETH_HLEN, 0);
}

/* EAPOL от точки: рукопожатие WPA2 */
static void wl_rx_eapol(const UINT8 *p, UINTN len)
{
    static UINT8 reply[512];
    UINTN rl = 0;

    /* первое сообщение точка шлёт сразу за ответом на association -
       поток wlan мог ещё не проснуться, поэтому JOIN тоже годится */
    if (g_wl.cur.sec == SEC_OPEN ||
        !(g_wl.state == ST_4WAY || g_wl.state == ST_UP || (g_wl.state == ST_JOIN && g_wl.got_assoc)))
        return;

    if (!wpa_supp_rx(&g_wl.wpa, p, len, reply, &rl))
        return;

    /* ответ: до установки ключей - открытым текстом; при смене ключей
       на ходу - ещё старым ключом (так требует 802.11) */
    wl_send_data(g_wl.cur.bssid, 0x888E, reply, rl, WLAN_TX_LOWRATE);

    if (g_wl.wpa.state == WPA_DONE &&
        (!g_wl.keys || memcmp(g_wl.tk, g_wl.wpa.ptk + 32, 16) != 0)) {
        memcpy(g_wl.tk, g_wl.wpa.ptk + 32, 16);
        g_wl.keys = TRUE;
        g_wl.tx_pn = 0;
        g_wl.rx_pn_ok = FALSE;
    }

    if (g_wl.wpa.gtk_new) {
        g_wl.wpa.gtk_new = FALSE;
        if (g_wl.wpa.gtk_len >= 16) {
            memcpy(g_wl.gtk, g_wl.wpa.gtk, 16);
            g_wl.gtk_id = g_wl.wpa.gtk_id;
            g_wl.gtk_ok = !g_wl.cur.group_tkip;
        }
    }

    net_wake(&g_wl);
}

/* Полезная нагрузка кадра данных (после LLC/SNAP) - в стек */
static void wl_deliver(const UINT8 *da, const UINT8 *sa, const UINT8 *p, UINTN len)
{
    static UINT8 eth[NET_FRAME_MAX];

    if (len < 8)
        return;

    UINT16 type;

    if (memcmp(p, g_snap, 6) == 0 ||
        (p[0] == 0xAA && p[1] == 0xAA && p[2] == 0x03 && p[3] == 0x00 && p[4] == 0x00 &&
         p[5] == 0xF8)) {
        type = net_get16(p + 6);
    } else {
        g_wl.rx_other++;
        return;                                   /* не IP-мир */
    }

    if (type == 0x888E) {
        wl_rx_eapol(p + 8, len - 8);
        return;
    }

    if (g_wl.state != ST_UP || g_wl.nif == NULL || len - 8 + ETH_HLEN > NET_FRAME_MAX)
        return;

    memcpy(eth, da, 6);
    memcpy(eth + 6, sa, 6);
    net_put16(eth + 12, type);
    memcpy(eth + ETH_HLEN, p + 8, len - 8);

    g_wl.nif->rx_packets++;
    g_wl.nif->rx_bytes += len - 8 + ETH_HLEN;
    net_rx_frame(g_wl.nif, eth, len - 8 + ETH_HLEN);
}

static void wl_rx_data(const UINT8 *f, UINTN len)
{
    static UINT8 plain[NET_FRAME_MAX + 64];
    UINT16 fc = (UINT16)(f[0] | (f[1] << 8));
    UINT8 sub = (UINT8)((fc >> 4) & 0xFu);
    UINTN hl = 24;
    BOOLEAN qos = (sub & 0x8u) != 0;
    BOOLEAN amsdu = FALSE;

    /* от точки к нам: FromDS = 1, ToDS = 0; отправитель - наша точка */
    if ((fc & 0x0300u) != 0x0200u || g_wl.state == ST_IDLE || !mac_eq(f + 10, g_wl.cur.bssid))
        return;

    if (sub & 0x4u)
        return;                                   /* "пустые" кадры (null data) */

    if (qos) {
        if (len < 26)
            return;
        amsdu = (f[24] & 0x80u) != 0;
        hl = 26;
        if (fc & 0x8000u)
            hl += 4;                              /* HT Control */
    }

    if (len <= hl)
        return;

    const UINT8 *da = f + 4, *sa = f + 16;
    const UINT8 *p = f + hl;
    UINTN plen = len - hl;

    /* свой же широковещательный кадр, который точка разослала всем */
    if (mac_eq(sa, g_wl.hw->mac))
        return;

    if (fc & 0x4000u) {                           /* зашифрован */

        if (len < hl + 16) {
            g_wl.rx_decrypt_fail++;
            return;
        }

        UINT8 keyid = (UINT8)(f[hl + 3] >> 6);
        BOOLEAN group = (da[0] & 1u) != 0;
        const UINT8 *key = NULL;

        if (!group && g_wl.keys)
            key = g_wl.tk;
        else if (group && g_wl.gtk_ok && keyid == g_wl.gtk_id)
            key = g_wl.gtk;

        if (key == NULL || len > sizeof(plain)) {
            g_wl.rx_no_key++;
            return;
        }

        UINT64 pn = 0;
        INTN n = ccmp_decrypt(key, f, len, hl, plain, &pn);

        if (n < 0) {
            g_wl.rx_decrypt_fail++;
            return;
        }

        /* повтор старого кадра (атака или дубль после потери ACK) */
        if (!group) {
            if (g_wl.rx_pn_ok && pn <= g_wl.rx_pn) {
                g_wl.rx_replay++;
                return;
            }
            g_wl.rx_pn = pn;
            g_wl.rx_pn_ok = TRUE;
        }

        p = plain;
        plen = (UINTN)n;

    } else if (g_wl.keys) {
        /* после установки ключей точка шлёт открытым только EAPOL */
        if (plen < 8 || net_get16(p + 6) != 0x888E) {
            g_wl.rx_no_key++;
            return;
        }
    }

    g_wl.rx_data++;

    if (!amsdu) {
        wl_deliver(da, sa, p, plen);
        return;
    }

    /* A-MSDU: несколько подкадров "DA SA длина LLC... + добивка до 4" */
    for (UINTN i = 0; i + 14 <= plen; ) {

        UINTN sl = (UINTN)((p[i + 12] << 8) | p[i + 13]);

        if (i + 14 + sl > plen)
            break;

        wl_deliver(p + i, p + i + 6, p + i + 14, sl);
        i += (14 + sl + 3u) & ~3u;
    }
}

/* ================================================================
 * Приём (из потока net, через драйвер)
 * ================================================================ */

void wlan_rx(WLAN_HW *hw, const UINT8 *f, UINTN len, INT32 rssi, UINT8 channel)
{
    (void)hw;

    if (len < 24)
        return;

    UINT16 fc = (UINT16)(f[0] | (f[1] << 8));
    UINT8 type = (UINT8)((fc >> 2) & 3u);
    UINT8 sub = (UINT8)((fc >> 4) & 0xFu);

    if (type == 2) {
        wl_rx_data(f, len);
        return;
    }

    if (type != 0)
        return;

    g_wl.rx_mgmt++;

    const UINT8 *body = f + 24;
    UINTN blen = len - 24;
    BOOLEAN to_me = mac_eq(f + 4, g_wl.hw->mac);
    BOOLEAN from_ap = g_wl.state != ST_IDLE && mac_eq(f + 10, g_wl.cur.bssid);

    switch (sub) {

    case 8:                                           /* маяк */
    case 5:                                           /* ответ на probe */
        wl_rx_beacon(f + 16, body, blen, rssi, channel);
        break;

    case 11:                                          /* authentication */
        if (to_me && from_ap && blen >= 6 && body[2] == 2) {
            g_wl.auth_status = (UINT16)(body[4] | (body[5] << 8));
            g_wl.got_auth = TRUE;
            net_wake(&g_wl);
        }
        break;

    case 1:                                           /* association response */
    case 3:                                           /* reassociation response */
        if (to_me && from_ap && blen >= 6) {
            g_wl.assoc_status = (UINT16)(body[2] | (body[3] << 8));
            g_wl.aid = (UINT16)((body[4] | (body[5] << 8)) & 0x3FFFu);
            g_wl.got_assoc = TRUE;
            net_wake(&g_wl);
        }
        break;

    case 12:                                          /* deauthentication */
    case 10:                                          /* disassociation */
        if (from_ap && (to_me || (f[4] & 1u)) && blen >= 2) {
            g_wl.kicked = (UINT16)((body[0] | (body[1] << 8)) + 1u);
            net_wake(&g_wl);
        }
        break;

    default:
        break;
    }
}

/* ================================================================
 * Поиск сетей
 * ================================================================ */

/* Каналы: 2,4 ГГц 1..13; 5 ГГц 36..64, 100..144, 149..165. На каналах
   с радарами (DFS: 52..144) спрашивать нельзя - только слушаем маяки */
static const UINT8 g_scan_ch[] = {
    1, 6, 11, 2, 7, 12, 3, 8, 13, 4, 9, 5, 10,
    36, 40, 44, 48, 149, 153, 157, 161, 165,
    52, 56, 60, 64, 100, 104, 108, 112, 116, 120, 124, 128, 132, 136, 140, 144
};

static UINT32 g_scan_dummy;

static void wl_do_scan(void)
{
    UINT8 back = g_wl.cur.channel;

    g_wl.scanning = TRUE;
    g_wl.hw->set_scan(g_wl.hw, TRUE);

    for (UINTN i = 0; i < sizeof(g_scan_ch); i++) {

        UINT8 ch = g_scan_ch[i];
        BOOLEAN dfs = ch >= 52 && ch <= 144;

        if (!g_wl.hw->set_channel(g_wl.hw, ch))
            continue;

        if (!dfs) {
            wl_send_probe(ch, "", 0);
            /* ищем конкретную сеть - спросить и по имени (скрытые сети) */
            if (g_wl.want && g_wl.ssid_len)
                wl_send_probe(ch, g_wl.ssid, g_wl.ssid_len);
        }

        /* ждать ответов: поток net принимает кадры, пока мы спим */
        net_wait(&g_scan_dummy, "wifi scan", dfs ? 130 : 110);
    }

    g_wl.hw->set_scan(g_wl.hw, FALSE);
    g_wl.scanning = FALSE;

    /* были подключены - назад на свой канал; маяки своей точки мы пока
       не слышали не по её вине */
    if (g_wl.state != ST_IDLE && back) {
        g_wl.hw->set_channel(g_wl.hw, back);
        g_wl.last_beacon_ms = net_now_ms();
    }

    g_wl.scan_gen++;
    net_wake(&g_wl.scan_gen);
}

/* ================================================================
 * Подключение
 * ================================================================ */

static const char *status_text(UINT16 s)
{
    switch (s) {
    case 1:  return "unspecified failure";
    case 10: return "capabilities not supported";
    case 12: return "denied (access point refused)";
    case 13: return "authentication algorithm not supported";
    case 17: return "access point is full (too many clients)";
    case 18: return "data rates not supported";
    case 27: return "802.11n (HT) required - MyOS is 802.11g only";
    case 30: return "rejected temporarily, try later";
    case 31: return "robust management frame policy violation";
    case 40: return "invalid information element";
    case 41: return "invalid group cipher";
    case 42: return "invalid pairwise cipher";
    case 43: return "invalid key management (AKM)";
    case 45: return "cipher rejected";
    case 53: return "invalid PMKID";
    default: return "see IEEE 802.11 status codes";
    }
}

static const char *reason_text(UINT16 r)
{
    switch (r) {
    case 1:  return "unspecified";
    case 2:  return "previous authentication no longer valid";
    case 3:  return "access point is leaving";
    case 4:  return "inactivity";
    case 6:
    case 7:  return "frame from a not connected client";
    case 8:  return "access point is leaving the network";
    case 14: return "message integrity failure";
    case 15: return "4-way handshake timeout (wrong password?)";
    case 16: return "group key handshake timeout";
    case 23: return "802.1X authentication failed";
    default: return "see IEEE 802.11 reason codes";
    }
}

/* Лучшая (самая сильная) точка с нашим именем, виденная недавно */
static WL_BSS *wl_find_target(UINT64 max_age_ms)
{
    WL_BSS *best = NULL;
    UINT64 now = net_now_ms();

    for (UINTN i = 0; i < WL_MAX_BSS; i++) {

        WL_BSS *b = &g_wl.bss[i];

        if (!b->used || b->ssid_len != g_wl.ssid_len ||
            memcmp(b->ssid, g_wl.ssid, g_wl.ssid_len) != 0 || now - b->seen_ms > max_age_ms)
            continue;

        if (best == NULL || b->rssi > best->rssi)
            best = b;
    }

    return best;
}

/* Ждать флага (выставит wlan_rx из потока net), не дольше ms */
static BOOLEAN wl_wait_flag(volatile BOOLEAN *flag, UINT64 ms)
{
    UINT64 end = net_now_ms() + ms;

    while (!*flag && !g_wl.kicked) {
        UINT64 now = net_now_ms();
        if (now >= end)
            break;
        net_wait(&g_wl, "wifi join", end - now);
    }

    return *flag;
}

static void wl_link_down(void)
{
    BOOLEAN was = g_wl.state == ST_UP;

    g_wl.state = ST_IDLE;
    g_wl.keys = FALSE;
    g_wl.gtk_ok = FALSE;
    g_wl.hw->set_link(g_wl.hw, FALSE, 0, 0);
    g_wl.hw->set_bssid(g_wl.hw, NULL);

    if (g_wl.nif && was)
        net_link_changed(g_wl.nif, FALSE);
}

static void wl_give_up(UINT64 retry_ms)
{
    g_wl.fail = TRUE;
    g_wl.next_try_ms = net_now_ms() + retry_ms;
    wl_link_down();
    net_wake(&g_wl.log_seq);
}

static void wl_do_connect(void)
{
    g_wl.attempt++;
    g_wl.fail = FALSE;
    g_wl.kicked = 0;
    g_wl.got_auth = FALSE;
    g_wl.got_assoc = FALSE;

    /* 1. найти точку: сначала в недавних результатах поиска */
    WL_BSS *t = wl_find_target(20000);

    if (t == NULL) {
        wl_log("searching for \"%s\"...", g_wl.ssid);
        wl_do_scan();
        t = wl_find_target(20000);
    }

    if (t == NULL) {
        wl_log("network \"%s\" not found (out of range, or the name has a typo?)", g_wl.ssid);
        wl_give_up(15000);
        return;
    }

    g_wl.cur = *t;

    switch (t->sec) {
    case SEC_OPEN:
        break;
    case SEC_WPA2:
    case SEC_WPA23:
        if (!g_wl.has_pass) {
            wl_log("\"%s\" needs a password: wifi connect <name> <password>", g_wl.ssid);
            g_wl.want = FALSE;
            wl_give_up(0);
            return;
        }
        if (t->mfp_required) {
            wl_log("\"%s\" requires protected management frames (802.11w) - not supported yet",
                   g_wl.ssid);
            g_wl.want = FALSE;
            wl_give_up(0);
            return;
        }
        break;
    default:
        wl_log("\"%s\" uses %s - MyOS supports only open and WPA2-Personal networks",
               g_wl.ssid, sec_name(t));
        g_wl.want = FALSE;
        wl_give_up(0);
        return;
    }

    wl_log("connecting to \"%s\" (%02x:%02x:%02x:%02x:%02x:%02x, channel %u, %d dBm, %s)...",
           g_wl.ssid, t->bssid[0], t->bssid[1], t->bssid[2], t->bssid[3], t->bssid[4],
           t->bssid[5], t->channel, (int)t->rssi, sec_name(t));

    /* 2. канал и точка (драйвер заодно калибрует передатчик) */
    g_wl.state = ST_JOIN;
    g_wl.rssi_avg = t->rssi;
    g_wl.hw->set_channel(g_wl.hw, g_wl.cur.channel);
    g_wl.hw->set_bssid(g_wl.hw, g_wl.cur.bssid);

    /* 3. authentication */
    BOOLEAN ok = FALSE;

    for (UINTN i = 0; i < 4 && !ok && !g_wl.kicked; i++) {
        g_wl.got_auth = FALSE;
        wl_send_auth();
        ok = wl_wait_flag(&g_wl.got_auth, 300);
    }

    if (!ok) {
        wl_log("no answer from the access point (authentication) - too far away?");
        wl_give_up(10000);
        return;
    }

    if (g_wl.auth_status != 0) {
        wl_log("access point refused authentication: status %u (%s)", g_wl.auth_status,
               status_text(g_wl.auth_status));
        wl_give_up(15000);
        return;
    }

    /* 4. association (сторона WPA2 готова заранее: первое сообщение
       рукопожатия придёт сразу за ответом) */
    if (g_wl.cur.sec != SEC_OPEN) {
        wpa_supp_init(&g_wl.wpa, g_wl.pmk, g_wl.hw->mac, g_wl.cur.bssid);
        g_wl.wpa.rsn_ie_len = (UINT8)wl_rsn_ie(g_wl.wpa.rsn_ie);
    }

    ok = FALSE;

    for (UINTN i = 0; i < 3 && !ok && !g_wl.kicked; i++) {
        g_wl.got_assoc = FALSE;
        wl_send_assoc();
        ok = wl_wait_flag(&g_wl.got_assoc, 500);
    }

    if (!ok) {
        wl_log("no answer from the access point (association)");
        wl_give_up(10000);
        return;
    }

    if (g_wl.assoc_status != 0) {
        wl_log("access point refused association: status %u (%s)", g_wl.assoc_status,
               status_text(g_wl.assoc_status));
        wl_give_up(15000);
        return;
    }

    wl_log("associated (AID %u)", g_wl.aid);
    g_wl.hw->set_link(g_wl.hw, TRUE, g_wl.aid, g_wl.cur.rates);
    g_wl.last_beacon_ms = net_now_ms();

    /* 5. WPA2: точка начинает 4-стороннее рукопожатие сама */
    if (g_wl.cur.sec != SEC_OPEN) {

        g_wl.state = ST_4WAY;

        UINT64 end = net_now_ms() + 6000;

        while (!(g_wl.keys && g_wl.wpa.state == WPA_DONE) && !g_wl.kicked &&
               net_now_ms() < end)
            net_wait(&g_wl, "wifi 4-way", 200);

        if (!(g_wl.keys && g_wl.wpa.state == WPA_DONE)) {

            /* Неверный пароль видно так: точка прислала 1/4, мы ответили
               2/4 с подписью по нашему ключу, а 3/4 так и не пришло (точка
               нашу подпись не приняла) - или пришло с чужой подписью */
            BOOLEAN bad_pass = g_wl.wpa.mic_failures > 0 || g_wl.wpa.state == WPA_WAIT_M3;

            if (bad_pass)
                wl_log("WRONG PASSWORD for \"%s\"? The access point did not accept our key.",
                       g_wl.ssid);
            else if (g_wl.kicked)
                wl_log("access point dropped us during the handshake: reason %u (%s)",
                       g_wl.kicked - 1u, reason_text((UINT16)(g_wl.kicked - 1u)));
            else
                wl_log("WPA2 handshake did not start (no key messages from the access point)");

            if (!g_wl.kicked)
                wl_send_deauth(3);

            /* неверный пароль - не долбить точку бесконечно */
            if (bad_pass)
                g_wl.want = FALSE;

            wl_give_up(15000);
            return;
        }

        wl_log("WPA2 keys installed%s", g_wl.cur.group_tkip ?
               " (note: this router's group cipher is TKIP - broadcasts are not decrypted)" : "");
    }

    /* 6. связь есть - интерфейс вверх, дальше DHCP (поток net) */
    g_wl.state = ST_UP;
    g_wl.tries = 0;
    g_wl.last_beacon_ms = net_now_ms();

    if (g_wl.nif) {
        g_wl.nif->speed_mbps = 54;
        net_link_changed(g_wl.nif, TRUE);
    }

    wl_log("connected to \"%s\" - link UP, asking for an address (DHCP)...", g_wl.ssid);
}

/* ================================================================
 * Поток wlan
 * ================================================================ */

static void wl_thread(void *arg)
{
    (void)arg;

    kmutex_lock(&g_net_mutex);

    for (;;) {

        UINT64 now = net_now_ms();

        if (g_wl.scan_req) {
            g_wl.scan_req = FALSE;
            wl_do_scan();
            continue;
        }

        /* точка нас "выгнала" */
        if (g_wl.kicked && g_wl.state == ST_UP) {
            UINT16 rs = (UINT16)(g_wl.kicked - 1u);
            g_wl.kicked = 0;
            wl_log("disconnected by the access point: reason %u (%s)", rs, reason_text(rs));
            wl_link_down();
            g_wl.next_try_ms = now + 2000;
            continue;
        }

        /* маяков точки нет 10 с - связь потеряна */
        if (g_wl.state == ST_UP && !g_wl.scanning && now - g_wl.last_beacon_ms > 10000) {
            wl_log("lost the access point (no beacons for 10 s) - reconnecting");
            wl_link_down();
            g_wl.next_try_ms = now + 1000;
            continue;
        }

        if (g_wl.want && g_wl.state == ST_IDLE && now >= g_wl.next_try_ms) {
            wl_do_connect();
            continue;
        }

        net_wait(&g_wl, "wifi idle", 500);
    }
}

/* ================================================================
 * Интерфейс wlan0
 * ================================================================ */

static void wl_nif_poll(NETIF *nif)
{
    (void)nif;

    if (g_wl.hw && g_wl.hw->poll)
        g_wl.hw->poll(g_wl.hw);
}

/* Адаптер включён: интерфейс wlan0 и поток wlan */
BOOLEAN wlan_use_hw(SIMPLE_TEXT_OUTPUT_INTERFACE *out, WLAN_HW *hw)
{
    if (g_wl.hw != NULL)
        return g_wl.hw == hw;

    g_wl.hw = hw;

    NETIF *nif = net_if_add("wlan", hw->driver, hw->model, hw->mac, wl_eth_tx, NULL);

    if (nif) {
        nif->poll = wl_nif_poll;
        nif->poll_fast = TRUE;
        nif->irq_mode = "polling (2 ms)";
        nif->cfg = NET_CFG_DHCP;
        nif->link = FALSE;
        g_wl.nif = nif;
    }

    g_wl.thread = kthread_create("wlan", wl_thread, NULL, 16);
    g_wl.started = g_wl.thread != NULL;

    kprintf(out, "  step 6/6: interface %s ready (MAC %02x:%02x:%02x:%02x:%02x:%02x)\n",
            nif ? nif->name : "?", hw->mac[0], hw->mac[1], hw->mac[2], hw->mac[3],
            hw->mac[4], hw->mac[5]);
    return TRUE;
}

/* Найти адаптер и включить его (печатая шаги) */
BOOLEAN wlan_up(SIMPLE_TEXT_OUTPUT_INTERFACE *out)
{
    if (g_wl.hw != NULL)
        return TRUE;

    for (UINTN nth = 0; nth < 8; nth++) {

        UINT8 b, d, f;

        if (!pci_find_class(0x02, 0x80, -1, nth, &b, &d, &f))
            break;

        UINT32 id = pci_config_read32(b, d, f, 0);
        UINT16 ven = (UINT16)id, dev = (UINT16)(id >> 16);

        if (ven != 0x10EC || (dev != 0xC821 && dev != 0xB821))
            continue;

        kprintf(out, "Starting Wi-Fi: Realtek RTL8821CE at PCI %u:%u.%u\n", b, d, f);

        WLAN_HW *hw = NULL;

        if (!rtw8821c_attach(b, d, f, out, &hw)) {
            print(out, "Wi-Fi did not start. Please take a photo of these lines - they tell\n"
                       "exactly which step failed ('wifi debug' shows more).\n");
            return FALSE;
        }

        return wlan_use_hw(out, hw);
    }

    print(out, "No supported Wi-Fi adapter: MyOS has a radio driver only for the Realtek\n"
               "RTL8821CE (see 'wifi' for what is in this computer; 'wifi sim' starts a\n"
               "software test access point).\n");
    return FALSE;
}

/* ================================================================
 * Команды
 * ================================================================ */

static void print_ssid(SIMPLE_TEXT_OUTPUT_INTERFACE *out, const WL_BSS *b)
{
    if (b->ssid_len == 0 || b->ssid[0] == '\0') {
        print(out, "(hidden network)");
        return;
    }

    /* не-ASCII (кириллица, эмодзи) шрифт консоли не покажет */
    for (UINTN i = 0; i < b->ssid_len; i++) {
        char c = b->ssid[i];
        kprintf(out, "%c", (c >= 32 && c < 127) ? c : '?');
    }
}

void wlan_cmd_scan(SIMPLE_TEXT_OUTPUT_INTERFACE *out)
{
    kmutex_lock(&g_net_mutex);

    if (!wlan_up(out) || !g_wl.started) {
        kmutex_unlock(&g_net_mutex);
        return;
    }

    print(out, "Scanning 2.4 and 5 GHz channels (about 5 seconds)...\n");

    UINT32 gen = g_wl.scan_gen;
    UINT64 t0 = net_now_ms();

    g_wl.scan_req = TRUE;
    net_wake(&g_wl);

    while (g_wl.scan_gen == gen && net_now_ms() - t0 < 20000)
        net_wait(&g_wl.scan_gen, "wifi scan", 200);

    /* отсортировать по силе сигнала (свежие: видны в этом поиске) */
    WL_BSS *list[WL_MAX_BSS];
    UINTN n = 0;

    for (UINTN i = 0; i < WL_MAX_BSS; i++)
        if (g_wl.bss[i].used && g_wl.bss[i].seen_ms >= t0)
            list[n++] = &g_wl.bss[i];

    for (UINTN i = 1; i < n; i++)
        for (UINTN k = i; k > 0 && list[k]->rssi > list[k - 1]->rssi; k--) {
            WL_BSS *x = list[k];
            list[k] = list[k - 1];
            list[k - 1] = x;
        }

    if (n == 0) {
        print(out, "No networks found. The radio may not be receiving: please send a photo of\n"
                   "'wifi debug'.\n");
        kmutex_unlock(&g_net_mutex);
        return;
    }

    kprintf(out, "Found %u network(s):\n  signal   ch  security           name\n", (UINT32)n);

    for (UINTN i = 0; i < n; i++) {

        WL_BSS *b = list[i];
        const char *bars = b->rssi >= -55 ? "####" : b->rssi >= -67 ? "### " :
                           b->rssi >= -75 ? "##  " : "#   ";

        kprintf(out, "  %s %3d %3u  %-18s ", bars, (int)b->rssi, b->channel, sec_name(b));
        print_ssid(out, b);
        print(out, "\n");
    }

    print(out, "Connect: wifi connect <name> <password>   (a name with spaces: in quotes)\n");
    kmutex_unlock(&g_net_mutex);
}

void wlan_cmd_connect(SIMPLE_TEXT_OUTPUT_INTERFACE *out, const char *ssid, const char *pass)
{
    UINTN sl = wl_strlen(ssid), pl = pass ? wl_strlen(pass) : 0;

    if (sl == 0 || sl > 32) {
        print(out, "network name must be 1..32 characters\n");
        return;
    }

    if (pass && (pl < 8 || pl > 63)) {
        print(out, "a WPA2 password is 8..63 characters\n");
        return;
    }

    /* ключ из пароля (PBKDF2, 4096 раундов) - до замка: это долго */
    UINT8 pmk[32];

    if (pass)
        pbkdf2_sha1((const UINT8 *)pass, pl, (const UINT8 *)ssid, sl, 4096, pmk, 32);

    wlan_cmd_connect_key(out, ssid, pass ? pmk : NULL, TRUE);
}

/* Что сейчас выбрано для подключения (для wifi save): имя сети и
   ключ PMK (не пароль - его мы не храним). FALSE - никуда не
   подключаемся. */
BOOLEAN wlan_current(char *ssid, UINTN cap, UINT8 pmk[32], BOOLEAN *has_pass)
{
    BOOLEAN ok = FALSE;

    kmutex_lock(&g_net_mutex);

    if (g_wl.want && g_wl.ssid_len > 0 && (UINTN)g_wl.ssid_len + 1 <= cap) {
        memcpy(ssid, g_wl.ssid, g_wl.ssid_len);
        ssid[g_wl.ssid_len] = '\0';
        memcpy(pmk, g_wl.pmk, 32);
        *has_pass = g_wl.has_pass;
        ok = TRUE;
    }

    kmutex_unlock(&g_net_mutex);
    return ok;
}

/* Есть ли адаптер, для которого у нас есть драйвер (не включая его) */
BOOLEAN wlan_hw_available(void)
{
    if (g_wl.hw != NULL)
        return TRUE;

    for (UINTN nth = 0; nth < 8; nth++) {

        UINT8 b, d, f;

        if (!pci_find_class(0x02, 0x80, -1, nth, &b, &d, &f))
            break;

        UINT32 id = pci_config_read32(b, d, f, 0);

        if ((UINT16)id == 0x10EC && ((id >> 16) == 0xC821 || (id >> 16) == 0xB821))
            return TRUE;
    }

    return FALSE;
}

/* Подключиться по готовому ключу (pmk = NULL - открытая сеть).
   wait - ждать результата, печатая журнал (команда); FALSE - только
   включить адаптер и попросить поток wlan подключаться (при загрузке:
   он сам будет пробовать, пока сеть не найдётся). */
void wlan_cmd_connect_key(SIMPLE_TEXT_OUTPUT_INTERFACE *out, const char *ssid, const UINT8 *pmk,
                          BOOLEAN wait)
{
    UINTN sl = wl_strlen(ssid);

    if (sl == 0 || sl > 32)
        return;

    kmutex_lock(&g_net_mutex);

    if (!wlan_up(out) || !g_wl.started) {
        kmutex_unlock(&g_net_mutex);
        return;
    }

    /* идёт попытка подключения - дать ей закончиться */
    for (UINT64 t = net_now_ms(); (g_wl.state == ST_JOIN || g_wl.state == ST_4WAY) &&
                                  net_now_ms() - t < 15000; )
        net_wait(&g_wl.log_seq, "wifi busy", 200);

    /* уже подключены (к этой или другой сети) - отключиться */
    if (g_wl.state != ST_IDLE) {
        wl_send_deauth(3);
        wl_link_down();
    }

    memcpy(g_wl.ssid, ssid, sl);
    g_wl.ssid[sl] = '\0';
    g_wl.ssid_len = (UINT8)sl;
    g_wl.has_pass = pmk != NULL;
    if (pmk)
        memcpy(g_wl.pmk, pmk, 32);
    g_wl.want = TRUE;
    g_wl.next_try_ms = 0;

    if (!wait) {
        net_wake(&g_wl);
        kmutex_unlock(&g_net_mutex);
        return;
    }

    UINT32 seq = g_wl.log_seq;
    UINT32 att = g_wl.attempt;
    UINT64 t0 = net_now_ms();
    BOOLEAN done = FALSE;

    net_wake(&g_wl);

    /* печатать журнал подключения, пока не станет ясно, чем кончилось */
    while (!done && net_now_ms() - t0 < 45000) {

        net_wait(&g_wl.log_seq, "wifi connect", 300);

        if (g_wl.log_seq - seq > WL_LOG_N)
            seq = g_wl.log_seq - WL_LOG_N;

        while (seq != g_wl.log_seq) {
            kprintf(out, "  %s\n", g_wl.log[seq % WL_LOG_N]);
            seq++;
        }

        if (g_wl.attempt != att && g_wl.state == ST_IDLE && g_wl.fail)
            done = TRUE;                             /* попытка не удалась */

        if (g_wl.state == ST_UP && g_wl.nif && g_wl.nif->up)
            done = TRUE;                             /* адрес получен */
    }

    if (g_wl.state == ST_UP && g_wl.nif && g_wl.nif->up) {
        char ip[20], gw[20];
        net_fmt_ip(ip, sizeof(ip), g_wl.nif->ip);
        net_fmt_ip(gw, sizeof(gw), g_wl.nif->gw);
        kprintf(out, "Wi-Fi is up: %s address %s, router %s. Try: ping archlinux.org\n",
                g_wl.nif->name, ip, gw);
    } else if (g_wl.state == ST_UP) {
        print(out, "Connected, but no address from DHCP yet - see 'ifconfig' in a few seconds.\n");
    } else if (g_wl.want) {
        print(out, "Not connected yet - MyOS keeps trying in the background ('wifi' shows the\n"
                   "state, 'wifi disconnect' stops).\n");
    }

    kmutex_unlock(&g_net_mutex);
}

void wlan_cmd_disconnect(SIMPLE_TEXT_OUTPUT_INTERFACE *out)
{
    kmutex_lock(&g_net_mutex);

    g_wl.want = FALSE;

    if (g_wl.hw && g_wl.state != ST_IDLE) {
        wl_send_deauth(3);
        wl_link_down();
        wl_log("disconnected");
        print(out, "Wi-Fi disconnected.\n");
    } else {
        print(out, "Wi-Fi was not connected.\n");
    }

    kmutex_unlock(&g_net_mutex);
}

void wlan_cmd_status(SIMPLE_TEXT_OUTPUT_INTERFACE *out, BOOLEAN debug)
{
    kmutex_lock(&g_net_mutex);

    if (g_wl.hw == NULL) {
        kmutex_unlock(&g_net_mutex);
        return;
    }

    static const char *st[] = { "not connected", "joining", "WPA2 handshake", "connected" };

    kprintf(out, "Wi-Fi %s: %s", g_wl.nif ? g_wl.nif->name : "?", st[g_wl.state & 3u]);

    if (g_wl.state != ST_IDLE) {
        print(out, " to \"");
        print_ssid(out, &g_wl.cur);
        kprintf(out, "\" (%02x:%02x:%02x:%02x:%02x:%02x), channel %u, signal %d dBm, %s",
                g_wl.cur.bssid[0], g_wl.cur.bssid[1], g_wl.cur.bssid[2], g_wl.cur.bssid[3],
                g_wl.cur.bssid[4], g_wl.cur.bssid[5], g_wl.cur.channel, (int)g_wl.rssi_avg,
                sec_name(&g_wl.cur));
    } else if (g_wl.want) {
        kprintf(out, " (will retry \"%s\")", g_wl.ssid);
    }
    print(out, "\n");

    g_wl.hw->info(g_wl.hw, out, debug);

    if (debug) {
        kprintf(out, "  802.11: mgmt rx %llu, beacons from AP %llu, data rx %llu (no key %llu, "
                     "decrypt fail %llu, replay %llu, other %llu), data tx %llu (fail %llu)\n",
                g_wl.rx_mgmt, g_wl.beacons, g_wl.rx_data, g_wl.rx_no_key, g_wl.rx_decrypt_fail,
                g_wl.rx_replay, g_wl.rx_other, g_wl.tx_data, g_wl.tx_fail);

        UINT32 from = g_wl.log_seq > WL_LOG_N ? g_wl.log_seq - WL_LOG_N : 0;

        if (from != g_wl.log_seq)
            print(out, "  recent events:\n");
        for (UINT32 i = from; i != g_wl.log_seq; i++)
            kprintf(out, "    %s\n", g_wl.log[i % WL_LOG_N]);
    }

    kmutex_unlock(&g_net_mutex);
}
