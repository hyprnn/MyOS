/*
 * kernel/pcache.c - кэш страниц файлов и "объекты памяти" (этап 11,
 * шаг 2). Часть MyOS; общие объявления - в myos.h.
 *
 * ЗАЧЕМ
 * -----
 * Программа Linux не читает свои библиотеки вызовом read - она их
 * ОТОБРАЖАЕТ (mmap): "пусть адреса A..B будут содержимым libc.so".
 * Раньше MyOS при mmap сразу читала весь файл в новые страницы
 * процесса. Для bash это терпимо, для Firefox - нет: его libxul.so
 * ~140 МБ, процессов у браузера несколько, и каждый получил бы свою
 * копию (а читать с диска пришлось бы всё, даже то, что не нужно).
 *
 * КАК
 * ---
 * Объект памяти (UOBJ) - "чем наполнена область":
 *   * UOBJ_FILE - файл тома ext4 (раздел Linux меняться не может - мы
 *     его не пишем, поэтому страницу файла достаточно прочитать
 *     ОДИН раз). Прочитанные страницы живут в кэше страниц: ключ
 *     (том, inode, номер страницы). Все процессы, отобразившие libc,
 *     получают одни и те же физические страницы - только для чтения и
 *     с пометкой "копировать при записи" (umem.c): кто захочет писать
 *     (например, ld.so правит таблицы адресов), получит свою копию;
 *   * UOBJ_TMPFS - узел tmpfs (/tmp, /dev/shm, memfd, общая анонимная
 *     память): страницы узла отображаются прямо, общие для всех.
 *
 * Страница появляется при первом касании (Page Fault -> uobj_page),
 * так что из 140 МБ читается только то, что программа трогает.
 *
 * Кэш не растёт бесконечно: если в нём больше половины памяти,
 * страницы, которые сейчас никто не отображает (хозяин - только
 * кэш), отдаются обратно.
 *
 * Чтение с диска может ждать (USB), поэтому uobj_page зовут там, где
 * спать можно: системный вызов или Page Fault программы (cpu.c
 * обрабатывает его "как системный вызов"). Всё - под большим замком.
 */
#include "myos.h"

/* ================================================================
 * Кэш страниц
 * ================================================================ */

typedef struct PCENT {
    struct PCENT *next;
    VFS_MOUNT    *m;
    UINT32        gen;
    UINT32        ino;
    UINT64        idx;
    UINT64        phys;          /* страница; кэш - один из её хозяев */
} PCENT;

#define PC_HASH 65536u

static PCENT  *g_pc[PC_HASH];
static UINT64  g_pc_pages, g_pc_hits, g_pc_misses;
static UINT32  g_pc_hand;        /* откуда продолжать вытеснение */

static UINT32 pc_hash(const VFS_MOUNT *m, UINT32 ino, UINT64 idx)
{
    UINT64 h = (UINT64)(UINTN)m * 0x9E3779B97F4A7C15ull ^ (UINT64)ino * 0xC2B2AE3D27D4EB4Full ^
               idx * 0x165667B19E3779F9ull;

    return (UINT32)(h >> 40) & (PC_HASH - 1u);
}

static PCENT *pc_find(VFS_MOUNT *m, UINT32 gen, UINT32 ino, UINT64 idx)
{
    for (PCENT *e = g_pc[pc_hash(m, ino, idx)]; e != NULL; e = e->next)
        if (e->m == m && e->gen == gen && e->ino == ino && e->idx == idx)
            return e;

    return NULL;
}

/* Сколько страниц кэшу можно держать */
static UINT64 pc_limit(void)
{
    return g_kmm_usable_pages / 2u;
}

/* Отдать страницы, которые никто не отображает, пока кэш не станет
   меньше want */
static void pc_shrink(UINT64 want)
{
    for (UINT32 n = 0; n < PC_HASH && g_pc_pages > want; n++) {

        UINT32 b = g_pc_hand;
        g_pc_hand = (g_pc_hand + 1u) & (PC_HASH - 1u);

        PCENT **pp = &g_pc[b];

        while (*pp != NULL) {
            PCENT *e = *pp;
            if (pmm_page_refs(e->phys) <= 1u) {
                *pp = e->next;
                pmm_page_unref(e->phys);
                kfree(e);
                g_pc_pages--;
                continue;
            }
            pp = &e->next;
        }
    }
}

/*
 * Страница idx файла (том m поколения gen, узел node): из кэша или с
 * диска. +1 хозяин для вызывающего. 0 - ошибка (диск пропал, нет
 * памяти). Хвост за концом файла - нули.
 */
static UINT64 pcache_get(VFS_MOUNT *m, UINT32 gen, VFS_NODE *node, UINT64 idx)
{
    PCENT *e = pc_find(m, gen, node->ino, idx);

    if (e != NULL) {
        g_pc_hits++;
        pmm_page_ref(e->phys);
        return e->phys;
    }

    g_pc_misses++;

    if (g_pc_pages >= pc_limit())
        pc_shrink(pc_limit() - pc_limit() / 8u);

    UINT64 phys = pmm_alloc_zeroed(1, 0);

    if (phys == 0) {
        pc_shrink(g_pc_pages / 2u);         /* память кончилась - отдать половину */
        phys = pmm_alloc_zeroed(1, 0);
        if (phys == 0)
            return 0;
    }

    /* чтение может уснуть (диск) - а кто-то другой тем временем мог
       прочитать ту же страницу: тогда берём его */
    INTN r = vfs_node_read(m, gen, node, idx * 4096u, P2V(phys), 4096u);

    if (r < 0) {
        pmm_page_unref(phys);
        return 0;
    }

    e = pc_find(m, gen, node->ino, idx);

    if (e != NULL) {
        pmm_page_unref(phys);
        pmm_page_ref(e->phys);
        return e->phys;
    }

    e = (PCENT *)kmalloc(sizeof(PCENT));

    if (e != NULL) {
        UINT32 h = pc_hash(m, node->ino, idx);
        e->m = m;
        e->gen = gen;
        e->ino = node->ino;
        e->idx = idx;
        e->phys = phys;
        e->next = g_pc[h];
        g_pc[h] = e;
        g_pc_pages++;
        pmm_page_ref(phys);                 /* второй хозяин - вызывающий */
    }

    /* без записи в кэше (нет памяти на неё) страница просто достаётся
       вызывающему целиком */
    return phys;
}

/* Том уходит (флешку вынули): его страницы - из кэша. Отображённые
   программами остаются у программ (счётчик хозяев). */
void pcache_drop_mount(VFS_MOUNT *m)
{
    for (UINT32 b = 0; b < PC_HASH; b++) {

        PCENT **pp = &g_pc[b];

        while (*pp != NULL) {
            PCENT *e = *pp;
            if (e->m == m) {
                *pp = e->next;
                pmm_page_unref(e->phys);
                kfree(e);
                g_pc_pages--;
                continue;
            }
            pp = &e->next;
        }
    }
}

void pcache_stats(UINT64 *pages, UINT64 *hits, UINT64 *misses)
{
    *pages = g_pc_pages;
    *hits = g_pc_hits;
    *misses = g_pc_misses;
}


/* ================================================================
 * Объекты памяти
 * ================================================================ */

/*
 * Объект для открытого файла VFS kfd: файл ext4 -> через кэш страниц,
 * файл tmpfs -> его узел. NULL - так отобразить нельзя (FAT, exFAT:
 * там mmap по-старому - копия содержимого).
 */
UOBJ *uobj_from_fd(INTN kfd)
{
    VFS_MOUNT *m = NULL;
    VFS_NODE *node = (VFS_NODE *)kmalloc(sizeof(VFS_NODE));

    if (node == NULL)
        return NULL;

    if (vfs_fd_node(kfd, &m, node) != VFS_OK ||
        (m->ops != &g_ext4_ops && m->ops != &g_tmp_ops)) {
        kfree(node);
        return NULL;
    }

    UOBJ *o = (UOBJ *)kzalloc(sizeof(UOBJ));

    if (o == NULL) {
        kfree(node);
        return NULL;
    }

    o->refs = 1;

    if (m->ops == &g_ext4_ops) {
        o->kind = UOBJ_FILE;
        o->m = m;
        o->gen = m->dev_gen;
        o->node = *node;
    } else {
        o->kind = UOBJ_TMPFS;
        o->tn = tmpfs_node(node);
        if (o->tn == NULL) {
            kfree(o);
            kfree(node);
            return NULL;
        }
        tmpfs_hold(o->tn);
    }

    kfree(node);
    return o;
}

/* Общая анонимная память (MAP_SHARED | MAP_ANONYMOUS): безымянный
   узел tmpfs - после fork родитель и потомок видят одни страницы */
UOBJ *uobj_anon(UINT64 size)
{
    UOBJ *o = (UOBJ *)kzalloc(sizeof(UOBJ));

    if (o == NULL)
        return NULL;

    o->tn = tmpfs_anon(size);

    if (o->tn == NULL) {
        kfree(o);
        return NULL;
    }

    o->refs = 1;
    o->kind = UOBJ_TMPFS;
    return o;
}

void uobj_ref(UOBJ *o)
{
    if (o != NULL)
        o->refs++;
}

void uobj_put(UOBJ *o)
{
    if (o == NULL || --o->refs > 0)
        return;

    if (o->kind == UOBJ_TMPFS)
        tmpfs_put(o->tn);

    kfree(o);
}

/* Страница idx объекта; +1 хозяин для вызывающего; 0 - не вышло */
UINT64 uobj_page(UOBJ *o, UINT64 idx)
{
    if (o->kind == UOBJ_TMPFS)
        return tmpfs_page(o->tn, idx);

    return pcache_get(o->m, o->gen, &o->node, idx);
}
