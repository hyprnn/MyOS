/*
 * touch - создать пустой файл (этап 10, Д3: раньше - команда ядра).
 * Существующий не трогаем (дату изменения MyOS пока не меняет).
 */
#include "myos.h"

int main(int argc, char **argv)
{
    if (argc < 2) {
        printf("Usage: touch <file>\n");
        return 1;
    }

    struct myos_dirent e;

    if (stat(argv[1], &e) == 0) {
        printf("File already exists.\n");
        return 0;
    }

    int fd = open(argv[1], O_WRITE | O_CREATE);

    if (fd < 0) {
        file_err("touch", argv[1], fd);
        return 1;
    }

    close(fd);
    printf("Created.\n");
    return 0;
}
