/*
 * drivers/e1000.c - сетевые карты Intel PRO/1000 (e1000 / e1000e).
 * Часть MyOS; сетевой стек - net/.
 *
 * Какие карты: 82540EM (её по умолчанию показывает QEMU с машиной
 * pc), 82545EM, 82574L/82583 (QEMU -device e1000e и машина q35) -
 * проверены в QEMU; I217/I218/I219 (встроенные в чипсеты Intel
 * ноутбуков 2013-2020 годов) - "вслепую": у них такие же кольца и
 * регистры, но нет своего сброса (сбрасывать их можно только вместе
 * с PHY по особым правилам) - берём их такими, как оставила прошивка.
 *
 * Как карта принимает и отправляет - через КОЛЬЦА ДЕСКРИПТОРОВ в
 * памяти (по 16 байт):
 *   приём: в каждом дескрипторе - адрес пустого буфера 2 КиБ. Карта
 *     кладёт туда кадр, пишет длину и ставит бит DD ("готово"), потом
 *     прерывание. Мы забираем кадр и возвращаем дескриптор карте,
 *     сдвигая "хвост" RDT;
 *   отправка: пишем в дескриптор адрес и длину кадра, команду (EOP -
 *     конец кадра, IFCS - посчитай CRC, RS - сообщи, когда готово) и
 *     сдвигаем хвост TDT; карта отправит и поставит DD.
 * Регистры - в памяти (BAR0, 128 КиБ).
 */
#include "../net/net.h"

#define E1K_MAX      2
#define E1K_RX       64
#define E1K_TX       32
#define E1K_BUF      2048

#define E_CTRL       0x0000
#define E_STATUS     0x0008
#define E_EERD       0x0014
#define E_ICR        0x00C0
#define E_ICS        0x00C8
#define E_IMS        0x00D0
#define E_IMC        0x00D8
#define E_RCTL       0x0100
#define E_TCTL       0x0400
#define E_TIPG       0x0410
#define E_RDBAL      0x2800
#define E_RDBAH      0x2804
#define E_RDLEN      0x2808
#define E_RDH        0x2810
#define E_RDT        0x2818
#define E_RDTR       0x2820
#define E_TDBAL      0x3800
#define E_TDBAH      0x3804
#define E_TDLEN      0x3808
#define E_TDH        0x3810
#define E_TDT        0x3818
#define E_MTA        0x5200
#define E_RAL0       0x5400
#define E_RAH0       0x5404

/* прерывания (ICR/IMS) */
#define E_INT_TXDW   (1u << 0)
#define E_INT_LSC    (1u << 2)     /* кабель вставили/вынули */
#define E_INT_RXDMT0 (1u << 4)
#define E_INT_RXO    (1u << 6)
#define E_INT_RXT0   (1u << 7)     /* кадры приняты */

typedef struct __attribute__((packed)) {
    UINT64 addr;
    UINT16 length;
    UINT16 csum;
    UINT8  status;
    UINT8  errors;
    UINT16 special;
} E1K_RXD;

typedef struct __attribute__((packed)) {
    UINT64 addr;
    UINT16 length;
    UINT8  cso;
    UINT8  cmd;
    UINT8  status;
    UINT8  css;
    UINT16 special;
} E1K_TXD;

typedef struct {
    UINT16      id;
    const char *name;
    UINT8       kind;      /* 0 - 8254x, 1 - 82574/82583, 2 - встроенная PCH (I21x) */
} E1K_MODEL;

static const E1K_MODEL g_e1k_models[] = {
    { 0x100E, "Intel 82540EM (e1000)", 0 },
    { 0x100F, "Intel 82545EM (e1000)", 0 },
    { 0x1004, "Intel 82543GC (e1000)", 0 },
    { 0x1026, "Intel 82545GM (e1000)", 0 },
    { 0x10D3, "Intel 82574L (e1000e)", 1 },
    { 0x150C, "Intel 82583V (e1000e)", 1 },
    { 0x153A, "Intel I217-LM", 2 },
    { 0x153B, "Intel I217-V", 2 },
    { 0x155A, "Intel I218-LM", 2 },
    { 0x1559, "Intel I218-V", 2 },
    { 0x15A0, "Intel I218-LM", 2 },
    { 0x15A1, "Intel I218-V", 2 },
    { 0x15A2, "Intel I218-LM", 2 },
    { 0x15A3, "Intel I218-V", 2 },
    { 0x156F, "Intel I219-LM", 2 },
    { 0x1570, "Intel I219-V", 2 },
    { 0x15B7, "Intel I219-LM", 2 },
    { 0x15B8, "Intel I219-V", 2 },
    { 0x15BB, "Intel I219-LM", 2 },
    { 0x15BC, "Intel I219-V", 2 },
    { 0x15BD, "Intel I219-LM", 2 },
    { 0x15BE, "Intel I219-V", 2 },
    { 0x15D7, "Intel I219-LM", 2 },
    { 0x15D8, "Intel I219-V", 2 },
    { 0x15E3, "Intel I219-LM", 2 },
    { 0x15D6, "Intel I219-V", 2 },
    { 0x0D4E, "Intel I219-LM", 2 },
    { 0x0D4F, "Intel I219-V", 2 },
    { 0x0D4C, "Intel I219-LM", 2 },
    { 0x0D4D, "Intel I219-V", 2 },
    { 0x15FB, "Intel I219-LM", 2 },
    { 0x15FC, "Intel I219-V", 2 },
};

typedef struct {
    BOOLEAN     used;
    UINT64      bar;
    UINT8       kind;
    NETIF      *nif;
    NET_PCI_IRQ irq;
    UINT64      rx_ring, tx_ring;     /* физические адреса колец */
    UINT64      rx_bufs, tx_bufs;
    UINT32      rx_cur, tx_cur;
    volatile UINT64 irqs;
    UINT64      rx_errors;
} E1K;

static E1K g_e1k[E1K_MAX];

/* Обработчики прерываний: по одному на карту (без аргументов) */
static void e1k_isr0(void);
static void e1k_isr1(void);
static KX_IRQ_HANDLER g_e1k_isr[E1K_MAX] = { e1k_isr0, e1k_isr1 };

static UINT32 e1k_r(E1K *e, UINT32 reg)
{
    return mmio_read32(e->bar + reg);
}

static void e1k_w(E1K *e, UINT32 reg, UINT32 v)
{
    mmio_write32(e->bar + reg, v);
}

/* ================================================================
 * Приём: забрать все готовые кадры (в прерывании или из потока net
 * при опросе - всегда с запрещёнными прерываниями)
 * ================================================================ */

static void e1k_rx(E1K *e)
{
    volatile E1K_RXD *ring = (volatile E1K_RXD *)P2V(e->rx_ring);

    for (UINTN guard = 0; guard < E1K_RX; guard++) {

        volatile E1K_RXD *d = &ring[e->rx_cur];

        if (!(d->status & 0x01u))           /* DD - кадр готов? */
            break;

        UINT16 len = d->length;

        /* EOP - кадр целиком в одном буфере; ошибки - мимо */
        if ((d->status & 0x02u) && d->errors == 0 && len >= ETH_HLEN)
            net_rx_frame(e->nif, (const UINT8 *)P2V(e->rx_bufs + (UINT64)e->rx_cur * E1K_BUF), len);
        else
            e->rx_errors++;

        d->status = 0;

        /* вернуть дескриптор карте: хвост = этот (карта не заходит
           на дескриптор хвоста - так кольцо не переполняется) */
        e1k_w(e, E_RDT, e->rx_cur);
        e->rx_cur = (e->rx_cur + 1u) % E1K_RX;
    }
}

static void e1k_link_check(E1K *e)
{
    UINT32 st = e1k_r(e, E_STATUS);
    BOOLEAN up = (st & 0x2u) != 0;

    if (up) {
        UINT32 sp = (st >> 6) & 3u;
        e->nif->speed_mbps = (sp == 0) ? 10u : (sp == 1) ? 100u : 1000u;
    }

    net_link_changed(e->nif, up);
}

static void e1k_poll(NETIF *nif);

static void e1k_isr(E1K *e)
{
    UINT32 icr = e1k_r(e, E_ICR);          /* чтение сбрасывает причины */

    if (net_pci_irq_storm(&e->irq, icr != 0)) {
        /* линию держит кто-то другой - уходим на опрос */
        net_pci_irq_off(&e->irq);
        e1k_w(e, E_IMC, 0xFFFFFFFFu);
        e->nif->irq_mode = "polling (shared IRQ line was stuck)";
        e->nif->poll = e1k_poll;
        e->nif->poll_fast = TRUE;
        klog("e1000: IRQ line %u is stuck, switching to polling\n", e->irq.gsi);
        return;
    }

    if (icr == 0)
        return;

    e->irqs++;
    e->nif->irqs++;

    if (icr & (E_INT_RXT0 | E_INT_RXDMT0 | E_INT_RXO))
        e1k_rx(e);

    if (icr & E_INT_LSC)
        e1k_link_check(e);
}

static void e1k_isr0(void) { if (g_e1k[0].used) e1k_isr(&g_e1k[0]); }
static void e1k_isr1(void) { if (g_e1k[1].used) e1k_isr(&g_e1k[1]); }

/* Опрос (карта без прерываний): из потока net */
static void e1k_poll(NETIF *nif)
{
    E1K *e = (E1K *)nif->drv;
    UINT64 fl = kx_irq_save();

    UINT32 icr = e1k_r(e, E_ICR);

    e1k_rx(e);

    if (icr & E_INT_LSC)
        e1k_link_check(e);

    kx_irq_restore(fl);
}

/* ================================================================
 * Отправка (поток, под g_net_mutex)
 * ================================================================ */

static BOOLEAN e1k_tx(NETIF *nif, const UINT8 *frame, UINTN len)
{
    E1K *e = (E1K *)nif->drv;
    volatile E1K_TXD *ring = (volatile E1K_TXD *)P2V(e->tx_ring);
    volatile E1K_TXD *d = &ring[e->tx_cur];

    if (len > E1K_BUF)
        return FALSE;

    /* дескриптор ещё не отправлен (кольцо полно) - чуть подождать */
    if (d->cmd != 0 && !(d->status & 0x01u)) {

        UINT64 end = rdtsc() + g_tsc_hz / 100u;       /* 10 мс */

        while (!(d->status & 0x01u) && rdtsc() < end)
            cpu_pause();

        if (!(d->status & 0x01u))
            return FALSE;
    }

    memcpy(P2V(e->tx_bufs + (UINT64)e->tx_cur * E1K_BUF), frame, len);

    d->addr = e->tx_bufs + (UINT64)e->tx_cur * E1K_BUF;
    d->length = (UINT16)len;
    d->cso = 0;
    d->css = 0;
    d->special = 0;
    d->status = 0;
    d->cmd = 0x01u | 0x02u | 0x08u;         /* EOP | IFCS | RS */

    __asm__ __volatile__("mfence" ::: "memory");

    e->tx_cur = (e->tx_cur + 1u) % E1K_TX;
    e1k_w(e, E_TDT, e->tx_cur);

    return TRUE;
}

/* ================================================================
 * Запуск карты
 * ================================================================ */

/* Слово EEPROM (там MAC-адрес, если его нет в регистрах) */
static BOOLEAN e1k_eeprom(E1K *e, UINT32 addr, UINT16 *out)
{
    UINT32 shift = (e->kind == 0) ? 8u : 2u;
    UINT32 done = (e->kind == 0) ? (1u << 4) : (1u << 1);

    e1k_w(e, E_EERD, (addr << shift) | 1u);

    for (UINTN i = 0; i < 10000; i++) {
        UINT32 v = e1k_r(e, E_EERD);
        if (v & done) {
            *out = (UINT16)(v >> 16);
            return TRUE;
        }
        tsc_delay_us(5);
    }

    return FALSE;
}

static BOOLEAN e1k_start(E1K *e, UINT8 bus, UINT8 dev, UINT8 fn,
                         const E1K_MODEL *model, SIMPLE_TEXT_OUTPUT_INTERFACE *out)
{
    UINT64 bar = pci_read_bar_address(bus, dev, fn, 0x10);

    if (bar == 0) {
        kprintf(out, "  %s: no memory BAR - skipped\n", model->name);
        return FALSE;
    }

    net_pci_wake(bus, dev, fn);
    vmm_map_mmio(bar, 0x20000u, VMM_UC);
    pci_enable_device(bus, dev, fn);

    e->bar = bar;
    e->kind = model->kind;

    /* всё выключить: прерывания, приём, отправку */
    e1k_w(e, E_IMC, 0xFFFFFFFFu);
    e1k_w(e, E_RCTL, 0);
    e1k_w(e, E_TCTL, 0);
    (void)e1k_r(e, E_STATUS);

    if (e->kind != 2) {
        /* полный сброс карты (CTRL.RST), потом снова всё выключить */
        e1k_w(e, E_CTRL, e1k_r(e, E_CTRL) | (1u << 26));
        tsc_delay_us(2000);

        for (UINTN i = 0; i < 1000 && (e1k_r(e, E_CTRL) & (1u << 26)); i++)
            tsc_delay_us(100);

        e1k_w(e, E_IMC, 0xFFFFFFFFu);
    }

    /* включить связь: SLU (Set Link Up) + автоопределение скорости;
       снять сброс PHY и "потерю сигнала", выключить VLAN */
    UINT32 ctrl = e1k_r(e, E_CTRL);
    ctrl |= (1u << 6) | (1u << 5);
    ctrl &= ~((1u << 3) | (1u << 31) | (1u << 7) | (1u << 30));
    e1k_w(e, E_CTRL, ctrl);

    /* MAC-адрес: сначала из регистра адреса 0 (его заполняет сама
       карта из EEPROM при сбросе), иначе - из EEPROM */
    UINT8 mac[6];
    UINT32 ral = e1k_r(e, E_RAL0);
    UINT32 rah = e1k_r(e, E_RAH0);

    if ((rah & (1u << 31)) && (ral != 0 || (rah & 0xFFFFu) != 0)) {
        for (UINTN i = 0; i < 4; i++)
            mac[i] = (UINT8)(ral >> (8u * i));
        mac[4] = (UINT8)rah;
        mac[5] = (UINT8)(rah >> 8);
    } else {
        UINT16 w[3];
        if (!e1k_eeprom(e, 0, &w[0]) || !e1k_eeprom(e, 1, &w[1]) || !e1k_eeprom(e, 2, &w[2])) {
            kprintf(out, "  %s: cannot read the MAC address - skipped\n", model->name);
            return FALSE;
        }
        for (UINTN i = 0; i < 3; i++) {
            mac[2 * i] = (UINT8)w[i];
            mac[2 * i + 1] = (UINT8)(w[i] >> 8);
        }
        e1k_w(e, E_RAL0, (UINT32)mac[0] | ((UINT32)mac[1] << 8) |
                         ((UINT32)mac[2] << 16) | ((UINT32)mac[3] << 24));
        e1k_w(e, E_RAH0, (UINT32)mac[4] | ((UINT32)mac[5] << 8) | (1u << 31));
    }

    /* таблица групповых адресов - пусто */
    for (UINT32 i = 0; i < 128; i++)
        e1k_w(e, E_MTA + 4u * i, 0);

    /* кольца и буферы: ниже 4 ГиБ (часть карт умеет только 32 бита) */
    e->rx_ring = pmm_alloc_zeroed(1, 0x100000000ull);
    e->tx_ring = pmm_alloc_zeroed(1, 0x100000000ull);
    e->rx_bufs = pmm_alloc_zeroed(E1K_RX * E1K_BUF / 4096u, 0x100000000ull);
    e->tx_bufs = pmm_alloc_zeroed(E1K_TX * E1K_BUF / 4096u, 0x100000000ull);

    if (!e->rx_ring || !e->tx_ring || !e->rx_bufs || !e->tx_bufs) {
        kprintf(out, "  %s: out of memory\n", model->name);
        return FALSE;
    }

    volatile E1K_RXD *rx = (volatile E1K_RXD *)P2V(e->rx_ring);

    for (UINTN i = 0; i < E1K_RX; i++) {
        rx[i].addr = e->rx_bufs + (UINT64)i * E1K_BUF;
        rx[i].status = 0;
    }

    e1k_w(e, E_RDBAL, (UINT32)e->rx_ring);
    e1k_w(e, E_RDBAH, (UINT32)(e->rx_ring >> 32));
    e1k_w(e, E_RDLEN, E1K_RX * 16u);
    e1k_w(e, E_RDH, 0);
    e1k_w(e, E_RDT, E1K_RX - 1u);           /* все буферы, кроме одного, - карте */
    e1k_w(e, E_RDTR, 0);                    /* прерывание без задержки */
    e->rx_cur = 0;

    e1k_w(e, E_TDBAL, (UINT32)e->tx_ring);
    e1k_w(e, E_TDBAH, (UINT32)(e->tx_ring >> 32));
    e1k_w(e, E_TDLEN, E1K_TX * 16u);
    e1k_w(e, E_TDH, 0);
    e1k_w(e, E_TDT, 0);
    e->tx_cur = 0;

    /* приём: включить (EN), широковещательные (BAM), без CRC в
       буфере (SECRC), буфер 2048 */
    e1k_w(e, E_RCTL, (1u << 1) | (1u << 15) | (1u << 26));

    /* отправка: включить (EN), добивать короткие кадры (PSP),
       повторы при коллизиях (CT), полный дуплекс (COLD 0x40) */
    e1k_w(e, E_TCTL, (1u << 1) | (1u << 3) | (0x0Fu << 4) | (0x40u << 12));
    e1k_w(e, E_TIPG, 10u | (8u << 10) | (6u << 20));

    e->nif = net_if_add("eth", "e1000", model->name, mac, e1k_tx, e);

    if (e->nif == NULL)
        return FALSE;

    e->used = TRUE;

    /* прерывание: MSI (82574 умеет) или линия INTx - и проверка, что
       оно правда приходит: просим карту саму его вызвать (ICS) */
    UINTN idx = (UINTN)(e - g_e1k);

    e->irq.bus = bus;
    e->irq.dev = dev;
    e->irq.fn = fn;

    const char *mode = net_pci_irq_setup(&e->irq, g_e1k_isr[idx]);

    (void)e1k_r(e, E_ICR);
    e1k_w(e, E_IMS, E_INT_RXT0 | E_INT_RXO | E_INT_RXDMT0 | E_INT_LSC);

    if (mode != NULL) {

        UINT64 before = e->irqs;

        e1k_w(e, E_ICS, E_INT_LSC);

        for (UINTN i = 0; i < 50 && e->irqs == before; i++)
            kx_sleep_us(1000);

        if (e->irqs == before) {
            klog("e1000: %s does not arrive - polling\n", mode);
            net_pci_irq_off(&e->irq);
            mode = NULL;
        }
    }

    if (mode == NULL) {
        e1k_w(e, E_IMC, 0xFFFFFFFFu);
        e->nif->poll = e1k_poll;
        e->nif->poll_fast = TRUE;
        e->nif->irq_mode = "polling";
    } else {
        e->nif->irq_mode = mode;
    }

    e1k_link_check(e);

    return TRUE;
}

void e1000_init(SIMPLE_TEXT_OUTPUT_INTERFACE *out)
{
    UINTN found = 0;

    for (UINTN nth = 0; nth < 8 && found < E1K_MAX; nth++) {

        UINT8 b, d, f;

        if (!pci_find_class(0x02, 0x00, -1, nth, &b, &d, &f))
            break;

        UINT32 id = pci_config_read32(b, d, f, 0);

        if ((id & 0xFFFFu) != 0x8086u)
            continue;

        const E1K_MODEL *m = NULL;

        for (UINTN i = 0; i < sizeof(g_e1k_models) / sizeof(g_e1k_models[0]); i++)
            if (g_e1k_models[i].id == (UINT16)(id >> 16))
                m = &g_e1k_models[i];

        if (m == NULL) {
            kprintf(out, "  Intel network card 8086:%04x - not supported yet\n", id >> 16);
            continue;
        }

        if (e1k_start(&g_e1k[found], b, d, f, m, out))
            found++;
    }
}
