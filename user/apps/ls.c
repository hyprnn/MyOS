/*
 * ls - что лежит в папке (этап 10, Д3: раньше - команда ядра).
 *
 *   ls            текущая папка
 *   ls ПАПКА      другая папка
 *   ls ФАЙЛ       одна строка про файл
 *   ls /          список томов (дисков) - его знает только ядро, поэтому
 *                 здесь программа просит ядро: kcmd("ls /") с флагом
 *                 MYOS_KCMD_KERNEL ("выполни сам, не отдавай программе ls",
 *                 иначе ядро снова запустило бы нас)
 *
 * Вид строк - как у версии ядра: дата изменения, размер или <folder>, имя.
 */
#include "myos.h"

static unsigned int g_files, g_dirs;
static unsigned long long g_bytes;

/* Одна строка списка. Дата - в формате FAT (так её хранят диски):
   год от 1980 в битах 9..15, месяц 5..8, день 0..4; время - часы
   11..15, минуты 5..10. 0 - даты нет (RAM-диск, /bin). */
static void show(const struct myos_dirent *d)
{
    char when[24];

    when[0] = '\0';

    if (d->wdate != 0)
        snprintf(when, sizeof(when), "%04u-%02u-%02u %02u:%02u",
                 1980u + (d->wdate >> 9), (d->wdate >> 5) & 0xFu, d->wdate & 0x1Fu,
                 (unsigned)d->wtime >> 11, ((unsigned)d->wtime >> 5) & 0x3Fu);

    if (d->is_dir) {
        printf("  %-16s  %-10s  %s/\n", when, "<folder>", d->name);
        g_dirs++;
    } else {
        printf("  %-16s  %10llu  %s\n", when, d->size, d->name);
        g_files++;
        g_bytes += d->size;
    }
}

int main(int argc, char **argv)
{
    const char *arg = (argc > 1) ? argv[1] : ".";
    char norm[512];

    if (path_normalize(arg, norm, sizeof(norm)) != 0) {
        printf("Path is too long.\n");
        return 1;
    }

    /* корень: тома знает только ядро */
    if (norm[1] == '\0') {
        kcmd("ls /", 0, MYOS_KCMD_KERNEL);
        return 0;
    }

    struct myos_dirent st;
    int r = stat(norm, &st);

    if (r != 0) {
        file_err("ls", norm, r);
        return 1;
    }

    if (!st.is_dir) {
        show(&st);
        return 0;
    }

    printf("%s:\n", norm);

    /* readdir(папка, номер) - по одной записи; 0 - записи кончились */
    for (int i = 0;; i++) {

        struct myos_dirent d;
        r = readdir(norm, i, &d);

        if (r < 0) {
            file_err("ls", norm, r);
            break;
        }

        if (r == 0)
            break;

        show(&d);
    }

    if (g_files == 0 && g_dirs == 0)
        printf("  (empty)\n");
    else
        printf("  %u file(s), %llu bytes; %u folder(s)\n", g_files, g_bytes, g_dirs);

    return r < 0 ? 1 : 0;
}
