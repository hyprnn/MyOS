/*
 * third_party/bearssl/myos/string.h - заменяет системный <string.h>
 * при сборке BearSSL для программ MyOS: BearSSL нужны только эти
 * функции, а они есть в мини-libc MyOS (user/lib/string.c). Системный
 * заголовок glibc хоста брать нельзя - он подставил бы __memcpy_chk и
 * прочее из glibc, которой в MyOS нет.
 */
#ifndef MYOS_BEARSSL_STRING_H
#define MYOS_BEARSSL_STRING_H

#include <stddef.h>

void  *memcpy(void *d, const void *s, size_t n);
void  *memmove(void *d, const void *s, size_t n);
void  *memset(void *d, int c, size_t n);
int    memcmp(const void *a, const void *b, size_t n);
size_t strlen(const char *s);

#endif
