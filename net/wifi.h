/*
 * net/wifi.h - Wi-Fi (этап 8): криптография WPA2 и рукопожатие
 * (net/wpa.c), поиск адаптеров и команда wifi (net/wifi.c). Часть MyOS.
 */
#ifndef MYOS_WIFI_H
#define MYOS_WIFI_H

#include "net.h"

typedef struct {
    UINT32 h[5];
    UINT64 len;
    UINT8  buf[64];
    UINT32 n;
} SHA1_CTX;

typedef struct {
    UINT8 rk[176];            /* 11 ключей раундов */
} AES128;

void sha1_init(SHA1_CTX *c);
void sha1_update(SHA1_CTX *c, const void *data, UINTN len);
void sha1_final(SHA1_CTX *c, UINT8 out[20]);
void hmac_sha1(const UINT8 *key, UINTN klen, const UINT8 *data, UINTN len, UINT8 out[20]);
void hmac_sha1_v(const UINT8 *key, UINTN klen, UINTN n, const UINT8 *parts[],
                 const UINTN lens[], UINT8 out[20]);
void pbkdf2_sha1(const UINT8 *pass, UINTN plen, const UINT8 *salt, UINTN slen,
                 UINT32 iters, UINT8 *out, UINTN olen);
void wpa_prf(const UINT8 *key, UINTN klen, const char *label, const UINT8 *data,
             UINTN dlen, UINT8 *out, UINTN olen);
void aes128_init(AES128 *a, const UINT8 key[16]);
void aes128_encrypt(const AES128 *a, const UINT8 in[16], UINT8 out[16]);
void aes128_decrypt(const AES128 *a, const UINT8 in[16], UINT8 out[16]);
void aes_wrap(const UINT8 kek[16], UINTN n, const UINT8 *plain, UINT8 *out);
BOOLEAN aes_unwrap(const UINT8 kek[16], UINTN n, const UINT8 *cipher, UINT8 *plain);
BOOLEAN aes_ccm(const UINT8 key[16], const UINT8 nonce[13], const UINT8 *aad, UINTN aadlen,
                const UINT8 *in, UINTN len, UINT8 *out, UINT8 *tag, UINTN M, BOOLEAN encrypt);
UINTN ccmp_encrypt(const UINT8 tk[16], const UINT8 *hdr, UINTN hlen, UINT64 pn, UINT8 keyid,
                   const UINT8 *data, UINTN len, UINT8 *out);
INTN ccmp_decrypt(const UINT8 tk[16], const UINT8 *frame, UINTN flen, UINTN hlen,
                  UINT8 *out, UINT64 *pn_out);

/* сторона клиента 4-стороннего рукопожатия */
#define WPA_WAIT_M1   0
#define WPA_WAIT_M3   1
#define WPA_DONE      2

typedef struct {
    UINT8   state;
    UINT8   pmk[32];
    UINT8   aa[6], spa[6];        /* MAC точки доступа и наш */
    UINT8   anonce[32], snonce[32];
    UINT8   ptk[48];              /* KCK 16 | KEK 16 | TK 16 */
    UINT8   replay[8];
    BOOLEAN have_replay;
    UINT8   gtk[32];
    UINT8   gtk_len, gtk_id;
    UINT32  mic_failures;
} WPA_SUPP;

extern const UINT8 g_wpa_rsn_ie[22];

void wpa_derive_ptk(const UINT8 pmk[32], const UINT8 aa[6], const UINT8 spa[6],
                    const UINT8 anonce[32], const UINT8 snonce[32], UINT8 ptk[48]);
void wpa_supp_init(WPA_SUPP *s, const UINT8 pmk[32], const UINT8 own_mac[6], const UINT8 ap_mac[6]);
BOOLEAN wpa_supp_rx(WPA_SUPP *s, const UINT8 *f, UINTN len, UINT8 *out, UINTN *olen);

#endif
