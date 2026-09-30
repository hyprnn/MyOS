/*
 * size - размер файла в байтах (этап 10, Д3: раньше - команда ядра).
 */
#include "myos.h"

int main(int argc, char **argv)
{
    if (argc < 2) {
        printf("Usage: size <file>\n");
        return 1;
    }

    struct myos_dirent e;
    int r = stat(argv[1], &e);

    if (r != 0) {
        file_err("size", argv[1], r);
        return 1;
    }

    if (e.is_dir)
        printf("%s is a folder\n", argv[1]);
    else
        printf("%llu bytes\n", e.size);

    return 0;
}
