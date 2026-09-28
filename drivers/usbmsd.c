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
 *
 * Это первый "диск" MyOS: этап 5 (файловая система) будет читать
 * флешку через usb_disk_read. Запись - там же, на этапе 5.
 */
#include "myos.h"

KX_MSD g_kx_msd[KX_MAX_MSD];

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

/* Номер флешки -> запись (n-я по счёту среди подключённых) */
static KX_MSD *kx_msd_nth(UINTN n)
{
    for (UINTN i = 0; i < KX_MAX_MSD; i++)
        if (g_kx_msd[i].used) {
            if (n == 0)
                return &g_kx_msd[i];
            n--;
        }

    return NULL;
}

/*
 * Прочитать count секторов, начиная с lba, в dst (обычная память
 * ядра). Для этапа 5: файловая система будет читать через это.
 */
BOOLEAN usb_disk_read(UINTN disk, UINT64 lba, UINT32 count, VOID *dst)
{
    KX_MSD *m = kx_msd_nth(disk);

    if (m == NULL || !m->ready || lba + count > m->blocks || lba > 0xFFFFFFFFull)
        return FALSE;

    UINT8 *out = (UINT8 *)dst;
    UINT32 per = 4096u / m->block_size;     /* секторов за одну передачу */

    while (count > 0) {

        UINT32 n = (count < per) ? count : per;
        UINT8 cdb[16];

        raw_zero_mem((volatile UINT8 *)cdb, 16);
        cdb[0] = 0x28;
        cdb[2] = (UINT8)(lba >> 24); cdb[3] = (UINT8)(lba >> 16);
        cdb[4] = (UINT8)(lba >> 8);  cdb[5] = (UINT8)lba;
        cdb[7] = (UINT8)(n >> 8);    cdb[8] = (UINT8)n;

        if (kx_msd_scsi(m, cdb, 10, n * m->block_size, TRUE) != 0) {
            m->errors++;
            return FALSE;
        }

        volatile UINT8 *src = (volatile UINT8 *)P2V(m->data_buf);

        for (UINT32 i = 0; i < n * m->block_size; i++)
            out[i] = src[i];

        out += n * m->block_size;
        lba += n;
        count -= n;
        m->reads++;
    }

    return TRUE;
}

/* ================================================================
 * Команда disk
 * ================================================================ */

static void disk_hexdump(SIMPLE_TEXT_OUTPUT_INTERFACE *out, const UINT8 *p, UINTN n)
{
    for (UINTN row = 0; row < n; row += 16) {

        char line[96];
        UINTN k = ksnprintf(line, sizeof(line), "  %03x: ", (UINT32)row);

        for (UINTN i = 0; i < 16; i++)
            k += ksnprintf(line + k, sizeof(line) - k, "%02x ", p[row + i]);

        k += ksnprintf(line + k, sizeof(line) - k, " ");

        for (UINTN i = 0; i < 16 && k + 2 < sizeof(line); i++) {
            UINT8 c = p[row + i];
            line[k++] = (c >= 32 && c < 127) ? (char)c : '.';
        }

        line[k] = '\0';
        kprintf(out, "%s\n", line);
    }
}

void kernel_cmd_disk(SIMPLE_TEXT_OUTPUT_INTERFACE *out, const char *arg)
{
    kernel_poll_input();       /* подобрать свежие подключения */

    if (arg[0] == '\0') {

        UINTN n = 0;

        for (UINTN i = 0; i < KX_MAX_MSD; i++) {

            KX_MSD *m = &g_kx_msd[i];

            if (!m->used)
                continue;

            char path[32];
            kx_dev_path(&g_kx_devs[m->dev], path, sizeof(path));

            if (m->ready) {
                UINT64 mib = (m->blocks * m->block_size) >> 20;
                kprintf(out, "disk %u: \"%s %s\" on USB port %s, %llu MiB "
                             "(%llu sectors x %u bytes)\n",
                        (UINT32)n, m->vendor, m->product, path, mib,
                        m->blocks, m->block_size);
            } else {
                kprintf(out, "disk %u: \"%s %s\" on USB port %s - %s\n",
                        (UINT32)n, m->vendor, m->product, path, m->note);
            }

            n++;
        }

        if (n == 0)
            print(out, "No USB drives. Plug in a flash drive - it is picked up automatically.\n");
        else
            print(out, "Read a sector: disk read <sector> [disk number]  (read-only for now)\n");

        return;
    }

    if (arg[0] == 'r' && arg[1] == 'e' && arg[2] == 'a' && arg[3] == 'd') {

        const char *p = arg + 4;
        UINT64 lba = 0;
        UINTN disk = 0;

        while (*p == ' ') p++;
        while (*p >= '0' && *p <= '9') lba = lba * 10u + (UINT64)(*p++ - '0');
        while (*p == ' ') p++;
        while (*p >= '0' && *p <= '9') disk = disk * 10u + (UINTN)(*p++ - '0');

        KX_MSD *m = kx_msd_nth(disk);

        if (m == NULL || !m->ready) {
            print(out, "No such ready disk (see 'disk').\n");
            return;
        }

        UINT8 *buf = (UINT8 *)kmalloc(m->block_size);

        if (buf == NULL) {
            print(out, "Out of memory.\n");
            return;
        }

        if (!usb_disk_read(disk, lba, 1, buf)) {
            kprintf(out, "Read of sector %llu failed.\n", lba);
            kfree(buf);
            return;
        }

        kprintf(out, "disk %u, sector %llu (first 256 of %u bytes):\n",
                (UINT32)disk, lba, m->block_size);
        disk_hexdump(out, buf, 256);

        /* что это за сектор - если узнаём */
        if (m->block_size >= 512 && buf[510] == 0x55 && buf[511] == 0xAA) {
            if (lba == 0) {
                print(out, "Boot signature 55 AA: this is an MBR. Partitions:\n");
                for (UINTN i = 0; i < 4; i++) {
                    const UINT8 *e = buf + 446 + i * 16;
                    UINT32 start = e[8] | (e[9] << 8) | (e[10] << 16) | ((UINT32)e[11] << 24);
                    UINT32 size = e[12] | (e[13] << 8) | (e[14] << 16) | ((UINT32)e[15] << 24);
                    if (e[4] == 0)
                        continue;
                    kprintf(out, "  #%u type 0x%02x%s, start %u, %u MiB\n",
                            (UINT32)i + 1, e[4],
                            e[4] == 0xEE ? " (GPT protective)" :
                            (e[4] == 0x0B || e[4] == 0x0C) ? " (FAT32)" :
                            e[4] == 0x07 ? " (NTFS/exFAT)" :
                            e[4] == 0x83 ? " (Linux)" : "",
                            start, (UINT32)(((UINT64)size * m->block_size) >> 20));
                }
            } else {
                print(out, "Boot signature 55 AA at the end.\n");
            }
        }

        if (buf[0] == 'E' && buf[1] == 'F' && buf[2] == 'I' && buf[3] == ' ' &&
            buf[4] == 'P' && buf[5] == 'A' && buf[6] == 'R' && buf[7] == 'T')
            print(out, "\"EFI PART\": this is a GPT header.\n");

        kfree(buf);
        return;
    }

    print(out, "Usage: disk             - list USB drives\n");
    print(out, "       disk read N [D]  - show sector N of drive D (default 0)\n");
}
