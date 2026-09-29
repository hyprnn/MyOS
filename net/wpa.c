/*
 * net/wpa.c - безопасность Wi-Fi (WPA2-PSK / RSN с CCMP): всё, что не
 * зависит от радиочипа. Часть MyOS; объявления - в net/wifi.h.
 *
 * Как подключается к сети с паролем любое устройство Wi-Fi:
 *   1. PMK (главный ключ) = PBKDF2-HMAC-SHA1(пароль, имя сети, 4096
 *      повторов, 32 байта) - считается один раз;
 *   2. после подключения к точке доступа (AP) - "4-стороннее
 *      рукопожатие" кадрами EAPOL-Key:
 *        1/4 AP -> мы:  ANonce (случайное число AP)
 *        2/4 мы -> AP:  SNonce + подпись (MIC); из PMK, обоих MAC и
 *                        обоих чисел обе стороны считают PTK (PRF-512):
 *                        KCK (подписи), KEK (шифрование ключей),
 *                        TK (шифрование данных);
 *        3/4 AP -> мы:  подписано, внутри - GTK (ключ для
 *                        широковещательных кадров), зашифрованный KEK
 *                        (AES Key Wrap, RFC 3394);
 *        4/4 мы -> AP:  "ключи установлены";
 *   3. дальше каждый кадр данных шифруется CCMP = AES-128 в режиме
 *      CCM (счётчик + подпись 8 байт) ключом TK.
 * Пароль по воздуху не передаётся никогда - только доказательство,
 * что он известен (подписи MIC).
 *
 * Здесь всё это написано с нуля: SHA-1, HMAC, PBKDF2, PRF, AES-128
 * (шифрование и расшифровка), Key Wrap, CCM/CCMP и сторона клиента
 * ("supplicant") рукопожатия. Проверяется командой "wifi selftest"
 * на официальных тестовых векторах (FIPS 180/197, RFC 2202/3394/3610,
 * IEEE 802.11i) и на полном рукопожатии с программной точкой доступа.
 * Не хватает только драйвера радиочасти конкретного чипа - см.
 * net/wifi.c.
 */
#include "net.h"
#include "wifi.h"

/* ================================================================
 * SHA-1 (FIPS 180-4)
 * ================================================================ */

static UINT32 rol32(UINT32 x, UINT32 n)
{
    return (x << n) | (x >> (32u - n));
}

static void sha1_block(SHA1_CTX *c, const UINT8 *p)
{
    UINT32 w[80];

    for (UINTN i = 0; i < 16; i++)
        w[i] = ((UINT32)p[4 * i] << 24) | ((UINT32)p[4 * i + 1] << 16) |
               ((UINT32)p[4 * i + 2] << 8) | p[4 * i + 3];

    for (UINTN i = 16; i < 80; i++)
        w[i] = rol32(w[i - 3] ^ w[i - 8] ^ w[i - 14] ^ w[i - 16], 1);

    UINT32 a = c->h[0], b = c->h[1], cc = c->h[2], d = c->h[3], e = c->h[4];

    for (UINTN i = 0; i < 80; i++) {

        UINT32 f, k;

        if (i < 20)      { f = (b & cc) | (~b & d);            k = 0x5A827999u; }
        else if (i < 40) { f = b ^ cc ^ d;                     k = 0x6ED9EBA1u; }
        else if (i < 60) { f = (b & cc) | (b & d) | (cc & d);  k = 0x8F1BBCDCu; }
        else             { f = b ^ cc ^ d;                     k = 0xCA62C1D6u; }

        UINT32 t = rol32(a, 5) + f + e + k + w[i];

        e = d;
        d = cc;
        cc = rol32(b, 30);
        b = a;
        a = t;
    }

    c->h[0] += a;
    c->h[1] += b;
    c->h[2] += cc;
    c->h[3] += d;
    c->h[4] += e;
}

void sha1_init(SHA1_CTX *c)
{
    c->h[0] = 0x67452301u;
    c->h[1] = 0xEFCDAB89u;
    c->h[2] = 0x98BADCFEu;
    c->h[3] = 0x10325476u;
    c->h[4] = 0xC3D2E1F0u;
    c->len = 0;
    c->n = 0;
}

void sha1_update(SHA1_CTX *c, const void *data, UINTN len)
{
    const UINT8 *p = (const UINT8 *)data;

    c->len += len;

    while (len > 0) {

        UINTN k = 64u - c->n;

        if (k > len)
            k = len;

        memcpy(c->buf + c->n, p, k);
        c->n += (UINT32)k;
        p += k;
        len -= k;

        if (c->n == 64) {
            sha1_block(c, c->buf);
            c->n = 0;
        }
    }
}

void sha1_final(SHA1_CTX *c, UINT8 out[20])
{
    UINT64 bits = c->len * 8u;
    UINT8 pad = 0x80;
    UINT8 zero = 0;

    sha1_update(c, &pad, 1);

    while (c->n != 56)
        sha1_update(c, &zero, 1);

    UINT8 lb[8];

    for (UINTN i = 0; i < 8; i++)
        lb[i] = (UINT8)(bits >> (56u - 8u * i));

    sha1_update(c, lb, 8);

    for (UINTN i = 0; i < 5; i++) {
        out[4 * i] = (UINT8)(c->h[i] >> 24);
        out[4 * i + 1] = (UINT8)(c->h[i] >> 16);
        out[4 * i + 2] = (UINT8)(c->h[i] >> 8);
        out[4 * i + 3] = (UINT8)c->h[i];
    }
}

/* ================================================================
 * HMAC-SHA1 (RFC 2104) - данные частями (n кусков)
 * ================================================================ */

void hmac_sha1_v(const UINT8 *key, UINTN klen, UINTN n, const UINT8 *parts[],
                 const UINTN lens[], UINT8 out[20])
{
    UINT8 k[64], pad[64], inner[20];
    SHA1_CTX c;

    memset(k, 0, sizeof(k));

    if (klen > 64) {
        sha1_init(&c);
        sha1_update(&c, key, klen);
        sha1_final(&c, k);
    } else {
        memcpy(k, key, klen);
    }

    for (UINTN i = 0; i < 64; i++)
        pad[i] = (UINT8)(k[i] ^ 0x36u);

    sha1_init(&c);
    sha1_update(&c, pad, 64);
    for (UINTN i = 0; i < n; i++)
        sha1_update(&c, parts[i], lens[i]);
    sha1_final(&c, inner);

    for (UINTN i = 0; i < 64; i++)
        pad[i] = (UINT8)(k[i] ^ 0x5Cu);

    sha1_init(&c);
    sha1_update(&c, pad, 64);
    sha1_update(&c, inner, 20);
    sha1_final(&c, out);
}

void hmac_sha1(const UINT8 *key, UINTN klen, const UINT8 *data, UINTN len, UINT8 out[20])
{
    const UINT8 *p[1] = { data };
    UINTN l[1] = { len };

    hmac_sha1_v(key, klen, 1, p, l, out);
}

/* PBKDF2-HMAC-SHA1 (RFC 8018): из пароля и имени сети - PMK */
void pbkdf2_sha1(const UINT8 *pass, UINTN plen, const UINT8 *salt, UINTN slen,
                 UINT32 iters, UINT8 *out, UINTN olen)
{
    for (UINT32 block = 1; olen > 0; block++) {

        UINT8 cnt[4] = { (UINT8)(block >> 24), (UINT8)(block >> 16), (UINT8)(block >> 8),
                         (UINT8)block };
        const UINT8 *parts[2] = { salt, cnt };
        UINTN lens[2] = { slen, 4 };
        UINT8 u[20], t[20];

        hmac_sha1_v(pass, plen, 2, parts, lens, u);
        memcpy(t, u, 20);

        for (UINT32 i = 1; i < iters; i++) {
            hmac_sha1(pass, plen, u, 20, u);
            for (UINTN k = 0; k < 20; k++)
                t[k] ^= u[k];
        }

        UINTN k = olen < 20 ? olen : 20;

        memcpy(out, t, k);
        out += k;
        olen -= k;
    }
}

/* PRF из IEEE 802.11i: HMAC-SHA1(K, метка || 0 || данные || номер) */
void wpa_prf(const UINT8 *key, UINTN klen, const char *label, const UINT8 *data,
             UINTN dlen, UINT8 *out, UINTN olen)
{
    UINT8 zero = 0;
    UINTN ll = 0;

    while (label[ll])
        ll++;

    for (UINT8 i = 0; olen > 0; i++) {

        const UINT8 *parts[4] = { (const UINT8 *)label, &zero, data, &i };
        UINTN lens[4] = { ll, 1, dlen, 1 };
        UINT8 h[20];

        hmac_sha1_v(key, klen, 4, parts, lens, h);

        UINTN k = olen < 20 ? olen : 20;

        memcpy(out, h, k);
        out += k;
        olen -= k;
    }
}

/* ================================================================
 * AES-128 (FIPS 197): шифрование и расшифровка блока 16 байт
 * ================================================================ */

static const UINT8 g_sbox[256] = {
    0x63,0x7c,0x77,0x7b,0xf2,0x6b,0x6f,0xc5,0x30,0x01,0x67,0x2b,0xfe,0xd7,0xab,0x76,
    0xca,0x82,0xc9,0x7d,0xfa,0x59,0x47,0xf0,0xad,0xd4,0xa2,0xaf,0x9c,0xa4,0x72,0xc0,
    0xb7,0xfd,0x93,0x26,0x36,0x3f,0xf7,0xcc,0x34,0xa5,0xe5,0xf1,0x71,0xd8,0x31,0x15,
    0x04,0xc7,0x23,0xc3,0x18,0x96,0x05,0x9a,0x07,0x12,0x80,0xe2,0xeb,0x27,0xb2,0x75,
    0x09,0x83,0x2c,0x1a,0x1b,0x6e,0x5a,0xa0,0x52,0x3b,0xd6,0xb3,0x29,0xe3,0x2f,0x84,
    0x53,0xd1,0x00,0xed,0x20,0xfc,0xb1,0x5b,0x6a,0xcb,0xbe,0x39,0x4a,0x4c,0x58,0xcf,
    0xd0,0xef,0xaa,0xfb,0x43,0x4d,0x33,0x85,0x45,0xf9,0x02,0x7f,0x50,0x3c,0x9f,0xa8,
    0x51,0xa3,0x40,0x8f,0x92,0x9d,0x38,0xf5,0xbc,0xb6,0xda,0x21,0x10,0xff,0xf3,0xd2,
    0xcd,0x0c,0x13,0xec,0x5f,0x97,0x44,0x17,0xc4,0xa7,0x7e,0x3d,0x64,0x5d,0x19,0x73,
    0x60,0x81,0x4f,0xdc,0x22,0x2a,0x90,0x88,0x46,0xee,0xb8,0x14,0xde,0x5e,0x0b,0xdb,
    0xe0,0x32,0x3a,0x0a,0x49,0x06,0x24,0x5c,0xc2,0xd3,0xac,0x62,0x91,0x95,0xe4,0x79,
    0xe7,0xc8,0x37,0x6d,0x8d,0xd5,0x4e,0xa9,0x6c,0x56,0xf4,0xea,0x65,0x7a,0xae,0x08,
    0xba,0x78,0x25,0x2e,0x1c,0xa6,0xb4,0xc6,0xe8,0xdd,0x74,0x1f,0x4b,0xbd,0x8b,0x8a,
    0x70,0x3e,0xb5,0x66,0x48,0x03,0xf6,0x0e,0x61,0x35,0x57,0xb9,0x86,0xc1,0x1d,0x9e,
    0xe1,0xf8,0x98,0x11,0x69,0xd9,0x8e,0x94,0x9b,0x1e,0x87,0xe9,0xce,0x55,0x28,0xdf,
    0x8c,0xa1,0x89,0x0d,0xbf,0xe6,0x42,0x68,0x41,0x99,0x2d,0x0f,0xb0,0x54,0xbb,0x16
};

static UINT8 g_inv_sbox[256];
static BOOLEAN g_aes_ready = FALSE;

static UINT8 xtime(UINT8 x)
{
    return (UINT8)((x << 1) ^ ((x & 0x80u) ? 0x1Bu : 0));
}

static UINT8 gmul(UINT8 a, UINT8 b)
{
    UINT8 r = 0;

    while (b) {
        if (b & 1u)
            r ^= a;
        a = xtime(a);
        b >>= 1;
    }

    return r;
}

void aes128_init(AES128 *a, const UINT8 key[16])
{
    if (!g_aes_ready) {
        for (UINTN i = 0; i < 256; i++)
            g_inv_sbox[g_sbox[i]] = (UINT8)i;
        g_aes_ready = TRUE;
    }

    /* расширение ключа: 11 ключей раундов по 16 байт */
    memcpy(a->rk, key, 16);

    UINT8 rcon = 1;

    for (UINTN i = 16; i < 176; i += 4) {

        UINT8 t[4] = { a->rk[i - 4], a->rk[i - 3], a->rk[i - 2], a->rk[i - 1] };

        if (i % 16 == 0) {
            UINT8 x = t[0];
            t[0] = (UINT8)(g_sbox[t[1]] ^ rcon);
            t[1] = g_sbox[t[2]];
            t[2] = g_sbox[t[3]];
            t[3] = g_sbox[x];
            rcon = xtime(rcon);
        }

        for (UINTN k = 0; k < 4; k++)
            a->rk[i + k] = (UINT8)(a->rk[i - 16 + k] ^ t[k]);
    }
}

void aes128_encrypt(const AES128 *a, const UINT8 in[16], UINT8 out[16])
{
    UINT8 s[16];

    for (UINTN i = 0; i < 16; i++)
        s[i] = (UINT8)(in[i] ^ a->rk[i]);

    for (UINTN round = 1; round <= 10; round++) {

        UINT8 t[16];

        /* SubBytes + ShiftRows (байт [строка r, столбец c] = s[4c + r]) */
        for (UINTN c = 0; c < 4; c++)
            for (UINTN r = 0; r < 4; r++)
                t[4 * c + r] = g_sbox[s[4 * ((c + r) % 4) + r]];

        /* MixColumns (кроме последнего раунда) */
        if (round != 10) {
            for (UINTN c = 0; c < 4; c++) {
                UINT8 *col = t + 4 * c;
                UINT8 a0 = col[0], a1 = col[1], a2 = col[2], a3 = col[3];
                UINT8 x = (UINT8)(a0 ^ a1 ^ a2 ^ a3);
                col[0] = (UINT8)(a0 ^ x ^ xtime((UINT8)(a0 ^ a1)));
                col[1] = (UINT8)(a1 ^ x ^ xtime((UINT8)(a1 ^ a2)));
                col[2] = (UINT8)(a2 ^ x ^ xtime((UINT8)(a2 ^ a3)));
                col[3] = (UINT8)(a3 ^ x ^ xtime((UINT8)(a3 ^ a0)));
            }
        }

        for (UINTN i = 0; i < 16; i++)
            s[i] = (UINT8)(t[i] ^ a->rk[16 * round + i]);
    }

    memcpy(out, s, 16);
}

void aes128_decrypt(const AES128 *a, const UINT8 in[16], UINT8 out[16])
{
    UINT8 s[16];

    for (UINTN i = 0; i < 16; i++)
        s[i] = (UINT8)(in[i] ^ a->rk[160 + i]);

    for (INTN round = 9; round >= 0; round--) {

        UINT8 t[16];

        /* InvShiftRows + InvSubBytes */
        for (UINTN c = 0; c < 4; c++)
            for (UINTN r = 0; r < 4; r++)
                t[4 * ((c + r) % 4) + r] = g_inv_sbox[s[4 * c + r]];

        for (UINTN i = 0; i < 16; i++)
            t[i] ^= a->rk[16 * (UINTN)round + i];

        /* InvMixColumns (кроме последнего) */
        if (round != 0) {
            for (UINTN c = 0; c < 4; c++) {
                UINT8 *col = t + 4 * c;
                UINT8 a0 = col[0], a1 = col[1], a2 = col[2], a3 = col[3];
                col[0] = (UINT8)(gmul(a0, 14) ^ gmul(a1, 11) ^ gmul(a2, 13) ^ gmul(a3, 9));
                col[1] = (UINT8)(gmul(a0, 9) ^ gmul(a1, 14) ^ gmul(a2, 11) ^ gmul(a3, 13));
                col[2] = (UINT8)(gmul(a0, 13) ^ gmul(a1, 9) ^ gmul(a2, 14) ^ gmul(a3, 11));
                col[3] = (UINT8)(gmul(a0, 11) ^ gmul(a1, 13) ^ gmul(a2, 9) ^ gmul(a3, 14));
            }
        }

        memcpy(s, t, 16);
    }

    memcpy(out, s, 16);
}

/* ================================================================
 * AES Key Wrap (RFC 3394): так AP присылает GTK в сообщении 3/4.
 * n - число 8-байтовых блоков ключа.
 * ================================================================ */

void aes_wrap(const UINT8 kek[16], UINTN n, const UINT8 *plain, UINT8 *out)
{
    AES128 a;
    UINT8 b[16];
    UINT8 *r = out + 8;

    aes128_init(&a, kek);
    memset(out, 0xA6, 8);
    memcpy(r, plain, 8 * n);

    for (UINTN j = 0; j < 6; j++) {
        for (UINTN i = 0; i < n; i++) {
            memcpy(b, out, 8);
            memcpy(b + 8, r + 8 * i, 8);
            aes128_encrypt(&a, b, b);
            UINT64 t = (UINT64)(n * j + i + 1);
            for (UINTN k = 0; k < 8; k++)
                out[k] = (UINT8)(b[k] ^ (UINT8)(t >> (56u - 8u * k)));
            memcpy(r + 8 * i, b + 8, 8);
        }
    }
}

BOOLEAN aes_unwrap(const UINT8 kek[16], UINTN n, const UINT8 *cipher, UINT8 *plain)
{
    AES128 a;
    UINT8 A[8], b[16];

    aes128_init(&a, kek);
    memcpy(A, cipher, 8);
    memcpy(plain, cipher + 8, 8 * n);

    for (INTN j = 5; j >= 0; j--) {
        for (INTN i = (INTN)n - 1; i >= 0; i--) {
            UINT64 t = (UINT64)(n * (UINTN)j + (UINTN)i + 1);
            for (UINTN k = 0; k < 8; k++)
                b[k] = (UINT8)(A[k] ^ (UINT8)(t >> (56u - 8u * k)));
            memcpy(b + 8, plain + 8 * i, 8);
            aes128_decrypt(&a, b, b);
            memcpy(A, b, 8);
            memcpy(plain + 8 * i, b + 8, 8);
        }
    }

    for (UINTN k = 0; k < 8; k++)
        if (A[k] != 0xA6)
            return FALSE;          /* неверный KEK или испорчено */

    return TRUE;
}

/* ================================================================
 * AES-CCM (RFC 3610) с L = 2 (длина сообщения - 2 байта, nonce - 13
 * байт) и подписью M байт. encrypt: in -> out + tag; иначе out = in
 * расшифрованное и сверка tag. TRUE - подпись верна.
 * ================================================================ */

BOOLEAN aes_ccm(const UINT8 key[16], const UINT8 nonce[13], const UINT8 *aad, UINTN aadlen,
                const UINT8 *in, UINTN len, UINT8 *out, UINT8 *tag, UINTN M, BOOLEAN encrypt)
{
    AES128 a;
    UINT8 x[16], blk[16], s[16], ctr[16];

    aes128_init(&a, key);

    /* --- CTR: A_i = флаги(L-1) || nonce || i --- */
    ctr[0] = 1;                         /* L - 1 */
    memcpy(ctr + 1, nonce, 13);

    for (UINTN off = 0, i = 1; off < len; off += 16, i++) {
        ctr[14] = (UINT8)(i >> 8);
        ctr[15] = (UINT8)i;
        aes128_encrypt(&a, ctr, s);
        UINTN k = (len - off < 16) ? len - off : 16;
        for (UINTN j = 0; j < k; j++)
            out[off + j] = (UINT8)(in[off + j] ^ s[j]);
    }

    const UINT8 *plain = encrypt ? in : out;

    /* --- CBC-MAC по B0, AAD (с длиной впереди), открытому тексту --- */
    blk[0] = (UINT8)((aadlen ? 0x40u : 0) | (((M - 2u) / 2u) << 3) | 1u);
    memcpy(blk + 1, nonce, 13);
    blk[14] = (UINT8)(len >> 8);
    blk[15] = (UINT8)len;
    aes128_encrypt(&a, blk, x);

    if (aadlen) {
        UINTN pos = 2;
        memset(blk, 0, 16);
        blk[0] = (UINT8)(aadlen >> 8);
        blk[1] = (UINT8)aadlen;
        for (UINTN i = 0; i < aadlen; i++) {
            blk[pos++] = aad[i];
            if (pos == 16) {
                for (UINTN k = 0; k < 16; k++) x[k] ^= blk[k];
                aes128_encrypt(&a, x, x);
                memset(blk, 0, 16);
                pos = 0;
            }
        }
        if (pos) {
            for (UINTN k = 0; k < 16; k++) x[k] ^= blk[k];
            aes128_encrypt(&a, x, x);
        }
    }

    for (UINTN off = 0; off < len; off += 16) {
        UINTN k = (len - off < 16) ? len - off : 16;
        for (UINTN j = 0; j < k; j++)
            x[j] ^= plain[off + j];
        aes128_encrypt(&a, x, x);
    }

    /* подпись = X XOR E(A_0) */
    ctr[14] = 0;
    ctr[15] = 0;
    aes128_encrypt(&a, ctr, s);

    if (encrypt) {
        for (UINTN j = 0; j < M; j++)
            tag[j] = (UINT8)(x[j] ^ s[j]);
        return TRUE;
    }

    UINT8 diff = 0;

    for (UINTN j = 0; j < M; j++)
        diff |= (UINT8)(tag[j] ^ x[j] ^ s[j]);

    return diff == 0;
}

/* ================================================================
 * CCMP: шифрование кадра данных 802.11.
 * hdr - заголовок MAC (24 байта; 26 - с QoS; 30/32 - с 4-м адресом),
 * на выходе: заголовок (бит Protected) + CCMP-заголовок 8 байт +
 * шифротекст + MIC 8 байт.
 * ================================================================ */

static void ccmp_aad_nonce(const UINT8 *hdr, UINTN hlen, UINT64 pn, UINT8 *aad, UINTN *aadlen,
                           UINT8 nonce[13])
{
    BOOLEAN a4 = ((hdr[1] & 3u) == 3u);                       /* ToDS и FromDS */
    BOOLEAN qos = ((hdr[0] & 0x0Cu) == 0x08u) && (hdr[0] & 0x80u);
    UINTN n = 0;

    /* FC: подтип (для данных), Retry, PwrMgt, MoreData - нули; Protected - 1 */
    aad[n++] = (UINT8)(hdr[0] & ((hdr[0] & 0x0Cu) == 0x08u ? 0x8Fu : 0xFFu));
    aad[n++] = (UINT8)((hdr[1] & 0xC7u) | 0x40u);
    memcpy(aad + n, hdr + 4, 18);                             /* A1, A2, A3 */
    n += 18;
    aad[n++] = (UINT8)(hdr[22] & 0x0Fu);                      /* только номер фрагмента */
    aad[n++] = 0;

    if (a4 && hlen >= 30) {
        memcpy(aad + n, hdr + 24, 6);
        n += 6;
    }

    UINT8 tid = 0;

    if (qos) {
        UINTN q = a4 ? 30 : 24;
        tid = (UINT8)(hdr[q] & 0x0Fu);
        aad[n++] = tid;
        aad[n++] = 0;
    }

    *aadlen = n;

    nonce[0] = tid;
    memcpy(nonce + 1, hdr + 10, 6);                           /* A2 - отправитель */
    for (UINTN i = 0; i < 6; i++)
        nonce[7 + i] = (UINT8)(pn >> (40u - 8u * i));
}

UINTN ccmp_encrypt(const UINT8 tk[16], const UINT8 *hdr, UINTN hlen, UINT64 pn, UINT8 keyid,
                   const UINT8 *data, UINTN len, UINT8 *out)
{
    UINT8 aad[32], nonce[13];
    UINTN aadlen;

    memcpy(out, hdr, hlen);
    out[1] |= 0x40u;                                          /* Protected */

    UINT8 *h = out + hlen;

    h[0] = (UINT8)pn;
    h[1] = (UINT8)(pn >> 8);
    h[2] = 0;
    h[3] = (UINT8)(0x20u | (keyid << 6));                     /* ExtIV + KeyID */
    h[4] = (UINT8)(pn >> 16);
    h[5] = (UINT8)(pn >> 24);
    h[6] = (UINT8)(pn >> 32);
    h[7] = (UINT8)(pn >> 40);

    ccmp_aad_nonce(out, hlen, pn, aad, &aadlen, nonce);
    aes_ccm(tk, nonce, aad, aadlen, data, len, h + 8, h + 8 + len, 8, TRUE);

    return hlen + 8u + len + 8u;
}

/* Расшифровать кадр (hlen - длина заголовка MAC). -1 - подпись
   неверна, иначе - длина данных в out; *pn - номер пакета */
INTN ccmp_decrypt(const UINT8 tk[16], const UINT8 *frame, UINTN flen, UINTN hlen,
                  UINT8 *out, UINT64 *pn_out)
{
    UINT8 aad[32], nonce[13], tag[8];
    UINTN aadlen;

    if (flen < hlen + 16u)
        return -1;

    const UINT8 *h = frame + hlen;
    UINT64 pn = (UINT64)h[0] | ((UINT64)h[1] << 8) | ((UINT64)h[4] << 16) |
                ((UINT64)h[5] << 24) | ((UINT64)h[6] << 32) | ((UINT64)h[7] << 40);
    UINTN len = flen - hlen - 16u;

    ccmp_aad_nonce(frame, hlen, pn, aad, &aadlen, nonce);
    memcpy(tag, frame + hlen + 8u + len, 8);

    if (!aes_ccm(tk, nonce, aad, aadlen, h + 8, len, out, tag, 8, FALSE))
        return -1;

    if (pn_out)
        *pn_out = pn;

    return (INTN)len;
}

/* ================================================================
 * 4-стороннее рукопожатие: сторона клиента (supplicant)
 *
 * Кадр EAPOL-Key (802.1X-2004 + 802.11i):
 *   [0] версия 2, [1] тип 3 (Key), [2..3] длина тела;
 *   тело: [4] тип дескриптора 2 (RSN), [5..6] Key Information,
 *   [7..8] длина ключа, [9..16] счётчик повторов, [17..48] Nonce,
 *   [49..64] IV, [65..72] RSC, [73..80] резерв, [81..96] MIC,
 *   [97..98] длина Key Data, [99..] Key Data.
 * ================================================================ */

#define KI_VER_2      0x0002    /* HMAC-SHA1-128 + AES Key Wrap */
#define KI_PAIRWISE   0x0008
#define KI_INSTALL    0x0040
#define KI_ACK        0x0080
#define KI_MIC        0x0100
#define KI_SECURE     0x0200
#define KI_ENC_DATA   0x1000

#define EK_INFO       5
#define EK_REPLAY     9
#define EK_NONCE      17
#define EK_MIC        81
#define EK_DLEN       97
#define EK_DATA       99

/* RSN IE клиента: WPA2, шифр CCMP (и для групповых), ключ из пароля (PSK) */
const UINT8 g_wpa_rsn_ie[22] = {
    0x30, 20, 1, 0,
    0x00, 0x0F, 0xAC, 4,                  /* групповой шифр: CCMP */
    1, 0, 0x00, 0x0F, 0xAC, 4,            /* парный: CCMP */
    1, 0, 0x00, 0x0F, 0xAC, 2,            /* ключи: PSK */
    0, 0                                  /* возможности */
};

static UINT16 be16(const UINT8 *p)
{
    return (UINT16)((p[0] << 8) | p[1]);
}

static void wpa_mic(const UINT8 kck[16], UINT8 *frame, UINTN len)
{
    UINT8 h[20];

    memset(frame + EK_MIC, 0, 16);
    hmac_sha1(kck, 16, frame, len, h);
    memcpy(frame + EK_MIC, h, 16);
}

static BOOLEAN wpa_mic_ok(const UINT8 kck[16], const UINT8 *frame, UINTN len)
{
    static UINT8 copy[512];
    UINT8 h[20];

    if (len > sizeof(copy))
        return FALSE;

    memcpy(copy, frame, len);
    memset(copy + EK_MIC, 0, 16);
    hmac_sha1(kck, 16, copy, len, h);

    UINT8 diff = 0;
    for (UINTN i = 0; i < 16; i++)
        diff |= (UINT8)(h[i] ^ frame[EK_MIC + i]);

    return diff == 0;
}

/* PTK = PRF-512(PMK, "Pairwise key expansion", min(AA,SPA) || max(AA,SPA)
   || min(ANonce,SNonce) || max(ANonce,SNonce)) */
void wpa_derive_ptk(const UINT8 pmk[32], const UINT8 aa[6], const UINT8 spa[6],
                    const UINT8 anonce[32], const UINT8 snonce[32], UINT8 ptk[48])
{
    UINT8 d[76];
    BOOLEAN a_first = memcmp(aa, spa, 6) < 0;
    BOOLEAN an_first = memcmp(anonce, snonce, 32) < 0;

    memcpy(d, a_first ? aa : spa, 6);
    memcpy(d + 6, a_first ? spa : aa, 6);
    memcpy(d + 12, an_first ? anonce : snonce, 32);
    memcpy(d + 44, an_first ? snonce : anonce, 32);

    wpa_prf(pmk, 32, "Pairwise key expansion", d, sizeof(d), ptk, 48);
}

void wpa_supp_init(WPA_SUPP *s, const UINT8 pmk[32], const UINT8 own_mac[6], const UINT8 ap_mac[6])
{
    memset(s, 0, sizeof(*s));
    memcpy(s->pmk, pmk, 32);
    memcpy(s->spa, own_mac, 6);
    memcpy(s->aa, ap_mac, 6);
    memcpy(s->rsn_ie, g_wpa_rsn_ie, sizeof(g_wpa_rsn_ie));
    s->rsn_ie_len = (UINT8)sizeof(g_wpa_rsn_ie);
    s->state = WPA_WAIT_M1;
}

/* Key Data: элементы (RSN IE AP, KDE). GTK KDE: dd len 00-0F-AC 01
   [номер ключа] [резерв] GTK. Нашли - в s->gtk */
static BOOLEAN wpa_find_gtk(WPA_SUPP *s, const UINT8 *kd, UINTN n)
{
    BOOLEAN found = FALSE;

    for (UINTN i = 0; i + 2 <= n; ) {

        UINT8 t = kd[i], l = kd[i + 1];

        if (t == 0xDD && l == 0)
            break;                        /* добивка */

        if (i + 2u + l > n)
            break;

        if (t == 0xDD && l >= 6 + 16 && kd[i + 2] == 0x00 && kd[i + 3] == 0x0F &&
            kd[i + 4] == 0xAC && kd[i + 5] == 1) {
            s->gtk_id = (UINT8)(kd[i + 6] & 3u);
            s->gtk_len = (UINT8)(l - 6 > 32 ? 32 : l - 6);
            memcpy(s->gtk, kd + i + 8, s->gtk_len);
            found = TRUE;
        }

        i += 2u + l;
    }

    return found;
}

/* Заголовок ответа EAPOL-Key (2/4, 4/4, 2/2) */
static void wpa_reply_head(UINT8 *out, UINTN n, UINT16 info, const UINT8 *replay)
{
    memset(out, 0, n);
    out[0] = 2;
    out[1] = 3;
    out[2] = (UINT8)((n - 4) >> 8);
    out[3] = (UINT8)(n - 4);
    out[4] = 2;
    out[EK_INFO] = (UINT8)(info >> 8);
    out[EK_INFO + 1] = (UINT8)info;
    memcpy(out + EK_REPLAY, replay, 8);
}

/*
 * Пришёл кадр EAPOL-Key от AP. Если нужно ответить - ответ в out
 * (*olen байт) и TRUE. После сообщения 3/4 s->state = WPA_DONE,
 * ключи - в s->ptk (TK = s->ptk + 32) и s->gtk.
 */
BOOLEAN wpa_supp_rx(WPA_SUPP *s, const UINT8 *f, UINTN len, UINT8 *out, UINTN *olen)
{
    if (len < EK_DATA || f[1] != 3 || f[4] != 2)
        return FALSE;

    UINT16 info = be16(f + EK_INFO);
    UINTN dlen = be16(f + EK_DLEN);

    if (EK_DATA + dlen > len || !(info & KI_ACK))
        return FALSE;

    /* счётчик повторов должен расти (защита от повторения старых кадров) */
    if (s->have_replay && memcmp(f + EK_REPLAY, s->replay, 8) <= 0)
        return FALSE;

    if (!(info & KI_PAIRWISE)) {

        /* --- групповое рукопожатие 1/2: точка раздаёт новый общий
               ключ (GTK) - обычно раз в час. Ответ 2/2 --- */
        if (s->state != WPA_DONE || !(info & KI_MIC) || !(info & KI_ENC_DATA))
            return FALSE;

        if (!wpa_mic_ok(s->ptk, f, len)) {
            s->mic_failures++;
            return FALSE;
        }

        if (dlen < 24 || dlen % 8 != 0 || dlen > 256)
            return FALSE;

        UINT8 kd[256];

        if (!aes_unwrap(s->ptk + 16, dlen / 8 - 1, f + EK_DATA, kd) ||
            !wpa_find_gtk(s, kd, dlen - 8))
            return FALSE;

        UINTN n = EK_DATA;

        wpa_reply_head(out, n, KI_MIC | KI_SECURE | KI_VER_2, f + EK_REPLAY);
        wpa_mic(s->ptk, out, n);

        memcpy(s->replay, f + EK_REPLAY, 8);
        s->gtk_new = TRUE;
        *olen = n;
        return TRUE;
    }

    if (!(info & KI_MIC)) {

        /* --- 1/4: ANonce. Наш SNonce, считаем PTK, отвечаем 2/4 --- */
        memcpy(s->anonce, f + EK_NONCE, 32);

        for (UINTN i = 0; i < 32; i += 4) {
            UINT32 r = net_random();
            memcpy(s->snonce + i, &r, 4);
        }

        wpa_derive_ptk(s->pmk, s->aa, s->spa, s->anonce, s->snonce, s->ptk);

        UINTN n = EK_DATA + s->rsn_ie_len;

        wpa_reply_head(out, n, KI_PAIRWISE | KI_MIC | KI_VER_2, f + EK_REPLAY);
        memcpy(out + EK_NONCE, s->snonce, 32);
        out[EK_DLEN] = 0;
        out[EK_DLEN + 1] = s->rsn_ie_len;
        memcpy(out + EK_DATA, s->rsn_ie, s->rsn_ie_len);
        wpa_mic(s->ptk, out, n);

        memcpy(s->replay, f + EK_REPLAY, 8);
        s->have_replay = TRUE;
        s->state = WPA_WAIT_M3;
        *olen = n;
        return TRUE;
    }

    /* --- 3/4: проверить подпись, достать GTK, ответить 4/4 --- */
    if (s->state != WPA_WAIT_M3 || !(info & KI_INSTALL) || !(info & KI_ENC_DATA) ||
        memcmp(f + EK_NONCE, s->anonce, 32) != 0)
        return FALSE;

    if (!wpa_mic_ok(s->ptk, f, len)) {
        s->mic_failures++;
        return FALSE;                     /* неверный пароль (PMK) - молчим */
    }

    if (dlen < 24 || dlen % 8 != 0 || dlen > 256)
        return FALSE;

    UINT8 kd[256];

    if (!aes_unwrap(s->ptk + 16, dlen / 8 - 1, f + EK_DATA, kd))
        return FALSE;

    wpa_find_gtk(s, kd, dlen - 8);

    UINTN n = EK_DATA;

    wpa_reply_head(out, n, KI_PAIRWISE | KI_MIC | KI_SECURE | KI_VER_2, f + EK_REPLAY);
    wpa_mic(s->ptk, out, n);

    memcpy(s->replay, f + EK_REPLAY, 8);
    s->state = WPA_DONE;
    s->gtk_new = TRUE;
    *olen = n;

    return TRUE;
}
