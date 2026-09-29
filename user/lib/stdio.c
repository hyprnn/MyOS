/*
 * user/lib/stdio.c - printf и строки с клавиатуры.
 * printf умеет: %d %i %u %x %X %p %s %c %% и ширину/ноль/минус
 * (%5d, %08x, %-10s), длинные %ld %lu %lld %llu %lx.
 */
#include "myos.h"

typedef struct { char *buf; size_t cap, len; } OUT;

static void put(OUT *o, char c)
{
    if (o->len + 1 < o->cap)
        o->buf[o->len] = c;
    o->len++;
}

static void pad(OUT *o, char c, int n) { while (n-- > 0) put(o, c); }

static void num(OUT *o, unsigned long long v, int base, int upper, int neg,
                int width, int zero, int left)
{
    char tmp[32];
    int n = 0;
    const char *dig = upper ? "0123456789ABCDEF" : "0123456789abcdef";

    do { tmp[n++] = dig[v % (unsigned)base]; v /= (unsigned)base; } while (v);

    int len = n + (neg ? 1 : 0);

    if (!left && !zero) pad(o, ' ', width - len);
    if (neg) put(o, '-');
    if (!left && zero) pad(o, '0', width - len);
    while (n) put(o, tmp[--n]);
    if (left) pad(o, ' ', width - len);
}

int vsnprintf(char *buf, size_t cap, const char *f, va_list ap)
{
    OUT o = { buf, cap, 0 };

    for (; *f; f++) {

        if (*f != '%') { put(&o, *f); continue; }

        f++;
        int left = 0, zero = 0, width = 0, lng = 0;

        if (*f == '-') { left = 1; f++; }
        if (*f == '0') { zero = 1; f++; }
        while (*f >= '0' && *f <= '9') width = width * 10 + (*f++ - '0');
        while (*f == 'l') { lng++; f++; }

        switch (*f) {
        case 'd': case 'i': {
            long long v = lng ? va_arg(ap, long long) : va_arg(ap, int);
            num(&o, v < 0 ? (unsigned long long)(-v) : (unsigned long long)v, 10, 0, v < 0,
                width, zero, left);
            break;
        }
        case 'u': {
            unsigned long long v = lng ? va_arg(ap, unsigned long long) : va_arg(ap, unsigned);
            num(&o, v, 10, 0, 0, width, zero, left);
            break;
        }
        case 'x': case 'X': {
            unsigned long long v = lng ? va_arg(ap, unsigned long long) : va_arg(ap, unsigned);
            num(&o, v, 16, *f == 'X', 0, width, zero, left);
            break;
        }
        case 'p':
            put(&o, '0'); put(&o, 'x');
            num(&o, (unsigned long long)va_arg(ap, void *), 16, 0, 0, width, 1, 0);
            break;
        case 's': {
            const char *s = va_arg(ap, const char *);
            if (!s) s = "(null)";
            int n = (int)strlen(s);
            if (!left) pad(&o, ' ', width - n);
            while (*s) put(&o, *s++);
            if (left) pad(&o, ' ', width - n);
            break;
        }
        case 'c':
            put(&o, (char)va_arg(ap, int));
            break;
        case '%':
            put(&o, '%');
            break;
        default:
            put(&o, '%');
            if (*f) put(&o, *f);
            else f--;
        }
    }

    if (cap > 0)
        buf[(o.len < cap) ? o.len : cap - 1] = '\0';

    return (int)o.len;
}

int snprintf(char *buf, size_t cap, const char *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    int n = vsnprintf(buf, cap, fmt, ap);
    va_end(ap);
    return n;
}

int printf(const char *fmt, ...)
{
    char buf[1024];
    va_list ap;
    va_start(ap, fmt);
    int n = vsnprintf(buf, sizeof(buf), fmt, ap);
    va_end(ap);
    write(1, buf, (n < (int)sizeof(buf)) ? (size_t)n : sizeof(buf) - 1);
    return n;
}

int fprintf_fd(int fd, const char *fmt, ...)
{
    char buf[1024];
    va_list ap;
    va_start(ap, fmt);
    int n = vsnprintf(buf, sizeof(buf), fmt, ap);
    va_end(ap);
    write(fd, buf, (n < (int)sizeof(buf)) ? (size_t)n : sizeof(buf) - 1);
    return n;
}

int putchar(int c) { char ch = (char)c; write(1, &ch, 1); return c; }

int puts(const char *s)
{
    write(1, s, strlen(s));
    write(1, "\n", 1);
    return 0;
}

char *getline_in(char *buf, size_t cap)
{
    long n = read(0, buf, cap - 1);

    if (n <= 0)
        return NULL;

    buf[n] = '\0';

    if (n > 0 && buf[n - 1] == '\n')
        buf[n - 1] = '\0';

    return buf;
}
