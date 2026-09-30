/*
 * mkdir - новая папка (этап 10, Д3: раньше - команда ядра).
 * Молчит, если всё получилось - как mkdir в Linux.
 */
#include "myos.h"

int main(int argc, char **argv)
{
    if (argc < 2) {
        printf("Usage: mkdir <folder>\n");
        return 1;
    }

    int rc = 0;

    for (int i = 1; i < argc; i++) {
        int r = mkdir(argv[i]);
        if (r != 0) {
            file_err("mkdir", argv[i], r);
            rc = 1;
        }
    }

    return rc;
}
