/*
 * user/tls/tls.c - TLS (шифрование HTTPS) для программ MyOS поверх
 * BearSSL (third_party/bearssl, MIT). Часть мини-libc; для программы
 * это пять функций из myos.h: tls_open / tls_send / tls_recv /
 * tls_close / tls_info.
 *
 * Как устроено соединение HTTPS:
 *   1. обычный TCP к порту 443 (сокет открывает программа);
 *   2. "рукопожатие" TLS 1.2: договориться о шифре, получить от
 *      сервера цепочку сертификатов и ПРОВЕРИТЬ её: подписи ведут к
 *      одному из корневых сертификатов (roots.c - список Mozilla), срок
 *      действия не истёк (нужны точные дата и время - UTC из ядра),
 *      сертификат выдан именно этому имени сайта;
 *   3. общий секретный ключ (ECDHE) - случайные числа для него даёт
 *      ядро (getrandom);
 *   4. дальше все данные шифруются (AES-GCM или ChaCha20-Poly1305).
 * Сам протокол целиком - BearSSL; здесь - связка с сокетами MyOS,
 * временем, случайными числами, файлами сертификатов, понятные
 * сообщения об ошибках и режим "без проверки" (--no-check-certificate).
 */
#include "tls.h"

struct myos_tls {
    br_ssl_client_context  sc;
    br_x509_minimal_context xc;
    /* режим без проверки: свой "проверяльщик" - берёт ключ из первого
       сертификата цепочки, ничего не проверяя */
    const br_x509_class   *nv_vtable;
    br_x509_decoder_context nv_dec;
    int                    nv_first;
    br_sslio_context       io;
    int                    fd;
    br_x509_trust_anchor  *tas;          /* корни Mozilla + из файла */
    size_t                 ntas, nextra;
    unsigned char          iobuf[BR_SSL_BUFSIZE_BIDI];
};

/* ================================================================
 * Сокет <-> BearSSL
 * ================================================================ */

static int sock_read(void *ctx, unsigned char *buf, size_t len)
{
    long r = recv(*(int *)ctx, buf, len);

    return (r > 0) ? (int)r : -1;
}

static int sock_write(void *ctx, const unsigned char *buf, size_t len)
{
    long r = send(*(int *)ctx, buf, len);

    return (r > 0) ? (int)r : -1;
}

/* ================================================================
 * Режим без проверки сертификата
 * ================================================================ */

static void nv_start_chain(const br_x509_class **ctx, const char *name)
{
    struct myos_tls *t = (struct myos_tls *)((char *)ctx - offsetof(struct myos_tls, nv_vtable));

    (void)name;
    t->nv_first = 1;
    br_x509_decoder_init(&t->nv_dec, 0, 0);
}

static void nv_start_cert(const br_x509_class **ctx, uint32_t length)
{
    (void)ctx;
    (void)length;
}

static void nv_append(const br_x509_class **ctx, const unsigned char *buf, size_t len)
{
    struct myos_tls *t = (struct myos_tls *)((char *)ctx - offsetof(struct myos_tls, nv_vtable));

    if (t->nv_first)
        br_x509_decoder_push(&t->nv_dec, buf, len);
}

static void nv_end_cert(const br_x509_class **ctx)
{
    struct myos_tls *t = (struct myos_tls *)((char *)ctx - offsetof(struct myos_tls, nv_vtable));

    t->nv_first = 0;
}

static unsigned nv_end_chain(const br_x509_class **ctx)
{
    struct myos_tls *t = (struct myos_tls *)((char *)ctx - offsetof(struct myos_tls, nv_vtable));

    return br_x509_decoder_get_pkey(&t->nv_dec) ? 0 : BR_ERR_X509_BAD_SERVER_NAME;
}

static const br_x509_pkey *nv_get_pkey(const br_x509_class *const *ctx, unsigned *usages)
{
    struct myos_tls *t = (struct myos_tls *)((char *)ctx - offsetof(struct myos_tls, nv_vtable));

    if (usages)
        *usages = BR_KEYTYPE_KEYX | BR_KEYTYPE_SIGN;

    return br_x509_decoder_get_pkey(&t->nv_dec);
}

static const br_x509_class g_noverify_class = {
    sizeof(const br_x509_class *),
    nv_start_chain, nv_start_cert, nv_append, nv_end_cert, nv_end_chain, nv_get_pkey
};

/* ================================================================
 * Свои корневые сертификаты из файла PEM (--ca-certificate)
 * ================================================================ */

typedef struct {
    unsigned char *buf;
    size_t len, cap;
} VBUF;

static void vbuf_append(void *ctx, const void *data, size_t len)
{
    VBUF *v = (VBUF *)ctx;

    if (v->len + len > v->cap) {
        size_t nc = (v->cap ? v->cap * 2 : 1024);
        while (nc < v->len + len)
            nc *= 2;
        unsigned char *nb = (unsigned char *)malloc(nc);
        if (nb == NULL)
            return;
        if (v->buf) {
            memcpy(nb, v->buf, v->len);
            free(v->buf);
        }
        v->buf = nb;
        v->cap = nc;
    }

    memcpy(v->buf + v->len, data, len);
    v->len += len;
}

static unsigned char *blobdup(const void *p, size_t n)
{
    unsigned char *b = (unsigned char *)malloc(n ? n : 1);

    if (b)
        memcpy(b, p, n);

    return b;
}

/* Один сертификат (DER) -> ещё один корень в t->tas. 0 - ок */
static int add_anchor(struct myos_tls *t, const unsigned char *der, size_t len)
{
    br_x509_decoder_context dc;
    VBUF dn = { 0, 0, 0 };

    br_x509_decoder_init(&dc, vbuf_append, &dn);
    br_x509_decoder_push(&dc, der, len);

    br_x509_pkey *pk = br_x509_decoder_get_pkey(&dc);

    if (pk == NULL || dn.buf == NULL)
        return -1;

    br_x509_trust_anchor *ta = &t->tas[t->ntas];

    ta->dn.data = dn.buf;
    ta->dn.len = dn.len;
    ta->flags = br_x509_decoder_isCA(&dc) ? BR_X509_TA_CA : 0;
    ta->pkey.key_type = pk->key_type;

    if (pk->key_type == BR_KEYTYPE_RSA) {
        ta->pkey.key.rsa.n = blobdup(pk->key.rsa.n, pk->key.rsa.nlen);
        ta->pkey.key.rsa.nlen = pk->key.rsa.nlen;
        ta->pkey.key.rsa.e = blobdup(pk->key.rsa.e, pk->key.rsa.elen);
        ta->pkey.key.rsa.elen = pk->key.rsa.elen;
    } else if (pk->key_type == BR_KEYTYPE_EC) {
        ta->pkey.key.ec.curve = pk->key.ec.curve;
        ta->pkey.key.ec.q = blobdup(pk->key.ec.q, pk->key.ec.qlen);
        ta->pkey.key.ec.qlen = pk->key.ec.qlen;
    } else {
        return -1;
    }

    t->ntas++;
    t->nextra++;
    return 0;
}

/* Прочитать PEM-файл: все блоки CERTIFICATE -> корни. Сколько добавлено
   (<0 - файл не прочитан) */
static int load_ca_file(struct myos_tls *t, const char *path, size_t room)
{
    int f = open(path, O_READ);

    if (f < 0)
        return -1;

    VBUF file = { 0, 0, 0 };
    unsigned char chunk[1024];

    for (;;) {
        long n = read(f, chunk, sizeof(chunk));
        if (n <= 0)
            break;
        vbuf_append(&file, chunk, (size_t)n);
    }

    close(f);
    vbuf_append(&file, "\n", 1);      /* файл без перевода строки в конце */

    br_pem_decoder_context pc;
    VBUF obj = { 0, 0, 0 };
    int in_cert = 0, added = 0;
    const unsigned char *p = file.buf;
    size_t len = file.len;

    br_pem_decoder_init(&pc);

    while (len > 0) {

        size_t k = br_pem_decoder_push(&pc, p, len);

        p += k;
        len -= k;

        switch (br_pem_decoder_event(&pc)) {

        case BR_PEM_BEGIN_OBJ: {
            const char *nm = br_pem_decoder_name(&pc);
            in_cert = (strlen(nm) == 11 && memcmp(nm, "CERTIFICATE", 11) == 0) ||
                      (strlen(nm) == 16 && memcmp(nm, "X509 CERTIFICATE", 16) == 0);
            obj.len = 0;
            br_pem_decoder_setdest(&pc, in_cert ? vbuf_append : 0, in_cert ? &obj : 0);
            break;
        }

        case BR_PEM_END_OBJ:
            if (in_cert && t->nextra < room && add_anchor(t, obj.buf, obj.len) == 0)
                added++;
            in_cert = 0;
            break;

        case BR_PEM_ERROR:
            len = 0;
            break;
        }
    }

    if (obj.buf) free(obj.buf);
    if (file.buf) free(file.buf);

    return added;
}

/* ================================================================
 * Понятные сообщения об ошибках
 * ================================================================ */

static const char *tls_err_text(int e)
{
    switch (e) {
    case BR_ERR_X509_NOT_TRUSTED:
        return "the site's certificate is not signed by a known authority";
    case BR_ERR_X509_EXPIRED:
        return "the certificate has expired or is not valid yet (is the clock right? see 'date')";
    case BR_ERR_X509_BAD_SERVER_NAME:
        return "the certificate belongs to another site (wrong name)";
    case BR_ERR_X509_TIME_UNKNOWN:
        return "the time is unknown - cannot check the certificate";
    case BR_ERR_X509_UNSUPPORTED:
    case BR_ERR_X509_WEAK_PUBLIC_KEY:
        return "the certificate uses something not supported";
    case BR_ERR_BAD_VERSION:
        return "the server and MyOS found no common TLS version (the site may need TLS 1.3)";
    case BR_ERR_BAD_CIPHER_SUITE:
        return "no common cipher";
    case BR_ERR_NO_RANDOM:
        return "no random numbers for the keys";
    case BR_ERR_IO:
        return "the connection broke";
    case BR_ERR_BAD_MAC:
        return "damaged data (bad MAC)";
    default:
        return 0;
    }
}

static void set_err(char *err, size_t cap, int e)
{
    const char *s = tls_err_text(e);

    if (err == 0 || cap == 0)
        return;

    if (s)
        snprintf(err, cap, "%s", s);
    else if (e >= BR_ERR_RECV_FATAL_ALERT && e < BR_ERR_SEND_FATAL_ALERT)
        snprintf(err, cap, "the server refused (TLS alert %d%s)", e - BR_ERR_RECV_FATAL_ALERT,
                 e - BR_ERR_RECV_FATAL_ALERT == 70 ? ": protocol version - TLS 1.3 only?" :
                 e - BR_ERR_RECV_FATAL_ALERT == 40 ? ": handshake failure" : "");
    else
        snprintf(err, cap, "TLS error %d", e);
}

/* ================================================================
 * API
 * ================================================================ */

/* дата -> дни от 1 января 0 года (как хочет BearSSL) */
static uint32_t days_from_civil(int y, unsigned m, unsigned d)
{
    y -= m <= 2;
    int era = (y >= 0 ? y : y - 399) / 400;
    unsigned yoe = (unsigned)(y - era * 400);
    unsigned doy = (153 * (m + (m > 2 ? -3 : 9)) + 2) / 5 + d - 1;
    unsigned doe = yoe * 365 + yoe / 4 - yoe / 100 + doy;
    long unix_days = (long)era * 146097 + (long)doe - 719468;

    return (uint32_t)(unix_days + 719528);
}

static int is_ip(const char *h)
{
    for (; *h; h++)
        if (!((*h >= '0' && *h <= '9') || *h == '.'))
            return 0;
    return 1;
}

struct myos_tls *tls_open(int fd, const char *host, int flags, const char *ca_file,
                          char *err, size_t errcap)
{
    struct myos_tls *t = (struct myos_tls *)malloc(sizeof(*t));

    if (t == 0) {
        snprintf(err, errcap, "out of memory");
        return 0;
    }

    memset(t, 0, sizeof(*t));
    t->fd = fd;

    /* корни: Mozilla + (если есть) из файла - до 32 штук */
    size_t room = ca_file ? 32 : 0;

    t->tas = (br_x509_trust_anchor *)malloc((g_tls_roots_num + room + 1) * sizeof(br_x509_trust_anchor));
    if (t->tas == 0) {
        free(t);
        snprintf(err, errcap, "out of memory");
        return 0;
    }

    memcpy(t->tas, g_tls_roots, g_tls_roots_num * sizeof(br_x509_trust_anchor));
    t->ntas = g_tls_roots_num;

    if (ca_file) {
        int n = load_ca_file(t, ca_file, room);
        if (n <= 0) {
            snprintf(err, errcap, n < 0 ? "cannot read the certificate file %s" :
                     "no certificates in %s", ca_file);
            tls_close(t);
            return 0;
        }
    }

    br_ssl_client_init_full(&t->sc, &t->xc, t->tas, t->ntas);

    /* время для проверки сроков сертификатов */
    struct myos_time tm;

    if (gettime_utc(&tm) == 0)
        br_x509_minimal_set_time(&t->xc, days_from_civil(tm.year, tm.month, tm.day),
                                 (uint32_t)tm.hour * 3600u + tm.minute * 60u + tm.second);

    if (flags & TLS_NO_VERIFY) {
        t->nv_vtable = &g_noverify_class;
        br_ssl_engine_set_x509(&t->sc.eng, &t->nv_vtable);
    }

    br_ssl_engine_set_buffer(&t->sc.eng, t->iobuf, sizeof(t->iobuf), 1);

    /* случайные числа для ключей - от ядра */
    unsigned char seed[48];

    if (getrandom(seed, sizeof(seed)) != (long)sizeof(seed)) {
        snprintf(err, errcap, "no random numbers from the kernel");
        tls_close(t);
        return 0;
    }

    br_ssl_engine_inject_entropy(&t->sc.eng, seed, sizeof(seed));

    /* имя сайта: для проверки сертификата и SNI. У адреса вида
       1.2.3.4 имени нет - проверяется только цепочка подписей */
    if (!br_ssl_client_reset(&t->sc, is_ip(host) ? 0 : host, 0)) {
        set_err(err, errcap, br_ssl_engine_last_error(&t->sc.eng));
        tls_close(t);
        return 0;
    }

    br_sslio_init(&t->io, &t->sc.eng, sock_read, &t->fd, sock_write, &t->fd);

    /* провести рукопожатие сейчас, чтобы ошибки сертификата были видны
       сразу, а не при первом чтении */
    br_sslio_flush(&t->io);

    unsigned st = br_ssl_engine_current_state(&t->sc.eng);

    if (st == BR_SSL_CLOSED || !(st & (BR_SSL_SENDAPP | BR_SSL_RECVAPP))) {
        int e = br_ssl_engine_last_error(&t->sc.eng);
        set_err(err, errcap, e ? e : BR_ERR_IO);
        tls_close(t);
        return 0;
    }

    return t;
}

long tls_send(struct myos_tls *t, const void *buf, size_t n)
{
    if (br_sslio_write_all(&t->io, buf, n) < 0 || br_sslio_flush(&t->io) < 0)
        return -1;

    return (long)n;
}

/* >0 - байты, 0 - сервер закончил, <0 - ошибка (текст - tls_error) */
long tls_recv(struct myos_tls *t, void *buf, size_t n)
{
    int r = br_sslio_read(&t->io, buf, n);

    if (r > 0)
        return r;

    int e = br_ssl_engine_last_error(&t->sc.eng);

    /* закрыл вежливо (close_notify) или просто оборвал TCP после
       ответа - для HTTP/1.0 это обычный конец */
    return (e == BR_ERR_OK || e == BR_ERR_IO) ? 0 : -1;
}

void tls_error(struct myos_tls *t, char *err, size_t cap)
{
    set_err(err, cap, br_ssl_engine_last_error(&t->sc.eng));
}

/* "TLS 1.2, ECDHE-RSA, AES-128-GCM" */
void tls_info(struct myos_tls *t, char *buf, size_t cap)
{
    br_ssl_session_parameters sp;

    br_ssl_engine_get_session_parameters(&t->sc.eng, &sp);

    unsigned v = sp.version;
    unsigned cs = sp.cipher_suite;
    const char *kx = "RSA", *ci = "?";

    switch (cs) {
    case BR_TLS_ECDHE_RSA_WITH_AES_128_GCM_SHA256:       kx = "ECDHE-RSA";   ci = "AES-128-GCM"; break;
    case BR_TLS_ECDHE_RSA_WITH_AES_256_GCM_SHA384:       kx = "ECDHE-RSA";   ci = "AES-256-GCM"; break;
    case BR_TLS_ECDHE_ECDSA_WITH_AES_128_GCM_SHA256:     kx = "ECDHE-ECDSA"; ci = "AES-128-GCM"; break;
    case BR_TLS_ECDHE_ECDSA_WITH_AES_256_GCM_SHA384:     kx = "ECDHE-ECDSA"; ci = "AES-256-GCM"; break;
    case BR_TLS_ECDHE_RSA_WITH_CHACHA20_POLY1305_SHA256: kx = "ECDHE-RSA";   ci = "ChaCha20-Poly1305"; break;
    case BR_TLS_ECDHE_ECDSA_WITH_CHACHA20_POLY1305_SHA256: kx = "ECDHE-ECDSA"; ci = "ChaCha20-Poly1305"; break;
    default: ci = "other cipher"; break;
    }

    snprintf(buf, cap, "TLS 1.%u, %s, %s (cipher 0x%04x)", (v & 0xFFu) - 1u, kx, ci, cs);
}

void tls_close(struct myos_tls *t)
{
    if (t == 0)
        return;

    if (br_ssl_engine_current_state(&t->sc.eng) != BR_SSL_CLOSED)
        br_sslio_close(&t->io);

    /* корни, прочитанные из файла, - наши копии */
    for (size_t i = g_tls_roots_num; i < t->ntas; i++) {
        free(t->tas[i].dn.data);
        if (t->tas[i].pkey.key_type == BR_KEYTYPE_RSA) {
            free(t->tas[i].pkey.key.rsa.n);
            free(t->tas[i].pkey.key.rsa.e);
        } else {
            free(t->tas[i].pkey.key.ec.q);
        }
    }

    free(t->tas);
    free(t);
}
