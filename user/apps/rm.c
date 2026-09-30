/*
 * rm - удалить файл (этап 10, Д3: раньше - команда ядра).
 * Папку - только rmdir (и только пустую): так случайно не снести всё.
 */
#include "myos.h"

int main(int argc, char **argv)
{
    if (argc < 2) {
        printf("Usage: rm <file>\n");
        return 1;
    }

    struct myos_dirent e;
    int r = stat(argv[1], &e);

    if (r == 0 && e.is_dir) {
        printf("That is a folder - use rmdir (it must be empty).\n");
        return 1;
    }

    if (r == 0)
        r = unlink(argv[1]);

    if (r != 0) {
        file_err("rm", argv[1], r);
        return 1;
    }

    printf("Deleted.\n");
    return 0;
}
