/*
 * cat - показать текстовый файл (этап 10, Д3: раньше - команда ядра).
 *
 * Как у версии ядра: управляющие символы и не-ASCII - '?', большой
 * файл - только первые 64 КиБ (экран всё равно столько не покажет),
 * а если попались нулевые байты - предупредить, что файл не текст.
 * С "> файл" шелла это ещё и копирование текста.
 */
#include "myos.h"

static int cat_one(const char *path)
{
    int fd = open(path, O_READ);

    if (fd < 0) {
        file_err("cat", path, fd);
        return 1;
    }

    char buf[512];
    unsigned long long total = 0;
    int binary = 0;
    char last = '\n';
    int rc = 0;

    for (;;) {

        long r = read(fd, buf, sizeof(buf));

        if (r < 0) {
            file_err("cat", path, (int)r);
            rc = 1;
            break;
        }

        if (r == 0)
            break;

        for (long i = 0; i < r; i++) {
            unsigned char c = (unsigned char)buf[i];
            if (c == '\r')
                c = ' ';
            if (c != '\n' && c != '\t' && (c < 32 || c > 126)) {
                if (c < 9)
                    binary = 1;
                c = '?';
            }
            buf[i] = (char)c;
        }

        write(1, buf, (size_t)r);
        last = buf[r - 1];
        total += (unsigned long long)r;

        /* большой файл - только начало */
        if (total >= 64u * 1024u) {
            printf("\n... (only the first 64 KiB shown)\n");
            last = '\n';
            break;
        }
    }

    if (last != '\n')
        printf("\n");

    if (binary)
        printf("(this looks like a binary file, not text)\n");

    close(fd);
    return rc;
}

int main(int argc, char **argv)
{
    if (argc < 2) {
        printf("Usage: cat <file>\n");
        return 1;
    }

    /* несколько файлов подряд - как в Linux (ядро умело только один) */
    int rc = 0;

    for (int i = 1; i < argc; i++)
        rc |= cat_one(argv[i]);

    return rc;
}
