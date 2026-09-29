/*
 * user/lib/string.c - строки и память (как в обычной libc).
 */
#include "myos.h"

size_t strlen(const char *s) { size_t n = 0; while (s[n]) n++; return n; }

int strcmp(const char *a, const char *b)
{
    while (*a && *a == *b) { a++; b++; }
    return (unsigned char)*a - (unsigned char)*b;
}

int strncmp(const char *a, const char *b, size_t n)
{
    for (size_t i = 0; i < n; i++) {
        if (a[i] != b[i] || !a[i])
            return (unsigned char)a[i] - (unsigned char)b[i];
    }
    return 0;
}

char *strcpy(char *d, const char *s) { char *r = d; while ((*d++ = *s++)) { } return r; }

char *strncpy(char *d, const char *s, size_t n)
{
    size_t i = 0;
    for (; i < n && s[i]; i++) d[i] = s[i];
    for (; i < n; i++) d[i] = 0;
    return d;
}

char *strcat(char *d, const char *s) { strcpy(d + strlen(d), s); return d; }

char *strchr(const char *s, int c)
{
    for (; *s; s++) if (*s == (char)c) return (char *)s;
    return (c == 0) ? (char *)s : NULL;
}

void *memcpy(void *d, const void *s, size_t n)
{
    unsigned char *a = d; const unsigned char *b = s;
    while (n--) *a++ = *b++;
    return d;
}

void *memmove(void *d, const void *s, size_t n)
{
    unsigned char *a = d; const unsigned char *b = s;
    if (a < b) { while (n--) *a++ = *b++; }
    else { a += n; b += n; while (n--) *--a = *--b; }
    return d;
}

void *memset(void *d, int c, size_t n)
{
    unsigned char *a = d;
    while (n--) *a++ = (unsigned char)c;
    return d;
}

int memcmp(const void *x, const void *y, size_t n)
{
    const unsigned char *a = x, *b = y;
    for (size_t i = 0; i < n; i++) if (a[i] != b[i]) return a[i] - b[i];
    return 0;
}

int isdigit(int c) { return c >= '0' && c <= '9'; }
int isspace(int c) { return c == ' ' || c == '\t' || c == '\n' || c == '\r'; }
int isalpha(int c) { return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z'); }
int toupper(int c) { return (c >= 'a' && c <= 'z') ? c - 'a' + 'A' : c; }
int tolower(int c) { return (c >= 'A' && c <= 'Z') ? c - 'A' + 'a' : c; }

long atol(const char *s)
{
    long v = 0; int neg = 0;
    while (isspace(*s)) s++;
    if (*s == '-') { neg = 1; s++; } else if (*s == '+') s++;
    while (isdigit(*s)) v = v * 10 + (*s++ - '0');
    return neg ? -v : v;
}

int atoi(const char *s) { return (int)atol(s); }

const char *strerror(int e)
{
    switch (e) {
    case MYOS_ENOENT:    return "no such file or folder";
    case MYOS_EEXIST:    return "already exists";
    case MYOS_ENOTDIR:   return "not a folder";
    case MYOS_EISDIR:    return "is a folder";
    case MYOS_ENOTEMPTY: return "folder is not empty";
    case MYOS_ENOSPC:    return "no space left";
    case MYOS_EROFS:     return "read-only";
    case MYOS_EIO:       return "disk error";
    case MYOS_EINVAL:    return "bad argument";
    case MYOS_EBADF:     return "bad file number";
    case MYOS_EMFILE:    return "too many open files";
    case MYOS_ENOSYS:    return "not supported";
    case MYOS_EGONE:     return "the disk was removed";
    case MYOS_EXDEV:     return "different disks";
    case MYOS_EFAULT:    return "bad pointer";
    default:             return "error";
    }
}

static unsigned int g_seed = 12345;
void srand(unsigned int s) { g_seed = s ? s : 1; }
unsigned int rand(void)
{
    /* xorshift - простой и неплохой генератор */
    g_seed ^= g_seed << 13;
    g_seed ^= g_seed >> 17;
    g_seed ^= g_seed << 5;
    return g_seed;
}
