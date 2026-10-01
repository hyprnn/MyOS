/*
 * fs/fat.c - файловая система FAT16 / FAT32: чтение и запись (этап 5).
 * Часть MyOS; общие объявления - в myos.h.
 *
 * FAT понимают и прошивка UEFI (раздел EFI - всегда FAT), и Linux, и
 * Windows, и любой фотоаппарат - поэтому с неё и начинаем: файл,
 * записанный здесь, откроется в Arch.
 *
 * КАК УСТРОЕН ТОМ FAT
 * -------------------
 *   [загрузочный сектор][резерв][FAT #1][FAT #2]([корень FAT16])[данные]
 *
 * * Загрузочный сектор (сектор 0 тома) - "паспорт": байт в секторе,
 *   секторов в кластере, сколько копий FAT, где корень и т.д. (BPB).
 * * Данные делятся на КЛАСТЕРЫ (1..64 сектора подряд). Номера
 *   кластеров начинаются с 2.
 * * FAT - таблица: для каждого кластера - номер СЛЕДУЮЩЕГО кластера
 *   того же файла. Файл = цепочка: первый кластер записан в папке,
 *   дальше идём по таблице до метки "конец" (EOC). 0 - кластер
 *   свободен. FAT16 - 2 байта на кластер, FAT32 - 4 (из них 28 бит).
 *   Копий таблицы обычно две - пишем в обе.
 * * Папка - это тоже файл: массив записей по 32 байта. Запись:
 *   имя 8.3 ("README  TXT"), атрибуты (папка? только чтение?),
 *   время, первый кластер, размер. Первый байт 0xE5 - запись
 *   удалена, 0x00 - дальше записей нет.
 * * Длинные имена (LFN, "Мой отчёт.txt" или "notes-2026.txt"):
 *   перед обычной записью лежат несколько служебных (атрибут 0x0F),
 *   в каждой - 13 символов имени в UTF-16, в обратном порядке. В
 *   каждой - контрольная сумма короткого имени: так видно, что они
 *   относятся именно к этой записи. Короткое имя при этом тоже
 *   нужно ("NOTES-~1.TXT") - его видят старые системы.
 * * Корень: у FAT16 - отдельная область фиксированного размера перед
 *   данными; у FAT32 - обычная цепочка кластеров (номер - в BPB).
 * * FAT32 хранит подсказку "сколько свободно" в секторе FSInfo.
 *   Пересчитывать её при каждой записи дорого - при первой записи
 *   мы пишем туда "не знаю" (0xFFFFFFFF): это законно, Linux и
 *   Windows тогда посчитают сами.
 *
 * В этом файле номер "папки" - номер её первого кластера; 0 -
 * корень FAT16 (у него нет кластеров). Узел VFS_NODE хранит, где
 * лежит его запись: parent_cluster + entry_index (+ сколько записей
 * длинного имени перед ней, чтобы при удалении стереть и их).
 */
#include "myos.h"


/* ================================================================
 * Мелочи
 * ================================================================ */

static UINT16 rd16(const UINT8 *p) { return (UINT16)(p[0] | (p[1] << 8)); }
static UINT32 rd32(const UINT8 *p)
{
    return (UINT32)p[0] | ((UINT32)p[1] << 8) | ((UINT32)p[2] << 16) | ((UINT32)p[3] << 24);
}
static void wr16(UINT8 *p, UINT16 v) { p[0] = (UINT8)v; p[1] = (UINT8)(v >> 8); }
static void wr32(UINT8 *p, UINT32 v)
{
    p[0] = (UINT8)v; p[1] = (UINT8)(v >> 8); p[2] = (UINT8)(v >> 16); p[3] = (UINT8)(v >> 24);
}

static char up(char c) { return (c >= 'a' && c <= 'z') ? (char)(c - 'a' + 'A') : c; }

static BOOLEAN name_eq_ci(const char *a, const char *b)
{
    while (*a && *b) {
        if (up(*a) != up(*b))
            return FALSE;
        a++;
        b++;
    }

    return *a == *b;
}

static FAT_VOL *VOL(VFS_MOUNT *m) { return &m->fat; }

static BOOLEAN is_eoc(FAT_VOL *v, UINT32 x)
{
    return (v->fat_bits == 16) ? (x >= 0xFFF8u) : (x >= 0x0FFFFFF8u);
}

static UINT32 eoc_mark(FAT_VOL *v)
{
    return (v->fat_bits == 16) ? 0xFFFFu : 0x0FFFFFFFu;
}

/* Номер кластера годный (не 0/1, не "плохой", не за концом) */
static BOOLEAN cl_ok(FAT_VOL *v, UINT32 cl)
{
    return cl >= 2 && cl < v->clusters + 2u;
}

static UINT64 cl_lba(FAT_VOL *v, UINT32 cl)
{
    return (UINT64)v->data_start + (UINT64)(cl - 2u) * v->spc;
}

/* Дата и время для записей папки (местное время, как у Windows и
   как ждёт Linux с настройками по умолчанию) */
static void fat_now(UINT16 *date, UINT16 *time)
{
    EFI_TIME t;

    if (!rtc_read(&t)) {
        *date = (UINT16)(((2026 - 1980) << 9) | (1 << 5) | 1);
        *time = 0;
        return;
    }

    tz_to_local(&t);

    *date = (UINT16)(((t.Year - 1980) << 9) | (t.Month << 5) | t.Day);
    *time = (UINT16)((t.Hour << 11) | (t.Minute << 5) | (t.Second / 2));
}


/* ================================================================
 * Проверка и "паспорт" тома
 * ================================================================ */

BOOLEAN fat_probe(UINTN dev, FAT_VOL *v, const char **why)
{
    UINT8 b[512];

    *why = NULL;

    if (g_blk[dev].sector_size != 512) {
        *why = "sector size is not 512";
        return FALSE;
    }

    if (!blk_read(dev, 0, 1, b)) {
        *why = "cannot read the first sector";
        return FALSE;
    }

    if (b[3] == 'E' && b[4] == 'X' && b[5] == 'F' && b[6] == 'A' && b[7] == 'T') {
        *why = "exFAT - not supported yet";
        return FALSE;
    }

    if (b[3] == 'N' && b[4] == 'T' && b[5] == 'F' && b[6] == 'S') {
        *why = "NTFS - not supported";
        return FALSE;
    }

    if (b[510] != 0x55 || b[511] != 0xAA)
        return FALSE;                  /* тихо: не FAT (например, ext4) */

    UINT32 bps = rd16(b + 11);
    UINT32 spc = b[13];
    UINT32 reserved = rd16(b + 14);
    UINT32 nfats = b[16];
    UINT32 root_entries = rd16(b + 17);
    UINT32 total = rd16(b + 19) ? rd16(b + 19) : rd32(b + 32);
    UINT32 fat_size = rd16(b + 22) ? rd16(b + 22) : rd32(b + 36);

    if (bps != 512 || spc == 0 || (spc & (spc - 1)) != 0 || reserved == 0 ||
        nfats == 0 || nfats > 4 || fat_size == 0 || total == 0)
        return FALSE;

    raw_zero_mem((volatile UINT8 *)v, sizeof(*v));

    v->dev = dev;
    v->spc = spc;
    v->cluster_bytes = spc * 512u;
    v->reserved = reserved;
    v->nfats = nfats;
    v->fat_size = fat_size;
    v->root_entries = root_entries;
    v->root_sectors = (root_entries * 32u + 511u) / 512u;
    v->root_sector = reserved + nfats * fat_size;
    v->data_start = v->root_sector + v->root_sectors;

    if (total <= v->data_start || total > g_blk[dev].sectors) {
        *why = "the FAT header does not fit the partition";
        return FALSE;
    }

    v->clusters = (total - v->data_start) / spc;

    /* тип FAT определяется ТОЛЬКО числом кластеров (так в спецификации
       Microsoft) */
    if (v->clusters < 4085) {
        *why = "FAT12 (tiny volume) - not supported";
        return FALSE;
    }

    v->fat_bits = (v->clusters < 65525) ? 16 : 32;

    const UINT8 *lab;

    if (v->fat_bits == 32) {
        v->root_cluster = rd32(b + 44);
        v->fsinfo = rd16(b + 48);
        lab = b + 71;
        if (!cl_ok(v, v->root_cluster)) {
            *why = "bad root cluster";
            return FALSE;
        }
    } else {
        lab = b + 43;
    }

    UINTN n = 0;

    for (UINTN i = 0; i < 11; i++)
        v->label[n++] = (lab[i] >= 32 && lab[i] < 127) ? (char)lab[i] : ' ';

    while (n > 0 && v->label[n - 1] == ' ')
        n--;

    v->label[n] = '\0';

    if (kstreq(v->label, "NO NAME"))
        v->label[0] = '\0';

    v->next_free = 2;
    v->free_count = -1;

    /* FAT32: подсказка из FSInfo, если она осмысленная */
    if (v->fat_bits == 32 && v->fsinfo != 0 && v->fsinfo < reserved &&
        blk_read(dev, v->fsinfo, 1, b) &&
        rd32(b) == 0x41615252u && rd32(b + 484) == 0x61417272u) {

        UINT32 fc = rd32(b + 488);
        UINT32 nf = rd32(b + 492);

        if (fc <= v->clusters)
            v->free_count = fc;
        if (cl_ok(v, nf))
            v->next_free = nf;
    }

    return TRUE;
}


/* ================================================================
 * Таблица FAT
 * ================================================================ */

static BOOLEAN fat_get(FAT_VOL *v, UINT32 cl, UINT32 *val)
{
    UINT8 b[512];
    UINT32 off = cl * (v->fat_bits / 8u);

    if (!blk_read(v->dev, v->reserved + off / 512u, 1, b))
        return FALSE;

    *val = (v->fat_bits == 16) ? rd16(b + off % 512u) : (rd32(b + off % 512u) & 0x0FFFFFFFu);

    return TRUE;
}

/* Записать значение во ВСЕ копии таблицы */
static BOOLEAN fat_set(FAT_VOL *v, UINT32 cl, UINT32 val)
{
    UINT8 b[512];
    UINT32 off = cl * (v->fat_bits / 8u);

    for (UINT32 f = 0; f < v->nfats; f++) {

        UINT64 lba = (UINT64)v->reserved + (UINT64)f * v->fat_size + off / 512u;

        if (!blk_read(v->dev, lba, 1, b))
            return FALSE;

        if (v->fat_bits == 16) {
            wr16(b + off % 512u, (UINT16)val);
        } else {
            UINT32 old = rd32(b + off % 512u);
            wr32(b + off % 512u, (old & 0xF0000000u) | (val & 0x0FFFFFFFu));
        }

        if (!blk_write(v->dev, lba, 1, b))
            return FALSE;
    }

    return TRUE;
}

/* При первой записи в том FAT32 - сказать FSInfo "свободное место:
   не знаю" (см. начало файла) */
static void fat_touch_fsinfo(FAT_VOL *v)
{
    if (v->fat_bits != 32 || v->fsinfo_invalidated || v->fsinfo == 0)
        return;

    v->fsinfo_invalidated = TRUE;

    UINT8 b[512];

    if (!blk_read(v->dev, v->fsinfo, 1, b) ||
        rd32(b) != 0x41615252u || rd32(b + 484) != 0x61417272u)
        return;

    wr32(b + 488, 0xFFFFFFFFu);
    wr32(b + 492, 0xFFFFFFFFu);
    blk_write(v->dev, v->fsinfo, 1, b);
}

static BOOLEAN fat_zero_cluster(FAT_VOL *v, UINT32 cl)
{
    UINT8 z[512];

    raw_zero_mem((volatile UINT8 *)z, sizeof(z));

    for (UINT32 s = 0; s < v->spc; s++)
        if (!blk_write(v->dev, cl_lba(v, cl) + s, 1, z))
            return FALSE;

    return TRUE;
}

/*
 * Взять свободный кластер, пометить "конец цепочки" и, если prev != 0,
 * прицепить после prev. zero - обнулить (для папок). 0 - места нет.
 */
static UINT32 fat_alloc(FAT_VOL *v, UINT32 prev, BOOLEAN zero)
{
    fat_touch_fsinfo(v);

    UINT32 total = v->clusters;
    UINT32 cl = cl_ok(v, v->next_free) ? v->next_free : 2u;

    for (UINT32 k = 0; k < total; k++) {

        UINT32 val;

        if (!fat_get(v, cl, &val))
            return 0;

        if (val == 0) {

            if (!fat_set(v, cl, eoc_mark(v)))
                return 0;

            if (zero && !fat_zero_cluster(v, cl))
                return 0;

            if (prev != 0 && !fat_set(v, prev, cl))
                return 0;

            v->next_free = cl + 1u;

            if (v->free_count > 0)
                v->free_count--;

            return cl;
        }

        cl++;

        if (cl >= total + 2u)
            cl = 2;
    }

    v->free_count = 0;

    return 0;
}

/* Освободить цепочку, начиная с cl */
static void fat_free_chain(FAT_VOL *v, UINT32 cl)
{
    fat_touch_fsinfo(v);

    for (UINT32 guard = 0; cl_ok(v, cl) && guard <= v->clusters; guard++) {

        UINT32 next;

        if (!fat_get(v, cl, &next))
            return;

        fat_set(v, cl, 0);

        if (v->free_count >= 0)
            v->free_count++;

        if (cl < v->next_free)
            v->next_free = cl;

        if (is_eoc(v, next) || next == 0)
            return;

        cl = next;
    }
}


/* ================================================================
 * Записи папок
 * ================================================================ */

/*
 * Где лежит запись номер idx папки dcl (0 - корень FAT16). extend -
 * если папка кончилась, удлинить её новым (обнулённым) кластером.
 * FALSE - записей больше нет (или места нет).
 */
static BOOLEAN dir_loc(FAT_VOL *v, UINT32 dcl, UINT32 idx, BOOLEAN extend,
                       UINT64 *lba, UINT32 *off)
{
    if (dcl == 0) {

        if (idx >= v->root_entries)
            return FALSE;

        *lba = v->root_sector + (idx * 32u) / 512u;
        *off = (idx * 32u) % 512u;
        return TRUE;
    }

    UINT32 per = v->cluster_bytes / 32u;
    UINT32 hops = idx / per;
    UINT32 cl = dcl;

    for (UINT32 i = 0; i < hops; i++) {

        UINT32 next;

        if (!fat_get(v, cl, &next))
            return FALSE;

        if (is_eoc(v, next) || !cl_ok(v, next)) {

            if (!extend)
                return FALSE;

            next = fat_alloc(v, cl, TRUE);

            if (next == 0)
                return FALSE;
        }

        cl = next;
    }

    UINT32 in = (idx % per) * 32u;

    *lba = cl_lba(v, cl) + in / 512u;
    *off = in % 512u;

    return TRUE;
}

static BOOLEAN dir_read_ent(FAT_VOL *v, UINT32 dcl, UINT32 idx, UINT8 e[32])
{
    UINT64 lba;
    UINT32 off;
    UINT8 b[512];

    if (!dir_loc(v, dcl, idx, FALSE, &lba, &off) || !blk_read(v->dev, lba, 1, b))
        return FALSE;

    for (UINTN i = 0; i < 32; i++)
        e[i] = b[off + i];

    return TRUE;
}

static BOOLEAN dir_write_ent(FAT_VOL *v, UINT32 dcl, UINT32 idx, const UINT8 e[32])
{
    UINT64 lba;
    UINT32 off;
    UINT8 b[512];

    if (!dir_loc(v, dcl, idx, TRUE, &lba, &off) || !blk_read(v->dev, lba, 1, b))
        return FALSE;

    for (UINTN i = 0; i < 32; i++)
        b[off + i] = e[i];

    return blk_write(v->dev, lba, 1, b);
}

/* Контрольная сумма короткого имени (для записей длинного имени) */
static UINT8 lfn_sum(const UINT8 *sn)
{
    UINT8 s = 0;

    for (UINTN i = 0; i < 11; i++)
        s = (UINT8)(((s & 1u) << 7) + (s >> 1) + sn[i]);

    return s;
}

/* Короткое имя 8.3 из записи -> "README.TXT" (с учётом флагов
   "строчными", которые ставят Windows и Linux) */
static void short_to_str(const UINT8 *e, char *out)
{
    UINTN n = 0;
    BOOLEAN lb = (e[12] & 0x08) != 0, le = (e[12] & 0x10) != 0;

    for (UINTN i = 0; i < 8 && e[i] != ' '; i++) {
        char c = (char)((i == 0 && e[0] == 0x05) ? 0xE5 : e[i]);
        if (lb && c >= 'A' && c <= 'Z') c = (char)(c - 'A' + 'a');
        out[n++] = ((UINT8)c < 32 || (UINT8)c > 126) ? '?' : c;
    }

    if (e[8] != ' ') {
        out[n++] = '.';
        for (UINTN i = 8; i < 11 && e[i] != ' '; i++) {
            char c = (char)e[i];
            if (le && c >= 'A' && c <= 'Z') c = (char)(c - 'A' + 'a');
            out[n++] = ((UINT8)c < 32 || (UINT8)c > 126) ? '?' : c;
        }
    }

    out[n] = '\0';
}

static UINT32 node_dir_cluster(FAT_VOL *v, VFS_NODE *dir)
{
    if (dir->is_root)
        return (v->fat_bits == 32) ? v->root_cluster : 0;

    return dir->first_cluster;
}

/*
 * Следующая "живая" запись папки начиная с *cookie: имя (длинное,
 * если есть и сходится контрольная сумма), узел. 1 - есть, 0 - конец,
 * <0 - ошибка. Записи "." и ".." пропускаются (VFS разбирает пути
 * сама).
 */
static INTN fat_readdir(VFS_MOUNT *m, VFS_NODE *dir, UINT64 *cookie, VFS_DIRENT *out)
{
    FAT_VOL *v = VOL(m);
    UINT32 dcl = node_dir_cluster(v, dir);
    UINT16 lfn[20 * 13 + 1];
    BOOLEAN have = FALSE;
    UINT8 sum = 0;
    UINT32 need = 0, got = 0;

    for (UINT32 idx = (UINT32)*cookie; idx < 65536u; idx++) {

        UINT8 e[32];

        if (!dir_read_ent(v, dcl, idx, e)) {
            *cookie = idx;
            return 0;
        }

        if (e[0] == 0x00) {
            *cookie = idx;
            return 0;
        }

        if (e[0] == 0xE5) {
            have = FALSE;
            continue;
        }

        if (e[11] == 0x0F) {

            UINT8 seq = e[0];
            UINT32 ord = seq & 0x1Fu;

            if (seq & 0x40) {
                have = TRUE;
                sum = e[13];
                need = ord;
                got = 0;
                for (UINTN i = 0; i < sizeof(lfn) / sizeof(lfn[0]); i++)
                    lfn[i] = 0;
            }

            if (!have || ord == 0 || ord > 20 || e[13] != sum) {
                have = FALSE;
                continue;
            }

            static const UINT8 pos[13] = { 1, 3, 5, 7, 9, 14, 16, 18, 20, 22, 24, 28, 30 };
            UINT32 base = (ord - 1u) * 13u;

            for (UINTN k = 0; k < 13; k++)
                lfn[base + k] = rd16(e + pos[k]);

            got++;
            continue;
        }

        if (e[11] & 0x08) {           /* метка тома */
            have = FALSE;
            continue;
        }

        /* обычная (короткая) запись */
        BOOLEAN dot = (e[0] == '.' && (e[1] == ' ' || (e[1] == '.' && e[2] == ' ')));

        if (dot) {
            have = FALSE;
            continue;
        }

        UINT32 lfn_used = 0;

        if (have && got == need && lfn_sum(e) == sum) {

            UINTN n = 0;

            for (UINTN i = 0; i < need * 13u && n + 1 < VFS_NAME_MAX; i++) {
                UINT16 c = lfn[i];
                if (c == 0x0000 || c == 0xFFFF)
                    break;
                out->name[n++] = (c >= 32 && c < 127) ? (char)c : '?';
            }

            out->name[n] = '\0';
            lfn_used = need;

        } else {
            short_to_str(e, out->name);
        }

        have = FALSE;

        VFS_NODE *nd = &out->node;

        raw_zero_mem((volatile UINT8 *)nd, sizeof(*nd));
        nd->attr = e[11];
        nd->is_dir = (e[11] & 0x10) != 0;
        nd->first_cluster = ((v->fat_bits == 32) ? ((UINT32)rd16(e + 20) << 16) : 0) |
                            rd16(e + 26);
        nd->size = nd->is_dir ? 0 : rd32(e + 28);
        nd->parent_cluster = dcl;
        nd->entry_index = idx;
        nd->lfn_count = lfn_used;
        nd->wtime = rd16(e + 22);
        nd->wdate = rd16(e + 24);
        nd->ram_index = -1;

        *cookie = idx + 1u;
        return 1;
    }

    return VFS_EIO;
}

static INTN fat_root(VFS_MOUNT *m, VFS_NODE *out)
{
    FAT_VOL *v = VOL(m);

    raw_zero_mem((volatile UINT8 *)out, sizeof(*out));
    out->is_root = TRUE;
    out->is_dir = TRUE;
    out->first_cluster = (v->fat_bits == 32) ? v->root_cluster : 0;
    out->ram_index = -1;
    out->attr = 0x10;

    return VFS_OK;
}

static INTN fat_lookup(VFS_MOUNT *m, VFS_NODE *dir, const char *name, VFS_NODE *out)
{
    UINT64 cookie = 0;
    VFS_DIRENT *de = (VFS_DIRENT *)kmalloc(sizeof(VFS_DIRENT));

    if (de == NULL)
        return VFS_EIO;

    for (;;) {

        INTN r = fat_readdir(m, dir, &cookie, de);

        if (r <= 0) {
            kfree(de);
            return (r == 0) ? VFS_ENOENT : r;
        }

        if (name_eq_ci(de->name, name)) {
            *out = de->node;
            kfree(de);
            return VFS_OK;
        }
    }
}


/* ================================================================
 * Имена: короткое 8.3 и длинное
 * ================================================================ */

static BOOLEAN sn_char_ok(char c)
{
    if (c >= 'A' && c <= 'Z') return TRUE;
    if (c >= '0' && c <= '9') return TRUE;

    const char *extra = "!#$%&'()-@^_`{}~";

    for (UINTN i = 0; extra[i]; i++)
        if (extra[i] == c)
            return TRUE;

    return FALSE;
}

/* Можно ли вообще так назвать файл */
static BOOLEAN name_valid(const char *s)
{
    UINTN n = 0;

    if (s[0] == '\0' || kstreq(s, ".") || kstreq(s, ".."))
        return FALSE;

    for (; s[n] != '\0'; n++) {
        char c = s[n];
        if ((UINT8)c < 32 || c == '/' || c == '\\' || c == ':' || c == '*' ||
            c == '?' || c == '"' || c == '<' || c == '>' || c == '|')
            return FALSE;
    }

    /* пробел или точка в конце Windows не любит */
    if (s[n - 1] == ' ' || s[n - 1] == '.')
        return FALSE;

    return n <= 255;
}

/*
 * Помещается ли имя в 8.3 ТОЧНО (тогда длинное не нужно): до 8
 * символов, точка, до 3; буквы - либо все заглавные, либо все
 * строчные в имени и (отдельно) в расширении - это запоминается
 * флагами в байте 12, так делает Windows NT и понимает Linux.
 */
static BOOLEAN fits_83(const char *s, UINT8 sn[11], UINT8 *flags)
{
    UINTN dot = 0, len = 0;
    BOOLEAN has_dot = FALSE;

    for (; s[len]; len++)
        if (s[len] == '.') {
            if (has_dot)
                return FALSE;
            has_dot = TRUE;
            dot = len;
        }

    UINTN blen = has_dot ? dot : len;
    UINTN elen = has_dot ? len - dot - 1 : 0;

    if (blen == 0 || blen > 8 || elen > 3 || (has_dot && elen == 0))
        return FALSE;

    for (UINTN i = 0; i < 11; i++)
        sn[i] = ' ';

    *flags = 0;

    BOOLEAN bl = FALSE, bu = FALSE, el = FALSE, eu = FALSE;

    for (UINTN i = 0; i < blen; i++) {
        char c = s[i];
        if (c >= 'a' && c <= 'z') bl = TRUE;
        if (c >= 'A' && c <= 'Z') bu = TRUE;
        c = up(c);
        if (!sn_char_ok(c))
            return FALSE;
        sn[i] = (UINT8)c;
    }

    for (UINTN i = 0; i < elen; i++) {
        char c = s[dot + 1 + i];
        if (c >= 'a' && c <= 'z') el = TRUE;
        if (c >= 'A' && c <= 'Z') eu = TRUE;
        c = up(c);
        if (!sn_char_ok(c))
            return FALSE;
        sn[8 + i] = (UINT8)c;
    }

    if ((bl && bu) || (el && eu))
        return FALSE;               /* "ReadMe" - смешанный: нужно длинное */

    if (bl) *flags |= 0x08;
    if (el) *flags |= 0x10;

    return TRUE;
}

/* Есть ли в папке запись с таким коротким именем */
static BOOLEAN short_exists(FAT_VOL *v, UINT32 dcl, const UINT8 sn[11])
{
    for (UINT32 idx = 0; idx < 65536u; idx++) {

        UINT8 e[32];

        if (!dir_read_ent(v, dcl, idx, e) || e[0] == 0x00)
            return FALSE;

        if (e[0] == 0xE5 || e[11] == 0x0F)
            continue;

        BOOLEAN same = TRUE;

        for (UINTN i = 0; i < 11; i++)
            if (e[i] != sn[i])
                same = FALSE;

        if (same)
            return TRUE;
    }

    return FALSE;
}

/* Короткое имя-"псевдоним" для длинного: "notes-2026.txt" ->
   "NOTES-~1TXT" (первый свободный номер) */
static BOOLEAN make_alias(FAT_VOL *v, UINT32 dcl, const char *s, UINT8 sn[11])
{
    UINTN len = 0, dot = 0;
    BOOLEAN has_dot = FALSE;

    for (; s[len]; len++)
        if (s[len] == '.') {
            has_dot = TRUE;
            dot = len;           /* последняя точка */
        }

    char base[9], ext[4];
    UINTN bn = 0, en = 0;

    for (UINTN i = 0; i < (has_dot ? dot : len) && bn < 6; i++) {
        char c = up(s[i]);
        if (c == ' ' || c == '.')
            continue;
        base[bn++] = sn_char_ok(c) ? c : '_';
    }

    if (bn == 0)
        base[bn++] = '_';

    if (has_dot)
        for (UINTN i = dot + 1; i < len && en < 3; i++) {
            char c = up(s[i]);
            if (c == ' ')
                continue;
            ext[en++] = sn_char_ok(c) ? c : '_';
        }

    for (UINT32 num = 1; num < 1000000u; num++) {

        char tail[8];
        UINTN tn = ksnprintf(tail, sizeof(tail), "~%u", num);
        UINTN keep = (bn + tn > 8) ? 8 - tn : bn;

        for (UINTN i = 0; i < 11; i++)
            sn[i] = ' ';
        for (UINTN i = 0; i < keep; i++)
            sn[i] = (UINT8)base[i];
        for (UINTN i = 0; i < tn; i++)
            sn[keep + i] = (UINT8)tail[i];
        for (UINTN i = 0; i < en; i++)
            sn[8 + i] = (UINT8)ext[i];

        if (!short_exists(v, dcl, sn))
            return TRUE;
    }

    return FALSE;
}

/*
 * Завести запись в папке: длинное имя (если нужно) + короткая
 * запись. Остальное - из tmpl (атрибуты, кластер, размер, время).
 * out - узел новой записи.
 */
static INTN dir_add(FAT_VOL *v, UINT32 dcl, const char *name, const UINT8 tmpl[32],
                    VFS_NODE *out)
{
    if (!name_valid(name))
        return VFS_EINVAL;

    UINT8 sn[11];
    UINT8 flags = 0;
    UINT32 nlfn = 0;
    UINTN len = 0;

    while (name[len])
        len++;

    if (!fits_83(name, sn, &flags) || short_exists(v, dcl, sn)) {
        flags = 0;
        if (!make_alias(v, dcl, name, sn))
            return VFS_ENOSPC;
        nlfn = (UINT32)((len + 12u) / 13u);
    }

    UINT32 total = nlfn + 1u;

    /* найти total свободных записей подряд (можно за концом папки -
       тогда она удлинится) */
    UINT32 start = 0, run = 0;
    UINT32 idx = 0;

    for (;; idx++) {

        UINT8 e[32];

        if (!dir_read_ent(v, dcl, idx, e)) {
            /* кончилась цепочка: дальше - новые (пустые) записи */
            if (run == 0)
                start = idx;
            break;
        }

        if (e[0] == 0x00) {
            if (run == 0)
                start = idx;
            break;                  /* дальше всё свободно */
        }

        if (e[0] == 0xE5) {
            if (run == 0)
                start = idx;
            run++;
            if (run == total)
                break;
            continue;
        }

        run = 0;
    }

    if (dcl == 0 && start + total > v->root_entries)
        return VFS_ENOSPC;          /* корень FAT16 не растёт */

    fat_touch_fsinfo(v);

    UINT8 sum = lfn_sum(sn);

    /* записи длинного имени: последний кусок - первым */
    for (UINT32 k = nlfn; k >= 1; k--) {

        UINT8 e[32];
        static const UINT8 pos[13] = { 1, 3, 5, 7, 9, 14, 16, 18, 20, 22, 24, 28, 30 };

        raw_zero_mem((volatile UINT8 *)e, 32);

        e[0] = (UINT8)(k | (k == nlfn ? 0x40u : 0u));
        e[11] = 0x0F;
        e[13] = sum;

        for (UINTN c = 0; c < 13; c++) {
            UINTN i = (k - 1u) * 13u + c;
            UINT16 ch = (i < len) ? (UINT8)name[i] : (i == len ? 0x0000 : 0xFFFF);
            wr16(e + pos[c], ch);
        }

        if (!dir_write_ent(v, dcl, start + (nlfn - k), e))
            return VFS_EIO;
    }

    UINT8 e[32];

    for (UINTN i = 0; i < 32; i++)
        e[i] = tmpl[i];

    for (UINTN i = 0; i < 11; i++)
        e[i] = sn[i];

    e[12] = flags;

    UINT32 sidx = start + nlfn;

    if (!dir_write_ent(v, dcl, sidx, e))
        return VFS_ENOSPC;

    raw_zero_mem((volatile UINT8 *)out, sizeof(*out));
    out->attr = e[11];
    out->is_dir = (e[11] & 0x10) != 0;
    out->first_cluster = ((v->fat_bits == 32) ? ((UINT32)rd16(e + 20) << 16) : 0) | rd16(e + 26);
    out->size = out->is_dir ? 0 : rd32(e + 28);
    out->parent_cluster = dcl;
    out->entry_index = sidx;
    out->lfn_count = nlfn;
    out->wtime = rd16(e + 22);
    out->wdate = rd16(e + 24);
    out->ram_index = -1;

    return VFS_OK;
}

/* Пометить записи узла удалёнными (короткую и длинное имя перед ней) */
static INTN dir_del(FAT_VOL *v, VFS_NODE *n)
{
    for (UINT32 i = n->entry_index - n->lfn_count; i <= n->entry_index; i++) {

        UINT8 e[32];

        if (!dir_read_ent(v, n->parent_cluster, i, e))
            return VFS_EIO;

        e[0] = 0xE5;

        if (!dir_write_ent(v, n->parent_cluster, i, e))
            return VFS_EIO;
    }

    return VFS_OK;
}

/* Переписать в записи узла кластер, размер и время изменения */
static INTN dir_update(FAT_VOL *v, VFS_NODE *n)
{
    UINT8 e[32];

    if (n->is_root)
        return VFS_OK;

    if (!dir_read_ent(v, n->parent_cluster, n->entry_index, e))
        return VFS_EIO;

    UINT16 d, t;

    fat_now(&d, &t);

    if (v->fat_bits == 32)
        wr16(e + 20, (UINT16)(n->first_cluster >> 16));
    wr16(e + 26, (UINT16)n->first_cluster);
    wr32(e + 28, n->is_dir ? 0 : (UINT32)n->size);
    wr16(e + 22, t);
    wr16(e + 24, d);
    wr16(e + 18, d);                   /* дата доступа */
    if (!n->is_dir)
        e[11] |= 0x20;                 /* "архивный": изменён */

    n->wtime = t;
    n->wdate = d;

    return dir_write_ent(v, n->parent_cluster, n->entry_index, e) ? VFS_OK : VFS_EIO;
}

static void tmpl_new(UINT8 e[32], UINT8 attr, UINT32 cl, UINT32 size, BOOLEAN fat32)
{
    UINT16 d, t;

    fat_now(&d, &t);
    raw_zero_mem((volatile UINT8 *)e, 32);

    e[11] = attr;
    wr16(e + 14, t);           /* создан */
    wr16(e + 16, d);
    wr16(e + 18, d);           /* доступ */
    wr16(e + 20, fat32 ? (UINT16)(cl >> 16) : 0);
    wr16(e + 22, t);           /* изменён */
    wr16(e + 24, d);
    wr16(e + 26, (UINT16)cl);
    wr32(e + 28, size);
}


/* ================================================================
 * Файлы: чтение, запись, обрезка
 * ================================================================ */

/* Кластер номер ci (от начала файла) цепочки first; 0 - нет такого */
static UINT32 chain_nth(FAT_VOL *v, UINT32 first, UINT32 ci)
{
    UINT32 cl = first;

    for (UINT32 i = 0; i < ci; i++) {

        UINT32 next;

        if (!cl_ok(v, cl) || !fat_get(v, cl, &next) || is_eoc(v, next))
            return 0;

        cl = next;
    }

    return cl_ok(v, cl) ? cl : 0;
}

static INTN fat_read(VFS_MOUNT *m, VFS_NODE *f, UINT64 off, VOID *buf, UINTN n)
{
    FAT_VOL *v = VOL(m);
    UINT8 *out = (UINT8 *)buf;
    UINT8 sec[512];

    if (f->is_dir)
        return VFS_EISDIR;

    if (off >= f->size)
        return 0;

    if (n > f->size - off)
        n = (UINTN)(f->size - off);

    UINT32 cb = v->cluster_bytes;
    UINT32 want = (UINT32)(off / cb);
    UINT32 cl;

    /* продолжение прошлого чтения - идти по цепочке от подсказки */
    if (f->hint_cl != 0 && f->hint_ci <= want && cl_ok(v, f->hint_cl))
        cl = chain_nth(v, f->hint_cl, want - f->hint_ci);
    else
        cl = chain_nth(v, f->first_cluster, want);

    UINT32 ci = want;
    UINTN done = 0;

    while (done < n) {

        if (cl == 0)
            return (done > 0) ? (INTN)done : VFS_EIO;

        /* запомнить: этот кластер - номер ci в цепочке */
        f->hint_ci = ci;
        f->hint_cl = cl;

        UINT32 in = (UINT32)(off % cb);
        UINT64 lba = cl_lba(v, cl) + in / 512u;
        UINT32 so = in % 512u;
        UINTN left = n - done;

        if (so == 0 && left >= 512u) {

            /* целыми секторами до конца кластера - одним запросом */
            UINT32 cnt = (UINT32)((cb - in) / 512u);

            if (cnt > left / 512u)
                cnt = (UINT32)(left / 512u);

            if (!blk_read(v->dev, lba, cnt, out + done))
                return VFS_EIO;

            done += (UINTN)cnt * 512u;
            off += (UINT64)cnt * 512u;

        } else {

            UINTN chunk = 512u - so;

            if (chunk > left)
                chunk = left;

            if (!blk_read(v->dev, lba, 1, sec))
                return VFS_EIO;

            for (UINTN i = 0; i < chunk; i++)
                out[done + i] = sec[so + i];

            done += chunk;
            off += chunk;
        }

        if (off % cb == 0 && done < n) {

            UINT32 next;

            if (!fat_get(v, cl, &next) || is_eoc(v, next))
                cl = 0;
            else
                cl = next;
            ci++;
        }
    }

    return (INTN)done;
}

static INTN fat_write(VFS_MOUNT *m, VFS_NODE *f, UINT64 off, const VOID *buf, UINTN n)
{
    /* цепочка может поменяться - подсказку чтения забыть */
    f->hint_cl = 0;

    FAT_VOL *v = VOL(m);
    const UINT8 *in_buf = (const UINT8 *)buf;
    UINT8 sec[512];

    if (f->is_dir)
        return VFS_EISDIR;

    if (n == 0)
        return 0;

    if (off > f->size)
        off = f->size;         /* "дыр" в файлах не делаем */

    if (off + n > 0xFFFFFFFFull)
        return VFS_ENOSPC;     /* FAT: файл не больше 4 ГиБ */

    UINT32 cb = v->cluster_bytes;

    /* первый кластер */
    if (f->first_cluster == 0) {
        UINT32 c = fat_alloc(v, 0, FALSE);
        if (c == 0)
            return VFS_ENOSPC;
        f->first_cluster = c;
    }

    /* дойти до кластера, где начинается запись (удлиняя цепочку) */
    UINT32 cl = f->first_cluster;

    for (UINT32 i = 0; i < (UINT32)(off / cb); i++) {

        UINT32 next;

        if (!fat_get(v, cl, &next))
            return VFS_EIO;

        if (is_eoc(v, next) || !cl_ok(v, next)) {
            next = fat_alloc(v, cl, FALSE);
            if (next == 0)
                return VFS_ENOSPC;
        }

        cl = next;
    }

    UINTN done = 0;
    INTN err = VFS_OK;

    while (done < n) {

        UINT32 in = (UINT32)(off % cb);
        UINT64 lba = cl_lba(v, cl) + in / 512u;
        UINT32 so = in % 512u;
        UINTN left = n - done;

        if (so == 0 && left >= 512u) {

            UINT32 cnt = (UINT32)((cb - in) / 512u);

            if (cnt > left / 512u)
                cnt = (UINT32)(left / 512u);

            if (!blk_write(v->dev, lba, cnt, in_buf + done)) {
                err = VFS_EIO;
                break;
            }

            done += (UINTN)cnt * 512u;
            off += (UINT64)cnt * 512u;

        } else {

            UINTN chunk = 512u - so;

            if (chunk > left)
                chunk = left;

            if (!blk_read(v->dev, lba, 1, sec)) {
                err = VFS_EIO;
                break;
            }

            for (UINTN i = 0; i < chunk; i++)
                sec[so + i] = in_buf[done + i];

            if (!blk_write(v->dev, lba, 1, sec)) {
                err = VFS_EIO;
                break;
            }

            done += chunk;
            off += chunk;
        }

        if (off % cb == 0 && done < n) {

            UINT32 next;

            if (!fat_get(v, cl, &next)) {
                err = VFS_EIO;
                break;
            }

            if (is_eoc(v, next) || !cl_ok(v, next)) {
                next = fat_alloc(v, cl, FALSE);
                if (next == 0) {
                    err = VFS_ENOSPC;
                    break;
                }
            }

            cl = next;
        }
    }

    if (off > f->size)
        f->size = off;

    INTN r = dir_update(v, f);

    if (done == 0)
        return (err != VFS_OK) ? err : r;

    return (INTN)done;
}

static INTN fat_truncate(VFS_MOUNT *m, VFS_NODE *f, UINT64 size)
{
    /* цепочка может поменяться - подсказку чтения забыть */
    f->hint_cl = 0;

    FAT_VOL *v = VOL(m);

    if (f->is_dir)
        return VFS_EISDIR;

    if (size >= f->size)
        return VFS_OK;          /* удлинять не умеем (и не нужно) */

    UINT32 cb = v->cluster_bytes;

    if (size == 0) {

        if (f->first_cluster != 0)
            fat_free_chain(v, f->first_cluster);

        f->first_cluster = 0;

    } else {

        UINT32 keep = (UINT32)((size + cb - 1u) / cb);
        UINT32 last = chain_nth(v, f->first_cluster, keep - 1u);
        UINT32 next;

        if (last != 0 && fat_get(v, last, &next) && !is_eoc(v, next) && cl_ok(v, next)) {
            fat_set(v, last, eoc_mark(v));
            fat_free_chain(v, next);
        }
    }

    f->size = size;

    return dir_update(v, f);
}


/* ================================================================
 * Создание, удаление, переименование
 * ================================================================ */

static INTN fat_create(VFS_MOUNT *m, VFS_NODE *dir, const char *name, BOOLEAN is_dir,
                       VFS_NODE *out)
{
    FAT_VOL *v = VOL(m);
    UINT32 dcl = node_dir_cluster(v, dir);
    UINT8 t[32];

    if (!is_dir) {
        tmpl_new(t, 0x20, 0, 0, v->fat_bits == 32);
        return dir_add(v, dcl, name, t, out);
    }

    /* папка: свой кластер с записями "." и ".." */
    UINT32 cl = fat_alloc(v, 0, TRUE);

    if (cl == 0)
        return VFS_ENOSPC;

    UINT8 e[32];

    tmpl_new(e, 0x10, cl, 0, v->fat_bits == 32);
    for (UINTN i = 0; i < 11; i++) e[i] = ' ';
    e[0] = '.';

    if (!dir_write_ent(v, cl, 0, e)) {
        fat_free_chain(v, cl);
        return VFS_EIO;
    }

    /* ".." на корень - всегда кластер 0 (так требует спецификация,
       даже у FAT32) */
    UINT32 pcl = dir->is_root ? 0 : dir->first_cluster;

    tmpl_new(e, 0x10, pcl, 0, v->fat_bits == 32);
    for (UINTN i = 0; i < 11; i++) e[i] = ' ';
    e[0] = '.';
    e[1] = '.';

    if (!dir_write_ent(v, cl, 1, e)) {
        fat_free_chain(v, cl);
        return VFS_EIO;
    }

    tmpl_new(t, 0x10, cl, 0, v->fat_bits == 32);

    INTN r = dir_add(v, dcl, name, t, out);

    if (r != VFS_OK)
        fat_free_chain(v, cl);

    return r;
}

static INTN fat_remove(VFS_MOUNT *m, VFS_NODE *dir, VFS_NODE *node)
{
    FAT_VOL *v = VOL(m);

    (void)dir;

    if (node->is_root)
        return VFS_EINVAL;

    if (node->is_dir) {

        VFS_DIRENT *de = (VFS_DIRENT *)kmalloc(sizeof(VFS_DIRENT));
        UINT64 cookie = 0;

        if (de == NULL)
            return VFS_EIO;

        INTN r = fat_readdir(m, node, &cookie, de);

        kfree(de);

        if (r > 0)
            return VFS_ENOTEMPTY;
        if (r < 0)
            return r;
    }

    INTN r = dir_del(v, node);

    if (r != VFS_OK)
        return r;

    if (node->first_cluster != 0)
        fat_free_chain(v, node->first_cluster);

    return VFS_OK;
}

static INTN fat_rename(VFS_MOUNT *m, VFS_NODE *dir, VFS_NODE *node,
                       VFS_NODE *newdir, const char *newname)
{
    FAT_VOL *v = VOL(m);
    UINT8 e[32];

    (void)dir;

    if (node->is_root)
        return VFS_EINVAL;

    /* новая запись - копия старой (время, атрибуты, кластер, размер) */
    if (!dir_read_ent(v, node->parent_cluster, node->entry_index, e))
        return VFS_EIO;

    VFS_NODE nn;
    UINT32 ndcl = node_dir_cluster(v, newdir);
    INTN r = dir_add(v, ndcl, newname, e, &nn);

    if (r != VFS_OK)
        return r;

    /* dir_add мог занять место ДО старой записи в той же папке -
       старые номера записей от этого не меняются */
    r = dir_del(v, node);

    if (r != VFS_OK)
        return r;

    /* папку перенесли в другую папку: поправить её ".." */
    if (node->is_dir && node->first_cluster != 0 && ndcl != node->parent_cluster) {

        UINT8 dd[32];

        if (dir_read_ent(v, node->first_cluster, 1, dd) && dd[0] == '.' && dd[1] == '.') {
            UINT32 pcl = newdir->is_root ? 0 : newdir->first_cluster;
            if (v->fat_bits == 32)
                wr16(dd + 20, (UINT16)(pcl >> 16));
            wr16(dd + 26, (UINT16)pcl);
            dir_write_ent(v, node->first_cluster, 1, dd);
        }
    }

    *node = nn;

    return VFS_OK;
}

static INTN fat_statfs(VFS_MOUNT *m, UINT64 *total, UINT64 *free_bytes)
{
    FAT_VOL *v = VOL(m);

    *total = (UINT64)v->clusters * v->cluster_bytes;

    if (v->free_count < 0) {

        /* посчитать один раз (потом ведём счёт сами); таблицу
           читаем кусками по 64 сектора - на большой флешке это
           мегабайты, по сектору было бы очень долго */
        UINT32 chunk = 64;
        UINT8 *b = (UINT8 *)kmalloc(chunk * 512u);
        UINT32 epb = 512u / (v->fat_bits / 8u);          /* записей в секторе */
        UINT32 need_secs = (v->clusters + 2u + epb - 1u) / epb;
        UINT64 cnt = 0;

        if (b == NULL)
            return VFS_EIO;

        for (UINT32 s0 = 0; s0 < need_secs; s0 += chunk) {

            UINT32 n = (need_secs - s0 < chunk) ? need_secs - s0 : chunk;

            if (!blk_read(v->dev, (UINT64)v->reserved + s0, n, b)) {
                kfree(b);
                return VFS_EIO;
            }

            for (UINT32 k = 0; k < n * epb; k++) {

                UINT32 cl = s0 * epb + k;

                if (cl < 2 || cl >= v->clusters + 2u)
                    continue;

                UINT32 val = (v->fat_bits == 16) ? rd16(b + k * 2u)
                                                 : (rd32(b + k * 4u) & 0x0FFFFFFFu);
                if (val == 0)
                    cnt++;
            }
        }

        kfree(b);
        v->free_count = (INT64)cnt;
    }

    *free_bytes = (UINT64)v->free_count * v->cluster_bytes;

    return VFS_OK;
}

const VFS_OPS g_fat_ops = {
    "FAT",
    fat_root,
    fat_readdir,
    fat_lookup,
    fat_read,
    fat_write,
    fat_truncate,
    fat_create,
    fat_remove,
    fat_rename,
    fat_statfs,
    NULL,                         /* close: нечего дописывать */
    NULL                          /* open: считать не нужно */
};
