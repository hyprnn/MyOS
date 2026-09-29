/*
 * drivers/usbnet.c - сеть через USB: "USB-модем" телефона (раздача
 * интернета по кабелю) и USB-Ethernet-переходники. Часть MyOS;
 * USB - drivers/usb.c, сетевой стек - net/.
 *
 * Зачем: встроенный Wi-Fi ноутбука - самая трудная часть любой ОС
 * (у каждого чипа своя закрытая прошивка и свой язык команд; плюс
 * шифрование WPA2). Телефон же, подключённый кабелем, в режиме
 * "USB-модем" притворяется сетевой картой стандартного USB-класса -
 * и через него MyOS выходит в Интернет по Wi-Fi или мобильной сети
 * ТЕЛЕФОНА. Протоколы (все три - "Ethernet поверх USB"):
 *
 *   CDC-ECM   - стандарт USB: каждый bulk-пакет = один кадр
 *               Ethernet. Многие USB-Ethernet-переходники, модемы,
 *               часть телефонов; QEMU -device usb-net (конфигурация 1);
 *   RNDIS     - вариант Microsoft: перед кадром заголовок 44 байта,
 *               настройка - сообщениями через управляющую точку.
 *               Так делает Android ("USB-модем") на большинстве
 *               телефонов; QEMU usb-net (конфигурация 2);
 *   CDC-NCM   - новый стандарт: несколько кадров в одном "блоке"
 *               (NTB). Новые Android и iPhone (iOS 17+). В QEMU такого
 *               устройства нет - проверено только по спецификации.
 *
 * Как устроено:
 *   * поток usb при подключении (kx_enum_device) спрашивает
 *     kx_net_probe: перебираем ВСЕ конфигурации устройства (у
 *     телефонов их бывает несколько), выбираем лучший протокол,
 *     SET_CONFIGURATION, настраиваем две bulk-точки;
 *   * приём: на bulk IN всегда стоит один запрос на 4 КиБ - это
 *     "труба" из drivers/usbhid.c с ролью KX_ROLE_NET; пришедшее
 *     разбирается прямо в прерывании (kx_net_report) и кадры уходят
 *     в очередь сетевого стека;
 *   * отправка: поток net под g_usb_mutex - синхронный kx_bulk;
 *   * регистрация интерфейса usb0 в стеке - из потока net
 *     (usbnet_sync): поток usb никогда не берёт g_net_mutex.
 */
#include "../net/net.h"

#define KX_MAX_NET   2

#define UN_ECM       1
#define UN_RNDIS     2
#define UN_NCM       3

#define UN_BUF       4096u          /* приём: столько за один запрос */

typedef struct {
    BOOLEAN used;             /* занято устройством USB */
    volatile BOOLEAN ready;   /* настроено, можно регистрировать в стеке */
    UINT8   dev;              /* индекс в g_kx_devs */
    UINT8   proto;            /* UN_* */
    UINT8   ctrl_iface, data_iface, data_alt;
    UINT8   in_addr, out_addr;
    UINT8   in_dci, out_dci;
    UINT16  in_mps, out_mps;
    KX_RING out_ring;
    INTN    pipe;             /* труба bulk IN в g_kx_hid */
    UINT64  out_buf;          /* страница для отправки */
    UINT8   mac[6];
    UINT32  serial;           /* номер подключения */
    UINT32  rndis_id;         /* номер запроса RNDIS */
    UINT16  ncm_seq;
    char    model[48];
    const char *note;

    /* сторона сети (только поток net) */
    NETIF  *nif;
    UINT32  nif_serial;

    volatile UINT64 rx_frames, rx_bad;
    UINT64  tx_frames, tx_errors;
} KX_NET;

static KX_NET g_kx_net[KX_MAX_NET];
static UINT32 g_net_serial = 0;

/* Отладка/проверка: предпочитать RNDIS, даже если есть ECM
   ("net usb rndis"; в QEMU у usb-net есть оба) */
BOOLEAN g_usbnet_prefer_rndis = FALSE;

static const char *un_proto_name(UINT8 p)
{
    return p == UN_ECM ? "usb-ecm" : p == UN_RNDIS ? "usb-rndis" : p == UN_NCM ? "usb-ncm" : "?";
}

static UINT32 le32(volatile const UINT8 *p)
{
    return (UINT32)p[0] | ((UINT32)p[1] << 8) | ((UINT32)p[2] << 16) | ((UINT32)p[3] << 24);
}

static UINT16 le16(volatile const UINT8 *p)
{
    return (UINT16)(p[0] | (p[1] << 8));
}

static void put_le32(volatile UINT8 *p, UINT32 v)
{
    p[0] = (UINT8)v;
    p[1] = (UINT8)(v >> 8);
    p[2] = (UINT8)(v >> 16);
    p[3] = (UINT8)(v >> 24);
}

static void put_le16(volatile UINT8 *p, UINT16 v)
{
    p[0] = (UINT8)v;
    p[1] = (UINT8)(v >> 8);
}

/* ================================================================
 * Поиск сетевой функции в конфигурации
 * ================================================================ */

typedef struct {
    BOOLEAN ok;
    UINT8   cfg_value;
    UINT8   proto;
    UINT8   ctrl_iface, data_iface, data_alt;
    UINT8   in_addr, out_addr;
    UINT16  in_mps, out_mps;
    UINT8   in_burst, out_burst;
    UINT8   mac_str;           /* iMACAddress (ECM/NCM) */
} UN_CAND;

/* Разобрать одну конфигурацию (cfg, total байт) */
static void un_parse_cfg(volatile const UINT8 *cfg, UINTN total, UN_CAND *c)
{
    UINT8 cur_if = 0xFF, cur_alt = 0, cur_class = 0;
    INTN ctrl_proto = 0;
    UINT8 ctrl_if = 0xFF, union_data = 0xFF, mac_str = 0;
    /* кандидат data-интерфейса с двумя bulk-точками */
    UINT8 dif = 0xFF, dalt = 0, din = 0, dout = 0, dinb = 0, doutb = 0;
    UINT16 dinm = 0, doutm = 0;
    UINT8 cur_in = 0, cur_out = 0, cur_inb = 0, cur_outb = 0, last = 0;
    UINT16 cur_inm = 0, cur_outm = 0;

    c->ok = FALSE;

    for (UINTN off = 0; off + 2u <= total; ) {

        UINT8 dl = cfg[off];
        UINT8 dt = cfg[off + 1];

        if (dl < 2)
            break;

        if (dt == 4 && off + 9u <= total) {

            /* закончить предыдущий интерфейс данных */
            if (cur_class == 0x0A && cur_in && cur_out && dif == 0xFF) {
                dif = cur_if; dalt = cur_alt;
                din = cur_in; dout = cur_out; dinm = cur_inm; doutm = cur_outm;
                dinb = cur_inb; doutb = cur_outb;
            }

            cur_if = cfg[off + 2];
            cur_alt = cfg[off + 3];
            cur_class = cfg[off + 5];
            cur_in = cur_out = 0;
            cur_inb = cur_outb = 0;
            cur_inm = cur_outm = 0;

            UINT8 sub = cfg[off + 6], pr = cfg[off + 7];

            if (ctrl_proto == 0) {
                if (cur_class == 2 && sub == 6)
                    ctrl_proto = UN_ECM;
                else if (cur_class == 2 && sub == 0x0D)
                    ctrl_proto = UN_NCM;
                else if ((cur_class == 2 && sub == 2 && pr == 0xFF) ||
                         (cur_class == 0xE0 && sub == 1 && pr == 3) ||
                         (cur_class == 0xEF && sub == 4 && pr == 1))
                    ctrl_proto = UN_RNDIS;
                if (ctrl_proto != 0)
                    ctrl_if = cur_if;
            }

        } else if (dt == 0x24 && off + 3u <= total && cur_if == ctrl_if) {

            /* функциональные дескрипторы CDC */
            UINT8 st = cfg[off + 2];

            if (st == 0x06 && dl >= 5)            /* Union: главный, подчинённый */
                union_data = cfg[off + 4];
            if (st == 0x0F && dl >= 4)            /* Ethernet: строка с MAC */
                mac_str = cfg[off + 3];

        } else if (dt == 5 && off + 7u <= total) {

            UINT8 a = cfg[off + 2];
            UINT8 at = cfg[off + 3] & 3u;
            UINT16 mp = (UINT16)((cfg[off + 4] | (cfg[off + 5] << 8)) & 0x7FFu);

            last = 0;
            if (at == 2 && (a & 0x80u) && cur_in == 0) {
                cur_in = a; cur_inm = mp; last = 1;
            } else if (at == 2 && !(a & 0x80u) && cur_out == 0) {
                cur_out = a; cur_outm = mp; last = 2;
            }

        } else if (dt == 0x30 && off + 3u <= total) {
            if (last == 1) cur_inb = cfg[off + 2];
            if (last == 2) cur_outb = cfg[off + 2];
            last = 0;
        }

        off += dl;
    }

    if (cur_class == 0x0A && cur_in && cur_out && dif == 0xFF) {
        dif = cur_if; dalt = cur_alt;
        din = cur_in; dout = cur_out; dinm = cur_inm; doutm = cur_outm;
        dinb = cur_inb; doutb = cur_outb;
    }

    if (ctrl_proto == 0 || dif == 0xFF)
        return;

    /* Union указывает интерфейс данных; если нет - берём найденный */
    if (union_data != 0xFF && union_data != dif)
        return;

    c->ok = TRUE;
    c->proto = (UINT8)ctrl_proto;
    c->ctrl_iface = ctrl_if;
    c->data_iface = dif;
    c->data_alt = dalt;
    c->in_addr = din;
    c->out_addr = dout;
    c->in_mps = dinm ? dinm : 64;
    c->out_mps = doutm ? doutm : 64;
    c->in_burst = dinb;
    c->out_burst = doutb;
    c->mac_str = mac_str;
}

/* ================================================================
 * RNDIS: сообщения через управляющую точку
 * ================================================================ */

/* Отправить сообщение (оно уже в d->buf + 2048) и прочитать ответ
   (в d->buf + 3072, до 1024 байт). TRUE - пришёл ответ нужного типа */
static BOOLEAN rndis_cmd(KX_NET *m, UINT32 len, UINT32 want_type)
{
    KX_DEV *d = &g_kx_devs[m->dev];
    UINT64 req = d->buf + 2048u, resp = d->buf + 3072u;
    volatile UINT8 *r = (volatile UINT8 *)P2V(resp);

    /* SEND_ENCAPSULATED_COMMAND */
    UINT8 cc = kx_control(d, 0x21, 0x00, 0, m->ctrl_iface, (UINT16)len, req);

    if (cc != 1 && cc != 13)
        return FALSE;

    /* ответ: GET_ENCAPSULATED_RESPONSE, пока не придёт (устройству
       нужно время подумать; по правилам надо ждать уведомления на
       interrupt-точке, но опрос работает везде, так делает и Linux) */
    for (UINTN tries = 0; tries < 50; tries++) {

        raw_zero_mem(r, 16);
        cc = kx_control(d, 0xA1, 0x01, 0, m->ctrl_iface, 1024, resp);

        if ((cc == 1 || cc == 13) && le32(r) == want_type &&
            le32(r + 8) == m->rndis_id)
            return TRUE;

        kx_msleep(10);
    }

    return FALSE;
}

static BOOLEAN rndis_init(KX_NET *m, SIMPLE_TEXT_OUTPUT_INTERFACE *out)
{
    KX_DEV *d = &g_kx_devs[m->dev];
    volatile UINT8 *q = (volatile UINT8 *)P2V(d->buf + 2048u);
    volatile UINT8 *r = (volatile UINT8 *)P2V(d->buf + 3072u);

    /* INITIALIZE: версия 1.0, мы принимаем передачи до 4 КиБ */
    raw_zero_mem(q, 64);
    put_le32(q + 0, 2);
    put_le32(q + 4, 24);
    put_le32(q + 8, ++m->rndis_id);
    put_le32(q + 12, 1);
    put_le32(q + 16, 0);
    put_le32(q + 20, UN_BUF);

    if (!rndis_cmd(m, 24, 0x80000002u) || le32(r + 12) != 0) {
        kx_out(out, "    RNDIS: INITIALIZE failed\n");
        return FALSE;
    }

    /* QUERY OID_802_3_PERMANENT_ADDRESS - MAC-адрес */
    raw_zero_mem(q, 64);
    put_le32(q + 0, 4);
    put_le32(q + 4, 28);
    put_le32(q + 8, ++m->rndis_id);
    put_le32(q + 12, 0x01010101u);

    if (rndis_cmd(m, 28, 0x80000004u) && le32(r + 12) == 0 && le32(r + 16) >= 6) {
        UINT32 o = 8u + le32(r + 20);
        if (o + 6u <= 1024u)
            for (UINTN i = 0; i < 6; i++)
                m->mac[i] = r[o + i];
    }

    /* SET OID_GEN_CURRENT_PACKET_FILTER = свои, широковещательные,
       все групповые: только после этого устройство начнёт отдавать
       кадры */
    raw_zero_mem(q, 64);
    put_le32(q + 0, 5);
    put_le32(q + 4, 32);
    put_le32(q + 8, ++m->rndis_id);
    put_le32(q + 12, 0x0001010Eu);
    put_le32(q + 16, 4);
    put_le32(q + 20, 20);
    put_le32(q + 28, 0x0Du);

    if (!rndis_cmd(m, 32, 0x80000005u) || le32(r + 12) != 0) {
        kx_out(out, "    RNDIS: packet filter refused\n");
        return FALSE;
    }

    return TRUE;
}

/* MAC из строкового дескриптора ECM/NCM: 12 шестнадцатеричных цифр */
static BOOLEAN un_mac_from_string(KX_NET *m, UINT8 idx)
{
    KX_DEV *d = &g_kx_devs[m->dev];
    volatile UINT8 *s = (volatile UINT8 *)P2V(d->buf + 3072u);

    if (idx == 0)
        return FALSE;

    raw_zero_mem(s, 64);

    UINT8 cc = kx_control(d, 0x80, 0x06, (UINT16)(0x0300u | idx), 0x0409, 64, d->buf + 3072u);

    if ((cc != 1 && cc != 13) || s[0] < 2 + 24 || s[1] != 3)
        return FALSE;

    for (UINTN i = 0; i < 12; i++) {

        UINT8 ch = s[2 + 2 * i];
        UINT8 v;

        if (ch >= '0' && ch <= '9') v = (UINT8)(ch - '0');
        else if (ch >= 'a' && ch <= 'f') v = (UINT8)(ch - 'a' + 10);
        else if (ch >= 'A' && ch <= 'F') v = (UINT8)(ch - 'A' + 10);
        else return FALSE;

        if (i % 2 == 0)
            m->mac[i / 2] = (UINT8)(v << 4);
        else
            m->mac[i / 2] |= v;
    }

    return TRUE;
}

/* ================================================================
 * Подключение (поток usb, под g_usb_mutex и kx_lock - из
 * kx_enum_device)
 * ================================================================ */

BOOLEAN kx_net_probe(SIMPLE_TEXT_OUTPUT_INTERFACE *out, UINTN di, UINT8 ncfg)
{
    KX_DEV *d = &g_kx_devs[di];
    UINT64 cfg_phys = d->buf + 1024u;
    volatile UINT8 *cfg = (volatile UINT8 *)P2V(cfg_phys);
    UN_CAND best, c;

    memset(&best, 0, sizeof(best));
    memset(&c, 0, sizeof(c));

    if (ncfg == 0) ncfg = 1;
    if (ncfg > 4) ncfg = 4;

    /* у телефонов бывает несколько конфигураций: смотрим все */
    for (UINT8 ci = 0; ci < ncfg; ci++) {

        UINT8 cc = kx_control(d, 0x80, 0x06, (UINT16)(0x0200u | ci), 0, 9, cfg_phys);

        if (cc != 1 && cc != 13)
            continue;

        UINT16 total = (UINT16)(cfg[2] | (cfg[3] << 8));
        UINT8 value = cfg[5];

        if (total > 1024u) total = 1024u;
        if (total < 9u) continue;

        cc = kx_control(d, 0x80, 0x06, (UINT16)(0x0200u | ci), 0, total, cfg_phys);

        if (cc != 1 && cc != 13)
            continue;

        un_parse_cfg(cfg, total, &c);

        if (!c.ok)
            continue;

        c.cfg_value = value;

        /* кого предпочесть: проверенные в QEMU ECM и RNDIS - раньше
           NCM ("net usb rndis" - RNDIS раньше ECM) */
        static const UINT8 rank_def[4] = { 0, 3, 2, 1 };
        static const UINT8 rank_rndis[4] = { 0, 2, 3, 1 };
        const UINT8 *rank = g_usbnet_prefer_rndis ? rank_rndis : rank_def;

        if (!best.ok || rank[c.proto] > rank[best.proto])
            best = c;
    }

    if (!best.ok)
        return FALSE;

    INTN mi = -1;

    for (UINTN i = 0; i < KX_MAX_NET; i++)
        if (!g_kx_net[i].used) {
            mi = (INTN)i;
            break;
        }

    if (mi < 0) {
        kx_out(out, "    network device: too many - skipped\n");
        return FALSE;
    }

    KX_NET *m = &g_kx_net[mi];

    raw_zero_mem((volatile UINT8 *)m, sizeof(*m));

    m->dev = (UINT8)di;
    m->proto = best.proto;
    m->ctrl_iface = best.ctrl_iface;
    m->data_iface = best.data_iface;
    m->data_alt = best.data_alt;
    m->in_addr = best.in_addr;
    m->out_addr = best.out_addr;
    m->in_mps = best.in_mps;
    m->out_mps = best.out_mps;
    m->in_dci = (UINT8)((best.in_addr & 0x0Fu) * 2u + 1u);
    m->out_dci = (UINT8)((best.out_addr & 0x0Fu) * 2u);
    m->pipe = -1;
    m->note = "starting";

    ksnprintf(m->model, sizeof(m->model), "USB %s %04x:%04x",
              best.proto == UN_ECM ? "Ethernet (CDC-ECM)" :
              best.proto == UN_RNDIS ? "modem (RNDIS)" : "Ethernet (CDC-NCM)",
              d->vid, d->pid);

    kx_out(out, "    network: %s, configuration %u, data interface %u (alt %u), "
                "EP IN 0x%02x / OUT 0x%02x\n",
           un_proto_name(best.proto), best.cfg_value, best.data_iface, best.data_alt,
           best.in_addr, best.out_addr);

    /* --- SET_CONFIGURATION --- */
    UINT8 cc = kx_control(d, 0x00, 0x09, best.cfg_value, 0, 0, 0);

    if (cc != 1) {
        kx_out(out, "    SET_CONFIGURATION failed, cc=%u\n", cc);
        return FALSE;
    }

    /* ECM/NCM: данные идут только в "рабочей" альтернативной
       настройке интерфейса данных (в нулевой - без конечных точек);
       переключение 0 -> рабочая заодно сбрасывает фильтры */
    if (best.proto != UN_RNDIS) {
        kx_control(d, 0x01, 0x0B, 0, best.data_iface, 0, 0);
        if (best.data_alt != 0)
            kx_control(d, 0x01, 0x0B, best.data_alt, best.data_iface, 0, 0);
    }

    /* --- кольца и конечные точки --- */
    UINT64 rout = kx_dev_page(d);
    m->out_buf = kx_dev_page(d);

    INTN pi = kx_pipe_alloc();

    if (!rout || !m->out_buf || pi < 0) {
        kx_out(out, "    out of memory\n");
        return FALSE;
    }

    KX_HID *h = &g_kx_hid[pi];
    UINT64 rin = kx_dev_page(d);

    h->rep_buf = kx_dev_page(d);

    if (!rin || !h->rep_buf) {
        kx_out(out, "    out of memory\n");
        return FALSE;
    }

    kx_ring_init(&h->ring, rin);
    kx_ring_init(&m->out_ring, rout);

    h->dev = (UINT8)di;
    h->iface = best.data_iface;
    h->ep_addr = best.in_addr;
    h->dci = m->in_dci;
    h->maxpkt = best.in_mps;
    h->role = KX_ROLE_NET;
    h->net_slot = (UINT8)mi;
    h->state = KX_EP_RUN;
    h->mode_note = un_proto_name(best.proto);

    KX_EPCFG eps[2];

    eps[0].dci = m->in_dci;
    eps[0].type = 6;                  /* Bulk IN */
    eps[0].maxpkt = best.in_mps;
    eps[0].burst = best.in_burst;
    eps[0].interval = 0;
    eps[0].ring = rin;
    eps[0].avg = 3072;
    eps[0].esit = 0;

    eps[1].dci = m->out_dci;
    eps[1].type = 2;                  /* Bulk OUT */
    eps[1].maxpkt = best.out_mps;
    eps[1].burst = best.out_burst;
    eps[1].interval = 0;
    eps[1].ring = rout;
    eps[1].avg = 3072;
    eps[1].esit = 0;

    cc = kx_configure_eps(d, eps, 2, 0, 0);

    if (cc != 1) {
        kx_out(out, "    Configure Endpoint failed, cc=%u\n", cc);
        return FALSE;
    }

    m->used = TRUE;
    d->net = (INT8)mi;

    /* --- протокол --- */
    BOOLEAN ok = TRUE;

    if (best.proto == UN_RNDIS) {

        ok = rndis_init(m, out);

    } else {

        if (!un_mac_from_string(m, best.mac_str)) {
            /* MAC не сказали - придумать "местный" (бит 0x02) */
            m->mac[0] = 0x02;
            m->mac[1] = 0x4D;               /* 'M' */
            m->mac[2] = 0x59;               /* 'Y' */
            m->mac[3] = (UINT8)d->vid;
            m->mac[4] = (UINT8)d->pid;
            m->mac[5] = (UINT8)(net_random() | 1u);
        }

        if (best.proto == UN_NCM) {
            /* наши блоки приёма - не больше 4 КиБ (SET_NTB_INPUT_SIZE) */
            volatile UINT8 *b = (volatile UINT8 *)P2V(d->buf + 2048u);
            put_le32(b, UN_BUF);
            kx_control(d, 0x21, 0x86, 0, best.ctrl_iface, 4, d->buf + 2048u);
        }

        /* SET_ETHERNET_PACKET_FILTER: свои + широковещательные +
           групповые (некоторые устройства отказываются - не страшно) */
        kx_control(d, 0x21, 0x43, 0x0E, best.ctrl_iface, 0, 0);
    }

    if (!ok) {
        m->note = "protocol setup failed";
        return TRUE;           /* устройство наше, но не работает */
    }

    h->used = TRUE;
    m->pipe = pi;
    m->serial = ++g_net_serial;
    m->note = "running";

    kx_hid_start((UINTN *)&m->pipe, 1);

    d->status = "network (USB tethering), active";
    m->ready = TRUE;

    kx_out(out, "    network adapter ready: MAC %02x:%02x:%02x:%02x:%02x:%02x\n",
           m->mac[0], m->mac[1], m->mac[2], m->mac[3], m->mac[4], m->mac[5]);

    net_kick();              /* поток net зарегистрирует интерфейс */

    return TRUE;
}

/* Устройство выдернули (kx_remove_device, под kx_lock) */
void kx_net_removed(INTN mi)
{
    if (mi < 0 || mi >= KX_MAX_NET)
        return;

    g_kx_net[mi].ready = FALSE;
    g_kx_net[mi].used = FALSE;

    net_kick();
}

/* ================================================================
 * Приём (в прерывании USB)
 * ================================================================ */

void kx_net_report(KX_HID *h, volatile UINT8 *b, UINTN len)
{
    KX_NET *m = &g_kx_net[h->net_slot];
    NETIF *nif = m->nif;

    if (!m->used || !m->ready || nif == NULL || m->nif_serial != m->serial)
        return;

    if (m->proto == UN_ECM) {

        if (len >= ETH_HLEN) {
            net_rx_frame(nif, (const UINT8 *)b, len > 1514u ? 1514u : len);
            m->rx_frames++;
        }
        return;
    }

    if (m->proto == UN_RNDIS) {

        /* одно или несколько сообщений PACKET_MSG подряд */
        UINTN off = 0;

        while (off + 44u <= len) {

            UINT32 type = le32(b + off);
            UINT32 mlen = le32(b + off + 4);

            if (type != 1 || mlen < 44u || off + mlen > len) {
                m->rx_bad++;
                break;
            }

            UINT32 doff = 8u + le32(b + off + 8);
            UINT32 dlen = le32(b + off + 12);

            if (doff + dlen <= mlen && dlen >= ETH_HLEN && dlen <= NET_FRAME_MAX) {
                net_rx_frame(nif, (const UINT8 *)(b + off + doff), dlen);
                m->rx_frames++;
            } else {
                m->rx_bad++;
            }

            off += mlen;
        }
        return;
    }

    /* NCM: заголовок блока NTH16 ("NCMH") -> таблица NDP16 ("NCM0"
       или "NCM1") -> пары (смещение, длина) кадров, конец - (0, 0) */
    if (len < 12 || le32(b) != 0x484D434Eu) {
        m->rx_bad++;
        return;
    }

    UINTN ndp = le16(b + 10);

    for (UINTN guard = 0; ndp != 0 && ndp + 8u <= len && guard < 8; guard++) {

        UINT32 sig = le32(b + ndp);

        if (sig != 0x304D434Eu && sig != 0x314D434Eu) {
            m->rx_bad++;
            return;
        }

        UINTN nlen = le16(b + ndp + 4);

        for (UINTN i = ndp + 8u; i + 4u <= ndp + nlen && i + 4u <= len; i += 4u) {

            UINTN fo = le16(b + i), fl = le16(b + i + 2);

            if (fo == 0 || fl == 0)
                break;

            if (fo + fl <= len && fl >= ETH_HLEN && fl <= NET_FRAME_MAX) {
                net_rx_frame(nif, (const UINT8 *)(b + fo), fl);
                m->rx_frames++;
            }
        }

        ndp = le16(b + ndp + 6);
    }
}

/* ================================================================
 * Отправка (поток net, под g_net_mutex)
 * ================================================================ */

static BOOLEAN usbnet_tx(NETIF *nif, const UINT8 *frame, UINTN len)
{
    KX_NET *m = (KX_NET *)nif->drv;
    BOOLEAN ok = FALSE;

    if (len > 1514u)
        return FALSE;

    kmutex_lock(&g_usb_mutex);

    if (m->used && m->ready && m->serial == nif->drv_serial) {

        KX_DEV *d = &g_kx_devs[m->dev];
        volatile UINT8 *o = (volatile UINT8 *)P2V(m->out_buf);
        UINT32 total;

        if (m->proto == UN_ECM) {

            memcpy((void *)o, frame, len);
            total = (UINT32)len;

            /* ровно кратно пакету - устройство ждёт продолжения;
               лишний нулевой байт кадру Ethernet не мешает */
            if (total % m->out_mps == 0)
                o[total++] = 0;

        } else if (m->proto == UN_RNDIS) {

            raw_zero_mem(o, 44);
            memcpy((void *)(o + 44), frame, len);
            total = 44u + (UINT32)len;
            if (total % m->out_mps == 0)
                o[total++] = 0;                 /* добивка входит в MessageLength */
            put_le32(o + 0, 1);                 /* PACKET_MSG */
            put_le32(o + 4, total);
            put_le32(o + 8, 36);                /* данные - с 8 + 36 = 44 */
            put_le32(o + 12, (UINT32)len);

        } else {

            /* NCM: NTH16 (12) + NDP16 (8 + 2 пары) + кадр (с 32) */
            raw_zero_mem(o, 32);
            total = 32u + (UINT32)len;
            if (total % m->out_mps == 0)
                total++;
            put_le32(o + 0, 0x484D434Eu);       /* "NCMH" */
            put_le16(o + 4, 12);
            put_le16(o + 6, m->ncm_seq++);
            put_le16(o + 8, (UINT16)total);
            put_le16(o + 10, 12);               /* NDP - сразу за заголовком */
            put_le32(o + 12, 0x304D434Eu);      /* "NCM0" */
            put_le16(o + 16, 16);
            put_le16(o + 18, 0);
            put_le16(o + 20, 32);
            put_le16(o + 22, (UINT16)len);
            memcpy((void *)(o + 32), frame, len);
            if (total > 32u + len)
                o[total - 1] = 0;
        }

        UINT32 actual = 0;
        UINT8 cc = kx_bulk(d, m->out_dci, m->out_addr, &m->out_ring, m->out_buf, total,
                           &actual, 500);

        ok = (cc == 1 || cc == 13);

        if (ok)
            m->tx_frames++;
        else
            m->tx_errors++;
    }

    kmutex_unlock(&g_usb_mutex);

    return ok;
}

/* ================================================================
 * Поток net: подключённые/выдернутые USB-модемы -> интерфейсы
 * ================================================================ */

void usbnet_sync(void)
{
    for (UINTN i = 0; i < KX_MAX_NET; i++) {

        KX_NET *m = &g_kx_net[i];

        /* пропал или сменился - убрать интерфейс */
        if (m->nif != NULL && (!m->used || !m->ready || m->serial != m->nif_serial)) {
            NETIF *old = m->nif;
            m->nif = NULL;
            net_if_remove(old);
        }

        if (m->used && m->ready && m->nif == NULL) {

            UINT32 serial = m->serial;
            NETIF *nif = net_if_add("usb", un_proto_name(m->proto), m->model, m->mac,
                                    usbnet_tx, m);

            if (nif == NULL)
                continue;

            kmutex_lock(&g_net_mutex);
            nif->drv_serial = serial;
            nif->irq_mode = "USB interrupt (xHCI)";
            nif->speed_mbps = 0;
            nif->cfg = NET_CFG_DHCP;
            nif->link = TRUE;                /* модем на связи, пока воткнут */
            kmutex_unlock(&g_net_mutex);

            m->nif_serial = serial;
            m->nif = nif;
        }
    }
}

/* Для команды usb: что с сетевыми USB-устройствами */
void kx_net_print(SIMPLE_TEXT_OUTPUT_INTERFACE *out)
{
    for (UINTN i = 0; i < KX_MAX_NET; i++) {

        KX_NET *m = &g_kx_net[i];

        if (!m->used)
            continue;

        kprintf(out, "  network %s: %s, %s; frames in %llu (bad %llu), out %llu (errors %llu)%s%s\n",
                un_proto_name(m->proto), m->model, m->note, m->rx_frames, m->rx_bad,
                m->tx_frames, m->tx_errors, m->nif ? " -> " : "", m->nif ? m->nif->name : "");
    }
}
