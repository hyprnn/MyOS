/*
 * user/include/myos.h - всё, что нужно программе для MyOS:
 * системные вызовы и маленькая "libc" (строки, printf, malloc).
 * Программы MyOS не могут пользоваться обычной libc Linux: у MyOS
 * свои системные вызовы (sysnum.h). Эта библиотека - в user/lib/.
 */
#ifndef MYOS_USER_H
#define MYOS_USER_H

#include "../../sysnum.h"

typedef unsigned long      size_t;
typedef long               ssize_t;
typedef unsigned long long uint64_t;
typedef long long          int64_t;
typedef unsigned int       uint32_t;
typedef int                int32_t;
typedef unsigned short     uint16_t;
typedef unsigned char      uint8_t;

#define NULL ((void *)0)

typedef __builtin_va_list va_list;
#define va_start(v, l) __builtin_va_start(v, l)
#define va_end(v)      __builtin_va_end(v)
#define va_arg(v, t)   __builtin_va_arg(v, t)

/* --- системные вызовы (user/lib/sys.c) --- */
long syscall3(long nr, long a1, long a2, long a3);
void exit(int code) __attribute__((noreturn));
long write(int fd, const void *buf, size_t n);
long read(int fd, void *buf, size_t n);
int  open(const char *path, int flags);
int  close(int fd);
int  sleep_ms(unsigned long ms);
unsigned long uptime_ms(void);
void *sbrk(long inc);
int  getpid(void);
int  gettime(struct myos_time *t);
int  readdir(const char *path, int index, struct myos_dirent *d);
int  stat(const char *path, struct myos_dirent *d);
int  mkdir(const char *path);
int  unlink(const char *path);
int  rename(const char *from, const char *to);
int  yield(void);
int  getkey(void);

/* --- строки (user/lib/string.c) --- */
size_t strlen(const char *s);
int    strcmp(const char *a, const char *b);
int    strncmp(const char *a, const char *b, size_t n);
char  *strcpy(char *d, const char *s);
char  *strncpy(char *d, const char *s, size_t n);
char  *strcat(char *d, const char *s);
char  *strchr(const char *s, int c);
char  *strstr(const char *s, const char *sub);
void  *memcpy(void *d, const void *s, size_t n);
void  *memmove(void *d, const void *s, size_t n);
void  *memset(void *d, int c, size_t n);
int    memcmp(const void *a, const void *b, size_t n);
int    atoi(const char *s);
long   atol(const char *s);
int    isdigit(int c);
int    isspace(int c);
int    isalpha(int c);
int    toupper(int c);
int    tolower(int c);
const char *strerror(int err);

/* --- ввод-вывод (user/lib/stdio.c) --- */
int  putchar(int c);
int  puts(const char *s);
int  printf(const char *fmt, ...);
int  snprintf(char *buf, size_t cap, const char *fmt, ...);
int  vsnprintf(char *buf, size_t cap, const char *fmt, va_list ap);
int  fprintf_fd(int fd, const char *fmt, ...);
char *getline_in(char *buf, size_t cap);   /* строка с клавиатуры без '\n'; NULL - конец */

/* --- память (user/lib/malloc.c) --- */
void *malloc(size_t n);
void  free(void *p);
void *calloc(size_t n, size_t sz);
void *realloc(void *p, size_t n);

/* --- сеть (этап 8, user/lib/net.c) --- */
long syscall4(long nr, long a1, long a2, long a3, long a4);
int  socket(int type);                                  /* MYOS_SOCK_STREAM/DGRAM/PING */
int  connect(int fd, unsigned int ip, int port);
int  bind(int fd, unsigned int ip, int port);
int  listen(int fd, int backlog);
int  accept(int fd, struct myos_sockaddr *from);        /* -> новый fd */
long send(int fd, const void *buf, size_t n);
long send_all(int fd, const void *buf, size_t n);       /* отправить всё */
long recv(int fd, void *buf, size_t n);                 /* 0 - собеседник закрыл */
long sendto(int fd, const void *buf, size_t n, unsigned int ip, int port);
long recvfrom(int fd, void *buf, size_t n, struct myos_sockaddr *from);
int  sock_timeout(int fd, unsigned int ms);             /* 0 - ждать сколько угодно */
int  resolve(const char *name, unsigned int *ip);       /* DNS */
int  netinfo(int index, struct myos_netif *ni);         /* 1 - есть, 0 - конец */
int  netctl(struct myos_netctl *c);
char *ip_to_str(unsigned int ip, char *buf);            /* buf >= 16 байт */
int  str_to_ip(const char *s, unsigned int *ip);        /* 1 - разобрали */

/* --- разное --- */
unsigned int rand(void);
void srand(unsigned int seed);

#endif

/* --- окна (этап 7, user/lib/win.c) --- */
int   win_create(int w, int h, const char *title); /* -> id окна, <0 ошибка */
void *win_pixels(int id);                          /* буфер cw*ch, 0x00RRGGBB */
int   win_update(int id);                          /* показать нарисованное */
int   win_event(int id, struct myos_event *e, int wait_ms); /* 1 - есть событие */
int   win_close(int id);
int   win_title(int id, const char *title);
/* рисование в буфер окна (буфер w*h пикселей, строка = w) */
void  gpx(unsigned int *buf, int w, int h, int x, int y, unsigned int col);
void  gfill(unsigned int *buf, int w, int h, int x, int y, int rw, int rh, unsigned int col);
void  gtext(unsigned int *buf, int w, int h, int x, int y, const char *s, unsigned int col);
int   gtextw(const char *s);
