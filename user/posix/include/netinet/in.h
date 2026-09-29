/*
 * netinet/in.h - адреса IPv4/IPv6 для сокетов (программы MyOS на
 * полной libc). Числа - как в Linux. Сеть MyOS - только IPv4; IPv6
 * объявлен, чтобы чужой код собирался (адреса AF_INET6 ядро не примет).
 */
#ifndef _MYOS_NETINET_IN_H
#define _MYOS_NETINET_IN_H

#include <stdint.h>
#include <sys/socket.h>
#include <arpa/inet.h>

typedef uint32_t in_addr_t;
typedef uint16_t in_port_t;

struct in_addr {
    in_addr_t s_addr;            /* в сетевом порядке байтов */
};

struct sockaddr_in {
    sa_family_t    sin_family;   /* AF_INET */
    in_port_t      sin_port;     /* в сетевом порядке */
    struct in_addr sin_addr;
    unsigned char  sin_zero[8];
};

struct in6_addr {
    union {
        uint8_t  __s6_addr[16];
        uint16_t __s6_addr16[8];
        uint32_t __s6_addr32[4];
    } __in6_u;
};
#define s6_addr __in6_u.__s6_addr

struct sockaddr_in6 {
    sa_family_t     sin6_family; /* AF_INET6 */
    in_port_t       sin6_port;
    uint32_t        sin6_flowinfo;
    struct in6_addr sin6_addr;
    uint32_t        sin6_scope_id;
};

extern const struct in6_addr in6addr_any;
extern const struct in6_addr in6addr_loopback;
#define IN6ADDR_ANY_INIT      { { { 0 } } }
#define IN6ADDR_LOOPBACK_INIT { { { 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1 } } }

#define INADDR_ANY        ((in_addr_t)0x00000000)
#define INADDR_BROADCAST  ((in_addr_t)0xffffffff)
#define INADDR_NONE       ((in_addr_t)0xffffffff)
#define INADDR_LOOPBACK   ((in_addr_t)0x7f000001)

#define INET_ADDRSTRLEN   16
#define INET6_ADDRSTRLEN  46

#define IPPROTO_IP     0
#define IPPROTO_ICMP   1
#define IPPROTO_TCP    6
#define IPPROTO_UDP    17
#define IPPROTO_IPV6   41
#define IPPROTO_RAW    255

#define IP_TOS         1
#define IP_TTL         2
#define IPV6_V6ONLY    26

#define IN6_IS_ADDR_UNSPECIFIED(a) \
    ((a)->__in6_u.__s6_addr32[0] == 0 && (a)->__in6_u.__s6_addr32[1] == 0 && \
     (a)->__in6_u.__s6_addr32[2] == 0 && (a)->__in6_u.__s6_addr32[3] == 0)
#define IN6_IS_ADDR_V4MAPPED(a) \
    ((a)->__in6_u.__s6_addr32[0] == 0 && (a)->__in6_u.__s6_addr32[1] == 0 && \
     (a)->__in6_u.__s6_addr32[2] == htonl(0xffff))

#endif
