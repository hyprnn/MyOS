/*
 * fs/exfat.c - тома exFAT (чтение и запись). Часть MyOS; общие
 * объявления - в myos.h.
 *
 * exFAT - файловая система почти всех флешек больше 32 ГБ и карт
 * SDXC; ею же размечает флешку Ventoy (большой раздел с образами).
 * У exFAT другая раскладка, чем у FAT16/32: битовая карта свободных
 * кластеров, "наборы записей" каталога с контрольными суммами,
 * таблица регистров для имён. Писать её с нуля долго и рискованно
 * (испорченная флешка!), поэтому здесь - проверенная библиотека FatFs
 * (ooFatFs R0.13c, BSD-1-clause, third_party/fatfs), а этот файл -
 * прослойка между ней и VFS MyOS:
 *
 *   * диск: disk_read/disk_write FatFs -> blk_read/blk_write MyOS
 *     (раздел уже выделен блочным слоем, кэш секторов - там же);
 *   * FatFs работает с ПУТЯМИ ("/iso/arch.iso"), а VFS MyOS - с
 *     узлами: поэтому узел exFAT хранит свой путь внутри тома
 *     (VFS_NODE.fpath);
 *   * чтобы не искать файл по пути на каждый read/write, последний
 *     открытый файл тома держится открытым (FIL) и закрывается при
 *     vfs_close или любой другой операции с томом;
 *   * вызовы идут под g_vfs_mutex - FatFs сама замков не берёт.
 */
#include "myos.h"
#include "../third_party/fatfs/ff.h"
#include "../third_party/fatfs/diskio.h"

typedef struct {
    FATFS   fs;
    UINTN   dev;                      /* блочное устройство */
    BOOLEAN writable;
    /* открытый сейчас файл (см. выше) */
    FIL     fil;
    BOOLEAN fil_open;
    BOOLEAN fil_write;
    char    fil_path[VFS_PATH_MAX];
    /* чтение каталога по частям: где остановились */
    FF_DIR  dir;
    BOOLEAN dir_open;
    char    dir_path[VFS_PATH_MAX];
    UINT64  dir_next;
} XVOL;

/* ================================================================
 * Диск для FatFs
 * ================================================================ */

DRESULT disk_read(void *drv, BYTE *buff, DWORD sector, UINT count)
{
    XVOL *v = (XVOL *)drv;

    return blk_read(v->dev, sector, count, buff) ? RES_OK : RES_ERROR;
}

DRESULT disk_write(void *drv, const BYTE *buff, DWORD sector, UINT count)
{
    XVOL *v = (XVOL *)drv;

    if (!v->writable)
        return RES_WRPRT;

    return blk_write(v->dev, sector, count, buff) ? RES_OK : RES_ERROR;
}

DRESULT disk_ioctl(void *drv, BYTE cmd, void *buff)
{
    XVOL *v = (XVOL *)drv;

    switch (cmd) {
    case IOCTL_INIT:
    case IOCTL_STATUS:
        *(DSTATUS *)buff = v->writable ? 0 : STA_PROTECT;
        return RES_OK;
    case CTRL_SYNC:
        return RES_OK;                /* запись у нас сквозная */
    case GET_SECTOR_COUNT:
        *(DWORD *)buff = (DWORD)g_blk[v->dev].sectors;
        return RES_OK;
    case GET_SECTOR_SIZE:
        *(WORD *)buff = 512;
        return RES_OK;
    case GET_BLOCK_SIZE:
        *(DWORD *)buff = 1;
        return RES_OK;
    default:
        return RES_PARERR;
    }
}

/* Время изменения файлов: местное (как пишут FAT и Windows) */
DWORD get_fattime(void)
{
    EFI_TIME t;

    if (!rtc_read(&t))
        return ((DWORD)(2026 - 1980) << 25) | (1u << 21) | (1u << 16);

    tz_to_local(&t);

    return ((DWORD)(t.Year - 1980) << 25) | ((DWORD)t.Month << 21) | ((DWORD)t.Day << 16) |
           ((DWORD)t.Hour << 11) | ((DWORD)t.Minute << 5) | ((DWORD)t.Second >> 1);
}

void *ff_memalloc(UINT msize)
{
    return kmalloc(msize);
}

void ff_memfree(void *mblock)
{
    kfree(mblock);
}

/* ================================================================
 * Помощники
 * ================================================================ */

static INTN fr_to_vfs(FRESULT r)
{
    switch (r) {
    case FR_OK:                 return VFS_OK;
    case FR_NO_FILE:
    case FR_NO_PATH:            return VFS_ENOENT;
    case FR_EXIST:              return VFS_EEXIST;
    case FR_DENIED:             return VFS_ENOSPC;     /* полон (или папка не пуста) */
    case FR_WRITE_PROTECTED:    return VFS_EROFS;
    case FR_INVALID_NAME:
    case FR_INVALID_PARAMETER:  return VFS_EINVAL;
    case FR_TOO_MANY_OPEN_FILES:return VFS_EMFILE;
    default:                    return VFS_EIO;
    }
}

static XVOL *xv(VFS_MOUNT *m)
{
    return (XVOL *)m->xfs;
}

static void path_join(char *out, UINTN cap, const char *dir, const char *name)
{
    if (dir[0] == '/' && dir[1] == '\0')
        ksnprintf(out, cap, "/%s", name);
    else
        ksnprintf(out, cap, "%s/%s", dir, name);
}

static void copy_str(char *d, UINTN cap, const char *s)
{
    UINTN i = 0;

    for (; s[i] && i + 1 < cap; i++)
        d[i] = s[i];

    d[i] = '\0';
}

/* Закрыть кэшированные файл и каталог (перед любой другой операцией) */
static void x_flush(XVOL *v)
{
    if (v->fil_open) {
        f_close(&v->fil);
        v->fil_open = FALSE;
    }

    if (v->dir_open) {
        f_closedir(&v->dir);
        v->dir_open = FALSE;
    }
}

/* Открыть файл (или взять уже открытый) */
static INTN x_file(XVOL *v, const char *path, BOOLEAN write)
{
    if (v->fil_open && kstreq(v->fil_path, path) && (v->fil_write || !write))
        return VFS_OK;

    x_flush(v);

    FRESULT r = f_open(&v->fs, &v->fil, path, write ? (FA_READ | FA_WRITE) : FA_READ);

    if (r != FR_OK)
        return fr_to_vfs(r);

    v->fil_open = TRUE;
    v->fil_write = write;
    copy_str(v->fil_path, sizeof(v->fil_path), path);

    return VFS_OK;
}

static void fill_node(VFS_NODE *n, const char *path, const FILINFO *fi)
{
    raw_zero_mem((volatile UINT8 *)n, sizeof(*n));
    n->is_dir = (fi->fattrib & AM_DIR) != 0;
    n->size = fi->fsize;
    n->attr = fi->fattrib;
    n->wdate = fi->fdate;
    n->wtime = fi->ftime;
    n->ram_index = -1;
    copy_str(n->fpath, sizeof(n->fpath), path);
}

/* ================================================================
 * Операции VFS
 * ================================================================ */

static INTN x_root(VFS_MOUNT *m, VFS_NODE *out)
{
    (void)m;
    raw_zero_mem((volatile UINT8 *)out, sizeof(*out));
    out->is_dir = TRUE;
    out->is_root = TRUE;
    out->ram_index = -1;
    out->fpath[0] = '/';
    out->fpath[1] = '\0';
    return VFS_OK;
}

static INTN x_readdir(VFS_MOUNT *m, VFS_NODE *dir, UINT64 *cookie, VFS_DIRENT *out)
{
    XVOL *v = xv(m);
    FILINFO fi;

    /* продолжить с места, где остановились, или открыть заново и
       пропустить *cookie записей */
    if (!(v->dir_open && kstreq(v->dir_path, dir->fpath) && v->dir_next == *cookie)) {

        x_flush(v);

        FRESULT r = f_opendir(&v->fs, &v->dir, dir->fpath);

        if (r != FR_OK)
            return fr_to_vfs(r);

        v->dir_open = TRUE;
        v->dir_next = 0;
        copy_str(v->dir_path, sizeof(v->dir_path), dir->fpath);

        while (v->dir_next < *cookie) {
            if (f_readdir(&v->dir, &fi) != FR_OK || fi.fname[0] == '\0')
                return 0;
            v->dir_next++;
        }
    }

    FRESULT r = f_readdir(&v->dir, &fi);

    if (r != FR_OK)
        return fr_to_vfs(r);

    if (fi.fname[0] == '\0')
        return 0;                              /* конец */

    v->dir_next++;
    *cookie = v->dir_next;

    char path[VFS_PATH_MAX];
    path_join(path, sizeof(path), dir->fpath, fi.fname);
    fill_node(&out->node, path, &fi);
    copy_str(out->name, sizeof(out->name), fi.fname);

    return 1;
}

static INTN x_lookup(VFS_MOUNT *m, VFS_NODE *dir, const char *name, VFS_NODE *out)
{
    XVOL *v = xv(m);
    char path[VFS_PATH_MAX];
    FILINFO fi;

    path_join(path, sizeof(path), dir->fpath, name);

    /* открытый на запись файл: размер в каталоге ещё старый */
    if (v->fil_open && v->fil_write && kstreq(v->fil_path, path))
        f_sync(&v->fil);

    FRESULT r = f_stat(&v->fs, path, &fi);

    if (r != FR_OK)
        return fr_to_vfs(r);

    fill_node(out, path, &fi);

    /* имя - как на диске (регистр букв) */
    return VFS_OK;
}

static INTN x_read(VFS_MOUNT *m, VFS_NODE *f, UINT64 off, VOID *buf, UINTN n)
{
    XVOL *v = xv(m);
    INTN r = x_file(v, f->fpath, FALSE);

    if (r != VFS_OK)
        return r;

    if (f_lseek(&v->fil, off) != FR_OK)
        return VFS_EIO;

    UINT got = 0;
    FRESULT fr = f_read(&v->fil, buf, (UINT)n, &got);

    return (fr == FR_OK) ? (INTN)got : fr_to_vfs(fr);
}

static INTN x_write(VFS_MOUNT *m, VFS_NODE *f, UINT64 off, const VOID *buf, UINTN n)
{
    XVOL *v = xv(m);
    INTN r = x_file(v, f->fpath, TRUE);

    if (r != VFS_OK)
        return r;

    if (f_lseek(&v->fil, off) != FR_OK)
        return VFS_EIO;

    UINT put = 0;
    FRESULT fr = f_write(&v->fil, buf, (UINT)n, &put);

    if (fr != FR_OK)
        return fr_to_vfs(fr);

    if (off + put > f->size)
        f->size = off + put;

    if (put < n)
        return put ? (INTN)put : VFS_ENOSPC;

    return (INTN)put;
}

static INTN x_truncate(VFS_MOUNT *m, VFS_NODE *f, UINT64 size)
{
    XVOL *v = xv(m);
    INTN r = x_file(v, f->fpath, TRUE);

    if (r != VFS_OK)
        return r;

    if (f_lseek(&v->fil, size) != FR_OK || f_truncate(&v->fil) != FR_OK)
        return VFS_EIO;

    f_sync(&v->fil);
    f->size = size;

    return VFS_OK;
}

static INTN x_create(VFS_MOUNT *m, VFS_NODE *dir, const char *name, BOOLEAN is_dir,
                     VFS_NODE *out)
{
    XVOL *v = xv(m);
    char path[VFS_PATH_MAX];
    FRESULT r;

    x_flush(v);
    path_join(path, sizeof(path), dir->fpath, name);

    if (is_dir) {
        r = f_mkdir(&v->fs, path);
    } else {
        r = f_open(&v->fs, &v->fil, path, FA_WRITE | FA_CREATE_NEW);
        if (r == FR_OK)
            r = f_close(&v->fil);
    }

    if (r != FR_OK)
        return fr_to_vfs(r);

    FILINFO fi;

    if (f_stat(&v->fs, path, &fi) != FR_OK)
        return VFS_EIO;

    fill_node(out, path, &fi);
    return VFS_OK;
}

static INTN x_remove(VFS_MOUNT *m, VFS_NODE *dir, VFS_NODE *node)
{
    XVOL *v = xv(m);

    (void)dir;
    x_flush(v);

    FRESULT r = f_unlink(&v->fs, node->fpath);

    if (r == FR_DENIED && node->is_dir)
        return VFS_ENOTEMPTY;

    return fr_to_vfs(r);
}

static INTN x_rename(VFS_MOUNT *m, VFS_NODE *dir, VFS_NODE *node, VFS_NODE *newdir,
                     const char *newname)
{
    XVOL *v = xv(m);
    char to[VFS_PATH_MAX];

    (void)dir;
    x_flush(v);
    path_join(to, sizeof(to), newdir->fpath, newname);

    return fr_to_vfs(f_rename(&v->fs, node->fpath, to));
}

static INTN x_statfs(VFS_MOUNT *m, UINT64 *total, UINT64 *free_bytes)
{
    XVOL *v = xv(m);
    DWORD nfree = 0;
    UINT64 cl = (UINT64)v->fs.csize * 512u;

    *total = (UINT64)(v->fs.n_fatent - 2u) * cl;
    *free_bytes = (f_getfree(&v->fs, &nfree) == FR_OK) ? (UINT64)nfree * cl : 0;

    return VFS_OK;
}

/* Программа закрыла файл: дописать всё на диск (размер, время) */
static INTN x_close(VFS_MOUNT *m, VFS_NODE *f)
{
    XVOL *v = xv(m);

    if (v->fil_open && kstreq(v->fil_path, f->fpath)) {
        f_close(&v->fil);
        v->fil_open = FALSE;
    }

    return VFS_OK;
}

const VFS_OPS g_exfat_ops = {
    "exFAT", x_root, x_readdir, x_lookup, x_read, x_write, x_truncate,
    x_create, x_remove, x_rename, x_statfs, x_close
};

/* ================================================================
 * Монтирование
 * ================================================================ */

/* Похоже ли на exFAT: "EXFAT   " по смещению 3 загрузочного сектора */
BOOLEAN exfat_detect(UINTN dev)
{
    static UINT8 bs[512];

    if (!blk_read(dev, 0, 1, bs))
        return FALSE;

    return memcmp(bs + 3, "EXFAT   ", 8) == 0 && bs[510] == 0x55 && bs[511] == 0xAA;
}

/* Смонтировать exFAT (под g_vfs_mutex). TRUE - том готов */
BOOLEAN exfat_mount(UINTN dev, VFS_MOUNT *m)
{
    if (!exfat_detect(dev))
        return FALSE;

    XVOL *v = (XVOL *)kzalloc(sizeof(XVOL));

    if (v == NULL)
        return FALSE;

    v->dev = dev;
    v->writable = g_blk[dev].writable;
    v->fs.drv = v;

    FRESULT r = f_mount(&v->fs);

    if (r != FR_OK || v->fs.fs_type != FS_EXFAT) {
        klog("exfat: %s: mount failed (FatFs result %u)\n", g_blk[dev].name, (UINT32)r);
        kfree(v);
        return FALSE;
    }

    m->xfs = v;
    m->ops = &g_exfat_ops;

    /* метка тома (UTF-8) - в общее поле, для ls / */
    char label[40];
    DWORD vsn = 0;

    m->fat.label[0] = '\0';
    if (f_getlabel(&v->fs, label, &vsn) == FR_OK)
        copy_str(m->fat.label, sizeof(m->fat.label), label);

    m->fat.cluster_bytes = (UINT32)v->fs.csize * 512u;
    m->fat.clusters = v->fs.n_fatent - 2u;

    return TRUE;
}

/* Том пропал или размонтирован */
void exfat_release(VFS_MOUNT *m)
{
    if (m->xfs != NULL) {
        kfree(m->xfs);
        m->xfs = NULL;
    }
}
