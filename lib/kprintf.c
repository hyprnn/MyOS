/*
 * lib/kprintf.c - форматированный вывод в духе printf.
 * Часть MyOS; общие объявления - в myos.h.
 *
 * Вместо цепочек
 *     print(out, "TSC: "); print_uint(out, mhz); print(out, " MHz\n");
 * можно писать
 *     kprintf(out, "TSC: %u MHz\n", mhz);
 *
 * Поддерживается (сознательно немного, ровно то, что нужно ОС):
 *   %s  - строка char*          %S  - строка CHAR16* (UEFI)
 *   %c  - символ                %%  - знак процента
 *   %d / %i - знаковое целое    %u  - беззнаковое
 *   %x / %X - шестнадцатеричное (строчные / заглавные буквы)
 *   %p  - указатель (0x + 16 цифр)
 *   ширина и заполнение нулями: %8x, %016llx, %5d, %-10s
 *   модификаторы длины: l, ll, z (UINTN/UINT64) - без них число
 *   считается 32-битным (int / unsigned int), как в настоящем printf
 *
 * <stdarg.h> - заголовок самого компилятора (не libc), он доступен
 * и в -ffreestanding.
 */
#include "myos.h"
#include <stdarg.h>

typedef struct {
    char  *buf;
    UINTN  cap;     /* сколько байт можно записать (с учётом '\0') */
    UINTN  len;     /* сколько символов "хотело" записаться */
} KFMT_OUT;

static void kfmt_put(KFMT_OUT *o, char c)
{
    if (o->len + 1 < o->cap)
        o->buf[o->len] = c;

    o->len++;
}

static void kfmt_pad(KFMT_OUT *o, char c, int n)
{
    while (n-- > 0)
        kfmt_put(o, c);
}

/* Число value в системе base -> в o, с шириной и выравниванием */
static void kfmt_num(
    KFMT_OUT *o, UINT64 value, BOOLEAN negative, UINT32 base,
    BOOLEAN upper, int width, BOOLEAN zero_pad, BOOLEAN left
)
{
    const char *digits = upper ? "0123456789ABCDEF" : "0123456789abcdef";
    char tmp[24];
    int n = 0;

    do {
        tmp[n++] = digits[value % base];
        value /= base;
    } while (value != 0 && n < 22);

    int total = n + (negative ? 1 : 0);

    if (!left && !zero_pad)
        kfmt_pad(o, ' ', width - total);

    if (negative)
        kfmt_put(o, '-');

    if (!left && zero_pad)
        kfmt_pad(o, '0', width - total);

    while (n > 0)
        kfmt_put(o, tmp[--n]);

    if (left)
        kfmt_pad(o, ' ', width - total);
}

UINTN kvsnprintf(char *buf, UINTN cap, const char *fmt, va_list ap)
{
    KFMT_OUT o;

    o.buf = buf;
    o.cap = cap;
    o.len = 0;

    while (*fmt) {

        if (*fmt != '%') {
            kfmt_put(&o, *fmt++);
            continue;
        }

        fmt++;

        BOOLEAN left = FALSE, zero_pad = FALSE;

        while (*fmt == '-' || *fmt == '0') {
            if (*fmt == '-') left = TRUE;
            if (*fmt == '0') zero_pad = TRUE;
            fmt++;
        }

        int width = 0;

        while (*fmt >= '0' && *fmt <= '9')
            width = width * 10 + (*fmt++ - '0');

        int longness = 0;   /* 0 = int, 1+ = 64 бита */

        while (*fmt == 'l' || *fmt == 'z') {
            longness++;
            fmt++;
        }

        char conv = *fmt ? *fmt++ : '\0';

        switch (conv) {

        case 'd':
        case 'i': {
            INT64 v = longness ? va_arg(ap, INT64) : (INT64)va_arg(ap, int);
            BOOLEAN neg = v < 0;
            UINT64 mag = neg ? (UINT64)(-(v + 1)) + 1u : (UINT64)v;
            kfmt_num(&o, mag, neg, 10, FALSE, width, zero_pad, left);
            break;
        }

        case 'u':
        case 'x':
        case 'X': {
            UINT64 v = longness ? va_arg(ap, UINT64)
                                : (UINT64)va_arg(ap, unsigned int);
            kfmt_num(&o, v, FALSE, conv == 'u' ? 10 : 16, conv == 'X',
                     width, zero_pad, left);
            break;
        }

        case 'p': {
            UINT64 v = (UINT64)(UINTN)va_arg(ap, void *);
            kfmt_put(&o, '0');
            kfmt_put(&o, 'x');
            kfmt_num(&o, v, FALSE, 16, TRUE, 16, TRUE, FALSE);
            break;
        }

        case 'c':
            kfmt_pad(&o, ' ', left ? 0 : width - 1);
            kfmt_put(&o, (char)va_arg(ap, int));
            kfmt_pad(&o, ' ', left ? width - 1 : 0);
            break;

        case 's':
        case 'S': {
            int n = 0;
            const char *s8 = NULL;
            const CHAR16 *s16 = NULL;

            if (conv == 's') {
                s8 = va_arg(ap, const char *);
                if (s8 == NULL) s8 = "(null)";
                while (s8[n]) n++;
            } else {
                s16 = va_arg(ap, const CHAR16 *);
                if (s16 == NULL) { s8 = "(null)"; while (s8[n]) n++; }
                else while (s16[n]) n++;
            }

            if (!left)
                kfmt_pad(&o, ' ', width - n);

            for (int k = 0; k < n; k++) {
                if (s16 != NULL)
                    kfmt_put(&o, s16[k] < 128 ? (char)s16[k] : '?');
                else
                    kfmt_put(&o, s8[k]);
            }

            if (left)
                kfmt_pad(&o, ' ', width - n);
            break;
        }

        case '%':
            kfmt_put(&o, '%');
            break;

        default:
            /* неизвестный формат - печатаем как есть, чтобы ошибку
               в строке формата было видно, а не молча терять */
            kfmt_put(&o, '%');
            if (conv) kfmt_put(&o, conv);
            break;
        }
    }

    if (o.cap > 0)
        o.buf[(o.len < o.cap) ? o.len : o.cap - 1] = '\0';

    return o.len;
}

UINTN ksnprintf(char *buf, UINTN cap, const char *fmt, ...)
{
    va_list ap;

    va_start(ap, fmt);
    UINTN n = kvsnprintf(buf, cap, fmt, ap);
    va_end(ap);

    return n;
}

/* В консоль (через print - то есть и в историю экрана, и, в
   kernel mode, в COM1). Длинные строки режутся на 512 символах. */
void kprintf(SIMPLE_TEXT_OUTPUT_INTERFACE *out, const char *fmt, ...)
{
    char buf[512];
    va_list ap;

    va_start(ap, fmt);
    kvsnprintf(buf, sizeof(buf), fmt, ap);
    va_end(ap);

    print(out, buf);
}

/* Только в отладочный лог COM1, с отметкой времени kernel mode:
   "[   12.345] сообщение". На экране не появляется. */
void klog(const char *fmt, ...)
{
    if (!g_serial_ok)
        return;

    char buf[512];
    va_list ap;
    UINT64 us = kx_uptime_us();

    ksnprintf(buf, sizeof(buf), "[%5llu.%03llu] ",
              us / 1000000u, (us / 1000u) % 1000u);
    serial_puts(buf);

    va_start(ap, fmt);
    kvsnprintf(buf, sizeof(buf), fmt, ap);
    va_end(ap);

    serial_puts(buf);
}
