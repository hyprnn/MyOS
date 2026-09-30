/*
 * cp - копировать файл (этап 10, Д3: раньше - команда ядра).
 *   cp ФАЙЛ НОВОЕ_ИМЯ    cp ФАЙЛ ПАПКА    (в том числе на другой диск)
 * Существующий файл не затирается - сначала rm.
 */
#include "myos.h"

int main(int argc, char **argv)
{
    if (argc < 3) {
        printf("Usage: cp <file> <to>\n");
        return 1;
    }

    unsigned long long n;
    int r = copy_file(argv[1], argv[2], &n);

    if (r != 0) {
        file_err("cp", argv[1], r);
        return 1;
    }

    printf("Copied, %llu bytes.\n", n);
    return 0;
}
