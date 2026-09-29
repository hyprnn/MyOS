/*
 * net/socket.c - сокеты: сеть глазами программы. Часть MyOS; общие
 * объявления - в net/net.h и myos.h.
 *
 * Сокет - "розетка", в которую программа пишет и из которой читает:
 *   SOCK_STREAM (TCP) - поток байтов с установкой соединения:
 *                        connect / listen + accept, send / recv;
 *   SOCK_DGRAM  (UDP) - отдельные сообщения: sendto / recvfrom;
 *   SOCK_PING   (ICMP)- эхо-запросы для ping (как "ping-сокеты"
 *                        Linux: программа даёт заголовок ICMP и
 *                        данные, ядро ставит номер и контрольную сумму).
 * Для программы сокет - обычный номер файла (fd): write/read/close
 * работают и с ним (см. kernel/syscall.c).
 *
 * Все функции берут g_net_mutex сами. Ожидание (данных, соединения,
 * места в буфере) - net_wait: замок отпускается, поток спит до
 * события или таймаута; программу при этом можно прервать Ctrl+C.
 */
#include "net.h"
#include "tcp.h"

#define SOCK_MAX   32
#define SOCK_DQ    16          /* сообщений UDP/ICMP в очереди */

typedef struct {
    UINT32  src;
    UINT16  sport;
    UINT8   ttl;
    UINT16  len;
    UINT8  *data;
} SOCK_DGRAM;

typedef struct SOCKET {
    BOOLEAN used;
    UINT8   type;              /* MYOS_SOCK_* */
    UINT32  pid;               /* чей (0 - ядра) */
    UINT32  lip;
    UINT16  lport;
    UINT32  rip;
    UINT16  rport;
    BOOLEAN connected;
    TCB    *tcb;
    SOCK_DGRAM q[SOCK_DQ];
    UINT8   qh, qn;
    UINT16  icmp_id;
    UINT64  timeout_ms;        /* 0 - ждать сколько угодно */
    BOOLEAN nonblock;          /* не ждать вовсе (MYOS_SO_NONBLOCK) */
    UINT64  dropped;
} SOCKET;

static SOCKET g_socks[SOCK_MAX];
static UINT16 g_next_port = 0;
static UINT16 g_next_icmp_id = 0;

/* ================================================================
 * Помощники
 * ================================================================ */

static SOCKET *sock_get(INTN s)
{
    if (s < 0 || s >= SOCK_MAX || !g_socks[s].used)
        return NULL;

    return &g_socks[s];
}

static BOOLEAN port_busy(UINT8 proto, UINT16 port)
{
    for (UINTN i = 0; i < SOCK_MAX; i++) {
        SOCKET *k = &g_socks[i];
        if (!k->used || k->lport != port)
            continue;
        if ((proto == IP_PROTO_TCP) == (k->type == MYOS_SOCK_STREAM))
            return TRUE;
    }

    if (proto == IP_PROTO_TCP)
        for (UINTN i = 0; i < TCB_MAX; i++)
            if (g_tcb[i].used && g_tcb[i].lport == port)
                return TRUE;

    return FALSE;
}

/* Свободный "временный" порт 49152..65535 (как у всех ОС) */
UINT16 sock_ephemeral_port(UINT8 proto)
{
    if (g_next_port == 0)
        g_next_port = (UINT16)(49152u + net_random() % 16000u);

    for (UINTN guard = 0; guard < 16384; guard++) {

        UINT16 p = g_next_port++;

        if (g_next_port < 49152u)
            g_next_port = 49152u;

        if (!port_busy(proto, p))
            return p;
    }

    return 0;
}

/* Программу прервали (Ctrl+C, закрыли окно)? */
static BOOLEAN sock_interrupted(void)
{
    return g_kcur != NULL && g_kcur->proc != NULL && g_kcur->proc->killed;
}

/*
 * Подождать события сокета: не дольше, чем осталось до deadline
 * (0 - без срока). Кусками по 100 мс - чтобы заметить Ctrl+C.
 * Возвращает 0 - можно проверять условие снова, MYOS_EAGAIN - вышел
 * срок, MYOS_EINTR - программу прервали.
 */
static INTN sock_wait(SOCKET *k, UINT64 deadline, const char *what)
{
    if (sock_interrupted())
        return MYOS_EINTR;

    /* неблокирующий сокет: "пока нельзя" - сразу, программа спросит
       снова, когда poll скажет, что можно */
    if (k->nonblock)
        return MYOS_EAGAIN;

    UINT64 now = net_now_ms();
    UINT64 slice = 100;

    if (deadline != 0) {
        if (now >= deadline)
            return MYOS_EAGAIN;
        if (deadline - now < slice)
            slice = deadline - now;
    }

    net_wait(k, what, slice);

    if (sock_interrupted())
        return MYOS_EINTR;

    return 0;
}

static UINT64 sock_deadline(SOCKET *k)
{
    return k->timeout_ms ? net_now_ms() + k->timeout_ms : 0;
}

static void dq_clear(SOCKET *k)
{
    while (k->qn > 0) {
        SOCK_DGRAM *d = &k->q[k->qh];
        if (d->data)
            kfree(d->data);
        d->data = NULL;
        k->qh = (UINT8)((k->qh + 1u) % SOCK_DQ);
        k->qn--;
    }
}

static BOOLEAN dq_push(SOCKET *k, UINT32 src, UINT16 sport, UINT8 ttl,
                       const UINT8 *data, UINTN len)
{
    if (k->qn >= SOCK_DQ) {
        k->dropped++;
        return FALSE;
    }

    UINT8 *copy = (UINT8 *)kmalloc(len ? len : 1);

    if (copy == NULL) {
        k->dropped++;
        return FALSE;
    }

    memcpy(copy, data, len);

    SOCK_DGRAM *d = &k->q[(k->qh + k->qn) % SOCK_DQ];

    d->src = src;
    d->sport = sport;
    d->ttl = ttl;
    d->len = (UINT16)len;
    d->data = copy;
    k->qn++;

    net_wake(k);
    return TRUE;
}

/* ================================================================
 * Приём от стека (поток net, замок уже взят)
 * ================================================================ */

BOOLEAN sock_udp_input(UINT32 src, UINT16 sport, UINT32 dst, UINT16 dport,
                       const UINT8 *data, UINTN len)
{
    for (UINTN i = 0; i < SOCK_MAX; i++) {

        SOCKET *k = &g_socks[i];

        if (!k->used || k->type != MYOS_SOCK_DGRAM || k->lport != dport)
            continue;
        if (k->lip != 0 && k->lip != dst && dst != 0xFFFFFFFFu)
            continue;
        if (k->connected && (k->rip != src || k->rport != sport))
            continue;

        dq_push(k, src, sport, 0, data, len);
        return TRUE;
    }

    return FALSE;
}

BOOLEAN sock_icmp_input(UINT32 src, UINT8 ttl, const UINT8 *icmp, UINTN len)
{
    UINT16 id;

    if (icmp[0] == 0) {
        id = net_get16(icmp + 4);
    } else {
        /* ошибка: внутри - заголовок нашего исходного пакета и
           8 байт его ICMP (там номер id) */
        if (len < 8 + IP_HLEN + 8)
            return FALSE;
        const UINT8 *in = icmp + 8;
        UINTN ihl = (UINTN)(in[0] & 0xFu) * 4u;
        if (in[9] != IP_PROTO_ICMP || len < 8 + ihl + 8)
            return FALSE;
        id = net_get16(in + ihl + 4);
    }

    for (UINTN i = 0; i < SOCK_MAX; i++) {
        SOCKET *k = &g_socks[i];
        if (k->used && k->type == MYOS_SOCK_PING && k->icmp_id == id) {
            dq_push(k, src, 0, ttl, icmp, len);
            return TRUE;
        }
    }

    return FALSE;
}

/* ================================================================
 * API сокетов
 * ================================================================ */

INTN sock_create(UINT32 type, UINT32 pid)
{
    if (type != MYOS_SOCK_STREAM && type != MYOS_SOCK_DGRAM && type != MYOS_SOCK_PING)
        return MYOS_EINVAL;

    kmutex_lock(&g_net_mutex);

    INTN s = MYOS_EMFILE;

    for (UINTN i = 0; i < SOCK_MAX; i++) {

        if (g_socks[i].used)
            continue;

        SOCKET *k = &g_socks[i];

        memset(k, 0, sizeof(*k));
        k->used = TRUE;
        k->type = (UINT8)type;
        k->pid = pid;

        if (type == MYOS_SOCK_PING) {
            if (g_next_icmp_id == 0)
                g_next_icmp_id = (UINT16)(net_random() | 1u);
            k->icmp_id = g_next_icmp_id++;
        }

        s = (INTN)i;
        break;
    }

    kmutex_unlock(&g_net_mutex);

    return s;
}

INTN sock_bind(INTN s, UINT32 ip, UINT16 port)
{
    kmutex_lock(&g_net_mutex);

    SOCKET *k = sock_get(s);
    INTN r = 0;

    if (k == NULL) {
        r = MYOS_EBADF;
    } else if (k->lport != 0) {
        r = MYOS_EINVAL;
    } else {
        UINT8 proto = (k->type == MYOS_SOCK_STREAM) ? IP_PROTO_TCP : IP_PROTO_UDP;
        if (port == 0)
            port = sock_ephemeral_port(proto);
        if (port == 0 || port_busy(proto, port))
            r = MYOS_EADDRINUSE;
        else {
            k->lip = ip;
            k->lport = port;
        }
    }

    kmutex_unlock(&g_net_mutex);

    return r;
}

INTN sock_connect(INTN s, UINT32 ip, UINT16 port)
{
    kmutex_lock(&g_net_mutex);

    SOCKET *k = sock_get(s);
    INTN r = 0;

    if (k == NULL) {
        kmutex_unlock(&g_net_mutex);
        return MYOS_EBADF;
    }

    UINT32 lip = ip_src_for(ip);

    if (lip == 0) {
        kmutex_unlock(&g_net_mutex);
        return MYOS_ENETUNREACH;       /* нет сети / адреса / шлюза */
    }

    if (k->type != MYOS_SOCK_STREAM) {
        /* UDP/ping: просто запомнить собеседника */
        if (k->type == MYOS_SOCK_DGRAM && k->lport == 0)
            k->lport = sock_ephemeral_port(IP_PROTO_UDP);
        k->rip = ip;
        k->rport = port;
        k->connected = TRUE;
        kmutex_unlock(&g_net_mutex);
        return 0;
    }

    if (k->tcb != NULL) {
        kmutex_unlock(&g_net_mutex);
        return MYOS_EINVAL;
    }

    if (k->lport == 0)
        k->lport = sock_ephemeral_port(IP_PROTO_TCP);

    TCB *t = tcp_connect(lip, k->lport, ip, port);

    if (t == NULL) {
        kmutex_unlock(&g_net_mutex);
        return MYOS_EMFILE;
    }

    t->sock = k;
    k->tcb = t;
    k->lip = lip;
    k->rip = ip;
    k->rport = port;

    UINT64 dl = sock_deadline(k);

    for (;;) {

        if (t->state == TCP_ESTABLISHED || t->state == TCP_CLOSE_WAIT) {
            k->connected = TRUE;
            r = 0;
            break;
        }

        if (t->state == TCP_CLOSED) {
            r = t->error ? t->error : MYOS_ECONNREFUSED;
            break;
        }

        if (k->nonblock) {
            /* соединение устанавливается дальше само; готовность -
               poll (можно писать), итог - MYOS_SO_ERROR */
            kmutex_unlock(&g_net_mutex);
            return MYOS_EINPROGRESS;
        }

        INTN w = sock_wait(k, dl, "tcp connect");

        if (w != 0) {
            r = (w == MYOS_EAGAIN) ? MYOS_ETIMEDOUT : w;
            break;
        }
    }

    if (r != 0) {
        /* не вышло - соединение забыть, сокет можно закрывать */
        tcp_abort(t);
        k->tcb = NULL;
    }

    kmutex_unlock(&g_net_mutex);

    return r;
}

INTN sock_listen(INTN s, UINT32 backlog)
{
    kmutex_lock(&g_net_mutex);

    SOCKET *k = sock_get(s);
    INTN r = 0;

    if (k == NULL)
        r = MYOS_EBADF;
    else if (k->type != MYOS_SOCK_STREAM || k->tcb != NULL || k->lport == 0)
        r = MYOS_EINVAL;
    else {
        TCB *t = tcp_listen(k->lip, k->lport, backlog > 16 ? 16 : backlog);
        if (t == NULL)
            r = MYOS_EMFILE;
        else {
            t->sock = k;
            k->tcb = t;
        }
    }

    kmutex_unlock(&g_net_mutex);

    return r;
}

INTN sock_accept(INTN s, UINT32 pid, UINT32 *ip, UINT16 *port)
{
    kmutex_lock(&g_net_mutex);

    SOCKET *k = sock_get(s);

    if (k == NULL || k->tcb == NULL || k->tcb->state != TCP_LISTEN) {
        kmutex_unlock(&g_net_mutex);
        return k ? MYOS_EINVAL : MYOS_EBADF;
    }

    UINT64 dl = sock_deadline(k);
    INTN r;

    for (;;) {

        TCB *c = NULL;

        for (UINTN i = 0; i < TCB_MAX; i++) {
            TCB *t = &g_tcb[i];
            if (t->used && t->listener == k->tcb && !t->accepted &&
                (t->state == TCP_ESTABLISHED || t->state == TCP_CLOSE_WAIT)) {
                c = t;
                break;
            }
        }

        if (c != NULL) {

            INTN ns = -1;

            for (UINTN i = 0; i < SOCK_MAX; i++)
                if (!g_socks[i].used) {
                    ns = (INTN)i;
                    break;
                }

            if (ns < 0) {
                r = MYOS_EMFILE;
                break;
            }

            SOCKET *n = &g_socks[ns];

            memset(n, 0, sizeof(*n));
            n->used = TRUE;
            n->type = MYOS_SOCK_STREAM;
            n->pid = pid;
            n->lip = c->lip;
            n->lport = c->lport;
            n->rip = c->rip;
            n->rport = c->rport;
            n->connected = TRUE;
            n->tcb = c;
            c->sock = n;
            c->accepted = TRUE;

            if (ip) *ip = c->rip;
            if (port) *port = c->rport;

            r = ns;
            break;
        }

        INTN w = sock_wait(k, dl, "tcp accept");

        if (w != 0) {
            r = w;
            break;
        }
    }

    kmutex_unlock(&g_net_mutex);

    return r;
}

/* Отправить: TCP - в поток (ждёт места в буфере), UDP/ping - одно
   сообщение (ip == 0 - собеседнику из connect) */
INTN sock_sendto(INTN s, const void *buf, UINTN n, UINT32 ip, UINT16 port)
{
    kmutex_lock(&g_net_mutex);

    SOCKET *k = sock_get(s);
    INTN r = 0;

    if (k == NULL) {
        kmutex_unlock(&g_net_mutex);
        return MYOS_EBADF;
    }

    if (k->type == MYOS_SOCK_STREAM) {

        TCB *t = k->tcb;
        const UINT8 *src = (const UINT8 *)buf;
        UINTN done = 0;
        UINT64 dl = sock_deadline(k);

        if (t == NULL || t->state == TCP_LISTEN) {
            kmutex_unlock(&g_net_mutex);
            return MYOS_ENOTCONN;
        }

        while (done < n) {

            /* неблокирующий connect ещё идёт - писать пока некуда */
            if (k->nonblock && (t->state == TCP_SYN_SENT || t->state == TCP_SYN_RCVD)) {
                r = MYOS_EAGAIN;
                break;
            }

            if (t->state != TCP_ESTABLISHED && t->state != TCP_CLOSE_WAIT) {
                r = t->error ? t->error : MYOS_ENOTCONN;
                break;
            }

            UINT32 room = TCP_SBUF - t->sbuf_len;

            if (room == 0) {
                INTN w = sock_wait(k, dl, "tcp send");
                if (w != 0) {
                    r = w;
                    break;
                }
                continue;
            }

            UINT32 c = (n - done < room) ? (UINT32)(n - done) : room;

            for (UINT32 i = 0; i < c; i++)
                t->sbuf[(t->sbuf_start + t->sbuf_len + i) % TCP_SBUF] = src[done + i];

            t->sbuf_len += c;
            done += c;

            tcp_output(t);
        }

        kmutex_unlock(&g_net_mutex);

        return (done > 0) ? (INTN)done : r;
    }

    if (ip == 0) {
        if (!k->connected) {
            kmutex_unlock(&g_net_mutex);
            return MYOS_ENOTCONN;
        }
        ip = k->rip;
        port = k->rport;
    }

    if (k->type == MYOS_SOCK_DGRAM) {

        if (n > NET_MTU - IP_HLEN - UDP_HLEN) {
            kmutex_unlock(&g_net_mutex);
            return MYOS_EINVAL;
        }

        if (k->lport == 0)
            k->lport = sock_ephemeral_port(IP_PROTO_UDP);

        r = udp_send(k->lip, k->lport, ip, port, (const UINT8 *)buf, n);

    } else {

        /* ping: заголовок ICMP (8 байт) + данные от программы;
           номер id и контрольная сумма - наши */
        static UINT8 m[NET_MTU];

        if (n < 8 || n > NET_MTU - IP_HLEN) {
            kmutex_unlock(&g_net_mutex);
            return MYOS_EINVAL;
        }

        memcpy(m, buf, n);
        m[0] = 8;                         /* Echo Request */
        m[1] = 0;
        net_put16(m + 2, 0);
        net_put16(m + 4, k->icmp_id);
        net_put16(m + 2, net_csum(m, n, 0));

        r = ip_send(0, ip, IP_PROTO_ICMP, m, n);
    }

    kmutex_unlock(&g_net_mutex);

    return (r < 0) ? MYOS_ENETUNREACH : (INTN)n;
}

INTN sock_send(INTN s, const void *buf, UINTN n)
{
    return sock_sendto(s, buf, n, 0, 0);
}

/* Принять: TCP - сколько есть (0 - собеседник закрыл), UDP/ping -
   одно сообщение (лишнее обрезается) */
INTN sock_recvfrom(INTN s, void *buf, UINTN n, UINT32 *ip, UINT16 *port, UINT8 *ttl)
{
    kmutex_lock(&g_net_mutex);

    SOCKET *k = sock_get(s);
    INTN r = 0;

    if (k == NULL) {
        kmutex_unlock(&g_net_mutex);
        return MYOS_EBADF;
    }

    UINT64 dl = sock_deadline(k);

    if (k->type == MYOS_SOCK_STREAM) {

        TCB *t = k->tcb;

        if (t == NULL || t->state == TCP_LISTEN) {
            kmutex_unlock(&g_net_mutex);
            return MYOS_ENOTCONN;
        }

        for (;;) {

            if (t->rbuf_len > 0) {

                UINT32 c = (n < t->rbuf_len) ? (UINT32)n : t->rbuf_len;
                UINT8 *dst = (UINT8 *)buf;

                for (UINT32 i = 0; i < c; i++)
                    dst[i] = t->rbuf[(t->rbuf_start + i) % TCP_RBUF];

                t->rbuf_start = (t->rbuf_start + c) % TCP_RBUF;
                t->rbuf_len -= c;

                tcp_window_update(t);

                if (ip) *ip = t->rip;
                if (port) *port = t->rport;
                r = (INTN)c;
                break;
            }

            if (t->rcv_fin) {
                r = 0;                        /* конец потока */
                break;
            }

            if (t->state == TCP_CLOSED) {
                r = t->error ? t->error : 0;
                break;
            }

            INTN w = sock_wait(k, dl, "tcp recv");

            if (w != 0) {
                r = w;
                break;
            }
        }

        kmutex_unlock(&g_net_mutex);
        return r;
    }

    if (k->type == MYOS_SOCK_DGRAM && k->lport == 0)
        k->lport = sock_ephemeral_port(IP_PROTO_UDP);  /* иначе ответ не найдёт нас */

    for (;;) {

        if (k->qn > 0) {

            SOCK_DGRAM *d = &k->q[k->qh];
            UINTN c = (n < d->len) ? n : d->len;

            memcpy(buf, d->data, c);

            if (ip) *ip = d->src;
            if (port) *port = d->sport;
            if (ttl) *ttl = d->ttl;

            kfree(d->data);
            d->data = NULL;
            k->qh = (UINT8)((k->qh + 1u) % SOCK_DQ);
            k->qn--;

            r = (INTN)c;
            break;
        }

        INTN w = sock_wait(k, dl, k->type == MYOS_SOCK_PING ? "ping reply" : "udp recv");

        if (w != 0) {
            r = w;
            break;
        }
    }

    kmutex_unlock(&g_net_mutex);

    return r;
}

INTN sock_recv(INTN s, void *buf, UINTN n)
{
    return sock_recvfrom(s, buf, n, NULL, NULL, NULL);
}

INTN sock_setopt(INTN s, UINT32 opt, UINT64 val)
{
    kmutex_lock(&g_net_mutex);

    SOCKET *k = sock_get(s);
    INTN r = 0;

    if (k == NULL)
        r = MYOS_EBADF;
    else if (opt == MYOS_SO_TIMEOUT)
        k->timeout_ms = val;
    else if (opt == MYOS_SO_NONBLOCK)
        k->nonblock = (val != 0);
    else if (opt == MYOS_SO_ERROR) {
        /* как идёт соединение (неблокирующий connect) */
        TCB *t = k->tcb;
        if (k->type != MYOS_SOCK_STREAM || t == NULL)
            r = 0;
        else if (t->state == TCP_SYN_SENT || t->state == TCP_SYN_RCVD)
            r = MYOS_EINPROGRESS;
        else if (t->state == TCP_CLOSED && !k->connected)
            r = t->error ? t->error : MYOS_ECONNREFUSED;
        else {
            k->connected = TRUE;
            r = 0;
        }
    } else if (opt == MYOS_SO_LOCALADDR) {
        UINT32 ip = k->lip;
        if (ip == 0 && k->tcb != NULL)
            ip = k->tcb->lip;
        r = (INTN)(((UINT64)ip << 16) | k->lport);
    } else
        r = MYOS_EINVAL;

    kmutex_unlock(&g_net_mutex);

    return r;
}

/*
 * Что с сокетом можно сделать без ожидания (для poll): MYOS_POLLIN -
 * есть данные или конец потока (recv не будет ждать), MYOS_POLLOUT -
 * есть место для send (или неблокирующий connect закончился),
 * MYOS_POLLHUP/ERR - соединение закрыто / не удалось.
 */
UINT32 sock_poll(INTN s)
{
    kmutex_lock(&g_net_mutex);

    SOCKET *k = sock_get(s);
    UINT32 ev = 0;

    if (k == NULL)
        ev = MYOS_POLLNVAL;
    else if (k->type != MYOS_SOCK_STREAM) {
        if (k->qn > 0)
            ev |= MYOS_POLLIN;
        ev |= MYOS_POLLOUT;
    } else if (k->tcb == NULL) {
        ev = MYOS_POLLHUP;
    } else if (k->tcb->state == TCP_LISTEN) {
        for (UINTN i = 0; i < TCB_MAX; i++) {
            TCB *t = &g_tcb[i];
            if (t->used && t->listener == k->tcb && !t->accepted &&
                (t->state == TCP_ESTABLISHED || t->state == TCP_CLOSE_WAIT)) {
                ev |= MYOS_POLLIN;
                break;
            }
        }
    } else {
        TCB *t = k->tcb;
        if (t->rbuf_len > 0 || t->rcv_fin)
            ev |= MYOS_POLLIN;
        if ((t->state == TCP_ESTABLISHED || t->state == TCP_CLOSE_WAIT) &&
            t->sbuf_len < TCP_SBUF)
            ev |= MYOS_POLLOUT;
        if (t->state == TCP_CLOSED) {
            ev |= MYOS_POLLIN | MYOS_POLLHUP;
            if (t->error || !k->connected)
                ev |= MYOS_POLLERR | MYOS_POLLOUT;
        }
    }

    kmutex_unlock(&g_net_mutex);

    return ev;
}

/*
 * Ожидание для poll: check (проверка всех fd программы) - под замком
 * сети, и сон - сразу после неё, не отпуская замка до net_wait: так
 * событие, случившееся между проверкой и сном, не потеряется (его
 * net_wake придёт, когда мы уже ждём). TRUE - check сказал "готово".
 */
BOOLEAN sock_poll_wait(BOOLEAN (*check)(void *ctx), void *ctx, UINT64 slice_ms)
{
    kmutex_lock(&g_net_mutex);

    BOOLEAN ready = check(ctx);

    if (!ready)
        net_wait(&g_net_any_event, "poll", slice_ms);

    kmutex_unlock(&g_net_mutex);

    return ready;
}

/* Сколько байт можно прочитать без ожидания; <0 - ошибка/закрыт */
INTN sock_pending(INTN s)
{
    kmutex_lock(&g_net_mutex);

    SOCKET *k = sock_get(s);
    INTN r;

    if (k == NULL)
        r = MYOS_EBADF;
    else if (k->type == MYOS_SOCK_STREAM)
        r = (k->tcb == NULL) ? MYOS_ENOTCONN : (INTN)k->tcb->rbuf_len;
    else
        r = k->qn ? k->q[k->qh].len : 0;

    kmutex_unlock(&g_net_mutex);

    return r;
}

INTN sock_close(INTN s)
{
    kmutex_lock(&g_net_mutex);

    SOCKET *k = sock_get(s);

    if (k == NULL) {
        kmutex_unlock(&g_net_mutex);
        return MYOS_EBADF;
    }

    if (k->tcb != NULL) {
        if (k->tcb->state == TCP_CLOSED)
            tcb_free(k->tcb);
        else
            tcp_close(k->tcb);
        k->tcb = NULL;
    }

    dq_clear(k);
    k->used = FALSE;

    net_wake(k);

    kmutex_unlock(&g_net_mutex);

    return 0;
}

/* Программа завершилась - закрыть её сокеты, о которых она забыла */
void sock_close_pid(UINT32 pid)
{
    for (UINTN i = 0; i < SOCK_MAX; i++)
        if (g_socks[i].used && g_socks[i].pid == pid && pid != 0)
            sock_close((INTN)i);
}

const char *net_strerror(INTN e)
{
    switch (e) {
    case MYOS_ETIMEDOUT:     return "timed out (no answer)";
    case MYOS_ECONNREFUSED:  return "connection refused (nobody listens on that port)";
    case MYOS_ECONNRESET:    return "connection reset by the other side";
    case MYOS_ENETUNREACH:   return "network unreachable (no address or no gateway yet - see ifconfig)";
    case MYOS_EADDRINUSE:    return "port already in use";
    case MYOS_ENOTCONN:      return "not connected";
    case MYOS_EHOSTNOTFOUND: return "host name not found (DNS)";
    case MYOS_EAGAIN:        return "nothing arrived in time";
    case MYOS_EINTR:         return "interrupted";
    case MYOS_EMFILE:        return "too many sockets";
    case MYOS_EINVAL:        return "invalid argument";
    case MYOS_EBADF:         return "bad socket";
    default:                 return vfs_strerror(e);
    }
}

void sock_print(SIMPLE_TEXT_OUTPUT_INTERFACE *out)
{
    UINTN n = 0;

    print(out, "Sockets:\n");

    for (UINTN i = 0; i < SOCK_MAX; i++) {

        SOCKET *k = &g_socks[i];

        if (!k->used)
            continue;

        char r[16];
        net_fmt_ip(r, sizeof(r), k->rip);

        kprintf(out, "  #%u %s  pid %u  port %u%s%s:%u  queued %u\n", (UINT32)i,
                k->type == MYOS_SOCK_STREAM ? "TCP " : k->type == MYOS_SOCK_DGRAM ? "UDP " : "PING",
                k->pid, k->lport, k->connected ? " -> " : "  ", k->connected ? r : "",
                k->rport, (UINT32)k->qn);
        n++;
    }

    if (n == 0)
        print(out, "  (none)\n");
}
