/*
 * drivers/hda.c - звук: Intel High Definition Audio (этап 10, А).
 * Часть MyOS; общие объявления - в myos.h.
 *
 * Звук в ноутбуке - это две микросхемы:
 *   * контроллер HDA (в чипсете Intel, PCI класс 04:03) - умеет только
 *     гонять данные: берёт отсчёты из памяти (DMA) и шлёт по шине
 *     HD Audio "link"; и возит команды ("глаголы", verbs) к кодекам;
 *   * кодек (у HP 250 G7 - Realtek ALC236) - сам ЦАП, усилители,
 *     переключатели и гнёзда. Внутри он - граф "виджетов" (узлов):
 *     ЦАП (DAC) -> смесители/переключатели -> "пины" (динамики,
 *     наушники). Каждый узел отвечает на глаголы: "твои параметры?",
 *     "с кем соединён?", "включи усилитель", "бери поток номер 1".
 *
 * Что делаем (по спецификации Intel HD Audio 1.0a и драйверу Linux):
 *   1. сброс контроллера (GCTL.CRST), кодеки откликаются в STATESTS;
 *   2. кольца команд CORB (туда пишем глаголы) и RIRB (оттуда читаем
 *      ответы) в памяти; если кольца молчат - "немедленные команды"
 *      через регистры ICOI/ICII (они есть почти у всех);
 *   3. обходим граф кодека: находим пины-выходы (динамик, наушники,
 *      линейный выход) и путь от каждого к ЦАП; включаем питание,
 *      усилители (0 дБ, без "mute"), выходы пинов и EAPD (внешний
 *      усилитель динамиков - без него у многих ноутбуков тишина);
 *   4. поток вывода: 48000 Гц, 16 бит, стерео. Буфер - кольцо в памяти
 *      (64 КиБ = 1/3 секунды), список его кусков (BDL) отдан контроллеру,
 *      и тот крутит кольцо бесконечно. Где он сейчас - регистр LPIB.
 *
 * Поток ядра "hda" раз в 10 мс смотрит LPIB: сыгранное стирает (тишина
 * вместо повтора старого звука, если программа не успела дописать), а
 * программы (системный вызов audio) пишут впереди него. Громкость -
 * программная (отсчёты умножаются при записи в кольцо): так она
 * одинакова у любого кодека. Наушники: раз в полсекунды спрашиваем пин
 * наушников "вставлен ли штекер" - вставлен, динамик выключаем.
 *
 * Память кольца - обычная (кэшируемая); у контроллеров Intel Skylake+
 * слежка за кэшем (snoop) бывает выключена битом NSNPEN - включаем её,
 * а после записи ещё и сбрасываем строки кэша (clflush): так надёжно
 * при любом раскладе.
 */
#include "myos.h"

/* ---------------- регистры контроллера ---------------- */
#define HDA_GCAP      0x00      /* 16: число потоков вывода/ввода */
#define HDA_VMIN      0x02
#define HDA_VMAJ      0x03
#define HDA_GCTL      0x08      /* 32: бит 0 CRST - "не в сбросе" */
#define HDA_STATESTS  0x0E      /* 16: какие кодеки откликнулись */
#define HDA_INTCTL    0x20
#define HDA_CORBLBASE 0x40
#define HDA_CORBUBASE 0x44
#define HDA_CORBWP    0x48      /* 16 */
#define HDA_CORBRP    0x4A      /* 16: бит 15 - сброс указателя */
#define HDA_CORBCTL   0x4C      /* 8: бит 1 - DMA кольца идёт */
#define HDA_CORBSIZE  0x4E      /* 8 */
#define HDA_RIRBLBASE 0x50
#define HDA_RIRBUBASE 0x54
#define HDA_RIRBWP    0x58      /* 16: бит 15 - сброс указателя */
#define HDA_RINTCNT   0x5A      /* 16 */
#define HDA_RIRBCTL   0x5C      /* 8 */
#define HDA_RIRBSTS   0x5D
#define HDA_RIRBSIZE  0x5E
#define HDA_ICOI      0x60      /* немедленная команда */
#define HDA_ICII      0x64      /* её ответ */
#define HDA_ICIS      0x68      /* 16: бит 0 занят, бит 1 ответ готов */

/* регистры потока n: 0x80 + 0x20 * n */
#define SD_CTL        0x00      /* 24 бита: 0 сброс, 1 RUN, [23:20] метка потока */
#define SD_STS        0x03
#define SD_LPIB       0x04      /* где сейчас контроллер (байт в кольце) */
#define SD_CBL        0x08      /* длина кольца */
#define SD_LVI        0x0C      /* номер последнего куска BDL */
#define SD_FMT        0x12      /* формат */
#define SD_BDPL       0x18
#define SD_BDPU       0x1C

/* формат: 48 кГц (база 48, множитель 1, делитель 1), 16 бит, 2 канала */
#define HDA_FMT_48K_16_2   0x0011u

#define HDA_STREAM_TAG     1u

/* ---------------- глаголы кодека ---------------- */
#define V_GET_PARAM       0xF00
#define V_GET_CONN_SEL    0xF01
#define V_GET_CONN_LIST   0xF02
#define V_SET_CONN_SEL    0x701
#define V_SET_POWER       0x705
#define V_SET_STREAM      0x706
#define V_SET_PIN_CTL     0x707
#define V_GET_PIN_SENSE   0xF09
#define V_SET_EAPD        0x70C
#define V_GET_CONFIG      0xF1C
#define V_SET_FORMAT4     0x2       /* 4-битные глаголы (16 бит данных) */
#define V_SET_AMP4        0x3

/* параметры (V_GET_PARAM) */
#define P_VENDOR_ID       0x00
#define P_NODE_COUNT      0x04
#define P_FG_TYPE         0x05
#define P_WIDGET_CAPS     0x09
#define P_PIN_CAPS        0x0C
#define P_IN_AMP_CAPS     0x0D
#define P_CONN_LEN        0x0E
#define P_OUT_AMP_CAPS    0x12

/* типы виджетов */
#define W_DAC     0
#define W_ADC     1
#define W_MIXER   2
#define W_SELECT  3
#define W_PIN     4

/* куда смотрит пин (конфигурация по умолчанию, биты 20..23) */
#define DEV_LINE_OUT  0
#define DEV_SPEAKER   1
#define DEV_HP_OUT    2

#define HDA_MAX_NODES   128
#define HDA_MAX_OUTS    4
#define HDA_BUF_BYTES   65536u      /* кольцо: ~0.34 с при 192000 байт/с */
#define HDA_BDL_N       4u          /* 4 куска по 16 КиБ */

typedef struct {
    UINT8  type;
    UINT8  nconn;
    UINT8  conn[16];                /* с кем соединён (входы) */
    UINT32 caps;
    UINT32 pincfg;
    UINT32 pincaps;
    UINT32 out_amp, in_amp;
} HDA_NODE;

typedef struct {
    UINT8 pin;                      /* пин (гнездо, динамик) */
    UINT8 dev;                      /* DEV_* */
    UINT8 path[8];                  /* путь pin -> ... -> DAC */
    UINT8 npath;
    UINT8 dac;
    BOOLEAN on;                     /* выход пина включён */
} HDA_OUT;

HDA_INFO g_hda;

static UINT64 g_mmio;               /* BAR0 */
static volatile UINT32 *g_corb;
static volatile UINT64 *g_rirb;
static UINT32 g_corb_n, g_rirb_n;
static UINT16 g_rirb_rp;
static BOOLEAN g_immediate;         /* кольца молчат - немедленные команды */
static UINT8  g_cad;                /* адрес кодека на шине */
static UINT8  g_afg;                /* узел "аудио-функции" кодека */
static HDA_NODE g_node[HDA_MAX_NODES];
static HDA_OUT  g_out[HDA_MAX_OUTS];
static UINTN    g_nout;
static UINT32   g_sd;               /* смещение регистров потока вывода */

static UINT8   *g_buf;              /* кольцо отсчётов (P2V) */
static UINT64  *g_bdl;

/* где мы в кольце: счётчики байт "от начала" (не по модулю) */
static volatile UINT64 g_played;    /* сколько сыграл контроллер */
static volatile UINT64 g_written;   /* докуда записано */
static UINT32 g_last_lpib;
static BOOLEAN g_streaming;         /* программа пишет подряд (для счёта недоборов) */

static KMUTEX g_hda_mutex = KMUTEX_INIT("hda");

/* ---------------- регистры ---------------- */

static UINT8  rd8(UINT32 r)  { return *(volatile UINT8 *)P2V(g_mmio + r); }
static UINT16 rd16(UINT32 r) { return *(volatile UINT16 *)P2V(g_mmio + r); }
static UINT32 rd32(UINT32 r) { return *(volatile UINT32 *)P2V(g_mmio + r); }
static void wr8(UINT32 r, UINT8 v)   { *(volatile UINT8 *)P2V(g_mmio + r) = v; }
static void wr16(UINT32 r, UINT16 v) { *(volatile UINT16 *)P2V(g_mmio + r) = v; }
static void wr32(UINT32 r, UINT32 v) { *(volatile UINT32 *)P2V(g_mmio + r) = v; }

/* Сбросить строки кэша [p, p+n) в память - чтобы контроллер (DMA)
   увидел записанное, а мы - записанное им */
static void hda_flush(const volatile void *p, UINTN n)
{
    UINTN a = (UINTN)p & ~(UINTN)63;
    UINTN end = (UINTN)p + n;

    for (; a < end; a += 64)
        __asm__ __volatile__("clflush (%0)" : : "r"(a) : "memory");

    __asm__ __volatile__("mfence" ::: "memory");
}

/* Ждать, пока (rd & mask) == want; не дольше ms миллисекунд */
static BOOLEAN wait16(UINT32 reg, UINT16 mask, UINT16 want, UINT32 ms)
{
    UINT64 end = kx_uptime_us() + (UINT64)ms * 1000u;

    while ((rd16(reg) & mask) != want) {
        if (kx_uptime_us() > end)
            return FALSE;
        cpu_pause();
    }

    return TRUE;
}

static BOOLEAN wait8(UINT32 reg, UINT8 mask, UINT8 want, UINT32 ms)
{
    UINT64 end = kx_uptime_us() + (UINT64)ms * 1000u;

    while ((rd8(reg) & mask) != want) {
        if (kx_uptime_us() > end)
            return FALSE;
        cpu_pause();
    }

    return TRUE;
}

static void udelay(UINT32 us)
{
    UINT64 end = kx_uptime_us() + us;

    while (kx_uptime_us() < end)
        cpu_pause();
}

/* ---------------- команды кодеку ---------------- */

/* Немедленная команда: регистры ICOI/ICIS/ICII, без колец */
static BOOLEAN cmd_immediate(UINT32 verb, UINT32 *resp)
{
    if (!wait16(HDA_ICIS, 1, 0, 10))
        return FALSE;

    wr16(HDA_ICIS, 2);                   /* сбросить "ответ готов" */
    wr32(HDA_ICOI, verb);
    wr16(HDA_ICIS, 1);                   /* отправить */

    UINT64 end = kx_uptime_us() + 20000u;

    while ((rd16(HDA_ICIS) & 3) != 2) {
        if (kx_uptime_us() > end)
            return FALSE;
        cpu_pause();
    }

    *resp = rd32(HDA_ICII);
    return TRUE;
}

/* Через кольца: глагол - в CORB, ответ - новая запись RIRB */
static BOOLEAN cmd_rings(UINT32 verb, UINT32 *resp)
{
    UINT16 wp = (UINT16)((rd16(HDA_CORBWP) + 1u) % g_corb_n);

    g_corb[wp] = verb;
    hda_flush(&g_corb[wp], 4);
    wr16(HDA_CORBWP, wp);

    UINT64 end = kx_uptime_us() + 50000u;

    for (;;) {

        UINT16 rirb_wp = (UINT16)(rd16(HDA_RIRBWP) & 0xFF);

        while (g_rirb_rp != rirb_wp) {

            g_rirb_rp = (UINT16)((g_rirb_rp + 1u) % g_rirb_n);
            hda_flush(&g_rirb[g_rirb_rp], 8);

            UINT64 e = g_rirb[g_rirb_rp];

            /* сбросить "ответ пришёл": после RINTCNT ответов контроллер
               ждёт этого и новые ответы не пишет (так и в QEMU) */
            wr8(HDA_RIRBSTS, 0x05);

            /* бит 4 второго слова - "сам сообщил" (штекер вставили);
               такие пропускаем - ждём ответ на наш глагол */
            if (!((e >> 32) & 0x10)) {
                *resp = (UINT32)e;
                return TRUE;
            }
        }

        if (kx_uptime_us() > end)
            return FALSE;

        cpu_pause();
    }
}

static UINT32 hda_cmd(UINT8 nid, UINT32 verb, UINT32 payload)
{
    UINT32 v;

    if (verb <= 0xF)
        v = ((UINT32)g_cad << 28) | ((UINT32)nid << 20) | (verb << 16) | (payload & 0xFFFFu);
    else
        v = ((UINT32)g_cad << 28) | ((UINT32)nid << 20) | (verb << 8) | (payload & 0xFFu);

    UINT32 r = 0;

    if (!g_immediate) {
        if (cmd_rings(v, &r))
            return r;
        /* кольца молчат - дальше немедленными командами */
        klog("hda: CORB/RIRB did not answer (CORB wp %u rp %u ctl %x, RIRB wp %u sts %x ctl %x) - "
             "using immediate commands\n", rd16(HDA_CORBWP), rd16(HDA_CORBRP), rd8(HDA_CORBCTL),
             rd16(HDA_RIRBWP), rd8(HDA_RIRBSTS), rd8(HDA_RIRBCTL));
        g_immediate = TRUE;
        g_hda.immediate = TRUE;
    }

    if (!cmd_immediate(v, &r)) {
        g_hda.cmd_timeouts++;
        return 0xFFFFFFFFu;
    }

    return r;
}

static UINT32 param(UINT8 nid, UINT32 p)
{
    return hda_cmd(nid, V_GET_PARAM, p);
}

/* ---------------- контроллер ---------------- */

static BOOLEAN hda_reset(void)
{
    /* остановить кольца */
    wr8(HDA_CORBCTL, 0);
    wr8(HDA_RIRBCTL, 0);
    wait8(HDA_CORBCTL, 2, 0, 10);
    wait8(HDA_RIRBCTL, 2, 0, 10);

    /* сброс: CRST=0, дождаться, CRST=1, дождаться */
    wr32(HDA_GCTL, rd32(HDA_GCTL) & ~1u);

    UINT64 end = kx_uptime_us() + 100000u;
    while (rd32(HDA_GCTL) & 1u) {
        if (kx_uptime_us() > end)
            return FALSE;
        cpu_pause();
    }

    udelay(200);
    wr32(HDA_GCTL, rd32(HDA_GCTL) | 1u);

    end = kx_uptime_us() + 100000u;
    while (!(rd32(HDA_GCTL) & 1u)) {
        if (kx_uptime_us() > end)
            return FALSE;
        cpu_pause();
    }

    /* кодекам - время представиться (спецификация: 521 мкс) */
    sched_sleep_ms(2);
    return TRUE;
}

/* Размер кольца по регистру возможностей: 256, 16 или 2 записи */
static UINT32 ring_size(UINT32 reg, UINT8 *code)
{
    UINT8 cap = rd8(reg);

    if (cap & 0x40) { *code = 2; return 256; }
    if (cap & 0x20) { *code = 1; return 16; }
    *code = 0;
    return 2;
}

static BOOLEAN hda_rings_init(void)
{
    UINT64 page = pmm_alloc_pages(1, 0);

    if (page == 0)
        return FALSE;

    UINT8 *p = (UINT8 *)P2V(page);

    for (UINTN i = 0; i < 4096; i++)
        p[i] = 0;
    hda_flush(p, 4096);

    /* CORB - первые 1 КиБ, RIRB - со смещения 2 КиБ (2 КиБ) */
    g_corb = (volatile UINT32 *)p;
    g_rirb = (volatile UINT64 *)(p + 2048);

    UINT8 code;

    g_corb_n = ring_size(HDA_CORBSIZE, &code);
    wr8(HDA_CORBSIZE, (UINT8)((rd8(HDA_CORBSIZE) & ~3u) | code));
    wr32(HDA_CORBLBASE, (UINT32)page);
    wr32(HDA_CORBUBASE, (UINT32)(page >> 32));

    /* сброс указателя чтения CORB: 1, дождаться 1, 0, дождаться 0
       (у части контроллеров бит не читается - не страшно) */
    wr16(HDA_CORBRP, 0x8000);
    wait16(HDA_CORBRP, 0x8000, 0x8000, 5);
    wr16(HDA_CORBRP, 0);
    wait16(HDA_CORBRP, 0x8000, 0, 5);
    wr16(HDA_CORBWP, 0);

    g_rirb_n = ring_size(HDA_RIRBSIZE, &code);
    wr8(HDA_RIRBSIZE, (UINT8)((rd8(HDA_RIRBSIZE) & ~3u) | code));
    wr32(HDA_RIRBLBASE, (UINT32)(page + 2048));
    wr32(HDA_RIRBUBASE, (UINT32)((page + 2048) >> 32));
    wr16(HDA_RIRBWP, 0x8000);
    wr16(HDA_RINTCNT, 0xFF);
    wr8(HDA_RIRBSTS, 0x05);
    g_rirb_rp = 0;

    wr8(HDA_CORBCTL, 2);
    wr8(HDA_RIRBCTL, 2);

    return TRUE;
}

/* ---------------- граф кодека ---------------- */

/* Список соединений узла (кто подаёт ему сигнал) */
static void read_conns(UINT8 nid, HDA_NODE *n)
{
    UINT32 len = param(nid, P_CONN_LEN);
    BOOLEAN lng = (len & 0x80u) != 0;     /* записи по 16 бит */
    UINT32 cnt = len & 0x7Fu;

    n->nconn = 0;

    for (UINT32 i = 0; i < cnt && n->nconn < 16; ) {

        UINT32 r = hda_cmd(nid, V_GET_CONN_LIST, i);
        UINT32 per = lng ? 2 : 4;

        for (UINT32 k = 0; k < per && i < cnt && n->nconn < 16; k++, i++) {
            UINT32 e = lng ? (r >> (16 * k)) & 0xFFFFu : (r >> (8 * k)) & 0xFFu;
            UINT32 id = lng ? (e & 0x7FFFu) : (e & 0x7Fu);
            BOOLEAN range = lng ? (e & 0x8000u) != 0 : (e & 0x80u) != 0;

            /* "диапазон": от предыдущего до этого */
            if (range && n->nconn > 0) {
                for (UINT32 x = n->conn[n->nconn - 1] + 1u; x <= id && n->nconn < 16; x++)
                    n->conn[n->nconn++] = (UINT8)x;
            } else if (id < HDA_MAX_NODES) {
                n->conn[n->nconn++] = (UINT8)id;
            }
        }
    }
}

/* Путь от узла до ЦАП поиском в глубину: path[0] - сам узел */
static BOOLEAN find_dac(UINT8 nid, UINT8 *path, UINT8 depth, UINT8 *npath)
{
    if (nid >= HDA_MAX_NODES || depth >= 8)
        return FALSE;

    path[depth] = nid;

    if (g_node[nid].type == W_DAC && depth > 0) {
        *npath = (UINT8)(depth + 1);
        return TRUE;
    }

    if (depth > 0 && g_node[nid].type != W_MIXER && g_node[nid].type != W_SELECT)
        return FALSE;

    for (UINT8 i = 0; i < g_node[nid].nconn; i++)
        if (find_dac(g_node[nid].conn[i], path, (UINT8)(depth + 1), npath))
            return TRUE;

    return FALSE;
}

/* Усилитель: 0 дБ (смещение из возможностей), без "mute".
   out - выходной; иначе входной номер idx. */
static void amp_unmute(UINT8 nid, BOOLEAN out, UINT8 idx)
{
    UINT32 caps = out ? g_node[nid].out_amp : g_node[nid].in_amp;

    /* у виджета нет своих возможностей усилителя - берём у AFG */
    if (caps == 0)
        caps = param(g_afg, out ? P_OUT_AMP_CAPS : P_IN_AMP_CAPS);

    UINT32 gain = caps & 0x7Fu;            /* шаг, где 0 дБ */

    UINT32 v = (out ? 0x8000u : 0x4000u) | 0x3000u /* левый и правый */ |
               ((UINT32)idx << 8) | gain;

    hda_cmd(nid, V_SET_AMP4, v);
}

static void out_enable(HDA_OUT *o, BOOLEAN on)
{
    UINT8 ctl = 0;

    if (on)
        ctl = (o->dev == DEV_HP_OUT) ? 0xC0 : 0x40;   /* HP + OUT / OUT */

    hda_cmd(o->pin, V_SET_PIN_CTL, ctl);
    o->on = on;
}

static void setup_out(HDA_OUT *o)
{
    /* питание всем узлам пути */
    for (UINT8 i = 0; i < o->npath; i++)
        hda_cmd(o->path[i], V_SET_POWER, 0);

    /* переключатели: выбрать следующий по пути вход */
    for (UINT8 i = 0; i + 1 < o->npath; i++) {

        HDA_NODE *n = &g_node[o->path[i]];
        UINT8 next = o->path[i + 1];
        UINT8 idx = 0;

        for (UINT8 k = 0; k < n->nconn; k++)
            if (n->conn[k] == next)
                idx = k;

        if (n->nconn > 1 && n->type != W_MIXER)
            hda_cmd(o->path[i], V_SET_CONN_SEL, idx);

        if (n->caps & (1u << 2))            /* есть выходной усилитель */
            amp_unmute(o->path[i], TRUE, 0);
        if (n->caps & (1u << 1))            /* есть входной */
            amp_unmute(o->path[i], FALSE, idx);
    }

    if (g_node[o->dac].caps & (1u << 2))
        amp_unmute(o->dac, TRUE, 0);

    /* EAPD - внешний усилитель динамиков/наушников */
    if (g_node[o->pin].pincaps & (1u << 16))
        hda_cmd(o->pin, V_SET_EAPD, 0x02);

    /* ЦАП: формат и номер потока */
    hda_cmd(o->dac, V_SET_FORMAT4, HDA_FMT_48K_16_2);
    hda_cmd(o->dac, V_SET_STREAM, HDA_STREAM_TAG << 4);

    out_enable(o, TRUE);
}

static const char *dev_name(UINT8 dev)
{
    return dev == DEV_SPEAKER ? "speaker" : dev == DEV_HP_OUT ? "headphones" : "line out";
}

/* Обойти кодек: узлы, выходы, пути; включить всё нужное */
static BOOLEAN codec_init(SIMPLE_TEXT_OUTPUT_INTERFACE *out)
{
    UINT32 vid = param(0, P_VENDOR_ID);

    if (vid == 0xFFFFFFFFu || vid == 0)
        return FALSE;

    g_hda.codec_vendor = vid;

    UINT32 nc = param(0, P_NODE_COUNT);
    UINT8 start = (UINT8)((nc >> 16) & 0xFF), count = (UINT8)(nc & 0xFF);

    g_afg = 0;

    for (UINT8 f = start; f < start + count; f++)
        if ((param(f, P_FG_TYPE) & 0xFF) == 1)
            g_afg = f;

    if (g_afg == 0) {
        kprintf(out, "  sound: codec %08x has no audio function\n", vid);
        return FALSE;
    }

    hda_cmd(g_afg, V_SET_POWER, 0);
    sched_sleep_ms(10);

    nc = param(g_afg, P_NODE_COUNT);
    start = (UINT8)((nc >> 16) & 0xFF);
    count = (UINT8)(nc & 0xFF);

    for (UINT32 n = start; n < (UINT32)start + count && n < HDA_MAX_NODES; n++) {

        HDA_NODE *w = &g_node[n];
        w->caps = param((UINT8)n, P_WIDGET_CAPS);
        w->type = (UINT8)((w->caps >> 20) & 0xF);

        if (w->caps & (1u << 8))              /* есть список соединений */
            read_conns((UINT8)n, w);

        if (w->caps & (1u << 2))
            w->out_amp = param((UINT8)n, P_OUT_AMP_CAPS);
        if (w->caps & (1u << 1))
            w->in_amp = param((UINT8)n, P_IN_AMP_CAPS);

        if (w->type == W_PIN) {
            w->pincfg = hda_cmd((UINT8)n, V_GET_CONFIG, 0);
            w->pincaps = param((UINT8)n, P_PIN_CAPS);
        }
    }

    /* выходы: пины, которые подключены (не "нет разъёма") и умеют вывод */
    for (UINT32 n = start; n < (UINT32)start + count && n < HDA_MAX_NODES && g_nout < HDA_MAX_OUTS; n++) {

        HDA_NODE *w = &g_node[n];

        if (w->type != W_PIN || !(w->pincaps & (1u << 4)))
            continue;

        UINT32 conn = w->pincfg >> 30;         /* 1 - разъёма нет */
        UINT8 dev = (UINT8)((w->pincfg >> 20) & 0xF);

        if (conn == 1 || (dev != DEV_LINE_OUT && dev != DEV_SPEAKER && dev != DEV_HP_OUT))
            continue;

        HDA_OUT *o = &g_out[g_nout];
        o->pin = (UINT8)n;
        o->dev = dev;

        if (!find_dac((UINT8)n, o->path, 0, &o->npath))
            continue;

        o->dac = o->path[o->npath - 1];
        g_nout++;
    }

    if (g_nout == 0) {
        kprintf(out, "  sound: codec %08x - no output pins with a path to a DAC\n", vid);
        return FALSE;
    }

    for (UINTN i = 0; i < g_nout; i++)
        setup_out(&g_out[i]);

    return TRUE;
}

/* ---------------- поток вывода ---------------- */

static BOOLEAN stream_init(void)
{
    UINT16 gcap = rd16(HDA_GCAP);
    UINT32 iss = (gcap >> 8) & 0xF;
    UINT32 oss = (gcap >> 12) & 0xF;

    if (oss == 0)
        return FALSE;

    /* первый поток вывода идёт после потоков ввода */
    g_sd = 0x80u + 0x20u * iss;

    UINT64 bufp = pmm_alloc_pages(HDA_BUF_BYTES / 4096u, 0);
    UINT64 bdlp = pmm_alloc_pages(1, 0);

    if (bufp == 0 || bdlp == 0)
        return FALSE;

    g_buf = (UINT8 *)P2V(bufp);
    g_bdl = (UINT64 *)P2V(bdlp);

    for (UINTN i = 0; i < HDA_BUF_BYTES; i++)
        g_buf[i] = 0;
    hda_flush(g_buf, HDA_BUF_BYTES);

    /* BDL: адрес (64), длина (32), "прерывание по концу куска" (32) */
    UINT32 piece = HDA_BUF_BYTES / HDA_BDL_N;

    for (UINT32 i = 0; i < HDA_BDL_N; i++) {
        g_bdl[i * 2] = bufp + (UINT64)i * piece;
        g_bdl[i * 2 + 1] = (UINT64)piece;
    }
    hda_flush(g_bdl, HDA_BDL_N * 16u);

    /* сброс потока */
    wr8(g_sd + SD_CTL, 1);
    wait8(g_sd + SD_CTL, 1, 1, 10);
    wr8(g_sd + SD_CTL, 0);
    wait8(g_sd + SD_CTL, 1, 0, 10);

    wr32(g_sd + SD_BDPL, (UINT32)bdlp);
    wr32(g_sd + SD_BDPU, (UINT32)(bdlp >> 32));
    wr32(g_sd + SD_CBL, HDA_BUF_BYTES);
    wr16(g_sd + SD_LVI, (UINT16)(HDA_BDL_N - 1));
    wr16(g_sd + SD_FMT, (UINT16)HDA_FMT_48K_16_2);
    wr8(g_sd + SD_CTL + 2, (UINT8)(HDA_STREAM_TAG << 4));
    wr8(g_sd + SD_STS, 0x1C);                     /* сбросить флаги */

    /* поехали: контроллер крутит кольцо, пока его не остановят */
    wr8(g_sd + SD_CTL, 2);

    g_last_lpib = rd32(g_sd + SD_LPIB);
    return TRUE;
}

/* Сколько контроллер сыграл с прошлого раза; сыгранное - стереть */
static void hda_advance(void)
{
    UINT32 lpib = rd32(g_sd + SD_LPIB);

    if (lpib >= HDA_BUF_BYTES)
        return;

    UINT32 delta = (lpib + HDA_BUF_BYTES - g_last_lpib) % HDA_BUF_BYTES;

    if (delta == 0)
        return;

    /* стереть сыгранное: если программа не успеет дописать, кольцо
       сыграет тишину, а не повторит старый звук */
    for (UINT32 i = 0; i < delta; ) {
        UINT32 at = (g_last_lpib + i) % HDA_BUF_BYTES;
        UINT32 k = HDA_BUF_BYTES - at;
        if (k > delta - i)
            k = delta - i;
        for (UINT32 j = 0; j < k; j++)
            g_buf[at + j] = 0;
        hda_flush(g_buf + at, k);
        i += k;
    }

    g_last_lpib = lpib;
    g_played += delta;
}

/* Штекер наушников: вставлен - динамик выключить (как в любой ОС) */
static void hda_jack_check(void)
{
    BOOLEAN hp = FALSE;

    for (UINTN i = 0; i < g_nout; i++)
        if (g_out[i].dev == DEV_HP_OUT && (g_node[g_out[i].pin].pincaps & (1u << 2))) {
            UINT32 s = hda_cmd(g_out[i].pin, V_GET_PIN_SENSE, 0);
            if (s != 0xFFFFFFFFu && (s & 0x80000000u))
                hp = TRUE;
        }

    if (hp == g_hda.headphones)
        return;

    g_hda.headphones = hp;

    for (UINTN i = 0; i < g_nout; i++)
        if (g_out[i].dev == DEV_SPEAKER)
            out_enable(&g_out[i], !hp);

    klog("hda: headphones %s\n", hp ? "plugged in - speaker off" : "unplugged - speaker on");
}

static void hda_thread(void *arg)
{
    (void)arg;
    UINTN n = 0;

    for (;;) {

        kmutex_lock(&g_hda_mutex);
        hda_advance();
        if (++n % 50 == 0)
            hda_jack_check();
        kmutex_unlock(&g_hda_mutex);

        sched_wake_all(&g_hda);          /* ждущим места в кольце */
        sched_sleep_ms(10);
    }
}

/* ---------------- для ядра и программ ---------------- */

/* Громкость 0..100 (программная: отсчёты умножаются при записи) */
INT32 hda_volume(INT32 v)
{
    if (v >= 0) {
        if (v > 100)
            v = 100;
        g_hda.volume = (UINT32)v;
        g_hda.muted = FALSE;
        klog("hda: volume %u%%\n", g_hda.volume);
    }

    return (INT32)g_hda.volume;
}

void hda_mute_toggle(void)
{
    g_hda.muted = !g_hda.muted;
    klog("hda: %s\n", g_hda.muted ? "muted" : "unmuted");
}

/* Сколько байт ещё не сыграно */
UINT32 hda_queued(void)
{
    if (!g_hda.ok || g_written <= g_played)
        return 0;
    return (UINT32)(g_written - g_played);
}

/*
 * Записать отсчёты (16 бит, стерео, 48 кГц) - сколько влезло в кольцо,
 * не дожидаясь. Вернуть число записанных байт (кратно 4).
 */
UINT32 hda_write(const INT16 *s, UINT32 bytes)
{
    if (!g_hda.ok)
        return 0;

    kmutex_lock(&g_hda_mutex);

    hda_advance();

    /* записанное кончилось (или программа не успела): писать с того,
       что контроллер сейчас играет, плюс чуть впереди - чтобы успеть
       до него (2 КиБ ~ 10 мс) */
    if (g_written < g_played + 1024u) {
        if (g_streaming && g_written < g_played)
            g_hda.underruns++;
        g_written = g_played + 2048u;
    }

    g_streaming = (g_hda.owner_pid != 0);

    /* место впереди контроллера: всё кольцо без куска-запаса 4 КиБ
       (туда контроллер вот-вот дойдёт) */
    UINT64 ahead = (g_written > g_played) ? g_written - g_played : 0;
    UINT32 room = (ahead + 4096u >= HDA_BUF_BYTES) ? 0 : (UINT32)(HDA_BUF_BYTES - 4096u - ahead);

    bytes &= ~3u;
    if (bytes > room)
        bytes = room & ~3u;

    UINT32 vol = g_hda.muted ? 0 : g_hda.volume;
    /* громкость по уровню слуха: квадрат (50% на регуляторе ~ -12 дБ) */
    UINT32 mul = vol * vol * 65536u / 10000u;

    for (UINT32 i = 0; i < bytes / 2u; ) {

        UINT32 at = (UINT32)((g_written + (UINT64)i * 2u) % HDA_BUF_BYTES);
        UINT32 k = (HDA_BUF_BYTES - at) / 2u;

        if (k > bytes / 2u - i)
            k = bytes / 2u - i;

        INT16 *d = (INT16 *)(g_buf + at);

        for (UINT32 j = 0; j < k; j++)
            d[j] = (INT16)(((INT32)s[i + j] * (INT32)mul) >> 16);

        hda_flush(d, k * 2u);
        i += k;
    }

    g_written += bytes;
    g_hda.bytes_played += bytes;

    kmutex_unlock(&g_hda_mutex);
    return bytes;
}

/* Ждать, пока в кольце появится место (или ms прошли) */
void hda_wait_room(UINT32 ms)
{
    UINT64 fl = kx_irq_save();
    sched_block(&g_hda, "sound", ms);
    kx_irq_restore(fl);
}

/*
 * Тон для проверки: `sound test` - 440 Гц (ля первой октавы), 1 с.
 * Синус без плавающей точки: рекуррентная формула
 *   s[n] = 2cos(w) * s[n-1] - s[n-2],  w = 2*pi*440/48000;
 * 2cos(w) = 2143922726 / 2^30, s[1] = A*sin(w); отсчёты - в Q16.
 */
void hda_test_tone(SIMPLE_TEXT_OUTPUT_INTERFACE *out, UINT32 ms)
{
    if (!g_hda.ok) {
        print(out, "No sound card (Intel HD Audio) found.\n");
        return;
    }

    if (g_hda.owner_pid != 0) {
        kprintf(out, "Sound is busy (program pid %u is playing).\n", g_hda.owner_pid);
        return;
    }

    static INT16 chunk[2 * 480];              /* 10 мс стерео */
    const INT64 k = 2143922726;
    INT64 s1 = 45270179;                      /* s[n-1]: 12000 * sin(w) * 2^16 */
    INT64 s2 = 0;                             /* s[n-2] */
    UINT32 total = ms * 48u;                  /* кадров */

    kprintf(out, "Playing a 440 Hz tone for %u ms...\n", ms);
    kcon_flush();

    UINT32 done = 0;

    while (done < total) {

        UINT32 n = (total - done > 480u) ? 480u : total - done;

        for (UINT32 i = 0; i < n; i++) {
            INT64 s0 = ((k * s1) >> 30) - s2;
            s2 = s1;
            s1 = s0;
            INT16 v = (INT16)(s1 >> 16);
            chunk[i * 2] = v;
            chunk[i * 2 + 1] = v;
        }

        UINT32 off = 0;

        while (off < n * 4u) {
            UINT32 w = hda_write((const INT16 *)((UINT8 *)chunk + off), n * 4u - off);
            off += w;
            if (off < n * 4u)
                hda_wait_room(20);
        }

        done += n;
    }

    /* дождаться, пока доиграет */
    while (hda_queued() > 0)
        hda_wait_room(20);

    print(out, "Done.\n");
}

/* ---------------- запуск ---------------- */

void hda_init(SIMPLE_TEXT_OUTPUT_INTERFACE *out)
{
    UINT8 b, d, f;

    g_hda.volume = 70;

    if (!pci_find_class(0x04, 0x03, -1, 0, &b, &d, &f)) {
        print(out, "  sound: no Intel HD Audio controller\n");
        return;
    }

    UINT32 id = pci_config_read32(b, d, f, 0x00);
    g_hda.pci_id = id;
    g_hda.bus = b;
    g_hda.dev = d;
    g_hda.fn = f;

    pci_enable_device(b, d, f);

    UINT64 bar = pci_read_bar_address(b, d, f, 0x10);

    if (bar == 0) {
        print(out, "  sound: HDA controller has no BAR0\n");
        return;
    }

    vmm_map_mmio(bar, 0x4000, VMM_UC);
    g_mmio = bar;

    /* Intel: включить слежку за кэшем (бит 11 NSNPEN регистра 0x78 -
       "без слежки" - снять), как делает Linux для Skylake и новее */
    if ((id & 0xFFFFu) == 0x8086u) {
        UINT32 devc = pci_config_read32(b, d, f, 0x78);
        if (devc & (1u << 11))
            pci_config_write32(b, d, f, 0x78, devc & ~(1u << 11));
    }

    if (!hda_reset()) {
        print(out, "  sound: HDA controller did not come out of reset\n");
        return;
    }

    UINT16 codecs = rd16(HDA_STATESTS);
    wr16(HDA_STATESTS, codecs);        /* сбросить */
    wr32(HDA_INTCTL, 0);               /* без прерываний: поток опрашивает */

    if (codecs == 0) {
        kprintf(out, "  sound: HDA %04x:%04x - no codecs answered\n", id & 0xFFFFu, id >> 16);
        return;
    }

    if (!hda_rings_init()) {
        print(out, "  sound: out of memory\n");
        return;
    }

    /* первый откликнувшийся кодек, у которого есть звук (у ноутбуков
       бывает и кодек HDMI видеокарты - у него нет пинов-динамиков) */
    for (UINT8 c = 0; c < 15; c++) {

        if (!(codecs & (1u << c)))
            continue;

        g_cad = c;
        for (UINTN i = 0; i < HDA_MAX_NODES; i++) {
            HDA_NODE z = { 0 };
            g_node[i] = z;
        }
        g_nout = 0;

        if (codec_init(out))
            break;

        g_nout = 0;
    }

    if (g_nout == 0)
        return;

    g_hda.codec_addr = g_cad;

    if (!stream_init()) {
        print(out, "  sound: no output stream\n");
        return;
    }

    g_hda.ok = TRUE;
    g_hda.nout = (UINT32)g_nout;

    for (UINTN i = 0; i < g_nout; i++) {
        g_hda.out_pin[i] = g_out[i].pin;
        g_hda.out_dac[i] = g_out[i].dac;
        g_hda.out_dev[i] = g_out[i].dev;
    }

    kthread_create("hda", hda_thread, NULL, 16);

    kprintf(out, "  sound: Intel HDA %04x:%04x, codec %u: %04x:%04x%s, outputs:",
            id & 0xFFFFu, id >> 16, g_cad, g_hda.codec_vendor >> 16, g_hda.codec_vendor & 0xFFFFu,
            g_immediate ? " (immediate commands)" : "");

    for (UINTN i = 0; i < g_nout; i++)
        kprintf(out, " %s (pin 0x%02x <- DAC 0x%02x)", dev_name(g_out[i].dev),
                g_out[i].pin, g_out[i].dac);

    print(out, "\n");
    klog("hda: ready, %u outputs, stream at 0x%x\n", (UINT32)g_nout, g_sd);
}

/* Команда sound: что за звуковая карта; sound test - тон 440 Гц */
void kernel_cmd_sound(SIMPLE_TEXT_OUTPUT_INTERFACE *out, const char *arg)
{
    while (*arg == ' ')
        arg++;

    if (kstreq(arg, "test")) {
        hda_test_tone(out, 1000);
        return;
    }

    if (*arg != '\0') {
        print(out, "Usage: sound [test]\n");
        return;
    }

    if (!g_hda.ok) {
        if (g_hda.pci_id)
            kprintf(out, "Intel HD Audio %04x:%04x found, but sound did not start "
                         "(see the [sound] line at boot).\n",
                    g_hda.pci_id & 0xFFFFu, g_hda.pci_id >> 16);
        else
            print(out, "No sound card (Intel HD Audio) found.\n");
        return;
    }

    kprintf(out, "Sound: Intel HD Audio %04x:%04x at %02x:%02x.%x, codec %u: %04x:%04x\n",
            g_hda.pci_id & 0xFFFFu, g_hda.pci_id >> 16, g_hda.bus, g_hda.dev, g_hda.fn,
            g_hda.codec_addr, g_hda.codec_vendor >> 16, g_hda.codec_vendor & 0xFFFFu);

    for (UINT32 i = 0; i < g_hda.nout; i++)
        kprintf(out, "  output: %s (pin 0x%02x <- DAC 0x%02x)%s\n", dev_name(g_hda.out_dev[i]),
                g_hda.out_pin[i], g_hda.out_dac[i],
                (g_hda.out_dev[i] == DEV_SPEAKER && g_hda.headphones) ? " - off, headphones in" : "");

    kprintf(out, "  48000 Hz, 16 bit, stereo; volume %u%%%s; played %llu KiB, underruns %u%s\n",
            g_hda.volume, g_hda.muted ? " (muted)" : "", g_hda.bytes_played / 1024u,
            g_hda.underruns, g_hda.immediate ? "; immediate commands" : "");

    if (g_hda.owner_pid)
        kprintf(out, "  playing: pid %u\n", g_hda.owner_pid);

    if (g_hda.cmd_timeouts)
        kprintf(out, "  codec command timeouts: %u\n", g_hda.cmd_timeouts);
}

/* ================================================================
 * Системный вызов audio (программы: play, doom...)
 * ================================================================ */

/* программа, которая играла, вышла (proc_reap) - звук свободен */
void hda_proc_gone(UINT32 pid)
{
    if (g_hda.owner_pid == pid) {
        g_hda.owner_pid = 0;
        g_streaming = FALSE;
    }
}

INT64 sys_audio(KPROC *p, UINT64 op, UINT64 a1, UINT64 a2)
{
    switch (op) {

    case MYOS_AUDIO_OPEN:
        if (!g_hda.ok)
            return MYOS_ENODEV;
        if (g_hda.owner_pid != 0 && g_hda.owner_pid != p->pid)
            return MYOS_EBUSY;
        g_hda.owner_pid = p->pid;
        g_streaming = FALSE;
        return 0;

    case MYOS_AUDIO_WRITE: {
        if (g_hda.owner_pid != p->pid)
            return MYOS_EBADF;
        if (!uptr_ok(p, a1, a2, FALSE))
            return MYOS_EFAULT;

        /* кусками через свой буфер: пока ждём места в кольце, память
           программы трогать не нужно */
        static INT16 tmp[2048];
        UINT64 done = 0;
        UINT64 total = a2 & ~3ull;

        while (done < total && !p->killed) {

            UINT32 n = (total - done > sizeof(tmp)) ? (UINT32)sizeof(tmp) : (UINT32)(total - done);
            memcpy(tmp, (const void *)(UINTN)(a1 + done), n);

            UINT32 off = 0;

            while (off < n && !p->killed) {
                UINT32 w = hda_write((const INT16 *)((UINT8 *)tmp + off), n - off);
                off += w;
                if (off < n)
                    hda_wait_room(20);
            }

            done += off;
        }

        return (INT64)done;
    }

    case MYOS_AUDIO_CLOSE:
        if (g_hda.owner_pid == p->pid)
            g_hda.owner_pid = 0;
        g_streaming = FALSE;
        return 0;

    case MYOS_AUDIO_DRAIN:
        g_streaming = FALSE;
        while (hda_queued() > 0 && !p->killed)
            hda_wait_room(20);
        return 0;

    case MYOS_AUDIO_VOLUME:
        if ((INT64)a1 == -2)
            hda_mute_toggle();
        else if ((INT64)a1 >= 0)
            hda_volume((INT32)a1);
        return g_hda.muted ? 0 : (INT64)g_hda.volume;

    case MYOS_AUDIO_INFO: {
        struct myos_audio_info inf;

        if (!uptr_ok(p, a1, sizeof(inf), TRUE))
            return MYOS_EFAULT;
        if (!g_hda.ok)
            return MYOS_ENODEV;

        memset(&inf, 0, sizeof(inf));
        inf.rate = 48000;
        inf.channels = 2;
        inf.bits = 16;
        inf.volume = g_hda.volume;
        inf.muted = g_hda.muted;
        inf.queued = hda_queued();
        inf.buffer = HDA_BUF_BYTES;
        inf.headphones = g_hda.headphones;
        ksnprintf(inf.device, sizeof(inf.device), "Intel HD Audio, codec %04x:%04x",
                  g_hda.codec_vendor >> 16, g_hda.codec_vendor & 0xFFFFu);
        memcpy((void *)(UINTN)a1, &inf, sizeof(inf));
        return 0;
    }
    }

    return MYOS_EINVAL;
}
