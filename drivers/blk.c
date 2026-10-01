/*
 * drivers/blk.c - блочные устройства: диски, разделы, кэш секторов
 * (этап 5). Часть MyOS; общие объявления - в myos.h.
 *
 * "Блочное устройство" - это всё, что умеет "прочитать N секторов с
 * номера X" и "записать". Флешка (USB), SATA-диск (AHCI) и NVMe-SSD
 * устроены внутри совсем по-разному, но файловой системе это
 * безразлично: она зовёт blk_read/blk_write, а какой драйвер
 * выполнит работу - решает таблица g_blk.
 *
 * РАЗДЕЛЫ. Диск обычно поделён на разделы; таблица разделов лежит в
 * начале диска: старая MBR (сектор 0, четыре записи) или GPT
 * (сектор 1 - заголовок "EFI PART", дальше - массив записей по 128
 * байт; так размечен твой ноутбук). Каждый раздел становится
 * отдельным устройством "usb0p1", "sata0p2" - по сути окно в диск со
 * сдвигом start. Бывают и флешки без таблицы - FAT прямо с сектора 0
 * ("superfloppy"); тогда том - весь диск.
 *
 * КЭШ. FAT постоянно перечитывает одни и те же секторы (таблицу
 * кластеров, папки). Кэш держит последние 512 прочитанных секторов
 * (256 КиБ). Запись - "сквозная" (write-through): сразу на диск и в
 * кэш. Так ничего не теряется, если выдернуть флешку или нажать
 * reset сразу после записи, - важнее скорости.
 *
 * БЕЗОПАСНОСТЬ. На ноутбуке на внутреннем диске - Arch. Ошибка в
 * драйвере записи может его испортить, поэтому внутренние диски
 * (SATA, NVMe) MyOS только ЧИТАЕТ. Писать можно на USB-флешки и на
 * диски QEMU (модель "QEMU ..."), где портить нечего.
 *
 * ЗАМОК. Всё, что касается дисков и файлов, делается под мьютексом
 * g_vfs_mutex (fs/vfs.c): шелл, GUI и поток usb не мешают друг
 * другу. Порядок замков всегда такой: g_vfs_mutex, потом (внутри
 * драйвера флешки) g_usb_mutex - никогда наоборот, иначе два потока
 * могли бы ждать друг друга вечно. Поэтому поток usb сам диски не
 * регистрирует: список флешек сверяется "лениво" (blk_sync_usb) при
 * следующем обращении к дискам.
 */
#include "myos.h"

BLKDEV g_blk[BLK_MAX];

static UINT32 g_blk_gen = 1;


/* ================================================================
 * Кэш секторов
 * ================================================================ */

typedef struct {
    BOOLEAN valid;
    UINTN   dev;         /* целый диск (не раздел) */
    UINT32  gen;
    UINT64  lba;
    UINT64  stamp;       /* когда трогали последний раз (для LRU) */
} BLK_CACHE_ENT;

static BLK_CACHE_ENT g_cache[BLK_CACHE_SECTORS];
static UINT8 *g_cache_data = NULL;
static UINT64 g_cache_clock = 0;

UINT64 g_blk_cache_hits = 0;
UINT64 g_blk_cache_misses = 0;

static INTN cache_find(UINTN dev, UINT32 gen, UINT64 lba)
{
    for (UINTN i = 0; i < BLK_CACHE_SECTORS; i++) {
        BLK_CACHE_ENT *e = &g_cache[i];
        if (e->valid && e->lba == lba && e->dev == dev && e->gen == gen)
            return (INTN)i;
    }

    return -1;
}

/* Место под новый сектор: пустое или самое давнее */
static UINTN cache_victim(void)
{
    UINTN best = 0;
    UINT64 oldest = ~0ull;

    for (UINTN i = 0; i < BLK_CACHE_SECTORS; i++) {
        if (!g_cache[i].valid)
            return i;
        if (g_cache[i].stamp < oldest) {
            oldest = g_cache[i].stamp;
            best = i;
        }
    }

    return best;
}

static void cache_drop_dev(UINTN dev)
{
    for (UINTN i = 0; i < BLK_CACHE_SECTORS; i++)
        if (g_cache[i].valid && g_cache[i].dev == dev)
            g_cache[i].valid = FALSE;
}


/* ================================================================
 * Регистрация
 * ================================================================ */

static void blk_copy_str(char *dst, const char *src, UINTN cap)
{
    UINTN i = 0;

    while (src != NULL && src[i] != '\0' && i + 1 < cap) {
        dst[i] = src[i];
        i++;
    }

    dst[i] = '\0';
}

static BOOLEAN blk_name_taken(const char *name)
{
    for (UINTN i = 0; i < BLK_MAX; i++)
        if (g_blk[i].used && kstreq(g_blk[i].name, name))
            return TRUE;

    return FALSE;
}

static INTN blk_alloc_slot(void)
{
    for (UINTN i = 0; i < BLK_MAX; i++)
        if (!g_blk[i].used)
            return (INTN)i;

    return -1;
}

/*
 * Новый целый диск. prefix - "usb", "sata", "nvme": имя получит
 * первый свободный номер ("usb0", "usb1"). Сразу же читаются разделы
 * и монтируются тома FAT. Возвращает индекс в g_blk или -1.
 */
INTN blk_register_disk(const char *prefix, const char *kind, const char *model,
                       UINT64 sectors, UINT32 sector_size, BOOLEAN writable,
                       const char *ro_reason, BLK_RW rw, UINTN drv_index,
                       UINT32 drv_serial)
{
    kmutex_lock(&g_vfs_mutex);

    INTN idx = blk_alloc_slot();

    if (idx < 0) {
        kmutex_unlock(&g_vfs_mutex);
        return -1;
    }

    BLKDEV *d = &g_blk[idx];

    raw_zero_mem((volatile UINT8 *)d, sizeof(*d));

    for (UINTN n = 0; n < 100; n++) {
        ksnprintf(d->name, sizeof(d->name), "%s%u", prefix, (UINT32)n);
        if (!blk_name_taken(d->name))
            break;
    }

    blk_copy_str(d->model, model, sizeof(d->model));
    d->kind = kind;
    d->sectors = sectors;
    d->sector_size = sector_size;
    d->writable = writable;
    d->ro_reason = ro_reason;
    d->rw = rw;
    d->drv_index = drv_index;
    d->drv_serial = drv_serial;
    d->gen = g_blk_gen++;
    d->parent = -1;
    d->used = TRUE;

    klog("blk: %s = %s \"%s\", %llu MiB, %s\n", d->name, kind, d->model,
         (sectors * sector_size) >> 20, writable ? "read/write" : "read-only");

    blk_scan_partitions((UINTN)idx);

    kmutex_unlock(&g_vfs_mutex);

    return idx;
}

/* Диск пропал (флешку выдернули): его тома, разделы и кэш - прочь */
void blk_remove(UINTN idx)
{
    kmutex_lock(&g_vfs_mutex);

    if (idx < BLK_MAX && g_blk[idx].used) {

        for (UINTN i = 0; i < BLK_MAX; i++) {
            if (g_blk[i].used && g_blk[i].parent == (INTN)idx) {
                vfs_forget_dev(i);
                g_blk[i].used = FALSE;
            }
        }

        vfs_forget_dev(idx);
        cache_drop_dev(idx);

        klog("blk: %s removed\n", g_blk[idx].name);
        g_blk[idx].used = FALSE;
    }

    kmutex_unlock(&g_vfs_mutex);
}


/* ================================================================
 * Чтение и запись
 * ================================================================ */

/* Раздел -> целый диск и сектор на нём. FALSE - за границей. */
static BOOLEAN blk_resolve(UINTN dev, UINT64 lba, UINT32 count,
                           UINTN *whole, UINT64 *abs_lba)
{
    if (dev >= BLK_MAX || !g_blk[dev].used)
        return FALSE;

    BLKDEV *d = &g_blk[dev];

    if (count == 0 || lba + count > d->sectors || lba + count < lba)
        return FALSE;

    if (d->parent >= 0) {
        *whole = (UINTN)d->parent;
        *abs_lba = d->start + lba;
    } else {
        *whole = dev;
        *abs_lba = lba;
    }

    return g_blk[*whole].used && g_blk[*whole].rw != NULL;
}

BOOLEAN blk_read(UINTN dev, UINT64 lba, UINT32 count, VOID *buf)
{
    UINTN w;
    UINT64 a;

    kmutex_lock(&g_vfs_mutex);

    if (!blk_resolve(dev, lba, count, &w, &a)) {
        kmutex_unlock(&g_vfs_mutex);
        return FALSE;
    }

    BLKDEV *wd = &g_blk[w];
    BOOLEAN ok;

    g_blk[dev].reads++;

    /* Кэш - только для одиночных секторов по 512 байт (FAT, папки):
       большие чтения файлов идут мимо и кэш не вымывают */
    if (count == 1 && wd->sector_size == BLK_SECTOR && g_cache_data != NULL) {

        INTN ci = cache_find(w, wd->gen, a);

        if (ci >= 0) {

            g_cache[ci].stamp = ++g_cache_clock;
            memcpy(buf, g_cache_data + (UINTN)ci * BLK_SECTOR, BLK_SECTOR);
            g_blk[dev].cache_hits++;
            g_blk_cache_hits++;
            ok = TRUE;

        } else {

            UINTN v = cache_victim();
            UINT8 *slot = g_cache_data + v * BLK_SECTOR;

            g_cache[v].valid = FALSE;
            g_blk_cache_misses++;

            ok = wd->rw(wd, a, 1, slot, FALSE);

            if (ok) {
                g_cache[v].valid = TRUE;
                g_cache[v].dev = w;
                g_cache[v].gen = wd->gen;
                g_cache[v].lba = a;
                g_cache[v].stamp = ++g_cache_clock;
                memcpy(buf, slot, BLK_SECTOR);
            }
        }

    } else {

        ok = wd->rw(wd, a, count, buf, FALSE);
    }

    if (!ok) {
        g_blk[dev].errors++;
        klog("blk: read error on %s, sector %llu (+%u)\n", g_blk[dev].name, lba, count);
    }

    kmutex_unlock(&g_vfs_mutex);

    return ok;
}

/* "Окно записи" (kernel/settings.c): раздел, на который сейчас можно
   писать, хотя весь диск - только для чтения. Открывается лишь на время
   записи файла настроек в EFI/MyOS (по команде пользователя), под
   g_vfs_mutex; -1 - закрыто. */
INTN g_blk_write_window = -1;

BOOLEAN blk_write(UINTN dev, UINT64 lba, UINT32 count, const VOID *buf)
{
    UINTN w;
    UINT64 a;

    kmutex_lock(&g_vfs_mutex);

    if (!blk_resolve(dev, lba, count, &w, &a) ||
        (!g_blk[w].writable && (INTN)dev != g_blk_write_window)) {
        kmutex_unlock(&g_vfs_mutex);
        return FALSE;
    }

    BLKDEV *wd = &g_blk[w];

    g_blk[dev].writes++;

    BOOLEAN ok = wd->rw(wd, a, count, (VOID *)buf, TRUE);

    /* кэш: у кого есть копия этих секторов - обновить (или забыть,
       если запись не удалась - на диске теперь неизвестно что) */
    if (wd->sector_size == BLK_SECTOR && g_cache_data != NULL) {
        for (UINT32 k = 0; k < count; k++) {
            INTN ci = cache_find(w, wd->gen, a + k);
            if (ci < 0)
                continue;
            if (ok)
                memcpy(g_cache_data + (UINTN)ci * BLK_SECTOR,
                             (const UINT8 *)buf + (UINTN)k * BLK_SECTOR, BLK_SECTOR);
            else
                g_cache[ci].valid = FALSE;
        }
    }

    if (!ok) {
        g_blk[dev].errors++;
        klog("blk: WRITE error on %s, sector %llu (+%u)\n", g_blk[dev].name, lba, count);
    }

    kmutex_unlock(&g_vfs_mutex);

    return ok;
}


/* ================================================================
 * Таблицы разделов
 * ================================================================ */

/* GUID из строки "C12A7328-F81F-11D2-BA4B-00A0C93EC93B" в байты, как
   он лежит на диске: первые три поля - задом наперёд (little-endian),
   остальное - как написано */
static void guid_from_str(const char *s, UINT8 out[16])
{
    UINT8 raw[16];
    UINTN n = 0;

    for (UINTN i = 0; s[i] != '\0' && n < 16; ) {

        if (s[i] == '-') {
            i++;
            continue;
        }

        UINT8 v = 0;

        for (UINTN k = 0; k < 2; k++) {
            char c = s[i + k];
            v = (UINT8)(v << 4);
            if (c >= '0' && c <= '9') v |= (UINT8)(c - '0');
            else if (c >= 'A' && c <= 'F') v |= (UINT8)(c - 'A' + 10);
            else if (c >= 'a' && c <= 'f') v |= (UINT8)(c - 'a' + 10);
        }

        raw[n++] = v;
        i += 2;
    }

    out[0] = raw[3]; out[1] = raw[2]; out[2] = raw[1]; out[3] = raw[0];
    out[4] = raw[5]; out[5] = raw[4];
    out[6] = raw[7]; out[7] = raw[6];

    for (UINTN i = 8; i < 16; i++)
        out[i] = raw[i];
}

static const struct { const char *guid; const char *name; } g_gpt_types[] = {
    { "C12A7328-F81F-11D2-BA4B-00A0C93EC93B", "EFI System" },
    { "EBD0A0A2-B9E5-4433-87C0-68B6B72699C7", "Basic data (FAT/NTFS)" },
    { "0FC63DAF-8483-4772-8E79-3D69D8477DE4", "Linux filesystem" },
    { "0657FD6D-A4AB-43C4-84E5-0933C84B4F4F", "Linux swap" },
    { "4F68BCE3-E8CD-4DB1-96E7-FBCAF984B709", "Linux root (x86-64)" },
    { "933AC7E1-2EB4-4F13-B844-0E14E2AEF915", "Linux home" },
    { "E6D6D379-F507-44C2-A23C-238F2A3DF928", "Linux LVM" },
    { "E3C9E316-0B5C-4DB8-817D-F92DF00215AE", "Microsoft reserved" },
    { "DE94BBA4-06D1-4D40-A16A-BFD50179D6AC", "Windows recovery" },
    { "21686148-6449-6E6F-744E-656564454649", "BIOS boot" },
};

static const char *mbr_type_name(UINT8 t)
{
    switch (t) {
    case 0x01: return "FAT12";
    case 0x04: case 0x06: case 0x0E: return "FAT16";
    case 0x0B: case 0x0C: return "FAT32";
    case 0x07: return "NTFS/exFAT";
    case 0x82: return "Linux swap";
    case 0x83: return "Linux";
    case 0x8E: return "Linux LVM";
    case 0xEF: return "EFI System";
    default:   return "unknown";
    }
}

/* Похож ли сектор на загрузочный сектор FAT (тогда таблицы
   разделов нет - FAT прямо с начала диска) */
static BOOLEAN looks_like_fat_vbr(const UINT8 *b)
{
    if (b[510] != 0x55 || b[511] != 0xAA)
        return FALSE;
    if (b[0] != 0xEB && b[0] != 0xE9)
        return FALSE;

    UINT16 bps = (UINT16)(b[11] | (b[12] << 8));
    UINT8 spc = b[13];

    if (bps != 512 || spc == 0 || (spc & (spc - 1)) != 0)
        return FALSE;

    return (b[54] == 'F' && b[55] == 'A' && b[56] == 'T') ||
           (b[82] == 'F' && b[83] == 'A' && b[84] == 'T');
}

static INTN blk_add_partition(UINTN disk, UINT32 no, UINT64 start, UINT64 count,
                              const char *ptype, const char *label)
{
    BLKDEV *p = &g_blk[disk];

    if (count == 0 || start >= p->sectors || start + count > p->sectors)
        return -1;

    INTN idx = blk_alloc_slot();

    if (idx < 0)
        return -1;

    BLKDEV *d = &g_blk[idx];

    raw_zero_mem((volatile UINT8 *)d, sizeof(*d));

    ksnprintf(d->name, sizeof(d->name), "%sp%u", p->name, no);
    blk_copy_str(d->model, p->model, sizeof(d->model));
    blk_copy_str(d->ptype, ptype, sizeof(d->ptype));
    blk_copy_str(d->label, label, sizeof(d->label));
    d->kind = "partition";
    d->sectors = count;
    d->sector_size = p->sector_size;
    d->writable = p->writable;
    d->ro_reason = p->ro_reason;
    d->parent = (INTN)disk;
    d->start = start;
    d->part_no = no;
    d->gen = p->gen;
    d->used = TRUE;

    return idx;
}

/* Попробовать смонтировать FAT, exFAT или ext4 на устройстве (тихо,
   если там что-то другое) */
static void blk_try_mount(UINTN idx)
{
    FAT_VOL v;
    const char *why = NULL;

    if (!fat_probe(idx, &v, &why) && !exfat_detect(idx) && !ext4_detect(idx)) {
        if (why != NULL)
            klog("blk: %s: not mounted - %s\n", g_blk[idx].name, why);
        return;
    }

    vfs_mount_dev(idx, g_blk[idx].name);
}

/*
 * Прочитать таблицу разделов диска idx и завести разделы. Тома FAT
 * сразу монтируются в /<имя раздела>.
 */
void blk_scan_partitions(UINTN idx)
{
    BLKDEV *d = &g_blk[idx];

    if (d->sector_size != BLK_SECTOR) {
        klog("blk: %s: sector size %u - partitions are not read (only 512)\n",
             d->name, d->sector_size);
        return;
    }

    UINT8 *b = (UINT8 *)kmalloc(BLK_SECTOR);

    if (b == NULL)
        return;

    if (!blk_read(idx, 0, 1, b)) {
        kfree(b);
        return;
    }

    /* 1. FAT без таблицы разделов */
    if (looks_like_fat_vbr(b)) {
        blk_copy_str(d->ptype, "whole disk", sizeof(d->ptype));
        kfree(b);
        blk_try_mount(idx);
        return;
    }

    if (b[510] != 0x55 || b[511] != 0xAA) {
        blk_copy_str(d->ptype, "no partition table", sizeof(d->ptype));
        kfree(b);
        return;
    }

    /* 2. MBR; запись типа 0xEE - "защитная": на самом деле GPT */
    BOOLEAN gpt = FALSE;
    UINT8 mbr[64];

    for (UINTN i = 0; i < 64; i++)
        mbr[i] = b[446 + i];

    for (UINTN i = 0; i < 4; i++)
        if (mbr[i * 16 + 4] == 0xEE)
            gpt = TRUE;

    if (gpt) {

        blk_copy_str(d->ptype, "GPT", sizeof(d->ptype));

        if (!blk_read(idx, 1, 1, b) ||
            b[0] != 'E' || b[1] != 'F' || b[2] != 'I' || b[3] != ' ' ||
            b[4] != 'P' || b[5] != 'A' || b[6] != 'R' || b[7] != 'T') {
            klog("blk: %s: protective MBR but no GPT header\n", d->name);
            kfree(b);
            return;
        }

        UINT64 ent_lba = 0;
        UINT32 nent = 0, esz = 0;

        for (UINTN k = 0; k < 8; k++) ent_lba |= (UINT64)b[72 + k] << (8 * k);
        for (UINTN k = 0; k < 4; k++) nent |= (UINT32)b[80 + k] << (8 * k);
        for (UINTN k = 0; k < 4; k++) esz |= (UINT32)b[84 + k] << (8 * k);

        if (esz < 128 || esz > 512 || (BLK_SECTOR % esz) != 0 || nent > 256)
            nent = 0;

        UINT32 per = (esz != 0) ? BLK_SECTOR / esz : 0;
        UINT32 no = 0;
        UINT64 cur_lba = ~0ull;

        for (UINT32 e = 0; e < nent; e++) {

            UINT64 l = ent_lba + e / per;

            if (l != cur_lba) {
                if (!blk_read(idx, l, 1, b))
                    break;
                cur_lba = l;
            }

            const UINT8 *ent = b + (e % per) * esz;
            BOOLEAN empty = TRUE;

            for (UINTN k = 0; k < 16; k++)
                if (ent[k] != 0)
                    empty = FALSE;

            no++;           /* номер - по месту в таблице, как в Linux */

            if (empty)
                continue;

            UINT64 first = 0, last = 0;

            for (UINTN k = 0; k < 8; k++) first |= (UINT64)ent[32 + k] << (8 * k);
            for (UINTN k = 0; k < 8; k++) last |= (UINT64)ent[40 + k] << (8 * k);

            const char *tname = "unknown";

            for (UINTN t = 0; t < sizeof(g_gpt_types) / sizeof(g_gpt_types[0]); t++) {
                UINT8 g[16];
                BOOLEAN same = TRUE;
                guid_from_str(g_gpt_types[t].guid, g);
                for (UINTN k = 0; k < 16; k++)
                    if (g[k] != ent[k])
                        same = FALSE;
                if (same)
                    tname = g_gpt_types[t].name;
            }

            /* имя раздела - UTF-16; берём латиницу, остальное '?' */
            char label[37];
            UINTN ln = 0;

            for (UINTN k = 0; k < 36; k++) {
                UINT16 c = (UINT16)(ent[56 + 2 * k] | (ent[57 + 2 * k] << 8));
                if (c == 0)
                    break;
                label[ln++] = (c >= 32 && c < 127) ? (char)c : '?';
            }

            label[ln] = '\0';

            INTN pi = blk_add_partition(idx, no, first, last - first + 1u, tname, label);

            if (pi >= 0)
                blk_try_mount((UINTN)pi);
        }

    } else {

        blk_copy_str(d->ptype, "MBR", sizeof(d->ptype));

        for (UINT32 i = 0; i < 4; i++) {

            const UINT8 *e = mbr + i * 16;
            UINT8 type = e[4];

            if (type == 0 || type == 0x05 || type == 0x0F)
                continue;           /* пусто / расширенный (не читаем) */

            UINT32 start = (UINT32)e[8] | ((UINT32)e[9] << 8) |
                           ((UINT32)e[10] << 16) | ((UINT32)e[11] << 24);
            UINT32 cnt = (UINT32)e[12] | ((UINT32)e[13] << 8) |
                         ((UINT32)e[14] << 16) | ((UINT32)e[15] << 24);

            INTN pi = blk_add_partition(idx, i + 1u, start, cnt, mbr_type_name(type), "");

            if (pi >= 0)
                blk_try_mount((UINTN)pi);
        }
    }

    kfree(b);
}


/* ================================================================
 * Флешки: сверка со списком драйвера USB
 * ================================================================ */

/* Прочитать/записать через флешку (drivers/usbmsd.c) */
static BOOLEAN blk_usb_rw(BLKDEV *d, UINT64 lba, UINT32 count, VOID *buf, BOOLEAN write)
{
    return usb_msd_rw(d->drv_index, d->drv_serial, lba, count, buf, write);
}

/*
 * Флешки подключает и отключает поток usb, а регистрировать диски
 * ему нельзя (порядок замков, см. начало файла). Поэтому при каждом
 * обращении к дискам (ls, cd, disk...) сверяем: какие флешки
 * пропали - убрать, какие появились - завести.
 */
void blk_sync_usb(void)
{
    kmutex_lock(&g_vfs_mutex);

    /* пропавшие */
    for (UINTN i = 0; i < BLK_MAX; i++) {

        BLKDEV *d = &g_blk[i];

        if (!d->used || d->parent >= 0 || d->rw != blk_usb_rw)
            continue;

        if (!usb_msd_alive(d->drv_index, d->drv_serial))
            blk_remove(i);
    }

    /* новые */
    for (UINTN m = 0; m < KX_MAX_MSD; m++) {

        UINT32 serial;
        UINT64 blocks;
        UINT32 bsize;
        char model[41];

        if (!usb_msd_info(m, &serial, &blocks, &bsize, model, sizeof(model)))
            continue;

        BOOLEAN known = FALSE;

        for (UINTN i = 0; i < BLK_MAX; i++)
            if (g_blk[i].used && g_blk[i].rw == blk_usb_rw &&
                g_blk[i].drv_index == m && g_blk[i].drv_serial == serial)
                known = TRUE;

        if (!known)
            blk_register_disk("usb", "USB", model, blocks, bsize, TRUE, NULL,
                              blk_usb_rw, m, serial);
    }

    kmutex_unlock(&g_vfs_mutex);
}


/* ================================================================
 * Запуск
 * ================================================================ */

void storage_init(SIMPLE_TEXT_OUTPUT_INTERFACE *out)
{
    vfs_init();

    g_cache_data = (UINT8 *)kmalloc(BLK_CACHE_SECTORS * BLK_SECTOR);

    if (g_cache_data == NULL)
        print(out, "  (no memory for the sector cache - disks will be slower)\n");

    ahci_init(out);
    nvme_init(out);

    /* флешки, найденные при старте USB */
    blk_sync_usb();

    UINTN disks = 0, vols = 0;

    for (UINTN i = 0; i < BLK_MAX; i++)
        if (g_blk[i].used && g_blk[i].parent < 0)
            disks++;

    for (UINTN i = 0; i < VFS_MAX_MOUNTS; i++) {
        if (!g_mounts[i].used)
            continue;
        vols++;
        if (vfs_is_disk(&g_mounts[i])) {
            char ty[12];
            vfs_fs_name(&g_mounts[i], ty, sizeof(ty));
            kprintf(out, "  mounted /%s: %s, label \"%s\"%s\n",
                    g_mounts[i].name, ty,
                    (g_mounts[i].ops == &g_ext4_ops) ? ext4_label(&g_mounts[i])
                                                     : g_mounts[i].fat.label,
                    g_mounts[i].readonly ? " (read-only)" : "");
        }
    }

    kprintf(out, "  Disks: %u; mounted: %u (including /ram - the RAM disk)\n",
            (UINT32)disks, (UINT32)vols);
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

static INTN blk_find_name(const char *name)
{
    for (UINTN i = 0; i < BLK_MAX; i++)
        if (g_blk[i].used && kstreq(g_blk[i].name, name))
            return (INTN)i;

    return -1;
}

static const char *blk_mount_of(UINTN dev)
{
    for (UINTN i = 0; i < VFS_MAX_MOUNTS; i++)
        if (g_mounts[i].used && !g_mounts[i].gone &&
            vfs_is_disk(&g_mounts[i]) && g_mounts[i].dev == dev)
            return g_mounts[i].name;

    return NULL;
}

static void disk_list(SIMPLE_TEXT_OUTPUT_INTERFACE *out)
{
    UINTN n = 0;

    for (UINTN i = 0; i < BLK_MAX; i++) {

        BLKDEV *d = &g_blk[i];

        if (!d->used || d->parent >= 0)
            continue;

        n++;

        UINT64 mib = (d->sectors * d->sector_size) >> 20;

        kprintf(out, "%-8s %-12s \"%s\", %llu MiB, %s", d->name, d->kind, d->model,
                mib, d->ptype[0] ? d->ptype : "?");
        kprintf(out, ", %s\n", d->writable ? "read/write" : "READ-ONLY");

        if (!d->writable && d->ro_reason)
            kprintf(out, "           (%s)\n", d->ro_reason);

        const char *mw = blk_mount_of(i);

        if (mw)
            kprintf(out, "           mounted: /%s\n", mw);

        for (UINTN j = 0; j < BLK_MAX; j++) {

            BLKDEV *p = &g_blk[j];

            if (!p->used || p->parent != (INTN)i)
                continue;

            const char *m = blk_mount_of(j);

            kprintf(out, "  %-10s %8llu MiB  %-22s", p->name,
                    (p->sectors * p->sector_size) >> 20, p->ptype);

            if (p->label[0])
                kprintf(out, " \"%s\"", p->label);

            if (m)
                kprintf(out, "  -> /%s", m);

            print(out, "\n");
        }
    }

    if (n == 0)
        print(out, "No disks found. Plug in a USB flash drive - it is picked up automatically.\n");

    kprintf(out, "Sector cache: %llu hits, %llu misses\n", g_blk_cache_hits, g_blk_cache_misses);
    print(out, "Raw sector: disk read <sector> [name]   Files: ls /, cd /usb0p1\n");
}

void kernel_cmd_disk(SIMPLE_TEXT_OUTPUT_INTERFACE *out, const char *arg)
{
    blk_sync_usb();

    kmutex_lock(&g_vfs_mutex);

    if (arg[0] == '\0') {
        disk_list(out);
        kmutex_unlock(&g_vfs_mutex);
        return;
    }

    /* disk protect <диск>: сделать диск (и его тома) только для чтения,
       как внутренний диск ноутбука - для проверки в QEMU */
    if (arg[0] == 'p' && arg[1] == 'r' && arg[2] == 'o' && arg[3] == 't' &&
        arg[4] == 'e' && arg[5] == 'c' && arg[6] == 't' && arg[7] == ' ') {

        const char *name = arg + 8;
        BOOLEAN found = FALSE;

        for (UINTN i = 0; i < BLK_MAX; i++) {

            BLKDEV *d = &g_blk[i];

            if (!d->used || !kstreq(d->name, name))
                continue;

            found = TRUE;
            d->writable = FALSE;
            d->ro_reason = "protected by 'disk protect'";

            for (UINTN k = 0; k < BLK_MAX; k++)
                if (g_blk[k].used && g_blk[k].parent == (INTN)i) {
                    g_blk[k].writable = FALSE;
                    g_blk[k].ro_reason = d->ro_reason;
                }

            for (UINTN k = 0; k < VFS_MAX_MOUNTS; k++)
                if (g_mounts[k].used && (g_mounts[k].dev == i || g_blk[g_mounts[k].dev].parent == (INTN)i))
                    g_mounts[k].readonly = TRUE;
        }

        kprintf(out, found ? "%s is read-only now.\n" : "No disk named '%s'.\n", name);
        kmutex_unlock(&g_vfs_mutex);
        return;
    }

    if (arg[0] == 'r' && arg[1] == 'e' && arg[2] == 'a' && arg[3] == 'd') {

        const char *p = arg + 4;
        UINT64 lba = 0;
        char name[16];
        UINTN k = 0;

        while (*p == ' ') p++;
        while (*p >= '0' && *p <= '9') lba = lba * 10u + (UINT64)(*p++ - '0');
        while (*p == ' ') p++;
        while (*p != '\0' && *p != ' ' && k + 1 < sizeof(name)) name[k++] = *p++;
        name[k] = '\0';

        INTN di = -1;

        if (name[0] == '\0') {
            for (UINTN i = 0; i < BLK_MAX && di < 0; i++)
                if (g_blk[i].used && g_blk[i].parent < 0)
                    di = (INTN)i;
        } else if (name[0] >= '0' && name[0] <= '9' && name[1] == '\0') {
            /* старый вид "disk read N D": D-й диск по счёту */
            UINTN want = (UINTN)(name[0] - '0');
            for (UINTN i = 0; i < BLK_MAX && di < 0; i++)
                if (g_blk[i].used && g_blk[i].parent < 0) {
                    if (want == 0)
                        di = (INTN)i;
                    else
                        want--;
                }
        } else {
            di = blk_find_name(name);
        }

        if (di < 0) {
            print(out, "No such disk (see 'disk').\n");
            kmutex_unlock(&g_vfs_mutex);
            return;
        }

        BLKDEV *d = &g_blk[di];
        UINT8 *buf = (UINT8 *)kmalloc(d->sector_size);

        if (buf == NULL) {
            print(out, "Out of memory.\n");
            kmutex_unlock(&g_vfs_mutex);
            return;
        }

        if (!blk_read((UINTN)di, lba, 1, buf)) {
            kprintf(out, "Read of sector %llu failed.\n", lba);
            kfree(buf);
            kmutex_unlock(&g_vfs_mutex);
            return;
        }

        kprintf(out, "%s, sector %llu (first 256 of %u bytes):\n",
                d->name, lba, d->sector_size);
        disk_hexdump(out, buf, 256);

        if (d->sector_size >= 512 && buf[510] == 0x55 && buf[511] == 0xAA) {
            if (lba == 0 && d->parent < 0 && !looks_like_fat_vbr(buf)) {
                print(out, "Boot signature 55 AA: this is an MBR. Partitions:\n");
                for (UINTN i = 0; i < 4; i++) {
                    const UINT8 *e = buf + 446 + i * 16;
                    UINT32 start = e[8] | (e[9] << 8) | (e[10] << 16) | ((UINT32)e[11] << 24);
                    UINT32 size = e[12] | (e[13] << 8) | (e[14] << 16) | ((UINT32)e[15] << 24);
                    if (e[4] == 0)
                        continue;
                    kprintf(out, "  #%u type 0x%02x (%s), start %u, %u MiB\n",
                            (UINT32)i + 1, e[4],
                            e[4] == 0xEE ? "GPT protective" : mbr_type_name(e[4]),
                            start, (UINT32)(((UINT64)size * d->sector_size) >> 20));
                }
            } else if (looks_like_fat_vbr(buf)) {
                print(out, "This is a FAT boot sector (the start of a FAT volume).\n");
            } else {
                print(out, "Boot signature 55 AA at the end.\n");
            }
        }

        if (buf[0] == 'E' && buf[1] == 'F' && buf[2] == 'I' && buf[3] == ' ' &&
            buf[4] == 'P' && buf[5] == 'A' && buf[6] == 'R' && buf[7] == 'T')
            print(out, "\"EFI PART\": this is a GPT header.\n");

        kfree(buf);
        kmutex_unlock(&g_vfs_mutex);
        return;
    }

    print(out, "Usage: disk                 - disks, partitions, where they are mounted\n");
    print(out, "       disk read N [name]   - show sector N (e.g. disk read 0 usb0)\n");

    kmutex_unlock(&g_vfs_mutex);
}


/* ================================================================
 * Общее для драйверов дисков
 * ================================================================ */

/*
 * Подождать, пока в регистре устройства (reg - уже виртуальный
 * адрес) биты mask станут равны want. Первые ~100 мкс просто
 * крутимся (быстрый SSD успевает), дальше - спим по миллисекунде,
 * чтобы не отнимать процессор у остальных потоков. FALSE - таймаут.
 */
BOOLEAN blk_wait(volatile UINT32 *reg, UINT32 mask, UINT32 want, UINT64 timeout_ms)
{
    UINT64 hz = g_tsc_hz ? g_tsc_hz : 2000000000ull;
    UINT64 spin = hz / 10000u;
    UINT64 limit = (hz / 1000u) * timeout_ms;
    UINT64 t0 = rdtsc();

    for (;;) {

        if ((*reg & mask) == want)
            return TRUE;

        UINT64 el = rdtsc() - t0;

        if (el > limit)
            return FALSE;

        if (el > spin && sched_can_block())
            sched_sleep_ms(1);
        else
            cpu_pause();
    }
}
