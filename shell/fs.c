/*
 * shell/fs.c - команды файлов и папок: ls, cd, pwd, mkdir, cat,
 * write, cp, mv, rm, df... (с этапа 5 - через VFS, fs/vfs.c).
 * Часть MyOS; общие объявления - в myos.h.
 *
 * Пути - как в Linux: "/" - корень, в нём тома (ram - RAM-диск,
 * usb0p1 - флешка, sata0p1 / nvme0p1 - разделы внутреннего диска).
 * Относительный путь считается от текущей папки (cd, pwd); ".." -
 * папка выше. Имя с пробелами можно взять в кавычки: cat "my file".
 *
 * Сам RAM-диск (g_fs) по-прежнему живёт здесь: 32 файла без папок,
 * пропадают при перезагрузке. Его видит и терминал GUI.
 */
#include "myos.h"

FS_FILE g_fs[FS_MAX_FILES];


int fs_find_free(void)
{
    for (int i = 0; i < FS_MAX_FILES; i++)
        if (!g_fs[i].used)
            return i;

    return -1;
}


/* ================================================================
 * Разбор строки
 * ================================================================ */

#define FS_ARGS 4

typedef struct {
    char        buf[LINE_MAX];
    char       *argv[FS_ARGS];
    UINTN       argc;
    const char *rest;            /* всё после первого аргумента (для write) */
} FS_LINE;

/* Пропустить одно слово (с учётом кавычек) и пробелы после него */
static const char *fs_skip_word(const char *p)
{
    while (*p == ' ')
        p++;

    if (*p == '"') {
        p++;
        while (*p && *p != '"')
            p++;
        if (*p == '"')
            p++;
    } else {
        while (*p && *p != ' ')
            p++;
    }

    while (*p == ' ')
        p++;

    return p;
}

/* CHAR16 -> char (не-ASCII -> '?') и деление на слова; кавычки
   склеивают слова с пробелами. rest - текст после команды и первого
   аргумента, как есть (для write/append). */
static void fs_split(const CHAR16 *line, FS_LINE *l)
{
    static char orig[LINE_MAX];
    static char tail[LINE_MAX];
    UINTN n = 0;

    for (; line[n] != 0 && n + 1 < LINE_MAX; n++)
        orig[n] = l->buf[n] = (line[n] < 128) ? (char)line[n] : '?';

    orig[n] = l->buf[n] = '\0';

    /* хвост */
    const char *r = fs_skip_word(fs_skip_word(orig));
    UINTN k = 0;

    for (; r[k] && k + 1 < LINE_MAX; k++)
        tail[k] = r[k];

    while (k > 0 && tail[k - 1] == ' ')
        k--;

    tail[k] = '\0';
    l->rest = tail;

    if (k >= 2 && tail[0] == '"' && tail[k - 1] == '"') {
        tail[k - 1] = '\0';
        l->rest = tail + 1;
    }

    /* слова */
    l->argc = 0;

    char *p = l->buf;

    while (*p && l->argc < FS_ARGS) {

        while (*p == ' ')
            p++;

        if (!*p)
            break;

        char *start;

        if (*p == '"') {
            p++;
            start = p;
            while (*p && *p != '"')
                p++;
        } else {
            start = p;
            while (*p && *p != ' ')
                p++;
        }

        l->argv[l->argc++] = start;

        if (*p) {
            *p = '\0';
            p++;
        }
    }
}

static void fs_err(SIMPLE_TEXT_OUTPUT_INTERFACE *out, const char *what, const char *path, INTN e)
{
    kprintf(out, "%s %s: %s\n", what, path, vfs_strerror(e));
}

/* Последний элемент пути ("a/b/c.txt" -> "c.txt") */
static const char *fs_basename(const char *p)
{
    const char *b = p;

    for (; *p; p++)
        if (*p == '/' && p[1] != '\0')
            b = p + 1;

    return b;
}

static void fs_human(char *buf, UINTN cap, UINT64 bytes)
{
    if (bytes >= (10ull << 30))
        ksnprintf(buf, cap, "%llu GiB", bytes >> 30);
    else if (bytes >= (10ull << 20))
        ksnprintf(buf, cap, "%llu MiB", bytes >> 20);
    else if (bytes >= (10ull << 10))
        ksnprintf(buf, cap, "%llu KiB", bytes >> 10);
    else
        ksnprintf(buf, cap, "%llu B", bytes);
}


/* ================================================================
 * ls
 * ================================================================ */

typedef struct {
    SIMPLE_TEXT_OUTPUT_INTERFACE *out;
    UINTN files, dirs;
    UINT64 bytes;
} LS_CTX;

static INTN ls_cb(void *ctx, const VFS_DIRENT *e)
{
    LS_CTX *c = (LS_CTX *)ctx;
    const VFS_NODE *n = &e->node;
    char when[24];

    when[0] = '\0';

    if (n->wdate != 0)
        ksnprintf(when, sizeof(when), "%04u-%02u-%02u %02u:%02u",
                  1980u + (n->wdate >> 9), (n->wdate >> 5) & 0xFu, n->wdate & 0x1Fu,
                  n->wtime >> 11, (n->wtime >> 5) & 0x3Fu);

    if (n->is_dir) {
        kprintf(c->out, "  %-16s  %-10s  %s/\n", when, "<folder>", e->name);
        c->dirs++;
    } else {
        kprintf(c->out, "  %-16s  %10llu  %s\n", when, n->size, e->name);
        c->files++;
        c->bytes += n->size;
    }

    return 0;
}

static void fs_ls_root(SIMPLE_TEXT_OUTPUT_INTERFACE *out)
{
    print(out, "Volumes (cd into one of them):\n");

    kmutex_lock(&g_vfs_mutex);

    for (UINTN i = 0; i < VFS_MAX_MOUNTS; i++) {

        VFS_MOUNT *m = &g_mounts[i];

        if (!m->used)
            continue;

        UINT64 total = 0, freeb = 0;
        char t[16], f[16];

        m->ops->statfs(m, &total, &freeb);
        fs_human(t, sizeof(t), total);
        fs_human(f, sizeof(f), freeb);

        if (m->ops == &g_fat_ops) {
            BLKDEV *d = &g_blk[m->dev];
            kprintf(out, "  /%-9s FAT%u %-12s %9s, %9s free  %s \"%s\"%s\n",
                    m->name, m->fat.fat_bits,
                    m->fat.label[0] ? m->fat.label : "(no label)", t, f,
                    d->parent >= 0 ? g_blk[d->parent].kind : d->kind,
                    d->model, m->readonly ? "  READ-ONLY" : "");
        } else if (m->ops == &g_bin_ops) {
            kprintf(out, "  /%-9s programs built into MyOS (read-only), %s\n", m->name, t);
        } else {
            kprintf(out, "  /%-9s RAM disk: %u files max, cleared on reboot\n",
                    m->name, (UINT32)FS_MAX_FILES);
        }
    }

    kmutex_unlock(&g_vfs_mutex);
}

static void fs_ls(SIMPLE_TEXT_OUTPUT_INTERFACE *out, const char *path)
{
    char norm[VFS_PATH_MAX];

    if (vfs_normalize(path, norm, sizeof(norm)) != VFS_OK) {
        print(out, "Path is too long.\n");
        return;
    }

    blk_sync_usb();

    if (norm[1] == '\0') {
        fs_ls_root(out);
        return;
    }

    VFS_DIRENT st;
    INTN r = vfs_stat(norm, &st);

    if (r != VFS_OK) {
        fs_err(out, "ls", norm, r);
        return;
    }

    if (!st.node.is_dir) {
        LS_CTX c = { out, 0, 0, 0 };
        ls_cb(&c, &st);
        return;
    }

    LS_CTX c = { out, 0, 0, 0 };

    kprintf(out, "%s:\n", norm);

    r = vfs_list(norm, ls_cb, &c);

    if (r != VFS_OK)
        fs_err(out, "ls", norm, r);

    if (c.files == 0 && c.dirs == 0)
        print(out, "  (empty)\n");
    else
        kprintf(out, "  %u file(s), %llu bytes; %u folder(s)\n",
                (UINT32)c.files, c.bytes, (UINT32)c.dirs);
}


/* ================================================================
 * cat, write, cp
 * ================================================================ */

static void fs_cat(SIMPLE_TEXT_OUTPUT_INTERFACE *out, const char *path)
{
    INTN fd = vfs_open(path, VFS_O_READ);

    if (fd < 0) {
        fs_err(out, "cat", path, fd);
        return;
    }

    char buf[513];
    UINT64 total = 0;
    BOOLEAN binary = FALSE;
    char last = '\n';

    for (;;) {

        INTN r = vfs_read(fd, buf, 512);

        if (r < 0) {
            fs_err(out, "cat", path, r);
            break;
        }

        if (r == 0)
            break;

        for (INTN i = 0; i < r; i++) {
            UINT8 c = (UINT8)buf[i];
            if (c == '\r')
                c = ' ';
            if (c != '\n' && c != '\t' && (c < 32 || c > 126)) {
                if (c == 0 || c < 9)
                    binary = TRUE;
                c = '?';
            }
            buf[i] = (char)c;
        }

        buf[r] = '\0';
        print(out, buf);
        last = buf[r - 1];
        total += (UINT64)r;

        /* большой файл - только начало */
        if (total >= 64u * 1024u) {
            print(out, "\n... (only the first 64 KiB shown)\n");
            last = '\n';
            break;
        }
    }

    if (last != '\n')
        print(out, "\n");

    if (binary)
        print(out, "(this looks like a binary file, not text)\n");

    vfs_close(fd);
}

static void fs_write(SIMPLE_TEXT_OUTPUT_INTERFACE *out, const char *path,
                     const char *text, BOOLEAN append)
{
    char buf[LINE_MAX + 2];
    UINTN n = 0;

    for (; text[n] && n < LINE_MAX; n++)
        buf[n] = text[n];

    /* строка файла, как принято в Linux, кончается '\n' */
    buf[n++] = '\n';

    INTN r = vfs_write_file(path, buf, n, append);

    if (r != VFS_OK) {
        fs_err(out, append ? "append" : "write", path, r);
        return;
    }

    VFS_DIRENT st;

    if (vfs_stat(path, &st) == VFS_OK)
        kprintf(out, "OK - %s is now %llu bytes.\n", fs_basename(path), st.node.size);
}

/* Копировать файл. Если to - папка, внутрь неё с тем же именем. */
static INTN fs_copy(const char *from, const char *to, UINT64 *copied)
{
    char dst[VFS_PATH_MAX];
    VFS_DIRENT st;

    *copied = 0;

    if (vfs_stat(to, &st) == VFS_OK && st.node.is_dir)
        ksnprintf(dst, sizeof(dst), "%s/%s", to, fs_basename(from));
    else
        ksnprintf(dst, sizeof(dst), "%s", to);

    if (vfs_stat(from, &st) != VFS_OK)
        return VFS_ENOENT;

    if (st.node.is_dir)
        return VFS_EISDIR;

    VFS_DIRENT st2;

    if (vfs_stat(dst, &st2) == VFS_OK)
        return VFS_EEXIST;

    INTN in = vfs_open(from, VFS_O_READ);

    if (in < 0)
        return in;

    INTN o = vfs_open(dst, VFS_O_WRITE | VFS_O_CREATE | VFS_O_TRUNC);

    if (o < 0) {
        vfs_close(in);
        return o;
    }

    UINT8 *buf = (UINT8 *)kmalloc(16384);
    INTN r = VFS_OK;

    if (buf == NULL)
        r = VFS_EIO;

    while (r == VFS_OK) {

        INTN got = vfs_read(in, buf, 16384);

        if (got <= 0) {
            r = got;
            break;
        }

        INTN w = vfs_write(o, buf, (UINTN)got);

        if (w < 0) {
            r = w;
            break;
        }

        if (w < got) {
            r = VFS_ENOSPC;
            break;
        }

        *copied += (UINT64)w;
    }

    if (buf)
        kfree(buf);

    vfs_close(in);
    vfs_close(o);

    return r;
}


/* ================================================================
 * df
 * ================================================================ */

static void fs_df(SIMPLE_TEXT_OUTPUT_INTERFACE *out)
{
    blk_sync_usb();
    kmutex_lock(&g_vfs_mutex);

    print(out, "Volume      Type      Size        Free        Device\n");

    for (UINTN i = 0; i < VFS_MAX_MOUNTS; i++) {

        VFS_MOUNT *m = &g_mounts[i];

        if (!m->used)
            continue;

        UINT64 total = 0, freeb = 0;
        char t[16], f[16], ty[12];

        m->ops->statfs(m, &total, &freeb);
        fs_human(t, sizeof(t), total);
        fs_human(f, sizeof(f), freeb);

        if (m->ops == &g_fat_ops)
            ksnprintf(ty, sizeof(ty), "FAT%u", m->fat.fat_bits);
        else if (m->ops == &g_bin_ops)
            ksnprintf(ty, sizeof(ty), "bin");
        else
            ksnprintf(ty, sizeof(ty), "ram");

        kprintf(out, "/%-10s %-9s %-11s %-11s %s%s\n", m->name, ty, t, f,
                m->ops == &g_fat_ops ? g_blk[m->dev].name :
                m->ops == &g_bin_ops ? "kernel image" : "memory",
                m->readonly ? " (read-only)" : "");
    }

    kmutex_unlock(&g_vfs_mutex);
}


/* ================================================================
 * Диспетчер
 * ================================================================ */

static BOOLEAN is_cmd(FS_LINE *l, const char *name)
{
    return l->argc > 0 && kstreq(l->argv[0], name);
}

/*
 * Выполнить команду файлов, если это она. TRUE - выполнена (или
 * объяснено, как правильно), FALSE - это не команда файлов.
 */
BOOLEAN fs_shell_command(EFI_SYSTEM_TABLE *st, const CHAR16 *line)
{
    SIMPLE_TEXT_OUTPUT_INTERFACE *out = st->ConOut;
    static FS_LINE l;

    fs_split(line, &l);

    if (l.argc == 0)
        return FALSE;

    const char *a1 = (l.argc > 1) ? l.argv[1] : NULL;
    const char *a2 = (l.argc > 2) ? l.argv[2] : NULL;

    if (is_cmd(&l, "ls") || is_cmd(&l, "dir") || is_cmd(&l, "files")) {

        fs_ls(out, a1 ? a1 : ".");

    } else if (is_cmd(&l, "cd")) {

        INTN r = vfs_chdir(a1 ? a1 : "/");

        if (r != VFS_OK)
            fs_err(out, "cd", a1 ? a1 : "/", r);

    } else if (is_cmd(&l, "pwd")) {

        kprintf(out, "%s\n", g_cwd);

    } else if (is_cmd(&l, "mkdir")) {

        if (!a1) {
            print(out, "Usage: mkdir <folder>\n");
        } else {
            INTN r = vfs_mkdir(a1);
            if (r != VFS_OK)
                fs_err(out, "mkdir", a1, r);
        }

    } else if (is_cmd(&l, "rmdir") || is_cmd(&l, "rm")) {

        BOOLEAN dir_cmd = is_cmd(&l, "rmdir");

        if (!a1) {
            print(out, dir_cmd ? "Usage: rmdir <empty folder>\n" : "Usage: rm <file>\n");
            return TRUE;
        }

        VFS_DIRENT e;
        INTN r = vfs_stat(a1, &e);

        if (r == VFS_OK && e.node.is_dir && !dir_cmd) {
            print(out, "That is a folder - use rmdir (it must be empty).\n");
        } else if (r == VFS_OK && !e.node.is_dir && dir_cmd) {
            print(out, "That is a file - use rm.\n");
        } else {
            if (r == VFS_OK)
                r = vfs_remove(a1);
            if (r != VFS_OK)
                fs_err(out, l.argv[0], a1, r);
            else
                print(out, "Deleted.\n");
        }

    } else if (is_cmd(&l, "touch")) {

        if (!a1) {
            print(out, "Usage: touch <file>\n");
        } else {
            VFS_DIRENT e;
            if (vfs_stat(a1, &e) == VFS_OK) {
                print(out, "File already exists.\n");
            } else {
                INTN fd = vfs_open(a1, VFS_O_WRITE | VFS_O_CREATE);
                if (fd < 0)
                    fs_err(out, "touch", a1, fd);
                else {
                    vfs_close(fd);
                    print(out, "Created.\n");
                }
            }
        }

    } else if (is_cmd(&l, "cat")) {

        if (!a1)
            print(out, "Usage: cat <file>\n");
        else
            fs_cat(out, a1);

    } else if (is_cmd(&l, "write") || is_cmd(&l, "append")) {

        BOOLEAN app = is_cmd(&l, "append");

        if (!a1)
            print(out, app ? "Usage: append <file> <text>\n" : "Usage: write <file> <text>\n");
        else
            fs_write(out, a1, l.rest, app);

    } else if (is_cmd(&l, "cp") || is_cmd(&l, "mv")) {

        BOOLEAN mv = is_cmd(&l, "mv");

        if (!a1 || !a2) {
            print(out, mv ? "Usage: mv <from> <to>\n" : "Usage: cp <file> <to>\n");
            return TRUE;
        }

        if (mv) {

            INTN r = vfs_rename(a1, a2);

            if (r == VFS_EXDEV) {
                /* другой диск: скопировать и удалить */
                UINT64 n;
                r = fs_copy(a1, a2, &n);
                if (r == VFS_OK)
                    r = vfs_remove(a1);
                if (r == VFS_OK)
                    kprintf(out, "Moved to another disk (%llu bytes copied).\n", n);
            } else if (r == VFS_OK) {
                print(out, "Moved.\n");
            }

            if (r != VFS_OK)
                fs_err(out, "mv", a1, r);

        } else {

            UINT64 n;
            INTN r = fs_copy(a1, a2, &n);

            if (r != VFS_OK)
                fs_err(out, "cp", a1, r);
            else
                kprintf(out, "Copied, %llu bytes.\n", n);
        }

    } else if (is_cmd(&l, "size") || is_cmd(&l, "stat")) {

        if (!a1) {
            print(out, "Usage: size <file>\n");
        } else {
            VFS_DIRENT e;
            INTN r = vfs_stat(a1, &e);
            if (r != VFS_OK)
                fs_err(out, l.argv[0], a1, r);
            else if (e.node.is_dir)
                kprintf(out, "%s is a folder\n", a1);
            else
                kprintf(out, "%llu bytes\n", e.node.size);
        }

    } else if (is_cmd(&l, "df") || is_cmd(&l, "mounts")) {

        fs_df(out);

    } else {

        return FALSE;
    }

    return TRUE;
}
