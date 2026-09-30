/*
 * append - дописать строку в конец файла (этап 10, Д3: раньше -
 * команда ядра). Файла нет - будет создан.
 *   append memo.txt и молока
 */
#include "myos.h"

int main(int argc, char **argv)
{
    return file_put_words(argc, argv, 2, 1);
}
