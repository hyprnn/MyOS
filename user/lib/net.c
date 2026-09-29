/*
 * user/lib/net.c - сеть для программ (этап 8): сокеты, DNS, сведения
 * об интерфейсах. Обёртки над системными вызовами SYS_SOCKET и
 * соседними (sysnum.h). Сокет - обычный fd: read/write/close тоже
 * работают. Адрес IPv4 - unsigned int "в порядке процессора":
 * 10.0.2.15 = 0x0A00020F (см. ip_to_str / str_to_ip).
 */
#include "myos.h"

long syscall4(long nr, long a1, long a2, long a3, long a4)
{
    long ret;
    register long r10 __asm__("r10") = a4;

    __asm__ __volatile__(
        "syscall"
        : "=a"(ret)
        : "a"(nr), "D"(a1), "S"(a2), "d"(a3), "r"(r10)
        : "rcx", "r11", "memory"
    );

    return ret;
}

int socket(int type)
{
    return (int)syscall3(SYS_SOCKET, type, 0, 0);
}

int connect(int fd, unsigned int ip, int port)
{
    struct myos_sockaddr a = { ip, (unsigned short)port, 0, 0 };

    return (int)syscall3(SYS_CONNECT, fd, (long)&a, 0);
}

int bind(int fd, unsigned int ip, int port)
{
    struct myos_sockaddr a = { ip, (unsigned short)port, 0, 0 };

    return (int)syscall3(SYS_BIND, fd, (long)&a, 0);
}

int listen(int fd, int backlog)
{
    return (int)syscall3(SYS_LISTEN, fd, backlog, 0);
}

int accept(int fd, struct myos_sockaddr *from)
{
    return (int)syscall3(SYS_ACCEPT, fd, (long)from, 0);
}

long send(int fd, const void *buf, size_t n)
{
    return syscall4(SYS_SENDTO, fd, (long)buf, (long)n, 0);
}

long recv(int fd, void *buf, size_t n)
{
    return syscall4(SYS_RECVFROM, fd, (long)buf, (long)n, 0);
}

long sendto(int fd, const void *buf, size_t n, unsigned int ip, int port)
{
    struct myos_sockaddr a = { ip, (unsigned short)port, 0, 0 };

    return syscall4(SYS_SENDTO, fd, (long)buf, (long)n, (long)&a);
}

long recvfrom(int fd, void *buf, size_t n, struct myos_sockaddr *from)
{
    return syscall4(SYS_RECVFROM, fd, (long)buf, (long)n, (long)from);
}

int sock_timeout(int fd, unsigned int ms)
{
    return (int)syscall3(SYS_SOCKOPT, fd, MYOS_SO_TIMEOUT, ms);
}

long send_all(int fd, const void *buf, size_t n)
{
    const char *p = (const char *)buf;
    size_t done = 0;

    while (done < n) {
        long r = send(fd, p + done, n - done);
        if (r <= 0)
            return (done > 0) ? (long)done : r;
        done += (size_t)r;
    }

    return (long)done;
}

int resolve(const char *name, unsigned int *ip)
{
    return (int)syscall3(SYS_RESOLVE, (long)name, (long)ip, 0);
}

int netinfo(int index, struct myos_netif *ni)
{
    return (int)syscall3(SYS_NETINFO, index, (long)ni, 0);
}

int netctl(struct myos_netctl *c)
{
    return (int)syscall3(SYS_NETCTL, (long)c, 0, 0);
}

/* "10.0.2.15" (буфер не меньше 16 байт) */
char *ip_to_str(unsigned int ip, char *buf)
{
    snprintf(buf, 16, "%u.%u.%u.%u", ip >> 24, (ip >> 16) & 255u, (ip >> 8) & 255u, ip & 255u);
    return buf;
}

/* "10.0.2.15" -> 0x0A00020F; 0 - не адрес */
int str_to_ip(const char *s, unsigned int *ip)
{
    unsigned int v = 0;

    for (int part = 0; part < 4; part++) {

        unsigned int n = 0;
        int digits = 0;

        while (*s >= '0' && *s <= '9') {
            n = n * 10u + (unsigned int)(*s - '0');
            s++;
            if (++digits > 3)
                return 0;
        }

        if (digits == 0 || n > 255)
            return 0;

        v = (v << 8) | n;

        if (part < 3) {
            if (*s != '.')
                return 0;
            s++;
        }
    }

    if (*s != '\0')
        return 0;

    *ip = v;
    return 1;
}
