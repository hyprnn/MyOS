/*
 * edit - построчный редактор (переехал из ядра в программу на
 * этапе 6; работает с файлами через системные вызовы open/read/write).
 *   edit <файл> - показать текст, затем набрать новый; строка "."
 *                 - конец (текст файла заменяется).
 */
#include "myos.h"

#define MAXTEXT 16384

int main(int argc, char **argv)
{
    if (argc < 2) {
        printf("Usage: edit <file>\n");
        return 1;
    }

    const char *path = argv[1];
    static char text[MAXTEXT + 1];
    struct myos_dirent st;

    if (stat(path, &st) == 0) {

        if (st.is_dir) {
            printf("That is a folder.\n");
            return 1;
        }

        int fd = open(path, O_READ);

        if (fd >= 0) {
            long n = read(fd, text, MAXTEXT);
            close(fd);
            if (n > 0) {
                text[n] = '\0';
                printf("--- current text ---\n%s", text);
                if (text[n - 1] != '\n')
                    printf("\n");
                printf("--------------------\n");
            }
        }
    }

    printf("Editing '%s'. Type the new text line by line; a line with just '.'\n"
           "finishes (the old text is replaced; '.' at once - keep it as is).\n", path);

    size_t len = 0;
    int lines = 0;
    char line[256];

    for (;;) {
        printf(": ");
        if (!getline_in(line, sizeof(line)))
            break;
        if (strcmp(line, ".") == 0)
            break;
        size_t k = strlen(line);
        if (len + k + 1 >= MAXTEXT)
            break;
        memcpy(text + len, line, k);
        len += k;
        text[len++] = '\n';
        lines++;
    }

    if (lines == 0) {
        printf("Nothing typed - the file is unchanged.\n");
        return 0;
    }

    int fd = open(path, O_WRITE | O_CREATE | O_TRUNC);

    if (fd < 0) {
        printf("Could not save %s: %s\n", path, strerror(fd));
        return 1;
    }

    long w = write(fd, text, len);
    close(fd);

    if (w != (long)len) {
        printf("Could not save %s: %s\n", path, w < 0 ? strerror((int)w) : "disk full");
        return 1;
    }

    printf("Saved: %d line(s), %u bytes.\n", lines, (unsigned)len);
    return 0;
}
