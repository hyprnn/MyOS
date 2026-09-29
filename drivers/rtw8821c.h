/*
 * drivers/rtw8821c.h - общие объявления драйвера Wi-Fi Realtek
 * RTL8821CE (drivers/rtw8821c.c) и его таблиц
 * (drivers/rtw8821c_table.c). Часть MyOS.
 *
 * Таблицы и порядок включения чипа взяты из драйвера Linux rtw88
 * (двойная лицензия GPL-2.0 OR BSD-3-Clause, MyOS берёт BSD-3-Clause;
 * см. third_party/README.md).
 */
#ifndef MYOS_RTW8821C_H
#define MYOS_RTW8821C_H

#include "../myos.h"

/* Пара "мощность по скорости" (PG): какие скорости какой добавкой
   к базовой мощности передавать. band 0 - 2,4 ГГц, 1 - 5 ГГц */
typedef struct {
    UINT32 band;
    UINT32 rf_path;
    UINT32 tx_num;
    UINT32 addr;
    UINT32 bitmask;
    UINT32 data;
} RTW_PG_PAIR;

/* Предел мощности (LMT): для страны regd, диапазона band, ширины
   канала bw, группы скоростей rs и канала ch - не больше txpwr_lmt
   (в единицах индекса мощности, 0,5 дБ) */
typedef struct {
    UINT8 regd;
    UINT8 band;
    UINT8 bw;
    UINT8 rs;
    UINT8 ch;
    INT8  txpwr_lmt;
} RTW_LMT_PAIR;

/* Таблицы: пары "адрес, значение" UINT32 с условиями (см. rtw_load_table) */
extern const UINT32 rtw8821c_mac[];
extern const UINTN  rtw8821c_mac_n;
extern const UINT32 rtw8821c_agc[];
extern const UINTN  rtw8821c_agc_n;
extern const UINT32 rtw8821c_agc_btg_type2[];
extern const UINTN  rtw8821c_agc_btg_type2_n;
extern const UINT32 rtw8821c_bb[];
extern const UINTN  rtw8821c_bb_n;
extern const UINT32 rtw8821c_rf_a[];
extern const UINTN  rtw8821c_rf_a_n;
extern const RTW_PG_PAIR  rtw8821c_bb_pg_type0[];
extern const UINTN  rtw8821c_bb_pg_type0_n;
extern const RTW_LMT_PAIR rtw8821c_txpwr_lmt_type0[];
extern const UINTN  rtw8821c_txpwr_lmt_type0_n;

/* Прошивка чипа (firmware/rtw88/rtw8821c_fw.bin, вклеена в ядро
   файлом firmware/firmware.S) */
extern const UINT8 g_fw_rtw8821c[];
extern const UINT8 g_fw_rtw8821c_end[];

#endif
