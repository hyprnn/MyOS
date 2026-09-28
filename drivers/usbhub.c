/*
 * drivers/usbhub.c - USB-хабы.
 * Часть MyOS; общие объявления - в myos.h.
 *
 * Хаб - обычное USB-устройство класса 9 с несколькими портами. Сам
 * он ничего не "перечисляет": это делаем мы, управляя его портами
 * через control-запросы на Endpoint 0:
 *   GET_STATUS(порт)            - что с портом (подключено? скорость?)
 *   SET_FEATURE(PORT_POWER)     - подать питание
 *   SET_FEATURE(PORT_RESET)     - сбросить порт (после этого
 *                                 устройство за ним отвечает на
 *                                 адресе 0 и его можно настраивать)
 *   CLEAR_FEATURE(C_PORT_*)     - "я видел это изменение"
 * А о том, что на каком-то порту что-то изменилось (воткнули,
 * выдернули), хаб сообщает сам - по своей трубе прерываний (один-два
 * байта, бит N = порт N).
 *
 * Для xHCI-контроллера устройство за хабом описывается "route
 * string" (номера портов по пути) и - для медленных LS/FS-устройств
 * за быстрым HS-хабом - адресом Transaction Translator'а этого хаба
 * (см. kx_enum_device в usb.c). А сам хаб надо отметить в его Slot
 * Context (флаг Hub, число портов, TT Think Time).
 */
#include "myos.h"

KX_HUB g_kx_hubs[KX_MAX_HUBS];
volatile BOOLEAN g_kx_hub_pending = FALSE;   /* поток usb: есть работа */

/* Запросы к портам хаба */
#define HUB_PORT_RESET        4
#define HUB_PORT_POWER        8
#define HUB_C_PORT_CONNECTION 16

/* GET_STATUS порта: wPortStatus, wPortChange */
static BOOLEAN kx_hub_port_status(KX_DEV *d, UINT8 port, UINT16 *st, UINT16 *ch)
{
    UINT64 buf = d->buf + 3584u;
    volatile UINT8 *b = (volatile UINT8 *)P2V(buf);

    UINT8 cc = kx_control(d, 0xA3, 0x00, 0, port, 4, buf);

    if (cc != 1 && cc != 13)
        return FALSE;

    *st = (UINT16)(b[0] | (b[1] << 8));
    *ch = (UINT16)(b[2] | (b[3] << 8));

    return TRUE;
}

static void kx_hub_port_feature(KX_DEV *d, UINT8 port, UINT16 feature, BOOLEAN set)
{
    kx_control(d, 0x23, set ? 0x03 : 0x01, feature, port, 0, 0);
}

/* Сбросить все флаги изменений порта (бит изменения -> номер
   "фичи" C_*, у USB2- и USB3-хабов немного разные) */
static void kx_hub_clear_changes(KX_DEV *d, BOOLEAN ss, UINT8 port, UINT16 ch)
{
    static const UINT8 usb2[5] = { 16, 17, 18, 19, 20 };
    static const UINT8 usb3[8] = { 16, 0, 0, 19, 20, 29, 25, 26 };

    for (UINTN bit = 0; bit < 8; bit++) {

        if (!(ch & (1u << bit)))
            continue;

        UINT8 f = ss ? usb3[bit] : ((bit < 5) ? usb2[bit] : 0);

        if (f != 0)
            kx_hub_port_feature(d, port, f, FALSE);
    }
}

/*
 * Проверить порт хаба и привести в соответствие: выдернули -
 * убрать устройство; воткнули - сбросить порт и настроить.
 */
static void kx_hub_port_check(SIMPLE_TEXT_OUTPUT_INTERFACE *out, UINTN hi, UINT8 p)
{
    KX_HUB *hb = &g_kx_hubs[hi];
    UINTN hdi = hb->dev;
    KX_DEV *hd = &g_kx_devs[hdi];
    UINT16 st = 0, ch = 0;
    char path[32];

    if (!kx_hub_port_status(hd, p, &st, &ch))
        return;

    if (ch != 0)
        kx_hub_clear_changes(hd, hb->ss, p, ch);

    BOOLEAN connected = (st & 1u) != 0;
    INT8 child = hb->child[p];

    if (child >= 0 && (!connected || (ch & 1u))) {

        kx_dev_path(&g_kx_devs[child], path, sizeof(path));
        kx_event_log("port %s: disconnected %04x:%04x", path,
                     g_kx_devs[child].vid, g_kx_devs[child].pid);
        kx_remove_device((UINTN)child);
        hb->child[p] = -1;
        g_kx.hot_removed++;
        child = -1;
    }

    if (!connected || child >= 0)
        return;

    kx_out(out, "\n    Hub port %u: device connected\n", p);

    if (out == NULL)
        g_kx.hot_added++;           /* воткнули уже во время работы */

    /* дребезг контактов при втыкании - дать устройству "сесть" */
    kx_msleep(100);

    /* --- сброс порта --- */
    kx_hub_port_feature(hd, p, HUB_PORT_RESET, TRUE);

    BOOLEAN done = FALSE;

    for (UINTN i = 0; i < 50; i++) {

        kx_msleep(10);

        if (!kx_hub_port_status(hd, p, &st, &ch))
            break;

        if (ch & (1u << 4)) {        /* C_PORT_RESET */
            done = TRUE;
            break;
        }
    }

    if (ch != 0)
        kx_hub_clear_changes(hd, hb->ss, p, ch);

    kx_msleep(20);                  /* восстановление после сброса */

    kx_hub_port_status(hd, p, &st, &ch);

    if (!done || !(st & 1u) || !(st & 2u)) {
        kx_out(out, "      hub port reset failed - skipped\n");
        return;
    }

    /* скорость: у USB2-хаба бит 9 - Low, бит 10 - High, иначе Full;
       за USB3-хабом - SuperSpeed */
    UINT8 speed;

    if (hb->ss)
        speed = 4;
    else if (st & (1u << 9))
        speed = 2;
    else if (st & (1u << 10))
        speed = 3;
    else
        speed = 1;

    kx_out(out, "      %s\n", kx_speed_name(speed));

    INTN di = kx_enum_device(out, (INTN)hdi, p, hd->root_port, speed);

    /* хаб могли выдернуть, пока мы настраивали ребёнка */
    if (!hb->used || hb->dev != hdi)
        return;

    if (di >= 0) {
        hb->child[p] = (INT8)di;
        if (out == NULL) {
            kx_dev_path(&g_kx_devs[di], path, sizeof(path));
            kx_event_log("port %s: connected %04x:%04x - %s", path,
                         g_kx_devs[di].vid, g_kx_devs[di].pid, g_kx_devs[di].status);
        }
    }
}

/* Настроить хаб: g_kx_devs[di] уже с адресом и Device Descriptor */
void kx_hub_setup(SIMPLE_TEXT_OUTPUT_INTERFACE *out, UINTN di)
{
    KX_DEV *d = &g_kx_devs[di];
    BOOLEAN ss = (d->speed >= 4);

    d->status = "hub, setup failed";

    if (d->depth >= 5) {
        kx_out(out, "    hub too deep (USB allows at most 5 in a chain)\n");
        return;
    }

    /* --- Configuration Descriptor: интерфейс хаба и его труба --- */
    UINT64 cfg_phys = d->buf + 1024u;
    volatile UINT8 *cfg = (volatile UINT8 *)P2V(cfg_phys);
    UINT8 cc = kx_control(d, 0x80, 0x06, 0x0200, 0, 9, cfg_phys);

    if (cc != 1 && cc != 13)
        return;

    UINT16 total = (UINT16)(cfg[2] | (cfg[3] << 8));
    UINT8 cfg_value = cfg[5];

    if (total > 1024u) total = 1024u;
    if (total < 9u) total = 9u;

    cc = kx_control(d, 0x80, 0x06, 0x0200, 0, total, cfg_phys);

    if (cc != 1 && cc != 13)
        return;

    UINT8 ep_addr = 0, interval = 0;
    UINT16 mps = 0;

    for (UINTN off = 0; off + 2u <= total; ) {

        UINT8 dl = cfg[off];

        if (dl < 2)
            break;

        if (cfg[off + 1] == 5 && off + 7u <= total && ep_addr == 0 &&
            (cfg[off + 2] & 0x80u) && (cfg[off + 3] & 3u) == 3u) {
            ep_addr = cfg[off + 2];
            mps = (UINT16)((cfg[off + 4] | (cfg[off + 5] << 8)) & 0x7FFu);
            interval = cfg[off + 6];
        }

        off += dl;
    }

    if (ep_addr == 0) {
        kx_out(out, "    hub without a status endpoint - skipped\n");
        return;
    }

    if (kx_control(d, 0x00, 0x09, cfg_value, 0, 0, 0) != 1) {
        kx_out(out, "    hub: SET_CONFIGURATION failed\n");
        return;
    }

    /* --- Hub Descriptor (0x29 у USB2, 0x2A у USB3) --- */
    UINT64 hd_phys = d->buf + 3072u;
    volatile UINT8 *hdsc = (volatile UINT8 *)P2V(hd_phys);

    cc = kx_control(d, 0xA0, 0x06, ss ? 0x2A00 : 0x2900, 0, 12, hd_phys);

    if (cc != 1 && cc != 13) {
        kx_out(out, "    hub: no hub descriptor\n");
        return;
    }

    UINT8 nports = hdsc[2];
    UINT16 chars = (UINT16)(hdsc[3] | (hdsc[4] << 8));
    UINT16 pwr = (UINT16)(hdsc[5] * 2u);

    if (nports > KX_HUB_MAX_PORTS)
        nports = KX_HUB_MAX_PORTS;

    /* --- записи хаба и его трубы --- */
    INTN hi = -1;

    for (UINTN i = 0; i < KX_MAX_HUBS; i++)
        if (!g_kx_hubs[i].used) {
            hi = (INTN)i;
            break;
        }

    INTN pi = kx_pipe_alloc();

    if (hi < 0 || pi < 0) {
        kx_out(out, "    too many hubs - skipped\n");
        return;
    }

    KX_HUB *hb = &g_kx_hubs[hi];
    KX_HID *h = &g_kx_hid[pi];

    raw_zero_mem((volatile UINT8 *)hb, sizeof(*hb));

    for (UINTN p = 0; p <= KX_HUB_MAX_PORTS; p++)
        hb->child[p] = -1;

    hb->dev = (UINT8)di;
    hb->nports = nports;
    hb->ss = ss;
    hb->think = (UINT8)((chars >> 5) & 3u);
    hb->pwr_ms = pwr;
    hb->pipe = (UINT8)pi;

    h->dev = (UINT8)di;
    h->role = KX_ROLE_HUB;
    h->ep_addr = ep_addr;
    h->dci = (UINT8)((ep_addr & 0x0Fu) * 2u + 1u);
    h->maxpkt = mps ? mps : 1;
    h->interval_raw = interval;
    h->interval_field = kx_interval_field(d->speed, interval);
    h->state = KX_EP_RUN;
    h->mode_note = "";

    UINT64 ring = kx_dev_page(d);
    h->rep_buf = kx_dev_page(d);

    if (!ring || !h->rep_buf) {
        kx_out(out, "    out of memory\n");
        return;
    }

    kx_ring_init(&h->ring, ring);

    KX_EPCFG e;

    e.dci = h->dci;
    e.type = 7;
    e.maxpkt = h->maxpkt;
    e.burst = 0;
    e.interval = h->interval_field;
    e.ring = ring;
    e.avg = h->maxpkt;
    e.esit = h->maxpkt;

    cc = kx_configure_eps(d, &e, 1, nports, (d->speed == 3) ? hb->think : 0);

    if (cc != 1) {
        kx_out(out, "    hub: Configure Endpoint failed, cc=%u\n", cc);
        return;
    }

    /* USB3-хабу - его глубина в дереве (для route string) */
    if (ss)
        kx_control(d, 0x20, 12, d->depth, 0, 0, 0);

    hb->used = TRUE;
    h->used = TRUE;
    d->hub = (INT8)hi;

    kx_out(out, "    USB hub, %u ports%s%s\n", nports,
           ss ? ", SuperSpeed" : "",
           (d->speed == 3) ? ", High Speed with Transaction Translator" : "");

    /* --- питание на все порты --- */
    for (UINT8 p = 1; p <= nports; p++)
        kx_hub_port_feature(d, p, HUB_PORT_POWER, TRUE);

    kx_msleep(pwr < 100 ? 100 : pwr);

    /* --- слушать хаб --- */
    kx_lock();
    kx_pipe_queue(h);
    kx_unlock();

    d->status = "hub, active";

    /* --- что уже воткнуто --- */
    for (UINT8 p = 1; p <= nports; p++)
        kx_hub_port_check(out, (UINTN)hi, p);
}

/* Сообщение хаба "изменились порты" (в прерывании): только
   запомнить - разберёт kx_hub_service */
void kx_hub_report(KX_HID *h, volatile UINT8 *rep, UINTN len)
{
    UINTN pi = (UINTN)(h - g_kx_hid);

    for (UINTN i = 0; i < KX_MAX_HUBS; i++) {

        KX_HUB *hb = &g_kx_hubs[i];

        if (!hb->used || hb->pipe != pi)
            continue;

        UINT32 bits = 0;

        for (UINTN k = 0; k < len && k < 4; k++)
            bits |= (UINT32)rep[k] << (8u * k);

        hb->pending |= bits;
        hb->events++;
        g_kx_hub_pending = TRUE;
        return;
    }
}

/* В основном коде: разобрать изменения на портах хабов */
void kx_hub_service(void)
{
    if (!g_kx_hub_pending)
        return;

    g_kx_hub_pending = FALSE;

    for (UINTN i = 0; i < KX_MAX_HUBS; i++) {

        KX_HUB *hb = &g_kx_hubs[i];

        if (!hb->used || hb->pending == 0)
            continue;

        UINT32 bits = hb->pending;
        UINTN dev = hb->dev;

        hb->pending = 0;

        /* бит 0 - изменился сам хаб (питание, перегрузка):
           подтвердить (C_HUB_LOCAL_POWER, C_HUB_OVER_CURRENT) */
        if (bits & 1u) {
            kx_control(&g_kx_devs[dev], 0x20, 0x01, 0, 0, 0, 0);
            kx_control(&g_kx_devs[dev], 0x20, 0x01, 1, 0, 0, 0);
        }

        for (UINT8 p = 1; p <= hb->nports; p++) {

            if (!(bits & (1u << p)))
                continue;

            /* хаб могли выдернуть, пока разбирали предыдущий порт */
            if (!hb->used || hb->dev != dev)
                break;

            kx_hub_port_check(NULL, i, p);
        }
    }
}
