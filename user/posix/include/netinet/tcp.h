/*
 * netinet/tcp.h - параметры TCP для setsockopt (программы MyOS на
 * полной libc). Ядро MyOS их принимает и молча игнорирует: Nagle у
 * него нет, keepalive - свой.
 */
#ifndef _MYOS_NETINET_TCP_H
#define _MYOS_NETINET_TCP_H

#define TCP_NODELAY    1
#define TCP_MAXSEG     2
#define TCP_KEEPIDLE   4
#define TCP_KEEPINTVL  5
#define TCP_KEEPCNT    6

#endif
