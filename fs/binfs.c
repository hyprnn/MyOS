/*
 * fs/binfs.c - том /bin: встроенные программы (этап 6).
 * Часть MyOS; общие объявления - в myos.h.
 *
 * Программы (user/) собираются отдельными файлами ELF - как на любой
 * ОС. Чтобы они были под рукой всегда (даже если MyOS загрузилась с
 * CD или диска, который она не видит), make вклеивает их копии в
 * образ ядра (build/apps.S, директива .incbin), и ядро показывает их
 * как том /bin "только для чтения" - как initramfs в Linux.
 * Те же файлы make кладёт и на флешку (esp/APPS/), откуда их можно
 * запускать по пути: /usb0p1/apps/hello.
 */
#include "myos.h"

typedef struct {
    const char  *name;
    const UINT8 *start;
    const UINT8 *end;
} APP_ENTRY;

/* build/apps.S: список {имя, начало, конец}, в конце - нули */
extern const APP_ENTRY g_app_table[];

static UINTN bin_count(void)
{
    UINTN n = 0;

    while (g_app_table[n].name != NULL)
        n++;

    return n;
}

static void bin_fill(UINTN i, VFS_DIRENT *out)
{
    UINTN k = 0;

    for (; g_app_table[i].name[k] && k + 1 < VFS_NAME_MAX; k++)
        out->name[k] = g_app_table[i].name[k];

    out->name[k] = '\0';

    raw_zero_mem((volatile UINT8 *)&out->node, sizeof(out->node));
    out->node.ram_index = (INTN)i;
    out->node.size = (UINT64)(g_app_table[i].end - g_app_table[i].start);
}

static INTN bin_root(VFS_MOUNT *m, VFS_NODE *out)
{
    (void)m;
    raw_zero_mem((volatile UINT8 *)out, sizeof(*out));
    out->is_root = TRUE;
    out->is_dir = TRUE;
    out->ram_index = -1;
    return VFS_OK;
}

static INTN bin_readdir(VFS_MOUNT *m, VFS_NODE *dir, UINT64 *cookie, VFS_DIRENT *out)
{
    (void)m;
    (void)dir;

    if (*cookie >= bin_count())
        return 0;

    bin_fill((UINTN)*cookie, out);
    (*cookie)++;
    return 1;
}

static BOOLEAN name_ci(const char *a, const char *b)
{
    for (; *a && *b; a++, b++) {
        char x = *a, y = *b;
        if (x >= 'A' && x <= 'Z') x = (char)(x - 'A' + 'a');
        if (y >= 'A' && y <= 'Z') y = (char)(y - 'A' + 'a');
        if (x != y)
            return FALSE;
    }

    return *a == *b;
}

static INTN bin_lookup(VFS_MOUNT *m, VFS_NODE *dir, const char *name, VFS_NODE *out)
{
    (void)m;
    (void)dir;

    for (UINTN i = 0; g_app_table[i].name != NULL; i++) {
        if (name_ci(g_app_table[i].name, name)) {
            VFS_DIRENT de;
            bin_fill(i, &de);
            *out = de.node;
            return VFS_OK;
        }
    }

    return VFS_ENOENT;
}

static INTN bin_read(VFS_MOUNT *m, VFS_NODE *f, UINT64 off, VOID *buf, UINTN n)
{
    (void)m;

    const APP_ENTRY *a = &g_app_table[f->ram_index];
    UINT64 size = (UINT64)(a->end - a->start);

    if (off >= size)
        return 0;

    if (n > size - off)
        n = (UINTN)(size - off);

    memcpy(buf, a->start + off, n);

    return (INTN)n;
}

static INTN bin_ro_write(VFS_MOUNT *m, VFS_NODE *f, UINT64 off, const VOID *b, UINTN n)
{
    (void)m; (void)f; (void)off; (void)b; (void)n;
    return VFS_EROFS;
}

static INTN bin_ro_trunc(VFS_MOUNT *m, VFS_NODE *f, UINT64 s)
{
    (void)m; (void)f; (void)s;
    return VFS_EROFS;
}

static INTN bin_ro_create(VFS_MOUNT *m, VFS_NODE *d, const char *n, BOOLEAN dir, VFS_NODE *o)
{
    (void)m; (void)d; (void)n; (void)dir; (void)o;
    return VFS_EROFS;
}

static INTN bin_ro_remove(VFS_MOUNT *m, VFS_NODE *d, VFS_NODE *n)
{
    (void)m; (void)d; (void)n;
    return VFS_EROFS;
}

static INTN bin_ro_rename(VFS_MOUNT *m, VFS_NODE *d, VFS_NODE *n, VFS_NODE *nd, const char *nn)
{
    (void)m; (void)d; (void)n; (void)nd; (void)nn;
    return VFS_EROFS;
}

static INTN bin_statfs(VFS_MOUNT *m, UINT64 *total, UINT64 *free_bytes)
{
    (void)m;

    UINT64 t = 0;

    for (UINTN i = 0; g_app_table[i].name != NULL; i++)
        t += (UINT64)(g_app_table[i].end - g_app_table[i].start);

    *total = t;
    *free_bytes = 0;

    return VFS_OK;
}

const VFS_OPS g_bin_ops = {
    "binfs",
    bin_root,
    bin_readdir,
    bin_lookup,
    bin_read,
    bin_ro_write,
    bin_ro_trunc,
    bin_ro_create,
    bin_ro_remove,
    bin_ro_rename,
    bin_statfs,
    NULL,                         /* close: нечего дописывать */
    NULL,                         /* open: считать не нужно */
    NULL                          /* readlink: ссылок нет */
};

/* Смонтировать /bin (после vfs_init) */
void binfs_mount(void)
{
    kmutex_lock(&g_vfs_mutex);

    for (UINTN i = 0; i < VFS_MAX_MOUNTS; i++) {

        VFS_MOUNT *m = &g_mounts[i];

        if (m->used)
            continue;

        raw_zero_mem((volatile UINT8 *)m, sizeof(*m));
        m->name[0] = 'b';
        m->name[1] = 'i';
        m->name[2] = 'n';
        m->name[3] = '\0';
        m->ops = &g_bin_ops;
        m->readonly = TRUE;
        m->used = TRUE;
        break;
    }

    kmutex_unlock(&g_vfs_mutex);

    klog("binfs: /bin has %u programs\n", (UINT32)bin_count());
}
