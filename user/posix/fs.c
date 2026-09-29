/*
 * user/posix/fs.c - ещё немного POSIX про файлы для перенесённых
 * программ (браузер NetSurf, FreeType): realpath, scandir, mmap,
 * функции "*at" (fstatat, unlinkat, openat) и dirfd. Часть MyOS (этап 9).
 *
 * Ядро MyOS всего этого не умеет - делаем из того, что умеет:
 *   mmap файла     - память из malloc + чтение файла в неё (отображать
 *                    файл в память по-настоящему ядро не умеет; браузеру
 *                    и FreeType нужно только читать);
 *   dirfd + *at    - номер "папки" - это место в таблице открытых
 *                    DIR, по нему находим путь папки и склеиваем его с
 *                    именем.
 */
#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include "myos_sys.h"

#define DIRFD_BASE  0x4000     /* такие fd - "папки" из dirfd() */
#define DIRFD_MAX   16

static DIR *g_dirs[DIRFD_MAX];

/* путь MyOS - до 256 байт (VFS_PATH_MAX в ядре) */
#define MYOS_PATH   256

/* ================================================================
 * realpath: полный путь без "." и ".."
 * ================================================================ */

char *realpath(const char *restrict path, char *restrict resolved)
{
    char tmp[MYOS_PATH * 2];
    char out[MYOS_PATH];
    size_t o = 0;
    struct stat st;

    if (path == NULL) {
        errno = EINVAL;
        return NULL;
    }

    if (path[0] == '/')
        snprintf(tmp, sizeof(tmp), "%s", path);
    else {
        char cwd[MYOS_PATH];
        if (getcwd(cwd, sizeof(cwd)) == NULL)
            return NULL;
        snprintf(tmp, sizeof(tmp), "%s/%s", cwd, path);
    }

    /* по частям: "." пропустить, ".." - стереть последнюю */
    out[0] = '\0';

    for (char *p = tmp; *p; ) {
        while (*p == '/')
            p++;
        if (*p == '\0')
            break;

        char *e = p;
        while (*e && *e != '/')
            e++;
        size_t n = (size_t)(e - p);

        if (n == 1 && p[0] == '.') {
            /* ничего */
        } else if (n == 2 && p[0] == '.' && p[1] == '.') {
            while (o > 0 && out[o - 1] != '/')
                o--;
            if (o > 0)
                o--;
            out[o] = '\0';
        } else {
            if (o + 1 + n + 1 > sizeof(out)) {
                errno = ENAMETOOLONG;
                return NULL;
            }
            out[o++] = '/';
            memcpy(out + o, p, n);
            o += n;
            out[o] = '\0';
        }
        p = e;
    }

    if (o == 0) {
        out[0] = '/';
        out[1] = '\0';
    }

    /* POSIX: такой файл или папка должны быть */
    if (stat(out, &st) < 0)
        return NULL;

    if (resolved == NULL)
        return strdup(out);

    strcpy(resolved, out);
    return resolved;
}

/* ================================================================
 * scandir: все записи папки, отобранные и отсортированные
 * ================================================================ */

int scandir(const char *dir, struct dirent ***list, int (*sel)(const struct dirent *),
            int (*cmp)(const struct dirent **, const struct dirent **))
{
    DIR *d = opendir(dir);
    struct dirent **v = NULL;
    size_t n = 0, cap = 0;
    struct dirent *e;

    if (d == NULL)
        return -1;

    while ((e = readdir(d)) != NULL) {

        if (sel != NULL && !sel(e))
            continue;

        if (n == cap) {
            cap = cap ? cap * 2 : 16;
            struct dirent **nv = realloc(v, cap * sizeof(*v));
            if (nv == NULL)
                goto nomem;
            v = nv;
        }

        v[n] = malloc(sizeof(struct dirent));
        if (v[n] == NULL)
            goto nomem;
        memcpy(v[n], e, sizeof(struct dirent));
        n++;
    }

    closedir(d);

    if (cmp != NULL && n > 1)
        qsort(v, n, sizeof(*v), (int (*)(const void *, const void *))cmp);

    *list = v;
    return (int)n;

nomem:
    for (size_t i = 0; i < n; i++)
        free(v[i]);
    free(v);
    closedir(d);
    errno = ENOMEM;
    return -1;
}

int alphasort(const struct dirent **a, const struct dirent **b)
{
    return strcoll((*a)->d_name, (*b)->d_name);
}

/* ================================================================
 * dirfd и функции "*at"
 * ================================================================ */

int dirfd(DIR *d)
{
    for (int i = 0; i < DIRFD_MAX; i++)
        if (g_dirs[i] == d)
            return DIRFD_BASE + i;

    for (int i = 0; i < DIRFD_MAX; i++)
        if (g_dirs[i] == NULL) {
            g_dirs[i] = d;
            return DIRFD_BASE + i;
        }

    errno = EMFILE;
    return -1;
}

/* os.c: closedir - забыть номер папки */
void __myos_dir_forget(DIR *d)
{
    for (int i = 0; i < DIRFD_MAX; i++)
        if (g_dirs[i] == d)
            g_dirs[i] = NULL;
}

/* "папка fd" + имя -> полный путь; 0 - получилось */
static int at_path(int fd, const char *name, char *out, size_t cap)
{
    if (name[0] == '/' || fd == AT_FDCWD) {
        snprintf(out, cap, "%s", name);
        return 0;
    }

    int i = fd - DIRFD_BASE;

    if (i < 0 || i >= DIRFD_MAX || g_dirs[i] == NULL) {
        errno = EBADF;
        return -1;
    }

    /* путь папки хранит наш opendir в буфере DIR (os.c) */
    if (snprintf(out, cap, "%s/%s", g_dirs[i]->buf, name) >= (int)cap) {
        errno = ENAMETOOLONG;
        return -1;
    }

    return 0;
}

int fstatat(int fd, const char *restrict name, struct stat *restrict st, int flag)
{
    char p[MYOS_PATH];

    (void)flag;                /* ссылок нет - "не идти по ссылке" не важно */

    if (at_path(fd, name, p, sizeof(p)) < 0)
        return -1;

    return stat(p, st);
}

int unlinkat(int fd, const char *name, int flag)
{
    char p[MYOS_PATH];

    if (at_path(fd, name, p, sizeof(p)) < 0)
        return -1;

    return (flag & AT_REMOVEDIR) ? rmdir(p) : unlink(p);
}

int openat(int fd, const char *name, int flags, ...)
{
    char p[MYOS_PATH];

    if (at_path(fd, name, p, sizeof(p)) < 0)
        return -1;

    return open(p, flags);
}

/* ================================================================
 * mmap / munmap: только чтение файла (или просто память)
 * ================================================================ */

void *mmap(void *addr, size_t len, int prot, int flags, int fd, off_t off)
{
    (void)addr;

    if (len == 0 || (flags & MAP_FIXED) || ((flags & MAP_SHARED) && (prot & PROT_WRITE))) {
        errno = (len == 0) ? EINVAL : ENOTSUP;
        return MAP_FAILED;
    }

    unsigned char *m = calloc(1, len);

    if (m == NULL) {
        errno = ENOMEM;
        return MAP_FAILED;
    }

    if (flags & MAP_ANONYMOUS)
        return m;

    /* копия файла: прочитать len байт с места off (дальше конца - нули) */
    off_t was = lseek(fd, 0, SEEK_CUR);

    if (lseek(fd, off, SEEK_SET) < 0) {
        free(m);
        return MAP_FAILED;
    }

    size_t got = 0;

    while (got < len) {
        ssize_t r = read(fd, m + got, len - got);
        if (r < 0) {
            free(m);
            return MAP_FAILED;
        }
        if (r == 0)
            break;
        got += (size_t)r;
    }

    if (was >= 0)
        lseek(fd, was, SEEK_SET);

    return m;
}

int munmap(void *addr, size_t len)
{
    (void)len;
    free(addr);
    return 0;
}

/* Копия номера файла: ядро MyOS так не умеет */
int dup(int fd)
{
    (void)fd;
    errno = EMFILE;
    return -1;
}
