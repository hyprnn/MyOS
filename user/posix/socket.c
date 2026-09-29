/*
 * user/posix/socket.c - сокеты BSD (socket/connect/poll/getaddrinfo...)
 * поверх системных вызовов MyOS. Часть MyOS (этап 9).
 *
 * ЗАЧЕМ
 * -----
 * Чужие сетевые программы (curl, а через него браузер) написаны под
 * сокеты BSD: адрес - struct sockaddr_in в сетевом порядке байтов,
 * ошибки - в errno, неблокирующий режим через fcntl(O_NONBLOCK),
 * ожидание нескольких соединений сразу - poll/select. У ядра MyOS свои
 * вызовы (sysnum.h: socket, connect, sendto, recvfrom, sockopt, poll,
 * resolve) с адресом struct myos_sockaddr (IP в порядке процессора) -
 * здесь перевод одного в другое.
 *
 * Сеть MyOS - IPv4 (TCP и UDP). IPv6 честно отвергаем (EAFNOSUPPORT):
 * curl тогда сам берёт IPv4.
 *
 * Что помним о сокете в программе (ядро этого не хранит): тип, адрес
 * собеседника (для getpeername) и "подсмотренные" байты: recv с
 * MSG_PEEK должен вернуть данные, не забирая их, а ядро так не умеет -
 * забираем и храним тут, следующий recv/read отдаст их первыми.
 */
#include <errno.h>
#include <fcntl.h>
#include <poll.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <sys/uio.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include "myos_sys.h"

#define SK_MAX   64            /* fd программы: 3 .. 3 + PROC_FDS */
#define PEEK_MAX 64

typedef struct {
    unsigned char      used;
    unsigned char      type;       /* SOCK_STREAM / SOCK_DGRAM */
    unsigned char      nonblock;
    struct sockaddr_in peer;       /* с кем соединён (connect/accept) */
    unsigned char      peek[PEEK_MAX];
    unsigned int       peek_len;
} SK;

static SK g_sk[SK_MAX];

int h_errno;
const struct in6_addr in6addr_any = IN6ADDR_ANY_INIT;
const struct in6_addr in6addr_loopback = IN6ADDR_LOOPBACK_INIT;

static SK *sk_of(int fd)
{
    if (fd < 0 || fd >= SK_MAX || !g_sk[fd].used)
        return NULL;

    return &g_sk[fd];
}

/* Для os.c: fd - сокет? (read/write/close/fcntl ведут себя иначе) */
int __myos_is_socket(int fd)
{
    return sk_of(fd) != NULL;
}

/* os.c: close() - забыть сокет */
void __myos_sock_forget(int fd)
{
    SK *k = sk_of(fd);

    if (k != NULL)
        memset(k, 0, sizeof(*k));
}

static int set_nonblock(int fd, SK *k, int on)
{
    long r = myos_syscall3(SYS_SOCKOPT, fd, MYOS_SO_NONBLOCK, on ? 1 : 0);

    if (r < 0)
        return myos_set_errno(r);

    k->nonblock = (unsigned char)(on != 0);
    return 0;
}

/* os.c: fcntl(F_GETFL/F_SETFL) и ioctl(FIONBIO) для сокетов */
int __myos_sock_nonblock(int fd, int set, int on)
{
    SK *k = sk_of(fd);

    if (k == NULL) {
        errno = EBADF;
        return -1;
    }

    if (!set)
        return k->nonblock;

    return set_nonblock(fd, k, on);
}

/* sockaddr (BSD, сетевой порядок) -> myos_sockaddr (порядок процессора) */
static int to_myos(const struct sockaddr *a, socklen_t len, struct myos_sockaddr *m)
{
    const struct sockaddr_in *in = (const struct sockaddr_in *)a;

    if (a == NULL || len < sizeof(struct sockaddr_in)) {
        errno = EINVAL;
        return -1;
    }

    if (a->sa_family != AF_INET) {
        errno = EAFNOSUPPORT;
        return -1;
    }

    memset(m, 0, sizeof(*m));
    m->ip = ntohl(in->sin_addr.s_addr);
    m->port = ntohs(in->sin_port);
    return 0;
}

/* и обратно; *len - сколько места у программы (обрезаем, как POSIX) */
static void from_myos(unsigned int ip, unsigned short port, struct sockaddr *a, socklen_t *len)
{
    struct sockaddr_in in;

    if (a == NULL || len == NULL)
        return;

    memset(&in, 0, sizeof(in));
    in.sin_family = AF_INET;
    in.sin_port = htons(port);
    in.sin_addr.s_addr = htonl(ip);

    memcpy(a, &in, (*len < sizeof(in)) ? *len : sizeof(in));
    *len = sizeof(in);
}

/* ================================================================
 * Сокеты
 * ================================================================ */

int socket(int domain, int type, int protocol)
{
    int base = type & ~(SOCK_NONBLOCK | SOCK_CLOEXEC);
    long kt;

    (void)protocol;

    if (domain != AF_INET) {
        errno = EAFNOSUPPORT;
        return -1;
    }

    if (base == SOCK_STREAM)
        kt = MYOS_SOCK_STREAM;
    else if (base == SOCK_DGRAM)
        kt = MYOS_SOCK_DGRAM;
    else {
        errno = EPROTONOSUPPORT;
        return -1;
    }

    long fd = myos_syscall3(SYS_SOCKET, kt, 0, 0);

    if (fd < 0)
        return myos_set_errno(fd);

    if (fd >= SK_MAX) {
        myos_syscall3(SYS_CLOSE, fd, 0, 0);
        errno = EMFILE;
        return -1;
    }

    SK *k = &g_sk[fd];

    memset(k, 0, sizeof(*k));
    k->used = 1;
    k->type = (unsigned char)base;

    if ((type & SOCK_NONBLOCK) && set_nonblock((int)fd, k, 1) < 0) {
        int e = errno;
        close((int)fd);
        errno = e;
        return -1;
    }

    return (int)fd;
}

int socketpair(int domain, int type, int protocol, int sv[2])
{
    (void)domain; (void)type; (void)protocol; (void)sv;
    errno = EOPNOTSUPP;
    return -1;
}

int connect(int fd, const struct sockaddr *addr, socklen_t len)
{
    SK *k = sk_of(fd);
    struct myos_sockaddr m;

    if (k == NULL) {
        errno = ENOTSOCK;
        return -1;
    }

    if (to_myos(addr, len, &m) < 0)
        return -1;

    memcpy(&k->peer, addr, sizeof(k->peer));

    long r = myos_syscall3(SYS_CONNECT, fd, (long)&m, 0);

    return (r < 0) ? myos_set_errno(r) : 0;
}

int bind(int fd, const struct sockaddr *addr, socklen_t len)
{
    struct myos_sockaddr m;

    if (sk_of(fd) == NULL) {
        errno = ENOTSOCK;
        return -1;
    }

    if (to_myos(addr, len, &m) < 0)
        return -1;

    long r = myos_syscall3(SYS_BIND, fd, (long)&m, 0);

    return (r < 0) ? myos_set_errno(r) : 0;
}

int listen(int fd, int backlog)
{
    long r = myos_syscall3(SYS_LISTEN, fd, backlog, 0);

    return (r < 0) ? myos_set_errno(r) : 0;
}

int accept(int fd, struct sockaddr *addr, socklen_t *len)
{
    struct myos_sockaddr m;
    long nfd = myos_syscall3(SYS_ACCEPT, fd, (long)&m, 0);

    if (nfd < 0)
        return myos_set_errno(nfd);

    if (nfd >= SK_MAX) {
        myos_syscall3(SYS_CLOSE, nfd, 0, 0);
        errno = EMFILE;
        return -1;
    }

    SK *k = &g_sk[nfd];
    socklen_t pl = sizeof(k->peer);

    memset(k, 0, sizeof(*k));
    k->used = 1;
    k->type = SOCK_STREAM;
    from_myos(m.ip, m.port, (struct sockaddr *)&k->peer, &pl);
    from_myos(m.ip, m.port, addr, len);

    return (int)nfd;
}

/* Отдать "подсмотренные" байты (MSG_PEEK) - они первые в очереди */
static size_t take_peek(SK *k, void *buf, size_t n, int keep)
{
    size_t c = (n < k->peek_len) ? n : k->peek_len;

    memcpy(buf, k->peek, c);

    if (!keep) {
        memmove(k->peek, k->peek + c, k->peek_len - c);
        k->peek_len -= (unsigned int)c;
    }

    return c;
}

ssize_t recvfrom(int fd, void *buf, size_t n, int flags, struct sockaddr *from, socklen_t *fromlen)
{
    SK *k = sk_of(fd);
    struct myos_sockaddr m;

    if (k == NULL) {
        errno = ENOTSOCK;
        return -1;
    }

    if (n == 0)
        return 0;

    /* есть подсмотренное - отдаём его, не спрашивая ядро */
    if (k->peek_len > 0) {
        if (from != NULL)
            memcpy(from, &k->peer, (fromlen && *fromlen < sizeof(k->peer)) ? *fromlen
                                                                             : sizeof(k->peer));
        return (ssize_t)take_peek(k, buf, n, flags & MSG_PEEK);
    }

    int was_nb = k->nonblock;

    if ((flags & MSG_DONTWAIT) && !was_nb)
        set_nonblock(fd, k, 1);

    size_t want = n;

    if ((flags & MSG_PEEK) && want > PEEK_MAX)
        want = PEEK_MAX;

    long r = myos_syscall4(SYS_RECVFROM, fd, (long)((flags & MSG_PEEK) ? (void *)k->peek : buf),
                           (long)want, (long)&m);

    if ((flags & MSG_DONTWAIT) && !was_nb)
        set_nonblock(fd, k, 0);

    if (r < 0)
        return myos_set_errno(r);

    if (flags & MSG_PEEK) {
        k->peek_len = (unsigned int)r;
        memcpy(buf, k->peek, (size_t)r);
    }

    if (from != NULL)
        from_myos(m.ip, m.port, from, fromlen);

    return (ssize_t)r;
}

ssize_t recv(int fd, void *buf, size_t n, int flags)
{
    return recvfrom(fd, buf, n, flags, NULL, NULL);
}

/* os.c: read() на сокете с подсмотренными байтами */
ssize_t __myos_sock_read(int fd, void *buf, size_t n)
{
    SK *k = sk_of(fd);

    if (k != NULL && k->peek_len > 0)
        return (ssize_t)take_peek(k, buf, n, 0);

    return -2;                 /* подсмотренного нет - обычный read */
}

ssize_t sendto(int fd, const void *buf, size_t n, int flags, const struct sockaddr *to,
               socklen_t tolen)
{
    SK *k = sk_of(fd);
    struct myos_sockaddr m;
    long r;

    if (k == NULL) {
        errno = ENOTSOCK;
        return -1;
    }

    int was_nb = k->nonblock;

    if ((flags & MSG_DONTWAIT) && !was_nb)
        set_nonblock(fd, k, 1);

    if (to != NULL && k->type == SOCK_DGRAM) {
        if (to_myos(to, tolen, &m) < 0)
            return -1;
        r = myos_syscall4(SYS_SENDTO, fd, (long)buf, (long)n, (long)&m);
    } else {
        r = myos_syscall4(SYS_SENDTO, fd, (long)buf, (long)n, 0);
    }

    if ((flags & MSG_DONTWAIT) && !was_nb)
        set_nonblock(fd, k, 0);

    /* MSG_NOSIGNAL: сигналов (SIGPIPE) в MyOS и так нет */
    return (r < 0) ? myos_set_errno(r) : (ssize_t)r;
}

ssize_t send(int fd, const void *buf, size_t n, int flags)
{
    return sendto(fd, buf, n, flags, NULL, 0);
}

ssize_t sendmsg(int fd, const struct msghdr *msg, int flags)
{
    ssize_t total = 0;

    for (size_t i = 0; i < msg->msg_iovlen; i++) {
        ssize_t r = sendto(fd, msg->msg_iov[i].iov_base, msg->msg_iov[i].iov_len, flags,
                           (const struct sockaddr *)msg->msg_name, msg->msg_namelen);
        if (r < 0)
            return total ? total : -1;
        total += r;
        if ((size_t)r < msg->msg_iov[i].iov_len)
            break;
    }

    return total;
}

ssize_t recvmsg(int fd, struct msghdr *msg, int flags)
{
    if (msg->msg_iovlen == 0)
        return 0;

    msg->msg_controllen = 0;
    msg->msg_flags = 0;
    return recvfrom(fd, msg->msg_iov[0].iov_base, msg->msg_iov[0].iov_len, flags,
                    (struct sockaddr *)msg->msg_name, &msg->msg_namelen);
}

ssize_t writev(int fd, const struct iovec *iov, int n)
{
    ssize_t total = 0;

    for (int i = 0; i < n; i++) {
        ssize_t r = write(fd, iov[i].iov_base, iov[i].iov_len);
        if (r < 0)
            return total ? total : -1;
        total += r;
        if ((size_t)r < iov[i].iov_len)
            break;
    }

    return total;
}

ssize_t readv(int fd, const struct iovec *iov, int n)
{
    ssize_t total = 0;

    for (int i = 0; i < n; i++) {
        ssize_t r = read(fd, iov[i].iov_base, iov[i].iov_len);
        if (r < 0)
            return total ? total : -1;
        total += r;
        if ((size_t)r < iov[i].iov_len)
            break;
    }

    return total;
}

int setsockopt(int fd, int level, int name, const void *val, socklen_t len)
{
    SK *k = sk_of(fd);

    if (k == NULL) {
        errno = ENOTSOCK;
        return -1;
    }

    if (level == SOL_SOCKET && name == SO_RCVTIMEO && val != NULL &&
        len >= sizeof(struct timeval)) {
        const struct timeval *tv = (const struct timeval *)val;
        long ms = (long)tv->tv_sec * 1000 + (long)tv->tv_usec / 1000;
        long r = myos_syscall3(SYS_SOCKOPT, fd, MYOS_SO_TIMEOUT, ms);
        return (r < 0) ? myos_set_errno(r) : 0;
    }

    /* остальное (Nagle, keepalive, размеры буферов, reuseaddr) ядро
       решает само - принимаем и ничего не делаем */
    return 0;
}

static int put_int(void *val, socklen_t *len, int v)
{
    if (val == NULL || len == NULL || *len < sizeof(int)) {
        errno = EINVAL;
        return -1;
    }

    memcpy(val, &v, sizeof(int));
    *len = sizeof(int);
    return 0;
}

int getsockopt(int fd, int level, int name, void *val, socklen_t *len)
{
    SK *k = sk_of(fd);

    if (k == NULL) {
        errno = ENOTSOCK;
        return -1;
    }

    if (level != SOL_SOCKET) {
        errno = ENOPROTOOPT;
        return -1;
    }

    switch (name) {
    case SO_ERROR: {
        /* итог неблокирующего connect: 0 - соединились */
        long r = myos_syscall3(SYS_SOCKOPT, fd, MYOS_SO_ERROR, 0);
        int e = 0;
        if (r < 0) {
            myos_set_errno(r);
            e = errno;
        }
        return put_int(val, len, e);
    }
    case SO_TYPE:
        return put_int(val, len, k->type);
    case SO_SNDBUF:
    case SO_RCVBUF:
        return put_int(val, len, 65536);
    case SO_KEEPALIVE:
    case SO_REUSEADDR:
        return put_int(val, len, 0);
    default:
        errno = ENOPROTOOPT;
        return -1;
    }
}

int getsockname(int fd, struct sockaddr *addr, socklen_t *len)
{
    if (sk_of(fd) == NULL) {
        errno = ENOTSOCK;
        return -1;
    }

    /* ядро отдаёт свой адрес и порт одним числом: ip << 16 | порт */
    long r = myos_syscall3(SYS_SOCKOPT, fd, MYOS_SO_LOCALADDR, 0);

    if (r < 0)
        return myos_set_errno(r);

    from_myos((unsigned int)((unsigned long)r >> 16), (unsigned short)(r & 0xFFFF), addr, len);
    return 0;
}

int getpeername(int fd, struct sockaddr *addr, socklen_t *len)
{
    SK *k = sk_of(fd);

    if (k == NULL) {
        errno = ENOTSOCK;
        return -1;
    }

    if (k->peer.sin_family != AF_INET) {
        errno = ENOTCONN;
        return -1;
    }

    memcpy(addr, &k->peer, (*len < sizeof(k->peer)) ? *len : sizeof(k->peer));
    *len = sizeof(k->peer);
    return 0;
}

int shutdown(int fd, int how)
{
    (void)how;

    /* "полузакрытия" ядро не умеет: закроется целиком в close() */
    if (sk_of(fd) == NULL) {
        errno = ENOTSOCK;
        return -1;
    }

    return 0;
}

/* ================================================================
 * poll / select
 * ================================================================ */

int poll(struct pollfd fds[], nfds_t n, int timeout)
{
    int ready = 0;

    /* подсмотренные байты ядро не видит - такие сокеты готовы сразу */
    for (nfds_t i = 0; i < n; i++) {
        SK *k = sk_of(fds[i].fd);
        if (k != NULL && k->peek_len > 0 && (fds[i].events & POLLIN))
            ready = 1;
    }

    long r = myos_syscall3(SYS_POLL, (long)fds, (long)n, ready ? 0 : timeout);

    if (r < 0)
        return myos_set_errno(r);

    if (ready) {
        r = 0;
        for (nfds_t i = 0; i < n; i++) {
            SK *k = sk_of(fds[i].fd);
            if (k != NULL && k->peek_len > 0 && (fds[i].events & POLLIN))
                fds[i].revents |= POLLIN;
            if (fds[i].revents)
                r++;
        }
    }

    return (int)r;
}

int select(int nfds, fd_set *rd, fd_set *wr, fd_set *ex, struct timeval *tv)
{
    struct pollfd p[SK_MAX];
    int n = 0;

    if (nfds > SK_MAX) {
        errno = EINVAL;
        return -1;
    }

    for (int fd = 0; fd < nfds; fd++) {
        short ev = 0;
        if (rd && FD_ISSET(fd, rd)) ev |= POLLIN;
        if (wr && FD_ISSET(fd, wr)) ev |= POLLOUT;
        if (ex && FD_ISSET(fd, ex)) ev |= POLLPRI;
        if (ev) {
            p[n].fd = fd;
            p[n].events = ev;
            p[n].revents = 0;
            n++;
        }
    }

    int timeout = (tv == NULL) ? -1 : (int)(tv->tv_sec * 1000 + tv->tv_usec / 1000);
    int r = poll(p, (nfds_t)n, timeout);

    if (r < 0)
        return -1;

    if (rd) FD_ZERO(rd);
    if (wr) FD_ZERO(wr);
    if (ex) FD_ZERO(ex);

    int count = 0;

    for (int i = 0; i < n; i++) {
        if (rd && (p[i].revents & (POLLIN | POLLHUP | POLLERR))) { FD_SET(p[i].fd, rd); count++; }
        if (wr && (p[i].revents & (POLLOUT | POLLERR))) { FD_SET(p[i].fd, wr); count++; }
        if (ex && (p[i].revents & POLLPRI)) { FD_SET(p[i].fd, ex); count++; }
    }

    return count;
}

/* ================================================================
 * Адреса: "1.2.3.4" <-> число
 * ================================================================ */

/* разбор "a.b.c.d" строго (4 числа до 255) -> порядок процессора */
static int parse_ip4(const char *s, uint32_t *out)
{
    uint32_t v = 0;

    for (int part = 0; part < 4; part++) {
        unsigned n = 0;
        int digits = 0;

        while (*s >= '0' && *s <= '9') {
            n = n * 10u + (unsigned)(*s - '0');
            s++;
            if (++digits > 3 || n > 255)
                return 0;
        }

        if (digits == 0)
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

    *out = v;
    return 1;
}

int inet_pton(int af, const char *src, void *dst)
{
    uint32_t v;

    if (af == AF_INET6)
        return 0;              /* IPv6 не поддерживаем: "не адрес" */

    if (af != AF_INET) {
        errno = EAFNOSUPPORT;
        return -1;
    }

    if (!parse_ip4(src, &v))
        return 0;

    v = htonl(v);
    memcpy(dst, &v, 4);
    return 1;
}

const char *inet_ntop(int af, const void *src, char *dst, socklen_t size)
{
    if (af != AF_INET) {
        errno = EAFNOSUPPORT;
        return NULL;
    }

    const unsigned char *b = (const unsigned char *)src;
    int n = snprintf(dst, size, "%u.%u.%u.%u", b[0], b[1], b[2], b[3]);

    if (n < 0 || (socklen_t)n >= size) {
        errno = ENOSPC;
        return NULL;
    }

    return dst;
}

uint32_t inet_addr(const char *cp)
{
    uint32_t v;

    return parse_ip4(cp, &v) ? htonl(v) : INADDR_NONE;
}

int inet_aton(const char *cp, struct in_addr *inp)
{
    uint32_t v;

    if (!parse_ip4(cp, &v))
        return 0;

    inp->s_addr = htonl(v);
    return 1;
}

char *inet_ntoa(struct in_addr in)
{
    static char buf[INET_ADDRSTRLEN];

    inet_ntop(AF_INET, &in.s_addr, buf, sizeof(buf));
    return buf;
}

/* ================================================================
 * DNS: getaddrinfo / gethostbyname
 * ================================================================ */

/* имя -> IPv4 (порядок процессора): число или вопрос к DNS ядра */
static int resolve4(const char *name, uint32_t *ip, int numeric_only)
{
    if (parse_ip4(name, ip))
        return 0;

    if (numeric_only)
        return EAI_NONAME;

    if (strcmp(name, "localhost") == 0) {
        *ip = INADDR_LOOPBACK;
        return 0;
    }

    unsigned int v = 0;
    long r = myos_syscall3(SYS_RESOLVE, (long)name, (long)&v, 0);

    if (r == MYOS_EHOSTNOTFOUND)
        return EAI_NONAME;
    if (r < 0)
        return (r == MYOS_ETIMEDOUT || r == MYOS_EAGAIN) ? EAI_AGAIN : EAI_FAIL;

    *ip = v;
    return 0;
}

static int service_port(const char *service, int numeric_only, unsigned *port)
{
    char *end;
    long v;

    if (service == NULL) {
        *port = 0;
        return 0;
    }

    v = strtol(service, &end, 10);

    if (*service != '\0' && *end == '\0' && v >= 0 && v <= 65535) {
        *port = (unsigned)v;
        return 0;
    }

    if (numeric_only)
        return EAI_NONAME;

    static const struct { const char *name; unsigned port; } known[] = {
        { "http", 80 }, { "https", 443 }, { "ftp", 21 }, { "ssh", 22 },
        { "domain", 53 }, { "ntp", 123 }, { "imap", 143 }, { "smtp", 25 },
    };

    for (size_t i = 0; i < sizeof(known) / sizeof(known[0]); i++)
        if (strcmp(service, known[i].name) == 0) {
            *port = known[i].port;
            return 0;
        }

    return EAI_SERVICE;
}

int getaddrinfo(const char *node, const char *service, const struct addrinfo *hints,
                struct addrinfo **res)
{
    int flags = hints ? hints->ai_flags : 0;
    int family = hints ? hints->ai_family : AF_UNSPEC;
    int socktype = hints ? hints->ai_socktype : 0;
    uint32_t ip;
    unsigned port;
    int e;

    *res = NULL;

    if (node == NULL && service == NULL)
        return EAI_NONAME;

    if (family != AF_UNSPEC && family != AF_INET)
        return EAI_FAMILY;

    if ((e = service_port(service, flags & AI_NUMERICSERV, &port)) != 0)
        return e;

    if (node == NULL)
        ip = (flags & AI_PASSIVE) ? INADDR_ANY : INADDR_LOOPBACK;
    else if ((e = resolve4(node, &ip, flags & AI_NUMERICHOST)) != 0)
        return e;

    /* одна запись на каждый тип сокета, о котором спросили (или оба) */
    int types[2], nt = 0;

    if (socktype == 0) {
        types[nt++] = SOCK_STREAM;
        types[nt++] = SOCK_DGRAM;
    } else {
        types[nt++] = socktype;
    }

    struct addrinfo *head = NULL, **tail = &head;

    for (int i = 0; i < nt; i++) {

        /* запись, адрес и имя - одним куском: freeaddrinfo = free */
        size_t nl = (node && (flags & AI_CANONNAME)) ? strlen(node) + 1 : 0;
        struct addrinfo *ai = calloc(1, sizeof(*ai) + sizeof(struct sockaddr_in) + nl);

        if (ai == NULL) {
            freeaddrinfo(head);
            return EAI_MEMORY;
        }

        struct sockaddr_in *sa = (struct sockaddr_in *)(ai + 1);

        sa->sin_family = AF_INET;
        sa->sin_port = htons((uint16_t)port);
        sa->sin_addr.s_addr = htonl(ip);

        ai->ai_family = AF_INET;
        ai->ai_socktype = types[i];
        ai->ai_protocol = (types[i] == SOCK_STREAM) ? IPPROTO_TCP : IPPROTO_UDP;
        ai->ai_addrlen = sizeof(*sa);
        ai->ai_addr = (struct sockaddr *)sa;

        if (nl) {
            ai->ai_canonname = (char *)(sa + 1);
            memcpy(ai->ai_canonname, node, nl);
        }

        *tail = ai;
        tail = &ai->ai_next;
    }

    *res = head;
    return 0;
}

void freeaddrinfo(struct addrinfo *res)
{
    while (res != NULL) {
        struct addrinfo *next = res->ai_next;
        free(res);
        res = next;
    }
}

const char *gai_strerror(int err)
{
    switch (err) {
    case 0:            return "Success";
    case EAI_NONAME:   return "Name or service not known";
    case EAI_AGAIN:    return "Temporary failure in name resolution";
    case EAI_FAIL:     return "Non-recoverable failure in name resolution";
    case EAI_FAMILY:   return "Address family not supported";
    case EAI_SOCKTYPE: return "Socket type not supported";
    case EAI_SERVICE:  return "Service not known";
    case EAI_MEMORY:   return "Out of memory";
    case EAI_BADFLAGS: return "Bad flags";
    default:           return "Unknown error";
    }
}

int getnameinfo(const struct sockaddr *sa, socklen_t salen, char *host, socklen_t hostlen,
                char *serv, socklen_t servlen, int flags)
{
    const struct sockaddr_in *in = (const struct sockaddr_in *)sa;

    (void)flags;               /* обратного DNS нет - всегда числом */

    if (sa == NULL || salen < sizeof(*in) || sa->sa_family != AF_INET)
        return EAI_FAMILY;

    if (host != NULL && hostlen > 0 && inet_ntop(AF_INET, &in->sin_addr, host, hostlen) == NULL)
        return EAI_OVERFLOW;

    if (serv != NULL && servlen > 0 &&
        snprintf(serv, servlen, "%u", ntohs(in->sin_port)) >= (int)servlen)
        return EAI_OVERFLOW;

    return 0;
}

struct hostent *gethostbyname(const char *name)
{
    static struct hostent h;
    static char namebuf[256];
    static uint32_t addr;
    static char *addrs[2];
    static char *aliases[1];
    uint32_t ip;

    if (resolve4(name, &ip, 0) != 0) {
        h_errno = HOST_NOT_FOUND;
        return NULL;
    }

    strncpy(namebuf, name, sizeof(namebuf) - 1);
    addr = htonl(ip);
    addrs[0] = (char *)&addr;
    addrs[1] = NULL;
    aliases[0] = NULL;
    h.h_name = namebuf;
    h.h_aliases = aliases;
    h.h_addrtype = AF_INET;
    h.h_length = 4;
    h.h_addr_list = addrs;
    return &h;
}

struct servent *getservbyname(const char *name, const char *proto)
{
    static struct servent s;
    static char *aliases[1];
    unsigned port;

    if (service_port(name, 0, &port) != 0)
        return NULL;

    s.s_name = (char *)name;
    s.s_aliases = aliases;
    s.s_port = htons((uint16_t)port);
    s.s_proto = (char *)(proto ? proto : "tcp");
    return &s;
}
