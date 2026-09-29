/*
 * arpa/inet.h - htons/ntohl (из picolibc) плюс разбор и печать адресов
 * (inet_pton, inet_ntop, inet_addr...), которых в picolibc нет.
 * Реализация - user/posix/socket.c.
 */
#ifndef _MYOS_ARPA_INET_H
#define _MYOS_ARPA_INET_H

#include_next <arpa/inet.h>
#include <stdint.h>
#include <sys/socket.h>

struct in_addr;

int         inet_pton(int af, const char *src, void *dst);
const char *inet_ntop(int af, const void *src, char *dst, socklen_t size);
uint32_t    inet_addr(const char *cp);
int         inet_aton(const char *cp, struct in_addr *inp);
char       *inet_ntoa(struct in_addr in);

#endif
