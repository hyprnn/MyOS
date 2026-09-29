/*
 * net/net.c - сеть (этап 8): интерфейсы, очередь приёма, поток
 * "net", контрольные суммы, прерывания сетевых карт.
 * Часть MyOS; общие объявления - в net/net.h и myos.h.
 *
 * Путь принятого кадра:
 *   карта -> прерывание -> драйвер копирует кадр в очередь приёма
 *   (net_rx_frame) и будит поток net -> поток под g_net_mutex
 *   разбирает кадр: ARP (net/arp.c) или IPv4 (net/ip.c) -> ICMP /
 *   UDP / TCP -> сокет -> программа, которая ждёт данные.
 * В обработчике прерывания ничего, кроме копирования, не делается:
 * там нельзя ни спать, ни брать мьютекс.
 *
 * Путь отправки: программа (или шелл) под g_net_mutex собирает
 * пакет -> ip_send -> arp_send_ip (узнать MAC получателя) ->
 * net_send_eth -> драйвер (NETIF.tx) кладёт кадр в кольцо карты.
 *
 * "lo" (127.0.0.1) - интерфейс-петля: всё, что отправлено на него,
 * сразу попадает в очередь приёма. Позволяет проверять TCP/UDP без
 * какой-либо сети.
 */
#include "net.h"

NETIF g_netifs[NET_MAX_IF];
KMUTEX g_net_mutex = KMUTEX_INIT("net");
volatile BOOLEAN g_net_work = FALSE;

static KTHREAD *g_net_thread = NULL;
static BOOLEAN g_net_started = FALSE;

/* ================================================================
 * Очередь приёма: кольцо кадров. Пишут драйверы (часто - в
 * прерывании), читает поток net. Защита - запрет прерываний
 * (спин-замок - на будущее, для нескольких ядер).
 * ================================================================ */

typedef struct {
    NETIF  *nif;
    UINT16  len;
    UINT8   data[NET_FRAME_MAX];
} NET_RXBUF;

static NET_RXBUF g_rxq[NET_RXQ];
static volatile UINTN g_rxq_head = 0;     /* куда класть */
static volatile UINTN g_rxq_tail = 0;     /* откуда брать */
static KSPINLOCK g_rxq_lock = KSPINLOCK_INIT;
static UINT64 g_rxq_overflows = 0;

void net_kick(void)
{
    g_net_work = TRUE;
    sched_wake_all((const void *)&g_net_work);
}

void net_rx_frame(NETIF *nif, const UINT8 *frame, UINTN len)
{
    if (len < ETH_HLEN || len > NET_FRAME_MAX)
        return;

    UINT64 fl = kspin_lock(&g_rxq_lock);
    UINTN next = (g_rxq_head + 1u) % NET_RXQ;

    if (next == g_rxq_tail) {
        /* очередь полна: поток net не успевает - кадр теряется
           (TCP потом повторит) */
        g_rxq_overflows++;
        nif->rx_dropped++;
        kspin_unlock(&g_rxq_lock, fl);
        return;
    }

    NET_RXBUF *b = &g_rxq[g_rxq_head];

    b->nif = nif;
    b->len = (UINT16)len;
    memcpy(b->data, frame, len);
    g_rxq_head = next;

    kspin_unlock(&g_rxq_lock, fl);

    net_kick();
}

/* Время для таймеров стека - миллисекунды от старта */
UINT64 net_now_ms(void)
{
    if (g_ktimer_ok)
        return g_kticks;

    return (g_tsc_hz != 0) ? rdtsc() / (g_tsc_hz / 1000u) : 0;
}

/* Случайные числа (номера портов, TCP ISN, xid DHCP): xorshift,
   подмешивается TSC - у каждой загрузки свои */
UINT32 net_random(void)
{
    static UINT64 s = 0;

    s ^= rdtsc();
    s ^= s << 13;
    s ^= s >> 7;
    s ^= s << 17;

    return (UINT32)(s >> 11);
}

/* ================================================================
 * Интерфейсы
 * ================================================================ */

NETIF *net_if_add(const char *prefix, const char *driver, const char *model,
                  const UINT8 mac[6], NET_TX_FN tx, void *drv)
{
    kmutex_lock(&g_net_mutex);

    NETIF *nif = NULL;

    for (UINTN i = 0; i < NET_MAX_IF; i++)
        if (!g_netifs[i].used) {
            nif = &g_netifs[i];
            break;
        }

    if (nif == NULL) {
        kmutex_unlock(&g_net_mutex);
        return NULL;
    }

    memset(nif, 0, sizeof(*nif));

    /* имя: prefix + первый свободный номер (eth0, eth1, usb0); петля - просто "lo" */
    if (kstreq(prefix, "lo"))
        ksnprintf(nif->name, sizeof(nif->name), "lo");

    for (UINTN n = 0; n < 10 && nif->name[0] == '\0'; n++) {

        char nm[8];
        BOOLEAN busy = FALSE;

        ksnprintf(nm, sizeof(nm), "%s%u", prefix, (UINT32)n);

        for (UINTN i = 0; i < NET_MAX_IF; i++)
            if (g_netifs[i].used && kstreq(g_netifs[i].name, nm))
                busy = TRUE;

        if (!busy) {
            ksnprintf(nif->name, sizeof(nif->name), "%s", nm);
            break;
        }
    }

    nif->driver = driver;
    ksnprintf(nif->model, sizeof(nif->model), "%s", model);
    memcpy(nif->mac, mac, 6);
    nif->tx = tx;
    nif->drv = drv;
    nif->irq_mode = "polling";
    nif->used = TRUE;

    kmutex_unlock(&g_net_mutex);

    klog("net: %s - %s (%s), MAC %02x:%02x:%02x:%02x:%02x:%02x\n",
         nif->name, model, driver, mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);

    net_kick();

    return nif;
}

/* Интерфейс пропал (выдернули USB-модем): забыть его адрес, ARP */
void net_if_remove(NETIF *nif)
{
    kmutex_lock(&g_net_mutex);

    if (nif->used) {

        klog("net: %s removed\n", nif->name);

        /* кадры этого интерфейса, ещё лежащие в очереди, - выбросить */
        UINT64 fl = kspin_lock(&g_rxq_lock);
        for (UINTN i = g_rxq_tail; i != g_rxq_head; i = (i + 1u) % NET_RXQ)
            if (g_rxq[i].nif == nif)
                g_rxq[i].nif = NULL;
        kspin_unlock(&g_rxq_lock, fl);

        dhcp_stop(nif);
        arp_forget_if(nif);
        nif->up = FALSE;
        nif->link = FALSE;
        nif->used = FALSE;
    }

    kmutex_unlock(&g_net_mutex);
}

NETIF *net_if_by_name(const char *name)
{
    for (UINTN i = 0; i < NET_MAX_IF; i++)
        if (g_netifs[i].used && kstreq(g_netifs[i].name, name))
            return &g_netifs[i];

    return NULL;
}

/* Драйвер заметил, что кабель вставили / вынули */
void net_link_changed(NETIF *nif, BOOLEAN link)
{
    if (nif->link == link)
        return;

    nif->link = link;
    klog("net: %s link %s\n", nif->name, link ? "UP" : "down");

    net_kick();         /* поток net запустит/остановит DHCP */
}

/* Отправить кадр: заголовок Ethernet + данные */
BOOLEAN net_send_eth(NETIF *nif, const UINT8 dst[6], UINT16 type,
                     const UINT8 *payload, UINTN len)
{
    static UINT8 frame[NET_FRAME_MAX];     /* под g_net_mutex */

    if (len + ETH_HLEN > NET_FRAME_MAX || nif->tx == NULL)
        return FALSE;

    memcpy(frame, dst, 6);
    memcpy(frame + 6, nif->mac, 6);
    net_put16(frame + 12, type);
    memcpy(frame + ETH_HLEN, payload, len);

    UINTN total = ETH_HLEN + len;

    /* короче 60 байт кадр Ethernet быть не может - добиваем нулями
       (многие карты делают это сами, но не все) */
    if (total < 60) {
        memset(frame + total, 0, 60 - total);
        total = 60;
    }

    if (!nif->tx(nif, frame, total)) {
        nif->tx_errors++;
        return FALSE;
    }

    nif->tx_packets++;
    nif->tx_bytes += total;

    return TRUE;
}

/* ================================================================
 * Петля (lo): отправленное сразу "принимается"
 * ================================================================ */

static BOOLEAN lo_tx(NETIF *nif, const UINT8 *frame, UINTN len)
{
    net_rx_frame(nif, frame, len);
    return TRUE;
}

/* ================================================================
 * Контрольная сумма Интернета (RFC 1071): сумма 16-битных слов с
 * переносом по кругу, в конце - инверсия. Одна и та же для IP,
 * ICMP, UDP и TCP.
 * ================================================================ */

UINT32 net_csum_add(const void *data, UINTN len, UINT32 sum)
{
    const UINT8 *p = (const UINT8 *)data;

    while (len > 1) {
        sum += ((UINT32)p[0] << 8) | p[1];
        p += 2;
        len -= 2;
    }

    if (len)
        sum += (UINT32)p[0] << 8;

    return sum;
}

UINT16 net_csum_fold(UINT32 sum)
{
    while (sum >> 16)
        sum = (sum & 0xFFFFu) + (sum >> 16);

    return (UINT16)~sum;
}

UINT16 net_csum(const void *data, UINTN len, UINT32 start)
{
    return net_csum_fold(net_csum_add(data, len, start));
}

/* ================================================================
 * Ожидание внутри стека: отпустить g_net_mutex, уснуть до события
 * obj или таймаута, снова взять замок. Условие проверяется
 * вызывающим в цикле - так же, как с sched_block.
 * ================================================================ */

BOOLEAN net_wait(const void *obj, const char *what, UINT64 timeout_ms)
{
    UINT64 fl = kx_irq_save();

    /* замок рекурсивный - запомнить глубину и отпустить целиком */
    UINTN depth = 0;

    if (g_net_mutex.owner == g_kcur) {
        depth = g_net_mutex.count;
        g_net_mutex.count = 0;
        g_net_mutex.owner = NULL;
        sched_wake_one(&g_net_mutex);
    }

    BOOLEAN woken = sched_block(obj, what, timeout_ms);

    kx_irq_restore(fl);

    if (depth > 0) {
        kmutex_lock(&g_net_mutex);
        g_net_mutex.count = depth;
    }

    return woken;
}

void net_wake(const void *obj)
{
    sched_wake_all(obj);
}

/* ================================================================
 * Адреса: печать и разбор "a.b.c.d"
 * ================================================================ */

void net_fmt_ip(char *buf, UINTN cap, UINT32 ip)
{
    ksnprintf(buf, cap, "%u.%u.%u.%u", ip >> 24, (ip >> 16) & 255u,
              (ip >> 8) & 255u, ip & 255u);
}

BOOLEAN net_parse_ip(const char *s, UINT32 *ip)
{
    UINT32 v = 0;

    for (UINTN part = 0; part < 4; part++) {

        UINT32 n = 0;
        UINTN digits = 0;

        while (*s >= '0' && *s <= '9') {
            n = n * 10u + (UINT32)(*s - '0');
            s++;
            if (++digits > 3)
                return FALSE;
        }

        if (digits == 0 || n > 255)
            return FALSE;

        v = (v << 8) | n;

        if (part < 3) {
            if (*s != '.')
                return FALSE;
            s++;
        }
    }

    if (*s != '\0')
        return FALSE;

    *ip = v;
    return TRUE;
}

/* ================================================================
 * Прерывания сетевых карт. У обработчика прерывания в MyOS нет
 * аргументов, поэтому для каждой карты - своя маленькая функция-
 * прокладка на своём векторе (0x60, 0x61, ...).
 * ================================================================ */

#define NET_VEC_BASE 0x60
#define NET_VEC_MAX  6

static UINTN g_net_vec_used = 0;

UINT8 net_alloc_vector(void)
{
    if (g_net_vec_used >= NET_VEC_MAX)
        return 0;

    return (UINT8)(NET_VEC_BASE + g_net_vec_used++);
}

/*
 * Включить прерывание карты: MSI/MSI-X, если есть; иначе - старая
 * линия INTx: её номер прошивка записала в конфигурацию PCI
 * (Interrupt Line), режим берём из переназначений MADT (у PCI по
 * умолчанию - по уровню, активный низкий). Драйвер после этого
 * ОБЯЗАН проверить, что прерывание правда приходит (карта умеет
 * вызвать его сама) - на машинах, где номер линии знает только AML
 * (таблица _PRT), он может быть не тем; тогда net_pci_irq_off и опрос.
 * Возвращает "MSI", "MSI-X", "IRQ n" или NULL (только опрос).
 */
const char *net_pci_irq_setup(NET_PCI_IRQ *q, KX_IRQ_HANDLER fn)
{
    static char names[NET_VEC_MAX][16];

    q->vector = net_alloc_vector();
    q->intx = FALSE;
    q->spurious = 0;

    if (q->vector == 0)
        return NULL;

    kx_irq_register(q->vector, fn);

    const char *m = kx_pci_enable_msi(q->bus, q->dev, q->fn, q->vector);

    if (m != NULL)
        return m;

    UINT8 line = (UINT8)(pci_config_read32(q->bus, q->dev, q->fn, 0x3C) & 0xFFu);
    UINT8 pin = (UINT8)((pci_config_read32(q->bus, q->dev, q->fn, 0x3C) >> 8) & 0xFFu);

    if (pin == 0 || line == 0 || line >= 0xF0)
        return NULL;

    BOOLEAN level, low;
    BOOLEAN iso = FALSE;

    for (UINTN i = 0; i < g_acpi.nisos; i++)
        if (g_acpi.isos[i].bus == 0 && g_acpi.isos[i].irq == line)
            iso = TRUE;

    q->gsi = kx_irq_to_gsi(line, &level, &low);

    if (!iso) {
        level = TRUE;
        low = TRUE;
    }

    /* INTx: снять запрет (бит 10 в Command) */
    UINT32 cmd = pci_config_read32(q->bus, q->dev, q->fn, 0x04);
    pci_config_write32(q->bus, q->dev, q->fn, 0x04, cmd & ~(1u << 10));

    if (!kx_ioapic_route(q->gsi, q->vector, level, low))
        return NULL;

    q->intx = TRUE;

    UINTN slot = (UINTN)(q->vector - NET_VEC_BASE);
    ksnprintf(names[slot], sizeof(names[slot]), "IRQ %u", q->gsi);

    return names[slot];
}

/* Прерывание не пришло (или "шторм") - выключить линию, дальше опрос */
void net_pci_irq_off(NET_PCI_IRQ *q)
{
    if (q->intx) {
        kx_ioapic_mask(q->gsi);
        UINT32 cmd = pci_config_read32(q->bus, q->dev, q->fn, 0x04);
        pci_config_write32(q->bus, q->dev, q->fn, 0x04, cmd | (1u << 10));
        q->intx = FALSE;
    }

    if (q->vector != 0)
        kx_irq_register(q->vector, NULL);
}

/*
 * Защита от "шторма": линия INTx общая, и если её держит другое
 * устройство, наш обработчик будет вызываться без конца, а машина -
 * стоять. ours - было ли прерывание от нашей карты. TRUE - пора
 * выключать линию (драйвер переходит на опрос).
 */
BOOLEAN net_pci_irq_storm(NET_PCI_IRQ *q, BOOLEAN ours)
{
    if (ours) {
        q->spurious = 0;
        return FALSE;
    }

    if (!q->intx)
        return FALSE;

    return ++q->spurious > 5000u;
}

/* ================================================================
 * Поток net
 * ================================================================ */

static void net_process_rx(void)
{
    for (UINTN guard = 0; guard < NET_RXQ; guard++) {

        UINT64 fl = kspin_lock(&g_rxq_lock);

        if (g_rxq_tail == g_rxq_head) {
            kspin_unlock(&g_rxq_lock, fl);
            return;
        }

        /* кадр копируется из очереди, чтобы не держать очередь
           занятой, пока он разбирается */
        static NET_RXBUF cur;
        NET_RXBUF *b = &g_rxq[g_rxq_tail];

        cur.nif = b->nif;
        cur.len = b->len;
        memcpy(cur.data, b->data, b->len);
        g_rxq_tail = (g_rxq_tail + 1u) % NET_RXQ;

        kspin_unlock(&g_rxq_lock, fl);

        NETIF *nif = cur.nif;

        if (nif == NULL || !nif->used)
            continue;

        nif->rx_packets++;
        nif->rx_bytes += cur.len;

        UINT16 type = net_get16(cur.data + 12);
        const UINT8 *pl = cur.data + ETH_HLEN;
        UINTN plen = cur.len - ETH_HLEN;

        /* чужие кадры (не нам, не всем) - мимо; карты обычно
           фильтруют сами, но USB-модемы и режим "всё подряд" - нет */
        BOOLEAN bcast = (cur.data[0] & 1u) != 0;
        if (!bcast && !nif->loopback && memcmp(cur.data, nif->mac, 6) != 0)
            continue;

        if (type == ETH_TYPE_ARP)
            arp_input(nif, pl, plen);
        else if (type == ETH_TYPE_IP)
            ip_input(nif, pl, plen);
    }
}

static UINT64 g_net_last_timer = 0;

static void net_timers(void)
{
    UINT64 now = net_now_ms();

    if (now - g_net_last_timer < 10u)
        return;

    g_net_last_timer = now;

    for (UINTN i = 0; i < NET_MAX_IF; i++) {

        NETIF *nif = &g_netifs[i];

        if (!nif->used || nif->loopback)
            continue;

        /* связь появилась - получить адрес; пропала - забыть его */
        if (nif->link && nif->cfg == NET_CFG_DHCP && nif->dhcp.state == DHCP_OFF)
            dhcp_start(nif);

        if (!nif->link && nif->cfg == NET_CFG_DHCP && nif->dhcp.state != DHCP_OFF) {
            dhcp_stop(nif);
            nif->up = FALSE;
            arp_forget_if(nif);
        }

        dhcp_timer(nif);
    }

    arp_timer();
    tcp_timer();
}

static BOOLEAN net_have_polling(void)
{
    for (UINTN i = 0; i < NET_MAX_IF; i++)
        if (g_netifs[i].used && g_netifs[i].poll != NULL)
            return TRUE;

    return FALSE;
}

static void net_thread(void *arg)
{
    (void)arg;

    for (;;) {

        /* USB-модемы подключаются/отключаются в потоке usb - здесь
           их регистрируем (порядок замков: net -> usb) */
        usbnet_sync();

        kmutex_lock(&g_net_mutex);

        for (UINTN i = 0; i < NET_MAX_IF; i++)
            if (g_netifs[i].used && g_netifs[i].poll != NULL)
                g_netifs[i].poll(&g_netifs[i]);

        net_process_rx();
        net_timers();

        kmutex_unlock(&g_net_mutex);

        /* спать до нового кадра; таймерам хватает 10 мс, карте без
           прерываний - 2 мс */
        UINT64 fl = kx_irq_save();

        if (!g_net_work)
            sched_block((const void *)&g_net_work, "net events",
                        net_have_polling() ? 2u : 10u);

        g_net_work = FALSE;

        kx_irq_restore(fl);
    }
}

/* ================================================================
 * Запуск (kmain)
 * ================================================================ */

void net_init(SIMPLE_TEXT_OUTPUT_INTERFACE *out)
{
    static const UINT8 lo_mac[6] = { 0, 0, 0, 0, 0, 0 };

    NETIF *lo = net_if_add("lo", "loopback", "loopback (this computer itself)",
                           lo_mac, lo_tx, NULL);

    if (lo != NULL) {
        lo->loopback = TRUE;
        lo->link = TRUE;
        lo->irq_mode = "-";
        lo->cfg = NET_CFG_STATIC;
        lo->ip = NET_IP(127, 0, 0, 1);
        lo->mask = NET_IP(255, 0, 0, 0);
        lo->up = TRUE;
    }

    e1000_init(out);
    rtl8169_init(out);
    wifi_scan_pci();

    UINTN n = 0;

    for (UINTN i = 0; i < NET_MAX_IF; i++) {

        NETIF *nif = &g_netifs[i];

        if (!nif->used || nif->loopback)
            continue;

        n++;
        nif->cfg = NET_CFG_DHCP;

        kprintf(out, "  %s: %s, MAC %02x:%02x:%02x:%02x:%02x:%02x, %s, link %s\n",
                nif->name, nif->model, nif->mac[0], nif->mac[1], nif->mac[2],
                nif->mac[3], nif->mac[4], nif->mac[5], nif->irq_mode,
                nif->link ? "UP" : "down (no cable?)");
    }

    if (n == 0)
        print(out, "  no wired network card found (USB tethering from a phone also works)\n");

    if (!g_sched_on) {
        print(out, "  no threads - network is off\n");
        return;
    }

    g_net_thread = kthread_create("net", net_thread, NULL, 16);
    g_net_started = (g_net_thread != NULL);

    if (g_net_started)
        print(out, "  TCP/IP stack is served by the thread 'net'; addresses come from DHCP.\n");
}

BOOLEAN net_running(void)
{
    return g_net_started;
}

UINT64 net_rxq_overflows(void)
{
    return g_rxq_overflows;
}
