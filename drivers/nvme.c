/*
 * drivers/nvme.c - NVMe-SSD (этап 5). Часть MyOS; общие объявления -
 * в myos.h.
 *
 * NVMe (PCI класс 01, подкласс 08, prog-if 02) - язык современных
 * SSD на шине PCIe. Устроен через ОЧЕРЕДИ в памяти:
 *   * Submission Queue (SQ) - кольцо команд по 64 байта: мы пишем
 *     команду в следующую ячейку и сообщаем контроллеру новый
 *     "хвост" через регистр-звонок (doorbell);
 *   * Completion Queue (CQ) - кольцо ответов по 16 байт: контроллер
 *     кладёт туда результат. Как понять, что ответ новый? В каждом
 *     ответе есть "фазовый" бит: на первом круге кольца контроллер
 *     пишет 1, на втором 0, и так далее - сравниваем с ожидаемым.
 * Очереди две пары: административная (номер 0 - настройка,
 * "кто ты") и одна пара для чтения/записи (номер 1).
 *
 * Команды:
 *   Identify (admin 0x06)          - модель, размер, размер сектора;
 *   Create I/O CQ / SQ (0x05/0x01) - завести очередь для данных;
 *   Read (0x02) / Write (0x01)     - секторы.
 * Где данные - говорят адреса PRP: PRP1 - первая страница, PRP2 -
 * вторая (больше двух страниц за раз мы не передаём, 8 КиБ).
 *
 * Как и AHCI: ждём опросом; ЗАПИСЬ разрешена только на диск QEMU.
 */
#include "myos.h"

#define NVME_MAX   2
#define NVME_QSIZE 16

typedef struct {
    BOOLEAN used;
    UINT64  bar;
    UINT32  dstrd;               /* шаг регистров-звонков */
    UINT64  asq, acq;            /* административные очереди */
    UINT64  iosq, iocq;          /* очереди данных */
    UINT16  asq_tail, acq_head, iosq_tail, iocq_head;
    UINT8   acq_phase, iocq_phase;
    UINT16  cid;
    UINT64  dma;                 /* буфер данных (dma_pages страниц подряд) */
    UINT32  dma_pages;
    UINT64  prp;                 /* список PRP: адреса страниц буфера со 2-й */
    UINT32  max_bytes;           /* сколько данных за одну команду */
    UINT64  ident;               /* страница для Identify */
    UINT64  sectors;
    UINT32  lba_size;
    char    model[41];
} NVME_CTRL;

static NVME_CTRL g_nvme[NVME_MAX];

#define NV_CAP   0x00
#define NV_CC    0x14
#define NV_CSTS  0x1C
#define NV_AQA   0x24
#define NV_ASQ   0x28
#define NV_ACQ   0x30

static UINT64 nv_db(NVME_CTRL *c, UINT32 qid, BOOLEAN cq)
{
    return c->bar + 0x1000u + (UINT64)(2u * qid + (cq ? 1u : 0u)) * (4u << c->dstrd);
}

/*
 * Отдать команду в очередь (admin, если io == FALSE) и дождаться
 * ответа. cmd - 16 двойных слов. Возвращает статус (0 - успех),
 * 0xFFFF - нет ответа.
 */
static UINT16 nvme_submit(NVME_CTRL *c, BOOLEAN io, UINT32 cmd[16])
{
    UINT64 sq = io ? c->iosq : c->asq;
    UINT64 cq = io ? c->iocq : c->acq;
    UINT16 *tail = io ? &c->iosq_tail : &c->asq_tail;
    UINT16 *head = io ? &c->iocq_head : &c->acq_head;
    UINT8 *phase = io ? &c->iocq_phase : &c->acq_phase;
    UINT32 qid = io ? 1u : 0u;

    UINT16 cid = ++c->cid;

    cmd[0] = (cmd[0] & 0xFFFFu) | ((UINT32)cid << 16);

    volatile UINT32 *slot = (volatile UINT32 *)P2V(sq + (UINT64)(*tail) * 64u);

    for (UINTN i = 0; i < 16; i++)
        slot[i] = cmd[i];

    *tail = (UINT16)((*tail + 1u) % NVME_QSIZE);

    __asm__ __volatile__("mfence" ::: "memory");

    mmio_write32(nv_db(c, qid, FALSE), *tail);

    /* ответ: 4-е двойное слово - статус (биты 17..31) и фаза (бит 16) */
    volatile UINT32 *ent = (volatile UINT32 *)P2V(cq + (UINT64)(*head) * 16u);

    if (!blk_wait(&ent[3], 0x10000u, (UINT32)(*phase) << 16, 5000)) {
        klog("nvme: no answer to command 0x%02x\n", cmd[0] & 0xFF);
        return 0xFFFF;
    }

    UINT16 status = (UINT16)(ent[3] >> 17);

    *head = (UINT16)((*head + 1u) % NVME_QSIZE);

    if (*head == 0)
        *phase ^= 1u;

    mmio_write32(nv_db(c, qid, TRUE), *head);

    if (status != 0)
        klog("nvme: command 0x%02x -> status 0x%04x\n", cmd[0] & 0xFF, status);

    return status;
}

static BOOLEAN nvme_rw(BLKDEV *d, UINT64 lba, UINT32 count, VOID *buf, BOOLEAN write)
{
    NVME_CTRL *c = &g_nvme[d->drv_index];
    UINT8 *b = (UINT8 *)buf;
    /* за команду - до max_bytes (128 КиБ): большие файлы (библиотеки
       Linux, Firefox) читаются в 16 раз меньшим числом команд */
    UINT32 per = c->max_bytes / c->lba_size;
    volatile UINT8 *dma = (volatile UINT8 *)P2V(c->dma);

    while (count > 0) {

        UINT32 n = (count < per) ? count : per;
        UINT32 bytes = n * c->lba_size;
        UINT32 cmd[16];

        for (UINTN i = 0; i < 16; i++)
            cmd[i] = 0;

        if (write)
            memcpy((void *)dma, b, bytes);

        cmd[0] = write ? 0x01u : 0x02u;
        cmd[1] = 1;                                   /* namespace 1 */
        cmd[6] = (UINT32)(c->dma & 0xFFFFFFFFu);      /* PRP1 */
        cmd[7] = (UINT32)(c->dma >> 32);
        if (bytes > 8192u) {
            /* больше двух страниц: PRP2 - список адресов остальных */
            cmd[8] = (UINT32)(c->prp & 0xFFFFFFFFu);
            cmd[9] = (UINT32)(c->prp >> 32);
        } else if (bytes > 4096u) {
            cmd[8] = (UINT32)((c->dma + 4096u) & 0xFFFFFFFFu);   /* PRP2 */
            cmd[9] = (UINT32)((c->dma + 4096u) >> 32);
        }
        cmd[10] = (UINT32)(lba & 0xFFFFFFFFu);
        cmd[11] = (UINT32)(lba >> 32);
        cmd[12] = n - 1u;                             /* "минус 1" */

        if (nvme_submit(c, TRUE, cmd) != 0)
            return FALSE;

        if (!write)
            memcpy(b, (const void *)dma, bytes);

        b += bytes;
        lba += n;
        count -= n;
    }

    return TRUE;
}

static BOOLEAN nvme_start(NVME_CTRL *c, SIMPLE_TEXT_OUTPUT_INTERFACE *out)
{
    UINT64 cap = mmio_read64(c->bar + NV_CAP);
    UINT64 to_ms = ((cap >> 24) & 0xFFu) * 500u + 500u;

    c->dstrd = (UINT32)((cap >> 32) & 0xFu);

    if ((cap >> 48) & 0xFu) {
        print(out, "    NVMe: minimum page size is not 4 KiB - not supported\n");
        return FALSE;
    }

    /* 1. выключить (CC.EN = 0) и дождаться CSTS.RDY = 0 */
    mmio_write32(c->bar + NV_CC, mmio_read32(c->bar + NV_CC) & ~1u);

    if (!blk_wait((volatile UINT32 *)P2V(c->bar + NV_CSTS), 1u, 0, to_ms)) {
        print(out, "    NVMe: controller does not stop\n");
        return FALSE;
    }

    /* 2. административные очереди */
    c->asq = pmm_alloc_zeroed(1, 0x100000000ull);
    c->acq = pmm_alloc_zeroed(1, 0x100000000ull);
    c->iosq = pmm_alloc_zeroed(1, 0x100000000ull);
    c->iocq = pmm_alloc_zeroed(1, 0x100000000ull);
    /* буфер данных: 128 КиБ подряд (не нашлось - хотя бы 8 КиБ) */
    c->dma_pages = 32;
    c->dma = pmm_alloc_zeroed(c->dma_pages, 0x100000000ull);
    if (c->dma == 0) {
        c->dma_pages = 2;
        c->dma = pmm_alloc_zeroed(c->dma_pages, 0x100000000ull);
    }
    c->prp = pmm_alloc_zeroed(1, 0x100000000ull);
    c->ident = pmm_alloc_zeroed(1, 0x100000000ull);

    if (!c->asq || !c->acq || !c->iosq || !c->iocq || !c->dma || !c->ident || !c->prp)
        return FALSE;

    {
        volatile UINT64 *l = (volatile UINT64 *)P2V(c->prp);
        for (UINT32 k = 1; k < c->dma_pages; k++)
            l[k - 1] = c->dma + 4096ull * k;
    }

    c->max_bytes = c->dma_pages * 4096u;

    c->acq_phase = 1;
    c->iocq_phase = 1;

    mmio_write32(c->bar + NV_AQA, ((NVME_QSIZE - 1u) << 16) | (NVME_QSIZE - 1u));
    mmio_write64(c->bar + NV_ASQ, c->asq);
    mmio_write64(c->bar + NV_ACQ, c->acq);

    /* 3. включить: EN, команды NVM, страница 4 КиБ, размеры
       записей очередей: SQ 2^6 = 64, CQ 2^4 = 16 байт */
    mmio_write32(c->bar + NV_CC, 1u | (6u << 16) | (4u << 20));

    if (!blk_wait((volatile UINT32 *)P2V(c->bar + NV_CSTS), 1u, 1u, to_ms)) {
        print(out, "    NVMe: controller does not start\n");
        return FALSE;
    }

    UINT32 cmd[16];

    /* 4. Identify Controller: модель */
    for (UINTN i = 0; i < 16; i++) cmd[i] = 0;
    cmd[0] = 0x06;
    cmd[6] = (UINT32)(c->ident & 0xFFFFFFFFu);
    cmd[7] = (UINT32)(c->ident >> 32);
    cmd[10] = 1;

    if (nvme_submit(c, FALSE, cmd) != 0)
        return FALSE;

    volatile UINT8 *id = (volatile UINT8 *)P2V(c->ident);

    for (UINTN i = 0; i < 40; i++) {
        UINT8 ch = id[24 + i];
        c->model[i] = (ch >= 32 && ch < 127) ? (char)ch : ' ';
    }

    c->model[40] = '\0';

    /* MDTS: предел одной передачи у контроллера (2^n страниц; 0 - нет) */
    if (id[77] != 0 && id[77] < 16 && (4096u << id[77]) < c->max_bytes)
        c->max_bytes = 4096u << id[77];

    for (INTN i = 39; i >= 0 && c->model[i] == ' '; i--)
        c->model[i] = '\0';

    /* 5. Identify Namespace 1: размер и размер сектора */
    for (UINTN i = 0; i < 16; i++) cmd[i] = 0;
    cmd[0] = 0x06;
    cmd[1] = 1;
    cmd[6] = (UINT32)(c->ident & 0xFFFFFFFFu);
    cmd[7] = (UINT32)(c->ident >> 32);
    cmd[10] = 0;

    if (nvme_submit(c, FALSE, cmd) != 0)
        return FALSE;

    UINT64 nsze = 0;

    for (UINTN k = 0; k < 8; k++)
        nsze |= (UINT64)id[k] << (8 * k);

    UINT8 flbas = id[26] & 0x0Fu;
    UINT8 lbads = id[128 + 4u * flbas + 2];

    c->sectors = nsze;
    c->lba_size = (lbads >= 9 && lbads <= 12) ? (1u << lbads) : 0;

    if (c->lba_size == 0 || nsze == 0) {
        print(out, "    NVMe: namespace 1 is empty or has a strange sector size\n");
        return FALSE;
    }

    /* 6. очереди данных: сначала CQ 1, потом SQ 1 (привязана к CQ 1) */
    for (UINTN i = 0; i < 16; i++) cmd[i] = 0;
    cmd[0] = 0x05;
    cmd[6] = (UINT32)(c->iocq & 0xFFFFFFFFu);
    cmd[7] = (UINT32)(c->iocq >> 32);
    cmd[10] = ((NVME_QSIZE - 1u) << 16) | 1u;
    cmd[11] = 1u;                         /* физически непрерывная */

    if (nvme_submit(c, FALSE, cmd) != 0)
        return FALSE;

    for (UINTN i = 0; i < 16; i++) cmd[i] = 0;
    cmd[0] = 0x01;
    cmd[6] = (UINT32)(c->iosq & 0xFFFFFFFFu);
    cmd[7] = (UINT32)(c->iosq >> 32);
    cmd[10] = ((NVME_QSIZE - 1u) << 16) | 1u;
    cmd[11] = (1u << 16) | 1u;            /* CQ 1, непрерывная */

    if (nvme_submit(c, FALSE, cmd) != 0)
        return FALSE;

    return TRUE;
}

void nvme_init(SIMPLE_TEXT_OUTPUT_INTERFACE *out)
{
    UINTN used = 0;

    for (UINTN n = 0; n < 4 && used < NVME_MAX; n++) {

        UINT8 bus, dev, fn;

        if (!pci_find_class(0x01, 0x08, 0x02, n, &bus, &dev, &fn))
            break;

        UINT64 bar = pci_read_bar_address(bus, dev, fn, 0x10);

        if (bar == 0)
            continue;

        NVME_CTRL *c = &g_nvme[used];

        raw_zero_mem((volatile UINT8 *)c, sizeof(*c));
        c->bar = bar;

        pci_enable_device(bus, dev, fn);
        vmm_map_mmio(bar, 0x4000u, VMM_UC);

        kprintf(out, "  NVMe at %u:%u.%u\n", bus, dev, fn);

        if (!nvme_start(c, out)) {
            print(out, "    NVMe: start failed (see COM1 log)\n");
            continue;
        }

        c->used = TRUE;

        BOOLEAN rw = c->model[0] == 'Q' && c->model[1] == 'E' &&
                     c->model[2] == 'M' && c->model[3] == 'U';

        INTN bi = blk_register_disk("nvme", "NVMe", c->model, c->sectors, c->lba_size,
                                    rw, rw ? NULL :
                                    "internal disk: MyOS only reads it - Arch lives there",
                                    nvme_rw, used, 0);

        kprintf(out, "    \"%s\", %llu MiB, sector %u bytes -> %s%s\n", c->model,
                (c->sectors * c->lba_size) >> 20, c->lba_size,
                bi >= 0 ? g_blk[bi].name : "?", rw ? "" : " (read-only)");

        used++;
    }
}
