/*
 * net/tcp.c - TCP: надёжный поток байтов поверх IP. Часть MyOS;
 * общие объявления - в net/net.h.
 *
 * IP может терять, дублировать и переставлять пакеты. TCP делает
 * из них "трубу": каждый байт потока имеет номер (sequence number),
 * получатель подтверждает (ACK), до какого номера всё получил,
 * отправитель повторяет неподтверждённое по таймеру. Соединение
 * открывается "тройным рукопожатием" (SYN, SYN+ACK, ACK) и
 * закрывается FIN с каждой стороны.
 *
 * Что здесь есть (по RFC 793 / 1122 / 5681 / 6298):
 *   * состояния: LISTEN, SYN_SENT, SYN_RCVD, ESTABLISHED,
 *     FIN_WAIT_1/2, CLOSE_WAIT, CLOSING, LAST_ACK, TIME_WAIT;
 *   * буферы отправки и приёма по 32 КиБ; окно приёма = свободное
 *     место в буфере;
 *   * повтор по таймеру (RTO по замерам времени ответа, алгоритм
 *     Карна), быстрый повтор после трёх одинаковых ACK;
 *   * управление перегрузкой: медленный старт и "избегание
 *     перегрузки" (окно cwnd), чтобы не завалить медленную сеть;
 *   * опция MSS (размер сегмента) в SYN;
 *   * пробы нулевого окна (получатель сказал "места нет").
 * Чего нет: пришедшие не по порядку сегменты не хранятся (их
 * повторит отправитель), нет масштабирования окна и SACK - для
 * скачивания страниц и файлов этого хватает.
 */
#include "net.h"
#include "tcp.h"

TCB g_tcb[TCB_MAX];

static UINT64 g_tcp_in = 0, g_tcp_bad = 0, g_tcp_rst_sent = 0, g_tcp_retrans = 0;

/* Сравнения номеров по кругу (номера 32-битные и переполняются) */
#define SEQ_LT(a, b)  ((INT32)((a) - (b)) < 0)
#define SEQ_LE(a, b)  ((INT32)((a) - (b)) <= 0)
#define SEQ_GT(a, b)  ((INT32)((a) - (b)) > 0)
#define SEQ_GE(a, b)  ((INT32)((a) - (b)) >= 0)

const char *tcp_state_name(UINT8 s)
{
    static const char *n[] = {
        "CLOSED", "LISTEN", "SYN_SENT", "SYN_RCVD", "ESTABLISHED",
        "FIN_WAIT_1", "FIN_WAIT_2", "CLOSE_WAIT", "CLOSING", "LAST_ACK", "TIME_WAIT"
    };

    return (s < sizeof(n) / sizeof(n[0])) ? n[s] : "?";
}

/* ================================================================
 * Буферы-кольца
 * ================================================================ */

static void ring_write(UINT8 *buf, UINT32 size, UINT32 start, UINT32 len,
                       UINT32 off, const UINT8 *src, UINT32 n)
{
    (void)len;
    for (UINT32 i = 0; i < n; i++)
        buf[(start + off + i) % size] = src[i];
}

static void ring_read(const UINT8 *buf, UINT32 size, UINT32 start, UINT32 off,
                      UINT8 *dst, UINT32 n)
{
    for (UINT32 i = 0; i < n; i++)
        dst[i] = buf[(start + off + i) % size];
}

/* ================================================================
 * Создание / удаление
 * ================================================================ */

TCB *tcb_alloc(void)
{
    for (UINTN i = 0; i < TCB_MAX; i++) {

        TCB *t = &g_tcb[i];

        if (t->used)
            continue;

        UINT8 *sb = (UINT8 *)kmalloc(TCP_SBUF);
        UINT8 *rb = (UINT8 *)kmalloc(TCP_RBUF);

        if (sb == NULL || rb == NULL) {
            if (sb) kfree(sb);
            if (rb) kfree(rb);
            return NULL;
        }

        memset(t, 0, sizeof(*t));
        t->used = TRUE;
        t->sbuf = sb;
        t->rbuf = rb;
        t->mss = 536;                    /* по умолчанию (RFC 1122) */
        t->rto_ms = TCP_RTO_INIT;
        t->cwnd = 2u * 1460u;
        t->ssthresh = 0xFFFFFFFFu;
        t->last_ms = net_now_ms();

        return t;
    }

    return NULL;
}

void tcb_free(TCB *t)
{
    if (!t->used)
        return;

    if (t->sbuf) kfree(t->sbuf);
    if (t->rbuf) kfree(t->rbuf);

    /* ждущий в accept потомок слушающего сокета - освободить тоже */
    for (UINTN i = 0; i < TCB_MAX; i++)
        if (g_tcb[i].used && g_tcb[i].listener == t)
            g_tcb[i].listener = NULL;

    t->sbuf = t->rbuf = NULL;
    t->used = FALSE;
    t->state = TCP_CLOSED;
}

/* Соединение закончилось (сброс, таймаут, закрыто): сообщить
   владельцу; если владельца нет - освободить */
static void tcb_finish(TCB *t, INTN err)
{
    t->state = TCP_CLOSED;

    if (err != 0 && t->error == 0)
        t->error = err;

    if (t->sock != NULL)
        net_wake(t->sock);    /* сокет увидит CLOSED/ошибку, освободит close */
    else
        tcb_free(t);
}

/* Есть ли кому читать принятое: сокет программы - или соединение
   ещё ждёт accept (клиент часто шлёт запрос сразу после SYN) */
static BOOLEAN tcb_has_reader(TCB *t)
{
    return t->sock != NULL || (t->listener != NULL && !t->accepted);
}

/* ================================================================
 * Отправка сегментов
 * ================================================================ */

static UINT16 tcp_window(TCB *t)
{
    UINT32 free = TCP_RBUF - t->rbuf_len;

    return (free > 65535u) ? 65535u : (UINT16)free;
}

/* Отправить один сегмент: флаги, номер seq, данные из буфера
   отправки (off - смещение в нём, n байт) */
static void tcp_send_seg(TCB *t, UINT8 flags, UINT32 seq, UINT32 off, UINT32 n)
{
    static UINT8 seg[TCP_HLEN + 4 + 1460];

    UINTN hl = TCP_HLEN;

    if (n > 1460u)
        n = 1460u;

    net_put16(seg + 0, t->lport);
    net_put16(seg + 2, t->rport);
    net_put32(seg + 4, seq);
    net_put32(seg + 8, (flags & TCP_ACK) ? t->rcv_nxt : 0);

    if (flags & TCP_SYN) {
        /* опция MSS: сколько данных мы готовы получать в сегменте */
        seg[20] = 2;
        seg[21] = 4;
        net_put16(seg + 22, 1460);
        hl += 4;
    }

    seg[12] = (UINT8)((hl / 4u) << 4);
    seg[13] = flags;
    net_put16(seg + 14, tcp_window(t));
    net_put16(seg + 16, 0);
    net_put16(seg + 18, 0);

    if (n > 0)
        ring_read(t->sbuf, TCP_SBUF, t->sbuf_start, off, seg + hl, n);

    UINTN total = hl + n;

    net_put16(seg + 16, net_csum(seg, total, ip_pseudo_sum(t->lip, t->rip, IP_PROTO_TCP, total)));

    ip_send(t->lip, t->rip, IP_PROTO_TCP, seg, total);

    t->last_adv_wnd = tcp_window(t);
    t->ack_now = FALSE;
    t->segs_out++;
}

/* Сброс (RST) в ответ на сегмент, которому нет соединения */
static void tcp_send_rst(UINT32 src, UINT32 dst, UINT16 sport, UINT16 dport,
                         UINT32 seq, UINT32 ack, BOOLEAN with_ack)
{
    UINT8 seg[TCP_HLEN];

    memset(seg, 0, sizeof(seg));
    net_put16(seg + 0, dport);
    net_put16(seg + 2, sport);
    net_put32(seg + 4, seq);
    net_put32(seg + 8, ack);
    seg[12] = 5u << 4;
    seg[13] = TCP_RST | (with_ack ? TCP_ACK : 0);
    net_put16(seg + 16, net_csum(seg, TCP_HLEN, ip_pseudo_sum(dst, src, IP_PROTO_TCP, TCP_HLEN)));

    ip_send(dst, src, IP_PROTO_TCP, seg, TCP_HLEN);
    g_tcp_rst_sent++;
}

static void tcp_arm_timer(TCB *t)
{
    t->rtx_at = net_now_ms() + t->rto_ms;
}

/*
 * Главная функция отправки: всё, что можно отправить сейчас
 * (данные в пределах окна получателя и окна перегрузки, FIN), и
 * ACK, если его надо отправить.
 */
void tcp_output(TCB *t)
{
    if (t->state == TCP_SYN_SENT || t->state == TCP_SYN_RCVD) {
        if (t->ack_now && t->state == TCP_SYN_RCVD)
            tcp_send_seg(t, TCP_SYN | TCP_ACK, t->iss, 0, 0);
        return;
    }

    if (t->state != TCP_ESTABLISHED && t->state != TCP_CLOSE_WAIT &&
        t->state != TCP_FIN_WAIT_1 && t->state != TCP_CLOSING &&
        t->state != TCP_LAST_ACK && t->state != TCP_FIN_WAIT_2 &&
        t->state != TCP_TIME_WAIT)
        return;

    BOOLEAN sent = FALSE;
    UINT32 wnd = (t->snd_wnd < t->cwnd) ? t->snd_wnd : t->cwnd;

    for (UINTN guard = 0; guard < 64; guard++) {

        UINT32 off = t->snd_nxt - t->sbuf_seq;      /* уже отправлено из буфера */

        if (off > t->sbuf_len)
            break;                                  /* (FIN уже ушёл) */

        UINT32 avail = t->sbuf_len - off;
        UINT32 flight = t->snd_nxt - t->snd_una;

        if (avail == 0 || flight >= wnd)
            break;

        UINT32 n = wnd - flight;

        if (n > avail) n = avail;
        if (n > t->mss) n = t->mss;

        /* не посылать крошечные куски, пока предыдущие в пути
           (алгоритм Нейгла) - кроме последнего куска данных */
        if (n < t->mss && n < avail && flight > 0)
            break;

        UINT8 fl = TCP_ACK;

        if (n == avail)
            fl |= TCP_PSH;

        if (!t->rtt_active) {
            t->rtt_active = TRUE;
            t->rtt_seq = t->snd_nxt + n;
            t->rtt_start = net_now_ms();
        }

        tcp_send_seg(t, fl, t->snd_nxt, off, n);
        t->snd_nxt += n;
        sent = TRUE;

        if (t->rtx_at == 0)
            tcp_arm_timer(t);
    }

    /* FIN - когда программа закрыла сокет и все данные ушли */
    if (t->fin_pending && !t->fin_sent &&
        t->snd_nxt == t->sbuf_seq + t->sbuf_len &&
        (t->state == TCP_ESTABLISHED || t->state == TCP_CLOSE_WAIT)) {

        tcp_send_seg(t, TCP_FIN | TCP_ACK, t->snd_nxt, 0, 0);
        t->fin_seq = t->snd_nxt;
        t->snd_nxt++;
        t->fin_sent = TRUE;
        sent = TRUE;

        t->state = (t->state == TCP_ESTABLISHED) ? TCP_FIN_WAIT_1 : TCP_LAST_ACK;

        if (t->rtx_at == 0)
            tcp_arm_timer(t);
    }

    if (!sent && t->ack_now)
        tcp_send_seg(t, TCP_ACK, t->snd_nxt, 0, 0);
}

/* ================================================================
 * Активное открытие (connect) и слушание (listen) - зовёт socket.c
 * ================================================================ */

TCB *tcp_connect(UINT32 lip, UINT16 lport, UINT32 rip, UINT16 rport)
{
    TCB *t = tcb_alloc();

    if (t == NULL)
        return NULL;

    t->lip = lip;
    t->lport = lport;
    t->rip = rip;
    t->rport = rport;

    /* начальный номер - непредсказуемый (RFC 6528): от TSC */
    t->iss = net_random();
    t->snd_una = t->iss;
    t->snd_nxt = t->iss + 1u;
    t->sbuf_seq = t->iss + 1u;
    t->snd_wnd = 1460;
    t->state = TCP_SYN_SENT;
    t->opened_ms = net_now_ms();

    tcp_send_seg(t, TCP_SYN, t->iss, 0, 0);
    tcp_arm_timer(t);

    return t;
}

TCB *tcp_listen(UINT32 lip, UINT16 lport, UINT32 backlog)
{
    TCB *t = tcb_alloc();

    if (t == NULL)
        return NULL;

    /* слушающему буферы не нужны */
    kfree(t->sbuf);
    kfree(t->rbuf);
    t->sbuf = t->rbuf = NULL;

    t->lip = lip;
    t->lport = lport;
    t->backlog = backlog ? backlog : 4;
    t->state = TCP_LISTEN;

    return t;
}

/* Программа закрыла сокет */
void tcp_close(TCB *t)
{
    t->sock = NULL;

    switch (t->state) {

    case TCP_LISTEN:
        /* ждущие accept соединения - сбросить */
        for (UINTN i = 0; i < TCB_MAX; i++) {
            TCB *c = &g_tcb[i];
            if (c->used && c->listener == t && !c->accepted) {
                tcp_send_rst(c->rip, c->lip, c->rport, c->lport, c->snd_nxt, 0, FALSE);
                tcb_free(c);
            }
        }
        tcb_free(t);
        return;

    case TCP_SYN_SENT:
    case TCP_CLOSED:
        tcb_free(t);
        return;

    case TCP_SYN_RCVD:
    case TCP_ESTABLISHED:
    case TCP_CLOSE_WAIT:
        /* непрочитанное выбрасываем; FIN уйдёт после данных */
        t->rbuf_len = 0;
        t->fin_pending = TRUE;
        t->last_ms = net_now_ms();
        tcp_output(t);
        return;

    default:
        /* FIN уже отправлен - соединение доживёт само */
        return;
    }
}

/* Жёсткий сброс (закрыли программу посреди передачи) */
void tcp_abort(TCB *t)
{
    if (t->state != TCP_LISTEN && t->state != TCP_CLOSED && t->state != TCP_TIME_WAIT &&
        t->state != TCP_SYN_SENT)
        tcp_send_rst(t->rip, t->lip, t->rport, t->lport, t->snd_nxt, t->rcv_nxt, TRUE);

    t->sock = NULL;

    if (t->state == TCP_LISTEN)
        tcp_close(t);
    else
        tcb_free(t);
}

/* Программа прочитала данные: окно открылось - сказать отправителю */
void tcp_window_update(TCB *t)
{
    UINT32 now_wnd = tcp_window(t);

    if (t->last_adv_wnd < TCP_RBUF / 2u && now_wnd >= t->last_adv_wnd + t->mss &&
        (t->state == TCP_ESTABLISHED || t->state == TCP_FIN_WAIT_1 ||
         t->state == TCP_FIN_WAIT_2)) {
        t->ack_now = TRUE;
        tcp_output(t);
    }
}

/* ================================================================
 * Приём
 * ================================================================ */

static TCB *tcp_find(UINT32 src, UINT16 sport, UINT32 dst, UINT16 dport)
{
    for (UINTN i = 0; i < TCB_MAX; i++) {
        TCB *t = &g_tcb[i];
        if (t->used && t->state != TCP_LISTEN && t->lport == dport && t->rport == sport &&
            t->rip == src && t->lip == dst)
            return t;
    }

    for (UINTN i = 0; i < TCB_MAX; i++) {
        TCB *t = &g_tcb[i];
        if (t->used && t->state == TCP_LISTEN && t->lport == dport &&
            (t->lip == 0 || t->lip == dst))
            return t;
    }

    return NULL;
}

static UINT16 tcp_parse_mss(const UINT8 *p, UINTN hl)
{
    UINTN i = TCP_HLEN;

    while (i < hl) {

        UINT8 k = p[i];

        if (k == 0)
            break;

        if (k == 1) {
            i++;
            continue;
        }

        if (i + 1 >= hl)
            break;

        UINT8 l = p[i + 1];

        if (l < 2 || i + l > hl)
            break;

        if (k == 2 && l == 4)
            return net_get16(p + i + 2);

        i += l;
    }

    return 536;
}

/* Время ответа (RFC 6298): сглаженное srtt и разброс rttvar */
static void tcp_rtt_sample(TCB *t, UINT32 ms)
{
    if (ms == 0)
        ms = 1;

    if (t->srtt == 0) {
        t->srtt = ms;
        t->rttvar = ms / 2u;
    } else {
        UINT32 d = (t->srtt > ms) ? t->srtt - ms : ms - t->srtt;
        t->rttvar = (3u * t->rttvar + d) / 4u;
        t->srtt = (7u * t->srtt + ms) / 8u;
    }

    UINT64 rto = t->srtt + ((4u * t->rttvar > 10u) ? 4u * t->rttvar : 10u);

    if (rto < TCP_RTO_MIN) rto = TCP_RTO_MIN;
    if (rto > TCP_RTO_MAX) rto = TCP_RTO_MAX;

    t->rto_ms = rto;
}

/* Пришёл ACK: выбросить подтверждённое, подвинуть окна */
static void tcp_ack(TCB *t, UINT32 ack, UINT32 win, UINT32 seq)
{
    if (SEQ_GT(ack, t->snd_una) && SEQ_LE(ack, t->snd_nxt)) {

        UINT32 acked = ack - t->snd_una;

        /* подтверждённые байты данных уходят из буфера */
        UINT32 data_acked = 0;
        if (SEQ_GT(ack, t->sbuf_seq)) {
            data_acked = ack - t->sbuf_seq;
            if (data_acked > t->sbuf_len)
                data_acked = t->sbuf_len;       /* остальное - FIN */
            t->sbuf_start = (t->sbuf_start + data_acked) % TCP_SBUF;
            t->sbuf_len -= data_acked;
            t->sbuf_seq += data_acked;
        }

        t->snd_una = ack;
        t->dupacks = 0;
        t->retries = 0;

        if (t->rtt_active && SEQ_GE(ack, t->rtt_seq)) {
            t->rtt_active = FALSE;
            tcp_rtt_sample(t, (UINT32)(net_now_ms() - t->rtt_start));
        }

        /* окно перегрузки: медленный старт - растёт на каждый ACK,
           потом - примерно на MSS за "круг" */
        if (t->cwnd < t->ssthresh)
            t->cwnd += (acked < t->mss) ? acked : t->mss;
        else
            t->cwnd += (t->mss * t->mss) / (t->cwnd ? t->cwnd : 1u) + 1u;

        if (t->cwnd > 256u * 1024u)
            t->cwnd = 256u * 1024u;

        /* всё подтверждено - таймер стоп, иначе - заново */
        if (t->snd_una == t->snd_nxt)
            t->rtx_at = 0;
        else
            tcp_arm_timer(t);

        t->bytes_acked += data_acked;

        if (t->sock != NULL)
            net_wake(t->sock);           /* место в буфере отправки */

    } else if (ack == t->snd_una && t->snd_nxt != t->snd_una && win == t->snd_wnd) {

        /* тот же ACK ещё раз: похоже, сегмент потерялся */
        if (++t->dupacks == 3) {
            UINT32 flight = t->snd_nxt - t->snd_una;
            t->ssthresh = (flight / 2u > 2u * t->mss) ? flight / 2u : 2u * t->mss;
            t->cwnd = t->ssthresh;
            t->rtt_active = FALSE;
            g_tcp_retrans++;

            UINT32 off = t->snd_una - t->sbuf_seq;
            if (SEQ_GE(t->snd_una, t->sbuf_seq) && off < t->sbuf_len) {
                UINT32 n = t->sbuf_len - off;
                if (n > t->mss) n = t->mss;
                tcp_send_seg(t, TCP_ACK, t->snd_una, off, n);
            }
        }
    }

    /* окно получателя (RFC 793: обновлять только по свежим сегментам) */
    if (SEQ_LT(t->snd_wl1, seq) || (t->snd_wl1 == seq && SEQ_LE(t->snd_wl2, ack))) {
        t->snd_wnd = win;
        t->snd_wl1 = seq;
        t->snd_wl2 = ack;
    }
}

void tcp_input(NETIF *nif, UINT32 src, UINT32 dst, const UINT8 *p, UINTN len)
{
    (void)nif;

    if (len < TCP_HLEN) {
        g_tcp_bad++;
        return;
    }

    UINTN hl = (UINTN)(p[12] >> 4) * 4u;

    if (hl < TCP_HLEN || hl > len ||
        net_csum(p, len, ip_pseudo_sum(src, dst, IP_PROTO_TCP, len)) != 0) {
        g_tcp_bad++;
        return;
    }

    g_tcp_in++;

    UINT16 sport = net_get16(p);
    UINT16 dport = net_get16(p + 2);
    UINT32 seq = net_get32(p + 4);
    UINT32 ack = net_get32(p + 8);
    UINT8 fl = p[13];
    UINT32 win = net_get16(p + 14);
    const UINT8 *data = p + hl;
    UINT32 dlen = (UINT32)(len - hl);

    TCB *t = tcp_find(src, sport, dst, dport);

    /* --- нет такого соединения: RST (на RST не отвечаем) --- */
    if (t == NULL) {
        if (!(fl & TCP_RST)) {
            if (fl & TCP_ACK)
                tcp_send_rst(src, dst, sport, dport, ack, 0, FALSE);
            else
                tcp_send_rst(src, dst, sport, dport, 0,
                             seq + dlen + ((fl & TCP_SYN) ? 1u : 0u) + ((fl & TCP_FIN) ? 1u : 0u),
                             TRUE);
        }
        return;
    }

    t->last_ms = net_now_ms();

    /* --- слушающий сокет: новое входящее соединение --- */
    if (t->state == TCP_LISTEN) {

        if (fl & TCP_RST)
            return;

        if (fl & TCP_ACK) {
            tcp_send_rst(src, dst, sport, dport, ack, 0, FALSE);
            return;
        }

        if (!(fl & TCP_SYN))
            return;

        /* очередь accept полна - молча игнорируем (клиент повторит) */
        UINT32 pending = 0;
        for (UINTN i = 0; i < TCB_MAX; i++)
            if (g_tcb[i].used && g_tcb[i].listener == t && !g_tcb[i].accepted)
                pending++;
        if (pending >= t->backlog)
            return;

        TCB *c = tcb_alloc();

        if (c == NULL)
            return;

        c->lip = dst;
        c->lport = dport;
        c->rip = src;
        c->rport = sport;
        c->listener = t;
        c->irs = seq;
        c->rcv_nxt = seq + 1u;
        c->iss = net_random();
        c->snd_una = c->iss;
        c->snd_nxt = c->iss + 1u;
        c->sbuf_seq = c->iss + 1u;
        c->snd_wnd = win;
        c->snd_wl1 = seq;
        c->snd_wl2 = 0;
        c->mss = tcp_parse_mss(p, hl);
        if (c->mss > 1460u) c->mss = 1460u;
        c->cwnd = 2u * c->mss;
        c->state = TCP_SYN_RCVD;

        tcp_send_seg(c, TCP_SYN | TCP_ACK, c->iss, 0, 0);
        tcp_arm_timer(c);
        return;
    }

    /* --- мы отправили SYN, ждём SYN+ACK --- */
    if (t->state == TCP_SYN_SENT) {

        BOOLEAN ack_ok = (fl & TCP_ACK) && ack == t->iss + 1u;

        if ((fl & TCP_ACK) && !ack_ok) {
            if (!(fl & TCP_RST))
                tcp_send_rst(src, dst, sport, dport, ack, 0, FALSE);
            return;
        }

        if (fl & TCP_RST) {
            if (ack_ok)
                tcb_finish(t, MYOS_ECONNREFUSED);     /* порт закрыт */
            return;
        }

        if (!(fl & TCP_SYN))
            return;

        t->irs = seq;
        t->rcv_nxt = seq + 1u;
        t->mss = tcp_parse_mss(p, hl);
        if (t->mss > 1460u) t->mss = 1460u;
        t->cwnd = 2u * t->mss;
        t->snd_wnd = win;
        t->snd_wl1 = seq;
        t->snd_wl2 = ack;

        if (ack_ok) {
            /* первый замер времени ответа - если SYN не повторялся */
            if (t->retries == 0)
                tcp_rtt_sample(t, (UINT32)(net_now_ms() - t->opened_ms));
            t->snd_una = ack;
            t->rtx_at = 0;
            t->retries = 0;
            t->state = TCP_ESTABLISHED;
            t->ack_now = TRUE;
            tcp_output(t);
            if (t->sock != NULL)
                net_wake(t->sock);
        } else {
            /* одновременное открытие (редкость) */
            t->state = TCP_SYN_RCVD;
            t->ack_now = TRUE;
            tcp_output(t);
        }
        return;
    }

    /* --- все остальные состояния --- */

    /* сегмент в окне приёма? (RFC 793, упрощённо: он должен
       начинаться не позже rcv_nxt и хоть что-то приносить) */
    UINT32 seg_len = dlen + ((fl & TCP_SYN) ? 1u : 0u) + ((fl & TCP_FIN) ? 1u : 0u);
    BOOLEAN acceptable;

    if (seg_len == 0)
        acceptable = SEQ_GE(seq, t->rcv_nxt) && SEQ_LE(seq, t->rcv_nxt + TCP_RBUF);
    else
        acceptable = SEQ_LE(seq, t->rcv_nxt) && SEQ_GT(seq + seg_len, t->rcv_nxt);

    if (!acceptable) {
        /* старый повтор или сегмент "из будущего" (что-то потерялось
           раньше) - напомнить, что нам нужно: ACK rcv_nxt */
        if (!(fl & TCP_RST)) {
            t->ack_now = TRUE;
            tcp_output(t);
        }
        return;
    }

    if (fl & TCP_RST) {
        if (t->state == TCP_SYN_RCVD && t->listener != NULL) {
            tcb_free(t);               /* клиент передумал */
            return;
        }
        tcb_finish(t, MYOS_ECONNRESET);
        return;
    }

    if (fl & TCP_SYN) {
        /* SYN посреди соединения: SYN+ACK потерялся и клиент
           повторил - повторим и мы; иначе просто напомнить ACK */
        if (t->state == TCP_SYN_RCVD && seq == t->irs) {
            t->ack_now = TRUE;
            tcp_output(t);
        } else {
            t->ack_now = TRUE;
            tcp_output(t);
        }
        return;
    }

    if (!(fl & TCP_ACK))
        return;

    /* --- ACK --- */
    if (t->state == TCP_SYN_RCVD) {

        if (SEQ_GT(ack, t->snd_una) && SEQ_LE(ack, t->snd_nxt)) {
            t->snd_una = ack;
            t->snd_wnd = win;
            t->snd_wl1 = seq;
            t->snd_wl2 = ack;
            t->rtx_at = 0;
            t->retries = 0;
            t->state = TCP_ESTABLISHED;
            /* готово к accept */
            if (t->listener != NULL && t->listener->sock != NULL)
                net_wake(t->listener->sock);
        } else {
            tcp_send_rst(src, dst, sport, dport, ack, 0, FALSE);
            return;
        }
    } else {
        if (SEQ_GT(ack, t->snd_nxt)) {
            /* подтверждают то, чего мы не посылали */
            t->ack_now = TRUE;
            tcp_output(t);
            return;
        }
        tcp_ack(t, ack, win, seq);
    }

    /* наш FIN подтверждён? */
    if (t->fin_sent && SEQ_GE(t->snd_una, t->fin_seq + 1u)) {
        if (t->state == TCP_FIN_WAIT_1) {
            t->state = TCP_FIN_WAIT_2;
        } else if (t->state == TCP_CLOSING) {
            t->state = TCP_TIME_WAIT;
            t->timewait_at = net_now_ms() + TCP_TIME_WAIT_MS;
        } else if (t->state == TCP_LAST_ACK) {
            tcb_finish(t, 0);
            return;
        }
    }

    /* --- данные --- */
    if (dlen > 0 && (t->state == TCP_ESTABLISHED || t->state == TCP_FIN_WAIT_1 ||
                     t->state == TCP_FIN_WAIT_2)) {

        /* начало могло быть уже получено (повтор с перекрытием) */
        UINT32 skip = t->rcv_nxt - seq;

        if (skip < dlen) {

            UINT32 n = dlen - skip;
            UINT32 room = TCP_RBUF - t->rbuf_len;

            if (n > room)
                n = room;           /* остальное отправитель повторит */

            if (!tcb_has_reader(t)) {
                /* программа уже закрыла сокет - принимаем "в никуда" */
                t->rcv_nxt += n;
            } else if (n > 0) {
                ring_write(t->rbuf, TCP_RBUF, t->rbuf_start, t->rbuf_len, t->rbuf_len,
                           data + skip, n);
                t->rbuf_len += n;
                t->rcv_nxt += n;
                t->bytes_in += n;
                if (t->sock != NULL)
                    net_wake(t->sock);
            }

            /* FIN считается, только если все данные до него приняты */
            if (n < dlen - skip)
                fl &= (UINT8)~TCP_FIN;
        }

        t->ack_now = TRUE;
    }

    /* --- FIN от собеседника: он больше ничего не пришлёт --- */
    if ((fl & TCP_FIN) && seq + dlen == t->rcv_nxt) {

        t->rcv_nxt++;
        t->rcv_fin = TRUE;
        t->ack_now = TRUE;

        if (t->state == TCP_ESTABLISHED || t->state == TCP_SYN_RCVD) {
            t->state = TCP_CLOSE_WAIT;
        } else if (t->state == TCP_FIN_WAIT_1) {
            t->state = TCP_CLOSING;
            if (t->fin_sent && SEQ_GE(t->snd_una, t->fin_seq + 1u)) {
                t->state = TCP_TIME_WAIT;
                t->timewait_at = net_now_ms() + TCP_TIME_WAIT_MS;
            }
        } else if (t->state == TCP_FIN_WAIT_2) {
            t->state = TCP_TIME_WAIT;
            t->timewait_at = net_now_ms() + TCP_TIME_WAIT_MS;
        }

        if (t->sock != NULL)
            net_wake(t->sock);
    }

    tcp_output(t);
}

/* ================================================================
 * Таймеры (каждые 10 мс из потока net)
 * ================================================================ */

void tcp_timer(void)
{
    UINT64 now = net_now_ms();

    for (UINTN i = 0; i < TCB_MAX; i++) {

        TCB *t = &g_tcb[i];

        if (!t->used)
            continue;

        if (t->state == TCP_CLOSED) {
            if (t->sock == NULL)
                tcb_free(t);
            continue;
        }

        if (t->state == TCP_TIME_WAIT) {
            if (now >= t->timewait_at) {
                if (t->sock == NULL)
                    tcb_free(t);
                else
                    t->state = TCP_CLOSED;
            }
            continue;
        }

        /* брошенное программой соединение, которое застряло
           (собеседник не закрывает) - через минуту забыть */
        if (!tcb_has_reader(t) && t->state != TCP_LISTEN &&
            now - t->last_ms > TCP_ORPHAN_MS) {
            tcp_send_rst(t->rip, t->lip, t->rport, t->lport, t->snd_nxt, t->rcv_nxt, TRUE);
            tcb_free(t);
            continue;
        }

        if (t->rtx_at == 0 || now < t->rtx_at)
            continue;

        /* --- таймер повтора сработал --- */
        UINT32 max = (t->state == TCP_SYN_SENT || t->state == TCP_SYN_RCVD) ?
                     TCP_SYN_RETRIES : TCP_MAX_RETRIES;

        if (t->retries >= max) {
            klog("tcp: connection to port %u timed out\n", t->rport);
            tcb_finish(t, MYOS_ETIMEDOUT);
            continue;
        }

        t->retries++;
        g_tcp_retrans++;
        t->rtt_active = FALSE;                     /* Карн: не мерить повторы */

        t->rto_ms *= 2u;
        if (t->rto_ms > TCP_RTO_MAX)
            t->rto_ms = TCP_RTO_MAX;

        if (t->state == TCP_SYN_SENT) {
            tcp_send_seg(t, TCP_SYN, t->iss, 0, 0);
            tcp_arm_timer(t);
            continue;
        }

        if (t->state == TCP_SYN_RCVD) {
            tcp_send_seg(t, TCP_SYN | TCP_ACK, t->iss, 0, 0);
            tcp_arm_timer(t);
            continue;
        }

        UINT32 flight = t->snd_nxt - t->snd_una;

        /* нулевое окно и ничего в пути: проба одним байтом */
        if (flight == 0 && t->snd_wnd == 0 && t->sbuf_len > t->snd_nxt - t->sbuf_seq) {
            UINT32 off = t->snd_nxt - t->sbuf_seq;
            tcp_send_seg(t, TCP_ACK, t->snd_nxt, off, 1);
            t->snd_nxt++;
            t->retries = 0;                        /* проба - не повтор */
            tcp_arm_timer(t);
            continue;
        }

        /* потеря: окно перегрузки - на минимум, всё с snd_una заново */
        t->ssthresh = (flight / 2u > 2u * t->mss) ? flight / 2u : 2u * t->mss;
        t->cwnd = t->mss;
        t->snd_nxt = t->snd_una;

        if (t->fin_sent && SEQ_LE(t->snd_una, t->fin_seq)) {
            t->fin_sent = FALSE;               /* FIN отправится снова */
            if (t->state == TCP_FIN_WAIT_1) t->state = TCP_ESTABLISHED;
            else if (t->state == TCP_LAST_ACK) t->state = TCP_CLOSE_WAIT;
            else if (t->state == TCP_CLOSING) t->state = TCP_CLOSE_WAIT;
        }

        t->rtx_at = 0;
        tcp_output(t);

        if (t->rtx_at == 0)
            tcp_arm_timer(t);
    }
}

/* ================================================================
 * Диагностика
 * ================================================================ */

void tcp_print(SIMPLE_TEXT_OUTPUT_INTERFACE *out)
{
    UINTN n = 0;

    print(out, "TCP connections:\n");

    for (UINTN i = 0; i < TCB_MAX; i++) {

        TCB *t = &g_tcb[i];

        if (!t->used)
            continue;

        char l[16], r[16];
        net_fmt_ip(l, sizeof(l), t->lip);
        net_fmt_ip(r, sizeof(r), t->rip);

        if (t->state == TCP_LISTEN)
            kprintf(out, "  %s:%u  LISTEN\n", l, t->lport);
        else
            kprintf(out, "  %s:%u -> %s:%u  %s  in %llu B, out %llu B, rtt %u ms%s\n",
                    l, t->lport, r, t->rport, tcp_state_name(t->state),
                    t->bytes_in, t->bytes_acked, t->srtt,
                    t->sock ? "" : " (closed by the program)");
        n++;
    }

    if (n == 0)
        print(out, "  (none)\n");

    kprintf(out, "TCP: %llu segments in, %llu bad, %llu resets sent, %llu retransmits\n",
            g_tcp_in, g_tcp_bad, g_tcp_rst_sent, g_tcp_retrans);
}
