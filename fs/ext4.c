/*
 * fs/ext4.c - тома ext4 (ext2/ext3 тоже), ТОЛЬКО ЧТЕНИЕ (этап 11).
 * Часть MyOS; общие объявления - в myos.h.
 *
 * Зачем: на ноутбуке рядом с MyOS стоит Arch Linux, и его корень -
 * раздел ext4. Чтобы запускать программы Linux (а потом и Firefox),
 * MyOS должна читать их файлы: /usr/bin, /usr/lib, /etc. Писать на
 * этот раздел MyOS не будет НИКОГДА - это чужая система с данными
 * пользователя; поэтому драйвер умеет только читать, а VFS считает
 * том read-only.
 *
 * Как устроен ext4 (коротко):
 *   * диск делится на блоки (1, 2 или 4 КиБ); блоки собраны в
 *     группы; у каждой группы - описатель (где её таблица inode);
 *   * суперблок (байт 1024 раздела) - размеры и "фичи" тома;
 *   * файл = inode (номер, тип и права, размер, время, где данные) +
 *     записи в папках "имя -> номер inode";
 *   * где лежат данные: у ext4 - дерево "экстентов" (кусок файла ->
 *     кусок диска подряд), у старых ext2/3 - прямые и косвенные
 *     номера блоков. Оба способа поддержаны;
 *   * папка - файл из записей переменной длины; у больших папок
 *     поверх этого есть индекс (htree), но записи остаются обычными,
 *     и их можно просто читать подряд - так мы и делаем;
 *   * символьная ссылка: короткая (< 60 байт) - прямо в inode,
 *     длинная - в блоке данных.
 *
 * Скорость (Firefox открывает тысячи файлов):
 *   * кэш блоков: 4096 последних прочитанных блоков (до 16 МиБ) -
 *     описатели групп, таблицы inode, папки, узлы дерева экстентов;
 *     большие куски файлов читаются мимо кэша прямо в буфер;
 *   * кэш имён: когда папку прочитали целиком, все её записи
 *     запоминаются ("имя -> inode"), и следующий поиск в ней (даже
 *     неудачный - ld.so ищет библиотеки по многим папкам) - без диска.
 *     Том не меняется (мы не пишем), поэтому кэш никогда не устаревает.
 *
 * Не поддержано (сообщаем и не показываем такие файлы): шифрованные
 * папки (fscrypt), папки с inline_data. Журнал не "проигрывается":
 * если Arch выключили некорректно (флаг RECOVER), последние изменения
 * могут быть не видны - об этом пишем в журнал ядра.
 *
 * Все вызовы - под g_vfs_mutex (так зовёт VFS).
 */
#include "myos.h"

/* ---------------- раскладка на диске ---------------- */

#define E4_MAGIC            0xEF53u

/* s_feature_incompat */
#define E4_IN_FILETYPE      0x0002u
#define E4_IN_RECOVER       0x0004u
#define E4_IN_JOURNAL_DEV   0x0008u
#define E4_IN_META_BG       0x0010u
#define E4_IN_EXTENTS       0x0040u
#define E4_IN_64BIT         0x0080u
#define E4_IN_MMP           0x0100u
#define E4_IN_FLEX_BG       0x0200u
#define E4_IN_EA_INODE      0x0400u
#define E4_IN_DIRDATA       0x1000u
#define E4_IN_CSUM_SEED     0x2000u
#define E4_IN_LARGEDIR      0x4000u
#define E4_IN_INLINE_DATA   0x8000u
#define E4_IN_ENCRYPT       0x10000u
#define E4_IN_CASEFOLD      0x20000u

/* то, что мы умеем читать (остальное - отказ монтировать: вдруг там
   раскладка, которую мы поймём неправильно и покажем мусор) */
#define E4_IN_KNOWN (E4_IN_FILETYPE | E4_IN_RECOVER | E4_IN_META_BG | E4_IN_EXTENTS | \
                     E4_IN_64BIT | E4_IN_MMP | E4_IN_FLEX_BG | E4_IN_EA_INODE |       \
                     E4_IN_CSUM_SEED | E4_IN_LARGEDIR | E4_IN_INLINE_DATA |           \
                     E4_IN_ENCRYPT | E4_IN_CASEFOLD)

/* s_feature_ro_compat */
#define E4_RO_SPARSE_SUPER  0x0001u

/* s_feature_compat */
#define E4_CO_SPARSE_SUPER2 0x0200u

/* i_flags */
#define E4_FL_INDEX         0x00001000u      /* папка с htree (читаем подряд) */
#define E4_FL_EXTENTS       0x00080000u
#define E4_FL_ENCRYPT       0x00000800u
#define E4_FL_INLINE        0x10000000u

#define E4_EXT_MAGIC        0xF30Au
#define E4_ROOT_INO         2u

static UINT16 rd16(const UINT8 *p) { return (UINT16)(p[0] | (p[1] << 8)); }
static UINT32 rd32(const UINT8 *p)
{
    return (UINT32)p[0] | ((UINT32)p[1] << 8) | ((UINT32)p[2] << 16) | ((UINT32)p[3] << 24);
}

/* Том */
typedef struct {
    UINTN   dev;
    UINT32  bs;                  /* размер блока */
    UINT32  bs_shift;            /* log2(bs) */
    UINT32  spb;                 /* секторов по 512 в блоке */
    UINT64  blocks;
    UINT64  free_blocks;
    UINT32  inodes_per_group;
    UINT32  blocks_per_group;
    UINT32  first_data_block;
    UINT32  inode_size;
    UINT32  desc_size;
    UINT32  groups;
    UINT32  incompat, ro_compat, compat;
    UINT32  first_meta_bg;
    UINT32  backup_bgs[2];       /* sparse_super2: где копии суперблока */
    UINT8   uuid[16];
    char    label[17];
    UINT8  *gdt;                 /* все описатели групп подряд (desc_size каждый) */
} E4VOL;

/* Inode в памяти - то, что нам нужно из 128+ байт на диске */
typedef struct {
    UINT32 ino;
    UINT32 mode;
    UINT32 uid, gid;
    UINT64 size;
    UINT32 flags;
    UINT32 nlink;
    INT64  atime, ctime, mtime;
    UINT8  block[60];            /* i_block: экстенты / номера блоков / ссылка */
    UINT8  inline_extra[256];    /* inline_data: продолжение из xattr "system.data" */
    UINT32 inline_extra_len;
} E4INODE;


/* ================================================================
 * Кэш блоков
 * ================================================================ */

#define E4_CACHE_N      4096u            /* блоков (по странице на блок) */
#define E4_CACHE_HASH   8192u

typedef struct {
    E4VOL  *vol;                 /* NULL - слот свободен */
    UINT64  blk;
    UINT8  *data;                /* страница (выделяется при первом использовании) */
    INT32   hnext;               /* следующий в цепочке хэша */
    BOOLEAN ref;                 /* "недавно трогали" (алгоритм часов) */
} E4CBLK;

static E4CBLK *g_e4c;            /* E4_CACHE_N слотов */
static INT32   g_e4h[E4_CACHE_HASH];
static UINT32  g_e4_hand;
static UINT64  g_e4_hits, g_e4_misses;

static UINT32 cache_hash(const E4VOL *v, UINT64 blk)
{
    UINT64 h = (UINT64)(UINTN)v * 0x9E3779B97F4A7C15ull ^ blk * 0xC2B2AE3D27D4EB4Full;
    return (UINT32)(h >> 40) & (E4_CACHE_HASH - 1u);
}

static BOOLEAN cache_init(void)
{
    if (g_e4c != NULL)
        return TRUE;

    g_e4c = (E4CBLK *)kzalloc(sizeof(E4CBLK) * E4_CACHE_N);

    if (g_e4c == NULL)
        return FALSE;

    for (UINTN i = 0; i < E4_CACHE_HASH; i++)
        g_e4h[i] = -1;

    return TRUE;
}

/* Убрать слот из цепочки хэша */
static void cache_unhash(UINT32 slot)
{
    E4CBLK *c = &g_e4c[slot];
    INT32 *pp = &g_e4h[cache_hash(c->vol, c->blk)];

    while (*pp >= 0 && *pp != (INT32)slot)
        pp = &g_e4c[*pp].hnext;

    if (*pp == (INT32)slot)
        *pp = c->hnext;

    c->vol = NULL;
    c->hnext = -1;
}

/* Блок тома в памяти (из кэша или с диска). NULL - ошибка чтения.
   Указатель живёт до следующего вызова blk_get (кэш может вытеснить
   блок) - поэтому вызывающий сразу берёт из него, что нужно. */
static const UINT8 *blk_get(E4VOL *v, UINT64 blk)
{
    if (blk >= v->blocks)
        return NULL;

    UINT32 h = cache_hash(v, blk);

    for (INT32 i = g_e4h[h]; i >= 0; i = g_e4c[i].hnext)
        if (g_e4c[i].vol == v && g_e4c[i].blk == blk) {
            g_e4c[i].ref = TRUE;
            g_e4_hits++;
            return g_e4c[i].data;
        }

    g_e4_misses++;

    /* жертва - по кругу; "недавно трогали" получают второй шанс */
    UINT32 slot;

    for (;;) {
        slot = g_e4_hand;
        g_e4_hand = (g_e4_hand + 1u) % E4_CACHE_N;
        if (g_e4c[slot].ref)
            g_e4c[slot].ref = FALSE;
        else
            break;
    }

    E4CBLK *c = &g_e4c[slot];

    if (c->vol != NULL)
        cache_unhash(slot);

    if (c->data == NULL) {
        UINT64 phys = pmm_alloc_pages(1, 0);
        if (phys == 0)
            return NULL;
        c->data = (UINT8 *)P2V(phys);
    }

    if (!blk_read(v->dev, blk * v->spb, v->spb, c->data))
        return NULL;

    c->vol = v;
    c->blk = blk;
    c->ref = TRUE;
    c->hnext = g_e4h[h];
    g_e4h[h] = (INT32)slot;

    return c->data;
}

/* Том уходит: его блоки - из кэша */
static void cache_drop_vol(const E4VOL *v)
{
    if (g_e4c == NULL)
        return;

    for (UINT32 i = 0; i < E4_CACHE_N; i++)
        if (g_e4c[i].vol == v)
            cache_unhash(i);
}


/* ================================================================
 * Кэш имён: (том, папка, имя) -> inode
 * ================================================================ */

typedef struct E4DENT {
    struct E4DENT *next;
    const E4VOL   *vol;
    UINT32         dir;
    UINT32         ino;
    UINT32         hash;
    char           name[];       /* с нулём в конце */
} E4DENT;

/* папки, прочитанные целиком: промах в кэше = "такого имени нет" */
typedef struct E4DFULL {
    struct E4DFULL *next;
    const E4VOL    *vol;
    UINT32          dir;
} E4DFULL;

#define E4_DHASH        16384u
#define E4_FHASH        2048u
#define E4_DENT_MAX     400000u          /* больше - кэш сбрасывается целиком */

static E4DENT  *g_dh[E4_DHASH];
static E4DFULL *g_fh[E4_FHASH];
static UINTN    g_dents;

static UINT32 name_hash(const E4VOL *v, UINT32 dir, const char *name, UINTN len)
{
    UINT32 h = 2166136261u ^ (UINT32)(UINTN)v ^ (dir * 0x9E3779B1u);

    for (UINTN i = 0; i < len; i++)
        h = (h ^ (UINT8)name[i]) * 16777619u;

    return h;
}

static void dcache_clear(const E4VOL *only)
{
    for (UINTN i = 0; i < E4_DHASH; i++) {
        E4DENT **pp = &g_dh[i];
        while (*pp != NULL) {
            E4DENT *d = *pp;
            if (only == NULL || d->vol == only) {
                *pp = d->next;
                kfree(d);
                g_dents--;
            } else {
                pp = &d->next;
            }
        }
    }

    for (UINTN i = 0; i < E4_FHASH; i++) {
        E4DFULL **pp = &g_fh[i];
        while (*pp != NULL) {
            E4DFULL *f = *pp;
            if (only == NULL || f->vol == only) {
                *pp = f->next;
                kfree(f);
            } else {
                pp = &f->next;
            }
        }
    }
}

static BOOLEAN dcache_full(const E4VOL *v, UINT32 dir)
{
    for (E4DFULL *f = g_fh[(dir * 2654435761u) % E4_FHASH]; f != NULL; f = f->next)
        if (f->vol == v && f->dir == dir)
            return TRUE;

    return FALSE;
}

static void dcache_mark_full(const E4VOL *v, UINT32 dir)
{
    E4DFULL *f = (E4DFULL *)kmalloc(sizeof(E4DFULL));

    if (f == NULL)
        return;

    UINT32 h = (dir * 2654435761u) % E4_FHASH;

    f->vol = v;
    f->dir = dir;
    f->next = g_fh[h];
    g_fh[h] = f;
}

/* 0 - нет в кэше */
static UINT32 dcache_find(const E4VOL *v, UINT32 dir, const char *name, UINTN len)
{
    UINT32 h = name_hash(v, dir, name, len);

    for (E4DENT *d = g_dh[h % E4_DHASH]; d != NULL; d = d->next) {
        if (d->hash != h || d->vol != v || d->dir != dir)
            continue;
        UINTN k = 0;
        while (k < len && d->name[k] == name[k])
            k++;
        if (k == len && d->name[k] == '\0')
            return d->ino;
    }

    return 0;
}

static void dcache_add(const E4VOL *v, UINT32 dir, const char *name, UINTN len, UINT32 ino)
{
    if (g_dents >= E4_DENT_MAX)
        dcache_clear(NULL);

    E4DENT *d = (E4DENT *)kmalloc(sizeof(E4DENT) + len + 1u);

    if (d == NULL)
        return;

    UINT32 h = name_hash(v, dir, name, len);

    d->vol = v;
    d->dir = dir;
    d->ino = ino;
    d->hash = h;
    memcpy(d->name, name, len);
    d->name[len] = '\0';
    d->next = g_dh[h % E4_DHASH];
    g_dh[h % E4_DHASH] = d;
    g_dents++;
}


/* ================================================================
 * Группы и inode
 * ================================================================ */

static BOOLEAN is_power_of(UINT32 n, UINT32 b)
{
    while (n > 1 && n % b == 0)
        n /= b;

    return n == 1;
}

/* Есть ли в группе копия суперблока (и, значит, сдвиг на блок) */
static BOOLEAN group_has_super(const E4VOL *v, UINT32 g)
{
    if (g == 0)
        return TRUE;

    if (v->compat & E4_CO_SPARSE_SUPER2)
        return g == v->backup_bgs[0] || g == v->backup_bgs[1];

    if (!(v->ro_compat & E4_RO_SPARSE_SUPER))
        return TRUE;

    return g == 1 || is_power_of(g, 3) || is_power_of(g, 5) || is_power_of(g, 7);
}

/* Где на диске блок описателей номер b (с учётом META_BG) */
static UINT64 gdt_block(const E4VOL *v, UINT32 b)
{
    UINT32 per = v->bs / v->desc_size;

    if (!(v->incompat & E4_IN_META_BG) || b < v->first_meta_bg)
        return (UINT64)v->first_data_block + 1u + b;

    UINT32 g0 = b * per;

    return (UINT64)v->first_data_block + (UINT64)g0 * v->blocks_per_group +
           (group_has_super(v, g0) ? 1u : 0u);
}

/* Блок начала таблицы inode группы g */
static UINT64 group_inode_table(const E4VOL *v, UINT32 g)
{
    const UINT8 *d = v->gdt + (UINTN)g * v->desc_size;
    UINT64 t = rd32(d + 0x08);

    if (v->desc_size >= 64)
        t |= (UINT64)rd32(d + 0x28) << 32;

    return t;
}

static BOOLEAN inode_read(E4VOL *v, UINT32 ino, E4INODE *out)
{
    if (ino == 0 || v->inodes_per_group == 0)
        return FALSE;

    UINT32 g = (ino - 1u) / v->inodes_per_group;
    UINT32 idx = (ino - 1u) % v->inodes_per_group;

    if (g >= v->groups)
        return FALSE;

    UINT64 off = (UINT64)idx * v->inode_size;
    UINT64 blk = group_inode_table(v, g) + (off >> v->bs_shift);
    const UINT8 *b = blk_get(v, blk);

    if (b == NULL)
        return FALSE;

    const UINT8 *in = b + (off & (v->bs - 1u));

    memset(out, 0, sizeof(*out));
    out->ino = ino;
    out->mode = rd16(in + 0x00);
    out->uid = rd16(in + 0x02) | ((UINT32)rd16(in + 0x78) << 16);
    out->gid = rd16(in + 0x18) | ((UINT32)rd16(in + 0x7A) << 16);
    out->size = rd32(in + 0x04) | ((UINT64)rd32(in + 0x6C) << 32);
    out->atime = (INT64)(INT32)rd32(in + 0x08);
    out->ctime = (INT64)(INT32)rd32(in + 0x0C);
    out->mtime = (INT64)(INT32)rd32(in + 0x10);
    out->nlink = rd16(in + 0x1A);
    out->flags = rd32(in + 0x20);
    memcpy(out->block, in + 0x28, 60);

    /* inline_data: продолжение данных - в xattr "system.data" внутри
       inode (за i_extra_isize). Разбираем только его. */
    if ((out->flags & E4_FL_INLINE) && v->inode_size > 128) {

        UINT32 extra = rd16(in + 0x80);
        UINT32 at = 128u + extra;

        if (at + 4u <= v->inode_size && rd32(in + at) == 0xEA020000u) {

            UINT32 first = at + 4u, e = first;

            while (e + 16u <= v->inode_size && rd32(in + e) != 0) {

                UINT32 nlen = in[e];
                UINT32 idx_ = in[e + 1];
                UINT32 voff = rd16(in + e + 2);
                UINT32 vsize = rd32(in + e + 8);

                if (idx_ == 7 && nlen == 4 && e + 16u + 4u <= v->inode_size &&
                    in[e + 16] == 'd' && in[e + 17] == 'a' && in[e + 18] == 't' && in[e + 19] == 'a') {
                    if (vsize > sizeof(out->inline_extra))
                        vsize = sizeof(out->inline_extra);
                    if (first + voff + vsize <= v->inode_size) {
                        memcpy(out->inline_extra, in + first + voff, vsize);
                        out->inline_extra_len = vsize;
                    }
                    break;
                }

                e += (16u + nlen + 3u) & ~3u;
            }
        }
    }

    return TRUE;
}


/* ================================================================
 * Где лежат данные файла
 * ================================================================ */

/*
 * Логический блок lblk файла -> физический блок *pblk и сколько
 * блоков подряд (*run) идут так же. *pblk = 0 - "дыра" (нули) на *run
 * блоков. FALSE - ошибка (испорченное дерево, диск).
 */
static BOOLEAN map_extent(E4VOL *v, const E4INODE *ip, UINT64 lblk, UINT64 *pblk, UINT64 *run)
{
    const UINT8 *node = ip->block;
    UINTN depth_guard = 0;

    for (;;) {

        if (rd16(node) != E4_EXT_MAGIC)
            return FALSE;

        UINT32 n = rd16(node + 2);
        UINT32 depth = rd16(node + 6);
        const UINT8 *e = node + 12;

        if (depth == 0) {

            /* лист: последний экстент с началом <= lblk */
            UINT64 next_start = ~0ull;

            for (UINT32 i = 0; i < n; i++) {

                const UINT8 *x = e + i * 12u;
                UINT64 s = rd32(x);
                UINT32 len = rd16(x + 4);
                BOOLEAN uninit = len > 32768u;

                if (uninit)
                    len -= 32768u;

                if (lblk >= s && lblk < s + len) {
                    UINT64 start = ((UINT64)rd16(x + 6) << 32) | rd32(x + 8);
                    *run = s + len - lblk;
                    /* "не записанный" экстент: место выделено, читается как нули */
                    *pblk = uninit ? 0 : start + (lblk - s);
                    return TRUE;
                }

                if (s > lblk && s < next_start)
                    next_start = s;
            }

            *pblk = 0;
            *run = (next_start == ~0ull) ? 0x100000u : next_start - lblk;
            return TRUE;
        }

        /* внутренний узел: последний индекс с началом <= lblk */
        INT32 pick = -1;

        for (UINT32 i = 0; i < n; i++) {
            if (rd32(e + i * 12u) <= lblk)
                pick = (INT32)i;
            else
                break;
        }

        if (pick < 0) {
            /* до первого индекса - дыра до его начала */
            *pblk = 0;
            *run = (n > 0) ? rd32(e) - lblk : 0x100000u;
            return TRUE;
        }

        const UINT8 *x = e + (UINT32)pick * 12u;
        UINT64 child = ((UINT64)rd16(x + 8) << 32) | rd32(x + 4);
        const UINT8 *b = blk_get(v, child);

        if (b == NULL || ++depth_guard > 8)
            return FALSE;

        /* старый узел больше не нужен: следующий blk_get (который может
           вытеснить блок из кэша) будет уже после разбора этого */
        node = b;
    }
}

/* Старый способ (ext2/3): 12 прямых номеров, потом косвенные */
static BOOLEAN map_indirect(E4VOL *v, const E4INODE *ip, UINT64 lblk, UINT64 *pblk, UINT64 *run)
{
    UINT64 per = v->bs / 4u;
    UINT32 idx[4];
    UINTN levels;
    UINT32 start;

    *run = 1;

    if (lblk < 12) {
        *pblk = rd32(ip->block + lblk * 4u);
        return TRUE;
    }

    lblk -= 12;

    if (lblk < per) {
        start = rd32(ip->block + 12 * 4);
        levels = 1;
        idx[0] = (UINT32)lblk;
    } else if ((lblk -= per) < per * per) {
        start = rd32(ip->block + 13 * 4);
        levels = 2;
        idx[0] = (UINT32)(lblk / per);
        idx[1] = (UINT32)(lblk % per);
    } else {
        lblk -= per * per;
        start = rd32(ip->block + 14 * 4);
        levels = 3;
        idx[0] = (UINT32)(lblk / (per * per));
        idx[1] = (UINT32)((lblk / per) % per);
        idx[2] = (UINT32)(lblk % per);
    }

    UINT64 cur = start;

    for (UINTN l = 0; l < levels; l++) {
        if (cur == 0)
            break;                   /* дыра */
        const UINT8 *b = blk_get(v, cur);
        if (b == NULL)
            return FALSE;
        cur = rd32(b + (UINTN)idx[l] * 4u);
    }

    *pblk = cur;
    return TRUE;
}

static BOOLEAN map_block(E4VOL *v, const E4INODE *ip, UINT64 lblk, UINT64 *pblk, UINT64 *run)
{
    if (ip->flags & E4_FL_EXTENTS)
        return map_extent(v, ip, lblk, pblk, run);

    return map_indirect(v, ip, lblk, pblk, run);
}

/* inline_data: всё содержимое файла - в inode */
static INTN read_inline(const E4INODE *ip, UINT64 off, UINT8 *buf, UINTN n)
{
    UINTN done = 0;

    while (done < n && off + done < ip->size) {
        UINT64 pos = off + done;
        if (pos < 60)
            buf[done] = ip->block[pos];
        else if (pos - 60 < ip->inline_extra_len)
            buf[done] = ip->inline_extra[pos - 60];
        else
            buf[done] = 0;
        done++;
    }

    return (INTN)done;
}

/* Прочитать кусок файла (inode уже в памяти) */
static INTN inode_pread(E4VOL *v, const E4INODE *ip, UINT64 off, VOID *buf, UINTN n)
{
    UINT8 *out = (UINT8 *)buf;
    UINTN done = 0;

    if (off >= ip->size)
        return 0;

    if (n > ip->size - off)
        n = (UINTN)(ip->size - off);

    if (ip->flags & E4_FL_INLINE)
        return read_inline(ip, off, out, n);

    while (done < n) {

        UINT64 pos = off + done;
        UINT64 lblk = pos >> v->bs_shift;
        UINT32 within = (UINT32)(pos & (v->bs - 1u));
        UINT64 pblk = 0, run = 0;

        if (!map_block(v, ip, lblk, &pblk, &run) || run == 0)
            return done ? (INTN)done : VFS_EIO;

        UINT64 span = run * v->bs - within;     /* байт до конца куска */
        UINTN want = n - done;

        if ((UINT64)want > span)
            want = (UINTN)span;

        if (pblk == 0) {
            memset(out + done, 0, want);        /* дыра */
        } else if (within == 0 && want >= v->bs) {
            /* целые блоки подряд - прямо в буфер, мимо кэша (не
               вымываем им папки и таблицы inode) */
            UINT64 nblk = want >> v->bs_shift;
            if (nblk > 256)
                nblk = 256;                     /* до 1 МиБ за раз */
            want = (UINTN)(nblk << v->bs_shift);
            if (!blk_read(v->dev, pblk * v->spb, (UINT32)(nblk * v->spb), out + done))
                return done ? (INTN)done : VFS_EIO;
        } else {
            const UINT8 *b = blk_get(v, pblk);
            if (b == NULL)
                return done ? (INTN)done : VFS_EIO;
            if (want > v->bs - within)
                want = v->bs - within;
            memcpy(out + done, b + within, want);
        }

        done += want;
    }

    return (INTN)done;
}


/* ================================================================
 * Узлы VFS
 * ================================================================ */

/* Секунды с 1970 -> дата и время FAT (их показывает ls шелла MyOS) */
static void unix_to_fat(INT64 t, UINT16 *d, UINT16 *tm)
{
    if (t < 315532800) {                         /* до 1980 года FAT не умеет */
        *d = 0;
        *tm = 0;
        return;
    }

    UINT64 days = (UINT64)t / 86400u, secs = (UINT64)t % 86400u;
    /* дни -> год, месяц, день (алгоритм Хиннанта) */
    INT64 z = (INT64)days + 719468;
    INT64 era = z / 146097;
    UINT64 doe = (UINT64)(z - era * 146097);
    UINT64 yoe = (doe - doe / 1460 + doe / 36524 - doe / 146096) / 365;
    INT64 y = (INT64)yoe + era * 400;
    UINT64 doy = doe - (365 * yoe + yoe / 4 - yoe / 100);
    UINT64 mp = (5 * doy + 2) / 153;
    UINT32 day = (UINT32)(doy - (153 * mp + 2) / 5 + 1);
    UINT32 mon = (UINT32)(mp < 10 ? mp + 3 : mp - 9);

    if (mon <= 2)
        y++;

    if (y > 2107) {
        *d = 0;
        *tm = 0;
        return;
    }

    *d = (UINT16)(((UINT32)(y - 1980) << 9) | (mon << 5) | day);
    *tm = (UINT16)(((secs / 3600u) << 11) | (((secs / 60u) % 60u) << 5) | ((secs % 60u) / 2u));
}

static void fill_node(const E4INODE *ip, VFS_NODE *out)
{
    raw_zero_mem((volatile UINT8 *)out, sizeof(*out));

    out->ino = ip->ino;
    out->mode = ip->mode;
    out->is_dir = (ip->mode & VFS_MODE_FMT) == VFS_MODE_DIR;
    out->is_root = (ip->ino == E4_ROOT_INO);
    out->size = ip->size;
    out->uid = ip->uid;
    out->gid = ip->gid;
    out->nlink = ip->nlink;
    out->mtime = ip->mtime;
    out->atime = ip->atime;
    out->ctime = ip->ctime;

    /* устройство (в образах бывают /dev/null и т.п.): старая запись -
       в i_block[0], новая - в i_block[1] */
    UINT32 fmt = ip->mode & VFS_MODE_FMT;

    if (fmt == 0020000u || fmt == 0060000u) {
        UINT32 b0 = rd32(ip->block), b1 = rd32(ip->block + 4);
        if (b0 != 0)
            out->rdev = b0 & 0xFFFFu;
        else
            out->rdev = ((b1 >> 8) & 0xFFFu) << 8 | (b1 & 0xFFu);
    }

    unix_to_fat(ip->mtime, &out->wdate, &out->wtime);
}

static E4VOL *vol_of(VFS_MOUNT *m)
{
    return (E4VOL *)m->e4;
}

static INTN e_root(VFS_MOUNT *m, VFS_NODE *out)
{
    E4INODE ip;

    if (!inode_read(vol_of(m), E4_ROOT_INO, &ip))
        return VFS_EIO;

    fill_node(&ip, out);
    out->is_root = TRUE;
    return VFS_OK;
}

/*
 * Пройти записи папки начиная с байта *pos: следующая живая запись ->
 * имя, его длина, inode, тип. 1 - есть, 0 - конец, <0 - ошибка.
 * Имя указывает во временный буфер name (cap байт).
 */
static INTN dir_next(E4VOL *v, const E4INODE *dp, UINT64 *pos, char *name, UINTN cap,
                     UINTN *len, UINT32 *ino)
{
    while (*pos < dp->size) {

        UINT64 lblk = *pos >> v->bs_shift;
        UINT32 within = (UINT32)(*pos & (v->bs - 1u));
        UINT64 pblk = 0, run = 0;

        if (!map_block(v, dp, lblk, &pblk, &run))
            return VFS_EIO;

        if (pblk == 0) {
            *pos = (lblk + 1u) << v->bs_shift;  /* дыра в папке - пропускаем блок */
            continue;
        }

        const UINT8 *b = blk_get(v, pblk);

        if (b == NULL)
            return VFS_EIO;

        const UINT8 *e = b + within;
        UINT32 eino = rd32(e);
        UINT32 rec = rd16(e + 4);
        UINT32 nlen = e[6];

        /* испорченная запись - к следующему блоку */
        if (rec < 8 || within + rec > v->bs || 8u + nlen > rec) {
            *pos = (lblk + 1u) << v->bs_shift;
            continue;
        }

        *pos += rec;

        if (eino == 0 || nlen == 0)
            continue;                           /* пустая запись / хвост с контрольной суммой */

        if (nlen + 1u > cap)
            continue;

        memcpy(name, e + 8, nlen);
        name[nlen] = '\0';
        *len = nlen;
        *ino = eino;
        return 1;
    }

    return 0;
}

static BOOLEAN is_dot(const char *n, UINTN len)
{
    return (len == 1 && n[0] == '.') || (len == 2 && n[0] == '.' && n[1] == '.');
}

static INTN e_readdir(VFS_MOUNT *m, VFS_NODE *dir, UINT64 *cookie, VFS_DIRENT *out)
{
    E4VOL *v = vol_of(m);
    E4INODE dp, ip;

    if (!inode_read(v, dir->ino, &dp))
        return VFS_EIO;

    if (dp.flags & E4_FL_INLINE)
        return 0;                               /* папки inline_data - не поддержаны */

    for (;;) {

        UINTN len = 0;
        UINT32 ino = 0;
        INTN r = dir_next(v, &dp, cookie, out->name, sizeof(out->name), &len, &ino);

        if (r <= 0)
            return r;

        if (is_dot(out->name, len))
            continue;                           /* "." и ".." добавляет VFS/слой Linux */

        if (!inode_read(v, ino, &ip))
            continue;

        fill_node(&ip, &out->node);
        return 1;
    }
}

static INTN e_lookup(VFS_MOUNT *m, VFS_NODE *dir, const char *name, VFS_NODE *out)
{
    E4VOL *v = vol_of(m);
    UINTN len = 0;

    while (name[len])
        len++;

    if (!dir->is_dir)
        return VFS_ENOTDIR;

    UINT32 ino = dcache_find(v, dir->ino, name, len);

    if (ino == 0 && !dcache_full(v, dir->ino)) {

        /* читаем папку целиком и запоминаем все имена */
        E4INODE dp;

        if (!inode_read(v, dir->ino, &dp))
            return VFS_EIO;

        if (dp.flags & E4_FL_INLINE)
            return VFS_ENOENT;

        char nm[256];
        UINT64 pos = 0;
        UINTN nl = 0;
        UINT32 ci = 0;
        INTN r;

        while ((r = dir_next(v, &dp, &pos, nm, sizeof(nm), &nl, &ci)) > 0) {
            dcache_add(v, dir->ino, nm, nl, ci);
            if (ino == 0 && nl == len && memcmp(nm, name, len) == 0)
                ino = ci;
        }

        if (r < 0)
            return r;

        dcache_mark_full(v, dir->ino);
    }

    if (ino == 0)
        return VFS_ENOENT;

    E4INODE ip;

    if (!inode_read(v, ino, &ip))
        return VFS_EIO;

    if (ip.flags & E4_FL_ENCRYPT)
        return VFS_ENOENT;                      /* шифрованные файлы не читаем */

    fill_node(&ip, out);
    return VFS_OK;
}

static INTN e_read(VFS_MOUNT *m, VFS_NODE *f, UINT64 off, VOID *buf, UINTN n)
{
    E4VOL *v = vol_of(m);
    E4INODE ip;

    if (!inode_read(v, f->ino, &ip))
        return VFS_EIO;

    if ((ip.mode & VFS_MODE_FMT) == VFS_MODE_DIR)
        return VFS_EISDIR;

    return inode_pread(v, &ip, off, buf, n);
}

static INTN e_readlink(VFS_MOUNT *m, VFS_NODE *n, char *buf, UINTN cap)
{
    E4VOL *v = vol_of(m);
    E4INODE ip;

    if (cap == 0 || !inode_read(v, n->ino, &ip))
        return VFS_EIO;

    if ((ip.mode & VFS_MODE_FMT) != VFS_MODE_LNK)
        return VFS_EINVAL;

    UINTN len = (ip.size < cap - 1u) ? (UINTN)ip.size : cap - 1u;

    /* короткая ссылка - прямо в i_block (без экстентов и inline) */
    if (ip.size < 60 && !(ip.flags & (E4_FL_EXTENTS | E4_FL_INLINE))) {
        memcpy(buf, ip.block, len);
        buf[len] = '\0';
        return (INTN)len;
    }

    INTN r = inode_pread(v, &ip, 0, buf, len);

    if (r < 0)
        return r;

    buf[r] = '\0';
    return r;
}

static INTN e_rofs_write(VFS_MOUNT *m, VFS_NODE *f, UINT64 off, const VOID *buf, UINTN n)
{
    (void)m; (void)f; (void)off; (void)buf; (void)n;
    return VFS_EROFS;
}

static INTN e_rofs_truncate(VFS_MOUNT *m, VFS_NODE *f, UINT64 size)
{
    (void)m; (void)f; (void)size;
    return VFS_EROFS;
}

static INTN e_rofs_create(VFS_MOUNT *m, VFS_NODE *dir, const char *name, BOOLEAN is_dir,
                          VFS_NODE *out)
{
    (void)m; (void)dir; (void)name; (void)is_dir; (void)out;
    return VFS_EROFS;
}

static INTN e_rofs_remove(VFS_MOUNT *m, VFS_NODE *dir, VFS_NODE *node)
{
    (void)m; (void)dir; (void)node;
    return VFS_EROFS;
}

static INTN e_rofs_rename(VFS_MOUNT *m, VFS_NODE *dir, VFS_NODE *node, VFS_NODE *newdir,
                          const char *newname)
{
    (void)m; (void)dir; (void)node; (void)newdir; (void)newname;
    return VFS_EROFS;
}

static INTN e_statfs(VFS_MOUNT *m, UINT64 *total, UINT64 *free_bytes)
{
    E4VOL *v = vol_of(m);

    *total = v->blocks * v->bs;
    *free_bytes = v->free_blocks * v->bs;
    return VFS_OK;
}

const VFS_OPS g_ext4_ops = {
    "ext4",
    e_root,
    e_readdir,
    e_lookup,
    e_read,
    e_rofs_write,
    e_rofs_truncate,
    e_rofs_create,
    e_rofs_remove,
    e_rofs_rename,
    e_statfs,
    NULL,                         /* close: нечего дописывать */
    NULL,                         /* open: считать не нужно */
    e_readlink,
};


/* ================================================================
 * Монтирование
 * ================================================================ */

/* Суперблок раздела dev (1024 байта с байта 1024) */
static BOOLEAN read_super(UINTN dev, UINT8 *sb)
{
    if (g_blk[dev].sector_size != 512)
        return FALSE;

    return blk_read(dev, 2, 1, sb) && blk_read(dev, 3, 1, sb + 512);
}

BOOLEAN ext4_detect(UINTN dev)
{
    UINT8 sb[1024];

    return read_super(dev, sb) && rd16(sb + 0x38) == E4_MAGIC;
}

BOOLEAN ext4_mount(UINTN dev, VFS_MOUNT *m)
{
    UINT8 sb[1024];

    if (!read_super(dev, sb) || rd16(sb + 0x38) != E4_MAGIC)
        return FALSE;

    E4VOL *v = (E4VOL *)kzalloc(sizeof(E4VOL));

    if (v == NULL || !cache_init()) {
        kfree(v);
        return FALSE;
    }

    UINT32 log = rd32(sb + 0x18);
    UINT32 rev = rd32(sb + 0x4C);

    v->dev = dev;
    v->bs_shift = 10u + log;
    v->bs = 1024u << log;
    v->spb = v->bs / 512u;
    v->first_data_block = rd32(sb + 0x14);
    v->blocks_per_group = rd32(sb + 0x20);
    v->inodes_per_group = rd32(sb + 0x28);
    v->inode_size = (rev == 0) ? 128u : rd16(sb + 0x58);
    v->compat = rd32(sb + 0x5C);
    v->incompat = (rev == 0) ? 0 : rd32(sb + 0x60);
    v->ro_compat = (rev == 0) ? 0 : rd32(sb + 0x64);
    v->first_meta_bg = rd32(sb + 0x104);
    v->backup_bgs[0] = rd32(sb + 0x24C);
    v->backup_bgs[1] = rd32(sb + 0x250);
    memcpy(v->uuid, sb + 0x68, 16);
    memcpy(v->label, sb + 0x78, 16);
    v->label[16] = '\0';

    v->blocks = rd32(sb + 0x04);
    v->free_blocks = rd32(sb + 0x0C);

    if (v->incompat & E4_IN_64BIT) {
        v->blocks |= (UINT64)rd32(sb + 0x150) << 32;
        v->free_blocks |= (UINT64)rd32(sb + 0x158) << 32;
        v->desc_size = rd16(sb + 0xFE);
        if (v->desc_size < 32)
            v->desc_size = 64;
    } else {
        v->desc_size = 32;
    }

    const char *why = NULL;

    if (log > 2)
        why = "block size above 4 KiB";
    else if (v->incompat & ~E4_IN_KNOWN)
        why = "unknown ext4 features";
    else if (v->incompat & E4_IN_JOURNAL_DEV)
        why = "this is an external journal, not a file system";
    else if (v->blocks_per_group == 0 || v->inodes_per_group == 0 || v->inode_size < 128 ||
             v->inode_size > v->bs || v->desc_size > v->bs ||
             (v->blocks - v->first_data_block) == 0)
        why = "broken superblock";

    if (why != NULL) {
        klog("ext4: %s: not mounted - %s (incompat 0x%x)\n", g_blk[dev].name, why, v->incompat);
        kfree(v);
        return FALSE;
    }

    v->groups = (UINT32)((v->blocks - v->first_data_block + v->blocks_per_group - 1u) /
                         v->blocks_per_group);

    /* описатели всех групп - в память (на 1 ТБ это 512 КиБ) */
    UINTN gdt_bytes = (UINTN)v->groups * v->desc_size;
    UINT32 per = v->bs / v->desc_size;
    UINT32 gdt_blocks = (v->groups + per - 1u) / per;

    v->gdt = (UINT8 *)kmalloc(gdt_bytes + v->bs);

    if (v->gdt == NULL) {
        kfree(v);
        return FALSE;
    }

    for (UINT32 b = 0; b < gdt_blocks; b++) {
        if (!blk_read(dev, gdt_block(v, b) * v->spb, v->spb, v->gdt + (UINTN)b * v->bs)) {
            klog("ext4: %s: cannot read group descriptors\n", g_blk[dev].name);
            kfree(v->gdt);
            kfree(v);
            return FALSE;
        }
    }

    m->e4 = v;
    m->ops = &g_ext4_ops;
    m->readonly = TRUE;              /* ВСЕГДА: чужую систему мы не пишем */

    /* корень должен читаться - иначе это не наш том */
    VFS_NODE root;

    if (e_root(m, &root) != VFS_OK || !root.is_dir) {
        klog("ext4: %s: the root folder is unreadable\n", g_blk[dev].name);
        m->e4 = NULL;
        m->ops = NULL;
        kfree(v->gdt);
        kfree(v);
        return FALSE;
    }

    if (v->incompat & E4_IN_RECOVER)
        klog("ext4: %s: the journal was not replayed (Linux was not shut down cleanly) - "
             "the newest changes may be invisible\n", g_blk[dev].name);

    return TRUE;
}

void ext4_release(VFS_MOUNT *m)
{
    E4VOL *v = vol_of(m);

    if (v == NULL)
        return;

    cache_drop_vol(v);
    dcache_clear(v);
    kfree(v->gdt);
    kfree(v);
    m->e4 = NULL;
}

/* Строка для журнала монтирования и df */
void ext4_info(const VFS_MOUNT *m, char *buf, UINTN cap)
{
    const E4VOL *v = (const E4VOL *)m->e4;

    if (v == NULL) {
        ksnprintf(buf, cap, "ext4");
        return;
    }

    ksnprintf(buf, cap, "ext4, %llu blocks of %u bytes, label \"%s\"", v->blocks, v->bs,
              v->label);
}

/* UUID тома (для /etc/fstab: UUID=...) - строкой, как пишет blkid */
BOOLEAN ext4_uuid(const VFS_MOUNT *m, char *buf, UINTN cap)
{
    const E4VOL *v = (const E4VOL *)m->e4;

    if (v == NULL || m->ops != &g_ext4_ops || cap < 37)
        return FALSE;

    static const char hex[] = "0123456789abcdef";
    UINTN o = 0;

    for (UINTN i = 0; i < 16; i++) {
        if (i == 4 || i == 6 || i == 8 || i == 10)
            buf[o++] = '-';
        buf[o++] = hex[v->uuid[i] >> 4];
        buf[o++] = hex[v->uuid[i] & 15u];
    }

    buf[o] = '\0';
    return TRUE;
}

/* Метка тома (LABEL= в fstab) */
const char *ext4_label(const VFS_MOUNT *m)
{
    const E4VOL *v = (const E4VOL *)m->e4;

    return (v != NULL && m->ops == &g_ext4_ops) ? v->label : "";
}

/* Статистика кэша (для команды disks/ext4 и отладки) */
void ext4_cache_stats(UINT64 *hits, UINT64 *misses, UINTN *names)
{
    *hits = g_e4_hits;
    *misses = g_e4_misses;
    *names = g_dents;
}
