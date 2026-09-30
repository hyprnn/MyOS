/*
 * fs/vfs.c - VFS: общий слой файлов (этап 5). Часть MyOS; общие
 * объявления - в myos.h.
 *
 * Программам (шеллу, GUI) всё равно, на какой файловой системе лежит
 * файл - они говорят "/usb0p1/notes/todo.txt". VFS разбирает путь:
 *   * первая часть пути - имя ТОМА (точки монтирования): "ram" -
 *     старый RAM-диск, "usb0p1" - первый раздел первой флешки,
 *     "sata0p1" - раздел EFI внутреннего диска и т.д.;
 *   * остальное - папки и файл внутри тома; их ищет драйвер
 *     файловой системы (fs/fat.c или RAM-диск ниже) через таблицу
 *     функций VFS_OPS.
 * Корень "/" - виртуальный: в нём только тома.
 *
 * Для работы с содержимым - как в Unix: vfs_open даёт номер
 * открытого файла (fd), дальше vfs_read / vfs_write / vfs_close.
 *
 * Всё - под мьютексом g_vfs_mutex: шелл, GUI и поток usb не
 * мешают друг другу (см. drivers/blk.c про порядок замков).
 */
#include "myos.h"

VFS_MOUNT g_mounts[VFS_MAX_MOUNTS];
char g_cwd[VFS_PATH_MAX] = "/ram";
KMUTEX g_vfs_mutex = KMUTEX_INIT("files and disks");

typedef struct {
    BOOLEAN  used;
    BOOLEAN  gone;           /* том пропал (флешку вынули) */
    INTN     mount;
    VFS_NODE node;
    UINT64   pos;
    UINT32   flags;
} VFS_FD;

static VFS_FD g_fds[VFS_MAX_FD];


/* ================================================================
 * RAM-диск как файловая система "ram"
 *
 * Старый RAM-диск (g_fs, shell/fs.c до этапа 5): 32 файла без
 * папок, текст хранится 16-битными символами (CHAR16). Здесь он
 * просто "одевается" в VFS_OPS - и старые файлы видны как /ram/имя.
 * ================================================================ */

static BOOLEAN ram_name_eq(const CHAR16 *a, const char *b)
{
    UINTN i = 0;

    for (; a[i] != 0 && b[i] != '\0'; i++)
        if (a[i] != (CHAR16)(UINT8)b[i])
            return FALSE;

    return a[i] == 0 && b[i] == '\0';
}

static void ram_fill(INTN i, VFS_DIRENT *out)
{
    UINTN n = 0;

    for (; g_fs[i].name[n] != 0 && n + 1 < VFS_NAME_MAX; n++)
        out->name[n] = (g_fs[i].name[n] < 128) ? (char)g_fs[i].name[n] : '?';

    out->name[n] = '\0';

    raw_zero_mem((volatile UINT8 *)&out->node, sizeof(out->node));
    out->node.ram_index = i;
    out->node.size = g_fs[i].size;
}

static INTN ram_root(VFS_MOUNT *m, VFS_NODE *out)
{
    (void)m;
    raw_zero_mem((volatile UINT8 *)out, sizeof(*out));
    out->is_root = TRUE;
    out->is_dir = TRUE;
    out->ram_index = -1;
    return VFS_OK;
}

static INTN ram_readdir(VFS_MOUNT *m, VFS_NODE *dir, UINT64 *cookie, VFS_DIRENT *out)
{
    (void)m;
    (void)dir;

    for (UINT64 i = *cookie; i < FS_MAX_FILES; i++) {
        if (g_fs[i].used) {
            ram_fill((INTN)i, out);
            *cookie = i + 1u;
            return 1;
        }
    }

    *cookie = FS_MAX_FILES;
    return 0;
}

static INTN ram_lookup(VFS_MOUNT *m, VFS_NODE *dir, const char *name, VFS_NODE *out)
{
    (void)m;
    (void)dir;

    for (INTN i = 0; i < FS_MAX_FILES; i++) {
        if (g_fs[i].used && ram_name_eq(g_fs[i].name, name)) {
            VFS_DIRENT de;
            ram_fill(i, &de);
            *out = de.node;
            return VFS_OK;
        }
    }

    return VFS_ENOENT;
}

static INTN ram_read(VFS_MOUNT *m, VFS_NODE *f, UINT64 off, VOID *buf, UINTN n)
{
    (void)m;

    FS_FILE *r = &g_fs[f->ram_index];
    UINT8 *out = (UINT8 *)buf;
    UINTN done = 0;

    while (done < n && off + done < r->size) {
        out[done] = (UINT8)r->data[off + done];
        done++;
    }

    return (INTN)done;
}

static INTN ram_write(VFS_MOUNT *m, VFS_NODE *f, UINT64 off, const VOID *buf, UINTN n)
{
    (void)m;

    FS_FILE *r = &g_fs[f->ram_index];
    const UINT8 *in = (const UINT8 *)buf;
    UINTN done = 0;

    if (off > r->size)
        off = r->size;

    /* последний символ - всегда 0 (так ждёт старый код) */
    while (done < n && off + done < FS_DATA_MAX - 1u) {
        r->data[off + done] = (CHAR16)in[done];
        done++;
    }

    if (off + done > r->size)
        r->size = off + done;

    r->data[r->size] = 0;
    f->size = r->size;

    if (done == 0 && n > 0)
        return VFS_ENOSPC;

    return (INTN)done;
}

static INTN ram_truncate(VFS_MOUNT *m, VFS_NODE *f, UINT64 size)
{
    (void)m;

    FS_FILE *r = &g_fs[f->ram_index];

    if (size < r->size) {
        r->size = size;
        r->data[size] = 0;
    }

    f->size = r->size;
    return VFS_OK;
}

static INTN ram_create(VFS_MOUNT *m, VFS_NODE *dir, const char *name, BOOLEAN is_dir,
                       VFS_NODE *out)
{
    (void)m;
    (void)dir;

    if (is_dir)
        return VFS_ENOSYS;          /* у RAM-диска нет папок */

    UINTN len = 0;

    while (name[len])
        len++;

    if (len == 0 || len >= FS_NAME_MAX)
        return VFS_EINVAL;

    int i = fs_find_free();

    if (i < 0)
        return VFS_ENOSPC;

    for (UINTN k = 0; k <= len; k++)
        g_fs[i].name[k] = (CHAR16)(UINT8)name[k];

    g_fs[i].data[0] = 0;
    g_fs[i].size = 0;
    g_fs[i].used = TRUE;

    VFS_DIRENT de;
    ram_fill(i, &de);
    *out = de.node;

    return VFS_OK;
}

static INTN ram_remove(VFS_MOUNT *m, VFS_NODE *dir, VFS_NODE *node)
{
    (void)m;
    (void)dir;

    if (node->ram_index < 0)
        return VFS_EINVAL;

    g_fs[node->ram_index].used = FALSE;
    return VFS_OK;
}

static INTN ram_rename(VFS_MOUNT *m, VFS_NODE *dir, VFS_NODE *node,
                       VFS_NODE *newdir, const char *newname)
{
    (void)m;
    (void)dir;
    (void)newdir;

    UINTN len = 0;

    while (newname[len])
        len++;

    if (node->ram_index < 0 || len == 0 || len >= FS_NAME_MAX)
        return VFS_EINVAL;

    for (UINTN k = 0; k <= len; k++)
        g_fs[node->ram_index].name[k] = (CHAR16)(UINT8)newname[k];

    return VFS_OK;
}

static INTN ram_statfs(VFS_MOUNT *m, UINT64 *total, UINT64 *free_bytes)
{
    (void)m;

    UINT64 used = 0;

    for (UINTN i = 0; i < FS_MAX_FILES; i++)
        if (g_fs[i].used)
            used++;

    *total = (UINT64)FS_MAX_FILES * (FS_DATA_MAX - 1u);
    *free_bytes = (UINT64)(FS_MAX_FILES - used) * (FS_DATA_MAX - 1u);

    return VFS_OK;
}

static const VFS_OPS g_ram_ops = {
    "ramfs",
    ram_root,
    ram_readdir,
    ram_lookup,
    ram_read,
    ram_write,
    ram_truncate,
    ram_create,
    ram_remove,
    ram_rename,
    ram_statfs,
    NULL                          /* close: нечего дописывать */
};


/* ================================================================
 * Тома
 * ================================================================ */

void vfs_init(void)
{
    VFS_MOUNT *m = &g_mounts[0];

    raw_zero_mem((volatile UINT8 *)m, sizeof(*m));
    m->used = TRUE;
    m->name[0] = 'r';
    m->name[1] = 'a';
    m->name[2] = 'm';
    m->name[3] = '\0';
    m->ops = &g_ram_ops;
}

/* Смонтировать FAT с устройства dev как /name */
INTN vfs_mount_dev(UINTN dev, const char *name)
{
    kmutex_lock(&g_vfs_mutex);

    INTN slot = -1;

    for (UINTN i = 0; i < VFS_MAX_MOUNTS; i++)
        if (!g_mounts[i].used) {
            slot = (INTN)i;
            break;
        }

    if (slot < 0) {
        kmutex_unlock(&g_vfs_mutex);
        return VFS_ENOSPC;
    }

    VFS_MOUNT *m = &g_mounts[slot];
    const char *why = NULL;

    raw_zero_mem((volatile UINT8 *)m, sizeof(*m));

    /* FAT16/32 - свой драйвер (fs/fat.c); exFAT - через FatFs */
    BOOLEAN exfat = FALSE;

    if (!fat_probe(dev, &m->fat, &why)) {
        raw_zero_mem((volatile UINT8 *)m, sizeof(*m));
        if (!exfat_mount(dev, m)) {
            kmutex_unlock(&g_vfs_mutex);
            return VFS_EINVAL;
        }
        exfat = TRUE;
    }

    UINTN k = 0;

    while (name[k] && k + 1 < sizeof(m->name)) {
        m->name[k] = name[k];
        k++;
    }

    m->name[k] = '\0';
    if (!exfat)
        m->ops = &g_fat_ops;
    m->dev = dev;
    m->dev_gen = g_blk[dev].gen;
    m->readonly = !g_blk[dev].writable;
    m->used = TRUE;

    char ty[12];
    vfs_fs_name(m, ty, sizeof(ty));
    klog("vfs: /%s mounted: %s, %u clusters of %u bytes, label \"%s\"%s\n",
         m->name, ty, m->fat.clusters, m->fat.cluster_bytes,
         m->fat.label, m->readonly ? ", read-only" : "");

    kmutex_unlock(&g_vfs_mutex);

    return slot;
}

/* Устройство пропало: его том - в небытие, открытые файлы - "gone",
   текущая папка на нём - в корень */
void vfs_forget_dev(UINTN dev)
{
    kmutex_lock(&g_vfs_mutex);

    for (UINTN i = 0; i < VFS_MAX_MOUNTS; i++) {

        VFS_MOUNT *m = &g_mounts[i];

        if (!m->used || !vfs_is_disk(m) || m->dev != dev)
            continue;

        klog("vfs: /%s is gone (the disk disappeared)\n", m->name);

        for (UINTN f = 0; f < VFS_MAX_FD; f++)
            if (g_fds[f].used && g_fds[f].mount == (INTN)i)
                g_fds[f].gone = TRUE;

        /* текущая папка внутри этого тома? */
        UINTN n = 0;

        while (m->name[n])
            n++;

        BOOLEAN inside = TRUE;

        for (UINTN k = 0; k < n; k++)
            if (g_cwd[1 + k] != m->name[k])
                inside = FALSE;

        if (inside && (g_cwd[1 + n] == '\0' || g_cwd[1 + n] == '/')) {
            g_cwd[0] = '/';
            g_cwd[1] = '\0';
        }

        /* и у программ (шелл-программа, этап 10) */
        proc_forget_volume(m->name);

        exfat_release(m);
        m->used = FALSE;
        m->gone = TRUE;
    }

    kmutex_unlock(&g_vfs_mutex);
}

static INTN mount_find(const char *name, UINTN len)
{
    for (UINTN i = 0; i < VFS_MAX_MOUNTS; i++) {

        VFS_MOUNT *m = &g_mounts[i];

        if (!m->used)
            continue;

        UINTN k = 0;
        BOOLEAN same = TRUE;

        for (; k < len; k++) {
            char a = m->name[k], b = name[k];
            if (a >= 'A' && a <= 'Z') a = (char)(a - 'A' + 'a');
            if (b >= 'A' && b <= 'Z') b = (char)(b - 'A' + 'a');
            if (a != b) {
                same = FALSE;
                break;
            }
        }

        if (same && m->name[len] == '\0')
            return (INTN)i;
    }

    return -1;
}

const char *vfs_strerror(INTN e)
{
    switch (e) {
    case VFS_OK:        return "ok";
    case VFS_ENOENT:    return "no such file or folder";
    case VFS_EEXIST:    return "already exists";
    case VFS_ENOTDIR:   return "not a folder";
    case VFS_EISDIR:    return "is a folder";
    case VFS_ENOTEMPTY: return "the folder is not empty";
    case VFS_ENOSPC:    return "no space left";
    case VFS_EROFS:     return "read-only (this disk is not written by MyOS)";
    case VFS_EIO:       return "disk error";
    case VFS_EINVAL:    return "bad name or argument";
    case VFS_EBADF:     return "bad file handle";
    case VFS_EMFILE:    return "too many open files";
    case VFS_ENOSYS:    return "this file system cannot do that (the RAM disk has no folders)";
    case VFS_EGONE:     return "the disk was removed";
    case VFS_EXDEV:     return "cannot move between disks this way";
    default:            return "error";
    }
}


/* ================================================================
 * Пути
 * ================================================================ */

/*
 * Путь -> полный и "чистый": "/usb0p1/docs/a.txt". Относительные -
 * от текущей папки; "." и ".." разбираются; лишние "/" убираются.
 */
INTN vfs_normalize(const char *path, char *out, UINTN cap)
{
    char tmp[VFS_PATH_MAX * 2];
    UINTN t = 0;

    if (path[0] != '/') {
        for (UINTN i = 0; g_cwd[i] && t + 1 < sizeof(tmp); i++)
            tmp[t++] = g_cwd[i];
        if (t + 1 < sizeof(tmp))
            tmp[t++] = '/';
    }

    for (UINTN i = 0; path[i] && t + 1 < sizeof(tmp); i++)
        tmp[t++] = path[i];

    tmp[t] = '\0';

    /* собираем по частям; ".." стирает последнюю */
    UINTN o = 0;

    out[o++] = '/';
    out[o] = '\0';

    UINTN i = 0;

    while (tmp[i]) {

        while (tmp[i] == '/')
            i++;

        if (!tmp[i])
            break;

        UINTN s = i;

        while (tmp[i] && tmp[i] != '/')
            i++;

        UINTN len = i - s;

        if (len == 1 && tmp[s] == '.')
            continue;

        if (len == 2 && tmp[s] == '.' && tmp[s + 1] == '.') {
            if (o > 1) {
                o--;                       /* убрать '/' в конце */
                while (o > 0 && out[o - 1] != '/')
                    o--;
                out[o] = '\0';
            }
            continue;
        }

        if (o + len + 2 >= cap || len >= VFS_NAME_MAX)
            return VFS_EINVAL;

        for (UINTN k = 0; k < len; k++)
            out[o++] = tmp[s + k];

        out[o++] = '/';
        out[o] = '\0';
    }

    if (o > 1)
        out[--o] = '\0';                   /* без '/' в конце */

    return VFS_OK;
}

/*
 * Разобрать путь: том, узел, его папка и последнее имя.
 *   *mount = -1 - это корень "/".
 * Если последнего элемента нет, но папка есть, - ENOENT, а parent и
 * leaf заполнены (так vfs_open и vfs_mkdir создают новое).
 */
static INTN vfs_resolve(const char *path, INTN *mount, VFS_NODE *node,
                        VFS_NODE *parent, char *leaf)
{
    char norm[VFS_PATH_MAX];
    INTN r = vfs_normalize(path, norm, sizeof(norm));

    if (r != VFS_OK)
        return r;

    if (leaf)
        leaf[0] = '\0';

    if (norm[1] == '\0') {
        *mount = -1;
        if (node) {
            raw_zero_mem((volatile UINT8 *)node, sizeof(*node));
            node->is_dir = TRUE;
            node->is_root = TRUE;
        }
        return VFS_OK;
    }

    /* имя тома */
    UINTN i = 1, s = 1;

    while (norm[i] && norm[i] != '/')
        i++;

    INTN mi = mount_find(norm + s, i - s);

    if (mi < 0)
        return VFS_ENOENT;

    VFS_MOUNT *m = &g_mounts[mi];
    VFS_NODE cur, par;

    *mount = mi;

    r = m->ops->root(m, &cur);

    if (r != VFS_OK)
        return r;

    par = cur;

    while (norm[i] == '/') {

        i++;
        s = i;

        while (norm[i] && norm[i] != '/')
            i++;

        char name[VFS_NAME_MAX];
        UINTN len = i - s;

        for (UINTN k = 0; k < len; k++)
            name[k] = norm[s + k];

        name[len] = '\0';

        if (!cur.is_dir)
            return VFS_ENOTDIR;

        par = cur;

        r = m->ops->lookup(m, &par, name, &cur);

        if (r != VFS_OK) {
            if (r == VFS_ENOENT && norm[i] == '\0') {
                /* нет только последнего - сообщаем папку и имя */
                if (parent)
                    *parent = par;
                if (leaf)
                    for (UINTN k = 0; k <= len; k++)
                        leaf[k] = name[k];
            } else if (leaf) {
                leaf[0] = '\0';           /* не хватает папки в середине */
            }
            return r;
        }

        if (leaf)
            for (UINTN k = 0; k <= len; k++)
                leaf[k] = name[k];
    }

    if (node)
        *node = cur;
    if (parent)
        *parent = par;

    return VFS_OK;
}


/* ================================================================
 * Операции с путями
 * ================================================================ */

INTN vfs_stat(const char *path, VFS_DIRENT *out)
{
    blk_sync_usb();
    kmutex_lock(&g_vfs_mutex);

    INTN mi;
    char leaf[VFS_NAME_MAX];
    INTN r = vfs_resolve(path, &mi, &out->node, NULL, leaf);

    if (r == VFS_OK) {
        UINTN k = 0;
        for (; leaf[k] && k + 1 < VFS_NAME_MAX; k++)
            out->name[k] = leaf[k];
        out->name[k] = '\0';
        if (mi >= 0 && leaf[0] == '\0') {
            /* корень тома: имя - как у тома */
            for (k = 0; g_mounts[mi].name[k]; k++)
                out->name[k] = g_mounts[mi].name[k];
            out->name[k] = '\0';
        }
    }

    kmutex_unlock(&g_vfs_mutex);
    return r;
}

INTN vfs_list(const char *path, INTN (*cb)(void *ctx, const VFS_DIRENT *e), void *ctx)
{
    blk_sync_usb();
    kmutex_lock(&g_vfs_mutex);

    INTN mi;
    VFS_NODE dir;
    INTN r = vfs_resolve(path, &mi, &dir, NULL, NULL);

    if (r != VFS_OK) {
        kmutex_unlock(&g_vfs_mutex);
        return r;
    }

    if (!dir.is_dir) {
        kmutex_unlock(&g_vfs_mutex);
        return VFS_ENOTDIR;
    }

    VFS_DIRENT *de = (VFS_DIRENT *)kmalloc(sizeof(VFS_DIRENT));

    if (de == NULL) {
        kmutex_unlock(&g_vfs_mutex);
        return VFS_EIO;
    }

    if (mi < 0) {

        /* корень: тома */
        for (UINTN i = 0; i < VFS_MAX_MOUNTS; i++) {

            if (!g_mounts[i].used)
                continue;

            UINTN k = 0;

            for (; g_mounts[i].name[k]; k++)
                de->name[k] = g_mounts[i].name[k];
            de->name[k] = '\0';

            raw_zero_mem((volatile UINT8 *)&de->node, sizeof(de->node));
            de->node.is_dir = TRUE;
            de->node.is_root = TRUE;

            if (cb(ctx, de) != 0)
                break;
        }

        kfree(de);
        kmutex_unlock(&g_vfs_mutex);
        return VFS_OK;
    }

    VFS_MOUNT *m = &g_mounts[mi];
    UINT64 cookie = 0;

    for (;;) {

        r = m->ops->readdir(m, &dir, &cookie, de);

        if (r <= 0)
            break;

        if (cb(ctx, de) != 0) {
            r = 0;
            break;
        }
    }

    kfree(de);
    kmutex_unlock(&g_vfs_mutex);

    return (r < 0) ? r : VFS_OK;
}

INTN vfs_open(const char *path, UINT32 flags)
{
    blk_sync_usb();
    kmutex_lock(&g_vfs_mutex);

    INTN fd = -1;

    for (UINTN i = 0; i < VFS_MAX_FD; i++)
        if (!g_fds[i].used) {
            fd = (INTN)i;
            break;
        }

    if (fd < 0) {
        kmutex_unlock(&g_vfs_mutex);
        return VFS_EMFILE;
    }

    INTN mi;
    VFS_NODE node, parent;
    char leaf[VFS_NAME_MAX];
    BOOLEAN wr = (flags & (VFS_O_WRITE | VFS_O_APPEND | VFS_O_TRUNC)) != 0;
    INTN r = vfs_resolve(path, &mi, &node, &parent, leaf);

    if (r == VFS_ENOENT && (flags & VFS_O_CREATE) && leaf[0] != '\0' && mi >= 0) {

        VFS_MOUNT *m = &g_mounts[mi];

        if (m->readonly) {
            r = VFS_EROFS;
        } else {
            r = m->ops->create(m, &parent, leaf, FALSE, &node);
        }

    } else if (r == VFS_OK) {

        if (mi < 0 || node.is_dir) {
            r = VFS_EISDIR;
        } else if (wr && g_mounts[mi].readonly) {
            r = VFS_EROFS;
        } else if (flags & VFS_O_TRUNC) {
            r = g_mounts[mi].ops->truncate(&g_mounts[mi], &node, 0);
        }
    }

    if (r != VFS_OK) {
        kmutex_unlock(&g_vfs_mutex);
        return r;
    }

    VFS_FD *f = &g_fds[fd];

    f->used = TRUE;
    f->gone = FALSE;
    f->mount = mi;
    f->node = node;
    f->flags = flags;
    f->pos = (flags & VFS_O_APPEND) ? node.size : 0;

    kmutex_unlock(&g_vfs_mutex);
    return fd;
}

static VFS_FD *fd_get(INTN fd)
{
    if (fd < 0 || fd >= VFS_MAX_FD || !g_fds[fd].used)
        return NULL;

    return &g_fds[fd];
}

INTN vfs_read(INTN fd, VOID *buf, UINTN n)
{
    kmutex_lock(&g_vfs_mutex);

    VFS_FD *f = fd_get(fd);
    INTN r;

    if (f == NULL)
        r = VFS_EBADF;
    else if (f->gone)
        r = VFS_EGONE;
    else {
        VFS_MOUNT *m = &g_mounts[f->mount];
        r = m->ops->read(m, &f->node, f->pos, buf, n);
        if (r > 0)
            f->pos += (UINT64)r;
    }

    kmutex_unlock(&g_vfs_mutex);
    return r;
}

INTN vfs_write(INTN fd, const VOID *buf, UINTN n)
{
    kmutex_lock(&g_vfs_mutex);

    VFS_FD *f = fd_get(fd);
    INTN r;

    if (f == NULL)
        r = VFS_EBADF;
    else if (f->gone)
        r = VFS_EGONE;
    else if (!(f->flags & (VFS_O_WRITE | VFS_O_APPEND)))
        r = VFS_EBADF;
    else {
        VFS_MOUNT *m = &g_mounts[f->mount];
        if (f->flags & VFS_O_APPEND)
            f->pos = f->node.size;
        r = m->ops->write(m, &f->node, f->pos, buf, n);
        if (r > 0)
            f->pos += (UINT64)r;
    }

    kmutex_unlock(&g_vfs_mutex);
    return r;
}

INTN vfs_size(INTN fd, UINT64 *size)
{
    kmutex_lock(&g_vfs_mutex);

    VFS_FD *f = fd_get(fd);

    if (f != NULL)
        *size = f->node.size;

    kmutex_unlock(&g_vfs_mutex);
    return f ? VFS_OK : VFS_EBADF;
}

/*
 * Сдвинуть место чтения/записи (lseek в POSIX): whence 0 - от начала,
 * 1 - от текущего места, 2 - от конца файла. Дальше конца не пускаем:
 * "дыры" в файле (запись далеко за концом) драйверы FAT не умеют, а
 * программам, которые мы переносим (libc, браузер), хватает этого.
 */
INTN vfs_seek(INTN fd, INT64 off, UINT32 whence, UINT64 *newpos)
{
    kmutex_lock(&g_vfs_mutex);

    VFS_FD *f = fd_get(fd);
    INTN r = VFS_OK;

    if (f == NULL)
        r = VFS_EBADF;
    else if (f->gone)
        r = VFS_EGONE;
    else {
        INT64 base = (whence == 0) ? 0 :
                     (whence == 1) ? (INT64)f->pos :
                     (whence == 2) ? (INT64)f->node.size : -1;
        INT64 np = base + off;

        if (base < 0 || np < 0 || (UINT64)np > f->node.size)
            r = VFS_EINVAL;
        else {
            f->pos = (UINT64)np;
            *newpos = f->pos;
        }
    }

    kmutex_unlock(&g_vfs_mutex);
    return r;
}

INTN vfs_close(INTN fd)
{
    kmutex_lock(&g_vfs_mutex);

    VFS_FD *f = fd_get(fd);

    if (f != NULL) {
        VFS_MOUNT *m = (f->mount >= 0) ? &g_mounts[f->mount] : NULL;
        if (!f->gone && m != NULL && m->used && m->ops->close != NULL)
            m->ops->close(m, &f->node);
        f->used = FALSE;
    }

    kmutex_unlock(&g_vfs_mutex);
    return f ? VFS_OK : VFS_EBADF;
}

/* Том на диске (FAT или exFAT), а не RAM-диск или /bin */
BOOLEAN vfs_is_disk(const VFS_MOUNT *m)
{
    return m->ops == &g_fat_ops || m->ops == &g_exfat_ops;
}

/* "FAT32", "exFAT", "bin", "ram" - для ls / и df */
void vfs_fs_name(const VFS_MOUNT *m, char *buf, UINTN cap)
{
    if (m->ops == &g_fat_ops)
        ksnprintf(buf, cap, "FAT%u", m->fat.fat_bits);
    else if (m->ops == &g_exfat_ops)
        ksnprintf(buf, cap, "exFAT");
    else if (m->ops == &g_bin_ops)
        ksnprintf(buf, cap, "bin");
    else
        ksnprintf(buf, cap, "ram");
}

INTN vfs_mkdir(const char *path)
{
    blk_sync_usb();
    kmutex_lock(&g_vfs_mutex);

    INTN mi;
    VFS_NODE node, parent;
    char leaf[VFS_NAME_MAX];
    INTN r = vfs_resolve(path, &mi, &node, &parent, leaf);

    if (r == VFS_OK)
        r = VFS_EEXIST;
    else if (r == VFS_ENOENT && leaf[0] != '\0' && mi >= 0) {
        VFS_MOUNT *m = &g_mounts[mi];
        r = m->readonly ? VFS_EROFS : m->ops->create(m, &parent, leaf, TRUE, &node);
    }

    kmutex_unlock(&g_vfs_mutex);
    return r;
}

INTN vfs_remove(const char *path)
{
    blk_sync_usb();
    kmutex_lock(&g_vfs_mutex);

    INTN mi;
    VFS_NODE node, parent;
    INTN r = vfs_resolve(path, &mi, &node, &parent, NULL);

    if (r == VFS_OK) {
        if (mi < 0 || node.is_root)
            r = VFS_EINVAL;                /* корень и тома не удаляются */
        else if (g_mounts[mi].readonly)
            r = VFS_EROFS;
        else
            r = g_mounts[mi].ops->remove(&g_mounts[mi], &parent, &node);
    }

    kmutex_unlock(&g_vfs_mutex);
    return r;
}

/*
 * Переименовать или перенести (в пределах одного тома). Если to -
 * существующая папка, from переезжает внутрь неё с прежним именем.
 */
INTN vfs_rename(const char *from, const char *to)
{
    blk_sync_usb();
    kmutex_lock(&g_vfs_mutex);

    INTN m1, m2;
    VFS_NODE node, parent, tnode, tparent;
    char leaf[VFS_NAME_MAX], tleaf[VFS_NAME_MAX];
    INTN r = vfs_resolve(from, &m1, &node, &parent, leaf);

    if (r != VFS_OK)
        goto out;

    if (m1 < 0 || node.is_root) {
        r = VFS_EINVAL;
        goto out;
    }

    r = vfs_resolve(to, &m2, &tnode, &tparent, tleaf);

    if (r == VFS_OK && tnode.is_dir) {

        /* внутрь папки, с прежним именем */
        if (m2 < 0) {
            r = VFS_EINVAL;
            goto out;
        }

        tparent = tnode;

        UINTN k = 0;
        for (; leaf[k]; k++)
            tleaf[k] = leaf[k];
        tleaf[k] = '\0';

        VFS_NODE dummy;
        VFS_MOUNT *mm = &g_mounts[m2];

        r = mm->ops->lookup(mm, &tparent, tleaf, &dummy);

        if (r == VFS_OK) {
            r = VFS_EEXIST;
            goto out;
        }

    } else if (r == VFS_OK) {
        r = VFS_EEXIST;
        goto out;
    } else if (r != VFS_ENOENT || tleaf[0] == '\0' || m2 < 0) {
        goto out;
    }

    if (m1 != m2) {
        r = VFS_EXDEV;
        goto out;
    }

    /* папку внутрь самой себя - нельзя */
    {
        char a[VFS_PATH_MAX], b[VFS_PATH_MAX];
        vfs_normalize(from, a, sizeof(a));
        vfs_normalize(to, b, sizeof(b));
        UINTN n = 0;
        while (a[n] && a[n] == b[n])
            n++;
        if (node.is_dir && a[n] == '\0' && (b[n] == '/' || b[n] == '\0')) {
            r = VFS_EINVAL;
            goto out;
        }
    }

    if (g_mounts[m1].readonly) {
        r = VFS_EROFS;
        goto out;
    }

    r = g_mounts[m1].ops->rename(&g_mounts[m1], &parent, &node, &tparent, tleaf);

out:
    kmutex_unlock(&g_vfs_mutex);
    return r;
}

INTN vfs_chdir(const char *path)
{
    blk_sync_usb();
    kmutex_lock(&g_vfs_mutex);

    char norm[VFS_PATH_MAX];
    INTN mi;
    VFS_NODE node;
    INTN r = vfs_normalize(path, norm, sizeof(norm));

    if (r == VFS_OK)
        r = vfs_resolve(norm, &mi, &node, NULL, NULL);

    if (r == VFS_OK && !node.is_dir)
        r = VFS_ENOTDIR;

    if (r == VFS_OK) {
        UINTN k = 0;
        for (; norm[k] && k + 1 < VFS_PATH_MAX; k++)
            g_cwd[k] = norm[k];
        g_cwd[k] = '\0';
    }

    kmutex_unlock(&g_vfs_mutex);
    return r;
}

INTN vfs_mount_info(const char *path, VFS_MOUNT **mo)
{
    kmutex_lock(&g_vfs_mutex);

    INTN mi;
    VFS_NODE node;
    INTN r = vfs_resolve(path, &mi, &node, NULL, NULL);

    if (r == VFS_OK && mi < 0)
        r = VFS_EINVAL;

    if (r == VFS_OK)
        *mo = &g_mounts[mi];

    kmutex_unlock(&g_vfs_mutex);
    return r;
}

/* Прочитать файл целиком (не больше cap байт) */
INTN vfs_read_file(const char *path, VOID *buf, UINTN cap, UINTN *got)
{
    INTN fd = vfs_open(path, VFS_O_READ);

    *got = 0;

    if (fd < 0)
        return fd;

    INTN r = VFS_OK;

    while (*got < cap) {
        r = vfs_read(fd, (UINT8 *)buf + *got, cap - *got);
        if (r <= 0)
            break;
        *got += (UINTN)r;
    }

    vfs_close(fd);

    return (r < 0) ? r : VFS_OK;
}

/* Записать файл целиком (или дописать в конец) */
INTN vfs_write_file(const char *path, const VOID *buf, UINTN n, BOOLEAN append)
{
    INTN fd = vfs_open(path, VFS_O_WRITE | VFS_O_CREATE |
                             (append ? VFS_O_APPEND : VFS_O_TRUNC));

    if (fd < 0)
        return fd;

    UINTN done = 0;
    INTN r = VFS_OK;

    while (done < n) {
        r = vfs_write(fd, (const UINT8 *)buf + done, n - done);
        if (r <= 0)
            break;
        done += (UINTN)r;
    }

    vfs_close(fd);

    if (r < 0)
        return r;

    return (done == n) ? VFS_OK : VFS_ENOSPC;
}
