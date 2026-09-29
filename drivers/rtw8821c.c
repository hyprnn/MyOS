/*
 * drivers/rtw8821c.c - драйвер Wi-Fi-чипа Realtek RTL8821CE (PCI
 * 10ec:c821 и 10ec:b821; 802.11ac, одна антенна, 2,4 и 5 ГГц, вместе с
 * Bluetooth в одном корпусе). Часть MyOS.
 *
 * Откуда знания. Документации на чип Realtek не публикует; всё ниже -
 * перенос драйвера Linux rtw88 (drivers/net/wireless/realtek/rtw88:
 * pci.c, mac.c, fw.c, efuse.c, phy.c, coex.c, rtw8821c.c; двойная
 * лицензия GPL-2.0 OR BSD-3-Clause, MyOS берёт BSD-3-Clause - см.
 * third_party/README.md). Порядок регистров и числа - как там; где
 * MyOS упрощает - сказано в комментарии.
 *
 * Чип - это маленький компьютер:
 *   * свой процессор (ядро "3081") с прошивкой rtw8821c_fw.bin
 *     (firmware/, 136 КиБ) - прошивку при каждом включении грузит
 *     драйвер (rtw_fw_download); прошивка ведёт выбор скорости передачи
 *     (rate adaptation), калибровку (IQK) и отвечает на команды "H2C"
 *     (host to chip: почтовые ящики HMEBOX и пакеты в очереди H2C);
 *   * MAC - "почтальон": очереди кадров, повторы, подтверждения ACK;
 *   * BB (baseband) и RF (радио) - модем и приёмопередатчик: тысячи
 *     регистров, которые задают таблицы Realtek (drivers/rtw8821c_table.c);
 *   * eFuse - прожжённая на заводе память: MAC-адрес, калибровка
 *     мощности, вариант платы (RFE: как разведены антенны).
 *
 * Обмен с процессором компьютера - через кольца дескрипторов в памяти
 * (как у проводной карты, drivers/rtl8169.c): 8 колец отправки (очереди
 * BK/BE/VI/VO/маяк/управление/high/H2C) и одно кольцо приёма. Каждая
 * ячейка кольца отправки - два "дескриптора буфера" по 8 байт: первый
 * указывает на 48-байтный дескриптор кадра (скорость, очередь...),
 * второй - на сам кадр. Адреса - 32-битные: всё ниже 4 ГиБ.
 *
 * Прерывания MyOS не включает: поток net каждые 2 мс забирает принятое
 * (rtw_poll) - для Wi-Fi со скоростями в десятки Мбит/с этого хватает.
 *
 * Bluetooth в том же чипе делит с Wi-Fi антенну. MyOS Bluetooth не
 * использует, поэтому антенна всегда отдана Wi-Fi (режим "только
 * Wi-Fi" из coex.c) - rtw_coex_*.
 */
#include "../net/net.h"
#include "../net/wlan.h"
#include "rtw8821c.h"

/* ================================================================
 * Регистры (имена - как в rtw88/reg.h)
 * ================================================================ */

#define REG_SYS_CTRL            0x0000
#define REG_SYS_FUNC_EN         0x0002
#define BIT_FEN_PCIEA           (1u << 6)
#define BIT_FEN_CPUEN           (1u << 2)
#define BIT_FEN_BB_GLB_RST      (1u << 1)
#define BIT_FEN_BB_RSTB         (1u << 0)
#define REG_SYS_PW_CTRL         0x0004
#define BIT_PFM_WOWL            (1u << 3)
#define REG_SYS_CLK_CTRL        0x0008
#define BIT_CPU_CLK_EN          (1u << 14)
#define REG_RSV_CTRL            0x001C
#define BIT_WLMCU_IOIF          (1u << 0)
#define REG_RF_CTRL             0x001F
#define BIT_RF_SDM_RSTB         (1u << 2)
#define BIT_RF_RSTB             (1u << 1)
#define BIT_RF_EN               (1u << 0)
#define REG_AFE_CTRL1           0x0024
#define REG_AFE_XTAL_CTRL       0x0024
#define REG_AFE_PLL_CTRL        0x0028
#define REG_EFUSE_CTRL          0x0030
#define BIT_EF_FLAG             (1u << 31)
#define REG_LDO_EFUSE_CTRL      0x0034
#define REG_GPIO_MUXCFG         0x0040
#define BIT_FSPI_EN             (1u << 19)
#define BIT_PO_BT_PTA_PINS      (1u << 9)
#define BIT_BT_PTA_EN           (1u << 5)
#define BIT_WLRFE_4_5_EN        (1u << 2)
#define REG_LED_CFG             0x004C
#define BIT_LNAON_SEL_EN        (1u << 26)
#define BIT_PAPE_SEL_EN         (1u << 25)
#define BIT_DPDT_WL_SEL         (1u << 24)
#define BIT_DPDT_SEL_EN         (1u << 23)
#define REG_PAD_CTRL1           0x0064
#define BIT_PAPE_WLBT_SEL       (1u << 29)
#define BIT_LNAON_WLBT_SEL      (1u << 28)
#define BIT_BTGP_JTAG_EN        (1u << 24)
#define BIT_BTGP_SPI_EN         (1u << 20)
#define BIT_LED1DIS             (1u << 15)
#define REG_CTRL_TYPE           0x0067
#define REG_SYS_SDIO_CTRL       0x0070
#define BIT_DBG_GNT_WL_BT       (1u << 27)
#define BIT_LTE_MUX_CTRL_PATH   (1u << 26)
#define BIT_SDIO_INT            (1u << 18)
#define REG_HCI_OPT_CTRL        0x0074
#define BIT_USB_SUS_DIS         (1u << 8)
#define REG_MCUFW_CTRL          0x0080
#define BIT_BOOT_FSPI_EN        (1u << 20)
#define BIT_FW_DW_RDY           (1u << 14)
#define BIT_DMEM_CHKSUM_OK      (1u << 6)
#define BIT_DMEM_DW_OK          (1u << 5)
#define BIT_IMEM_CHKSUM_OK      (1u << 4)
#define BIT_IMEM_DW_OK          (1u << 3)
#define BIT_MCUFWDL_EN          (1u << 0)
#define BIT_CHECK_SUM_OK        (BIT_IMEM_CHKSUM_OK | BIT_DMEM_CHKSUM_OK)
#define FW_READY                0xC078u    /* INIT_RDY | DW_RDY | IMEM/DMEM DW+CHKSUM OK */
#define FW_READY_MASK           0xCFFFu
#define REG_WIFI_BT_INFO        0x00AA
#define BIT_BT_INT_EN           (1u << 15)
#define REG_WLRF1               0x00EC
#define BIT_WLRF1_BBRF_EN       (7u << 24)
#define REG_SYS_CFG1            0x00F0
#define BIT_RF_TYPE_ID          (1u << 27)
#define REG_CR                  0x0100
#define BIT_ENSWBCN             (1u << 8)
#define MAC_TRX_ENABLE          0xFFu
#define REG_TXDMA_PQ_MAP        0x010C
#define REG_TRXFF_BNDY          0x0114
#define REG_RXFF_BNDY           0x011C
#define REG_C2HEVT              0x01A0
#define REG_HMETFR              0x01CC
#define REG_HMEBOX0             0x01D0
#define REG_HMEBOX0_EX          0x01F0
#define REG_FIFOPAGE_CTRL_2     0x0204
#define BIT_BCN_VALID_V1        (1u << 15)
#define REG_AUTO_LLT_V1         0x0208
#define REG_TXDMA_OFFSET_CHK    0x020C
#define REG_TXDMA_STATUS        0x0210
#define REG_RQPN_CTRL_2         0x022C
#define BIT_LD_RQPN             (1u << 31)
#define REG_FIFOPAGE_INFO_1     0x0230
#define REG_H2C_HEAD            0x0244
#define REG_H2C_TAIL            0x0248
#define REG_H2C_READ_ADDR       0x024C
#define REG_H2C_INFO            0x0254
#define REG_FWHW_TXQ_CTRL       0x0420
#define REG_BCNQ_BDNY_V1        0x0424
#define REG_CCK_CHECK           0x0454
#define REG_AMPDU_MAX_TIME_V1   0x0455
#define REG_BCNQ1_BDNY_V1       0x0456
#define REG_TX_HANG_CTRL        0x045E
#define REG_INIRTS_RATE_SEL     0x0480
#define REG_DATA_SC             0x0483
#define REG_QUEUE_CTRL          0x04C6
#define BIT_PTA_WL_TX_EN        (1u << 4)
#define BIT_PTA_EDCCA_EN        (1u << 5)
#define REG_PROT_MODE_CTRL      0x04C8
#define REG_BAR_MODE_CTRL       0x04CC
#define REG_PRECNT_CTRL         0x04E5
#define REG_EDCA_VO_PARAM       0x0500
#define REG_EDCA_VI_PARAM       0x0504
#define REG_PIFS                0x0512
#define REG_SIFS                0x0514
#define REG_SLOT                0x051B
#define REG_TX_PTCL_CTRL        0x0520
#define REG_TXPAUSE             0x0522
#define REG_TBTT_PROHIBIT       0x0540
#define REG_RD_NAV_NXT          0x0544
#define REG_BCN_CTRL            0x0550
#define BIT_DIS_TSF_UDT         (1u << 4)
#define BIT_EN_BCN_FUNCTION     (1u << 3)
#define REG_DRVERLYINT          0x0558
#define REG_BCNDMATIM           0x0559
#define REG_USTIME_TSF          0x055C
#define REG_RXTSF_OFFSET_CCK    0x055E
#define REG_TIMER0_SRC_SEL      0x05B4
#define REG_TCR                 0x0604
#define REG_RCR                 0x0608
#define BIT_APP_FCS             (1u << 31)
#define BIT_APP_MIC             (1u << 30)
#define BIT_APP_ICV             (1u << 29)
#define BIT_APP_PHYSTS          (1u << 28)
#define BIT_VHT_DACK            (1u << 26)
#define BIT_PKTCTL_DLEN         (1u << 20)
#define BIT_HTC_LOC_CTRL        (1u << 14)
#define BIT_CBSSID_BCN          (1u << 7)
#define BIT_CBSSID_DATA         (1u << 6)
#define BIT_AB                  (1u << 3)
#define BIT_AM                  (1u << 2)
#define BIT_APM                 (1u << 1)
#define REG_RX_PKT_LIMIT        0x060C
#define REG_RX_DRVINFO_SZ       0x060F
#define REG_MACID               0x0610
#define REG_BSSID               0x0618
#define REG_USTIME_EDCA         0x0638
#define REG_ACKTO_CCK           0x0639
#define REG_WMAC_TRXPTCL_CTL    0x0668
#define REG_WMAC_TRXPTCL_CTL_H  0x066C
#define REG_AID                 0x06A8
#define REG_RXFLTMAP0           0x06A0
#define REG_RXFLTMAP1           0x06A2
#define REG_RXFLTMAP2           0x06A4
#define REG_BT_COEX_TABLE0      0x06C0
#define REG_BT_COEX_TABLE1      0x06C4
#define REG_BT_COEX_BRK_TABLE   0x06C8
#define REG_BT_COEX_TABLE_H     0x06CC
#define REG_SND_PTCL_CTRL       0x0718
#define REG_BT_COEX_V2          0x0762
#define BIT_GNT_BT_POLARITY     (1u << 12)
#define REG_BT_STAT_CTRL        0x0778
#define REG_BT_TDMA_TIME        0x0790
#define REG_WMAC_OPTION_FUNCTION 0x07D0
#define REG_RXPSEL              0x0808
#define BIT_RX_PSEL_RST         (3u << 28)
#define REG_RXCCAMSK            0x0814
#define REG_CLKTRK              0x0860
#define REG_ADCCLK              0x08AC
#define REG_ADC160              0x08C4
#define REG_CHFIR               0x08F0
#define REG_ACBB0               0x0948
#define REG_ACBBRXFIR           0x094C
#define REG_FAS                 0x09A4
#define REG_CCA_FLTR            0x0A20
#define REG_TXSF2               0x0A24
#define REG_TXSF6               0x0A28
#define REG_RXDESC              0x0A2C
#define REG_CCK0_FAREPORT       0x0A2C
#define REG_FA_CCK              0x0A5C
#define REG_ENTXCCK             0x0A80
#define REG_ENRXCCA             0x0A84
#define REG_CSRATIO             0x0AAA
#define REG_TXFILTER            0x0AAC
#define REG_CNTRST              0x0B58
#define REG_TXSCALE_A           0x0C1C
#define REG_RXIGI_A             0x0C50
#define REG_TXDFIR              0x0C20
#define REG_LSSI_WRITE_A        0x0C90
#define REG_TXAGCIDX            0x0C94
#define REG_RFE_CTRL8           0x0CB4
#define REG_RFECTL              0x0CB8
#define B_BTG_SWITCH            (1u << 16)
#define B_CTRL_SWITCH           (1u << 18)
#define B_WL_SWITCH             ((1u << 20) | (1u << 22))
#define B_WLG_SWITCH            (1u << 21)
#define B_WLA_SWITCH            (1u << 23)
#define REG_FA_OFDM             0x0F48
#define REG_DMEM_CTRL           0x1080
#define REG_CPU_DMEM_CON        0x1080
#define BIT_WL_PLATFORM_RST     (1u << 16)
#define BIT_DDMA_EN             (1u << 8)
#define REG_H2C_PKT_READADDR    0x10D0
#define REG_H2C_PKT_WRITEADDR   0x10D4
#define REG_FW_DBG7             0x10FC
#define REG_CR_EXT              0x1100
#define REG_DDMA_CH0SA          0x1200
#define REG_DDMA_CH0DA          0x1204
#define REG_DDMA_CH0CTRL        0x1208
#define BIT_DDMACH0_OWN         (1u << 31)
#define BIT_DDMACH0_CHKSUM_EN   (1u << 29)
#define BIT_DDMACH0_CHKSUM_STS  (1u << 27)
#define BIT_DDMACH0_RESET_CHKSUM_STS (1u << 25)
#define BIT_DDMACH0_CHKSUM_CONT (1u << 24)
#define REG_H2CQ_CSR            0x1330
#define BIT_H2CQ_FULL           (1u << 31)
#define REG_FAST_EDCA_VOVI      0x1448
#define REG_FAST_EDCA_BEBK      0x144C
#define REG_LTECOEX_CTRL        0x1700
#define REG_LTECOEX_WDATA       0x1704
#define REG_LTECOEX_RDATA       0x1708
#define LTECOEX_READY           (1u << 29)
#define REG_TXAGC_BASE_A        0x1D00

/* Регистры PCI-части чипа (rtw88/pci.h) */
#define RTK_PCI_CTRL            0x0300
#define BIT_RST_TRXDMA_INTF     (1u << 20)
#define BIT_RX_TAG_EN           (1u << 15)
#define RTK_PCI_TXBD_BCN_WORK   0x0383
#define BIT_PCI_BCNQ_FLAG       (1u << 4)
#define RTK_PCI_RXBD_DESA_MPDUQ 0x0338
#define RTK_PCI_RXBD_NUM_MPDUQ  0x0382
#define RTK_PCI_RXBD_IDX_MPDUQ  0x03B4
#define RTK_PCI_TXBD_RWPTR_CLR  0x039C
#define RTK_PCI_TXBD_H2CQ_CSR   0x1330
#define RTK_PCI_HIMR0           0x00B0
#define RTK_PCI_HIMR1           0x00B8
#define RTK_PCI_HIMR3           0x10B8
#define REG_MDIO_V1             0x03F4
#define REG_PCIE_MIX_CFG        0x03F8

/* Радиочасть: регистры RF (адреса внутри RF) */
#define RF_DTXLOK               0x08
#define RF_CFGCH                0x18
#define RF_LUTWA                0x33
#define RF_LUTWD0               0x3F
#define RF_XTALX2               0xB8
#define RF_LUTDBG               0xDF
#define RF_LUTWE2               0xEE
#define RFREG_MASK              0xFFFFFu

/* Ячейки памяти процессора чипа для загрузки прошивки (mac.h) */
#define OCPBASE_TXBUF_88XX      0x18780000u
#define OCPBASE_DMEM_88XX       0x00200000u

/* Команды прошивке */
#define H2C_CMD_MEDIA_STATUS_RPT 0x01
#define H2C_CMD_DEFAULT_PORT    0x2C
#define H2C_CMD_RA_INFO         0x40
#define H2C_CMD_RSSI_MONITOR    0x42
#define H2C_CMD_RECOVER_BT_DEV  0xD1
#define H2C_PKT_GENERAL_INFO    0x0D
#define H2C_PKT_IQK             0x0E
#define H2C_PKT_PHYDM_INFO      0x11

/* Скорости в дескрипторах (DESC_RATE*) */
#define DESC_RATE1M             0x00
#define DESC_RATE11M            0x03
#define DESC_RATE6M             0x04
#define DESC_RATE54M            0x0B
#define DESC_RATEMCS0           0x0C
#define DESC_RATEMCS7           0x13
#define DESC_RATEVHT1SS_MCS0    0x2C
#define DESC_RATEVHT1SS_MCS9    0x35
#define DESC_RATE_NUM           0x36     /* нам нужны только 1 поток */

/* Группы скоростей (для мощности): CCK, OFDM, HT 1 поток, VHT 1 поток */
#define RS_CCK                  0
#define RS_OFDM                 1
#define RS_HT1                  2
#define RS_VHT1                 3
#define RS_NUM                  4

#define RATEID_BG               6
#define RATEID_G                7
#define RATEID_B_20M            8

/* ================================================================
 * Кольца
 * ================================================================ */

#define Q_BK   0
#define Q_BE   1
#define Q_VI   2
#define Q_VO   3
#define Q_BCN  4
#define Q_MGMT 5
#define Q_HI0  6
#define Q_H2C  7
#define Q_NUM  8

#define TX_DESC_SZ    48u        /* дескриптор кадра */
#define TX_BD_SZ      16u        /* два дескриптора буфера */
#define RX_DESC_SZ    24u
#define RX_BD_SZ      8u
#define RX_BUF_SZ     (11454u + 24u)   /* как в Linux: самый длинный кадр VHT + дескриптор */
#define RX_SLOT       12288u     /* 3 страницы на ячейку */
#define RX_RING_LEN   64u

typedef struct {
    UINT16 num_reg;              /* сколько ячеек (16 бит) */
    UINT16 desa_reg;             /* адрес кольца (32 бита) */
    UINT16 idx_reg;              /* индексы: мл. 12 бит - наш wp, ст. - rp чипа */
    UINT16 len;
    UINT16 slot;                 /* байт на ячейку (буфер кадра) */
} RTW_QDEF;

static const RTW_QDEF g_rtw_q[Q_NUM] = {
    [Q_BK]   = { 0x038A, 0x0330, 0x03AC, 4,  4096 },
    [Q_BE]   = { 0x0388, 0x0328, 0x03A8, 64, 4096 },
    [Q_VI]   = { 0x0386, 0x0320, 0x03A4, 4,  4096 },
    [Q_VO]   = { 0x0384, 0x0318, 0x03A0, 4,  4096 },
    [Q_BCN]  = { 0,      0x0308, 0,      1,  8192 },
    [Q_MGMT] = { 0x0380, 0x0310, 0x03B0, 32, 4096 },
    [Q_HI0]  = { 0x038C, 0x0340, 0x03B8, 4,  4096 },
    [Q_H2C]  = { 0x1328, 0x1320, 0x132C, 32, 4096 },
};

typedef struct {
    UINT64 bd;                   /* физ. адрес кольца дескрипторов буферов */
    UINT64 buf;                  /* физ. адрес буферов */
    UINT32 wp;                   /* куда пишем следующий кадр */
} RTW_TXR;

/* ================================================================
 * Состояние чипа
 * ================================================================ */

typedef struct {
    BOOLEAN   used;
    UINT8     bus, dev, fn;
    UINT64    bar;

    /* версия */
    UINT32    sys_cfg1;
    UINT8     cut;               /* ревизия кристалла: 0 - A, 1 - B, ... */

    /* eFuse */
    UINT8     efuse[512];        /* логическая карта */
    BOOLEAN   efuse_ok;
    UINT8     rfe;               /* вариант платы (RFE option) */
    UINT8     pkg;
    BOOLEAN   rfe_btg;           /* 2,4 ГГц идёт через антенну Bluetooth */
    UINT8     xtal;              /* подстройка кварца */
    UINT8     bt_setting;
    UINT8     board_opt;
    UINT8     swing_2g, swing_5g;
    BOOLEAN   btcoex, share_ant;
    UINT8     txpwr[42];         /* калибровка мощности, путь A */
    UINT8     mac[6];

    /* что сообщила прошивка о чипе */
    BOOLEAN   hwcap_ok;
    UINT8     hwcap_nss, hwcap_bw, hwcap_ant, hwcap_ptcl;

    /* прошивка */
    UINT16    fw_ver;
    UINT8     fw_sub;
    BOOLEAN   fw_running;
    UINT8     h2c_box;
    UINT16    h2c_seq;

    /* кольца */
    RTW_TXR   tx[Q_NUM];
    UINT64    rx_bd, rx_buf;
    UINT32    rx_rp;

    /* радио */
    UINT8     channel;
    UINT32    ch_param[3];       /* фильтры CCK по умолчанию (из таблиц) */
    UINT8     igi_default, igi;
    BOOLEAN   scanning;
    INT8      byrate_2g[DESC_RATE_NUM], byrate_5g[DESC_RATE_NUM];
    INT8      lmt_2g[RS_NUM][14], lmt_5g[RS_NUM][53];
    UINT8     pwr_idx[DESC_RATE_NUM];   /* последние записанные индексы мощности */

    /* связь */
    BOOLEAN   bssid_set;
    BOOLEAN   linked;
    UINT8     rate_id;
    UINT32    ra_mask;
    INT32     rssi;              /* дБм, для DIG */
    UINT64    last_dm_ms;

    /* статистика */
    UINT64    rx_frames, rx_crc, rx_c2h, rx_bad;
    UINT64    tx_frames, tx_full;
    UINT32    iqk_ms;
    BOOLEAN   iqk_ok;
    UINT32    last_fa;

    /* ход включения - для "wifi debug" */
    const char *stage;
    char      err[96];

    WLAN_HW   hw;
} RTW;

static RTW g_rtw;

/* ================================================================
 * Доступ к регистрам
 * ================================================================ */

static UINT8  r8(RTW *r, UINT32 o)  { return *(volatile UINT8 *)P2V(r->bar + o); }
static UINT16 r16(RTW *r, UINT32 o) { return *(volatile UINT16 *)P2V(r->bar + o); }
static UINT32 r32(RTW *r, UINT32 o) { return *(volatile UINT32 *)P2V(r->bar + o); }
static void w8(RTW *r, UINT32 o, UINT8 v)   { *(volatile UINT8 *)P2V(r->bar + o) = v; }
static void w16(RTW *r, UINT32 o, UINT16 v) { *(volatile UINT16 *)P2V(r->bar + o) = v; }
static void w32(RTW *r, UINT32 o, UINT32 v) { *(volatile UINT32 *)P2V(r->bar + o) = v; }

static void w8_set(RTW *r, UINT32 o, UINT8 b)   { w8(r, o, (UINT8)(r8(r, o) | b)); }
static void w8_clr(RTW *r, UINT32 o, UINT8 b)   { w8(r, o, (UINT8)(r8(r, o) & ~b)); }
static void w16_set(RTW *r, UINT32 o, UINT16 b) { w16(r, o, (UINT16)(r16(r, o) | b)); }
static void w32_set(RTW *r, UINT32 o, UINT32 b) { w32(r, o, r32(r, o) | b); }
static void w32_clr(RTW *r, UINT32 o, UINT32 b) { w32(r, o, r32(r, o) & ~b); }

static UINT32 r32_mask(RTW *r, UINT32 o, UINT32 mask)
{
    return (r32(r, o) & mask) >> __builtin_ctz(mask);
}

static void w32_mask(RTW *r, UINT32 o, UINT32 mask, UINT32 v)
{
    UINT32 sh = (UINT32)__builtin_ctz(mask);
    w32(r, o, (r32(r, o) & ~mask) | ((v << sh) & mask));
}

static void w8_mask(RTW *r, UINT32 o, UINT8 mask, UINT8 v)
{
    UINT32 sh = (UINT32)__builtin_ctz(mask);
    w8(r, o, (UINT8)((r8(r, o) & ~mask) | ((UINT32)(v << sh) & mask)));
}

static void udelay(UINT64 us) { tsc_delay_us(us); }
static void mdelay(UINT64 ms) { tsc_delay_us(ms * 1000u); }

/* Ждать, пока поле регистра (по маске) станет target: до 10 мс, как
   check_hw_ready в Linux */
static BOOLEAN hw_ready(RTW *r, UINT32 o, UINT32 mask, UINT32 target)
{
    for (UINTN i = 0; i < 1000; i++) {
        if (r32_mask(r, o, mask) == target)
            return TRUE;
        udelay(10);
    }
    return FALSE;
}

static BOOLEAN rtw_fail(RTW *r, const char *what)
{
    ksnprintf(r->err, sizeof(r->err), "%s", what);
    klog("rtw8821c: FAILED at %s: %s\n", r->stage ? r->stage : "?", what);
    return FALSE;
}

static UINT32 le32(const UINT8 *p)
{
    return (UINT32)p[0] | ((UINT32)p[1] << 8) | ((UINT32)p[2] << 16) | ((UINT32)p[3] << 24);
}

/* ================================================================
 * Радиочасть (RF): запись через "3-проводной" порт SIPI, чтение -
 * прямо из окна 0x2800 (rtw_phy_write_rf_reg_sipi / rtw_phy_read_rf)
 * ================================================================ */

static UINT32 rf_read(RTW *r, UINT32 addr, UINT32 mask)
{
    addr &= 0xFFu;
    return r32_mask(r, 0x2800u + (addr << 2), mask & RFREG_MASK);
}

static void rf_write(RTW *r, UINT32 addr, UINT32 mask, UINT32 data)
{
    addr &= 0xFFu;
    mask &= RFREG_MASK;

    if (mask != RFREG_MASK) {
        UINT32 old = rf_read(r, addr, RFREG_MASK);
        UINT32 sh = (UINT32)__builtin_ctz(mask);
        data = (old & ~mask) | ((data << sh) & mask);
    }

    w32(r, REG_LSSI_WRITE_A, ((addr << 20) | (data & 0xFFFFFu)) & 0x0FFFFFFFu);
    udelay(13);
}

/* ================================================================
 * Последовательности включения/выключения питания (rtw8821c.c:
 * card_enable_flow / card_disable_flow; оставлены шаги для PCIe)
 * ================================================================ */

#define PW_W  1          /* записать биты mask значением val */
#define PW_P  2          /* ждать, пока (reg & mask) == val */

typedef struct {
    UINT16 off;
    UINT8  cmd, mask, val;
} RTW_PWR;

static const RTW_PWR g_pwr_on[] = {
    /* выкл. -> "эмуляция карты" */
    { 0x0005, PW_W, 0x98, 0x00 },
    { 0x0300, PW_W, 0xFF, 0x00 },
    { 0x0301, PW_W, 0xFF, 0x00 },
    /* эмуляция -> работа */
    { 0x0005, PW_W, 0x1C, 0x00 },
    { 0x0075, PW_W, 0x01, 0x01 },
    { 0x0006, PW_P, 0x02, 0x02 },
    { 0x0075, PW_W, 0x01, 0x00 },
    { 0x0006, PW_W, 0x01, 0x01 },
    { 0x0005, PW_W, 0x80, 0x00 },
    { 0x0005, PW_W, 0x18, 0x00 },
    { 0x0005, PW_W, 0x01, 0x01 },
    { 0x0005, PW_P, 0x01, 0x00 },
    { 0x0020, PW_W, 0x08, 0x08 },
    { 0x0074, PW_W, 0x20, 0x20 },
    { 0x0022, PW_W, 0x02, 0x00 },
    { 0x0062, PW_W, 0xE0, 0xE0 },
    { 0x0061, PW_W, 0xE0, 0x00 },
    { 0x007C, PW_W, 0x02, 0x00 },
    { 0xFFFF, 0, 0, 0 }
};

static const RTW_PWR g_pwr_off[] = {
    /* работа -> эмуляция */
    { 0x0093, PW_W, 0x08, 0x00 },
    { 0x001F, PW_W, 0xFF, 0x00 },
    { 0x0049, PW_W, 0x02, 0x00 },
    { 0x0006, PW_W, 0x01, 0x01 },
    { 0x0002, PW_W, 0x02, 0x00 },
    { 0x0005, PW_W, 0x02, 0x02 },
    { 0x0005, PW_P, 0x02, 0x00 },
    { 0x0020, PW_W, 0x08, 0x00 },
    /* эмуляция -> выкл. */
    { 0x0067, PW_W, 0x20, 0x00 },
    { 0x0005, PW_W, 0x04, 0x04 },
    { 0x0081, PW_W, 0xC0, 0x00 },
    { 0x0090, PW_W, 0x02, 0x00 },
    { 0xFFFF, 0, 0, 0 }
};

static BOOLEAN pwr_poll(RTW *r, const RTW_PWR *c)
{
    for (UINTN i = 0; i < 20000; i++) {             /* 20000 x 50 мкс = 1 с */
        if ((r8(r, c->off) & c->mask) == (c->val & c->mask))
            return TRUE;
        udelay(50);
    }
    return FALSE;
}

static BOOLEAN rtw_pwr_seq(RTW *r, const RTW_PWR *seq)
{
    for (const RTW_PWR *c = seq; c->off != 0xFFFF; c++) {

        if (c->cmd == PW_W) {
            UINT8 v = r8(r, c->off);
            w8(r, c->off, (UINT8)((v & ~c->mask) | (c->val & c->mask)));
            continue;
        }

        if (pwr_poll(r, c))
            continue;

        /* PCIe: "толкнуть" конечный автомат питания (BIT_PFM_WOWL) и
           подождать ещё раз - так делает Linux */
        UINT8 v = r8(r, REG_SYS_PW_CTRL);
        w8(r, REG_SYS_PW_CTRL, (UINT8)(v | BIT_PFM_WOWL));
        w8(r, REG_SYS_PW_CTRL, (UINT8)(v & ~BIT_PFM_WOWL));

        if (!pwr_poll(r, c)) {
            char m[64];
            ksnprintf(m, sizeof(m), "power: register 0x%04x never became 0x%02x", c->off,
                      c->val & c->mask);
            return rtw_fail(r, m);
        }
    }

    return TRUE;
}

/* Перед включением: выводы антенн - к Wi-Fi/Bluetooth, модем и радио -
   в сброс (rtw_mac_pre_system_cfg) */
static void rtw_pre_system_cfg(RTW *r)
{
    w8(r, REG_RSV_CTRL, 0);
    w32_set(r, REG_HCI_OPT_CTRL, BIT_USB_SUS_DIS);

    w32_set(r, REG_PAD_CTRL1, BIT_PAPE_WLBT_SEL | BIT_LNAON_WLBT_SEL);
    w32_clr(r, REG_LED_CFG, BIT_PAPE_SEL_EN | BIT_LNAON_SEL_EN);
    w32_set(r, REG_GPIO_MUXCFG, BIT_WLRFE_4_5_EN);

    w8_clr(r, REG_SYS_FUNC_EN, BIT_FEN_BB_RSTB | BIT_FEN_BB_GLB_RST);
    w8_clr(r, REG_RF_CTRL, BIT_RF_SDM_RSTB | BIT_RF_RSTB | BIT_RF_EN);
    w32_clr(r, REG_WLRF1, BIT_WLRF1_BBRF_EN);
}

/* 1 - переключили, 0 - ошибка, 2 - уже было в этом состоянии */
static int rtw_power_switch(RTW *r, BOOLEAN on)
{
    /* прошивка ещё жива (после тёплого перезапуска)? - разбудить её
       сигналом RPWM, иначе она не даст выключить питание */
    UINT8 rpwm = r8(r, 0x03D9);

    if (r16(r, REG_MCUFW_CTRL) == FW_READY) {
        rpwm = (UINT8)((rpwm ^ 0x80u) & 0x80u);
        w8(r, 0x03D9, rpwm);
    }

    BOOLEAN cur = r8(r, REG_CR) != 0xEA;           /* 0xEA - "питание выключено" */

    if (cur == on)
        return 2;

    return rtw_pwr_seq(r, on ? g_pwr_on : g_pwr_off) ? 1 : 0;
}

static BOOLEAN rtw_mac_power_on(RTW *r)
{
    rtw_pre_system_cfg(r);

    int rc = rtw_power_switch(r, TRUE);

    if (rc == 2) {
        /* чип уже включён (прошивка UEFI или прошлый запуск) - выключить
           и включить заново, чтобы начать с чистого листа */
        rtw_power_switch(r, FALSE);
        rtw_pre_system_cfg(r);
        rc = rtw_power_switch(r, TRUE);
    }

    if (rc != 1)
        return FALSE;

    /* __rtw_mac_init_system_cfg */
    w32_set(r, REG_CPU_DMEM_CON, BIT_WL_PLATFORM_RST | BIT_DDMA_EN);
    w8_set(r, REG_SYS_FUNC_EN + 1, 0xD8);
    w8(r, REG_CR_EXT + 3, (UINT8)((r8(r, REG_CR_EXT + 3) & 0xF0u) | 0x0Cu));

    UINT32 t = r32(r, REG_MCUFW_CTRL);

    if (t & BIT_BOOT_FSPI_EN) {
        /* не загружаться с флеш-памяти SPI: прошивку дадим мы */
        w32(r, REG_MCUFW_CTRL, t & ~BIT_BOOT_FSPI_EN);
        w32_clr(r, REG_GPIO_MUXCFG, BIT_FSPI_EN);
    }

    return TRUE;
}

/* ================================================================
 * Кольца: адреса в чипе (rtw_pci_reset_buf_desc) и сброс DMA
 * ================================================================ */

static void rtw_hci_setup(RTW *r)
{
    w8(r, RTK_PCI_CTRL + 3, (UINT8)(r8(r, RTK_PCI_CTRL + 3) | 0xF7u));

    for (UINTN q = 0; q < Q_NUM; q++) {

        const RTW_QDEF *d = &g_rtw_q[q];

        r->tx[q].wp = 0;
        if (d->num_reg)
            w16(r, d->num_reg, (UINT16)(d->len & 0xFFFu));
        w32(r, d->desa_reg, (UINT32)r->tx[q].bd);
    }

    r->rx_rp = 0;
    w16(r, RTK_PCI_RXBD_NUM_MPDUQ, (UINT16)RX_RING_LEN);
    w32(r, RTK_PCI_RXBD_DESA_MPDUQ, (UINT32)r->rx_bd);

    /* сбросить указатели чтения/записи всех колец */
    w32(r, RTK_PCI_TXBD_RWPTR_CLR, 0xFFFFFFFFu);
    w32_set(r, RTK_PCI_TXBD_H2CQ_CSR, (1u << 16) | (1u << 8));

    /* сброс DMA и счётчика "тегов" приёма */
    w32_set(r, RTK_PCI_CTRL, BIT_RST_TRXDMA_INTF | BIT_RX_TAG_EN);
}

/* Вернуть ячейку приёма чипу: размер буфера и адрес */
static void rtw_rx_slot_reset(RTW *r, UINT32 i)
{
    volatile UINT32 *bd = (volatile UINT32 *)P2V(r->rx_bd + (UINT64)i * RX_BD_SZ);

    bd[1] = (UINT32)(r->rx_buf + (UINT64)i * RX_SLOT);
    bd[0] = RX_BUF_SZ;                    /* buf_size; total_pkt_size = 0 */
}

static BOOLEAN rtw_alloc_rings(RTW *r)
{
    for (UINTN q = 0; q < Q_NUM; q++) {

        const RTW_QDEF *d = &g_rtw_q[q];
        UINT64 bd_pages = ((UINT64)d->len * TX_BD_SZ + 4095u) / 4096u;
        UINT64 buf_pages = ((UINT64)d->len * d->slot + 4095u) / 4096u;

        r->tx[q].bd = pmm_alloc_zeroed(bd_pages, 0x100000000ull);
        r->tx[q].buf = pmm_alloc_zeroed(buf_pages, 0x100000000ull);

        if (!r->tx[q].bd || !r->tx[q].buf)
            return FALSE;
    }

    r->rx_bd = pmm_alloc_zeroed(1, 0x100000000ull);
    r->rx_buf = pmm_alloc_zeroed((UINT64)RX_RING_LEN * RX_SLOT / 4096u, 0x100000000ull);

    if (!r->rx_bd || !r->rx_buf)
        return FALSE;

    for (UINT32 i = 0; i < RX_RING_LEN; i++)
        rtw_rx_slot_reset(r, i);

    return TRUE;
}

/* ================================================================
 * Отправка: дескриптор кадра (rtw_tx_fill_tx_desc) и ячейка кольца
 * ================================================================ */

typedef struct {
    UINT32 size;          /* длина кадра */
    UINT8  offset;        /* где начинается кадр (48; для H2C - 0) */
    UINT8  qsel;          /* очередь внутри чипа */
    UINT8  mac_id;
    UINT8  rate_id;
    UINT8  rate;
    BOOLEAN bmc;          /* широковещательный */
    BOOLEAN ls;           /* последний кусок */
    BOOLEAN use_rate;     /* скорость задаём мы (иначе выбирает прошивка) */
    BOOLEAN dis_fb;       /* не снижать скорость при повторах */
    BOOLEAN hwseq;        /* номер кадра ставит чип */
} RTW_TXI;

#define QSEL_TID0     0
#define QSEL_BEACON   16
#define QSEL_HIGH     17
#define QSEL_MGMT     18
#define QSEL_H2C      19

static void rtw_fill_txdesc(UINT8 *p, const RTW_TXI *t)
{
    UINT32 w[12];

    memset(w, 0, sizeof(w));

    w[0] = (t->size & 0xFFFFu) | ((UINT32)t->offset << 16) | ((UINT32)(t->bmc ? 1 : 0) << 24) |
           ((UINT32)(t->ls ? 1 : 0) << 26) | ((UINT32)(t->hwseq ? 1 : 0) << 31);
    w[1] = t->mac_id | ((UINT32)(t->qsel & 0x1Fu) << 8) | ((UINT32)(t->rate_id & 0x1Fu) << 16) |
           ((UINT32)(t->qsel == QSEL_HIGH ? 1 : 0) << 29);
    /* hw_ssn_sel = 0: общий счётчик номеров кадров */
    w[3] = ((UINT32)(t->use_rate ? 1 : 0) << 8) | ((UINT32)(t->dis_fb ? 1 : 0) << 10);
    w[4] = t->rate & 0x7Fu;
    w[8] = (UINT32)(t->hwseq ? 1 : 0) << 15;

    for (UINTN i = 0; i < 12; i++) {
        p[i * 4 + 0] = (UINT8)w[i];
        p[i * 4 + 1] = (UINT8)(w[i] >> 8);
        p[i * 4 + 2] = (UINT8)(w[i] >> 16);
        p[i * 4 + 3] = (UINT8)(w[i] >> 24);
    }
}

/* Сколько ячеек свободно: одна всегда пустая (иначе "полно" = "пусто") */
static UINT32 ring_avail(UINT32 wp, UINT32 rp, UINT32 len)
{
    return rp > wp ? rp - wp - 1u : len - wp + rp - 1u;
}

static BOOLEAN rtw_tx_queue(RTW *r, UINTN q, const RTW_TXI *t, const UINT8 *data, UINTN len)
{
    const RTW_QDEF *d = &g_rtw_q[q];
    RTW_TXR *ring = &r->tx[q];

    if (len + TX_DESC_SZ > d->slot)
        return FALSE;

    if (q != Q_BCN) {

        /* свободна ли ячейка: чип сообщает, докуда дочитал (rp) */
        UINT32 rp = (r16(r, d->idx_reg + 2u)) & 0xFFFu;

        if (ring_avail(ring->wp, rp, d->len) == 0) {

            UINT64 end = rdtsc() + g_tsc_hz / 50u;         /* до 20 мс */

            while (ring_avail(ring->wp, rp, d->len) == 0 && rdtsc() < end) {
                cpu_pause();
                rp = (r16(r, d->idx_reg + 2u)) & 0xFFFu;
            }

            if (ring_avail(ring->wp, rp, d->len) == 0) {
                r->tx_full++;
                return FALSE;
            }
        }
    }

    UINT64 phys = ring->buf + (UINT64)ring->wp * d->slot;
    UINT8 *buf = (UINT8 *)P2V(phys);

    memset(buf, 0, TX_DESC_SZ);
    rtw_fill_txdesc(buf, t);
    memcpy(buf + TX_DESC_SZ, data, len);

    volatile UINT32 *bd = (volatile UINT32 *)P2V(ring->bd + (UINT64)ring->wp * TX_BD_SZ);
    UINT32 psb = ((UINT32)(len + TX_DESC_SZ) - 1u) / 128u + 1u;

    if (q == Q_BCN)
        psb |= 1u << 15;                     /* OWN: ячейка отдана чипу */

    bd[1] = (UINT32)phys;
    bd[0] = TX_DESC_SZ | (psb << 16);
    bd[3] = (UINT32)(phys + TX_DESC_SZ);
    bd[2] = (UINT32)len;

    __asm__ __volatile__("mfence" ::: "memory");

    if (q == Q_BCN) {
        /* маячная очередь: "поехали" - флаг в BCN_WORK */
        w8(r, RTK_PCI_TXBD_BCN_WORK, (UINT8)(r8(r, RTK_PCI_TXBD_BCN_WORK) | BIT_PCI_BCNQ_FLAG));
        return TRUE;
    }

    ring->wp = (ring->wp + 1u) % d->len;
    w16(r, d->idx_reg, (UINT16)(ring->wp & 0xFFFu));    /* "звонок" */

    return TRUE;
}

/* ================================================================
 * Команды прошивке (H2C)
 * ================================================================ */

/* Короткая команда (8 байт) через почтовые ящики HMEBOX0..3 */
static BOOLEAN rtw_h2c_cmd(RTW *r, const UINT8 h2c[8])
{
    UINT8 box = r->h2c_box;

    for (UINTN i = 0; i < 30; i++) {                       /* до 3 мс */
        if (!((r8(r, REG_HMETFR) >> box) & 1u))
            break;
        udelay(100);
    }

    if ((r8(r, REG_HMETFR) >> box) & 1u) {
        klog("rtw8821c: H2C mailbox %u busy - firmware does not answer\n", box);
        return FALSE;
    }

    w32(r, REG_HMEBOX0_EX + box * 4u, le32(h2c + 4));
    w32(r, REG_HMEBOX0 + box * 4u, le32(h2c));

    r->h2c_box = (UINT8)((box + 1u) & 3u);
    return TRUE;
}

/* Длинная команда (32 байта) - пакетом в очереди H2C */
static BOOLEAN rtw_h2c_pkt(RTW *r, UINT8 sub_id, const UINT8 *payload, UINTN plen)
{
    UINT8 p[32];
    RTW_TXI t;

    memset(p, 0, sizeof(p));
    p[0] = 0x01;                                  /* категория */
    p[1] = 0xFF;                                  /* команда: "пакетная" */
    p[2] = sub_id;
    p[4] = (UINT8)(8u + plen);                    /* общая длина */
    p[6] = (UINT8)r->h2c_seq;
    p[7] = (UINT8)(r->h2c_seq >> 8);
    memcpy(p + 8, payload, plen);
    r->h2c_seq++;

    memset(&t, 0, sizeof(t));
    t.size = sizeof(p);
    t.qsel = QSEL_H2C;

    return rtw_tx_queue(r, Q_H2C, &t, p, sizeof(p));
}

/* ================================================================
 * Прошивка: загрузка в память процессора чипа (mac.c)
 * ================================================================ */

/* Кусок (до 4 КиБ) - в "зарезервированную страницу" буфера чипа через
   маячную очередь (rtw_fw_write_data_rsvd_page) */
static BOOLEAN rtw_write_rsvd_page(RTW *r, UINT16 pg, const UINT8 *data, UINTN len)
{
    UINT8 bcn_ctrl = r8(r, REG_BCN_CTRL);

    w16(r, REG_FIFOPAGE_CTRL_2, (UINT16)((pg & 0xFFFu) | BIT_BCN_VALID_V1));

    UINT8 cr1 = r8(r, REG_CR + 1);
    w8(r, REG_CR + 1, (UINT8)(cr1 | (BIT_ENSWBCN >> 8)));
    w8(r, REG_BCN_CTRL, (UINT8)((bcn_ctrl & ~BIT_EN_BCN_FUNCTION) | BIT_DIS_TSF_UDT));

    UINT8 txq2 = r8(r, REG_FWHW_TXQ_CTRL + 2);
    w8(r, REG_FWHW_TXQ_CTRL + 2, (UINT8)(txq2 & ~(1u << 6)));    /* EN_BCNQ_DL выкл. */

    RTW_TXI t;
    memset(&t, 0, sizeof(t));
    t.size = (UINT32)len;
    t.offset = TX_DESC_SZ;
    t.qsel = QSEL_BEACON;
    t.ls = TRUE;
    t.hwseq = TRUE;
    t.use_rate = TRUE;
    t.dis_fb = TRUE;
    t.rate_id = RATEID_G;
    t.rate = DESC_RATE6M;
    t.bmc = len >= 10 && data[4] == 0xFF && data[5] == 0xFF && data[6] == 0xFF &&
            data[7] == 0xFF && data[8] == 0xFF && data[9] == 0xFF;

    BOOLEAN ok = rtw_tx_queue(r, Q_BCN, &t, data, len) &&
                 hw_ready(r, REG_FIFOPAGE_CTRL_2, BIT_BCN_VALID_V1, 1);

    /* вернуть как было */
    w16(r, REG_FIFOPAGE_CTRL_2, (UINT16)(0 | BIT_BCN_VALID_V1));
    w8(r, REG_BCN_CTRL, bcn_ctrl);
    w8(r, REG_FWHW_TXQ_CTRL + 2, txq2);
    w8(r, REG_CR + 1, cr1);

    return ok;
}

static BOOLEAN iddma_copy(RTW *r, UINT32 src, UINT32 dst, UINT32 len, BOOLEAN first)
{
    UINT32 ctrl = BIT_DDMACH0_CHKSUM_EN | BIT_DDMACH0_OWN | (len & 0x3FFFFu);

    if (!first)
        ctrl |= BIT_DDMACH0_CHKSUM_CONT;

    if (!hw_ready(r, REG_DDMA_CH0CTRL, BIT_DDMACH0_OWN, 0))
        return FALSE;

    w32(r, REG_DDMA_CH0SA, src);
    w32(r, REG_DDMA_CH0DA, dst);
    w32(r, REG_DDMA_CH0CTRL, ctrl);

    return hw_ready(r, REG_DDMA_CH0CTRL, BIT_DDMACH0_OWN, 0);
}

/* Часть прошивки (DMEM/IMEM/EMEM) - кусками по 4 КиБ: в буфер чипа,
   оттуда его же DMA - в память процессора, с проверкой суммы */
static BOOLEAN fw_to_mem(RTW *r, const UINT8 *data, UINT32 dst, UINT32 size)
{
    w32_set(r, REG_DDMA_CH0CTRL, BIT_DDMACH0_RESET_CHKSUM_STS);

    UINT32 off = 0;
    BOOLEAN first = TRUE;

    while (off < size) {

        UINT32 n = size - off > 0x1000u ? 0x1000u : size - off;

        if (!rtw_write_rsvd_page(r, 0, data + off, n))
            return rtw_fail(r, "firmware: chip did not take a 4 KiB piece (beacon queue)");

        if (!iddma_copy(r, OCPBASE_TXBUF_88XX + TX_DESC_SZ, dst + off, n, first))
            return rtw_fail(r, "firmware: chip DMA copy did not finish");

        first = FALSE;
        off += n;
    }

    UINT8 fwc = r8(r, REG_MCUFW_CTRL);

    if (r32(r, REG_DDMA_CH0CTRL) & BIT_DDMACH0_CHKSUM_STS) {
        if (dst < OCPBASE_DMEM_88XX)
            w8(r, REG_MCUFW_CTRL, (UINT8)((fwc | BIT_IMEM_DW_OK) & ~BIT_IMEM_CHKSUM_OK));
        else
            w8(r, REG_MCUFW_CTRL, (UINT8)((fwc | BIT_DMEM_DW_OK) & ~BIT_DMEM_CHKSUM_OK));
        return rtw_fail(r, "firmware: checksum mismatch inside the chip");
    }

    if (dst < OCPBASE_DMEM_88XX)
        w8(r, REG_MCUFW_CTRL, (UINT8)(fwc | BIT_IMEM_DW_OK | BIT_IMEM_CHKSUM_OK));
    else
        w8(r, REG_MCUFW_CTRL, (UINT8)(fwc | BIT_DMEM_DW_OK | BIT_DMEM_CHKSUM_OK));

    return TRUE;
}

static BOOLEAN lte_read(RTW *r, UINT16 off, UINT32 *v)
{
    if (!hw_ready(r, REG_LTECOEX_CTRL, LTECOEX_READY, 1))
        return FALSE;
    w32(r, REG_LTECOEX_CTRL, 0x800F0000u | off);
    *v = r32(r, REG_LTECOEX_RDATA);
    return TRUE;
}

static BOOLEAN lte_write(RTW *r, UINT16 off, UINT32 v)
{
    if (!hw_ready(r, REG_LTECOEX_CTRL, LTECOEX_READY, 1))
        return FALSE;
    w32(r, REG_LTECOEX_WDATA, v);
    w32(r, REG_LTECOEX_CTRL, 0xC00F0000u | off);
    return TRUE;
}

static void wlan_cpu_enable(RTW *r, BOOLEAN on)
{
    if (on) {
        w8_set(r, REG_RSV_CTRL + 1, BIT_WLMCU_IOIF);
        w8_set(r, REG_SYS_FUNC_EN + 1, BIT_FEN_CPUEN);
    } else {
        w8_clr(r, REG_SYS_FUNC_EN + 1, BIT_FEN_CPUEN);
        w8_clr(r, REG_RSV_CTRL + 1, BIT_WLMCU_IOIF);
    }
}

static UINT32 fw_le32(const UINT8 *fw, UINTN off)
{
    return le32(fw + off);
}

static BOOLEAN rtw_fw_download(RTW *r)
{
    const UINT8 *fw = g_fw_rtw8821c;
    UINTN size = (UINTN)(g_fw_rtw8821c_end - g_fw_rtw8821c);

    /* заголовок прошивки (struct rtw_fw_hdr): 64 байта */
    if (size < 64 || fw[0] != 0x21 || fw[1] != 0x88)
        return rtw_fail(r, "firmware file is not rtw8821c_fw.bin");

    UINT32 dmem_addr = fw_le32(fw, 0x20) & ~(1u << 31);
    UINT32 dmem = fw_le32(fw, 0x24) + 8u;                    /* + 8 байт суммы */
    UINT32 imem = fw_le32(fw, 0x30) + 8u;
    UINT32 emem = (fw[0x18] & 0x10u) ? fw_le32(fw, 0x34) + 8u : 0;
    UINT32 emem_addr = fw_le32(fw, 0x38) & ~(1u << 31);
    UINT32 imem_addr = fw_le32(fw, 0x3C) & ~(1u << 31);

    r->fw_ver = (UINT16)(fw[4] | (fw[5] << 8));
    r->fw_sub = fw[6];

    if (64u + dmem + imem + emem != size)
        return rtw_fail(r, "firmware file size does not match its header");

    UINT32 lte = 0;

    if (!lte_read(r, 0x38, &lte))
        return rtw_fail(r, "firmware: LTE-coex register is not ready");

    wlan_cpu_enable(r, FALSE);

    /* --- download_firmware_reg_backup --- */
    UINT8  b_pq = r8(r, REG_TXDMA_PQ_MAP + 1);
    w8(r, REG_TXDMA_PQ_MAP + 1, 3u << 6);                  /* HIQ -> высокий приоритет */
    UINT8  b_cr = r8(r, REG_CR);
    w8(r, REG_CR, 0x05);                                   /* HCI_TXDMA_EN | TXDMA_EN */
    w32(r, REG_H2CQ_CSR, BIT_H2CQ_FULL);
    UINT16 b_fp1 = r16(r, REG_FIFOPAGE_INFO_1);
    UINT32 b_rq2 = r32(r, REG_RQPN_CTRL_2) | BIT_LD_RQPN;
    w16(r, REG_FIFOPAGE_INFO_1, 0x200);
    w32(r, REG_RQPN_CTRL_2, b_rq2);
    UINT8  b_bcn = r8(r, REG_BCN_CTRL);
    w8(r, REG_BCN_CTRL, (UINT8)((b_bcn & ~BIT_EN_BCN_FUNCTION) | BIT_DIS_TSF_UDT));

    /* --- download_firmware_reset_platform --- */
    w8_clr(r, REG_CPU_DMEM_CON + 2, (UINT8)(BIT_WL_PLATFORM_RST >> 16));
    w8_clr(r, REG_SYS_CLK_CTRL + 1, (UINT8)(BIT_CPU_CLK_EN >> 8));
    w8_set(r, REG_CPU_DMEM_CON + 2, (UINT8)(BIT_WL_PLATFORM_RST >> 16));
    w8_set(r, REG_SYS_CLK_CTRL + 1, (UINT8)(BIT_CPU_CLK_EN >> 8));

    /* --- start_download_firmware --- */
    w16(r, REG_MCUFW_CTRL, (UINT16)((r16(r, REG_MCUFW_CTRL) & 0x3800u) | BIT_MCUFWDL_EN));

    BOOLEAN ok = fw_to_mem(r, fw + 64, dmem_addr, dmem) &&
                 fw_to_mem(r, fw + 64 + dmem, imem_addr, imem) &&
                 (emem == 0 || fw_to_mem(r, fw + 64 + dmem + imem, emem_addr, emem));

    if (!ok) {
        w8_clr(r, REG_MCUFW_CTRL, BIT_MCUFWDL_EN);
        w8_set(r, REG_SYS_FUNC_EN + 1, BIT_FEN_CPUEN);
        return FALSE;
    }

    /* --- download_firmware_reg_restore --- */
    w8(r, REG_TXDMA_PQ_MAP + 1, b_pq);
    w8(r, REG_CR, b_cr);
    w32(r, REG_H2CQ_CSR, BIT_H2CQ_FULL);
    w16(r, REG_FIFOPAGE_INFO_1, b_fp1);
    w32(r, REG_RQPN_CTRL_2, b_rq2);
    w8(r, REG_BCN_CTRL, b_bcn);

    /* --- download_firmware_end_flow --- */
    w32(r, REG_TXDMA_STATUS, 1u << 2);                     /* BTI_PAGE_OVF */

    UINT16 fwc = r16(r, REG_MCUFW_CTRL);

    if ((fwc & BIT_CHECK_SUM_OK) == BIT_CHECK_SUM_OK)
        w16(r, REG_MCUFW_CTRL, (UINT16)((fwc | BIT_FW_DW_RDY) & ~BIT_MCUFWDL_EN));

    wlan_cpu_enable(r, TRUE);
    lte_write(r, 0x38, lte);

    /* процессор чипа стартует и докладывает "готов" (FW_READY) */
    if (!hw_ready(r, REG_MCUFW_CTRL, FW_READY_MASK, FW_READY)) {

        /* подождать ещё: на некоторых чипах старт дольше 10 мс */
        BOOLEAN late = FALSE;

        for (UINTN i = 0; i < 50 && !late; i++) {
            mdelay(10);
            late = (r32(r, REG_MCUFW_CTRL) & FW_READY_MASK) == FW_READY;
        }

        if (!late) {
            char m[80];
            ksnprintf(m, sizeof(m), "firmware did not start (MCUFW_CTRL=0x%08x, key 0x%08x)",
                      r32(r, REG_MCUFW_CTRL), r32(r, REG_FW_DBG7) & 0xFFFFFF00u);
            return rtw_fail(r, m);
        }
    }

    /* прошивка сама трогала кольца - сбросить их индексы */
    rtw_hci_setup(r);
    r->h2c_box = 0;
    r->h2c_seq = 0;
    r->fw_running = TRUE;

    /* RTL8821CE: попросить прошивку восстановить Bluetooth-часть */
    UINT8 h2c[8] = { H2C_CMD_RECOVER_BT_DEV, 0x01, 0, 0, 0, 0, 0, 0 };
    rtw_h2c_cmd(r, h2c);

    return TRUE;
}

/* ================================================================
 * eFuse: прожжённая на заводе память (efuse.c)
 * ================================================================ */

static BOOLEAN rtw_efuse_read(RTW *r)
{
    static UINT8 phy[512];

    /* банк Wi-Fi, 2,5 В для eFuse не включать (только чтение) */
    w32_mask(r, REG_LDO_EFUSE_CTRL, (1u << 8) | (1u << 9), 0);
    w8(r, REG_LDO_EFUSE_CTRL + 3, (UINT8)(r8(r, REG_LDO_EFUSE_CTRL + 3) & ~0x80u));

    UINT32 ctl = r32(r, REG_EFUSE_CTRL);

    for (UINT32 a = 0; a < 512; a++) {

        ctl &= ~(0xFFu | (0x3FFu << 8));
        ctl |= (a & 0x3FFu) << 8;
        w32(r, REG_EFUSE_CTRL, ctl & ~BIT_EF_FLAG);

        UINTN n = 0;

        do {
            udelay(1);
            ctl = r32(r, REG_EFUSE_CTRL);
            if (++n > 100000)
                return rtw_fail(r, "eFuse does not answer");
        } while (!(ctl & BIT_EF_FLAG));

        phy[a] = (UINT8)ctl;
    }

    /* физическая карта -> логическая: записи "заголовок блока + до 4 слов"
       (rtw_dump_logical_efuse_map), защищённые последние 96 байт не читаем */
    UINT8 *log = r->efuse;

    memset(log, 0xFF, 512);

    for (UINT32 p = 0; p < 512 - 96; ) {

        UINT8 h1 = phy[p], h2 = phy[p + 1];
        UINT8 blk, we;

        if (h1 == 0xFF || ((h1 & 0x1Fu) == 0x0Fu && h2 == 0xFF))
            break;

        if ((h1 & 0x1Fu) == 0x0Fu) {
            blk = (UINT8)(((h2 & 0xF0u) >> 1) | ((h1 >> 5) & 0x07u));
            we = (UINT8)(h2 & 0x0Fu);
            p += 2;
        } else {
            blk = (UINT8)((h1 & 0xF0u) >> 4);
            we = (UINT8)(h1 & 0x0Fu);
            p += 1;
        }

        for (UINT32 i = 0; i < 4; i++) {

            if (we & (1u << i))
                continue;

            UINT32 li = ((UINT32)blk << 3) + (i << 1);

            if (p + 1 > 512 - 96 || li + 1 > 511)
                return rtw_fail(r, "eFuse map is damaged");

            log[li] = phy[p];
            log[li + 1] = phy[p + 1];
            p += 2;
        }
    }

    /* rtw8821c_read_efuse: поля логической карты (struct rtw8821c_efuse) */
    r->rfe = log[0xCA] & 0x1Fu;
    r->pkg = (log[0xCA] & 0x20u) ? 1 : 0;
    r->board_opt = log[0xC1] == 0xFF ? 0 : log[0xC1];
    r->xtal = log[0xB9] == 0xFF ? 0 : log[0xB9];
    r->bt_setting = log[0xC3];
    r->swing_2g = log[0xC6] == 0xFF ? 0 : log[0xC6];
    r->swing_5g = log[0xC7] == 0xFF ? 0 : log[0xC7];
    memcpy(r->txpwr, log + 0x10, 42);

    /* варианты 2 и 4: 2,4 ГГц идёт по пути B калибровки */
    if (r->rfe == 2 || r->rfe == 4)
        memcpy(r->txpwr, log + 0x10 + 42, 18);

    switch (r->rfe) {
    case 0x2: case 0x4: case 0x7: case 0xA: case 0xC: case 0xF:
        r->rfe_btg = TRUE;
        break;
    default:
        r->rfe_btg = FALSE;
    }

    r->share_ant = (r->bt_setting != 0xFF) && (r->bt_setting & 1u);
    r->btcoex = (r->board_opt & 0xE0u) == 0x20u;

    memcpy(r->mac, log + 0xD0, 6);
    r->efuse_ok = TRUE;

    BOOLEAN zero = TRUE, ff = TRUE;

    for (UINTN i = 0; i < 6; i++) {
        if (r->mac[i] != 0)
            zero = FALSE;
        if (r->mac[i] != 0xFF)
            ff = FALSE;
    }

    if (zero || ff || (r->mac[0] & 1u)) {
        /* адреса нет - случайный "локальный" */
        for (UINTN i = 0; i < 6; i++)
            r->mac[i] = (UINT8)net_random();
        r->mac[0] = (UINT8)((r->mac[0] & 0xFEu) | 0x02u);
        klog("rtw8821c: no MAC address in eFuse - using a random one\n");
    }

    return TRUE;
}

/* Что прошивка сообщила о чипе (rtw_dump_hw_feature): ответ на
   C2H_HW_FEATURE_DUMP лежит в REG_C2HEVT */
static void rtw_read_hw_feature(RTW *r)
{
    if (r8(r, REG_C2HEVT) != 0x19)
        return;

    UINT8 f[13];

    for (UINTN i = 0; i < 13; i++)
        f[i] = r8(r, REG_C2HEVT + 2 + (UINT32)i);

    w8(r, REG_C2HEVT, 0);

    UINT32 w1 = le32(f + 4);

    r->hwcap_bw = (UINT8)((w1 >> 16) & 7u);
    r->hwcap_nss = (UINT8)((w1 >> 19) & 3u);
    r->hwcap_ant = (UINT8)((w1 >> 21) & 7u);
    r->hwcap_ptcl = (UINT8)((w1 >> 26) & 3u);
    r->hwcap_ok = TRUE;
}

/* ================================================================
 * Настройка MAC после прошивки (rtw_mac_init + rtw8821c_mac_init)
 * ================================================================ */

static BOOLEAN rtw_mac_init(RTW *r)
{
    /* --- txdma_queue_mapping (PCIe): VO,VI -> normal; BE,BK -> low;
           MGMT -> extra; HI -> high --- */
    w16(r, REG_TXDMA_PQ_MAP, (UINT16)((3u << 14) | (0u << 12) | (1u << 10) | (1u << 8) |
                                      (2u << 6) | (2u << 4)));
    w8(r, REG_CR, 0);
    w8(r, REG_CR, MAC_TRX_ENABLE);
    w32(r, REG_H2CQ_CSR, BIT_H2CQ_FULL);

    /* --- priority_queue_cfg: буфер отправки 64 КиБ = 512 страниц по 128;
           52 страницы - служебные (H2C, прошивка), остальное - очередям:
           high 16, low 16, normal 16, extra 14, промежуток 1, общая 397 --- */
    w16(r, REG_FIFOPAGE_INFO_1, 16);
    w16(r, REG_FIFOPAGE_INFO_1 + 4, 16);
    w16(r, REG_FIFOPAGE_INFO_1 + 8, 16);
    w16(r, REG_FIFOPAGE_INFO_1 + 12, 14);
    w16(r, REG_FIFOPAGE_INFO_1 + 16, 397);
    w32_set(r, REG_RQPN_CTRL_2, BIT_LD_RQPN);

    w16(r, REG_FIFOPAGE_CTRL_2, 460);                 /* граница служебных страниц */
    w8_set(r, REG_FWHW_TXQ_CTRL + 2, 1u << 4);        /* EN_WR_FREE_TAIL */
    w16(r, REG_BCNQ_BDNY_V1, 460);
    w16(r, REG_FIFOPAGE_CTRL_2 + 2, 460);
    w16(r, REG_BCNQ1_BDNY_V1, 460);
    w32(r, REG_RXFF_BNDY, 16384u - 256u - 1u);        /* буфер приёма минус место для C2H */
    w8_set(r, REG_AUTO_LLT_V1, 1u);

    if (!hw_ready(r, REG_AUTO_LLT_V1, 1u, 0))
        return rtw_fail(r, "MAC: buffer page list (LLT) did not initialize");

    w8(r, REG_CR + 3, 0);

    /* --- init_h2c: очередь пакетов-команд - страницы 500..507 --- */
    UINT32 h2cq = 500u << 7, h2cs = 8u << 7;

    w32(r, REG_H2C_HEAD, (r32(r, REG_H2C_HEAD) & 0xFFFC0000u) | h2cq);
    w32(r, REG_H2C_READ_ADDR, (r32(r, REG_H2C_READ_ADDR) & 0xFFFC0000u) | h2cq);
    w32(r, REG_H2C_TAIL, (r32(r, REG_H2C_TAIL) & 0xFFFC0000u) | (h2cq + h2cs));
    w8(r, REG_H2C_INFO, (UINT8)((r8(r, REG_H2C_INFO) & 0xFCu) | 0x01u));
    w8(r, REG_H2C_INFO, (UINT8)((r8(r, REG_H2C_INFO) & 0xFBu) | 0x04u));
    w8(r, REG_TXDMA_OFFSET_CHK + 1, (UINT8)((r8(r, REG_TXDMA_OFFSET_CHK + 1) & 0x7Fu) | 0x80u));

    UINT32 wp = r32(r, REG_H2C_PKT_WRITEADDR) & 0x3FFFFu;
    UINT32 rp = r32(r, REG_H2C_PKT_READADDR) & 0x3FFFFu;
    UINT32 fr = wp >= rp ? h2cs - (wp - rp) : rp - wp;

    if (fr != h2cs)
        return rtw_fail(r, "MAC: H2C queue pointers mismatch");

    /* --- rtw8821c_mac_init: протокол, EDCA, маяки, фильтры приёма --- */
    w8(r, REG_AMPDU_MAX_TIME_V1, 0x70);
    w8_set(r, REG_TX_HANG_CTRL, 1u << 2);
    UINT16 pre = 0x1E4u | (1u << 11);
    w8(r, REG_PRECNT_CTRL, (UINT8)pre);
    w8(r, REG_PRECNT_CTRL + 1, (UINT8)(pre >> 8));
    w32(r, REG_PROT_MODE_CTRL, 0xFFu | (0x08u << 8) | (0x20u << 16) | (0x20u << 24));
    w16(r, REG_BAR_MODE_CTRL + 2, (UINT16)(0x01u | (0x08u << 8)));
    w8(r, REG_FAST_EDCA_VOVI, 0x06);
    w8(r, REG_FAST_EDCA_VOVI + 2, 0x06);
    w8(r, REG_FAST_EDCA_BEBK, 0x06);
    w8(r, REG_FAST_EDCA_BEBK + 2, 0x06);
    w8_set(r, REG_INIRTS_RATE_SEL, 1u << 5);

    w8_clr(r, REG_TIMER0_SRC_SEL, 0x70);
    w16(r, REG_TXPAUSE, 0);
    w8(r, REG_SLOT, 0x09);
    w8(r, REG_PIFS, 0x19);
    w32(r, REG_SIFS, 0x0Au | (0x0Eu << 8) | (0x10u << 16) | (0x10u << 24));
    w16(r, REG_EDCA_VO_PARAM + 2, 0x186);
    w16(r, REG_EDCA_VI_PARAM + 2, 0x3BC);
    w32(r, REG_RD_NAV_NXT, 0x05u | (0x1Bu << 16));
    w16(r, REG_RXTSF_OFFSET_CCK, 0x30u | (0x30u << 8));

    w8_set(r, REG_BCN_CTRL, BIT_EN_BCN_FUNCTION);
    w32(r, REG_TBTT_PROHIBIT, 0x04u | (0x064u << 8));
    w8(r, REG_DRVERLYINT, 0x04);
    w8(r, REG_BCNDMATIM, 0x02);
    w8_clr(r, REG_TX_PTCL_CTRL + 1, (1u << 12) >> 8);

    w16(r, REG_RXFLTMAP0, 0xFFFF);
    w16(r, REG_RXFLTMAP1, 0x0FFF);
    w16(r, REG_RXFLTMAP2, 0xFFFF);
    w32(r, REG_RCR, 0xE400220Eu);
    w8(r, REG_RX_PKT_LIMIT, 12288u >> 9);
    w8(r, REG_TCR + 2, 0x30);
    w8(r, REG_TCR + 1, 0x30);
    w8(r, REG_ACKTO_CCK, 0x40);
    w8_set(r, REG_WMAC_TRXPTCL_CTL_H, 1u << 1);
    w8_set(r, REG_SND_PTCL_CTRL, 1u << 6);             /* DIS_CHK_VHTSIGB_CRC */
    w32(r, REG_WMAC_OPTION_FUNCTION + 8, 0xB0810041u);
    w8(r, REG_WMAC_OPTION_FUNCTION + 4, 0x98);

    /* --- rtw_drv_info_cfg: к каждому принятому кадру - 32 байта
           "состояния модема" (сила сигнала) --- */
    w8(r, REG_RX_DRVINFO_SZ, 4);
    w8(r, REG_TRXFF_BNDY + 1, (UINT8)((r8(r, REG_TRXFF_BNDY + 1) & 0xF0u) | 0x0Fu));
    w32_set(r, REG_RCR, BIT_APP_PHYSTS);
    w32_clr(r, REG_WMAC_OPTION_FUNCTION + 4, (1u << 8) | (1u << 9));

    return TRUE;
}

/* ================================================================
 * Таблицы модема и радио (phy.c: rtw_parse_tbl_phy_cond)
 *
 * Таблица - пары UINT32 "адрес, значение". Если в первом слове пары
 * стоит старший бит (pos) - это условие "если плата такая-то"
 * (IF/ELIF/ELSE/ENDIF), бит 30 (neg) - "проверить условие". Условие -
 * ревизия кристалла, корпус, шина (PCIe), вариант платы (RFE).
 * ================================================================ */

#define TBL_MAC  0
#define TBL_BB   1
#define TBL_AGC  2
#define TBL_RF   3

static BOOLEAN tbl_cond_match(RTW *r, UINT32 c)
{
    UINT32 rfe = c & 0xFFu;
    UINT32 intf = (c >> 8) & 0xFu;
    UINT32 pkg = (c >> 12) & 0xFu;
    UINT32 cut = (c >> 24) & 0xFu;
    UINT32 my_cut = r->cut ? r->cut : 15u;
    UINT32 my_pkg = r->pkg ? r->pkg : 15u;

    if (cut && cut != my_cut)
        return FALSE;
    if (pkg && pkg != my_pkg)
        return FALSE;
    if (intf && intf != 1u)                /* 1 - PCIe */
        return FALSE;
    return rfe == r->rfe;
}

static void tbl_apply(RTW *r, UINTN kind, UINT32 addr, UINT32 data)
{
    switch (kind) {

    case TBL_MAC:
        w8(r, addr, (UINT8)data);
        break;

    case TBL_AGC:
        w32(r, addr, data);
        break;

    case TBL_BB:
        if (addr == 0xFE)
            mdelay(50);
        else if (addr == 0xFD)
            mdelay(5);
        else if (addr == 0xFC)
            mdelay(1);
        else if (addr == 0xFB)
            udelay(50);
        else if (addr == 0xFA)
            udelay(5);
        else if (addr == 0xF9)
            udelay(1);
        else
            w32(r, addr, data);
        break;

    case TBL_RF:
        if (addr == 0xFFE)
            mdelay(50);
        else if (addr == 0xFE)
            udelay(100);
        else {
            rf_write(r, addr, RFREG_MASK, data);
            udelay(1);
        }
        break;
    }
}

static UINTN rtw_load_table(RTW *r, const UINT32 *t, UINTN n, UINTN kind)
{
    UINT32 pos = 0;
    BOOLEAN matched = TRUE, skipped = FALSE;
    UINTN writes = 0;

    for (UINTN i = 0; i + 1 < n; i += 2) {

        UINT32 a = t[i], d = t[i + 1];

        if (a & 0x80000000u) {                     /* pos: IF/ELIF/ELSE/ENDIF */

            UINT32 branch = (a >> 28) & 3u;

            if (branch == 3) {                     /* ENDIF */
                matched = TRUE;
                skipped = FALSE;
            } else if (branch == 2) {              /* ELSE */
                matched = skipped ? FALSE : TRUE;
            } else {                               /* IF / ELIF: запомнить условие */
                pos = a;
            }

        } else if (a & 0x40000000u) {              /* neg: проверить условие */

            if (!skipped) {
                if (tbl_cond_match(r, pos)) {
                    matched = TRUE;
                    skipped = TRUE;
                } else {
                    matched = FALSE;
                    skipped = FALSE;
                }
            } else {
                matched = FALSE;
            }

        } else if (matched) {
            tbl_apply(r, kind, a, d);
            writes++;
        }
    }

    return writes;
}

/* ================================================================
 * Мощность передачи (phy.c: rtw_phy_get_tx_power_index)
 *
 * Индекс мощности (шаг 0,5 дБ) = база из eFuse (заводская калибровка
 * по группам каналов) + поправка для скорости (таблица PG) - но не
 * выше предела (таблица LMT). MyOS берёт самый строгий предел из всех
 * стран ("worldwide") - так точно не нарушим ничьи правила.
 * ================================================================ */

static const UINT8 g_rs_rates[RS_NUM][10] = {
    { 0x00, 0x01, 0x02, 0x03 },                                        /* CCK */
    { 0x04, 0x05, 0x06, 0x07, 0x08, 0x09, 0x0A, 0x0B },                /* OFDM */
    { 0x0C, 0x0D, 0x0E, 0x0F, 0x10, 0x11, 0x12, 0x13 },                /* HT MCS0-7 */
    { 0x2C, 0x2D, 0x2E, 0x2F, 0x30, 0x31, 0x32, 0x33, 0x34, 0x35 },    /* VHT 1SS */
};
static const UINT8 g_rs_size[RS_NUM] = { 4, 8, 8, 10 };

static const UINT8 g_ch5g[53] = {
    36, 38, 40, 42, 44, 46, 48, 52, 54, 56, 58, 60, 62, 64,
    100, 102, 104, 106, 108, 110, 112, 116, 118, 120, 122, 124, 126, 128,
    132, 134, 136, 138, 140, 142, 144, 149, 151, 153, 155, 157, 159, 161,
    165, 167, 169, 171, 173, 175, 177
};

static int ch5g_idx(UINT8 ch)
{
    for (int i = 0; i < 53; i++)
        if (g_ch5g[i] == ch)
            return i;
    return -1;
}

/* группа скоростей Linux (rs в таблице LMT) -> наша */
static int lmt_rs(UINT8 rs)
{
    switch (rs) {
    case 0: return RS_CCK;
    case 1: return RS_OFDM;
    case 2: return RS_HT1;
    case 4: return RS_VHT1;
    default: return -1;
    }
}

static UINT8 bcd(UINT32 v)
{
    return (UINT8)(((v >> 4) & 0xFu) * 10u + (v & 0xFu));
}

static void rtw_power_tables(RTW *r)
{
    memset(r->byrate_2g, 0, sizeof(r->byrate_2g));
    memset(r->byrate_5g, 0, sizeof(r->byrate_5g));

    /* --- PG: регистр TXAGC пути A -> четыре скорости, числа в BCD --- */
    for (UINTN i = 0; i < rtw8821c_bb_pg_type0_n; i++) {

        const RTW_PG_PAIR *p = &rtw8821c_bb_pg_type0[i];
        UINT8 rates[4];
        UINTN nr = 4;

        if (p->rf_path != 0)
            continue;

        switch (p->addr) {
        case 0xC20: rates[0] = 0x00; rates[1] = 0x01; rates[2] = 0x02; rates[3] = 0x03; break;
        case 0xC24: rates[0] = 0x04; rates[1] = 0x05; rates[2] = 0x06; rates[3] = 0x07; break;
        case 0xC28: rates[0] = 0x08; rates[1] = 0x09; rates[2] = 0x0A; rates[3] = 0x0B; break;
        case 0xC2C: rates[0] = 0x0C; rates[1] = 0x0D; rates[2] = 0x0E; rates[3] = 0x0F; break;
        case 0xC30: rates[0] = 0x10; rates[1] = 0x11; rates[2] = 0x12; rates[3] = 0x13; break;
        case 0xC3C: rates[0] = 0x2C; rates[1] = 0x2D; rates[2] = 0x2E; rates[3] = 0x2F; break;
        case 0xC40: rates[0] = 0x30; rates[1] = 0x31; rates[2] = 0x32; rates[3] = 0x33; break;
        case 0xC44: rates[0] = 0x34; rates[1] = 0x35; nr = 2; break;   /* дальше - 2 потока */
        default: continue;
        }

        for (UINTN k = 0; k < nr; k++) {
            INT8 v = (INT8)bcd(p->data >> (k * 8u));
            if (p->band == 0)
                r->byrate_2g[rates[k]] = v;
            else
                r->byrate_5g[rates[k]] = v;
        }
    }

    /* поправки - относительно "главной" скорости группы (11M, 54M, MCS7) */
    INT8 base2[RS_NUM], base5[RS_NUM];

    for (UINTN rs = 0; rs < RS_NUM; rs++) {

        UINT8 bi = (rs == RS_VHT1) ? g_rs_rates[rs][g_rs_size[rs] - 3u] :
                                     g_rs_rates[rs][g_rs_size[rs] - 1u];

        base2[rs] = r->byrate_2g[bi];
        base5[rs] = r->byrate_5g[bi];

        for (UINTN k = 0; k < g_rs_size[rs]; k++) {
            r->byrate_2g[g_rs_rates[rs][k]] = (INT8)(r->byrate_2g[g_rs_rates[rs][k]] - base2[rs]);
            r->byrate_5g[g_rs_rates[rs][k]] = (INT8)(r->byrate_5g[g_rs_rates[rs][k]] - base5[rs]);
        }
    }

    /* --- LMT: минимум по всем странам, только ширина 20 МГц --- */
    for (UINTN rs = 0; rs < RS_NUM; rs++) {
        for (UINTN c = 0; c < 14; c++)
            r->lmt_2g[rs][c] = 63;
        for (UINTN c = 0; c < 53; c++)
            r->lmt_5g[rs][c] = 63;
    }

    for (UINTN i = 0; i < rtw8821c_txpwr_lmt_type0_n; i++) {

        const RTW_LMT_PAIR *p = &rtw8821c_txpwr_lmt_type0[i];
        int rs = lmt_rs(p->rs);
        INT8 v = p->txpwr_lmt;

        if (p->bw != 0 || rs < 0)
            continue;

        if (v > 63)
            v = 63;
        if (v < -63)
            v = -63;

        if (p->band == 0 && p->ch >= 1 && p->ch <= 14) {
            if (v < r->lmt_2g[rs][p->ch - 1])
                r->lmt_2g[rs][p->ch - 1] = v;
        } else if (p->band == 1) {
            int ci = ch5g_idx(p->ch);
            if (ci >= 0 && v < r->lmt_5g[rs][ci])
                r->lmt_5g[rs][ci] = v;
        }
    }

    /* 5 ГГц: если для HT предела нет, а для VHT есть - взять его (и наоборот) */
    for (UINTN c = 0; c < 53; c++) {
        if (r->lmt_5g[RS_HT1][c] == 63)
            r->lmt_5g[RS_HT1][c] = r->lmt_5g[RS_VHT1][c];
        else if (r->lmt_5g[RS_VHT1][c] == 63)
            r->lmt_5g[RS_VHT1][c] = r->lmt_5g[RS_HT1][c];
    }

    /* пределы - тоже относительно главной скорости группы */
    for (UINTN rs = 0; rs < RS_NUM; rs++) {
        for (UINTN c = 0; c < 14; c++)
            r->lmt_2g[rs][c] = (INT8)(r->lmt_2g[rs][c] - base2[rs]);
        for (UINTN c = 0; c < 53; c++)
            r->lmt_5g[rs][c] = (INT8)(r->lmt_5g[rs][c] - base5[rs]);
    }
}

static int nib(UINT8 v)                   /* 4 бита со знаком */
{
    v &= 0xFu;
    return v >= 8 ? (int)v - 16 : (int)v;
}

static UINT8 ch_group(UINT8 ch, UINT8 rate)
{
    if (ch <= 2 || (ch >= 36 && ch <= 42)) return 0;
    if (ch <= 5 || (ch >= 44 && ch <= 50)) return 1;
    if (ch <= 8 || (ch >= 52 && ch <= 58)) return 2;
    if (ch <= 11 || (ch >= 60 && ch <= 64)) return 3;
    if (ch <= 13) return 4;
    if (ch == 14) return rate <= DESC_RATE11M ? 5 : 4;
    if (ch <= 106) return 4;
    if (ch <= 114) return 5;
    if (ch <= 122) return 6;
    if (ch <= 130) return 7;
    if (ch <= 138) return 8;
    if (ch <= 144) return 9;
    if (ch <= 155) return 10;
    if (ch <= 161) return 11;
    if (ch <= 171) return 12;
    return 13;
}

static UINT8 rtw_pwr_index(RTW *r, UINT8 rate, UINT8 ch, int rs)
{
    const UINT8 *e = r->txpwr;
    UINT8 g = ch_group(ch, rate);
    BOOLEAN mcs = rate >= DESC_RATEMCS0;
    BOOLEAN ofdm = rate >= DESC_RATE6M && rate <= DESC_RATE54M;
    int base, offset, limit;

    if (ch <= 14) {
        base = rate <= DESC_RATE11M ? e[g] : e[6 + (g > 4 ? 4 : g)];
        if (ofdm)
            base += nib(e[11]);                   /* ht_1s_diff.ofdm */
        if (mcs)
            base += nib((UINT8)(e[11] >> 4));     /* ht_1s_diff.bw20 */
        offset = r->byrate_2g[rate];
        limit = r->lmt_2g[rs][ch - 1];
    } else {
        int ci = ch5g_idx(ch);
        base = e[18 + g];
        if (!mcs)
            base += nib(e[32]);
        else
            base += nib((UINT8)(e[32] >> 4));
        offset = r->byrate_5g[rate];
        limit = ci >= 0 ? r->lmt_5g[rs][ci] : 0;
    }

    int p = base + (offset < limit ? offset : limit);

    if (p < 0)
        p = 0;
    if (p > 63)
        p = 63;

    return (UINT8)p;
}

/* Записать индексы мощности: по 4 скорости в регистр 0x1D00 + (rate & ~3) */
static void rtw_set_tx_power(RTW *r, UINT8 ch)
{
    for (int rs = 0; rs < RS_NUM; rs++) {

        if (rs == RS_CCK && ch > 14)
            continue;                         /* на 5 ГГц CCK не бывает */

        UINT32 acc = 0;

        for (UINTN k = 0; k < g_rs_size[rs]; k++) {

            UINT8 rate = g_rs_rates[rs][k];
            UINT8 idx = rtw_pwr_index(r, rate, ch, rs);

            r->pwr_idx[rate] = idx;
            acc |= (UINT32)idx << ((rate & 3u) * 8u);

            if ((rate & 3u) == 3u || rate == DESC_RATEVHT1SS_MCS9) {
                w32(r, REG_TXAGC_BASE_A + (rate & 0xFCu), acc);
                acc = 0;
            }
        }
    }
}

/* ================================================================
 * Совместная работа с Bluetooth (coex.c, rtw8821c.c: coex_*)
 * ================================================================ */

#define SW_BBSW  0          /* антенным переключателем управляет модем */
#define SW_PTA   1          /* ... арбитр Wi-Fi/Bluetooth (PTA) */
#define POS_BT     0
#define POS_WLG    1
#define POS_WLA    2
#define POS_WLG_BT 3

/* rtw8821c_coex_cfg_rfe_type + rtw8821c_coex_cfg_ant_switch */
static void coex_ant_switch(RTW *r, UINTN ctrl, UINTN pos)
{
    UINT8 t = r->rfe;
    BOOLEAN inverse = (t == 3 || t == 11 || t == 4 || t == 12);
    BOOLEAN exists = !(t == 5 || t == 13 || t == 6 || t == 14);
    BOOLEAN wlg_at_btg = (t == 2 || t == 10 || t == 7 || t == 15 || t == 4 || t == 12);
    UINT8 v;

    if (!exists)
        return;

    /* 2,4 ГГц Wi-Fi разведён через тракт Bluetooth: переключатель
       всегда у модема, в положении "общий" (или 5 ГГц при инверсии) */
    if (wlg_at_btg) {
        ctrl = SW_BBSW;
        pos = inverse ? POS_WLA : POS_WLG_BT;
    }

    w32_clr(r, REG_LED_CFG, BIT_DPDT_SEL_EN);
    w32_set(r, REG_LED_CFG, BIT_DPDT_WL_SEL);

    if (ctrl == SW_PTA) {
        w8_mask(r, REG_RFE_CTRL8, 0xFF, 0x66);
        v = inverse ? 1 : 2;
    } else {
        w8_mask(r, REG_RFE_CTRL8, 0xFF, 0x77);
        if (pos == POS_WLG_BT)
            v = (t != 4 && t != 2) ? 3 : (inverse ? 1 : 2);
        else if (pos == POS_WLG)
            v = inverse ? 1 : 2;
        else
            v = inverse ? 2 : 1;
    }

    w32_mask(r, REG_RFE_CTRL8, 0xF0000000u, v);

    /* управление - не у Bluetooth */
    w8_set(r, REG_CTRL_TYPE, (1u << 5) | (1u << 4));
}

static void coex_gnt(RTW *r, BOOLEAN bt, UINT32 state)
{
    UINT32 v;

    if (!lte_read(r, 0x38, &v))
        return;

    if (bt)
        v = (v & ~0xCC00u) | (state << 14) | (state << 10);
    else
        v = (v & ~0x3300u) | (state << 12) | (state << 8);

    lte_write(r, 0x38, v);
}

/* Антенна - Wi-Fi: "разрешение" (GNT) Wi-Fi - всегда да, Bluetooth -
   всегда нет; переключатель - на нужный диапазон */
static void coex_wifi_only(RTW *r, BOOLEAN band5)
{
    coex_gnt(r, TRUE, 1);                      /* GNT_BT: программно, 0 */
    coex_gnt(r, FALSE, 3);                     /* GNT_WL: программно, 1 */
    w8_set(r, REG_SYS_SDIO_CTRL + 3, (UINT8)(BIT_LTE_MUX_CTRL_PATH >> 24));   /* хозяин - Wi-Fi */
    coex_ant_switch(r, SW_BBSW, band5 ? POS_WLA : POS_WLG);
}

static void rtw_coex_init(RTW *r)
{
    /* rtw_coex_power_on_setting */
    w8_set(r, REG_SYS_FUNC_EN, BIT_FEN_BB_GLB_RST | BIT_FEN_BB_RSTB);
    w8(r, 0xFF1A, 0);

    /* rtw8821c_coex_cfg_gnt_debug: выводы отладки - не наши */
    w32_clr(r, REG_PAD_CTRL1, BIT_BTGP_SPI_EN);
    w32_clr(r, REG_PAD_CTRL1, BIT_BTGP_JTAG_EN);
    w32_clr(r, REG_GPIO_MUXCFG, BIT_FSPI_EN);
    w32_clr(r, REG_PAD_CTRL1, BIT_LED1DIS);
    w32_clr(r, REG_SYS_SDIO_CTRL, BIT_SDIO_INT);
    w32_clr(r, REG_SYS_SDIO_CTRL, BIT_DBG_GNT_WL_BT);

    /* __rtw_coex_init_hw_config + rtw8821c_coex_cfg_init */
    w8_set(r, REG_BCN_CTRL, BIT_EN_BCN_FUNCTION);
    w8_mask(r, REG_BT_TDMA_TIME, 0x3F, 0x5);
    w8(r, REG_BT_STAT_CTRL, 0x1);
    w32_set(r, REG_GPIO_MUXCFG, BIT_BT_PTA_EN);
    w32_set(r, REG_GPIO_MUXCFG, BIT_PO_BT_PTA_PINS);
    w8_set(r, REG_QUEUE_CTRL, BIT_PTA_WL_TX_EN);
    w8_clr(r, REG_QUEUE_CTRL, BIT_PTA_EDCCA_EN);
    w16_set(r, REG_BT_COEX_V2, BIT_GNT_BT_POLARITY);
    w8_mask(r, REG_BT_COEX_TABLE_H + 3, 1u << 3, 1);

    /* ответы (ACK), маяки и их очередь у Wi-Fi - в высоком приоритете */
    w8_mask(r, REG_BT_COEX_TABLE_H + 0, 1u << 3, 1);
    w8_mask(r, REG_BT_COEX_TABLE_H + 0, 1u << 4, 1);
    w8_mask(r, REG_BT_COEX_TABLE_H + 3, 1u << 3, 1);

    coex_wifi_only(r, FALSE);

    /* "табло" для Bluetooth-части: Wi-Fi включён и работает */
    w16(r, REG_WIFI_BT_INFO, (UINT16)(0x2u | 0x1u | BIT_BT_INT_EN));

    /* таблица арбитра: всё - Wi-Fi (случай 1 для общей антенны) */
    if (r->share_ant) {
        w32(r, REG_BT_COEX_TABLE0, 0x55555555u);
        w32(r, REG_BT_COEX_TABLE1, 0x5a5a5a5au);
        w32(r, REG_BT_COEX_BRK_TABLE, 0xF0FFFFFFu);
    }
}

/* ================================================================
 * Модем: первый запуск (rtw8821c_phy_set_param)
 * ================================================================ */

static BOOLEAN rtw_phy_init(RTW *r, SIMPLE_TEXT_OUTPUT_INTERFACE *out)
{
    /* питание модема и радио, сброс модема */
    UINT8 v = r8(r, REG_SYS_FUNC_EN);

    v |= BIT_FEN_PCIEA;
    w8(r, REG_SYS_FUNC_EN, v);
    v |= BIT_FEN_BB_RSTB | BIT_FEN_BB_GLB_RST;
    w8(r, REG_SYS_FUNC_EN, v);
    v &= (UINT8)~(BIT_FEN_BB_RSTB | BIT_FEN_BB_GLB_RST);
    w8(r, REG_SYS_FUNC_EN, v);
    v |= BIT_FEN_BB_RSTB | BIT_FEN_BB_GLB_RST;
    w8(r, REG_SYS_FUNC_EN, v);

    w8(r, REG_RF_CTRL, BIT_RF_EN | BIT_RF_RSTB | BIT_RF_SDM_RSTB);
    udelay(10);
    w8(r, REG_WLRF1 + 3, BIT_RF_EN | BIT_RF_RSTB | BIT_RF_SDM_RSTB);
    udelay(10);

    w32_clr(r, REG_RXPSEL, BIT_RX_PSEL_RST);

    /* таблицы Realtek */
    UINTN n_mac = rtw_load_table(r, rtw8821c_mac, rtw8821c_mac_n, TBL_MAC);
    UINTN n_bb = rtw_load_table(r, rtw8821c_bb, rtw8821c_bb_n, TBL_BB);
    UINTN n_agc = rtw_load_table(r, rtw8821c_agc, rtw8821c_agc_n, TBL_AGC);

    if (r->rfe == 2 || r->rfe == 4)
        n_agc += rtw_load_table(r, rtw8821c_agc_btg_type2, rtw8821c_agc_btg_type2_n, TBL_AGC);

    UINTN n_rf = rtw_load_table(r, rtw8821c_rf_a, rtw8821c_rf_a_n, TBL_RF);

    kprintf(out, "  radio tables: MAC %u, modem %u, gain %u, RF %u registers\n",
            (UINT32)n_mac, (UINT32)n_bb, (UINT32)n_agc, (UINT32)n_rf);

    if (n_bb < 100 || n_rf < 100)
        return rtw_fail(r, "radio tables: almost nothing matched this board (RFE option?)");

    /* подстройка кварца из eFuse */
    UINT8 xc = r->xtal & 0x3Fu;

    w32_mask(r, REG_AFE_XTAL_CTRL, 0x7E000000u, xc);
    w32_mask(r, REG_AFE_PLL_CTRL, 0x7Eu, xc);
    w32_mask(r, REG_CCK0_FAREPORT, (1u << 18) | (1u << 22), 0);

    w32_set(r, REG_RXPSEL, BIT_RX_PSEL_RST);

    r->ch_param[0] = r32(r, REG_TXSF2);
    r->ch_param[1] = r32(r, REG_TXSF6);
    r->ch_param[2] = r32(r, REG_TXFILTER);

    r->igi_default = (UINT8)r32_mask(r, REG_RXIGI_A, 0x7F);
    r->igi = r->igi_default;

    rtw_power_tables(r);

    return TRUE;
}

/* ================================================================
 * Канал (rtw8821c_set_channel + rtw_set_channel_mac)
 * ================================================================ */

static void rtw_switch_rf_set(RTW *r, UINTN set)
{
    w32_set(r, REG_DMEM_CTRL, 1u << 16);            /* BIT_WL_RST */
    w32_set(r, REG_SYS_CTRL, 1u << 26);             /* BIT_FEN_EN */

    UINT32 reg = r32(r, REG_RFECTL);

    if (set == 0) {                                  /* BTG: 2,4 ГГц через тракт Bluetooth */
        reg |= B_BTG_SWITCH;
        reg &= ~(B_CTRL_SWITCH | B_WL_SWITCH | B_WLG_SWITCH | B_WLA_SWITCH);
        w32_mask(r, REG_ENRXCCA, 0x00FF0000u, 0x0E);
        w32_mask(r, REG_ENTXCCK, 0x0000FFFFu, 0xFC84);
    } else if (set == 1) {                           /* WLG */
        reg |= B_WL_SWITCH | B_WLG_SWITCH;
        reg &= ~(B_BTG_SWITCH | B_CTRL_SWITCH | B_WLA_SWITCH);
        w32_mask(r, REG_ENRXCCA, 0x00FF0000u, 0x12);
        w32_mask(r, REG_ENTXCCK, 0x0000FFFFu, 0x7532);
    } else {                                         /* WLA: 5 ГГц */
        reg |= B_WL_SWITCH | B_WLA_SWITCH;
        reg &= ~(B_BTG_SWITCH | B_CTRL_SWITCH | B_WLG_SWITCH);
    }

    w32(r, REG_RFECTL, reg);
}

static BOOLEAN rtw_set_channel(WLAN_HW *hw, UINT8 ch)
{
    RTW *r = (RTW *)hw->priv;

    if (!r->fw_running)
        return FALSE;

    BOOLEAN g2 = ch <= 14;

    /* --- модем (set_channel_bb) --- */
    if (g2) {
        w32_mask(r, REG_RXPSEL, 1u << 28, 1);
        w32_mask(r, REG_CCK_CHECK, 1u << 7, 0);
        w32_mask(r, REG_ENTXCCK, 1u << 18, 0);
        w32_mask(r, REG_RXCCAMSK, 0x0000FC00u, 15);
        w32_mask(r, REG_TXSCALE_A, 0xF00u, 0);
        w32_mask(r, REG_CLKTRK, 0x1FFE0000u, 0x96A);

        if (ch == 14) {
            w32(r, REG_TXSF2, 0x0000B81Cu);
            w32_mask(r, REG_TXSF6, 0xFFFFu, 0x0000);
            w32(r, REG_TXFILTER, 0x00003667u);
        } else {
            w32(r, REG_TXSF2, r->ch_param[0]);
            w32_mask(r, REG_TXSF6, 0xFFFFu, r->ch_param[1] & 0xFFFFu);
            w32(r, REG_TXFILTER, r->ch_param[2]);
        }
    } else {
        w32_mask(r, REG_ENTXCCK, 1u << 18, 1);
        w32_mask(r, REG_CCK_CHECK, 1u << 7, 1);
        w32_mask(r, REG_RXPSEL, 1u << 28, 0);
        w32_mask(r, REG_RXCCAMSK, 0x0000FC00u, 15);

        if (ch >= 36 && ch <= 64)
            w32_mask(r, REG_TXSCALE_A, 0xF00u, 1);
        else if (ch >= 100 && ch <= 144)
            w32_mask(r, REG_TXSCALE_A, 0xF00u, 2);
        else if (ch >= 149)
            w32_mask(r, REG_TXSCALE_A, 0xF00u, 3);

        if (ch >= 36 && ch <= 48)
            w32_mask(r, REG_CLKTRK, 0x1FFE0000u, 0x494);
        else if (ch >= 52 && ch <= 64)
            w32_mask(r, REG_CLKTRK, 0x1FFE0000u, 0x453);
        else if (ch >= 100 && ch <= 116)
            w32_mask(r, REG_CLKTRK, 0x1FFE0000u, 0x452);
        else if (ch >= 118 && ch <= 177)
            w32_mask(r, REG_CLKTRK, 0x1FFE0000u, 0x412);
    }

    /* ширина 20 МГц */
    UINT32 adc = r32(r, REG_ADCCLK);
    adc &= 0xFFCFFC00u;
    adc |= 0x10010000u;
    w32(r, REG_ADCCLK, adc);
    w32_mask(r, REG_ADC160, 1u << 30, 1);

    /* --- размах сигнала передатчика из eFuse (bb_swing) --- */
    static const UINT32 swing[4] = { 0x200, 0x16A, 0x101, 0x0B6 };
    UINT8 sw = g2 ? r->swing_2g : r->swing_5g;

    if (sw > 9)
        sw = 0;
    w32_mask(r, REG_TXSCALE_A, 0xFFE00000u, swing[sw / 3u]);

    /* --- MAC (rtw_set_channel_mac): 20 МГц, часы MAC 80 МГц --- */
    w8(r, REG_DATA_SC, 0);
    w32_clr(r, REG_WMAC_TRXPTCL_CTL, (1u << 7) | (1u << 8));
    w32_mask(r, REG_AFE_CTRL1, (1u << 20) | (1u << 21), 0);
    w8(r, REG_USTIME_TSF, 80);
    w8(r, REG_USTIME_EDCA, 80);
    w8(r, REG_CCK_CHECK, (UINT8)((r8(r, REG_CCK_CHECK) & ~(1u << 7)) | (g2 ? 0 : (1u << 7))));

    /* --- радио (set_channel_rf): RF 0x18 - диапазон, канал, ширина --- */
    UINT32 rf18 = rf_read(r, RF_CFGCH, RFREG_MASK);

    rf18 &= ~(((1u << 16) | (1u << 9) | (1u << 8)) | 0xFFu | ((1u << 18) | (1u << 17)) |
              ((1u << 11) | (1u << 10)));
    rf18 |= g2 ? 0 : ((1u << 16) | (1u << 8));
    rf18 |= ch;
    if (ch >= 100 && ch <= 140)
        rf18 |= 1u << 17;
    else if (ch > 140)
        rf18 |= 1u << 18;
    rf18 |= (1u << 11) | (1u << 10);                  /* 20 МГц */

    if (g2) {
        rtw_switch_rf_set(r, r->rfe_btg ? 0 : 1);
        rf_write(r, RF_LUTDBG, 1u << 6, 1);
        rf_write(r, 0x64, 0xF, 0xF);
    } else {
        rtw_switch_rf_set(r, 2);
        rf_write(r, RF_LUTDBG, 1u << 6, 0);
    }

    rf_write(r, RF_CFGCH, RFREG_MASK, rf18);
    rf_write(r, RF_XTALX2, 1u << 19, 0);
    rf_write(r, RF_XTALX2, 1u << 19, 1);

    /* --- фильтры приёма для 20 МГц (set_channel_rxdfir) --- */
    w32_mask(r, REG_ACBB0, (1u << 29) | (1u << 28), 0x2);
    w32_mask(r, REG_ACBBRXFIR, (1u << 29) | (1u << 28), 0x2);
    w32_mask(r, REG_TXDFIR, 1u << 31, 0x1);
    w32_mask(r, REG_CHFIR, 1u << 31, 0x0);

    /* антенна Wi-Fi - на нужный диапазон */
    if ((r->channel <= 14) != g2 || r->channel == 0)
        coex_wifi_only(r, !g2);

    r->channel = ch;
    rtw_set_tx_power(r, ch);

    return TRUE;
}

/* Калибровка передатчика (IQK) - её делает прошивка (rtw8821c_do_iqk) */
static void rtw_iqk(RTW *r)
{
    UINT8 p = r->linked ? 0x02 : 0x00;               /* segment_iqk */
    UINT64 t0 = net_now_ms();

    rtw_h2c_pkt(r, H2C_PKT_IQK, &p, 1);

    r->iqk_ok = FALSE;

    for (UINTN i = 0; i < 300; i++) {
        if (rf_read(r, RF_DTXLOK, RFREG_MASK) == 0xABCDE) {
            r->iqk_ok = TRUE;
            break;
        }
        mdelay(20);
    }

    rf_write(r, RF_DTXLOK, RFREG_MASK, 0);
    r->iqk_ms = (UINT32)(net_now_ms() - t0);
}

/* ================================================================
 * Интерфейс для net/wlan.c
 * ================================================================ */

static void rtw_set_scan(WLAN_HW *hw, BOOLEAN scanning)
{
    RTW *r = (RTW *)hw->priv;

    r->scanning = scanning;

    if (scanning) {
        /* принимать маяки всех сетей; чувствительность - максимальная */
        w32_clr(r, REG_RCR, BIT_CBSSID_BCN | BIT_CBSSID_DATA);
        w32_mask(r, REG_RXIGI_A, 0x7F, 0x1C);
    } else {
        w32_mask(r, REG_RXIGI_A, 0x7F, r->igi);
        if (r->bssid_set)
            w32_set(r, REG_RCR, BIT_CBSSID_BCN);
    }
}

static void rtw_set_bssid(WLAN_HW *hw, const UINT8 *bssid)
{
    RTW *r = (RTW *)hw->priv;
    static const UINT8 none[6] = { 0 };

    if (bssid == NULL)
        bssid = none;

    for (UINT32 i = 0; i < 6; i++)
        w8(r, REG_BSSID + i, bssid[i]);

    r->bssid_set = (bssid != none);

    /* есть своя сеть - принимать только её маяки */
    if (bssid != none)
        w32_set(r, REG_RCR, BIT_CBSSID_BCN);
    else
        w32_clr(r, REG_RCR, BIT_CBSSID_BCN);

    /* перед первым кадром точке - откалибровать передатчик на её канале */
    if (bssid != none)
        rtw_iqk(r);
}

/* Сказать прошивке, какими скоростями говорить с точкой (RA info) */
static void rtw_send_ra(RTW *r)
{
    UINT8 h[8];

    memset(h, 0, sizeof(h));
    h[0] = H2C_CMD_RA_INFO;
    h[1] = 0;                                        /* mac_id */
    h[2] = (UINT8)(r->rate_id & 0x1Fu) | (1u << 5);  /* init_ra_lv = 1 */
    h[3] = (UINT8)(1u << 6);                         /* 20 МГц, без SGI/LDPC/VHT; DIS_PT */
    h[4] = (UINT8)r->ra_mask;
    h[5] = (UINT8)(r->ra_mask >> 8);
    h[6] = (UINT8)(r->ra_mask >> 16);
    h[7] = (UINT8)(r->ra_mask >> 24);

    rtw_h2c_cmd(r, h);
}

static void rtw_set_link(WLAN_HW *hw, BOOLEAN up, UINT16 aid, UINT32 rates)
{
    RTW *r = (RTW *)hw->priv;
    UINT8 h[8];

    r->linked = up;

    /* тип сети порта 0: 2 - "клиент, подключён", 0 - нет связи */
    w32_mask(r, REG_CR, 0x30000u, up ? 2u : 0u);
    w32_mask(r, REG_AID, 0x7FFu, up ? aid : 0u);

    memset(h, 0, sizeof(h));
    h[0] = H2C_CMD_MEDIA_STATUS_RPT;
    h[1] = up ? 1 : 0;                               /* op_mode */
    h[2] = 0;                                        /* mac_id */
    rtw_h2c_cmd(r, h);

    if (!up)
        return;

    if (r->channel <= 14) {
        r->rate_id = (rates & 0xFF0u) ? RATEID_BG : RATEID_B_20M;
        r->ra_mask = rates & 0xFFFu;
    } else {
        r->rate_id = RATEID_G;
        r->ra_mask = rates & 0xFF0u;
    }

    if (r->ra_mask == 0)
        r->ra_mask = r->channel <= 14 ? 0xFFFu : 0xFF0u;

    rtw_send_ra(r);

    memset(h, 0, sizeof(h));
    h[0] = H2C_CMD_DEFAULT_PORT;
    h[1] = 0;                                        /* порт 0 */
    h[2] = 0;                                        /* mac_id */
    rtw_h2c_cmd(r, h);
}

static void rtw_report_rssi(WLAN_HW *hw, INT32 rssi_dbm)
{
    RTW *r = (RTW *)hw->priv;

    r->rssi = rssi_dbm;
}

static BOOLEAN rtw_tx(WLAN_HW *hw, const UINT8 *frame, UINTN len, UINT32 flags)
{
    RTW *r = (RTW *)hw->priv;
    RTW_TXI t;
    BOOLEAN g2 = r->channel <= 14;
    UINTN q;

    if (!r->fw_running || len < 24)
        return FALSE;

    memset(&t, 0, sizeof(t));
    t.size = (UINT32)len;
    t.offset = TX_DESC_SZ;
    t.ls = TRUE;
    t.hwseq = TRUE;
    t.mac_id = 0;
    t.bmc = (frame[4] & 1u) != 0;

    if (flags & WLAN_TX_MGMT) {
        q = Q_MGMT;
        t.qsel = QSEL_MGMT;
    } else {
        q = Q_BE;
        t.qsel = QSEL_TID0;
    }

    if ((flags & (WLAN_TX_MGMT | WLAN_TX_LOWRATE)) || !r->linked) {
        /* самая надёжная скорость: 1 Мбит/с (2,4 ГГц) или 6 (5 ГГц) */
        t.use_rate = TRUE;
        t.dis_fb = TRUE;
        t.rate_id = g2 ? RATEID_B_20M : RATEID_G;
        t.rate = g2 ? DESC_RATE1M : DESC_RATE6M;
    } else {
        /* скорость выбирает прошивка (rate adaptation) */
        t.rate_id = r->rate_id;
        t.rate = DESC_RATE54M;
    }

    if (!rtw_tx_queue(r, q, &t, frame, len))
        return FALSE;

    r->tx_frames++;
    return TRUE;
}

/* ================================================================
 * Приём
 * ================================================================ */

static const INT8 g_lna0[8] = { 22, 8, -6, -22, -31, -40, -46, -52 };
static const INT8 g_lna1[16] = { 10, 6, 2, -2, -6, -10, -14, -17,
                                 -20, -24, -28, -31, -34, -37, -40, -44 };

/* Сила сигнала из "состояния модема" (query_phy_status_page0/1) */
static INT32 rtw_phy_rssi(RTW *r, const UINT8 *ps)
{
    UINT32 page = ps[0] & 0xFu;

    if (page == 0) {                                  /* CCK */
        UINT32 w3 = le32(ps + 12);
        UINT32 vga = (w3 >> 8) & 0x1Fu;
        UINT32 lna = ((w3 >> 23) & 1u) << 3 | ((w3 >> 13) & 7u);

        if (r->rfe == 0)
            return lna < 8 ? g_lna0[lna] - 2 * (INT32)vga : -120;
        return lna < 16 ? g_lna1[lna] - 2 * (INT32)vga : -120;
    }

    if (page == 1)                                    /* OFDM/HT/VHT */
        return (INT32)((le32(ps) >> 8) & 0xFFu) - 110;

    return -120;
}

static void rtw_rx(RTW *r)
{
    UINT32 hw = (r32(r, RTK_PCI_RXBD_IDX_MPDUQ) >> 16) & 0xFFFu;

    if (hw >= RX_RING_LEN)
        return;                                   /* чип не отвечает (0xFFFF...) */

    UINT32 cnt = hw >= r->rx_rp ? hw - r->rx_rp : RX_RING_LEN - (r->rx_rp - hw);

    if (cnt == 0)
        return;

    while (cnt--) {

        const UINT8 *b = (const UINT8 *)P2V(r->rx_buf + (UINT64)r->rx_rp * RX_SLOT);
        UINT32 w0 = le32(b), w2 = le32(b + 8);
        UINT32 plen = w0 & 0x3FFFu;
        BOOLEAN crc = (w0 >> 14) & 1u;
        UINT32 drv = ((w0 >> 16) & 0xFu) * 8u;
        UINT32 shift = (w0 >> 24) & 3u;
        BOOLEAN physt = (w0 >> 26) & 1u;
        BOOLEAN c2h = (w2 >> 28) & 1u;

        if (c2h) {
            r->rx_c2h++;
        } else if (crc) {
            r->rx_crc++;
        } else if (plen <= 4u + 10u || RX_DESC_SZ + shift + drv + plen > RX_BUF_SZ) {
            r->rx_bad++;
        } else {
            const UINT8 *ps = b + RX_DESC_SZ + shift;
            const UINT8 *f = ps + drv;
            INT32 rssi = physt && drv >= 32 ? rtw_phy_rssi(r, ps) : -100;

            r->rx_frames++;
            /* в конце кадра - FCS (4 байта, BIT_APP_FCS): не нужен */
            wlan_rx(&r->hw, f, plen - 4u, rssi, r->channel);
        }

        rtw_rx_slot_reset(r, r->rx_rp);
        r->rx_rp = (r->rx_rp + 1u) % RX_RING_LEN;
    }

    w16(r, RTK_PCI_RXBD_IDX_MPDUQ, (UINT16)r->rx_rp);
}

/* ================================================================
 * Раз в 2 с: чувствительность приёмника (DIG) по числу "ложных
 * тревог" модема (rtw_phy_dig) и сила сигнала для прошивки
 * ================================================================ */

static void rtw_dm(RTW *r)
{
    UINT64 now = net_now_ms();

    if (now - r->last_dm_ms < 2000u)
        return;

    r->last_dm_ms = now;

    /* счётчики ложных тревог (rtw8821c_false_alarm_statistics) */
    BOOLEAN cck = (r32(r, REG_RXPSEL) & (1u << 28)) != 0;
    UINT32 fa = r16(r, REG_FA_OFDM) + (cck ? r16(r, REG_FA_CCK) : 0u);

    w32_set(r, REG_FAS, 1u << 17);
    w32_clr(r, REG_FAS, 1u << 17);
    w32_clr(r, REG_RXDESC, 1u << 15);
    w32_set(r, REG_RXDESC, 1u << 15);
    w32_set(r, REG_CNTRST, 1u << 0);
    w32_clr(r, REG_CNTRST, 1u << 0);

    r->last_fa = fa;

    if (r->scanning)
        return;

    UINT32 th[3];
    UINT8 step[3] = { 4, 3, 2 };
    UINT8 dmax, dmid, dmin, low, up;
    UINT8 rssi = 0;

    if (r->linked) {
        th[0] = 750; th[1] = 500; th[2] = 250;
        INT32 q = 100 + r->rssi;
        rssi = (UINT8)(q < 0 ? 0 : q > 100 ? 100 : q);
        dmax = 0x5A; dmid = 0x40; dmin = 0x1C;
        if (rssi < dmin)
            rssi = dmin;
    } else {
        th[0] = 5000; th[1] = 4000; th[2] = 2000;
        dmax = 0x2A; dmid = 0x26; dmin = 0x1C;
        rssi = dmin;
    }

    if (dmax > rssi + 15u)
        dmax = (UINT8)(rssi + 15u);

    low = rssi < dmin ? dmin : rssi > dmid ? dmid : rssi;
    up = (UINT8)(low + 15u);
    if (up < dmin)
        up = dmin;
    if (up > dmax)
        up = dmax;

    int igi = r->igi;

    for (UINTN i = 0; i < 3; i++)
        if (fa > th[i]) {
            igi += step[i];
            break;
        }
    igi -= 2;

    if (igi < low)
        igi = low;
    if (igi > up)
        igi = up;

    if ((UINT8)igi != r->igi) {
        r->igi = (UINT8)igi;
        w32_mask(r, REG_RXIGI_A, 0x7F, r->igi);
    }

    if (r->linked) {
        UINT8 h[8];
        memset(h, 0, sizeof(h));
        h[0] = H2C_CMD_RSSI_MONITOR;
        h[1] = 0;                                     /* mac_id */
        h[3] = rssi;
        rtw_h2c_cmd(r, h);
    }
}

static void rtw_poll(WLAN_HW *hw)
{
    RTW *r = (RTW *)hw->priv;

    if (!r->fw_running)
        return;

    rtw_rx(r);
    rtw_dm(r);
}

/* ================================================================
 * Сведения ("wifi", "wifi debug")
 * ================================================================ */

static void rtw_info(WLAN_HW *hw, SIMPLE_TEXT_OUTPUT_INTERFACE *out, BOOLEAN debug)
{
    RTW *r = (RTW *)hw->priv;

    kprintf(out, "  chip: RTL8821C cut %c, %s, RFE option %u%s, firmware %u.%u %s\n",
            'A' + r->cut, (r->sys_cfg1 & BIT_RF_TYPE_ID) ? "2T2R" : "1T1R", r->rfe,
            r->rfe_btg ? " (2.4 GHz via BT path)" : "", r->fw_ver, r->fw_sub,
            r->fw_running ? "running" : "NOT running");

    if (!debug)
        return;

    kprintf(out, "  PCI %u:%u.%u, registers at 0x%llx; stage: %s%s%s\n", r->bus, r->dev, r->fn,
            r->bar, r->stage ? r->stage : "-", r->err[0] ? ", error: " : "", r->err);
    kprintf(out, "  eFuse: xtal 0x%02x, board 0x%02x, BT 0x%02x (%s antenna, coex %s), "
                 "swing %u/%u\n",
            r->xtal, r->board_opt, r->bt_setting, r->share_ant ? "shared" : "own",
            r->btcoex ? "yes" : "no", r->swing_2g, r->swing_5g);
    if (r->hwcap_ok)
        kprintf(out, "  firmware says: streams %u, bandwidth caps 0x%x, antennas %u, protocol %u\n",
                r->hwcap_nss, r->hwcap_bw, r->hwcap_ant, r->hwcap_ptcl);
    kprintf(out, "  channel %u, gain IGI 0x%02x (default 0x%02x), false alarms %u/2s, "
                 "IQK %s in %u ms\n",
            r->channel, r->igi, r->igi_default, r->last_fa, r->iqk_ok ? "OK" : "not done/failed",
            r->iqk_ms);
    kprintf(out, "  TX power index (0.5 dB): 1M %u, 11M %u, 6M %u, 54M %u, MCS7 %u\n",
            r->pwr_idx[0x00], r->pwr_idx[0x03], r->pwr_idx[0x04], r->pwr_idx[0x0B],
            r->pwr_idx[0x13]);
    kprintf(out, "  frames: rx %llu (CRC errors %llu, chip messages %llu, bad %llu), "
                 "tx %llu (ring full %llu)\n",
            r->rx_frames, r->rx_crc, r->rx_c2h, r->rx_bad, r->tx_frames, r->tx_full);
    kprintf(out, "  regs: CR 0x%08x RCR 0x%08x MCUFW 0x%08x SYS_CFG1 0x%08x\n",
            r32(r, REG_CR), r32(r, REG_RCR), r32(r, REG_MCUFW_CTRL), r->sys_cfg1);
    kprintf(out, "        RXBD idx 0x%08x MGQ idx 0x%08x BEQ idx 0x%08x RFE 0x%08x/0x%08x\n",
            r32(r, RTK_PCI_RXBD_IDX_MPDUQ), r32(r, g_rtw_q[Q_MGMT].idx_reg),
            r32(r, g_rtw_q[Q_BE].idx_reg), r32(r, REG_RFECTL), r32(r, REG_RFE_CTRL8));
    kprintf(out, "        RF18 0x%05x, LED_CFG 0x%08x, BT_INFO 0x%04x\n",
            rf_read(r, RF_CFGCH, RFREG_MASK), r32(r, REG_LED_CFG), r16(r, REG_WIFI_BT_INFO));
}

/* ================================================================
 * Включение
 * ================================================================ */

static void rtw_pcie_cfg(RTW *r)
{
    /* параметры PHY шины PCIe gen1 (pcie_gen1_param_8821c): MDIO 0x09 = 0x6380 */
    w16(r, REG_MDIO_V1, 0x6380);
    w8(r, REG_PCIE_MIX_CFG, 0x09);
    w8(r, REG_PCIE_MIX_CFG + 3, 0);
    w32_mask(r, REG_PCIE_MIX_CFG, 1u << 5, 1);

    for (UINTN i = 0; i < 20 && r32_mask(r, REG_PCIE_MIX_CFG, 1u << 5); i++)
        udelay(10);

    /* RTL8821CE: выключить "таймаут завершения" PCIe (как Linux) -
       иначе чип иногда пропадает с шины */
    UINT8 cap = pci_find_cap(r->bus, r->dev, r->fn, 0x10);

    if (cap) {
        UINT32 v = pci_config_read32(r->bus, r->dev, r->fn, (UINT8)(cap + 0x28));
        pci_config_write32(r->bus, r->dev, r->fn, (UINT8)(cap + 0x28), v | (1u << 4));
    }
}

BOOLEAN rtw8821c_attach(UINT8 bus, UINT8 dev, UINT8 fn, SIMPLE_TEXT_OUTPUT_INTERFACE *out,
                        WLAN_HW **hw_out)
{
    RTW *r = &g_rtw;

    if (r->used && r->fw_running) {
        *hw_out = &r->hw;
        return TRUE;
    }

    BOOLEAN first = !r->used;

    if (first) {
        memset(r, 0, sizeof(*r));
        r->bus = bus;
        r->dev = dev;
        r->fn = fn;
    }
    r->err[0] = '\0';

    /* описание для net/wlan.c - сразу: функции драйвера (например,
       rtw_set_channel ниже) берут своё состояние из hw.priv */
    r->hw.driver = "rtw8821c";
    r->hw.model = "Realtek RTL8821CE (802.11ac)";
    r->hw.set_channel = rtw_set_channel;
    r->hw.tx = rtw_tx;
    r->hw.set_scan = rtw_set_scan;
    r->hw.set_bssid = rtw_set_bssid;
    r->hw.set_link = rtw_set_link;
    r->hw.poll = rtw_poll;
    r->hw.report_rssi = rtw_report_rssi;
    r->hw.info = rtw_info;
    r->hw.priv = r;

    /* --- шина: разбудить (D0), найти регистры (BAR2, 64 КиБ) --- */
    r->stage = "PCI";
    net_pci_wake(bus, dev, fn);

    UINT32 raw = pci_config_read32(bus, dev, fn, 0x18);

    if (raw == 0 || (raw & 1u)) {
        kprintf(out, "  no memory registers (BAR2 = 0x%08x)\n", raw);
        return rtw_fail(r, "no memory BAR");
    }

    r->bar = pci_read_bar_address(bus, dev, fn, 0x18);
    vmm_map_mmio(r->bar, 0x10000u, VMM_UC);
    pci_enable_device(bus, dev, fn);

    if (r32(r, REG_SYS_CFG1) == 0xFFFFFFFFu) {
        kprintf(out, "  chip does not answer on the bus (all registers 0xFFFFFFFF)\n");
        return rtw_fail(r, "no answer on PCI");
    }

    /* прерывания чипа - выключены (работаем опросом) */
    w32(r, RTK_PCI_HIMR0, 0);
    w32(r, RTK_PCI_HIMR1, 0);
    w32(r, RTK_PCI_HIMR3, 0);

    r->sys_cfg1 = r32(r, REG_SYS_CFG1);
    r->cut = (UINT8)((r->sys_cfg1 >> 12) & 0xFu);
    kprintf(out, "  step 1/6: chip found - RTL8821C cut %c, SYS_CFG1 0x%08x\n", 'A' + r->cut,
            r->sys_cfg1);

    if (first && !rtw_alloc_rings(r)) {
        kprintf(out, "  out of memory for DMA rings\n");
        return rtw_fail(r, "out of memory");
    }
    r->used = TRUE;

    rtw_pcie_cfg(r);

    /* --- питание --- */
    r->stage = "power on";
    rtw_hci_setup(r);

    if (!rtw_mac_power_on(r)) {
        kprintf(out, "  step 2/6: power on FAILED: %s\n", r->err);
        return FALSE;
    }
    kprintf(out, "  step 2/6: power on - OK (CR 0x%02x)\n", r8(r, REG_CR));

    /* --- прошивка --- */
    r->stage = "firmware";
    w8(r, REG_C2HEVT, 0xFD);                  /* C2H_HW_FEATURE_DUMP: доложить о себе */

    UINT64 t0 = net_now_ms();

    if (!rtw_fw_download(r)) {
        kprintf(out, "  step 3/6: firmware FAILED: %s\n", r->err);
        return FALSE;
    }
    kprintf(out, "  step 3/6: firmware %u.%u loaded and running (%llu ms)\n", r->fw_ver,
            r->fw_sub, net_now_ms() - t0);

    /* --- eFuse --- */
    r->stage = "eFuse";

    if (!rtw_efuse_read(r)) {
        kprintf(out, "  step 4/6: eFuse FAILED: %s\n", r->err);
        return FALSE;
    }

    rtw_read_hw_feature(r);

    kprintf(out, "  step 4/6: eFuse - MAC %02x:%02x:%02x:%02x:%02x:%02x, RFE option %u%s\n",
            r->mac[0], r->mac[1], r->mac[2], r->mac[3], r->mac[4], r->mac[5], r->rfe,
            r->hwcap_ok ? "" : " (no report from firmware)");

    if (!(r->rfe == 0 || r->rfe == 2 || r->rfe == 4 || r->rfe == 6)) {
        kprintf(out, "  this board variant (RFE %u) is not supported by Realtek's tables\n",
                r->rfe);
        return rtw_fail(r, "unsupported RFE option");
    }

    /* --- MAC --- */
    r->stage = "MAC init";

    if (!rtw_mac_init(r)) {
        kprintf(out, "  step 5/6: MAC setup FAILED: %s\n", r->err);
        return FALSE;
    }

    /* --- модем и радио --- */
    r->stage = "radio tables";

    if (!rtw_phy_init(r, out)) {
        kprintf(out, "  step 5/6: radio setup FAILED: %s\n", r->err);
        return FALSE;
    }

    /* прошивке: где её буфер и что за радио (general/phydm info) */
    UINT8 gi[4] = { 0, 0, 508 - 460, 0 };
    rtw_h2c_pkt(r, H2C_PKT_GENERAL_INFO, gi, 4);

    UINT8 pi[8] = { r->rfe, 4 /* FW_RF_1T1R */, r->cut, 0x11, 0, 0, 0, 0 };
    rtw_h2c_pkt(r, H2C_PKT_PHYDM_INFO, pi, 8);

    rtw_coex_init(r);

    /* фильтр приёма по умолчанию (rtw_core_init): свои, широковещательные,
       групповые кадры + FCS и состояние модема в конце/начале */
    w32(r, REG_RCR, BIT_APP_FCS | BIT_APP_MIC | BIT_APP_ICV | BIT_PKTCTL_DLEN |
                    BIT_HTC_LOC_CTRL | BIT_APP_PHYSTS | BIT_AB | BIT_AM | BIT_APM |
                    BIT_VHT_DACK);

    /* свой адрес - порт 0; тип сети "нет связи"; маяки - включены */
    for (UINT32 i = 0; i < 6; i++)
        w8(r, REG_MACID + i, r->mac[i]);
    w32_mask(r, REG_CR, 0x30000u, 0);
    w8(r, REG_BCN_CTRL, BIT_EN_BCN_FUNCTION);

    r->stage = "channel";
    r->channel = 0;
    rtw_set_channel(&r->hw, 1);

    kprintf(out, "  step 5/6: MAC, modem and radio configured (channel 1, TX power index %u)\n",
            r->pwr_idx[DESC_RATE1M]);

    r->stage = "running";
    memcpy(r->hw.mac, r->mac, 6);

    klog("rtw8821c: up - firmware %u.%u, MAC %02x:%02x:%02x:%02x:%02x:%02x, RFE %u\n",
         r->fw_ver, r->fw_sub, r->mac[0], r->mac[1], r->mac[2], r->mac[3], r->mac[4],
         r->mac[5], r->rfe);

    *hw_out = &r->hw;
    return TRUE;
}
