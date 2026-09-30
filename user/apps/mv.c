/*
 * mv - переименовать или переместить (этап 10, Д3: раньше - команда ядра).
 *
 * На том же диске это rename: меняется только запись в папке, данные
 * не двигаются. Между дисками rename невозможен (MYOS_EXDEV) - тогда,
 * как mv в Linux, копируем и удаляем оригинал.
 */
#include "myos.h"

int main(int argc, char **argv)
{
    if (argc < 3) {
        printf("Usage: mv <from> <to>\n");
        return 1;
    }

    int r = rename(argv[1], argv[2]);

    if (r == MYOS_EXDEV) {
        unsigned long long n;
        r = copy_file(argv[1], argv[2], &n);
        if (r == 0)
            r = unlink(argv[1]);
        if (r == 0)
            printf("Moved to another disk (%llu bytes copied).\n", n);
    } else if (r == 0) {
        printf("Moved.\n");
    }

    if (r != 0) {
        file_err("mv", argv[1], r);
        return 1;
    }

    return 0;
}
