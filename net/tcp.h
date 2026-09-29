/*
 * net/tcp.h - соединение TCP (TCB, "transmission control block") -
 * общее для net/tcp.c и net/socket.c. Часть MyOS.
 */
#ifndef MYOS_TCP_H
#define MYOS_TCP_H

#include "net.h"

#define TCB_MAX            32
#define TCP_SBUF           (32u * 1024u)     /* буфер отправки */
#define TCP_RBUF           (32u * 1024u)     /* буфер приёма */

#define TCP_RTO_INIT       1000u    /* мс - до первого замера (RFC 6298) */
#define TCP_RTO_MIN        200u
#define TCP_RTO_MAX        60000u
#define TCP_MAX_RETRIES    8        /* ~ 2-4 минуты тишины - обрыв */
#define TCP_SYN_RETRIES    5        /* 1+2+4+8+16 = 31 с на соединение */
#define TCP_TIME_WAIT_MS   2000u    /* по правилам 2 MSL = минуты; нам хватит */
#define TCP_ORPHAN_MS      60000u
#define TCP_OOO_MAX        32       /* сегментов "не по порядку" в ожидании */

#define TCP_FIN   0x01
#define TCP_SYN   0x02
#define TCP_RST   0x04
#define TCP_PSH   0x08
#define TCP_ACK   0x10

#define TCP_CLOSED       0
#define TCP_LISTEN       1
#define TCP_SYN_SENT     2
#define TCP_SYN_RCVD     3
#define TCP_ESTABLISHED  4
#define TCP_FIN_WAIT_1   5
#define TCP_FIN_WAIT_2   6
#define TCP_CLOSE_WAIT   7
#define TCP_CLOSING      8
#define TCP_LAST_ACK     9
#define TCP_TIME_WAIT    10

struct SOCKET;

/* Сегмент, пришедший раньше предыдущих (что-то потерялось по
   дороге): ждёт, пока недостающее придёт повтором */
typedef struct {
    UINT32  seq;
    UINT16  len;
    BOOLEAN fin;
    UINT8  *data;
} TCP_OOO;

struct TCB {
    BOOLEAN used;
    UINT8   state;
    UINT32  lip, rip;
    UINT16  lport, rport;

    /* отправка: байты от snd_una до snd_nxt - в пути, дальше - ждут
       окна. Буфер-кольцо: sbuf_len байт, начиная с номера sbuf_seq */
    UINT32  iss, snd_una, snd_nxt, snd_wnd, snd_wl1, snd_wl2;
    UINT8  *sbuf;
    UINT32  sbuf_start, sbuf_len, sbuf_seq;
    BOOLEAN fin_pending;      /* программа закрыла - отправить FIN */
    BOOLEAN fin_sent;
    UINT32  fin_seq;

    /* приём: кольцо с непрочитанным программой */
    UINT32  irs, rcv_nxt;
    UINT8  *rbuf;
    UINT32  rbuf_start, rbuf_len;
    BOOLEAN rcv_fin;          /* собеседник закрыл свою сторону */
    UINT32  last_adv_wnd;     /* какое окно мы объявили последним */
    UINT32  last_adv_ack;     /* ... и при каком rcv_nxt */
    BOOLEAN force_wnd;        /* сообщить новое окно, даже без новых данных */
    BOOLEAN ack_now;

    UINT16  mss;              /* сегмент собеседника */

    TCP_OOO ooo[TCP_OOO_MAX];
    UINT32  ooo_n, ooo_bytes;

    /* таймеры и замер времени ответа */
    UINT64  rto_ms;
    UINT64  rtx_at;           /* когда повторять (0 - не надо) */
    UINT32  retries;
    UINT32  srtt, rttvar;
    BOOLEAN rtt_active;
    UINT32  rtt_seq;
    UINT64  rtt_start;
    UINT64  opened_ms;
    UINT64  timewait_at;
    UINT64  last_ms;          /* последняя активность */

    /* перегрузка */
    UINT32  cwnd, ssthresh, dupacks;

    INTN    error;            /* MYOS_ECONNREFUSED / ECONNRESET / ETIMEDOUT */
    struct SOCKET *sock;      /* чей (NULL - программа закрыла) */
    TCB    *listener;         /* пришло на слушающий сокет */
    BOOLEAN accepted;         /* ... и его уже забрал accept */
    UINT32  backlog;          /* для LISTEN */

    UINT64  bytes_in, bytes_acked, segs_out;
};

extern TCB g_tcb[TCB_MAX];

TCB *tcb_alloc(void);
void tcb_free(TCB *t);
TCB *tcp_connect(UINT32 lip, UINT16 lport, UINT32 rip, UINT16 rport);
TCB *tcp_listen(UINT32 lip, UINT16 lport, UINT32 backlog);
void tcp_close(TCB *t);
void tcp_abort(TCB *t);
void tcp_output(TCB *t);
void tcp_window_update(TCB *t);
const char *tcp_state_name(UINT8 s);

#endif
