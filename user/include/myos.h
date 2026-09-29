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

/* --- разное --- */
unsigned int rand(void);
void srand(unsigned int seed);

#endif
