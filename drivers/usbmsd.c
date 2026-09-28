/*
 * drivers/usbmsd.c - USB-флешки и USB-диски (класс Mass Storage).
 * Часть MyOS; общие объявления - в myos.h.
 *
 * Почти все флешки говорят так: транспорт "Bulk-Only" (BOT) и
 * команды SCSI - те же, что у серверных дисков 1980-х.
 * Каждая операция - три шага по двум bulk-конечным точкам:
 *   1. CBW (Command Block Wrapper, 31 байт) по bulk OUT: подпись
 *      "USBC", номер (tag), сколько байт данных ждать и куда, сама
 *      команда SCSI (до 16 байт);
 *   2. данные - по bulk IN (чтение) или OUT (запись);
 *   3. CSW (Command Status Wrapper, 13 байт) по bulk IN: подпись
 *      "USBS", тот же tag, статус (0 = успех).
 *
 * Команды SCSI, которые нам нужны:
 *   INQUIRY (0x12)          - кто ты (производитель, модель)
 *   TEST UNIT READY (0x00)  - готов? (после втыкания флешка
 *                             несколько сотен мс "просыпается")
 *   REQUEST SENSE (0x03)    - а почему не готов? (сбрасывает ошибку)
 *   READ CAPACITY(10) (0x25)- сколько секторов и какого размера
 *   READ(10) (0x28)         - прочитать секторы
 *   WRITE(10) (0x2A)        - записать секторы
 *
 * Для остальной системы флешка - блочное устройство "usbN"
 * (drivers/blk.c): оно зовёт usb_msd_rw.
 */
#include "myos.h"

KX_MSD g_kx_msd[KX_MAX_MSD];

static UINT32 g_msd_serial = 0;     /* счётчик подключений */

/*
 * Подготовить флешку: записи, кольца, буферы и две bulk-конечные
 * точки для Configure Endpoint. Возвращает индекс или -1.
 */
INTN kx_msd_prepare(SIMPLE_TEXT_OUTPUT_INTERFACE *out, UINTN di,
                    KX_MSD_CAND *c, KX_EPCFG *eps, UINTN *neps)
{
    KX_DEV *d = &g_kx_devs[di];
    INTN mi = -1;

    for (UINTN i = 0; i < KX_MAX_MSD; i++)
        if (!g_kx_msd[i].used) {
            mi = (INTN)i;
            break;
        }

    if (mi < 0) {
        kx_out(out, "    too many USB drives - skipped\n");
        return -1;
    }

    KX_MSD *m = &g_kx_msd[mi];

    raw_zero_mem((volatile UINT8 *)m, sizeof(*m));

    m->serial = ++g_msd_serial;
    m->dev = (UINT8)di;
    m->iface = c->iface;
    m->in_addr = c->in_addr;
    m->out_addr = c->out_addr;
    m->in_dci = (UINT8)((c->in_addr & 0x0Fu) * 2u + 1u);
    m->out_dci = (UINT8)((c->out_addr & 0x0Fu) * 2u);
    m->in_mps = c->in_mps;
    m->out_mps = c->out_mps;
    m->note = "not initialised";

    UINT64 rin = kx_dev_page(d);
    UINT64 rout = kx_dev_page(d);

    m->cmd_buf = kx_dev_page(d);
    m->data_buf = kx_dev_page(d);

    if (!rin || !rout || !m->cmd_buf || !m->data_buf) {
        kx_out(out, "    out of memory\n");
        return -1;
    }

    kx_ring_init(&m->in_ring, rin);
    kx_ring_init(&m->out_ring, rout);

    KX_EPCFG *e = &eps[(*neps)++];

    e->dci = m->in_dci;
    e->type = 6;                    /* Bulk IN */
    e->maxpkt = m->in_mps;
    e->burst = c->in_burst;
    e->interval = 0;
    e->ring = rin;
    e->avg = 3072;
    e->esit = 0;

    e = &eps[(*neps)++];

    e->dci = m->out_dci;
    e->type = 2;                    /* Bulk OUT */
    e->maxpkt = m->out_mps;
    e->burst = c->out_burst;
    e->interval = 0;
    e->ring = rout;
    e->avg = 3072;
    e->esit = 0;

    m->used = TRUE;
    d->msd = (INT8)mi;

    kx_out(out, "    USB mass storage (Bulk-Only), interface %u, EP IN 0x%02x / OUT 0x%02x\n",
           c->iface, c->in_addr, c->out_addr);

    return mi;
}

/* Сброс "Bulk-Only Mass Storage Reset" + снять halt с обеих точек -
   так положено восстанавливаться после сбоя протокола */
static void kx_msd_reset(KX_MSD *m)
{
    KX_DEV *d = &g_kx_devs[m->dev];

    kx_control(d, 0x21, 0xFF, 0, m->iface, 0, 0);
    kx_recover_sync(d->slot, m->in_dci, &m->in_ring);
    kx_control(d, 0x02, 0x01, 0, m->in_addr, 0, 0);
    kx_recover_sync(d->slot, m->out_dci, &m->out_ring);
    kx_control(d, 0x02, 0x01, 0, m->out_addr, 0, 0);
}

/*
 * Одна команда SCSI через Bulk-Only. data_len байт данных читаются
 * в m->data_buf (dir_in) или пишутся оттуда. Возвращает статус
 * CSW (0 = успех), 0xFF - сбой транспорта.
 */
static UINT8 kx_msd_scsi(KX_MSD *m, const UINT8 *cdb, UINT8 cdb_len,
                         UINT32 data_len, BOOLEAN dir_in)
{
    KX_DEV *d = &g_kx_devs[m->dev];
    volatile UINT8 *cbw = (volatile UINT8 *)P2V(m->cmd_buf);
    UINT32 tag = ++m->tag;
    UINT32 got = 0;

    raw_zero_mem(cbw, 64);

    /* CBW: "USBC", tag, длина данных, флаги (0x80 = к нам), LUN 0,
       длина команды, команда */
    cbw[0] = 'U'; cbw[1] = 'S'; cbw[2] = 'B'; cbw[3] = 'C';
    cbw[4] = (UINT8)tag; cbw[5] = (UINT8)(tag >> 8);
    cbw[6] = (UINT8)(tag >> 16); cbw[7] = (UINT8)(tag >> 24);
    cbw[8] = (UINT8)data_len; cbw[9] = (UINT8)(data_len >> 8);
    cbw[10] = (UINT8)(data_len >> 16); cbw[11] = (UINT8)(data_len >> 24);
    cbw[12] = dir_in ? 0x80 : 0x00;
    cbw[13] = 0;
    cbw[14] = cdb_len;

    for (UINT8 i = 0; i < cdb_len && i < 16; i++)
        cbw[15 + i] = cdb[i];

    UINT8 cc = kx_bulk(d, m->out_dci, m->out_addr, &m->out_ring,
                       m->cmd_buf, 31, &got, 2000);

    if (cc != 1 && cc != 13) {
        kx_msd_reset(m);
        return 0xFF;
    }

    if (data_len > 0) {

        if (dir_in)
            cc = kx_bulk(d, m->in_dci, m->in_addr, &m->in_ring,
                         m->data_buf, data_len, &got, 5000);
        else
            cc = kx_bulk(d, m->out_dci, m->out_addr, &m->out_ring,
                         m->data_buf, data_len, &got, 5000);

        /* STALL на данных - не беда: статус всё равно придёт в CSW
           (kx_bulk уже снял halt) */
    }

    /* CSW - в тот же буфер, со смещения 64 */
    UINT64 csw_phys = m->cmd_buf + 64u;
    volatile UINT8 *csw = (volatile UINT8 *)P2V(csw_phys);

    raw_zero_mem(csw, 16);

    cc = kx_bulk(d, m->in_dci, m->in_addr, &m->in_ring, csw_phys, 13, &got, 2000);

    if (cc != 1 && cc != 13) {
        /* одна повторная попытка - так советует спецификация BOT */
        cc = kx_bulk(d, m->in_dci, m->in_addr, &m->in_ring, csw_phys, 13, &got, 2000);
    }

    if ((cc != 1 && cc != 13) || csw[0] != 'U' || csw[1] != 'S' ||
        csw[2] != 'B' || csw[3] != 'S') {
        kx_msd_reset(m);
        return 0xFF;
    }

    UINT32 rtag = (UINT32)csw[4] | ((UINT32)csw[5] << 8) |
                  ((UINT32)csw[6] << 16) | ((UINT32)csw[7] << 24);

    if (rtag != tag) {
        kx_msd_reset(m);
        return 0xFF;
    }

    return csw[12];
}

/* Запустить флешку: узнать, кто она и какого размера */
void kx_msd_start(SIMPLE_TEXT_OUTPUT_INTERFACE *out, UINTN mi)
{
    KX_MSD *m = &g_kx_msd[mi];
    KX_DEV *d = &g_kx_devs[m->dev];
    volatile UINT8 *b = (volatile UINT8 *)P2V(m->data_buf);
    UINT8 cdb[16];

    /* GET MAX LUN - многие флешки на него отвечают STALL, и это
       нормально; нам нужен только LUN 0 */
    kx_control(d, 0xA1, 0xFE, 0, m->iface, 1, m->cmd_buf + 128u);

    /* --- INQUIRY --- */
    raw_zero_mem((volatile UINT8 *)cdb, 16);
    cdb[0] = 0x12;
    cdb[4] = 36;

    if (kx_msd_scsi(m, cdb, 6, 36, TRUE) == 0) {
        for (UINTN i = 0; i < 8; i++) {
            UINT8 c = b[8 + i];
            m->vendor[i] = (c >= 32 && c < 127) ? (char)c : ' ';
        }
        for (UINTN i = 0; i < 16; i++) {
            UINT8 c = b[16 + i];
            m->product[i] = (c >= 32 && c < 127) ? (char)c : ' ';
        }
        m->vendor[8] = '\0';
        m->product[16] = '\0';

        for (INTN i = 7; i >= 0 && m->vendor[i] == ' '; i--) m->vendor[i] = '\0';
        for (INTN i = 15; i >= 0 && m->product[i] == ' '; i--) m->product[i] = '\0';
    } else {
        m->note = "INQUIRY failed";
    }

    /* --- TEST UNIT READY, пока не проснётся (до ~3 с) --- */
    BOOLEAN ready = FALSE;

    for (UINTN attempt = 0; attempt < 30; attempt++) {

        raw_zero_mem((volatile UINT8 *)cdb, 16);

        if (kx_msd_scsi(m, cdb, 6, 0, TRUE) == 0) {
            ready = TRUE;
            break;
        }

        /* REQUEST SENSE - прочитать (и тем самым сбросить) причину */
        raw_zero_mem((volatile UINT8 *)cdb, 16);
        cdb[0] = 0x03;
        cdb[4] = 18;
        kx_msd_scsi(m, cdb, 6, 18, TRUE);

        kx_msleep(100);
    }

    if (!ready) {
        m->note = "not ready (no medium?)";
        kx_out(out, "    drive \"%s %s\": not ready\n", m->vendor, m->product);
        return;
    }

    /* --- READ CAPACITY(10): последний сектор и размер сектора
       (big-endian, как всё в SCSI) --- */
    raw_zero_mem((volatile UINT8 *)cdb, 16);
    cdb[0] = 0x25;

    if (kx_msd_scsi(m, cdb, 10, 8, TRUE) != 0) {
        m->note = "READ CAPACITY failed";
        kx_out(out, "    drive \"%s %s\": READ CAPACITY failed\n", m->vendor, m->product);
        return;
    }

    UINT32 last = ((UINT32)b[0] << 24) | ((UINT32)b[1] << 16) | ((UINT32)b[2] << 8) | b[3];
    UINT32 bs = ((UINT32)b[4] << 24) | ((UINT32)b[5] << 16) | ((UINT32)b[6] << 8) | b[7];

    if (bs == 0 || bs > 4096u || (bs & (bs - 1u)) != 0) {
        m->note = "strange sector size";
        return;
    }

    m->blocks = (UINT64)last + 1u;
    m->block_size = bs;
    m->ready = TRUE;
    m->note = "ready";

    UINT64 mib = (m->blocks * m->block_size) >> 20;

    kx_out(out, "    drive \"%s %s\": %llu MiB (%llu sectors of %u bytes)\n",
           m->vendor, m->product, mib, m->blocks, m->block_size);
}

/*
 * Прочитать или записать count секторов, начиная с lba. mi - номер
 * записи в g_kx_msd, serial - "паспорт" подключения: если флешку
 * вынули и на её место встала другая, serial не совпадёт и мы ничего
 * не испортим. buf - обычная память ядра; данные идут через буфер
 * флешки (4 КиБ за раз).
 *
 * Под мьютексом контроллера: пока идёт обмен, поток usb не начнёт
 * настраивать новое устройство (ответы пришли бы в один "ящик").
 */
BOOLEAN usb_msd_rw(UINTN mi, UINT32 serial, UINT64 lba, UINT32 count, VOID *buf, BOOLEAN write)
{
    if (mi >= KX_MAX_MSD)
        return FALSE;

    kmutex_lock(&g_usb_mutex);

    KX_MSD *m = &g_kx_msd[mi];

    if (!m->used || !m->ready || m->serial != serial ||
        lba + count > m->blocks || lba + count > 0xFFFFFFFFull) {
        kmutex_unlock(&g_usb_mutex);
        return FALSE;
    }

    UINT8 *p = (UINT8 *)buf;
    UINT32 per = 4096u / m->block_size;     /* секторов за одну передачу */
    BOOLEAN ok = TRUE;

    while (count > 0) {

        UINT32 n = (count < per) ? count : per;
        UINT32 bytes = n * m->block_size;
        UINT8 cdb[16];
        volatile UINT8 *dma = (volatile UINT8 *)P2V(m->data_buf);

        raw_zero_mem((volatile UINT8 *)cdb, 16);
        cdb[0] = write ? 0x2A : 0x28;
        cdb[2] = (UINT8)(lba >> 24); cdb[3] = (UINT8)(lba >> 16);
        cdb[4] = (UINT8)(lba >> 8);  cdb[5] = (UINT8)lba;
        cdb[7] = (UINT8)(n >> 8);    cdb[8] = (UINT8)n;

        if (write)
            for (UINT32 i = 0; i < bytes; i++)
                dma[i] = p[i];

        UINT8 st = kx_msd_scsi(m, cdb, 10, bytes, !write);

        if (st != 0) {
            /* одна повторная попытка (флешки иногда отвечают "занята") */
            if (st != 0xFF) {
                UINT8 sense[16];
                raw_zero_mem((volatile UINT8 *)sense, 16);
                sense[0] = 0x03;
                sense[4] = 18;
                kx_msd_scsi(m, sense, 6, 18, TRUE);
            }

            if (write)
                for (UINT32 i = 0; i < bytes; i++)
                    dma[i] = p[i];

            st = kx_msd_scsi(m, cdb, 10, bytes, !write);
        }

        if (st != 0) {
            m->errors++;
            ok = FALSE;
            break;
        }

        if (!write)
            for (UINT32 i = 0; i < bytes; i++)
                p[i] = dma[i];

        p += bytes;
        lba += n;
        count -= n;

        if (write)
            m->writes++;
        else
            m->reads++;
    }

    kmutex_unlock(&g_usb_mutex);

    return ok;
}

/* Флешка с этим "паспортом" ещё на месте? */
BOOLEAN usb_msd_alive(UINTN mi, UINT32 serial)
{
    if (mi >= KX_MAX_MSD)
        return FALSE;

    kmutex_lock(&g_usb_mutex);
    BOOLEAN alive = g_kx_msd[mi].used && g_kx_msd[mi].ready &&
                    g_kx_msd[mi].serial == serial;
    kmutex_unlock(&g_usb_mutex);

    return alive;
}

/* Сведения о готовой флешке mi (для регистрации диска) */
BOOLEAN usb_msd_info(UINTN mi, UINT32 *serial, UINT64 *blocks, UINT32 *bsize,
                     char *model, UINTN cap)
{
    if (mi >= KX_MAX_MSD)
        return FALSE;

    kmutex_lock(&g_usb_mutex);

    KX_MSD *m = &g_kx_msd[mi];
    BOOLEAN ok = m->used && m->ready;

    if (ok) {
        *serial = m->serial;
        *blocks = m->blocks;
        *bsize = m->block_size;
        ksnprintf(model, cap, "%s %s", m->vendor, m->product);
    }

    kmutex_unlock(&g_usb_mutex);

    return ok;
}
