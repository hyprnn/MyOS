/*
 * net/net.h - сеть (этап 8): общие типы и функции сетевого стека.
 * Часть MyOS. Остальной ОС (шеллу, системным вызовам) хватает
 * объявлений из myos.h (раздел "сеть"); этот заголовок нужен только
 * самому стеку (файлы net/) и драйверам сетевых карт (drivers/e1000.c,
 * drivers/rtl8169.c, drivers/usbnet.c).
 *
 * Как устроена сеть MyOS - снизу вверх:
 *
 *   драйвер карты  - кладёт принятый кадр Ethernet в очередь приёма
 *                    (net_rx_frame, можно прямо из прерывания) и
 *                    умеет отправить кадр (NETIF.tx);
 *   поток "net"    - разбирает очередь приёма и крутит таймеры
 *                    (повторы TCP, запросы ARP, продление DHCP);
 *   Ethernet/ARP   - net/arp.c: кто в локальной сети имеет такой IP;
 *   IPv4 + ICMP    - net/ip.c: заголовок IP, маршрут (своя сеть или
 *                    шлюз), ping;
 *   UDP            - net/udp.c; на нём DHCP (net/dhcp.c, адрес от
 *                    роутера) и DNS (net/dns.c, имя -> адрес);
 *   TCP            - net/tcp.c: соединения, подтверждения, повторы;
 *   сокеты         - net/socket.c: то, чем пользуются программы.
 *
 * Все структуры стека защищены одним спящим замком g_net_mutex.
 * Порядок замков: g_net_mutex -> g_usb_mutex (отправка через
 * USB-модем); поток usb НИКОГДА не берёт g_net_mutex.
 *
 * Адреса IPv4 внутри стека - UINT32 в ПОРЯДКЕ ПРОЦЕССОРА:
 * 10.0.2.15 = 0x0A00020F. В сеть (big-endian) их переводят
 * net_put32/net_get32 при сборке и разборе пакетов.
 */
#ifndef MYOS_NET_H
#define MYOS_NET_H

#include "../myos.h"

#define NET_MAX_IF        6          /* lo + до 5 карт */
#define NET_MTU           1500
#define ETH_HLEN          14
#define NET_FRAME_MAX     1536       /* кадр Ethernet с запасом */
#define NET_RXQ           128        /* кадров в очереди приёма */

#define ETH_TYPE_IP       0x0800
#define ETH_TYPE_ARP      0x0806

#define IP_PROTO_ICMP     1
#define IP_PROTO_TCP      6
#define IP_PROTO_UDP      17

#define IP_HLEN           20
#define UDP_HLEN          8
#define TCP_HLEN          20

#define NET_IP(a, b, c, d) (((UINT32)(a) << 24) | ((UINT32)(b) << 16) | \
                            ((UINT32)(c) << 8) | (UINT32)(d))

/* как настроен адрес интерфейса */
#define NET_CFG_NONE      0
#define NET_CFG_DHCP      1
#define NET_CFG_STATIC    2

typedef struct NETIF NETIF;

/* Отправить кадр Ethernet (len байт, заголовок уже внутри).
   Зовётся под g_net_mutex из обычного потока. */
typedef BOOLEAN (*NET_TX_FN)(NETIF *nif, const UINT8 *frame, UINTN len);
/* Для драйверов без прерываний: забрать принятое (из потока net) */
typedef void (*NET_POLL_FN)(NETIF *nif);

/* Состояние клиента DHCP на интерфейсе (net/dhcp.c) */
typedef struct {
    UINT8   state;            /* DHCP_* */
    UINT32  xid;              /* номер "разговора" */
    UINT32  offered_ip;
    UINT32  server;           /* кто предложил (DHCP server id) */
    UINT64  next_ms;          /* когда повторить / продлить */
    UINT32  tries;
    UINT64  lease_start_ms;
    UINT32  lease_s;          /* срок аренды адреса */
    UINT64  bound_ms;         /* когда получили адрес */
    const char *note;         /* что сейчас происходит - для ifconfig */
} DHCP_STATE;

struct NETIF {
    BOOLEAN     used;
    char        name[8];      /* lo, eth0, usb0 */
    const char *driver;       /* "e1000", "rtl8169", "usb-ecm", ... */
    char        model[48];    /* что за карта */
    UINT8       mac[6];
    volatile BOOLEAN link;    /* кабель/модем на связи */
    UINT32      speed_mbps;
    BOOLEAN     loopback;
    const char *irq_mode;     /* "MSI", "MSI-X", "IRQ 11", "polling" */
    NET_TX_FN   tx;
    NET_POLL_FN poll;         /* зовётся из потока net каждый круг (NULL - не нужно) */
    volatile BOOLEAN poll_fast; /* драйвер работает опросом: будить поток часто */
    void       *drv;          /* данные драйвера */
    UINT32      drv_serial;   /* USB: номер подключения */

    /* адрес */
    UINT8       cfg;          /* NET_CFG_* */
    BOOLEAN     up;           /* адрес есть, можно слать IP */
    UINT32      ip, mask, gw, dns, dns2;
    DHCP_STATE  dhcp;

    /* счётчики */
    UINT64      rx_packets, rx_bytes, rx_dropped;
    UINT64      tx_packets, tx_bytes, tx_errors;
    UINT64      irqs;
};

extern NETIF g_netifs[NET_MAX_IF];
extern KMUTEX g_net_mutex;
extern volatile BOOLEAN g_net_work;

/* --- net/net.c: интерфейсы, очередь приёма, поток net --- */
NETIF *net_if_add(const char *prefix, const char *driver, const char *model,
                  const UINT8 mac[6], NET_TX_FN tx, void *drv);
void   net_if_remove(NETIF *nif);
NETIF *net_if_by_name(const char *name);
void   net_rx_frame(NETIF *nif, const UINT8 *frame, UINTN len);
void   net_kick(void);
UINT64 net_now_ms(void);
BOOLEAN net_send_eth(NETIF *nif, const UINT8 dst[6], UINT16 type,
                     const UINT8 *payload, UINTN len);
void   net_link_changed(NETIF *nif, BOOLEAN link);
UINT16 net_csum(const void *data, UINTN len, UINT32 start);
UINT32 net_csum_add(const void *data, UINTN len, UINT32 sum);
UINT16 net_csum_fold(UINT32 sum);
BOOLEAN net_wait(const void *obj, const char *what, UINT64 timeout_ms);
void   net_wake(const void *obj);
void   net_fmt_ip(char *buf, UINTN cap, UINT32 ip);
BOOLEAN net_parse_ip(const char *s, UINT32 *ip);
UINT32 net_random(void);

static inline UINT16 net_get16(const UINT8 *p)
{
    return (UINT16)(((UINT16)p[0] << 8) | p[1]);
}

static inline UINT32 net_get32(const UINT8 *p)
{
    return ((UINT32)p[0] << 24) | ((UINT32)p[1] << 16) | ((UINT32)p[2] << 8) | p[3];
}

static inline void net_put16(UINT8 *p, UINT16 v)
{
    p[0] = (UINT8)(v >> 8);
    p[1] = (UINT8)v;
}

static inline void net_put32(UINT8 *p, UINT32 v)
{
    p[0] = (UINT8)(v >> 24);
    p[1] = (UINT8)(v >> 16);
    p[2] = (UINT8)(v >> 8);
    p[3] = (UINT8)v;
}

/* --- net/arp.c --- */
void arp_input(NETIF *nif, const UINT8 *pkt, UINTN len);
BOOLEAN arp_send_ip(NETIF *nif, UINT32 next_hop, const UINT8 *ip_pkt, UINTN len);
void arp_timer(void);
void arp_forget_if(NETIF *nif);
void arp_print(SIMPLE_TEXT_OUTPUT_INTERFACE *out);
void arp_announce(NETIF *nif);
BOOLEAN arp_busy(void);

/* --- net/ip.c --- */
void ip_input(NETIF *nif, const UINT8 *pkt, UINTN len);
INTN ip_send(UINT32 src, UINT32 dst, UINT8 proto, const UINT8 *payload, UINTN len);
INTN ip_send_if(NETIF *nif, UINT32 src, UINT32 dst, UINT8 proto,
                const UINT8 *payload, UINTN len);
NETIF *ip_route(UINT32 dst, UINT32 *next_hop);
BOOLEAN ip_is_local(UINT32 ip);
UINT32 ip_src_for(UINT32 dst);
UINT32 ip_pseudo_sum(UINT32 src, UINT32 dst, UINT8 proto, UINTN len);
void icmp_input(NETIF *nif, UINT32 src, UINT32 dst, UINT8 ttl, const UINT8 *p, UINTN len);

/* --- net/udp.c --- */
void udp_input(NETIF *nif, UINT32 src, UINT32 dst, const UINT8 *p, UINTN len);
INTN udp_send(UINT32 src, UINT16 sport, UINT32 dst, UINT16 dport,
              const UINT8 *data, UINTN len);
INTN udp_send_if(NETIF *nif, UINT32 src, UINT16 sport, UINT32 dst, UINT16 dport,
                 const UINT8 *data, UINTN len);

/* --- net/dhcp.c --- */
#define DHCP_OFF        0
#define DHCP_SELECTING  1     /* отправили DISCOVER, ждём OFFER */
#define DHCP_REQUESTING 2     /* отправили REQUEST, ждём ACK */
#define DHCP_BOUND      3     /* адрес получен */
#define DHCP_RENEWING   4     /* продлеваем */
#define DHCP_FAILED     5     /* никто не ответил - попробуем позже */
void dhcp_start(NETIF *nif);
void dhcp_stop(NETIF *nif);
void dhcp_input(NETIF *nif, const UINT8 *p, UINTN len);
void dhcp_timer(NETIF *nif);
const char *dhcp_state_name(UINT8 s);

/* --- net/dns.c --- */
INTN dns_resolve(const char *name, UINT32 *ip, UINT64 timeout_ms);
void dns_print_cache(SIMPLE_TEXT_OUTPUT_INTERFACE *out);

/* --- net/tcp.c --- */
typedef struct TCB TCB;
void tcp_input(NETIF *nif, UINT32 src, UINT32 dst, const UINT8 *p, UINTN len);
void tcp_timer(void);
void tcp_print(SIMPLE_TEXT_OUTPUT_INTERFACE *out);
BOOLEAN tcp_busy(void);

/* --- net/socket.c --- */
BOOLEAN sock_udp_input(UINT32 src, UINT16 sport, UINT32 dst, UINT16 dport,
                       const UINT8 *data, UINTN len);
BOOLEAN sock_icmp_input(UINT32 src, UINT8 ttl, const UINT8 *icmp, UINTN len);
UINT16 sock_ephemeral_port(UINT8 proto);
void sock_print(SIMPLE_TEXT_OUTPUT_INTERFACE *out);

/* --- драйверы --- */
void e1000_init(SIMPLE_TEXT_OUTPUT_INTERFACE *out);
void rtl8169_init(SIMPLE_TEXT_OUTPUT_INTERFACE *out);
void usbnet_sync(void);
void wifi_scan_pci(void);
void wifi_boot_report(SIMPLE_TEXT_OUTPUT_INTERFACE *out);
void net_pci_wake(UINT8 bus, UINT8 dev, UINT8 fn);

/* Общая помощь драйверам PCI: прерывание от карты - MSI, если
   умеет, иначе старая линия INTx через I/O APIC (с проверкой, что
   она правда доходит), иначе опрос. См. net/net.c */
typedef struct {
    UINT8   bus, dev, fn;
    UINT8   vector;
    BOOLEAN intx;             /* работаем по линии INTx */
    UINT32  gsi;
    volatile UINT32 spurious; /* прерываний подряд "не от нас" */
} NET_PCI_IRQ;
const char *net_pci_irq_setup(NET_PCI_IRQ *q, KX_IRQ_HANDLER fn);
void net_pci_irq_off(NET_PCI_IRQ *q);
BOOLEAN net_pci_irq_storm(NET_PCI_IRQ *q, BOOLEAN ours);
UINT8 net_alloc_vector(void);

#endif
