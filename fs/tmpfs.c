/*
 * fs/tmpfs.c - том /tmp: файлы и папки в оперативной памяти (этап 11).
 * Часть MyOS; общие объявления - в myos.h.
 *
 * Зачем: программы Linux пишут временные файлы в /tmp и ждут там
 * настоящую файловую систему - папки, большие файлы, имена с учётом
 * регистра, "удалённый, но ещё открытый" файл. Старый RAM-диск MyOS
 * (/ram) - это 32 маленьких файла без папок, для него это слишком.
 *
 * Устройство:
 *   * узел (TNODE) - файл или папка; у папки - список детей;
 *   * данные файла - массив физических страниц по 4 КиБ (растёт по
 *     мере записи; "дыры" - нулевые страницы);
 *   * узел VFS (VFS_NODE) хранит адрес TNODE в поле ram_index;
 *   * удалённый файл, который кто-то держит открытым (так делают
 *     tmpfile() и многие программы), живёт до последнего close: open
 *     и close считаются (VFS_OPS.open / close).
 * После перезагрузки /tmp пуст - как и в Linux.
 */
#include "myos.h"

typedef struct TNODE {
    struct TNODE *parent;
    struct TNODE *child;          /* первый ребёнок (папка) */
    struct TNODE *next;           /* следующий в папке */
    char          name[VFS_NAME_MAX];
    BOOLEAN       is_dir;
    BOOLEAN       unlinked;       /* удалён из папки, ждёт последнего close */
    UINT32        opens;          /* сколько раз открыт сейчас */
    UINT64        size;
    UINT64       *pages;          /* физические адреса страниц данных (0 - нулевая) */
    UINTN         cap;            /* размер массива pages */
    UINT16        wdate, wtime;   /* дата изменения (как у FAT) */
} TNODE;

static TNODE g_troot;
static UINT64 g_tmp_bytes;       /* занято данными */

static TNODE *tn(const VFS_NODE *n)
{
    return n->is_root ? &g_troot : (TNODE *)(UINTN)n->ram_index;
}

static void tn_stamp(TNODE *t)
{
    EFI_TIME now;

    if (rtc_read(&now)) {
        t->wdate = (UINT16)(((now.Year - 1980u) << 9) | (now.Month << 5) | now.Day);
        t->wtime = (UINT16)((now.Hour << 11) | (now.Minute << 5) | (now.Second / 2u));
    }
}

static void tn_fill(TNODE *t, VFS_NODE *out)
{
    raw_zero_mem((volatile UINT8 *)out, sizeof(*out));
    out->is_dir = t->is_dir;
    out->is_root = (t == &g_troot);
    out->size = t->size;
    out->ram_index = (INTN)(UINTN)t;
    out->wdate = t->wdate;
    out->wtime = t->wtime;
}

static void tn_free_data(TNODE *t, UINT64 from_page)
{
    /* unref, а не free: страницу может держать отображение программы
       (mmap MAP_SHARED, memfd) - она уйдёт с последним хозяином */
    for (UINTN i = (UINTN)from_page; i < t->cap; i++)
        if (t->pages[i] != 0) {
            pmm_page_unref(t->pages[i]);
            t->pages[i] = 0;
            g_tmp_bytes -= 4096u;
        }
}

static void tn_destroy(TNODE *t)
{
    tn_free_data(t, 0);
    kfree(t->pages);
    kfree(t);
}

static INTN t_root(VFS_MOUNT *m, VFS_NODE *out)
{
    (void)m;
    g_troot.is_dir = TRUE;
    tn_fill(&g_troot, out);
    return VFS_OK;
}

static INTN t_readdir(VFS_MOUNT *m, VFS_NODE *dir, UINT64 *cookie, VFS_DIRENT *out)
{
    (void)m;

    TNODE *c = tn(dir)->child;

    for (UINT64 i = 0; c != NULL && i < *cookie; i++)
        c = c->next;

    if (c == NULL)
        return 0;

    ksnprintf(out->name, sizeof(out->name), "%s", c->name);
    tn_fill(c, &out->node);
    (*cookie)++;
    return 1;
}

static BOOLEAN name_eq(const char *a, const char *b)
{
    while (*a && *a == *b) {
        a++;
        b++;
    }

    return *a == *b;
}

static INTN t_lookup(VFS_MOUNT *m, VFS_NODE *dir, const char *name, VFS_NODE *out)
{
    (void)m;

    for (TNODE *c = tn(dir)->child; c != NULL; c = c->next)
        if (name_eq(c->name, name)) {
            tn_fill(c, out);
            return VFS_OK;
        }

    return VFS_ENOENT;
}

static INTN t_read(VFS_MOUNT *m, VFS_NODE *f, UINT64 off, VOID *buf, UINTN n)
{
    (void)m;

    TNODE *t = tn(f);
    UINT8 *out = (UINT8 *)buf;
    UINTN done = 0;

    while (done < n && off + done < t->size) {

        UINT64 pos = off + done;
        UINTN pg = (UINTN)(pos / 4096u), in = (UINTN)(pos % 4096u);
        UINTN k = 4096u - in;

        if (k > n - done)
            k = n - done;
        if (k > t->size - pos)
            k = (UINTN)(t->size - pos);

        if (pg < t->cap && t->pages[pg] != 0)
            memcpy(out + done, (UINT8 *)P2V(t->pages[pg]) + in, k);
        else
            memset(out + done, 0, k);

        done += k;
    }

    return (INTN)done;
}

/* Массив страниц - не меньше need */
static BOOLEAN tn_grow(TNODE *t, UINTN need)
{
    if (need <= t->cap)
        return TRUE;

    UINTN nc = t->cap ? t->cap * 2u : 16u;

    while (nc < need)
        nc *= 2u;

    UINT64 *np = (UINT64 *)kzalloc(nc * sizeof(UINT64));

    if (np == NULL)
        return FALSE;

    if (t->pages != NULL) {
        memcpy(np, t->pages, t->cap * sizeof(UINT64));
        kfree(t->pages);
    }

    t->pages = np;
    t->cap = nc;
    return TRUE;
}

static INTN t_write(VFS_MOUNT *m, VFS_NODE *f, UINT64 off, const VOID *buf, UINTN n)
{
    (void)m;

    TNODE *t = tn(f);
    const UINT8 *in = (const UINT8 *)buf;
    UINTN done = 0;

    if (t->is_dir)
        return VFS_EISDIR;

    if (n == 0)
        return 0;

    if (!tn_grow(t, (UINTN)((off + n + 4095u) / 4096u)))
        return VFS_ENOSPC;

    while (done < n) {

        UINT64 pos = off + done;
        UINTN pg = (UINTN)(pos / 4096u), at = (UINTN)(pos % 4096u);
        UINTN k = 4096u - at;

        if (k > n - done)
            k = n - done;

        if (t->pages[pg] == 0) {
            UINT64 phys = pmm_alloc_zeroed(1, 0);
            if (phys == 0)
                break;
            t->pages[pg] = phys;
            g_tmp_bytes += 4096u;
        }

        memcpy((UINT8 *)P2V(t->pages[pg]) + at, in + done, k);
        done += k;
    }

    if (off + done > t->size)
        t->size = off + done;

    f->size = t->size;
    tn_stamp(t);

    if (done == 0)
        return VFS_ENOSPC;

    return (INTN)done;
}

static INTN t_truncate(VFS_MOUNT *m, VFS_NODE *f, UINT64 size)
{
    (void)m;

    TNODE *t = tn(f);

    if (size < t->size) {

        UINT64 keep = (size + 4095u) / 4096u;
        tn_free_data(t, keep);

        /* хвост последней страницы - нули (вдруг файл потом вырастет) */
        if (size % 4096u && keep > 0 && keep - 1 < t->cap && t->pages[keep - 1] != 0)
            memset((UINT8 *)P2V(t->pages[keep - 1]) + size % 4096u, 0, 4096u - size % 4096u);
    }

    t->size = size;
    f->size = size;
    tn_stamp(t);
    return VFS_OK;
}

static INTN t_create(VFS_MOUNT *m, VFS_NODE *dir, const char *name, BOOLEAN is_dir, VFS_NODE *out)
{
    (void)m;

    TNODE *d = tn(dir);
    UINTN len = 0;

    while (name[len])
        len++;

    if (len == 0 || len >= VFS_NAME_MAX)
        return VFS_EINVAL;

    for (TNODE *c = d->child; c != NULL; c = c->next)
        if (name_eq(c->name, name))
            return VFS_EEXIST;

    TNODE *t = (TNODE *)kzalloc(sizeof(TNODE));

    if (t == NULL)
        return VFS_ENOSPC;

    ksnprintf(t->name, sizeof(t->name), "%s", name);
    t->is_dir = is_dir;
    t->parent = d;
    t->next = d->child;
    d->child = t;
    tn_stamp(t);

    tn_fill(t, out);
    return VFS_OK;
}

/* Вынуть узел из его папки */
static void tn_unlink(TNODE *t)
{
    TNODE **pp = &t->parent->child;

    while (*pp != NULL && *pp != t)
        pp = &(*pp)->next;

    if (*pp == t)
        *pp = t->next;

    t->next = NULL;
}

static INTN t_remove(VFS_MOUNT *m, VFS_NODE *dir, VFS_NODE *node)
{
    (void)m;
    (void)dir;

    TNODE *t = tn(node);

    if (t == &g_troot)
        return VFS_EINVAL;

    if (t->is_dir && t->child != NULL)
        return VFS_ENOTEMPTY;

    tn_unlink(t);

    if (t->opens > 0)
        t->unlinked = TRUE;         /* освободит последний close */
    else
        tn_destroy(t);

    return VFS_OK;
}

static INTN t_rename(VFS_MOUNT *m, VFS_NODE *dir, VFS_NODE *node, VFS_NODE *newdir,
                     const char *newname)
{
    (void)m;
    (void)dir;

    TNODE *t = tn(node), *nd = tn(newdir);

    /* папку нельзя перенести внутрь неё самой */
    for (TNODE *a = nd; a != NULL; a = a->parent)
        if (a == t)
            return VFS_EINVAL;

    for (TNODE *c = nd->child; c != NULL; c = c->next)
        if (c != t && name_eq(c->name, newname))
            return VFS_EEXIST;

    tn_unlink(t);
    ksnprintf(t->name, sizeof(t->name), "%s", newname);
    t->parent = nd;
    t->next = nd->child;
    nd->child = t;
    return VFS_OK;
}

static INTN t_statfs(VFS_MOUNT *m, UINT64 *total, UINT64 *free_bytes)
{
    (void)m;
    *free_bytes = g_kmm_free_pages * 4096u;
    *total = *free_bytes + g_tmp_bytes;
    return VFS_OK;
}

static INTN t_open(VFS_MOUNT *m, VFS_NODE *f)
{
    (void)m;
    tn(f)->opens++;
    return VFS_OK;
}

/* Узел ещё нужен (кто-то отобразил его в память) / уже не нужен */
static void tn_put(TNODE *t)
{
    if (t->opens > 0)
        t->opens--;

    if (t->unlinked && t->opens == 0)
        tn_destroy(t);
}

static INTN t_close(VFS_MOUNT *m, VFS_NODE *f)
{
    (void)m;

    tn_put(tn(f));
    return VFS_OK;
}

const VFS_OPS g_tmp_ops = {
    "tmpfs",
    t_root,
    t_readdir,
    t_lookup,
    t_read,
    t_write,
    t_truncate,
    t_create,
    t_remove,
    t_rename,
    t_statfs,
    t_close,
    t_open,
    NULL,                         /* readlink: ссылок (пока) нет */
};

void tmpfs_mount(void)
{
    kmutex_lock(&g_vfs_mutex);

    for (UINTN i = 0; i < VFS_MAX_MOUNTS; i++) {

        VFS_MOUNT *m = &g_mounts[i];

        if (m->used)
            continue;

        raw_zero_mem((volatile UINT8 *)m, sizeof(*m));
        ksnprintf(m->name, sizeof(m->name), "tmp");
        m->ops = &g_tmp_ops;
        m->used = TRUE;
        break;
    }

    g_troot.is_dir = TRUE;
    tn_stamp(&g_troot);

    kmutex_unlock(&g_vfs_mutex);
}


/* ================================================================
 * Файлы tmpfs как память программ (этап 11, шаг 2)
 *
 * mmap(MAP_SHARED) файла из /tmp или /dev/shm, memfd_create и общая
 * анонимная память (MAP_SHARED | MAP_ANONYMOUS) - это одно и то же:
 * страницы узла tmpfs прямо в таблицах страниц программы. Все, кто
 * отобразил узел, видят одни и те же физические страницы (так
 * Firefox и Wayland передают картинки между процессами). Отображение
 * держит узел (как открытый файл), страницы - через счётчик хозяев
 * pmm (umem.c).
 * ================================================================ */

/* Узел tmpfs за узлом VFS (вызывающий проверил, что том - tmpfs) */
void *tmpfs_node(const VFS_NODE *n)
{
    return n->is_root ? NULL : (void *)(UINTN)n->ram_index;
}

void tmpfs_hold(void *node)
{
    ((TNODE *)node)->opens++;
}

void tmpfs_put(void *node)
{
    tn_put((TNODE *)node);
}

/* Страница idx узла (нет - обнулённая новая); +1 хозяин для
   вызывающего. 0 - нет памяти. */
UINT64 tmpfs_page(void *node, UINT64 idx)
{
    TNODE *t = (TNODE *)node;

    if (idx > (1ull << 28) || !tn_grow(t, (UINTN)idx + 1u))
        return 0;

    if (t->pages[idx] == 0) {
        UINT64 phys = pmm_alloc_zeroed(1, 0);
        if (phys == 0)
            return 0;
        t->pages[idx] = phys;
        g_tmp_bytes += 4096u;
    }

    pmm_page_ref(t->pages[idx]);
    return t->pages[idx];
}

/* Узел без имени (общая анонимная память, memfd): ни в одной папке,
   живёт, пока его держат */
void *tmpfs_anon(UINT64 size)
{
    TNODE *t = (TNODE *)kzalloc(sizeof(TNODE));

    if (t == NULL)
        return NULL;

    t->parent = &g_troot;
    t->unlinked = TRUE;
    t->opens = 1;
    t->size = size;
    ksnprintf(t->name, sizeof(t->name), "anon");
    return t;
}
