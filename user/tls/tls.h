/*
 * user/tls/tls.h - TLS (HTTPS) для программ MyOS: внутренности обёртки
 * над BearSSL. Программы видят только функции tls_* из
 * user/include/myos.h; этот заголовок - для файлов user/tls/ (здесь нельзя
 * включать myos.h: его uint64_t и BearSSL-овские типы <stdint.h>
 * различаются, поэтому нужное из мини-libc объявлено ниже вручную).
 */
#ifndef MYOS_TLS_H
#define MYOS_TLS_H

#include <stddef.h>
#include <stdint.h>
#include "bearssl.h"
#include "sysnum.h"

/* из мини-libc MyOS (user/lib) */
long  send(int fd, const void *buf, size_t n);
long  recv(int fd, void *buf, size_t n);
int   open(const char *path, int flags);
long  read(int fd, void *buf, size_t n);
int   close(int fd);
void *malloc(size_t n);
void  free(void *p);
int   snprintf(char *buf, size_t cap, const char *fmt, ...);
int   gettime_utc(struct myos_time *t);
long  getrandom(void *buf, size_t n);

struct myos_tls;
#define TLS_NO_VERIFY 1
struct myos_tls *tls_open(int fd, const char *host, int flags, const char *ca_file,
                          char *err, size_t errcap);
long tls_send(struct myos_tls *t, const void *buf, size_t n);
long tls_recv(struct myos_tls *t, void *buf, size_t n);
void tls_error(struct myos_tls *t, char *err, size_t cap);
void tls_info(struct myos_tls *t, char *buf, size_t cap);
void tls_close(struct myos_tls *t);

/* корневые сертификаты Mozilla (roots.c) */
extern const br_x509_trust_anchor *const g_tls_roots;
extern const size_t g_tls_roots_num;

#endif
