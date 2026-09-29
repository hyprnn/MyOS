/*
 * net/wlan.h - Wi-Fi-клиент MyOS (этап 8): связь между драйвером
 * радиочипа (drivers/rtw8821c.c) и "802.11-частью" (net/wlan.c).
 * Часть MyOS.
 *
 * Кто что делает:
 *   драйвер чипа  - включает чип, грузит прошивку, настраивает радио,
 *                   переключает каналы, отправляет и принимает кадры
 *                   802.11 как есть (с заголовком 802.11, без FCS);
 *   net/wlan.c    - сам протокол Wi-Fi: поиск сетей, подключение
 *                   (authentication, association), WPA2 (net/wpa.c),
 *                   шифрование CCMP, превращение кадров 802.11 в
 *                   Ethernet и обратно для остального сетевого стека
 *                   (интерфейс wlan0).
 * Всё - под g_net_mutex.
 */
#ifndef MYOS_WLAN_H
#define MYOS_WLAN_H

#include "net.h"

typedef struct WLAN_HW WLAN_HW;

/* флаги отправки */
#define WLAN_TX_MGMT     0x01u   /* кадр управления: своя очередь, низшая скорость */
#define WLAN_TX_LOWRATE  0x02u   /* данные, но самой надёжной скоростью (EAPOL) */

struct WLAN_HW {
    const char *driver;          /* "rtw8821c" */
    const char *model;           /* "Realtek RTL8821CE (802.11ac)" */
    UINT8       mac[6];

    /* канал 1..13 (2,4 ГГц) или 36..165 (5 ГГц), ширина 20 МГц */
    BOOLEAN (*set_channel)(WLAN_HW *hw, UINT8 ch);
    /* кадр 802.11 без FCS (контрольную сумму добавит чип) */
    BOOLEAN (*tx)(WLAN_HW *hw, const UINT8 *frame, UINTN len, UINT32 flags);
    /* режим: поиск сетей (принимать маяки всех сетей) / подключение
       к точке bssid (NULL - никакой) */
    void    (*set_scan)(WLAN_HW *hw, BOOLEAN scanning);
    void    (*set_bssid)(WLAN_HW *hw, const UINT8 *bssid);
    /* подключились (aid - номер от точки, rates - маска скоростей
       точки: биты 0..3 - 1/2/5,5/11 Мбит/с, 4..11 - 6..54) / отключились */
    void    (*set_link)(WLAN_HW *hw, BOOLEAN up, UINT16 aid, UINT32 rates);
    /* забрать принятое (из потока net): для каждого кадра - wlan_rx */
    void    (*poll)(WLAN_HW *hw);
    /* сила сигнала последних кадров от точки (для прошивки чипа) */
    void    (*report_rssi)(WLAN_HW *hw, INT32 rssi_dbm);
    /* подробности для "wifi" и "wifi debug" */
    void    (*info)(WLAN_HW *hw, SIMPLE_TEXT_OUTPUT_INTERFACE *out, BOOLEAN debug);

    void       *priv;
};

/* Драйвер -> wlan: принят кадр 802.11 (без FCS); rssi - в дБм,
   channel - на каком канале стоял приёмник */
void wlan_rx(WLAN_HW *hw, const UINT8 *frame, UINTN len, INT32 rssi_dbm, UINT8 channel);

/* Драйверы чипов: включить адаптер PCI bus:dev.fn (печатая шаги в out)
   и отдать его описание */
BOOLEAN rtw8821c_attach(UINT8 bus, UINT8 dev, UINT8 fn, SIMPLE_TEXT_OUTPUT_INTERFACE *out,
                        WLAN_HW **hw_out);

/* Программная точка доступа для автотеста (net/wlan_sim.c) */
BOOLEAN wlan_sim_attach(SIMPLE_TEXT_OUTPUT_INTERFACE *out, WLAN_HW **hw_out);

/* net/wlan.c: команды wifi (вызываются из net/wifi.c) */
BOOLEAN wlan_up(SIMPLE_TEXT_OUTPUT_INTERFACE *out);
BOOLEAN wlan_use_hw(SIMPLE_TEXT_OUTPUT_INTERFACE *out, WLAN_HW *hw);
void    wlan_cmd_scan(SIMPLE_TEXT_OUTPUT_INTERFACE *out);
void    wlan_cmd_connect(SIMPLE_TEXT_OUTPUT_INTERFACE *out, const char *ssid, const char *pass);
void    wlan_cmd_disconnect(SIMPLE_TEXT_OUTPUT_INTERFACE *out);
void    wlan_cmd_status(SIMPLE_TEXT_OUTPUT_INTERFACE *out, BOOLEAN debug);
BOOLEAN wlan_present(void);

#endif
