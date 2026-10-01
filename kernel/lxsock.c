/*
 * kernel/lxsock.c - сокеты, epoll и timerfd для программ Linux
 * (этап 11, шаг 2). Часть MyOS; общие объявления - в myos.h и
 * kernel/linux.h.
 *
 * СОКЕТЫ AF_INET (TCP, UDP)
 * -------------------------
 * Сеть у MyOS уже есть (net/: TCP, UDP, DNS - на ней работают wget и
 * браузер NetSurf). Сокет Linux здесь - обёртка над сокетом MyOS
 * (LFILE.sock): адреса переводятся из "сетевого" порядка байт Linux в
 * порядок MyOS, ошибки - в номера Linux. Сокет MyOS всегда
 * неблокирующий, а ждём здесь сами - так ожидание прерывается
 * сигналами Linux (Ctrl+C, SIGALRM), как в настоящем Linux.
 *
 * СОКЕТЫ AF_UNIX
 * --------------
 * "Сокет в файле": программы одной машины говорят друг с другом
 * (Wayland, X11, D-Bus) и - главное для Firefox - его процессы между
 * собой (socketpair). Это чисто наш код, сеть тут ни при чём:
 *   * у каждого конца (LUSOCK) - очередь пришедших сообщений;
 *     send кладёт сообщение в очередь собеседника;
 *   * SOCK_STREAM - поток байтов (сообщения сливаются при чтении),
 *     SOCK_SEQPACKET и SOCK_DGRAM - по одному сообщению за раз;
 *   * к сообщению можно приложить ОТКРЫТЫЕ ФАЙЛЫ (SCM_RIGHTS): так
 *     Wayland передаёт память с картинкой окна (memfd), а Firefox -
 *     каналы и общую память между процессами. Получатель получает
 *     новые номера на те же открытые файлы;
 *   * bind(путь) регистрирует имя (и создаёт пустой файл, чтобы
 *     stat() видел сокет), connect(путь) находит слушающего, accept
 *     выдаёт серверный конец соединения. Имена "абстрактные" (первый
 *     байт 0) - только в нашей таблице.
 *
 * EPOLL
 * -----
 * Список "следить за этими номерами"; epoll_wait спрашивает у каждого
 * файла готовность (как poll) и ждёт изменений. Режим EPOLLET ("по
 * фронту") упрощён: готовность на чтение сообщается, пока данные
 * есть (программы с EPOLLET всё равно читают до EAGAIN), а
 * готовность на запись - только при переходе "нельзя -> можно"
 * (иначе программа крутилась бы без конца). EPOLLONESHOT - после
 * сообщения номер молчит до EPOLL_CTL_MOD.
 *
 * TIMERFD
 * -------
 * Таймер в виде файла: читается (8 байт - сколько раз сработал),
 * когда время пришло; виден в poll/epoll.
 */
#include "myos.h"
#include "linux.h"

#define LX_AF_UNIX      1
#define LX_AF_INET      2
#define LX_AF_INET6     10
#define LX_AF_NETLINK   16

#define LX_SOCK_STREAM     1
#define LX_SOCK_DGRAM      2
#define LX_SOCK_SEQPACKET  5
#define LX_SOCK_TYPEMASK   0xF
#define LX_SOCK_NONBLOCK   0x800
#define LX_SOCK_CLOEXEC    0x80000

#define LX_MSG_PEEK        0x2
#define LX_MSG_CTRUNC      0x8
#define LX_MSG_TRUNC       0x20
#define LX_MSG_DONTWAIT    0x40
#define LX_MSG_WAITALL     0x100
#define LX_MSG_NOSIGNAL    0x4000
#define LX_MSG_CMSG_CLOEXEC 0x40000000

#define LX_SOL_SOCKET      1
#define LX_SCM_RIGHTS      1
#define LX_SO_TYPE         3
#define LX_SO_ERROR        4
#define LX_SO_SNDBUF       7
#define LX_SO_RCVBUF       8
#define LX_SO_PEERCRED     17
#define LX_SO_RCVTIMEO     20
#define LX_SO_ACCEPTCONN   30
#define LX_SO_PROTOCOL     38
#define LX_SO_DOMAIN       39

#define LX_POLLIN   0x001
#define LX_POLLOUT  0x004
#define LX_POLLERR  0x008
#define LX_POLLHUP  0x010
#define LX_POLLRDHUP 0x2000

enum {
    NR_socket = 41, NR_connect = 42, NR_accept = 43, NR_sendto = 44, NR_recvfrom = 45,
    NR_sendmsg = 46, NR_recvmsg = 47, NR_shutdown = 48, NR_bind = 49, NR_listen = 50,
    NR_getsockname = 51, NR_getpeername = 52, NR_socketpair = 53, NR_setsockopt = 54,
    NR_getsockopt = 55, NR_epoll_create = 213, NR_epoll_wait = 232, NR_epoll_ctl = 233,
    NR_epoll_pwait = 281, NR_timerfd_create = 283, NR_timerfd_settime = 286,
    NR_timerfd_gettime = 287, NR_accept4 = 288, NR_epoll_create1 = 291,
    NR_recvmmsg = 299, NR_sendmmsg = 307, NR_epoll_pwait2 = 441,
};

/* Ошибка сети MyOS -> номер Linux */
static INT64 net_err(INTN e)
{
    switch (e) {
    case MYOS_ETIMEDOUT:     return -LX_ETIMEDOUT;
    case MYOS_ECONNREFUSED:  return -LX_ECONNREFUSED;
    case MYOS_ECONNRESET:    return -LX_ECONNRESET;
    case MYOS_ENETUNREACH:   return -LX_ENETUNREACH;
    case MYOS_EADDRINUSE:    return -LX_EADDRINUSE;
    case MYOS_ENOTCONN:      return -LX_ENOTCONN;
    case MYOS_EAGAIN:        return -LX_EAGAIN;
    case MYOS_EINTR:         return -LX_EINTR;
    case MYOS_EINPROGRESS:   return -LX_EINPROGRESS;
    case MYOS_EMFILE:        return -LX_EMFILE;
    case MYOS_EBADF:         return -LX_EBADF;
    case MYOS_EINVAL:        return -LX_EINVAL;
    default:                 return linux_errno(e);
    }
}

/* Скопировать из/в память программы (с проверкой адреса) */
static BOOLEAN from_user(KPROC *p, void *dst, UINT64 u, UINTN n)
{
    if (n == 0)
        return TRUE;

    if (!uptr_ok(p, u, n, FALSE))
        return FALSE;

    memcpy(dst, (const void *)(UINTN)u, n);
    return TRUE;
}

static BOOLEAN to_user(KPROC *p, UINT64 u, const void *src, UINTN n)
{
    if (n == 0)
        return TRUE;

    if (!uptr_ok(p, u, n, TRUE))
        return FALSE;

    memcpy((void *)(UINTN)u, src, n);
    return TRUE;
}

/* Срок ожидания из SO_RCVTIMEO (0 - без срока) */
static UINT64 rcv_deadline(LFILE *f)
{
    return f->rcvtimeo_ms ? g_kticks + f->rcvtimeo_ms : 0;
}

static BOOLEAN deadline_passed(UINT64 dl)
{
    return dl != 0 && g_kticks >= dl;
}


/* ================================================================
 * AF_INET
 * ================================================================ */

/* sockaddr_in программы -> адрес и порт MyOS */
static INT64 inet_addr_in(KPROC *p, UINT64 uaddr, UINT64 len, UINT32 *ip, UINT16 *port)
{
    UINT8 sa[16];

    if (len < 8 || !from_user(p, sa, uaddr, 8))
        return -LX_EFAULT;

    UINT16 fam = (UINT16)(sa[0] | (sa[1] << 8));

    if (fam != LX_AF_INET)
        return (fam == 0) ? -LX_EAFNOSUPPORT : -LX_EAFNOSUPPORT;

    *port = (UINT16)((sa[2] << 8) | sa[3]);
    *ip = ((UINT32)sa[4] << 24) | ((UINT32)sa[5] << 16) | ((UINT32)sa[6] << 8) | sa[7];
    return 0;
}

/* Адрес MyOS -> sockaddr_in программы (len - указатель на длину) */
static INT64 inet_addr_out(KPROC *p, UINT64 uaddr, UINT64 ulen, UINT32 ip, UINT16 port)
{
    if (uaddr == 0)
        return 0;

    UINT32 cap = 0;

    if (ulen == 0 || !from_user(p, &cap, ulen, 4))
        return -LX_EFAULT;

    UINT8 sa[16];

    memset(sa, 0, sizeof(sa));
    sa[0] = LX_AF_INET;
    sa[2] = (UINT8)(port >> 8);
    sa[3] = (UINT8)port;
    sa[4] = (UINT8)(ip >> 24);
    sa[5] = (UINT8)(ip >> 16);
    sa[6] = (UINT8)(ip >> 8);
    sa[7] = (UINT8)ip;

    UINT32 n = (cap < 16) ? cap : 16;
    UINT32 full = 16;

    if (!to_user(p, uaddr, sa, n) || !to_user(p, ulen, &full, 4))
        return -LX_EFAULT;

    return 0;
}

/* Подождать события сокета MyOS (wanted - MYOS_POLL*) */
typedef struct {
    INTN   s;
    UINT32 want;
} INET_WAIT;

static BOOLEAN inet_ready(void *ctx)
{
    INET_WAIT *w = (INET_WAIT *)ctx;

    return (sock_poll(w->s) & (w->want | MYOS_POLLERR | MYOS_POLLHUP | MYOS_POLLNVAL)) != 0;
}

static INT64 inet_wait(KPROC *p, INTN s, UINT32 want, UINT64 deadline)
{
    INET_WAIT w = { s, want };

    if (p->killed || lx_signal_pending())
        return -LX_ERESTARTSYS;

    if (deadline_passed(deadline))
        return -LX_EAGAIN;

    sock_poll_wait(inet_ready, &w, 50);
    return 0;
}

static INT64 inet_recv(KPROC *p, LFILE *f, UINT8 *dst, UINTN n, UINT32 flags, UINT32 *ip,
                       UINT16 *port)
{
    UINT64 dl = rcv_deadline(f);
    BOOLEAN nb = (f->flags & LX_O_NONBLOCK) || (flags & LX_MSG_DONTWAIT);
    UINTN got = 0;

    for (;;) {

        INTN r = sock_recvfrom_ex(f->sock, dst + got, n - got, ip, port, NULL,
                                  (flags & LX_MSG_PEEK) != 0);

        if (r > 0) {
            got += (UINTN)r;
            /* MSG_WAITALL у потока - пока не наберётся всё */
            if (!(flags & LX_MSG_WAITALL) || f->stype != LX_SOCK_STREAM || got >= n)
                return (INT64)got;
            continue;
        }

        if (r == 0)
            return (INT64)got;                  /* конец потока */

        if (r != MYOS_EAGAIN)
            return got ? (INT64)got : net_err(r);

        if (nb)
            return got ? (INT64)got : -LX_EAGAIN;

        INT64 w = inet_wait(p, f->sock, MYOS_POLLIN, dl);

        if (w < 0)
            return got ? (INT64)got : w;
    }
}

static INT64 inet_send(KPROC *p, LFILE *f, const UINT8 *src, UINTN n, UINT32 flags, UINT32 ip,
                       UINT16 port)
{
    BOOLEAN nb = (f->flags & LX_O_NONBLOCK) || (flags & LX_MSG_DONTWAIT);
    UINTN done = 0;

    for (;;) {

        INTN r = sock_sendto(f->sock, src + done, n - done, ip, port);

        if (r > 0) {
            done += (UINTN)r;
            if (done >= n || f->stype != LX_SOCK_STREAM)
                return (INT64)done;
            continue;
        }

        if (r != MYOS_EAGAIN) {
            if (done)
                return (INT64)done;
            if ((r == MYOS_ENOTCONN || r == MYOS_ECONNRESET) && f->stype == LX_SOCK_STREAM) {
                if (!(flags & LX_MSG_NOSIGNAL))
                    lx_signal_send(p, LX_SIGPIPE);
                return -LX_EPIPE;
            }
            return net_err(r);
        }

        if (nb)
            return done ? (INT64)done : -LX_EAGAIN;

        INT64 w = inet_wait(p, f->sock, MYOS_POLLOUT, 0);

        if (w < 0)
            return done ? (INT64)done : w;
    }
}

static INT64 inet_connect(KPROC *p, LFILE *f, UINT64 uaddr, UINT64 len)
{
    UINT32 ip;
    UINT16 port;
    INT64 r = inet_addr_in(p, uaddr, len, &ip, &port);

    if (r < 0)
        return r;

    INTN c = sock_connect(f->sock, ip, port);

    if (c != MYOS_EINPROGRESS)
        return (c < 0) ? net_err(c) : 0;

    if (f->flags & LX_O_NONBLOCK)
        return -LX_EINPROGRESS;

    /* блокирующий connect: ждём, чем кончится */
    for (;;) {
        INTN e = sock_setopt(f->sock, MYOS_SO_ERROR, 0);
        if (e != MYOS_EINPROGRESS)
            return (e < 0) ? net_err(e) : 0;
        INT64 w = inet_wait(p, f->sock, MYOS_POLLOUT, 0);
        if (w < 0)
            return (w == -LX_ERESTARTSYS) ? -LX_EINTR : w;   /* соединение идёт дальше */
    }
}


/* ================================================================
 * AF_UNIX
 * ================================================================ */

#define LU_MAXFDS   32               /* файлов в одном сообщении */
#define LU_BUF      (256u * 1024u)   /* очередь: больше - отправитель ждёт */

typedef struct LUMSG {
    struct LUMSG *next;
    UINT32        len, off;          /* данные и сколько уже прочитано */
    UINT32        nfds;
    LFILE        *fds[LU_MAXFDS];    /* приложенные открытые файлы */
    UINT8         data[];
} LUMSG;

enum { LU_NEW = 0, LU_LISTEN, LU_CONN };

typedef struct LUSOCK {
    UINT32         stype;
    UINT32         state;
    struct LUSOCK *peer;
    BOOLEAN        peer_gone;        /* был соединён - собеседник закрыл */
    BOOLEAN        shut_rd, shut_wr;
    LUMSG         *rq, *rq_tail;     /* пришедшее */
    UINT32         rq_bytes;
    /* слушающий: ждущие accept соединения (серверные концы) */
    struct LUSOCK *bl_head, *bl_tail, *bl_next;
    UINT32         bl_n, bl_max;
    /* имя (bind) */
    BOOLEAN        bound;
    char           name[VFS_PATH_MAX];
    struct LUSOCK *reg_next;
    UINT32         pid, peer_pid;    /* SO_PEERCRED */
} LUSOCK;

static LUSOCK *g_lu_reg;             /* сокеты с именем */

static LUSOCK *lu_new(UINT32 stype, UINT32 pid)
{
    LUSOCK *u = (LUSOCK *)kzalloc(sizeof(LUSOCK));

    if (u != NULL) {
        u->stype = stype;
        u->pid = pid;
    }

    return u;
}

static void lu_wake(LUSOCK *u)
{
    if (u != NULL)
        sched_wake_all(u);

    lx_poll_wake();
}

static void lu_msg_free(LUMSG *m)
{
    for (UINT32 i = 0; i < m->nfds; i++)
        lfile_unref(m->fds[i]);

    kfree(m);
}

static void lu_free(LUSOCK *u);

/* Конец сокета закрыт: собеседнику - "конец", очередь - прочь */
static void lu_free(LUSOCK *u)
{
    if (u->bound) {
        LUSOCK **pp = &g_lu_reg;
        while (*pp != NULL && *pp != u)
            pp = &(*pp)->reg_next;
        if (*pp == u)
            *pp = u->reg_next;
    }

    /* непринятые соединения слушающего - закрыть */
    while (u->bl_head != NULL) {
        LUSOCK *c = u->bl_head;
        u->bl_head = c->bl_next;
        lu_free(c);
    }

    if (u->peer != NULL) {
        u->peer->peer = NULL;
        u->peer->peer_gone = TRUE;
        lu_wake(u->peer);
    }

    while (u->rq != NULL) {
        LUMSG *m = u->rq;
        u->rq = m->next;
        lu_msg_free(m);
    }

    kfree(u);
}

/* Имя из sockaddr_un -> ключ таблицы ("@абстрактное" или путь MyOS) */
static INT64 lu_name(KPROC *p, UINT64 uaddr, UINT64 len, char *key, UINTN cap)
{
    UINT8 sa[112];

    if (len < 3 || len > sizeof(sa))
        return -LX_EINVAL;

    memset(sa, 0, sizeof(sa));

    if (!from_user(p, sa, uaddr, (UINTN)len))
        return -LX_EFAULT;

    if (sa[0] != LX_AF_UNIX || sa[1] != 0)
        return -LX_EAFNOSUPPORT;

    if (sa[2] == '\0') {
        /* абстрактное имя: байты после нуля (могут быть любыми) */
        UINTN k = 0;
        key[k++] = '@';
        for (UINTN i = 3; i < (UINTN)len && k + 3 < cap; i++) {
            UINT8 c = sa[i];
            if (c >= 32 && c < 127) {
                key[k++] = (char)c;
            } else {
                static const char hx[] = "0123456789abcdef";
                key[k++] = '%';
                key[k++] = hx[c >> 4];
                key[k++] = hx[c & 15];
            }
        }
        key[k] = '\0';
        return 0;
    }

    char lpath[110];
    UINTN n = 0;

    for (; n + 2 < (UINTN)len && n < sizeof(lpath) - 1 && sa[2 + n]; n++)
        lpath[n] = (char)sa[2 + n];

    lpath[n] = '\0';

    return lx_resolve_kpath(p, LX_AT_FDCWD, lpath, TRUE, key, cap);
}

static LUSOCK *lu_lookup(const char *key)
{
    for (LUSOCK *u = g_lu_reg; u != NULL; u = u->reg_next)
        if (u->bound) {
            const char *a = u->name, *b = key;
            while (*a && *a == *b) {
                a++;
                b++;
            }
            if (*a == *b)
                return u;
        }

    return NULL;
}

/* Есть ли сокет с таким путём MyOS (stat видит S_IFSOCK) */
BOOLEAN lx_unix_bound(const char *mypath)
{
    return lu_lookup(mypath) != NULL;
}

static INT64 lu_bind(KPROC *p, LUSOCK *u, UINT64 uaddr, UINT64 len)
{
    char key[VFS_PATH_MAX];
    INT64 r = lu_name(p, uaddr, len, key, sizeof(key));

    if (r < 0)
        return r;

    if (u->bound)
        return -LX_EINVAL;

    if (lu_lookup(key) != NULL)
        return -LX_EADDRINUSE;

    if (key[0] != '@') {
        /* имя в файловой системе: пустой файл (как "файл-сокет" Linux);
           уже есть - адрес занят (так ведёт себя Linux) */
        VFS_DIRENT *e = (VFS_DIRENT *)kmalloc(sizeof(VFS_DIRENT));
        BOOLEAN exists = (e != NULL) && vfs_lstat(key, e) == VFS_OK;
        kfree(e);
        if (exists)
            return -LX_EADDRINUSE;
        INTN kfd = vfs_open(key, VFS_O_WRITE | VFS_O_CREATE);
        if (kfd < 0)
            return linux_errno(kfd);
        vfs_close(kfd);
    }

    ksnprintf(u->name, sizeof(u->name), "%s", key);
    u->bound = TRUE;
    u->reg_next = g_lu_reg;
    g_lu_reg = u;
    return 0;
}

static INT64 lu_connect(KPROC *p, LFILE *f, UINT64 uaddr, UINT64 len)
{
    LUSOCK *u = f->us;
    char key[VFS_PATH_MAX];
    INT64 r = lu_name(p, uaddr, len, key, sizeof(key));

    if (r < 0)
        return r;

    if (u->state == LU_CONN)
        return -LX_EISCONN;

    LUSOCK *l = lu_lookup(key);

    if (l == NULL) {
        /* файла нет - ENOENT, есть, но никто не слушает - отказ */
        VFS_DIRENT *e = (VFS_DIRENT *)kmalloc(sizeof(VFS_DIRENT));
        BOOLEAN exists = (key[0] != '@') && e != NULL && vfs_lstat(key, e) == VFS_OK;
        kfree(e);
        return exists ? -LX_ECONNREFUSED : -LX_ENOENT;
    }

    if (u->stype == LX_SOCK_DGRAM && l->stype == LX_SOCK_DGRAM) {
        /* датаграммы: просто запомнить, куда слать */
        u->peer = NULL;
        return -LX_EOPNOTSUPP;
    }

    if (l->state != LU_LISTEN || l->stype != u->stype)
        return -LX_ECONNREFUSED;

    while (l->bl_n >= l->bl_max) {
        if (f->flags & LX_O_NONBLOCK)
            return -LX_EAGAIN;
        if (p->killed || lx_signal_pending())
            return -LX_ERESTARTSYS;
        UINT64 fl = kx_irq_save();
        sched_block(l, "unix connect", 100);
        kx_irq_restore(fl);
        if (lu_lookup(key) != l)
            return -LX_ECONNREFUSED;    /* слушающий ушёл, пока ждали */
    }

    /* серверный конец - в очередь слушающего */
    LUSOCK *s = lu_new(u->stype, l->pid);

    if (s == NULL)
        return -LX_ENOMEM;

    s->state = LU_CONN;
    s->peer = u;
    s->peer_pid = p->pid;
    u->state = LU_CONN;
    u->peer = s;
    u->peer_pid = l->pid;

    if (l->bl_tail != NULL)
        l->bl_tail->bl_next = s;
    else
        l->bl_head = s;
    l->bl_tail = s;
    l->bl_n++;

    lu_wake(l);
    return 0;
}

static INT64 lu_send(KPROC *p, LFILE *f, const UINT8 *src, UINTN n, LFILE **fds, UINT32 nfds,
                     UINT32 flags)
{
    LUSOCK *u = f->us;
    BOOLEAN nb = (f->flags & LX_O_NONBLOCK) || (flags & LX_MSG_DONTWAIT);
    BOOLEAN stream = (u->stype == LX_SOCK_STREAM);
    UINTN done = 0;

    if (!stream && n > LU_BUF)
        return -LX_EMSGSIZE;

    for (;;) {

        LUSOCK *t = u->peer;

        if (t == NULL || u->shut_wr) {
            if (done)
                return (INT64)done;
            if (u->peer_gone || u->shut_wr) {
                if (!(flags & LX_MSG_NOSIGNAL))
                    lx_signal_send(p, LX_SIGPIPE);
                return -LX_EPIPE;
            }
            return -LX_ENOTCONN;
        }

        UINT32 room = (t->rq_bytes < LU_BUF) ? LU_BUF - t->rq_bytes : 0;

        /* место есть (поток - хоть сколько-то; сообщение - целиком) */
        if ((stream && room > 0) || (!stream && room >= n) || (n == 0 && room > 0)) {

            UINTN k = n - done;

            if (stream && k > room)
                k = room;

            LUMSG *m = (LUMSG *)kmalloc(sizeof(LUMSG) + (k ? k : 1));

            if (m == NULL)
                return done ? (INT64)done : -LX_ENOMEM;

            m->next = NULL;
            m->len = (UINT32)k;
            m->off = 0;
            m->nfds = 0;
            memcpy(m->data, src + done, k);

            /* файлы - с первым куском */
            if (done == 0) {
                for (UINT32 i = 0; i < nfds; i++) {
                    fds[i]->refs++;
                    m->fds[m->nfds++] = fds[i];
                }
            }

            if (t->rq_tail != NULL)
                t->rq_tail->next = m;
            else
                t->rq = m;
            t->rq_tail = m;
            t->rq_bytes += (UINT32)k;
            done += k;

            lu_wake(t);

            if (done >= n)
                return (INT64)done;
            continue;
        }

        if (nb)
            return done ? (INT64)done : -LX_EAGAIN;

        if (p->killed || lx_signal_pending())
            return done ? (INT64)done : -LX_ERESTARTSYS;

        UINT64 fl = kx_irq_save();
        sched_block(t, "unix send", 100);
        kx_irq_restore(fl);
    }
}

/*
 * Принять. Файлы из сообщения (SCM_RIGHTS) - в rfds (до *nfds, лишние
 * закрываются; rfds == NULL - все закрываются, как при обычном read).
 * *mflags - MSG_TRUNC / MSG_CTRUNC.
 */
static INT64 lu_recv(KPROC *p, LFILE *f, UINT8 *dst, UINTN n, UINT32 flags, LFILE **rfds,
                     UINT32 *nfds, UINT32 *mflags)
{
    LUSOCK *u = f->us;
    BOOLEAN nb = (f->flags & LX_O_NONBLOCK) || (flags & LX_MSG_DONTWAIT);
    BOOLEAN peek = (flags & LX_MSG_PEEK) != 0;
    UINT64 dl = rcv_deadline(f);
    UINT32 cap = (nfds != NULL) ? *nfds : 0;

    if (nfds != NULL)
        *nfds = 0;

    for (;;) {

        if (u->rq != NULL)
            break;

        if (u->peer_gone || u->shut_rd || (u->state == LU_CONN && u->peer == NULL))
            return 0;                           /* конец */

        if (u->state != LU_CONN)
            return -LX_ENOTCONN;

        if (nb || deadline_passed(dl))
            return -LX_EAGAIN;

        if (p->killed || lx_signal_pending())
            return -LX_ERESTARTSYS;

        UINT64 fl = kx_irq_save();
        if (u->rq == NULL)
            sched_block(u, "unix recv", 100);
        kx_irq_restore(fl);
    }

    UINTN got = 0;
    LUMSG *m = u->rq;
    BOOLEAN first = TRUE;

    while (m != NULL && got < n) {

        /* сообщение с файлами - только первым: файлы не должны
           "приклеиться" к чужим байтам */
        if (!first && m->nfds > 0)
            break;

        if (m->nfds > 0 && !peek) {
            for (UINT32 i = 0; i < m->nfds; i++) {
                if (rfds != NULL && *nfds < cap) {
                    rfds[(*nfds)++] = m->fds[i];        /* ссылка переходит */
                } else {
                    lfile_unref(m->fds[i]);
                    if (rfds != NULL)
                        *mflags |= LX_MSG_CTRUNC;
                }
            }
            m->nfds = 0;
        }

        UINTN k = m->len - m->off;

        if (k > n - got)
            k = n - got;

        memcpy(dst + got, m->data + m->off, k);
        got += k;
        first = FALSE;

        if (u->stype != LX_SOCK_STREAM) {
            /* одно сообщение за раз; не влезло - хвост пропадает */
            if (k < m->len - m->off)
                *mflags |= LX_MSG_TRUNC;
            if (!peek) {
                u->rq = m->next;
                if (u->rq == NULL)
                    u->rq_tail = NULL;
                u->rq_bytes -= m->len;
                lu_msg_free(m);
            }
            break;
        }

        if (peek) {
            m = m->next;
            continue;
        }

        m->off += (UINT32)k;
        u->rq_bytes -= (UINT32)k;

        if (m->off >= m->len) {
            u->rq = m->next;
            if (u->rq == NULL)
                u->rq_tail = NULL;
            lu_msg_free(m);
            m = u->rq;
        }
    }

    if (!peek && u->peer != NULL)
        lu_wake(u->peer);                       /* отправителю - место */
    lx_poll_wake();

    return (INT64)got;
}

static UINT32 lu_poll(LUSOCK *u)
{
    UINT32 ev = 0;

    if (u->state == LU_LISTEN)
        return (u->bl_n > 0) ? LX_POLLIN : 0;

    if (u->rq != NULL)
        ev |= LX_POLLIN;

    if (u->state == LU_CONN && u->peer == NULL)
        ev |= LX_POLLIN | LX_POLLHUP | LX_POLLRDHUP;

    if (u->peer != NULL && u->peer->rq_bytes < LU_BUF && !u->shut_wr)
        ev |= LX_POLLOUT;

    if (u->state == LU_NEW && u->stype == LX_SOCK_DGRAM)
        ev |= LX_POLLOUT;

    return ev;
}


/* ================================================================
 * epoll
 * ================================================================ */

#define LX_EPOLLIN      0x001
#define LX_EPOLLOUT     0x004
#define LX_EPOLLERR     0x008
#define LX_EPOLLHUP     0x010
#define LX_EPOLLONESHOT (1u << 30)
#define LX_EPOLLET      (1u << 31)

typedef struct {
    INT32   fd;
    LFILE  *f;                  /* какой файл был под этим номером */
    UINT32  events;
    UINT64  data;
    BOOLEAN disabled;           /* EPOLLONESHOT уже сработал */
    BOOLEAN out_was;            /* EPOLLET: запись была можна в прошлый раз */
} LEPITEM;

typedef struct LEPOLL {
    LEPITEM *it;
    UINTN    n, cap;
    UINTN    depth;             /* от зацикливания epoll в epoll */
} LEPOLL;

typedef struct __attribute__((packed)) {
    UINT32 events;
    UINT64 data;
} LX_EPEV;

static LEPITEM *ep_find(LEPOLL *e, INT32 fd)
{
    for (UINTN i = 0; i < e->n; i++)
        if (e->it[i].fd == fd)
            return &e->it[i];

    return NULL;
}

static INT64 ep_ctl(KPROC *p, LFILE *ef, INT64 op, INT64 fd, UINT64 uev)
{
    LEPOLL *e = ef->ep;
    LFILE *t = lx_fd_get(p, fd);
    LX_EPEV ev = { 0, 0 };

    if (t == NULL)
        return -LX_EBADF;

    if (t == ef)
        return -LX_EINVAL;

    if (op != 2 && !from_user(p, &ev, uev, sizeof(ev)))   /* 2 - EPOLL_CTL_DEL */
        return -LX_EFAULT;

    LEPITEM *it = ep_find(e, (INT32)fd);

    /* номер уже указывает на другой файл - старая запись мёртвая */
    if (it != NULL && it->f != t) {
        *it = e->it[--e->n];
        it = NULL;
    }

    switch (op) {

    case 1:                                     /* EPOLL_CTL_ADD */
        if (it != NULL)
            return -LX_EEXIST;
        if (e->n == e->cap) {
            UINTN nc = e->cap ? e->cap * 2u : 16u;
            LEPITEM *ni = (LEPITEM *)kzalloc(nc * sizeof(LEPITEM));
            if (ni == NULL)
                return -LX_ENOMEM;
            if (e->it != NULL) {
                memcpy(ni, e->it, e->n * sizeof(LEPITEM));
                kfree(e->it);
            }
            e->it = ni;
            e->cap = nc;
        }
        it = &e->it[e->n++];
        memset(it, 0, sizeof(*it));
        it->fd = (INT32)fd;
        it->f = t;
        it->events = ev.events;
        it->data = ev.data;
        lx_poll_wake();
        return 0;

    case 2:                                     /* EPOLL_CTL_DEL */
        if (it == NULL)
            return -LX_ENOENT;
        *it = e->it[--e->n];
        return 0;

    case 3:                                     /* EPOLL_CTL_MOD */
        if (it == NULL)
            return -LX_ENOENT;
        it->events = ev.events;
        it->data = ev.data;
        it->disabled = FALSE;
        it->out_was = FALSE;
        lx_poll_wake();
        return 0;
    }

    return -LX_EINVAL;
}

/* Собрать готовые (до max); out == NULL - только посчитать (для poll
   самого epoll) */
static UINTN ep_collect(KPROC *p, LEPOLL *e, LX_EPEV *out, UINTN max)
{
    UINTN got = 0;

    if (e->depth > 4)
        return 0;

    e->depth++;

    for (UINTN i = 0; i < e->n && got < max; i++) {

        LEPITEM *it = &e->it[i];

        /* файл закрыт (номер пуст или уже другой) - запись убрать */
        if (lx_fd_get(p, it->fd) != it->f) {
            *it = e->it[--e->n];
            i--;
            continue;
        }

        if (it->disabled)
            continue;

        UINT32 ready = lx_file_poll(p, it->f);
        UINT32 ev = ready & (it->events | LX_EPOLLERR | LX_EPOLLHUP);

        if ((it->events & LX_EPOLLET) && (ev & LX_EPOLLOUT)) {
            /* "по фронту": запись - только если раньше было нельзя */
            if (it->out_was)
                ev &= ~(UINT32)LX_EPOLLOUT;
        }

        if (out != NULL)
            it->out_was = (ready & LX_EPOLLOUT) != 0;

        if (ev == 0)
            continue;

        if (out != NULL) {
            out[got].events = ev;
            out[got].data = it->data;
            if (it->events & LX_EPOLLONESHOT)
                it->disabled = TRUE;
        }

        got++;
    }

    e->depth--;
    return got;
}

static INT64 ep_wait(KPROC *p, LFILE *ef, UINT64 uev, INT64 max, INT64 timeout_ms)
{
    if (max <= 0 || max > 4096)
        return -LX_EINVAL;

    if (!uptr_ok(p, uev, (UINT64)max * sizeof(LX_EPEV), TRUE))
        return -LX_EFAULT;

    LX_EPEV *buf = (LX_EPEV *)kmalloc((UINTN)max * sizeof(LX_EPEV));

    if (buf == NULL)
        return -LX_ENOMEM;

    UINT64 dl = (timeout_ms > 0) ? g_kticks + (UINT64)timeout_ms : 0;
    INT64 r;

    for (;;) {

        UINTN n = ep_collect(p, ef->ep, buf, (UINTN)max);

        if (n > 0) {
            memcpy((void *)(UINTN)uev, buf, n * sizeof(LX_EPEV));
            r = (INT64)n;
            break;
        }

        if (timeout_ms == 0 || (timeout_ms > 0 && g_kticks >= dl)) {
            r = 0;
            break;
        }

        r = lx_wait_poll(p, dl);

        if (r < 0)
            break;
    }

    kfree(buf);
    return r;
}


/* ================================================================
 * timerfd
 * ================================================================ */

/* Сработал ли таймер (пересчитать count) */
static void tfd_update(LFILE *f)
{
    if (f->t_next_ns == 0)
        return;

    UINT64 now = lx_now_ns(f->t_clock == 0);

    if (now < f->t_next_ns)
        return;

    if (f->t_iv_ns == 0) {
        f->count++;
        f->t_next_ns = 0;
    } else {
        UINT64 k = (now - f->t_next_ns) / f->t_iv_ns + 1u;
        f->count += k;
        f->t_next_ns += k * f->t_iv_ns;
    }
}

typedef struct {
    INT64 iv_s, iv_ns, val_s, val_ns;
} LX_ITIMERSPEC;

static INT64 tfd_settime(KPROC *p, LFILE *f, UINT64 flags, UINT64 unew, UINT64 uold)
{
    LX_ITIMERSPEC ns, os;

    if (!from_user(p, &ns, unew, sizeof(ns)))
        return -LX_EFAULT;

    tfd_update(f);

    UINT64 now = lx_now_ns(f->t_clock == 0);
    UINT64 left = (f->t_next_ns > now) ? f->t_next_ns - now : 0;

    os.iv_s = (INT64)(f->t_iv_ns / 1000000000u);
    os.iv_ns = (INT64)(f->t_iv_ns % 1000000000u);
    os.val_s = (INT64)(left / 1000000000u);
    os.val_ns = (INT64)(left % 1000000000u);

    UINT64 val = (UINT64)ns.val_s * 1000000000u + (UINT64)ns.val_ns;

    f->t_iv_ns = (UINT64)ns.iv_s * 1000000000u + (UINT64)ns.iv_ns;
    f->count = 0;

    if (val == 0)
        f->t_next_ns = 0;
    else if (flags & 1u)                        /* TFD_TIMER_ABSTIME */
        f->t_next_ns = val;
    else
        f->t_next_ns = now + val;

    lx_poll_wake();

    if (uold != 0 && !to_user(p, uold, &os, sizeof(os)))
        return -LX_EFAULT;

    return 0;
}

static INT64 tfd_read(KPROC *p, LFILE *f, UINT8 *dst, UINTN n)
{
    if (n < 8)
        return -LX_EINVAL;

    for (;;) {

        tfd_update(f);

        if (f->count > 0) {
            UINT64 v = f->count;
            f->count = 0;
            memcpy(dst, &v, 8);
            return 8;
        }

        if (f->flags & LX_O_NONBLOCK)
            return -LX_EAGAIN;

        UINT64 dl = 0;

        if (f->t_next_ns != 0) {
            UINT64 now = lx_now_ns(f->t_clock == 0);
            UINT64 ms = (f->t_next_ns > now) ? (f->t_next_ns - now + 999999u) / 1000000u : 0;
            dl = g_kticks + ms;
        }

        INT64 w = lx_wait_poll(p, dl);

        if (w < 0)
            return w;
    }
}


/* ================================================================
 * Общее: чтение, запись, poll, закрытие (из kernel/lxfile.c)
 * ================================================================ */

INT64 lxs_read(KPROC *p, LFILE *f, UINT8 *dst, UINTN n)
{
    UINT32 mf = 0;

    switch (f->type) {
    case LF_INET:
        return inet_recv(p, f, dst, n, 0, NULL, NULL);
    case LF_UNIX:
        return lu_recv(p, f, dst, n, 0, NULL, NULL, &mf);
    case LF_TIMERFD:
        return tfd_read(p, f, dst, n);
    default:
        return -LX_EINVAL;
    }
}

INT64 lxs_write(KPROC *p, LFILE *f, const UINT8 *src, UINTN n)
{
    switch (f->type) {
    case LF_INET:
        return inet_send(p, f, src, n, 0, 0, 0);
    case LF_UNIX:
        return lu_send(p, f, src, n, NULL, 0, 0);
    default:
        return -LX_EINVAL;
    }
}

UINT32 lxs_poll(KPROC *p, LFILE *f)
{
    switch (f->type) {
    case LF_INET:
        return sock_poll(f->sock) & (LX_POLLIN | LX_POLLOUT | LX_POLLERR | LX_POLLHUP);
    case LF_UNIX:
        return lu_poll(f->us);
    case LF_EPOLL:
        return ep_collect(p, f->ep, NULL, 1) ? LX_POLLIN : 0;
    case LF_TIMERFD:
        tfd_update(f);
        return f->count ? LX_POLLIN : 0;
    default:
        return 0;
    }
}

void lxs_release(LFILE *f)
{
    switch (f->type) {
    case LF_INET:
        if (f->sock >= 0)
            sock_close(f->sock);
        break;
    case LF_UNIX:
        if (f->us != NULL)
            lu_free(f->us);
        break;
    case LF_EPOLL:
        if (f->ep != NULL) {
            kfree(f->ep->it);
            kfree(f->ep);
        }
        break;
    }
}


/* ================================================================
 * Системные вызовы
 * ================================================================ */

static INT64 install(KPROC *p, LFILE *f, BOOLEAN cloexec)
{
    INT64 fd = lx_fd_install(p, f, cloexec);

    if (fd < 0)
        lfile_unref(f);

    return fd;
}

static LFILE *new_unix(KPROC *p, UINT32 stype, UINT32 nonblock)
{
    LFILE *f = lx_lfile_new(LF_UNIX, LX_O_RDWR | nonblock);

    if (f == NULL)
        return NULL;

    f->stype = stype;
    f->us = lu_new(stype, p->pid);

    if (f->us == NULL) {
        kfree(f);
        return NULL;
    }

    ksnprintf(f->path, sizeof(f->path), "socket:[unix]");
    return f;
}

static INT64 sys_socket(KPROC *p, UINT64 dom, UINT64 type, UINT64 proto)
{
    UINT32 st = (UINT32)type & LX_SOCK_TYPEMASK;
    UINT32 nb = (type & LX_SOCK_NONBLOCK) ? LX_O_NONBLOCK : 0;
    BOOLEAN ce = (type & LX_SOCK_CLOEXEC) != 0;

    (void)proto;

    if (dom == LX_AF_UNIX) {
        if (st != LX_SOCK_STREAM && st != LX_SOCK_DGRAM && st != LX_SOCK_SEQPACKET)
            return -LX_EINVAL;
        LFILE *f = new_unix(p, st, nb);
        return (f == NULL) ? -LX_ENOMEM : install(p, f, ce);
    }

    if (dom != LX_AF_INET)
        return -LX_EAFNOSUPPORT;        /* IPv6, netlink: программы обходятся IPv4 */

    if (st != LX_SOCK_STREAM && st != LX_SOCK_DGRAM)
        return -LX_EINVAL;

    /* сокет MyOS "ничей" (pid 0): закроется вместе с последней ссылкой
       на LFILE (после fork - у потомка тоже) */
    INTN s = sock_create(st == LX_SOCK_STREAM ? MYOS_SOCK_STREAM : MYOS_SOCK_DGRAM, 0);

    if (s < 0)
        return (s == MYOS_EMFILE) ? -LX_EMFILE : net_err(s);

    sock_setopt(s, MYOS_SO_NONBLOCK, 1);

    LFILE *f = lx_lfile_new(LF_INET, LX_O_RDWR | nb);

    if (f == NULL) {
        sock_close(s);
        return -LX_ENOMEM;
    }

    f->sock = s;
    f->stype = st;
    ksnprintf(f->path, sizeof(f->path), "socket:[inet]");
    return install(p, f, ce);
}

static INT64 sys_socketpair(KPROC *p, UINT64 dom, UINT64 type, UINT64 usv)
{
    UINT32 st = (UINT32)type & LX_SOCK_TYPEMASK;
    UINT32 nb = (type & LX_SOCK_NONBLOCK) ? LX_O_NONBLOCK : 0;
    BOOLEAN ce = (type & LX_SOCK_CLOEXEC) != 0;

    if (dom != LX_AF_UNIX)
        return -LX_EOPNOTSUPP;

    if (st != LX_SOCK_STREAM && st != LX_SOCK_DGRAM && st != LX_SOCK_SEQPACKET)
        return -LX_EINVAL;

    if (!uptr_ok(p, usv, 8, TRUE))
        return -LX_EFAULT;

    LFILE *a = new_unix(p, st, nb);
    LFILE *b = new_unix(p, st, nb);

    if (a == NULL || b == NULL) {
        lfile_unref(a);
        lfile_unref(b);
        return -LX_ENOMEM;
    }

    a->us->state = b->us->state = LU_CONN;
    a->us->peer = b->us;
    b->us->peer = a->us;
    a->us->peer_pid = b->us->peer_pid = p->pid;

    INT64 fa = install(p, a, ce);

    if (fa < 0) {
        lfile_unref(b);
        return fa;
    }

    INT64 fb = install(p, b, ce);

    if (fb < 0) {
        /* закрыть первый номер (он уже наш) */
        LFILE *x = p->lx->fd[fa];
        p->lx->fd[fa] = NULL;
        lfile_unref(x);
        return fb;
    }

    INT32 sv[2] = { (INT32)fa, (INT32)fb };
    memcpy((void *)(UINTN)usv, sv, 8);
    return 0;
}

static INT64 sys_accept(KPROC *p, LFILE *f, UINT64 uaddr, UINT64 ulen, UINT64 flags)
{
    BOOLEAN ce = (flags & LX_SOCK_CLOEXEC) != 0;
    UINT32 nb = (flags & LX_SOCK_NONBLOCK) ? LX_O_NONBLOCK : 0;

    if (f->type == LF_UNIX) {

        LUSOCK *l = f->us;

        if (l->state != LU_LISTEN)
            return -LX_EINVAL;

        while (l->bl_head == NULL) {
            if (f->flags & LX_O_NONBLOCK)
                return -LX_EAGAIN;
            if (p->killed || lx_signal_pending())
                return -LX_ERESTARTSYS;
            UINT64 fl = kx_irq_save();
            sched_block(l, "unix accept", 100);
            kx_irq_restore(fl);
        }

        LUSOCK *s = l->bl_head;
        l->bl_head = s->bl_next;
        if (l->bl_head == NULL)
            l->bl_tail = NULL;
        l->bl_n--;
        s->bl_next = NULL;
        lu_wake(l);

        LFILE *nf = lx_lfile_new(LF_UNIX, LX_O_RDWR | nb);

        if (nf == NULL) {
            lu_free(s);
            return -LX_ENOMEM;
        }

        nf->us = s;
        nf->stype = s->stype;
        ksnprintf(nf->path, sizeof(nf->path), "socket:[unix]");

        if (uaddr != 0) {
            UINT16 fam = LX_AF_UNIX;
            UINT32 len = 2;
            if (!to_user(p, uaddr, &fam, 2) || !to_user(p, ulen, &len, 4)) {
                lfile_unref(nf);
                return -LX_EFAULT;
            }
        }

        return install(p, nf, ce);
    }

    /* TCP */
    for (;;) {

        UINT32 ip = 0;
        UINT16 port = 0;
        INTN s = sock_accept(f->sock, 0, &ip, &port);

        if (s >= 0) {
            sock_setopt(s, MYOS_SO_NONBLOCK, 1);
            LFILE *nf = lx_lfile_new(LF_INET, LX_O_RDWR | nb);
            if (nf == NULL) {
                sock_close(s);
                return -LX_ENOMEM;
            }
            nf->sock = s;
            nf->stype = LX_SOCK_STREAM;
            ksnprintf(nf->path, sizeof(nf->path), "socket:[inet]");
            INT64 r = inet_addr_out(p, uaddr, ulen, ip, port);
            if (r < 0) {
                lfile_unref(nf);
                return r;
            }
            return install(p, nf, ce);
        }

        if (s != MYOS_EAGAIN)
            return net_err(s);

        if (f->flags & LX_O_NONBLOCK)
            return -LX_EAGAIN;

        INT64 w = inet_wait(p, f->sock, MYOS_POLLIN, 0);

        if (w < 0)
            return w;
    }
}

/* ---------- sendmsg / recvmsg ---------- */

typedef struct {
    UINT64 name;
    UINT32 namelen, pad;
    UINT64 iov;
    UINT64 iovlen;
    UINT64 control;
    UINT64 controllen;
    INT32  flags, pad2;
} LX_MSGHDR;

typedef struct {
    UINT64 base, len;
} LX_IOVEC;

#define MAX_MSG (4u * 1024u * 1024u)

/* Собрать данные из iov в буфер ядра */
static INT64 gather(KPROC *p, const LX_MSGHDR *h, UINT8 **out, UINTN *len)
{
    UINTN total = 0;

    if (h->iovlen > 1024)
        return -LX_EINVAL;

    LX_IOVEC *iv = (LX_IOVEC *)kmalloc((UINTN)(h->iovlen ? h->iovlen : 1) * sizeof(LX_IOVEC));

    if (iv == NULL)
        return -LX_ENOMEM;

    if (!from_user(p, iv, h->iov, (UINTN)h->iovlen * sizeof(LX_IOVEC))) {
        kfree(iv);
        return -LX_EFAULT;
    }

    for (UINT64 i = 0; i < h->iovlen; i++)
        total += (UINTN)iv[i].len;

    if (total > MAX_MSG) {
        kfree(iv);
        return -LX_EMSGSIZE;
    }

    UINT8 *b = (UINT8 *)kmalloc(total ? total : 1);

    if (b == NULL) {
        kfree(iv);
        return -LX_ENOMEM;
    }

    UINTN o = 0;

    for (UINT64 i = 0; i < h->iovlen; i++) {
        if (!from_user(p, b + o, iv[i].base, (UINTN)iv[i].len)) {
            kfree(iv);
            kfree(b);
            return -LX_EFAULT;
        }
        o += (UINTN)iv[i].len;
    }

    kfree(iv);
    *out = b;
    *len = total;
    return 0;
}

/* Разложить n байт буфера ядра по iov */
static INT64 scatter(KPROC *p, const LX_MSGHDR *h, const UINT8 *b, UINTN n, UINTN *cap)
{
    if (h->iovlen > 1024)
        return -LX_EINVAL;

    UINTN o = 0;
    *cap = 0;

    for (UINT64 i = 0; i < h->iovlen; i++) {
        LX_IOVEC v;
        if (!from_user(p, &v, h->iov + i * sizeof(LX_IOVEC), sizeof(v)))
            return -LX_EFAULT;
        *cap += (UINTN)v.len;
        if (o < n) {
            UINTN k = (n - o < v.len) ? n - o : (UINTN)v.len;
            if (!to_user(p, v.base, b + o, k))
                return -LX_EFAULT;
            o += k;
        }
    }

    return 0;
}

/* Сколько всего места в iov */
static INT64 iov_total(KPROC *p, const LX_MSGHDR *h, UINTN *total)
{
    *total = 0;

    if (h->iovlen > 1024)
        return -LX_EINVAL;

    for (UINT64 i = 0; i < h->iovlen; i++) {
        LX_IOVEC v;
        if (!from_user(p, &v, h->iov + i * sizeof(LX_IOVEC), sizeof(v)))
            return -LX_EFAULT;
        *total += (UINTN)v.len;
    }

    if (*total > MAX_MSG)
        *total = MAX_MSG;

    return 0;
}

static INT64 do_sendmsg(KPROC *p, LFILE *f, UINT64 umsg, UINT32 flags)
{
    LX_MSGHDR h;

    if (!from_user(p, &h, umsg, sizeof(h)))
        return -LX_EFAULT;

    UINT8 *buf = NULL;
    UINTN len = 0;
    INT64 r = gather(p, &h, &buf, &len);

    if (r < 0)
        return r;

    if (f->type == LF_INET) {
        UINT32 ip = 0;
        UINT16 port = 0;
        if (h.name != 0 && h.namelen > 0) {
            r = inet_addr_in(p, h.name, h.namelen, &ip, &port);
            if (r < 0) {
                kfree(buf);
                return r;
            }
        }
        r = inet_send(p, f, buf, len, flags, ip, port);
        kfree(buf);
        return r;
    }

    /* AF_UNIX: файлы из SCM_RIGHTS */
    LFILE *fds[LU_MAXFDS];
    UINT32 nfds = 0;

    if (h.control != 0 && h.controllen >= 16) {

        UINT64 cl = (h.controllen < 4096) ? h.controllen : 4096;
        UINT8 *c = (UINT8 *)kmalloc((UINTN)cl);

        if (c == NULL || !from_user(p, c, h.control, (UINTN)cl)) {
            kfree(c);
            kfree(buf);
            return -LX_EFAULT;
        }

        for (UINT64 o = 0; o + 16 <= cl;) {
            UINT64 clen;
            INT32 lvl, typ;
            memcpy(&clen, c + o, 8);
            memcpy(&lvl, c + o + 8, 4);
            memcpy(&typ, c + o + 12, 4);
            if (clen < 16 || o + clen > cl)
                break;
            if (lvl == LX_SOL_SOCKET && typ == LX_SCM_RIGHTS) {
                for (UINT64 k = 16; k + 4 <= clen && nfds < LU_MAXFDS; k += 4) {
                    INT32 fd;
                    memcpy(&fd, c + o + k, 4);
                    LFILE *x = lx_fd_get(p, fd);
                    if (x == NULL) {
                        kfree(c);
                        kfree(buf);
                        return -LX_EBADF;
                    }
                    fds[nfds++] = x;
                }
            }
            o += (clen + 7u) & ~7ull;
        }

        kfree(c);
    }

    r = lu_send(p, f, buf, len, fds, nfds, flags);
    kfree(buf);
    return r;
}

static INT64 do_recvmsg(KPROC *p, LFILE *f, UINT64 umsg, UINT32 flags)
{
    LX_MSGHDR h;

    if (!from_user(p, &h, umsg, sizeof(h)))
        return -LX_EFAULT;

    UINTN cap = 0;
    INT64 r = iov_total(p, &h, &cap);

    if (r < 0)
        return r;

    UINT8 *buf = (UINT8 *)kmalloc(cap ? cap : 1);

    if (buf == NULL)
        return -LX_ENOMEM;

    UINT32 mflags = 0;
    UINT64 ctl_used = 0;

    if (f->type == LF_INET) {

        UINT32 ip = 0;
        UINT16 port = 0;

        r = inet_recv(p, f, buf, cap, flags, &ip, &port);

        if (r >= 0 && h.name != 0) {
            UINT8 sa[16];
            memset(sa, 0, sizeof(sa));
            sa[0] = LX_AF_INET;
            sa[2] = (UINT8)(port >> 8);
            sa[3] = (UINT8)port;
            sa[4] = (UINT8)(ip >> 24);
            sa[5] = (UINT8)(ip >> 16);
            sa[6] = (UINT8)(ip >> 8);
            sa[7] = (UINT8)ip;
            UINT32 n = (h.namelen < 16) ? h.namelen : 16;
            to_user(p, h.name, sa, n);
            h.namelen = 16;
        }

    } else {

        LFILE *got[LU_MAXFDS];
        UINT32 room = 0;

        /* сколько номеров влезет в control */
        if (h.control != 0 && h.controllen >= 16 + 4)
            room = (UINT32)((h.controllen - 16) / 4);
        if (room > LU_MAXFDS)
            room = LU_MAXFDS;

        UINT32 nfds = room;

        r = lu_recv(p, f, buf, cap, flags, got, &nfds, &mflags);

        if (r >= 0 && nfds > 0) {
            /* новые номера - получателю; SCM_RIGHTS в control */
            UINT8 c[16 + 4 * LU_MAXFDS];
            UINT64 clen = 16 + 4u * nfds;
            INT32 lvl = LX_SOL_SOCKET, typ = LX_SCM_RIGHTS;
            memcpy(c, &clen, 8);
            memcpy(c + 8, &lvl, 4);
            memcpy(c + 12, &typ, 4);
            for (UINT32 i = 0; i < nfds; i++) {
                INT64 nfd = lx_fd_install(p, got[i], (flags & LX_MSG_CMSG_CLOEXEC) != 0);
                if (nfd < 0) {
                    lfile_unref(got[i]);
                    nfd = -1;
                    mflags |= LX_MSG_CTRUNC;
                }
                INT32 v = (INT32)nfd;
                memcpy(c + 16 + 4 * i, &v, 4);
            }
            to_user(p, h.control, c, (UINTN)clen);
            ctl_used = (clen + 7u) & ~7ull;
        }

        if (h.name != 0) {
            UINT16 fam = LX_AF_UNIX;
            to_user(p, h.name, &fam, (h.namelen >= 2) ? 2 : 0);
            h.namelen = 2;
        }
    }

    if (r >= 0) {
        UINTN dummy;
        INT64 e = scatter(p, &h, buf, (UINTN)r, &dummy);
        if (e < 0)
            r = e;
    }

    kfree(buf);

    if (r < 0)
        return r;

    h.controllen = ctl_used;
    h.flags = (INT32)mflags;

    /* "обрезано" для датаграммы: вернуть настоящую длину нельзя -
       мы её уже потеряли; программы смотрят на флаг */
    if (!to_user(p, umsg, &h, sizeof(h)))
        return -LX_EFAULT;

    return r;
}

/* ---------- getsockopt / setsockopt ---------- */

static INT64 sys_getsockopt(KPROC *p, LFILE *f, UINT64 level, UINT64 opt, UINT64 uval, UINT64 ulen)
{
    UINT32 len = 0;

    if (!from_user(p, &len, ulen, 4))
        return -LX_EFAULT;

    UINT8 out[16];
    UINT32 n = 4;
    INT32 v = 0;

    memset(out, 0, sizeof(out));

    if (level == LX_SOL_SOCKET) {
        switch (opt) {
        case LX_SO_TYPE:
            v = (INT32)f->stype;
            break;
        case LX_SO_DOMAIN:
            v = (f->type == LF_UNIX) ? LX_AF_UNIX : LX_AF_INET;
            break;
        case LX_SO_PROTOCOL:
            v = 0;
            break;
        case LX_SO_ERROR:
            if (f->type == LF_INET) {
                INTN e = sock_setopt(f->sock, MYOS_SO_ERROR, 0);
                v = (e < 0 && e != MYOS_EINPROGRESS) ? (INT32)-net_err(e) : 0;
            }
            break;
        case LX_SO_SNDBUF:
        case LX_SO_RCVBUF:
            v = 212992;
            break;
        case LX_SO_ACCEPTCONN:
            v = (f->type == LF_UNIX && f->us->state == LU_LISTEN) ? 1 : 0;
            break;
        case LX_SO_PEERCRED: {
            /* кто на том конце: pid, uid, gid */
            UINT32 cred[3] = { (f->type == LF_UNIX) ? f->us->peer_pid : 0, 0, 0 };
            memcpy(out, cred, 12);
            n = 12;
            break;
        }
        default:
            v = 0;
        }
    }

    if (n == 4)
        memcpy(out, &v, 4);

    if (len < n)
        n = len;

    if (!to_user(p, uval, out, n) || !to_user(p, ulen, &n, 4))
        return -LX_EFAULT;

    return 0;
}

static INT64 sys_setsockopt(KPROC *p, LFILE *f, UINT64 level, UINT64 opt, UINT64 uval, UINT64 len)
{
    if (level == LX_SOL_SOCKET && opt == LX_SO_RCVTIMEO && len >= 16) {
        INT64 tv[2];
        if (!from_user(p, tv, uval, 16))
            return -LX_EFAULT;
        f->rcvtimeo_ms = (UINT64)tv[0] * 1000u + (UINT64)tv[1] / 1000u;
    }

    /* остальное (SO_REUSEADDR, TCP_NODELAY, SO_KEEPALIVE...) - принимаем
       молча: наш стек и так ведёт себя разумно */
    return 0;
}

static INT64 sys_sockname(KPROC *p, LFILE *f, UINT64 uaddr, UINT64 ulen, BOOLEAN peer)
{
    if (f->type == LF_UNIX) {
        UINT8 sa[110];
        UINT32 n = 2;
        UINT32 cap = 0;
        LUSOCK *u = peer ? f->us->peer : f->us;

        if (peer && u == NULL)
            return -LX_ENOTCONN;

        memset(sa, 0, sizeof(sa));
        sa[0] = LX_AF_UNIX;

        if (u != NULL && u->bound && u->name[0] != '@') {
            char lp[VFS_PATH_MAX];
            lx_path_to_linux(p, u->name, lp, sizeof(lp));
            UINTN k = 0;
            while (lp[k] && k < 107) {
                sa[2 + k] = (UINT8)lp[k];
                k++;
            }
            n = (UINT32)(2 + k + 1);
        }

        if (!from_user(p, &cap, ulen, 4))
            return -LX_EFAULT;

        if (!to_user(p, uaddr, sa, (cap < n) ? cap : n) || !to_user(p, ulen, &n, 4))
            return -LX_EFAULT;

        return 0;
    }

    UINT32 ip = 0;
    UINT16 port = 0;

    if (peer) {
        INTN r = sock_peer(f->sock, &ip, &port);
        if (r < 0)
            return net_err(r);
    } else {
        INTN r = sock_setopt(f->sock, MYOS_SO_LOCALADDR, 0);
        if (r < 0)
            return net_err(r);
        ip = (UINT32)((UINT64)r >> 16);
        port = (UINT16)r;
    }

    return inet_addr_out(p, uaddr, ulen, ip, port);
}

static BOOLEAN is_sock(LFILE *f)
{
    return f != NULL && (f->type == LF_INET || f->type == LF_UNIX);
}

INT64 lx_sock_syscall(KPROC *p, UINT64 nr, UINT64 *a, BOOLEAN *handled)
{
    LFILE *f = NULL;

    *handled = TRUE;

    switch (nr) {

    case NR_socket:
        return sys_socket(p, a[0], a[1], a[2]);

    case NR_socketpair:
        return sys_socketpair(p, a[0], a[1], a[3]);

    case NR_connect:
    case NR_bind:
    case NR_listen:
    case NR_accept:
    case NR_accept4:
    case NR_sendto:
    case NR_recvfrom:
    case NR_sendmsg:
    case NR_recvmsg:
    case NR_shutdown:
    case NR_getsockname:
    case NR_getpeername:
    case NR_getsockopt:
    case NR_setsockopt:
    case NR_sendmmsg:
    case NR_recvmmsg:
        f = lx_fd_get(p, (INT64)(INT32)a[0]);
        if (f == NULL)
            return -LX_EBADF;
        if (!is_sock(f))
            return -LX_ENOTSOCK;
        break;

    case NR_epoll_create:
    case NR_epoll_create1: {
        LFILE *e = lx_lfile_new(LF_EPOLL, LX_O_RDWR);
        if (e == NULL)
            return -LX_ENOMEM;
        e->ep = (LEPOLL *)kzalloc(sizeof(LEPOLL));
        if (e->ep == NULL) {
            kfree(e);
            return -LX_ENOMEM;
        }
        ksnprintf(e->path, sizeof(e->path), "anon_inode:[eventpoll]");
        return install(p, e, nr == NR_epoll_create1 && (a[0] & LX_O_CLOEXEC));
    }

    case NR_epoll_ctl:
        f = lx_fd_get(p, (INT64)(INT32)a[0]);
        if (f == NULL || f->type != LF_EPOLL)
            return f ? -LX_EINVAL : -LX_EBADF;
        return ep_ctl(p, f, (INT64)a[1], (INT64)(INT32)a[2], a[3]);

    case NR_epoll_wait:
    case NR_epoll_pwait:
    case NR_epoll_pwait2: {
        f = lx_fd_get(p, (INT64)(INT32)a[0]);
        if (f == NULL || f->type != LF_EPOLL)
            return f ? -LX_EINVAL : -LX_EBADF;
        INT64 ms = (INT64)(INT32)a[3];
        if (nr == NR_epoll_pwait2) {
            if (a[3] == 0) {
                ms = -1;
            } else {
                INT64 ts[2];
                if (!from_user(p, ts, a[3], 16))
                    return -LX_EFAULT;
                ms = ts[0] * 1000 + ts[1] / 1000000;
            }
        }
        return ep_wait(p, f, a[1], (INT64)(INT32)a[2], ms);
    }

    case NR_timerfd_create: {
        if (a[0] != 0 && a[0] != 1 && a[0] != 7 && a[0] != 8)   /* REALTIME, MONOTONIC, BOOTTIME, ALARM */
            return -LX_EINVAL;
        LFILE *t = lx_lfile_new(LF_TIMERFD, LX_O_RDWR | (a[1] & LX_O_NONBLOCK));
        if (t == NULL)
            return -LX_ENOMEM;
        t->t_clock = (a[0] == 0 || a[0] == 8) ? 0 : 1;
        ksnprintf(t->path, sizeof(t->path), "anon_inode:[timerfd]");
        return install(p, t, (a[1] & LX_O_CLOEXEC) != 0);
    }

    case NR_timerfd_settime:
        f = lx_fd_get(p, (INT64)(INT32)a[0]);
        if (f == NULL || f->type != LF_TIMERFD)
            return f ? -LX_EINVAL : -LX_EBADF;
        return tfd_settime(p, f, a[1], a[2], a[3]);

    case NR_timerfd_gettime: {
        f = lx_fd_get(p, (INT64)(INT32)a[0]);
        if (f == NULL || f->type != LF_TIMERFD)
            return f ? -LX_EINVAL : -LX_EBADF;
        tfd_update(f);
        UINT64 now = lx_now_ns(f->t_clock == 0);
        UINT64 left = (f->t_next_ns > now) ? f->t_next_ns - now : 0;
        LX_ITIMERSPEC ts = { (INT64)(f->t_iv_ns / 1000000000u), (INT64)(f->t_iv_ns % 1000000000u),
                             (INT64)(left / 1000000000u), (INT64)(left % 1000000000u) };
        return to_user(p, a[1], &ts, sizeof(ts)) ? 0 : -LX_EFAULT;
    }

    default:
        *handled = FALSE;
        return 0;
    }

    /* операции над сокетом f */
    switch (nr) {

    case NR_connect:
        if (f->type == LF_UNIX)
            return lu_connect(p, f, a[1], a[2]);
        return inet_connect(p, f, a[1], a[2]);

    case NR_bind:
        if (f->type == LF_UNIX)
            return lu_bind(p, f->us, a[1], a[2]);
        {
            UINT32 ip;
            UINT16 port;
            INT64 r = inet_addr_in(p, a[1], a[2], &ip, &port);
            if (r < 0)
                return r;
            INTN b = sock_bind(f->sock, ip, port);
            return (b < 0) ? net_err(b) : 0;
        }

    case NR_listen:
        if (f->type == LF_UNIX) {
            if (!f->us->bound || f->us->state == LU_CONN)
                return -LX_EINVAL;
            f->us->state = LU_LISTEN;
            f->us->bl_max = (a[1] > 0 && a[1] < 128) ? (UINT32)a[1] : 128;
            return 0;
        } else {
            INTN r = sock_listen(f->sock, (UINT32)a[1]);
            return (r < 0) ? net_err(r) : 0;
        }

    case NR_accept:
    case NR_accept4:
        return sys_accept(p, f, a[1], a[2], (nr == NR_accept4) ? a[3] : 0);

    case NR_sendto: {
        if (!uptr_ok(p, a[1], a[2], FALSE))
            return -LX_EFAULT;
        if (f->type == LF_UNIX)
            return lu_send(p, f, (const UINT8 *)(UINTN)a[1], (UINTN)a[2], NULL, 0, (UINT32)a[3]);
        UINT32 ip = 0;
        UINT16 port = 0;
        if (a[4] != 0) {
            INT64 r = inet_addr_in(p, a[4], a[5], &ip, &port);
            if (r < 0)
                return r;
        }
        UINT8 *k = (UINT8 *)kmalloc(a[2] ? (UINTN)a[2] : 1);
        if (k == NULL)
            return -LX_ENOMEM;
        memcpy(k, (const void *)(UINTN)a[1], (UINTN)a[2]);
        INT64 r = inet_send(p, f, k, (UINTN)a[2], (UINT32)a[3], ip, port);
        kfree(k);
        return r;
    }

    case NR_recvfrom: {
        if (!uptr_ok(p, a[1], a[2], TRUE))
            return -LX_EFAULT;
        UINT8 *k = (UINT8 *)kmalloc(a[2] ? (UINTN)a[2] : 1);
        if (k == NULL)
            return -LX_ENOMEM;
        INT64 r;
        if (f->type == LF_UNIX) {
            UINT32 mf = 0;
            r = lu_recv(p, f, k, (UINTN)a[2], (UINT32)a[3], NULL, NULL, &mf);
            if (r >= 0 && a[4] != 0) {
                UINT16 fam = LX_AF_UNIX;
                UINT32 two = 2;
                to_user(p, a[4], &fam, 2);
                to_user(p, a[5], &two, 4);
            }
        } else {
            UINT32 ip = 0;
            UINT16 port = 0;
            r = inet_recv(p, f, k, (UINTN)a[2], (UINT32)a[3], &ip, &port);
            if (r >= 0 && a[4] != 0) {
                INT64 e = inet_addr_out(p, a[4], a[5], ip, port);
                if (e < 0)
                    r = e;
            }
        }
        if (r > 0)
            memcpy((void *)(UINTN)a[1], k, (UINTN)r);
        kfree(k);
        return r;
    }

    case NR_sendmsg:
        return do_sendmsg(p, f, a[1], (UINT32)a[2]);

    case NR_recvmsg:
        return do_recvmsg(p, f, a[1], (UINT32)a[2]);

    case NR_sendmmsg:
    case NR_recvmmsg: {
        /* несколько сообщений: struct mmsghdr = msghdr + длина (64 байта) */
        UINT64 n = a[2];
        if (n > 1024)
            n = 1024;
        UINT64 i = 0;
        for (; i < n; i++) {
            UINT64 um = a[1] + i * 64u;
            INT64 r = (nr == NR_sendmmsg) ? do_sendmsg(p, f, um, (UINT32)a[3])
                                          : do_recvmsg(p, f, um, (UINT32)a[3] | (i ? LX_MSG_DONTWAIT : 0));
            if (r < 0)
                return i ? (INT64)i : r;
            UINT32 len = (UINT32)r;
            if (!to_user(p, um + 56, &len, 4))
                return i ? (INT64)i : -LX_EFAULT;
        }
        return (INT64)i;
    }

    case NR_shutdown:
        if (f->type == LF_UNIX) {
            LUSOCK *u = f->us;
            if (a[1] == 0 || a[1] == 2)
                u->shut_rd = TRUE;
            if (a[1] == 1 || a[1] == 2) {
                u->shut_wr = TRUE;
                if (u->peer != NULL) {
                    u->peer->shut_rd = TRUE;    /* у собеседника - конец потока */
                    lu_wake(u->peer);
                }
            }
            lu_wake(u);
            return 0;
        }
        return 0;           /* TCP: полузакрытия наш стек не умеет - закроется с close */

    case NR_getsockname:
        return sys_sockname(p, f, a[1], a[2], FALSE);

    case NR_getpeername:
        return sys_sockname(p, f, a[1], a[2], TRUE);

    case NR_getsockopt:
        return sys_getsockopt(p, f, a[1], a[2], a[3], a[4]);

    case NR_setsockopt:
        return sys_setsockopt(p, f, a[1], a[2], a[3], a[4]);
    }

    return -LX_ENOSYS;
}

/* Сколько байт можно прочитать сразу (ioctl FIONREAD) */
UINTN lxs_pending(LFILE *f)
{
    if (f->type == LF_INET) {
        INTN r = sock_pending(f->sock);
        return (r > 0) ? (UINTN)r : 0;
    }

    if (f->type == LF_UNIX && f->us->rq != NULL)
        return (f->us->stype == LX_SOCK_STREAM) ? f->us->rq_bytes : f->us->rq->len - f->us->rq->off;

    return 0;
}
