/*
 * net/wifi.c - Wi-Fi: какой адаптер стоит в компьютере и что с ним
 * можно сделать; самопроверка безопасности WPA2. Часть MyOS; общие
 * объявления - в net/wifi.h.
 *
 * Wi-Fi в MyOS - три части:
 *   * этот файл: какой адаптер стоит в компьютере (таблица известных
 *     чипов) и команда wifi;
 *   * net/wpa.c - вся "безопасность" Wi-Fi, не зависящая от чипа:
 *     ключ из пароля (PBKDF2), 4-стороннее рукопожатие WPA2, шифрование
 *     кадров CCMP (AES); "wifi selftest" гоняет официальные тестовые
 *     векторы и рукопожатие с программной точкой доступа;
 *   * net/wlan.c + драйвер чипа (пока один: drivers/rtw8821c.c, Realtek
 *     RTL8821CE) - поиск сетей, подключение, интерфейс wlan0.
 *
 * Чип Wi-Fi - маленький компьютер со своей программой: драйвер грузит в
 * него прошивку производителя и тысячи регистров радиочасти из таблиц
 * калибровки, поэтому для каждого семейства чипов нужен свой большой
 * драйвер. Для остальных адаптеров выход в Интернет без кабеля - через
 * телефон: "USB-модем" (drivers/usbnet.c).
 */
#include "net.h"
#include "wifi.h"
#include "wlan.h"

/* Известные адаптеры Wi-Fi: PCI (встроенные) и USB (свистки) */
typedef struct {
    UINT16      vendor, device;
    const char *name;
    const char *linux_driver;   /* как это делает Linux - ориентир */
    UINT8       difficulty;     /* 1 - без прошивки (softMAC), 2 - прошивка
                                   + таблицы, 3 - сложная прошивка */
} WIFI_MODEL;

static const WIFI_MODEL g_wifi_pci[] = {
    { 0x8086, 0x9DF0, "Intel Wireless-AC 9560 / 9462 (CNVi)", "iwlwifi, firmware iwlwifi-9000-*.ucode", 3 },
    { 0x8086, 0xA370, "Intel Wireless-AC 9560 (CNVi)", "iwlwifi, firmware iwlwifi-9000-*.ucode", 3 },
    { 0x8086, 0x31DC, "Intel Wireless-AC 9560 / 9462 (CNVi)", "iwlwifi, firmware iwlwifi-9000-*.ucode", 3 },
    { 0x8086, 0x30DC, "Intel Wireless-AC 9560 (CNVi)", "iwlwifi, firmware iwlwifi-9000-*.ucode", 3 },
    { 0x8086, 0x02F0, "Intel Wi-Fi 6 AX201 / AC 9560 (CNVi)", "iwlwifi, firmware iwlwifi-QuZ-*.ucode", 3 },
    { 0x8086, 0x06F0, "Intel Wi-Fi 6 AX201 (CNVi)", "iwlwifi", 3 },
    { 0x8086, 0x34F0, "Intel Wi-Fi 6 AX201 (CNVi)", "iwlwifi", 3 },
    { 0x8086, 0xA0F0, "Intel Wi-Fi 6 AX201 (CNVi)", "iwlwifi", 3 },
    { 0x8086, 0x43F0, "Intel Wi-Fi 6 AX201 (CNVi)", "iwlwifi", 3 },
    { 0x8086, 0x51F0, "Intel Wi-Fi 6E AX211 (CNVi)", "iwlwifi", 3 },
    { 0x8086, 0x54F0, "Intel Wi-Fi 6E AX211 (CNVi)", "iwlwifi", 3 },
    { 0x8086, 0x7AF0, "Intel Wi-Fi 6E AX211 (CNVi)", "iwlwifi", 3 },
    { 0x8086, 0x2723, "Intel Wi-Fi 6 AX200", "iwlwifi, firmware iwlwifi-cc-a0-*.ucode", 3 },
    { 0x8086, 0x2725, "Intel Wi-Fi 6E AX210", "iwlwifi, firmware iwlwifi-ty-a0-gf-a0-*.ucode", 3 },
    { 0x8086, 0x2526, "Intel Wireless-AC 9260", "iwlwifi, firmware iwlwifi-9260-*.ucode", 3 },
    { 0x8086, 0x24FD, "Intel Wireless 8265", "iwlwifi, firmware iwlwifi-8265-*.ucode", 3 },
    { 0x8086, 0x24F3, "Intel Wireless 8260", "iwlwifi", 3 },
    { 0x8086, 0x24FB, "Intel Wireless 3168", "iwlwifi", 3 },
    { 0x8086, 0x3165, "Intel Wireless 3165", "iwlwifi", 3 },
    { 0x8086, 0x3166, "Intel Wireless 3165", "iwlwifi", 3 },
    { 0x8086, 0x095A, "Intel Wireless 7265", "iwlwifi", 3 },
    { 0x8086, 0x095B, "Intel Wireless 7265", "iwlwifi", 3 },
    { 0x8086, 0x08B1, "Intel Wireless 7260", "iwlwifi", 3 },
    { 0x8086, 0x08B2, "Intel Wireless 7260", "iwlwifi", 3 },
    { 0x10EC, 0xC821, "Realtek RTL8821CE (802.11ac)", "rtw88_8821ce, firmware rtw88/rtw8821c_fw.bin", 2 },
    { 0x10EC, 0xB822, "Realtek RTL8822BE (802.11ac)", "rtw88_8822be, firmware rtw88/rtw8822b_fw.bin", 2 },
    { 0x10EC, 0xC822, "Realtek RTL8822CE (802.11ac)", "rtw88_8822ce, firmware rtw88/rtw8822c_fw.bin", 2 },
    { 0x10EC, 0xD723, "Realtek RTL8723DE (802.11n)", "rtw88_8723de, firmware rtw88/rtw8723d_fw.bin", 2 },
    { 0x10EC, 0xB723, "Realtek RTL8723BE (802.11n)", "rtl8723be, firmware rtlwifi/rtl8723befw.bin", 2 },
    { 0x10EC, 0x8179, "Realtek RTL8188EE (802.11n)", "rtl8188ee, firmware rtlwifi/rtl8188efw.bin", 2 },
    { 0x10EC, 0x818B, "Realtek RTL8192EE (802.11n)", "rtl8192ee", 2 },
    { 0x10EC, 0x8821, "Realtek RTL8821AE (802.11ac)", "rtl8821ae", 2 },
    { 0x10EC, 0x8852, "Realtek RTL8852AE (Wi-Fi 6)", "rtw89_8852ae", 3 },
    { 0x10EC, 0xB852, "Realtek RTL8852BE (Wi-Fi 6)", "rtw89_8852be", 3 },
    { 0x10EC, 0xC852, "Realtek RTL8852CE (Wi-Fi 6E)", "rtw89_8852ce", 3 },
    { 0x168C, 0x0042, "Qualcomm Atheros QCA9377 (802.11ac)", "ath10k_pci, firmware ath10k/QCA9377", 3 },
    { 0x168C, 0x003E, "Qualcomm Atheros QCA6174 (802.11ac)", "ath10k_pci, firmware ath10k/QCA6174", 3 },
    { 0x168C, 0x0032, "Qualcomm Atheros AR9485 (802.11n)", "ath9k - NO firmware needed (softMAC)", 1 },
    { 0x168C, 0x0034, "Qualcomm Atheros AR9462 (802.11n)", "ath9k - NO firmware needed (softMAC)", 1 },
    { 0x168C, 0x0036, "Qualcomm Atheros AR9565 (802.11n)", "ath9k - NO firmware needed (softMAC)", 1 },
    { 0x168C, 0x002B, "Qualcomm Atheros AR9285 (802.11n)", "ath9k - NO firmware needed (softMAC)", 1 },
    { 0x168C, 0x0030, "Qualcomm Atheros AR93xx (802.11n)", "ath9k - NO firmware needed (softMAC)", 1 },
    { 0x17CB, 0x1101, "Qualcomm QCA6390 (Wi-Fi 6)", "ath11k_pci", 3 },
    { 0x17CB, 0x1103, "Qualcomm WCN6855 (Wi-Fi 6E)", "ath11k_pci", 3 },
    { 0x14C3, 0x7961, "MediaTek MT7921 (Wi-Fi 6)", "mt7921e, firmware mediatek/WIFI_RAM_CODE_MT7961*.bin", 3 },
    { 0x14C3, 0x0616, "MediaTek MT7922 (Wi-Fi 6E)", "mt7921e", 3 },
    { 0x14E4, 0x4365, "Broadcom BCM43142 (802.11n)", "wl (closed driver only)", 3 },
    { 0x14E4, 0x43B1, "Broadcom BCM4352 (802.11ac)", "wl / brcmfmac", 3 },
    { 0x14E4, 0x43A0, "Broadcom BCM4360 (802.11ac)", "wl", 3 },
    { 0x14E4, 0x4727, "Broadcom BCM4313 (802.11n)", "brcmsmac", 2 },
};

static const WIFI_MODEL g_wifi_usb[] = {
    { 0x0CF3, 0x9271, "Atheros AR9271 USB", "ath9k_htc - OPEN firmware (the easiest for a hobby OS)", 1 },
    { 0x0CF3, 0x7015, "Atheros AR7010 USB", "ath9k_htc - open firmware", 1 },
    { 0x148F, 0x5370, "Ralink RT5370 USB", "rt2800usb, firmware rt2870.bin", 2 },
    { 0x148F, 0x5372, "Ralink RT5372 USB", "rt2800usb", 2 },
    { 0x148F, 0x7601, "MediaTek MT7601U USB", "mt7601u, firmware mt7601u.bin", 2 },
    { 0x0BDA, 0x8179, "Realtek RTL8188EUS USB", "rtl8xxxu / r8188eu", 2 },
    { 0x0BDA, 0x8176, "Realtek RTL8188CUS USB", "rtl8xxxu", 2 },
    { 0x0BDA, 0xF179, "Realtek RTL8188FTV USB", "rtl8xxxu", 2 },
    { 0x0BDA, 0xB812, "Realtek RTL88x2BU USB", "rtw88_8822bu", 2 },
    { 0x0BDA, 0xC811, "Realtek RTL8811CU USB", "rtw88_8821cu", 2 },
    { 0x0BDA, 0xC820, "Realtek RTL8821CU USB", "rtw88_8821cu", 2 },
    { 0x2357, 0x010C, "TP-Link TL-WN722N v2/v3 (RTL8188EUS)", "rtl8xxxu", 2 },
};

typedef struct {
    BOOLEAN     usb;
    UINT8       bus, dev, fn;
    UINT16      vendor, device;
    const WIFI_MODEL *model;
} WIFI_FOUND;

static WIFI_FOUND g_wifi_found[4];
static UINTN g_wifi_n = 0;

/* При запуске (и по команде wifi): найти Wi-Fi на PCI. PCI-класс
   0x0280 - "другой сетевой контроллер" - почти всегда Wi-Fi */
void wifi_scan_pci(void)
{
    g_wifi_n = 0;

    for (UINTN nth = 0; nth < 8 && g_wifi_n < 4; nth++) {

        UINT8 b, d, f;

        if (!pci_find_class(0x02, 0x80, -1, nth, &b, &d, &f))
            break;

        UINT32 id = pci_config_read32(b, d, f, 0);
        WIFI_FOUND *w = &g_wifi_found[g_wifi_n++];

        w->usb = FALSE;
        w->bus = b;
        w->dev = d;
        w->fn = f;
        w->vendor = (UINT16)id;
        w->device = (UINT16)(id >> 16);
        w->model = NULL;

        for (UINTN i = 0; i < sizeof(g_wifi_pci) / sizeof(g_wifi_pci[0]); i++)
            if (g_wifi_pci[i].vendor == w->vendor && g_wifi_pci[i].device == w->device)
                w->model = &g_wifi_pci[i];

        klog("wifi: PCI %u:%u.%u %04x:%04x - %s\n", b, d, f, w->vendor, w->device,
             w->model ? w->model->name : "unknown wireless adapter");
    }
}

static BOOLEAN wifi_has_driver(UINT16 vendor, UINT16 device)
{
    return vendor == 0x10EC && (device == 0xC821 || device == 0xB821);
}

/* Строка в журнал загрузки: какой Wi-Fi нашёлся */
void wifi_boot_report(SIMPLE_TEXT_OUTPUT_INTERFACE *out)
{
    for (UINTN i = 0; i < g_wifi_n; i++) {

        const WIFI_FOUND *w = &g_wifi_found[i];

        if (wifi_has_driver(w->vendor, w->device))
            kprintf(out, "  Wi-Fi: %s (%04x:%04x) - 'wifi scan', then\n"
                         "         'wifi connect <network> <password>'\n",
                    w->model ? w->model->name : "adapter", w->vendor, w->device);
        else
            kprintf(out, "  Wi-Fi: %s (%04x:%04x) - no radio driver yet, see 'wifi';\n"
                         "         internet without a cable: USB tethering from a phone\n",
                    w->model ? w->model->name : "unknown adapter", w->vendor, w->device);
    }
}

/* ================================================================
 * Самопроверка: тестовые векторы + рукопожатие с "точкой доступа"
 * ================================================================ */

static BOOLEAN hex_eq(const UINT8 *got, const char *hex, UINTN n)
{
    for (UINTN i = 0; i < n; i++) {

        UINT8 v = 0;

        for (UINTN k = 0; k < 2; k++) {
            char c = hex[2 * i + k];
            v = (UINT8)(v << 4);
            v |= (UINT8)((c >= '0' && c <= '9') ? c - '0' :
                         (c >= 'a' && c <= 'f') ? c - 'a' + 10 : c - 'A' + 10);
        }

        if (got[i] != v)
            return FALSE;
    }

    return TRUE;
}

static UINTN g_st_pass, g_st_fail;

static void st_report(SIMPLE_TEXT_OUTPUT_INTERFACE *out, const char *what, BOOLEAN ok)
{
    kprintf(out, "  %-58s %s\n", what, ok ? "OK" : "FAILED");

    if (ok)
        g_st_pass++;
    else
        g_st_fail++;
}

/* Программная точка доступа: сообщения 1/4 и 3/4, как у hostapd */
static UINTN ap_build(UINT8 *f, UINT16 info, UINT64 replay, const UINT8 nonce[32],
                      const UINT8 *kd, UINTN kdlen, const UINT8 *kck)
{
    UINTN n = 99 + kdlen;

    memset(f, 0, n);
    f[0] = 2;
    f[1] = 3;
    f[2] = (UINT8)((n - 4) >> 8);
    f[3] = (UINT8)(n - 4);
    f[4] = 2;
    f[5] = (UINT8)(info >> 8);
    f[6] = (UINT8)info;
    f[8] = 16;                                    /* длина ключа CCMP */
    for (UINTN i = 0; i < 8; i++)
        f[9 + i] = (UINT8)(replay >> (56u - 8u * i));
    memcpy(f + 17, nonce, 32);
    f[97] = (UINT8)(kdlen >> 8);
    f[98] = (UINT8)kdlen;
    memcpy(f + 99, kd, kdlen);

    if (kck) {
        UINT8 h[20];
        hmac_sha1(kck, 16, f, n, h);
        memcpy(f + 81, h, 16);
    }

    return n;
}

static BOOLEAN ap_check_mic(const UINT8 *kck, const UINT8 *f, UINTN n)
{
    static UINT8 c[512];
    UINT8 h[20];

    memcpy(c, f, n);
    memset(c + 81, 0, 16);
    hmac_sha1(kck, 16, c, n, h);

    return memcmp(h, f + 81, 16) == 0;
}

static void wifi_selftest(SIMPLE_TEXT_OUTPUT_INTERFACE *out)
{
    UINT8 h[64];
    SHA1_CTX c;

    g_st_pass = g_st_fail = 0;

    print(out, "Wi-Fi security (WPA2-PSK, CCMP) self-test - official test vectors:\n");

    sha1_init(&c);
    sha1_update(&c, "abc", 3);
    sha1_final(&c, h);
    st_report(out, "SHA-1(\"abc\")                      (FIPS 180)",
              hex_eq(h, "a9993e364706816aba3e25717850c26c9cd0d89d", 20));

    UINT8 k0b[20];
    memset(k0b, 0x0b, 20);
    hmac_sha1(k0b, 20, (const UINT8 *)"Hi There", 8, h);
    st_report(out, "HMAC-SHA1 test 1                  (RFC 2202)",
              hex_eq(h, "b617318655057264e28bc0b6fb378c8ef146be00", 20));

    wpa_prf(k0b, 20, "prefix", (const UINT8 *)"Hi There", 8, h, 64);
    st_report(out, "PRF-512 test 1                    (IEEE 802.11i H.3)",
              hex_eq(h, "bcd4c650b30b9684951829e0d75f9d54b862175ed9f00606e17d8da35402ffee"
                        "75df78c3d31e0f889f012120c0862beb67753e7439ae242edb8373698356cf5a", 64));

    pbkdf2_sha1((const UINT8 *)"password", 8, (const UINT8 *)"IEEE", 4, 4096, h, 32);
    st_report(out, "PSK from \"password\" / SSID \"IEEE\"  (IEEE 802.11i H.4)",
              hex_eq(h, "f42c6fc52df0ebef9ebb4b90b38a5f902e83fe1b135a70e23aed762e9710a12e", 32));

    pbkdf2_sha1((const UINT8 *)"ThisIsAPassword", 15, (const UINT8 *)"ThisIsASSID", 11, 4096, h, 32);
    st_report(out, "PSK from \"ThisIsAPassword\"         (IEEE 802.11i H.4)",
              hex_eq(h, "0dc0d6eb90555ed6419756b9a15ec3e3209b63df707dd508d14581f8982721af", 32));

    {
        static const UINT8 key[16] = { 0,1,2,3,4,5,6,7,8,9,10,11,12,13,14,15 };
        static const UINT8 pt[16] = { 0x00,0x11,0x22,0x33,0x44,0x55,0x66,0x77,
                                      0x88,0x99,0xaa,0xbb,0xcc,0xdd,0xee,0xff };
        AES128 a;
        UINT8 ct[16], back[16];
        aes128_init(&a, key);
        aes128_encrypt(&a, pt, ct);
        aes128_decrypt(&a, ct, back);
        st_report(out, "AES-128 encrypt + decrypt         (FIPS 197 C.1)",
                  hex_eq(ct, "69c4e0d86a7b0430d8cdb78070b4c55a", 16) && memcmp(back, pt, 16) == 0);

        UINT8 w[24], u[16];
        static const UINT8 kd[16] = { 0x00,0x11,0x22,0x33,0x44,0x55,0x66,0x77,
                                      0x88,0x99,0xAA,0xBB,0xCC,0xDD,0xEE,0xFF };
        aes_wrap(key, 2, kd, w);
        BOOLEAN uw = aes_unwrap(key, 2, w, u);
        st_report(out, "AES Key Wrap / Unwrap             (RFC 3394 4.1)",
                  hex_eq(w, "1fa68b0a8112b447aef34bd8fb5a7b829d3e862371d2cfe5", 24) && uw &&
                  memcmp(u, kd, 16) == 0);
    }

    {
        UINT8 key[16], nonce[13] = { 0,0,0,3,2,1,0,0xA0,0xA1,0xA2,0xA3,0xA4,0xA5 };
        UINT8 aad[8], p[23], o[23], tag[8], back[23];
        for (UINTN i = 0; i < 16; i++) key[i] = (UINT8)(0xC0 + i);
        for (UINTN i = 0; i < 8; i++) aad[i] = (UINT8)i;
        for (UINTN i = 0; i < 23; i++) p[i] = (UINT8)(8 + i);
        aes_ccm(key, nonce, aad, 8, p, 23, o, tag, 8, TRUE);
        BOOLEAN vec = hex_eq(o, "588c979a61c663d2f066d0c2c0f989806d5f6b61dac384", 23) &&
                      hex_eq(tag, "17e8d12cfdf926e0", 8);
        BOOLEAN dec = aes_ccm(key, nonce, aad, 8, o, 23, back, tag, 8, FALSE);
        tag[0] ^= 1;                                        /* испорченная подпись */
        BOOLEAN bad = aes_ccm(key, nonce, aad, 8, o, 23, back, tag, 8, FALSE);
        st_report(out, "AES-CCM packet vector #1          (RFC 3610)",
                  vec && dec && !bad && memcmp(back, p, 23) == 0);
    }

    {
        /* CCMP: кадр данных, эталон посчитан pycryptodome (AES.MODE_CCM) */
        static const UINT8 tk[16] = { 0xc9,0x7c,0x1f,0x67,0xce,0x37,0x11,0x85,
                                      0x51,0x4a,0x8a,0x19,0xf2,0xbd,0xd5,0x2f };
        UINT8 hdr[24] = { 0x08, 0x02, 0, 0,
                          0x02,0x13,0xb3,0xaa,0x01,0x02,
                          0x50,0x30,0x1d,0x1b,0x42,0xb3,
                          0x50,0x30,0x1d,0x1b,0x42,0xb3,
                          0x30, 0x01 };
        UINT8 data[38] = { 0xaa,0xaa,0x03,0x00,0x00,0x00,0x08,0x00,0x45,0x00,0x00,0x54,
                           0x00,0x00,0x40,0x00,0x40,0x01 };
        for (UINTN i = 0; i < 20; i++) data[18 + i] = (UINT8)i;
        static UINT8 frame[128], plain[128];
        UINTN n = ccmp_encrypt(tk, hdr, 24, 0x7e0d0c0b0a09ull, 0, data, 38, frame);
        BOOLEAN vec = n == 24 + 8 + 38 + 8 &&
                      hex_eq(frame + 32, "9702454531d948200f0dfb4e9f6717cbbd2fb4627e4c9034a562"
                                         "1a90421a35c66e3c1150ad94", 38) &&
                      hex_eq(frame + 70, "21aa8c0323e26473", 8);
        UINT64 pn = 0;
        INTN dl = ccmp_decrypt(tk, frame, n, 24, plain, &pn);
        frame[40] ^= 0x80;                                  /* испортить бит */
        INTN bad = ccmp_decrypt(tk, frame, n, 24, plain + 64, NULL);
        st_report(out, "CCMP encrypt/decrypt of a data frame (802.11 AES-CCM)",
                  vec && dl == 38 && pn == 0x7e0d0c0b0a09ull && memcmp(plain, data, 38) == 0 &&
                  bad < 0);
    }

    /* --- полное 4-стороннее рукопожатие: наш клиент <-> программная AP --- */
    {
        static const UINT8 ap_mac[6] = { 0x02, 0x00, 0x00, 0xAA, 0xBB, 0xCC };
        static const UINT8 my_mac[6] = { 0x02, 0x4D, 0x59, 0x4F, 0x53, 0x01 };
        UINT8 pmk[32], anonce[32], ptk_ap[48], gtk[16];
        static UINT8 m[512], r[512];
        UINTN rl = 0;
        WPA_SUPP s;

        pbkdf2_sha1((const UINT8 *)"myos-secret", 11, (const UINT8 *)"MyOS-Test", 9, 4096, pmk, 32);

        for (UINTN i = 0; i < 32; i++) anonce[i] = (UINT8)(0x40 + i * 3);
        for (UINTN i = 0; i < 16; i++) gtk[i] = (UINT8)(0xF0 - i);

        wpa_supp_init(&s, pmk, my_mac, ap_mac);

        /* 1/4 */
        UINTN n = ap_build(m, 0x0002 | 0x0008 | 0x0080, 1, anonce, NULL, 0, NULL);
        BOOLEAN ok = wpa_supp_rx(&s, m, n, r, &rl);

        /* AP: из 2/4 - SNonce, свой PTK, проверка подписи клиента */
        wpa_derive_ptk(pmk, ap_mac, my_mac, anonce, r + 17, ptk_ap);
        ok = ok && ap_check_mic(ptk_ap, r, rl) && memcmp(ptk_ap, s.ptk, 48) == 0 &&
             r[99] == 0x30;                              /* RSN IE клиента */

        /* 3/4: RSN IE + GTK KDE, зашифровано KEK */
        UINT8 kd[48], wk[56];
        UINTN kl = 0;
        memcpy(kd, g_wpa_rsn_ie, sizeof(g_wpa_rsn_ie));
        kl = sizeof(g_wpa_rsn_ie);
        kd[kl++] = 0xDD; kd[kl++] = 22;
        kd[kl++] = 0x00; kd[kl++] = 0x0F; kd[kl++] = 0xAC; kd[kl++] = 0x01;
        kd[kl++] = 0x01; kd[kl++] = 0x00;
        memcpy(kd + kl, gtk, 16);
        kl += 16;
        if (kl % 8) {                                   /* добивка: DD 00 00 ... */
            kd[kl++] = 0xDD;
            while (kl % 8)
                kd[kl++] = 0;
        }
        aes_wrap(ptk_ap + 16, kl / 8, kd, wk);

        n = ap_build(m, 0x0002 | 0x0008 | 0x0040 | 0x0080 | 0x0100 | 0x0200 | 0x1000,
                     2, anonce, wk, kl + 8, ptk_ap);
        BOOLEAN ok3 = wpa_supp_rx(&s, m, n, r, &rl);

        ok = ok && ok3 && s.state == WPA_DONE && ap_check_mic(ptk_ap, r, rl) &&
             s.gtk_len == 16 && memcmp(s.gtk, gtk, 16) == 0 && s.gtk_id == 1;
        st_report(out, "4-way handshake with a software access point", ok);

        /* групповое рукопожатие: точка раздаёт новый GTK (номер 2) */
        UINT8 gtk2[16];
        for (UINTN i = 0; i < 16; i++) gtk2[i] = (UINT8)(0x11 * i + 3);
        kl = 0;
        kd[kl++] = 0xDD; kd[kl++] = 22;
        kd[kl++] = 0x00; kd[kl++] = 0x0F; kd[kl++] = 0xAC; kd[kl++] = 0x01;
        kd[kl++] = 0x02; kd[kl++] = 0x00;
        memcpy(kd + kl, gtk2, 16);
        kl += 16;
        aes_wrap(ptk_ap + 16, kl / 8, kd, wk);
        s.gtk_new = FALSE;
        n = ap_build(m, 0x0002 | 0x0080 | 0x0100 | 0x0200 | 0x1000, 3, anonce, wk, kl + 8, ptk_ap);
        BOOLEAN g = wpa_supp_rx(&s, m, n, r, &rl);
        st_report(out, "group key update (GTK rekey, message 1/2 -> 2/2)",
                  g && s.gtk_new && s.gtk_id == 2 && memcmp(s.gtk, gtk2, 16) == 0 &&
                  ap_check_mic(ptk_ap, r, rl) && !(r[6] & 0x08));

        /* неверный пароль: подпись 3/4 не сходится - клиент молчит */
        WPA_SUPP bad;
        UINT8 wrong[32];
        pbkdf2_sha1((const UINT8 *)"wrong-pass", 10, (const UINT8 *)"MyOS-Test", 9, 4096, wrong, 32);
        wpa_supp_init(&bad, wrong, my_mac, ap_mac);
        n = ap_build(m, 0x0002 | 0x0008 | 0x0080, 1, anonce, NULL, 0, NULL);
        wpa_supp_rx(&bad, m, n, r, &rl);
        n = ap_build(m, 0x0002 | 0x0008 | 0x0040 | 0x0080 | 0x0100 | 0x0200 | 0x1000,
                     2, anonce, wk, kl + 8, ptk_ap);
        BOOLEAN answered = wpa_supp_rx(&bad, m, n, r, &rl);
        st_report(out, "wrong password is detected (MIC mismatch)",
                  !answered && bad.mic_failures == 1 && bad.state != WPA_DONE);
    }

    kprintf(out, "Result: %u passed, %u failed. %s\n", (UINT32)g_st_pass, (UINT32)g_st_fail,
            g_st_fail == 0 ? "wifi selftest: OK" : "wifi selftest: FAILED");
}

/* ================================================================
 * Команда wifi
 * ================================================================ */

static void wifi_show_saved(SIMPLE_TEXT_OUTPUT_INTERFACE *out);

static void wifi_list(SIMPLE_TEXT_OUTPUT_INTERFACE *out)
{
    UINTN n = 0;

    wifi_scan_pci();

    print(out, "Wi-Fi adapters in this computer:\n");

    for (UINTN i = 0; i < g_wifi_n; i++) {

        WIFI_FOUND *w = &g_wifi_found[i];

        kprintf(out, "  PCI %u:%u.%u  %04x:%04x  %s\n", w->bus, w->dev, w->fn, w->vendor,
                w->device, w->model ? w->model->name : "unknown wireless adapter");

        if (w->model)
            kprintf(out, "      in Linux: %s\n", w->model->linux_driver);
        n++;
    }

    /* USB-свистки */
    for (UINTN i = 0; i < KX_MAX_DEVS; i++) {

        KX_DEV *d = &g_kx_devs[i];

        if (!d->used)
            continue;

        for (UINTN k = 0; k < sizeof(g_wifi_usb) / sizeof(g_wifi_usb[0]); k++)
            if (g_wifi_usb[k].vendor == d->vid && g_wifi_usb[k].device == d->pid) {
                kprintf(out, "  USB %04x:%04x  %s\n      in Linux: %s\n", d->vid, d->pid,
                        g_wifi_usb[k].name, g_wifi_usb[k].linux_driver);
                n++;
            }
    }

    if (n == 0)
        print(out, "  none found (PCI class 0x0280 or a known USB Wi-Fi stick)\n");

    BOOLEAN supported = FALSE;

    for (UINTN i = 0; i < g_wifi_n; i++)
        if (wifi_has_driver(g_wifi_found[i].vendor, g_wifi_found[i].device))
            supported = TRUE;

    if (wlan_present()) {
        print(out, "\n");
        wlan_cmd_status(out, FALSE);
        wifi_show_saved(out);
        print(out, "Commands: wifi scan | wifi connect <name> [password] | wifi disconnect |\n"
                   "          wifi save | wifi forget | wifi debug | wifi selftest\n");
        return;
    }

    if (supported) {
        print(out,
              "\nMyOS has a radio driver for this adapter (Realtek RTL8821CE):\n"
              "  wifi scan                        - list networks around\n"
              "  wifi connect <name> <password>   - connect (WPA2 or open network)\n"
              "  wifi save                        - remember it: connect at every start\n"
              "  wifi disconnect | wifi forget | wifi debug | wifi selftest\n");
        wifi_show_saved(out);
        return;
    }

    print(out,
          "\nStatus: MyOS has no radio driver for these chips yet. A Wi-Fi chip is a small\n"
          "computer of its own: it needs the vendor's closed firmware file loaded into it\n"
          "and thousands of calibration registers - a separate big driver per chip family.\n"
          "Ready and tested: the chip-independent part - WPA2 keys from the password,\n"
          "the 4-way handshake and CCMP (AES) encryption: 'wifi selftest'.\n"
          "\nInternet without a cable TODAY: connect a phone by USB and turn on\n"
          "'USB tethering' (Android: Settings - Network - Hotspot - USB tethering).\n"
          "The phone's Wi-Fi or mobile data becomes interface usb0 (see ifconfig).\n");
}

/* Слово из строки: до пробела или в кавычках ("My Home WiFi").
   Возвращает начало следующего слова */
static const char *wifi_word(const char *s, char *out, UINTN cap)
{
    UINTN n = 0;

    while (*s == ' ')
        s++;

    if (*s == '"') {
        s++;
        while (*s && *s != '"') {
            if (n + 1 < cap)
                out[n++] = *s;
            s++;
        }
        if (*s == '"')
            s++;
    } else {
        while (*s && *s != ' ') {
            if (n + 1 < cap)
                out[n++] = *s;
            s++;
        }
    }

    out[n] = '\0';

    while (*s == ' ')
        s++;

    return s;
}

/* ================================================================
 * Запомненная сеть (этап 9): файл wifi.cfg в папке EFI/MyOS тома, с
 * которого загрузились (kernel/settings.c). Пишется ТОЛЬКО командой
 * wifi save. Хранится не пароль, а ключ PMK (64 шестнадцатеричные
 * цифры, как psk= у wpa_supplicant): пароль из него не восстановить,
 * но к этой сети по нему подключиться можно - файл надо беречь так же,
 * как пароль.
 * ================================================================ */

#define WIFI_CFG "wifi.cfg"

typedef struct {
    char    ssid[33];
    BOOLEAN has_pass;
    UINT8   pmk[32];
} WIFI_SAVED;

static INT32 hexval(char c)
{
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'a' && c <= 'f') return c - 'a' + 10;
    if (c >= 'A' && c <= 'F') return c - 'A' + 10;
    return -1;
}

/* Прочитать wifi.cfg: строки "ssid=..." и "psk=<64 hex>" или "psk=open" */
static BOOLEAN wifi_load_saved(WIFI_SAVED *w, char *where, UINTN cap)
{
    char buf[512];
    UINTN got = 0;

    if (!settings_path(WIFI_CFG, where, cap))
        return FALSE;

    if (vfs_read_file(where, buf, sizeof(buf) - 1, &got) != VFS_OK)
        return FALSE;

    buf[got] = '\0';

    BOOLEAN have_ssid = FALSE, have_psk = FALSE;
    char *line = buf;

    w->ssid[0] = '\0';
    w->has_pass = FALSE;

    while (*line) {

        char *end = line;
        while (*end && *end != '\n' && *end != '\r')
            end++;
        char save = *end;
        *end = '\0';

        if (line[0] == 's' && line[1] == 's' && line[2] == 'i' && line[3] == 'd' && line[4] == '=') {
            UINTN n = 0;
            for (const char *c = line + 5; *c && n < 32; c++)
                w->ssid[n++] = *c;
            w->ssid[n] = '\0';
            have_ssid = n > 0;
        } else if (line[0] == 'p' && line[1] == 's' && line[2] == 'k' && line[3] == '=') {
            const char *h = line + 4;
            if (kstreq(h, "open")) {
                have_psk = TRUE;
            } else {
                UINTN i = 0;
                for (; i < 32; i++) {
                    INT32 a = hexval(h[2 * i]), b = (a >= 0) ? hexval(h[2 * i + 1]) : -1;
                    if (a < 0 || b < 0)
                        break;
                    w->pmk[i] = (UINT8)(a * 16 + b);
                }
                if (i == 32) {
                    w->has_pass = TRUE;
                    have_psk = TRUE;
                }
            }
        }

        *end = save;
        line = end;
        while (*line == '\n' || *line == '\r')
            line++;
    }

    return have_ssid && have_psk;
}

static void wifi_show_saved(SIMPLE_TEXT_OUTPUT_INTERFACE *out)
{
    WIFI_SAVED w;
    char where[96];

    if (wifi_load_saved(&w, where, sizeof(where)))
        kprintf(out, "Saved network: '%s' (%s) - connects at start; 'wifi connect' alone\n"
                     "uses it now.\n", w.ssid, where);
}

static void wifi_cmd_save(SIMPLE_TEXT_OUTPUT_INTERFACE *out, BOOLEAN confirmed)
{
    WIFI_SAVED w;

    if (!wlan_current(w.ssid, sizeof(w.ssid), w.pmk, &w.has_pass)) {
        print(out, "Connect first ('wifi connect <name> <password>'), then 'wifi save'.\n");
        return;
    }

    /* MyOS загружена с внутреннего диска (рядом с Arch), а его она
       держит только для чтения - спросить разрешения */
    char where[96];

    if (settings_readonly() && !confirmed) {
        settings_path(WIFI_CFG, where, sizeof(where));
        kprintf(out, "MyOS booted from the computer's internal disk, which it keeps read-only\n"
                     "(your other system lives there). Saving writes ONE file into MyOS's own\n"
                     "folder: %s\n"
                     "To allow that, type:  wifi save confirm\n", where);
        return;
    }

    char text[256];
    UINTN n = ksnprintf(text, sizeof(text),
                        "# MyOS: saved Wi-Fi network ('wifi save'; 'wifi forget' deletes this file)\n"
                        "ssid=%s\npsk=", w.ssid);

    if (w.has_pass) {
        for (UINTN i = 0; i < 32 && n + 3 < sizeof(text); i++)
            n += ksnprintf(text + n, sizeof(text) - n, "%02x", w.pmk[i]);
    } else {
        n += ksnprintf(text + n, sizeof(text) - n, "open");
    }

    n += ksnprintf(text + n, sizeof(text) - n, "\n");

    INTN r = settings_write(WIFI_CFG, text, n, where, sizeof(where));

    if (r == -1) {
        print(out, "Cannot find the disk MyOS booted from (MyOS keeps settings next to its\n"
                   "kernel, in EFI/MyOS). Nothing was written.\n");
        return;
    }

    if (r != VFS_OK) {
        kprintf(out, "Could not write %s: %s\n", where, vfs_strerror(r));
        return;
    }

    kprintf(out, "Saved network '%s' to %s\n", w.ssid, where);
    print(out, "MyOS will connect to it by itself at every start. The file holds the WPA\n"
               "key, not the password text - keep it private. 'wifi forget' deletes it.\n");
}

static void wifi_cmd_forget(SIMPLE_TEXT_OUTPUT_INTERFACE *out)
{
    char where[96];
    INTN r = settings_remove(WIFI_CFG, where, sizeof(where));

    if (r == VFS_OK)
        kprintf(out, "Forgot the saved network (%s deleted).\n", where);
    else
        print(out, "No saved network.\n");
}

/* При загрузке (kmain, после сети): есть запомненная сеть - включить
   Wi-Fi и подключаться в фоне, не задерживая загрузку */
void wifi_boot_autoconnect(SIMPLE_TEXT_OUTPUT_INTERFACE *out)
{
    WIFI_SAVED w;
    char where[96];

    if (!wifi_load_saved(&w, where, sizeof(where)))
        return;

    if (!wlan_hw_available()) {
        kprintf(out, "  saved Wi-Fi network '%s' - but no Wi-Fi adapter to use it\n", w.ssid);
        return;
    }

    kprintf(out, "  saved Wi-Fi network '%s' - connecting in the background\n", w.ssid);
    wlan_cmd_connect_key(out, w.ssid, w.has_pass ? w.pmk : NULL, FALSE);
}

void kernel_cmd_wifi(SIMPLE_TEXT_OUTPUT_INTERFACE *out, const char *arg)
{
    if (kstreq(arg, "save") || kstreq(arg, "save confirm")) {
        wifi_cmd_save(out, kstreq(arg, "save confirm"));
        return;
    }

    if (kstreq(arg, "forget")) {
        wifi_cmd_forget(out);
        return;
    }

    /* wifi connect без имени - к запомненной сети */
    if (kstreq(arg, "connect")) {
        WIFI_SAVED w;
        char where[96];
        if (wifi_load_saved(&w, where, sizeof(where))) {
            kprintf(out, "Saved network '%s' (%s)\n", w.ssid, where);
            wlan_cmd_connect_key(out, w.ssid, w.has_pass ? w.pmk : NULL, TRUE);
            return;
        }
    }

    if (kstreq(arg, "selftest") || kstreq(arg, "test")) {
        wifi_selftest(out);
        return;
    }

    if (arg[0] == 'p' && arg[1] == 's' && arg[2] == 'k' && arg[3] == ' ') {

        /* wifi psk <сеть> <пароль> - ключ PMK, как wpa_passphrase */
        const char *ssid = arg + 4;
        const char *sp = ssid;

        while (*sp && *sp != ' ')
            sp++;

        UINTN sl = (UINTN)(sp - ssid);
        const char *pass = (*sp == ' ') ? sp + 1 : sp;
        UINTN pl = 0;

        while (pass[pl])
            pl++;

        if (sl == 0 || sl > 32 || pl < 8 || pl > 63) {
            print(out, "usage: wifi psk <network name> <password 8..63 chars>\n");
            return;
        }

        UINT8 pmk[32];
        UINT64 t0 = net_now_ms();

        pbkdf2_sha1((const UINT8 *)pass, pl, (const UINT8 *)ssid, sl, 4096, pmk, 32);

        print(out, "network={\n\tssid=\"");
        for (UINTN i = 0; i < sl; i++)
            kprintf(out, "%c", ssid[i]);
        print(out, "\"\n\tpsk=");
        for (UINTN i = 0; i < 32; i++)
            kprintf(out, "%02x", pmk[i]);
        kprintf(out, "\n}\n(PBKDF2-SHA1, 4096 rounds: %llu ms)\n", net_now_ms() - t0);
        return;
    }

    if (kstreq(arg, "scan")) {
        wlan_cmd_scan(out);
        return;
    }

    if (arg[0] == 'c' && arg[1] == 'o' && arg[2] == 'n' && arg[3] == 'n' && arg[4] == 'e' &&
        arg[5] == 'c' && arg[6] == 't' && (arg[7] == ' ' || arg[7] == '\0')) {

        char name[64], pass[80];
        const char *rest = wifi_word(arg + 7, name, sizeof(name));

        wifi_word(rest, pass, sizeof(pass));

        if (name[0] == '\0') {
            print(out, "usage: wifi connect <network name> <password>\n"
                       "       wifi connect \"name with spaces\" <password>\n"
                       "       wifi connect <open network name>\n");
            return;
        }

        wlan_cmd_connect(out, name, pass[0] ? pass : NULL);
        return;
    }

    if (kstreq(arg, "sim")) {

        /* программная точка доступа "MyOS-Test" - для автотеста в QEMU */
        kmutex_lock(&g_net_mutex);

        if (wlan_present()) {
            print(out, "Wi-Fi is already running (see 'wifi').\n");
        } else {
            WLAN_HW *hw = NULL;
            if (wlan_sim_attach(out, &hw))
                wlan_use_hw(out, hw);
        }

        kmutex_unlock(&g_net_mutex);
        return;
    }

    if (kstreq(arg, "disconnect") || kstreq(arg, "off")) {
        wlan_cmd_disconnect(out);
        return;
    }

    if (kstreq(arg, "debug") || kstreq(arg, "status")) {

        if (!wlan_present()) {
            /* ещё не включали - включить (шаги видны на экране) */
            kmutex_lock(&g_net_mutex);
            BOOLEAN ok = wlan_up(out);
            kmutex_unlock(&g_net_mutex);
            if (!ok)
                return;
        }

        wlan_cmd_status(out, kstreq(arg, "debug"));
        return;
    }

    if (arg[0] != '\0') {
        print(out, "usage: wifi | wifi scan | wifi connect <name> [password] | wifi disconnect\n"
                   "       wifi save | wifi forget | wifi connect   (the saved network)\n"
                   "       wifi debug | wifi selftest | wifi psk <network> <password>\n");
        return;
    }

    wifi_list(out);
}
