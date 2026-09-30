/*
 * write - записать строку в файл, заменив его содержимое
 * (этап 10, Д3: раньше - команда ядра).
 *   write memo.txt купить хлеба
 * Дописать в конец - append. Общий код - file_put_words (user/lib/files.c).
 */
#include "myos.h"

int main(int argc, char **argv)
{
    return file_put_words(argc, argv, 2, 0);
}
