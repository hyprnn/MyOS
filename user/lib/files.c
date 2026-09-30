/*
 * user/lib/files.c - общее для программ-команд файлов (этап 10, Д3):
 * ls, cat, cp, mv, rm, rmdir, mkdir, touch, write, append, size.
 *
 * Раньше эти команды были кодом ядра (shell/fs.c). Теперь это обычные
 * программы в /bin - как в Linux (/bin/ls, /bin/cp): ошибка в них не
 * роняет систему, их можно заменить своими. Сообщения - те же, что у
 * версий ядра (они остались для аварийного шелла), чтобы пользователь
 * не заметил подмены.
 */
#include "myos.h"

/* Последний элемент пути ("a/b/c.txt" -> "c.txt"; "a/b/" -> "b/") */
const char *path_base(const char *p)
{
    const char *b = p;

    for (; *p; p++)
        if (*p == '/' && p[1] != '\0')
            b = p + 1;

    return b;
}

/*
 * Полный путь без "." и ".." - как vfs_normalize в ядре: относительный
 * путь - от текущей папки программы. Нужен ls - заголовку "/usb0p1:"
 * и чтобы узнать корень ("ls ..", "ls /usb0p1/.."). 0 - получилось,
 * MYOS_EINVAL - путь не помещается.
 */
int path_normalize(const char *path, char *out, size_t cap)
{
    char tmp[512];
    size_t t = 0;

    if (cap < 2)
        return MYOS_EINVAL;

    if (path[0] != '/') {
        long n = getcwd_len(tmp, sizeof(tmp) - 1);
        if (n < 0)
            n = 0;
        t = (size_t)n;
        tmp[t++] = '/';
    }

    for (size_t i = 0; path[i] && t + 1 < sizeof(tmp); i++)
        tmp[t++] = path[i];

    tmp[t] = '\0';

    /* собираем по частям; ".." стирает последнюю */
    size_t o = 0;
    out[o++] = '/';
    out[o] = '\0';

    size_t i = 0;

    while (tmp[i]) {

        while (tmp[i] == '/')
            i++;

        if (!tmp[i])
            break;

        size_t s = i;

        while (tmp[i] && tmp[i] != '/')
            i++;

        size_t len = i - s;

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

        if (o + len + 2 >= cap)
            return MYOS_EINVAL;

        for (size_t k = 0; k < len; k++)
            out[o++] = tmp[s + k];

        out[o++] = '/';
        out[o] = '\0';
    }

    /* "/usb0p1/" -> "/usb0p1" (корень "/" остаётся) */
    if (o > 1)
        out[o - 1] = '\0';

    return 0;
}

/* "cp a.txt: no such file or folder" - как fs_err в ядре */
void file_err(const char *what, const char *path, int e)
{
    /* у файлов "не поддерживается" значит одно: RAM-диск без папок */
    if (e == MYOS_ENOSYS)
        printf("%s %s: this file system cannot do that (the RAM disk has no folders)\n",
               what, path);
    else
        printf("%s %s: %s\n", what, path, strerror(e));
}

/*
 * Копировать файл. Если to - папка, то внутрь неё с тем же именем.
 * Существующий файл не затирается (MYOS_EEXIST) - как у версии ядра:
 * случайно потерять файл хуже, чем набрать rm. *copied - сколько байт.
 */
int copy_file(const char *from, const char *to, unsigned long long *copied)
{
    char dst[512];
    struct myos_dirent st;

    *copied = 0;

    if (stat(to, &st) == 0 && st.is_dir)
        snprintf(dst, sizeof(dst), "%s/%s", to, path_base(from));
    else
        snprintf(dst, sizeof(dst), "%s", to);

    if (stat(from, &st) != 0)
        return MYOS_ENOENT;

    if (st.is_dir)
        return MYOS_EISDIR;

    if (stat(dst, &st) == 0)
        return MYOS_EEXIST;

    int in = open(from, O_READ);

    if (in < 0)
        return in;

    int o = open(dst, O_WRITE | O_CREATE | O_TRUNC);

    if (o < 0) {
        close(in);
        return o;
    }

    /* 16 КиБ за раз: меньше системных вызовов, а куча у программы своя */
    char *buf = malloc(16384);
    int r = 0;

    if (buf == NULL)
        r = MYOS_EIO;

    while (r == 0) {

        long got = read(in, buf, 16384);

        if (got <= 0) {
            r = (int)got;
            break;
        }

        long w = write(o, buf, (size_t)got);

        if (w < 0) {
            r = (int)w;
            break;
        }

        if (w < got) {
            r = MYOS_ENOSPC;
            break;
        }

        *copied += (unsigned long long)w;
    }

    free(buf);
    close(in);
    close(o);

    return r;
}

/*
 * write и append: одна строка текста в файл (с '\n' в конце, как
 * принято в Linux). Слова текста - аргументы с first-го, через пробел.
 */
int file_put_words(int argc, char **argv, int first, int append)
{
    const char *name = append ? "append" : "write";

    if (argc <= 1) {
        printf("Usage: %s <file> <text>\n", name);
        return 1;
    }

    const char *path = argv[1];
    char buf[4096];
    size_t n = 0;

    for (int i = first; i < argc; i++) {
        for (const char *s = argv[i]; *s && n + 2 < sizeof(buf); s++)
            buf[n++] = *s;
        if (i + 1 < argc && n + 2 < sizeof(buf))
            buf[n++] = ' ';
    }

    buf[n++] = '\n';

    int fd = open(path, O_WRITE | O_CREATE | (append ? O_APPEND : O_TRUNC));

    if (fd < 0) {
        file_err(name, path, fd);
        return 1;
    }

    long w = write(fd, buf, n);
    close(fd);

    if (w < 0) {
        file_err(name, path, (int)w);
        return 1;
    }

    if (w < (long)n) {
        file_err(name, path, MYOS_ENOSPC);
        return 1;
    }

    struct myos_dirent st;

    if (stat(path, &st) == 0)
        printf("OK - %s is now %llu bytes.\n", path_base(path), st.size);

    return 0;
}
