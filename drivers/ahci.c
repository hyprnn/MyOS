/*
 * drivers/ahci.c - SATA-диски через контроллер AHCI (этап 5).
 * Часть MyOS; общие объявления - в myos.h.
 *
 * AHCI - стандартный "язык" SATA-контроллеров Intel/AMD (PCI класс
 * 01, подкласс 06, prog-if 01). Контроллер - это окно регистров
 * (ABAR, BAR5) и до 32 портов; к каждому порту подключается диск.
 *
 * Как отдаётся команда диску (у каждого порта - своя память):
 *   * Command List - 32 "заголовка команд" по 32 байта; мы
 *     пользуемся только слотом 0;
 *   * Command Table - сама команда в виде FIS (Frame Information
 *     Structure - "пакет", который SATA передаёт по проводу; нам
 *     нужен "Register Host to Device", 20 байт: код команды ATA,
 *     номер сектора, сколько секторов) + таблица PRDT: куда в памяти
 *     класть данные (адрес и длина кусков);
 *   * Received FIS - сюда контроллер кладёт ответы диска.
 * Записали всё это, поставили бит 0 в регистре PxCI ("слот 0 -
 * выполнять") - и ждём, пока контроллер сбросит его обратно.
 *
 * Команды ATA:
 *   IDENTIFY DEVICE (0xEC)      - 512 байт о диске: модель, размер;
 *   READ DMA EXT (0x25)         - прочитать секторы (48-битный номер);
 *   WRITE DMA EXT (0x35)        - записать.
 *
 * Прерывания не используем - ждём опросом (blk_wait спит по
 * миллисекунде, процессор не простаивает).
 *
 * ЗАПИСЬ на SATA-диск разрешена только в QEMU (модель "QEMU ..."):
 * на настоящем ноутбуке там Arch, а ошибка в драйвере записи может
 * испортить файловую систему. Читать - можно (например, раздел EFI).
 */
#include "myos.h"

#define AHCI_MAX_PORTS   8
#define AHCI_DMA_PAGES   16          /* 64 КиБ за одну команду */

typedef struct {
    BOOLEAN used;
    UINT64  abar;                    /* физический адрес регистров */
    UINT32  port;                    /* номер порта */
    UINT64  mem;                     /* страница: Command List, FIS, Table */
    UINT64  dma;                     /* 64 КиБ буфер для данных */
    UINT64  sectors;
    char    model[41];
} AHCI_PORT;

static AHCI_PORT g_ahci[AHCI_MAX_PORTS];

/* Регистры порта: 0x100 + 0x80 * номер */
#define PX(p, off)  ((p)->abar + 0x100u + 0x80u * (p)->port + (off))
#define PxCLB   0x00
#define PxCLBU  0x04
#define PxFB    0x08
#define PxFBU   0x0C
#define PxIS    0x10
#define PxCMD   0x18
#define PxTFD   0x20
#define PxSIG   0x24
#define PxSSTS  0x28
#define PxSERR  0x30
#define PxCI    0x38

static volatile UINT32 *ahci_reg(UINT64 phys)
{
    return (volatile UINT32 *)P2V(phys);
}

/* Остановить "движок" порта (перед настройкой) */
static BOOLEAN ahci_stop(AHCI_PORT *p)
{
    UINT32 cmd = mmio_read32(PX(p, PxCMD));

    cmd &= ~(1u << 0);                        /* ST */
    mmio_write32(PX(p, PxCMD), cmd);

    if (!blk_wait(ahci_reg(PX(p, PxCMD)), 1u << 15, 0, 500))   /* CR */
        return FALSE;

    cmd = mmio_read32(PX(p, PxCMD));
    cmd &= ~(1u << 4);                        /* FRE */
    mmio_write32(PX(p, PxCMD), cmd);

    return blk_wait(ahci_reg(PX(p, PxCMD)), 1u << 14, 0, 500);  /* FR */
}

/*
 * Одна команда ATA через слот 0. dir_write - данные идут к диску.
 * bytes - сколько (не больше 64 КиБ), данные - в p->dma.
 */
static BOOLEAN ahci_cmd(AHCI_PORT *p, UINT8 command, UINT64 lba, UINT32 count,
                        UINT32 bytes, BOOLEAN dir_write)
{
    volatile UINT8 *mem = (volatile UINT8 *)P2V(p->mem);
    volatile UINT32 *hdr = (volatile UINT32 *)mem;              /* слот 0 */
    volatile UINT8 *tbl = mem + 0x800;                         /* Command Table */
    volatile UINT32 *prdt = (volatile UINT32 *)(tbl + 0x80);

    /* диск не занят? (TFD: BSY бит 7, DRQ бит 3) */
    if (!blk_wait(ahci_reg(PX(p, PxTFD)), 0x88u, 0, 1000))
        return FALSE;

    raw_zero_mem(tbl, 0x100);

    /* FIS Register H2D */
    tbl[0] = 0x27;                 /* тип FIS */
    tbl[1] = 0x80;                 /* C = это команда */
    tbl[2] = command;
    tbl[4] = (UINT8)lba;
    tbl[5] = (UINT8)(lba >> 8);
    tbl[6] = (UINT8)(lba >> 16);
    tbl[7] = 0x40;                 /* режим LBA */
    tbl[8] = (UINT8)(lba >> 24);
    tbl[9] = (UINT8)(lba >> 32);
    tbl[10] = (UINT8)(lba >> 40);
    tbl[12] = (UINT8)count;
    tbl[13] = (UINT8)(count >> 8);

    UINT32 prdtl = 0;

    if (bytes > 0) {
        /* один кусок: буфер 64 КиБ лежит подряд в физической памяти;
           длина записывается "минус 1" */
        prdt[0] = (UINT32)(p->dma & 0xFFFFFFFFu);
        prdt[1] = (UINT32)(p->dma >> 32);
        prdt[2] = 0;
        prdt[3] = (bytes - 1u) & 0x3FFFFFu;
        prdtl = 1;
    }

    /* заголовок: длина FIS в двойных словах (5), W - запись, число
       кусков PRDT; адрес таблицы */
    hdr[0] = 5u | (dir_write ? (1u << 6) : 0u) | (prdtl << 16);
    hdr[1] = 0;
    hdr[2] = (UINT32)((p->mem + 0x800u) & 0xFFFFFFFFu);
    hdr[3] = (UINT32)((p->mem + 0x800u) >> 32);

    mmio_write32(PX(p, PxIS), 0xFFFFFFFFu);    /* сбросить старые флаги */

    __asm__ __volatile__("mfence" ::: "memory");

    mmio_write32(PX(p, PxCI), 1u);

    /* ждём, пока контроллер снимет бит слота 0 */
    BOOLEAN done = blk_wait(ahci_reg(PX(p, PxCI)), 1u, 0, 5000);
    UINT32 is = mmio_read32(PX(p, PxIS));
    UINT32 tfd = mmio_read32(PX(p, PxTFD));

    if (!done || (is & (1u << 30)) || (tfd & 0x01u)) {   /* TFES / ERR */
        klog("ahci: port %u command 0x%02x failed (IS=0x%08x TFD=0x%08x%s)\n",
             p->port, command, is, tfd, done ? "" : ", timeout");
        /* перезапуск порта - чтобы следующая команда могла пройти */
        ahci_stop(p);
        mmio_write32(PX(p, PxSERR), 0xFFFFFFFFu);
        mmio_write32(PX(p, PxIS), 0xFFFFFFFFu);
        mmio_write32(PX(p, PxCMD), mmio_read32(PX(p, PxCMD)) | (1u << 4));
        mmio_write32(PX(p, PxCMD), mmio_read32(PX(p, PxCMD)) | 1u);
        return FALSE;
    }

    return TRUE;
}

static BOOLEAN ahci_rw(BLKDEV *d, UINT64 lba, UINT32 count, VOID *buf, BOOLEAN write)
{
    AHCI_PORT *p = &g_ahci[d->drv_index];
    UINT8 *b = (UINT8 *)buf;
    UINT32 per = (AHCI_DMA_PAGES * 4096u) / 512u;
    volatile UINT8 *dma = (volatile UINT8 *)P2V(p->dma);

    while (count > 0) {

        UINT32 n = (count < per) ? count : per;
        UINT32 bytes = n * 512u;

        if (write)
            memcpy((void *)dma, b, bytes);

        if (!ahci_cmd(p, write ? 0x35 : 0x25, lba, n, bytes, write))
            return FALSE;

        if (!write)
            memcpy(b, (const void *)dma, bytes);

        b += bytes;
        lba += n;
        count -= n;
    }

    return TRUE;
}

/* Настроить порт с диском и спросить IDENTIFY */
static BOOLEAN ahci_port_init(AHCI_PORT *p)
{
    if (!ahci_stop(p)) {
        klog("ahci: port %u does not stop\n", p->port);
        return FALSE;
    }

    p->mem = pmm_alloc_zeroed(1, 0x100000000ull);
    p->dma = pmm_alloc_zeroed(AHCI_DMA_PAGES, 0x100000000ull);

    if (p->mem == 0 || p->dma == 0)
        return FALSE;

    /* Command List - начало страницы (1 КиБ), Received FIS - 0x400
       (256 байт), Command Table - 0x800 */
    mmio_write32(PX(p, PxCLB), (UINT32)(p->mem & 0xFFFFFFFFu));
    mmio_write32(PX(p, PxCLBU), (UINT32)(p->mem >> 32));
    mmio_write32(PX(p, PxFB), (UINT32)((p->mem + 0x400u) & 0xFFFFFFFFu));
    mmio_write32(PX(p, PxFBU), (UINT32)((p->mem + 0x400u) >> 32));

    mmio_write32(PX(p, PxSERR), 0xFFFFFFFFu);
    mmio_write32(PX(p, PxIS), 0xFFFFFFFFu);

    /* SUD (раскрутить диск) и POD (питание) - на случай, если
       прошивка их не включила; FRE, потом ST - порт принимает команды */
    mmio_write32(PX(p, PxCMD), mmio_read32(PX(p, PxCMD)) | (1u << 1) | (1u << 2));
    mmio_write32(PX(p, PxCMD), mmio_read32(PX(p, PxCMD)) | (1u << 4));
    mmio_write32(PX(p, PxCMD), mmio_read32(PX(p, PxCMD)) | 1u);

    if (!ahci_cmd(p, 0xEC, 0, 0, 512, FALSE))
        return FALSE;

    volatile UINT16 *id = (volatile UINT16 *)P2V(p->dma);

    /* размер: LBA48 (слова 100-103), если поддерживается (слово 83
       бит 10), иначе LBA28 (слова 60-61) */
    if (id[83] & (1u << 10))
        p->sectors = (UINT64)id[100] | ((UINT64)id[101] << 16) |
                     ((UINT64)id[102] << 32) | ((UINT64)id[103] << 48);
    else
        p->sectors = (UINT64)id[60] | ((UINT64)id[61] << 16);

    /* модель: слова 27-46, в каждом слове байты переставлены */
    UINTN k = 0;

    for (UINTN w = 27; w <= 46; w++) {
        p->model[k++] = (char)(id[w] >> 8);
        p->model[k++] = (char)(id[w] & 0xFF);
    }

    p->model[40] = '\0';

    for (INTN i = 39; i >= 0 && (p->model[i] == ' ' || p->model[i] == '\0'); i--)
        p->model[i] = '\0';

    return p->sectors != 0;
}

static BOOLEAN model_is_qemu(const char *m)
{
    return m[0] == 'Q' && m[1] == 'E' && m[2] == 'M' && m[3] == 'U';
}

void ahci_init(SIMPLE_TEXT_OUTPUT_INTERFACE *out)
{
    UINTN used = 0;

    for (UINTN ctl = 0; ctl < 4; ctl++) {

        UINT8 bus, dev, fn;

        if (!pci_find_class(0x01, 0x06, 0x01, ctl, &bus, &dev, &fn))
            break;

        UINT32 bar5 = pci_config_read32(bus, dev, fn, 0x24);
        UINT64 abar = bar5 & 0xFFFFFFF0u;

        if (abar == 0) {
            kprintf(out, "  AHCI at %u:%u.%u: no registers (BAR5 empty)\n", bus, dev, fn);
            continue;
        }

        pci_enable_device(bus, dev, fn);
        vmm_map_mmio(abar, 0x1100u, VMM_UC);

        /* BIOS/OS handoff (если контроллер это умеет): попросить
           прошивку отпустить контроллер */
        UINT32 cap2 = mmio_read32(abar + 0x24);

        if (cap2 & 1u) {
            mmio_write32(abar + 0x28, mmio_read32(abar + 0x28) | (1u << 1));   /* OOS */
            blk_wait(ahci_reg(abar + 0x28), 1u, 0, 100);                      /* BOS */
        }

        /* GHC.AE - работаем в режиме AHCI */
        mmio_write32(abar + 0x04, mmio_read32(abar + 0x04) | (1u << 31));

        UINT32 pi = mmio_read32(abar + 0x0C);     /* какие порты есть */
        UINT32 vs = mmio_read32(abar + 0x10);

        kprintf(out, "  AHCI %u.%u at %u:%u.%u, ports 0x%08x\n",
                vs >> 16, (vs >> 8) & 0xFF, bus, dev, fn, pi);

        for (UINT32 port = 0; port < 32 && used < AHCI_MAX_PORTS; port++) {

            if (!(pi & (1u << port)))
                continue;

            AHCI_PORT *p = &g_ahci[used];

            raw_zero_mem((volatile UINT8 *)p, sizeof(*p));
            p->abar = abar;
            p->port = port;

            UINT32 ssts = mmio_read32(PX(p, PxSSTS));
            UINT32 sig = mmio_read32(PX(p, PxSIG));

            /* DET = 3: устройство есть и связь установлена */
            if ((ssts & 0x0Fu) != 3)
                continue;

            if (sig == 0xEB140101u) {
                kprintf(out, "    port %u: CD/DVD drive (ATAPI) - skipped\n", port);
                continue;
            }

            if (sig != 0x00000101u) {
                kprintf(out, "    port %u: unknown device (signature 0x%08x)\n", port, sig);
                continue;
            }

            if (!ahci_port_init(p)) {
                kprintf(out, "    port %u: disk did not answer IDENTIFY\n", port);
                continue;
            }

            p->used = TRUE;

            BOOLEAN rw = model_is_qemu(p->model);

            INTN bi = blk_register_disk("sata", "SATA (AHCI)", p->model, p->sectors, 512,
                                        rw, rw ? NULL :
                                        "internal disk: MyOS only reads it - Arch lives there",
                                        ahci_rw, used, 0);

            kprintf(out, "    port %u: \"%s\", %llu MiB -> %s%s\n", port, p->model,
                    (p->sectors * 512u) >> 20, bi >= 0 ? g_blk[bi].name : "?",
                    rw ? "" : " (read-only)");

            used++;
        }
    }
}
