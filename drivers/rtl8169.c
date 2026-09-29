/*
 * drivers/rtl8169.c - сетевые карты Realtek: RTL8111/8168 (гигабит,
 * стоит в большинстве ноутбуков и настольных плат, в том числе в
 * HP 250 G7), RTL8101/8106E (100 Мбит, тоже бывает в HP 250 G7),
 * RTL8169, и RTL8139C+ (её эмулирует QEMU: -device rtl8139).
 * Часть MyOS; сетевой стек - net/.
 *
 * У всех этих карт одинаковый "режим C+": кольца дескрипторов по
 * 16 байт в памяти:
 *   opts1: бит 31 OWN (1 - дескриптор у карты), 30 EOR (последний в
 *          кольце), 29 FS / 28 LS (первый/последний кусок кадра),
 *          младшие биты - размер буфера (приём) или кадра (отправка);
 *   opts2: VLAN - не нужен;
 *   адрес буфера (64 бита).
 * Приём: все дескрипторы отдаём карте (OWN=1); принятый кадр - OWN=0
 * и длина в opts1. Отправка: заполняем дескриптор, OWN=1, и "звонок"
 * в регистр TxPoll.
 *
 * Отличия RTL8139C+ от 8169-семейства: TxPoll на 0xD9 (а не 0x38),
 * режим C+ включается битами в CpCmd (0xE0), связь - в BasicModeStatus.
 *
 * ВАЖНО: RTL8111/8106E проверены только "на бумаге" (по документации
 * и драйверу Linux r8169): в QEMU таких карт нет. Новые чипы (8168G и
 * позже) после прошивки держат приём "закрытым" битом RXDV_GATED_EN
 * в регистре MISC - снимаем его, как это делает Linux.
 */
#include "../net/net.h"

#define RTL_MAX     2
#define RTL_RX      64
#define RTL_TX      32
#define RTL_BUF     2048

/* регистры */
#define R_IDR0      0x00      /* MAC-адрес */
#define R_MAR0      0x08      /* фильтр групповых адресов */
#define R_TXDESC    0x20      /* адрес кольца отправки (8169) = TxAddr0/1 (8139C+) */
#define R_CMD       0x37
#define R_TXPOLL69  0x38
#define R_IMR       0x3C
#define R_ISR       0x3E
#define R_TXCFG     0x40
#define R_RXCFG     0x44
#define R_9346      0x50
#define R_BMSR39    0x64      /* 8139: BasicModeStatus */
#define R_PHYSTAT   0x6C      /* 8169: PHYstatus */
#define R_TXPOLL39  0xD9
#define R_RXMAX     0xDA
#define R_CPCMD     0xE0
#define R_RXDESC    0xE4
#define R_MAXTX     0xEC
#define R_MISC      0xF0

#define CMD_RESET   0x10
#define CMD_RXEN    0x08
#define CMD_TXEN    0x04

#define INT_ROK     0x0001
#define INT_RER     0x0002
#define INT_TOK     0x0004
#define INT_TER     0x0008
#define INT_RDU     0x0010    /* нет свободных дескрипторов приёма */
#define INT_LINK    0x0020
#define INT_FOVW    0x0040
#define INT_SWINT   0x0100    /* 8169: прерывание "по просьбе" */

#define D_OWN       (1u << 31)
#define D_EOR       (1u << 30)
#define D_FS        (1u << 29)
#define D_LS        (1u << 28)

typedef struct __attribute__((packed)) {
    UINT32 opts1;
    UINT32 opts2;
    UINT64 addr;
} RTL_DESC;

#define KIND_8139   0         /* RTL8139C+ (QEMU) */
#define KIND_8169   1         /* PCI RTL8169/8110 */
#define KIND_8168   2         /* PCIe RTL8111/8168 */
#define KIND_8101   3         /* PCIe RTL8101/8102/8103/8105/8106/8107 */

typedef struct {
    UINT16      vendor, device;
    UINT8       kind;
    const char *name;
} RTL_MODEL;

static const RTL_MODEL g_rtl_models[] = {
    { 0x10EC, 0x8139, KIND_8139, "Realtek RTL8139C+" },
    { 0x10EC, 0x8169, KIND_8169, "Realtek RTL8169" },
    { 0x10EC, 0x8167, KIND_8169, "Realtek RTL8169SC" },
    { 0x10EC, 0x8168, KIND_8168, "Realtek RTL8111/8168" },
    { 0x10EC, 0x8161, KIND_8168, "Realtek RTL8111/8168" },
    { 0x10EC, 0x8136, KIND_8101, "Realtek RTL8101/8106E" },
    { 0x1186, 0x4300, KIND_8169, "D-Link DGE-528T (RTL8169)" },
    { 0x1186, 0x4302, KIND_8169, "D-Link DGE-530T (RTL8169)" },
    { 0x1259, 0xC107, KIND_8169, "Allied Telesyn (RTL8169)" },
    { 0x16EC, 0x0116, KIND_8169, "USRobotics (RTL8169)" },
    { 0x1737, 0x1032, KIND_8169, "Linksys EG1032 (RTL8169)" },
};

typedef struct {
    BOOLEAN     used;
    UINT8       kind;
    UINT32      xid;           /* номер версии чипа (из TxConfig) */
    UINT64      bar;
    NETIF      *nif;
    NET_PCI_IRQ irq;
    UINT64      rx_ring, tx_ring, rx_bufs, tx_bufs;
    UINT32      rx_cur, tx_cur;
    volatile UINT64 irqs;
    UINT64      rx_errors;
    /* сторож прерываний: кадр ждёт, а прерывания всё нет */
    UINT64      watch_irqs;
    UINT64      watch_since;
    BOOLEAN     watch_pending;
    BOOLEAN     polling;
} RTL;

static RTL g_rtl[RTL_MAX];

static void rtl_isr0(void);
static void rtl_isr1(void);
static KX_IRQ_HANDLER g_rtl_isr[RTL_MAX] = { rtl_isr0, rtl_isr1 };

static UINT8  r8(RTL *r, UINT32 o)            { return *(volatile UINT8 *)P2V(r->bar + o); }
static UINT16 r16(RTL *r, UINT32 o)           { return *(volatile UINT16 *)P2V(r->bar + o); }
static UINT32 r32(RTL *r, UINT32 o)           { return mmio_read32(r->bar + o); }
static void   w8(RTL *r, UINT32 o, UINT8 v)   { *(volatile UINT8 *)P2V(r->bar + o) = v; }
static void   w16(RTL *r, UINT32 o, UINT16 v) { *(volatile UINT16 *)P2V(r->bar + o) = v; }
static void   w32(RTL *r, UINT32 o, UINT32 v) { mmio_write32(r->bar + o, v); }

/* ================================================================
 * Приём (прерывание или опрос; прерывания запрещены)
 * ================================================================ */

static void rtl_rx(RTL *r)
{
    volatile RTL_DESC *ring = (volatile RTL_DESC *)P2V(r->rx_ring);

    for (UINTN guard = 0; guard < RTL_RX; guard++) {

        volatile RTL_DESC *d = &ring[r->rx_cur];
        UINT32 o = d->opts1;

        if (o & D_OWN)
            break;                                  /* ещё у карты */

        /* длина - с 4 байтами CRC; ошибка приёма - бит 21 (RES) у
           8169-семейства */
        UINT32 len = o & 0x3FFFu;
        BOOLEAN err = (r->kind != KIND_8139) && (o & (1u << 21));

        if ((o & D_FS) && (o & D_LS) && !err && len > 4u + ETH_HLEN)
            net_rx_frame(r->nif, (const UINT8 *)P2V(r->rx_bufs + (UINT64)r->rx_cur * RTL_BUF),
                         len - 4u);
        else
            r->rx_errors++;

        /* вернуть дескриптор карте */
        d->opts2 = 0;
        d->opts1 = D_OWN | (r->rx_cur == RTL_RX - 1u ? D_EOR : 0) | RTL_BUF;

        r->rx_cur = (r->rx_cur + 1u) % RTL_RX;
    }
}

static void rtl_link_check(RTL *r)
{
    BOOLEAN up;

    if (r->kind == KIND_8139) {
        up = (r16(r, R_BMSR39) & 0x0004u) != 0;
        r->nif->speed_mbps = 100;
    } else {
        UINT8 ps = r8(r, R_PHYSTAT);
        up = (ps & 0x02u) != 0;
        r->nif->speed_mbps = (ps & 0x10u) ? 1000u : (ps & 0x08u) ? 100u : 10u;
    }

    net_link_changed(r->nif, up);
}

static void rtl_poll(NETIF *nif);

static void rtl_isr(RTL *r)
{
    UINT16 st = r16(r, R_ISR);

    if (st == 0xFFFFu)
        st = 0;                                     /* карта пропала? */

    if (net_pci_irq_storm(&r->irq, st != 0)) {
        net_pci_irq_off(&r->irq);
        w16(r, R_IMR, 0);
        r->polling = TRUE;
        r->nif->poll_fast = TRUE;
        r->nif->irq_mode = "polling (shared IRQ line was stuck)";
        r->nif->poll = rtl_poll;
        return;
    }

    if (st == 0)
        return;

    w16(r, R_ISR, st);                              /* сбросить (запись 1) */

    r->irqs++;
    r->nif->irqs++;

    if (st & (INT_ROK | INT_RER | INT_RDU | INT_FOVW))
        rtl_rx(r);

    if (st & INT_LINK)
        rtl_link_check(r);
}

static void rtl_isr0(void) { if (g_rtl[0].used) rtl_isr(&g_rtl[0]); }
static void rtl_isr1(void) { if (g_rtl[1].used) rtl_isr(&g_rtl[1]); }

/*
 * Из потока net: в режиме опроса - забрать кадры; в режиме
 * прерываний - сторож: если кадр лежит готовый, а прерываний нет
 * уже 200 мс - линия INTx не та (номер знает только AML), переходим
 * на опрос.
 */
static void rtl_poll(NETIF *nif)
{
    RTL *r = (RTL *)nif->drv;
    UINT64 fl = kx_irq_save();

    if (r->polling) {
        UINT16 st = r16(r, R_ISR);
        if (st && st != 0xFFFFu)
            w16(r, R_ISR, st);
        rtl_rx(r);
        if (st & INT_LINK)
            rtl_link_check(r);
        kx_irq_restore(fl);
        return;
    }

    volatile RTL_DESC *d = &((volatile RTL_DESC *)P2V(r->rx_ring))[r->rx_cur];
    UINT64 now = net_now_ms();

    /* кадр ждёт, а прерываний с прошлого раза не было: первый раз -
       запомнить (прерывание могло просто не успеть), и только если
       и через 200 мс то же самое - линия не та */
    if ((d->opts1 & D_OWN) || r->irqs != r->watch_irqs) {
        r->watch_irqs = r->irqs;
        r->watch_pending = FALSE;
    } else if (!r->watch_pending) {
        r->watch_pending = TRUE;
        r->watch_since = now;
    } else if (now - r->watch_since > 200u) {
        klog("rtl8169: frames arrive but no interrupts - switching to polling\n");
        net_pci_irq_off(&r->irq);
        w16(r, R_IMR, 0);
        r->polling = TRUE;
        r->nif->poll_fast = TRUE;
        r->nif->irq_mode = "polling (interrupt did not arrive)";
        rtl_rx(r);
    }

    kx_irq_restore(fl);
}

/* ================================================================
 * Отправка
 * ================================================================ */

static BOOLEAN rtl_tx(NETIF *nif, const UINT8 *frame, UINTN len)
{
    RTL *r = (RTL *)nif->drv;
    volatile RTL_DESC *d = &((volatile RTL_DESC *)P2V(r->tx_ring))[r->tx_cur];

    if (len > RTL_BUF)
        return FALSE;

    if (d->opts1 & D_OWN) {

        /* кольцо полно: подождать до 10 мс */
        UINT64 end = rdtsc() + g_tsc_hz / 100u;

        while ((d->opts1 & D_OWN) && rdtsc() < end)
            cpu_pause();

        if (d->opts1 & D_OWN)
            return FALSE;
    }

    UINT64 buf = r->tx_bufs + (UINT64)r->tx_cur * RTL_BUF;

    memcpy(P2V(buf), frame, len);

    d->addr = buf;
    d->opts2 = 0;

    __asm__ __volatile__("sfence" ::: "memory");

    d->opts1 = D_OWN | D_FS | D_LS | (r->tx_cur == RTL_TX - 1u ? D_EOR : 0) | (UINT32)len;

    __asm__ __volatile__("mfence" ::: "memory");

    r->tx_cur = (r->tx_cur + 1u) % RTL_TX;

    /* "звонок": есть что отправить (NPQ - обычная очередь) */
    w8(r, r->kind == KIND_8139 ? R_TXPOLL39 : R_TXPOLL69, 0x40);

    return TRUE;
}

/* ================================================================
 * Запуск
 * ================================================================ */

/* Чипы 8168G и новее (по номеру версии XID, как в Linux r8169) */
static BOOLEAN rtl_is_8168g_plus(UINT32 xid)
{
    static const UINT16 ids[] = {
        0x4c0, 0x4c1, 0x509, 0x5c8,          /* 8168G, 8168GU, 8411B */
        0x540, 0x541,                        /* 8168H */
        0x502, 0x503, 0x504,                 /* 8168EP */
        0x54a, 0x54b,                        /* 8168FP */
        0x5c0, 0x6c0,                        /* 8168M / 8117 */
        0x445, 0x448, 0x44a, 0x44b, 0x4c8, 0x4c9, 0x4cb /* 8106E/8107E... */
    };

    for (UINTN i = 0; i < sizeof(ids) / sizeof(ids[0]); i++)
        if ((xid & 0x7CFu) == ids[i] || (xid & 0x7C8u) == ids[i])
            return TRUE;

    return FALSE;
}

static BOOLEAN rtl_start(RTL *r, UINT8 bus, UINT8 dev, UINT8 fn, const RTL_MODEL *m,
                         SIMPLE_TEXT_OUTPUT_INTERFACE *out)
{
    net_pci_wake(bus, dev, fn);

    /* регистры в памяти: первый memory-BAR (у 8139 - BAR1, у 8168 - BAR2) */
    UINT64 bar = 0;

    for (UINT8 off = 0x10; off <= 0x24 && bar == 0; off += 4) {
        UINT32 raw = pci_config_read32(bus, dev, fn, off);
        if (raw != 0 && !(raw & 1u))
            bar = pci_read_bar_address(bus, dev, fn, off);
        if (raw != 0 && !(raw & 1u) && ((raw >> 1) & 3u) == 2u)
            off += 4;                        /* 64-битный BAR занимает два */
    }

    if (bar == 0) {
        kprintf(out, "  %s: no memory BAR - skipped\n", m->name);
        return FALSE;
    }

    vmm_map_mmio(bar, 0x1000u, VMM_UC);
    pci_enable_device(bus, dev, fn);

    r->bar = bar;
    r->kind = m->kind;
    r->xid = (r32(r, R_TXCFG) >> 20) & 0x7CFu;

    /* всё выключить, сброс */
    w16(r, R_IMR, 0);
    w16(r, R_ISR, 0xFFFFu);
    w8(r, R_CMD, CMD_RESET);

    for (UINTN i = 0; i < 1000 && (r8(r, R_CMD) & CMD_RESET); i++)
        tsc_delay_us(100);

    UINT8 mac[6];
    for (UINTN i = 0; i < 6; i++)
        mac[i] = r8(r, R_IDR0 + (UINT32)i);

    if ((mac[0] | mac[1] | mac[2] | mac[3] | mac[4] | mac[5]) == 0 || mac[0] == 0xFF) {
        kprintf(out, "  %s: no MAC address - skipped\n", m->name);
        return FALSE;
    }

    /* кольца ниже 4 ГиБ, выровнены на 256 байт (у нас - страница) */
    r->rx_ring = pmm_alloc_zeroed(1, 0x100000000ull);
    r->tx_ring = pmm_alloc_zeroed(1, 0x100000000ull);
    r->rx_bufs = pmm_alloc_zeroed(RTL_RX * RTL_BUF / 4096u, 0x100000000ull);
    r->tx_bufs = pmm_alloc_zeroed(RTL_TX * RTL_BUF / 4096u, 0x100000000ull);

    if (!r->rx_ring || !r->tx_ring || !r->rx_bufs || !r->tx_bufs) {
        kprintf(out, "  %s: out of memory\n", m->name);
        return FALSE;
    }

    volatile RTL_DESC *rx = (volatile RTL_DESC *)P2V(r->rx_ring);

    for (UINTN i = 0; i < RTL_RX; i++) {
        rx[i].addr = r->rx_bufs + (UINT64)i * RTL_BUF;
        rx[i].opts2 = 0;
        rx[i].opts1 = D_OWN | (i == RTL_RX - 1u ? D_EOR : 0) | RTL_BUF;
    }

    volatile RTL_DESC *tx = (volatile RTL_DESC *)P2V(r->tx_ring);

    for (UINTN i = 0; i < RTL_TX; i++)
        tx[i].opts1 = (i == RTL_TX - 1u) ? D_EOR : 0;

    r->rx_cur = 0;
    r->tx_cur = 0;

    /* разрешить запись в регистры настройки */
    w8(r, R_9346, 0xC0);

    if (r->kind == KIND_8139) {
        /* режим C+: приём и отправка кольцами */
        w16(r, R_CPCMD, 0x0003u);
    } else {
        /* 8169: C+ Command - оставить как есть, без VLAN; макс. кадр */
        w16(r, R_CPCMD, (UINT16)(r16(r, R_CPCMD) & ~(1u << 6)));
        w16(r, R_RXMAX, RTL_BUF);
        if (r->kind == KIND_8168)
            w8(r, R_MAXTX, 0x3F);
    }

    w32(r, R_TXDESC, (UINT32)r->tx_ring);
    w32(r, R_TXDESC + 4, (UINT32)(r->tx_ring >> 32));
    w32(r, R_RXDESC, (UINT32)r->rx_ring);
    w32(r, R_RXDESC + 4, (UINT32)(r->rx_ring >> 32));

    /* новые чипы: снять "задвижку" приёма (RXDV_GATED_EN) */
    BOOLEAN newchip = (r->kind == KIND_8168 || r->kind == KIND_8101) && rtl_is_8168g_plus(r->xid);

    if (newchip) {
        UINT32 misc = r32(r, R_MISC);
        if (misc & (1u << 19))
            w32(r, R_MISC, misc & ~(1u << 19));
    }

    w8(r, R_CMD, CMD_RXEN | CMD_TXEN);

    /* отправка: промежуток между кадрами по стандарту, DMA кусками
       без ограничения */
    w32(r, R_TXCFG, (3u << 24) | (7u << 8));

    /* приём: свой адрес, широковещательные, групповые + режим DMA
       (у каждого поколения чипов - свои биты, как в Linux) */
    UINT32 rxcfg = 0x0Eu | (7u << 8);

    if (r->kind == KIND_8169)
        rxcfg |= (7u << 13);                          /* порог FIFO: нет */
    else if (r->kind == KIND_8168)
        rxcfg |= (1u << 15) | (1u << 14) | (newchip ? (1u << 11) : 0);
    else if (r->kind == KIND_8101)
        rxcfg |= (1u << 15);

    w32(r, R_RXCFG, rxcfg);
    w32(r, R_MAR0, 0xFFFFFFFFu);
    w32(r, R_MAR0 + 4, 0xFFFFFFFFu);

    w8(r, R_9346, 0x00);                              /* запись закрыта */

    r->nif = net_if_add("eth", "rtl8169", m->name, mac, rtl_tx, r);

    if (r->nif == NULL)
        return FALSE;

    r->used = TRUE;

    UINTN idx = (UINTN)(r - g_rtl);

    r->irq.bus = bus;
    r->irq.dev = dev;
    r->irq.fn = fn;

    const char *mode = net_pci_irq_setup(&r->irq, g_rtl_isr[idx]);

    w16(r, R_ISR, 0xFFFFu);
    w16(r, R_IMR, INT_ROK | INT_RER | INT_RDU | INT_LINK | INT_FOVW);

    r->nif->poll = rtl_poll;          /* сторож прерываний / опрос */
    r->watch_since = net_now_ms();

    if (mode == NULL) {
        w16(r, R_IMR, 0);
        r->polling = TRUE;
        r->nif->poll_fast = TRUE;
        r->nif->irq_mode = "polling";
    } else {
        r->nif->irq_mode = mode;
    }

    if (r->kind != KIND_8139 && r->kind != KIND_8169)
        klog("rtl8169: %s, chip version 0x%03x%s\n", m->name, r->xid,
             newchip ? " (8168G or newer)" : "");

    rtl_link_check(r);

    return TRUE;
}

void rtl8169_init(SIMPLE_TEXT_OUTPUT_INTERFACE *out)
{
    UINTN found = 0;

    for (UINTN nth = 0; nth < 8 && found < RTL_MAX; nth++) {

        UINT8 b, d, f;

        if (!pci_find_class(0x02, 0x00, -1, nth, &b, &d, &f))
            break;

        UINT32 id = pci_config_read32(b, d, f, 0);
        const RTL_MODEL *m = NULL;

        for (UINTN i = 0; i < sizeof(g_rtl_models) / sizeof(g_rtl_models[0]); i++)
            if (g_rtl_models[i].vendor == (UINT16)id && g_rtl_models[i].device == (UINT16)(id >> 16))
                m = &g_rtl_models[i];

        if (m == NULL) {
            if ((id & 0xFFFFu) == 0x10ECu)
                kprintf(out, "  Realtek network card 10ec:%04x - not supported yet\n", id >> 16);
            continue;
        }

        if (rtl_start(&g_rtl[found], b, d, f, m, out))
            found++;
    }
}
