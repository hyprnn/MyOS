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
int  gettime_utc(struct myos_time *t);     /* всемирное время (UTC) */
long getrandom(void *buf, size_t n);       /* случайные байты (до 4096) */
int  readdir(const char *path, int index, struct myos_dirent *d);
int  stat(const char *path, struct myos_dirent *d);
int  mkdir(const char *path);
int  unlink(const char *path);
int  rename(const char *from, const char *to);
int  yield(void);
int  getkey(void);
/* этап 10: шелл-программа */
int  chdir(const char *path);
long getcwd_len(char *buf, size_t cap);                 /* -> длина пути */
int  spawn(const struct myos_spawn *sp);                /* -> pid */
int  waitpid_info(int pid, struct myos_waitinfo *wi, int flags);  /* 1 кончилась, 0 нет */
int  readkey(long timeout_ms);                          /* -1 - ждать сколько угодно */
int  kcmd(const char *line, const char *out_path, int flags);     /* 1 - команда ядра */
/* звук (этап 10): 48000 Гц, 16 бит, стерео */
int  audio_open(void);                                  /* 0 / MYOS_EBUSY / MYOS_ENODEV */
long audio_write(const void *buf, size_t n);            /* ждёт места -> записано байт */
int  audio_close(void);
int  audio_drain(void);                                 /* дождаться конца звука */
int  audio_info(struct myos_audio_info *i);
int  audio_volume(int v);                               /* -1 узнать, -2 вкл/выкл */

/* user/lib/files.c - общее для команд файлов (ls, cp, mv, write...) */
const char *path_base(const char *p);                   /* "a/b/c.txt" -> "c.txt" */
int  path_normalize(const char *path, char *out, size_t cap);   /* без "." и ".." */
void file_err(const char *what, const char *path, int e);       /* "cp x: no such..." */
int  copy_file(const char *from, const char *to, unsigned long long *copied);
int  file_put_words(int argc, char **argv, int first, int append); /* write, append */

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

/* --- TLS / HTTPS (user/tls/, на BearSSL) --- */
typedef struct myos_tls TLS;
#define TLS_NO_VERIFY 1        /* не проверять сертификат (--no-check-certificate) */
/* рукопожатие поверх соединённого TCP-сокета fd; host - имя сайта (для
   проверки сертификата); ca_file - PEM с доп. корнями или NULL.
   NULL - не вышло, причина - в err */
TLS  *tls_open(int fd, const char *host, int flags, const char *ca_file,
               char *err, size_t errcap);
long  tls_send(TLS *t, const void *buf, size_t n);
long  tls_recv(TLS *t, void *buf, size_t n);          /* 0 - конец, <0 - ошибка */
void  tls_error(TLS *t, char *err, size_t cap);
void  tls_info(TLS *t, char *buf, size_t cap);        /* версия и шифр */
void  tls_close(TLS *t);

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

/* --- рисование как у окон ядра (этап 10, user/lib/gfx.c): шрифт 8x16
   с кириллицей, рамки Win95, значки 16x16 --- */
#ifndef FONT_W
#define FONT_W 8
#define FONT_H 16
#endif
#define GFX_WHITE  0xFFFFFFu
#define GFX_LIGHT  0xDFDFDFu
#define GFX_FACE   0xC0C0C0u     /* серый фон окон и кнопок */
#define GFX_SHADOW 0x808080u
#define GFX_BLACK  0x000000u
#define GFX_NAVY   0x000080u

typedef struct {
    unsigned int *px;
    int w, h, stride;
    int cx0, cy0, cx1, cy1;      /* отсечение (clip) */
} GFX;

void gfx_init(GFX *g, unsigned int *px, int w, int h);
void gfx_noclip(GFX *g);
void gfx_clip(GFX *g, int x, int y, int w, int h);
void gfx_fill(GFX *g, int x, int y, int w, int h, unsigned int col);
void gfx_pixel(GFX *g, int x, int y, unsigned int col);
void gfx_bevel(GFX *g, int x, int y, int w, int h, int raised);
void gfx_button(GFX *g, int x, int y, int w, int h, int pressed);
void gfx_glyph(GFX *g, int x, int y, unsigned int cp, unsigned int col);
int  gfx_text(GFX *g, int x, int y, const char *s, unsigned int col);      /* -> ширина */
int  gfx_text_fit(GFX *g, int x, int y, const char *s, unsigned int col, int max_chars);
int  gfx_text_bold(GFX *g, int x, int y, const char *s, unsigned int col);
int  gfx_text_width(const char *s);
void gfx_icon(GFX *g, int x, int y, const char *const *rows, int scale, int selected);
const char *const *gfx_icon_rows(const char *name);    /* "logo", "mine"...; нет - "app" */
unsigned int utf8_next(const char **s);
int  utf8_put(unsigned int c, char *out);                /* out >= 4 байт */
int  utf8_len(const char *s);
