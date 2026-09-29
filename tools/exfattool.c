/*
 * tools/exfattool.c - файлы в образе диска с exFAT (для автотеста):
 * положить, достать, показать. Собирается на хосте из той же FatFs
 * (third_party/fatfs), что и драйвер ядра; независимую проверку
 * целостности тома делает fsck.exfat (exfatprogs).
 *
 *   exfattool образ смещение_в_секторах put файл /путь
 *   exfattool образ смещение_в_секторах get /путь      > файл
 *   exfattool образ смещение_в_секторах ls /папка
 *
 *   gcc -O2 -Ithird_party/fatfs -DFFCONF_H='"ffconf.h"' -o exfattool \
 *       tools/exfattool.c third_party/fatfs/ff.c third_party/fatfs/ffunicode.c
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "ff.h"
#include "diskio.h"

static FILE *g_img;
static long long g_off;

DRESULT disk_read(void *drv, BYTE *buff, DWORD sector, UINT count)
{
    (void)drv;
    if (fseeko(g_img, (g_off + sector) * 512LL, SEEK_SET) != 0 ||
        fread(buff, 512, count, g_img) != count)
        return RES_ERROR;
    return RES_OK;
}

DRESULT disk_write(void *drv, const BYTE *buff, DWORD sector, UINT count)
{
    (void)drv;
    if (fseeko(g_img, (g_off + sector) * 512LL, SEEK_SET) != 0 ||
        fwrite(buff, 512, count, g_img) != count)
        return RES_ERROR;
    return RES_OK;
}

DRESULT disk_ioctl(void *drv, BYTE cmd, void *buff)
{
    (void)drv;
    if (cmd == IOCTL_INIT || cmd == IOCTL_STATUS) {
        *(DSTATUS *)buff = 0;
        return RES_OK;
    }
    if (cmd == GET_SECTOR_SIZE) {
        *(WORD *)buff = 512;
        return RES_OK;
    }
    return RES_OK;
}

DWORD get_fattime(void)
{
    return ((DWORD)(2026 - 1980) << 25) | (9u << 21) | (29u << 16);
}

void *ff_memalloc(UINT n) { return malloc(n); }
void ff_memfree(void *p) { free(p); }

int main(int argc, char **argv)
{
    if (argc < 5) {
        fprintf(stderr, "usage: exfattool img offset_sectors put|get|ls ...\n");
        return 2;
    }

    g_img = fopen(argv[1], "r+b");
    g_off = atoll(argv[2]);

    if (g_img == NULL) {
        perror(argv[1]);
        return 2;
    }

    static FATFS fs;
    fs.drv = &fs;

    FRESULT r = f_mount(&fs);

    if (r != FR_OK) {
        fprintf(stderr, "mount failed: %d\n", r);
        return 1;
    }

    if (strcmp(argv[3], "put") == 0 && argc >= 6) {
        FILE *src = fopen(argv[4], "rb");
        FIL f;
        char buf[65536];
        size_t n;
        UINT w;
        if (!src || f_open(&fs, &f, argv[5], FA_WRITE | FA_CREATE_ALWAYS) != FR_OK) {
            fprintf(stderr, "cannot put %s\n", argv[5]);
            return 1;
        }
        while ((n = fread(buf, 1, sizeof(buf), src)) > 0)
            if (f_write(&f, buf, (UINT)n, &w) != FR_OK || w != n)
                return 1;
        f_close(&f);
        fclose(src);
    } else if (strcmp(argv[3], "mkdir") == 0) {
        if (f_mkdir(&fs, argv[4]) != FR_OK)
            return 1;
    } else if (strcmp(argv[3], "get") == 0) {
        FIL f;
        char buf[65536];
        UINT got;
        if (f_open(&fs, &f, argv[4], FA_READ) != FR_OK)
            return 1;
        while (f_read(&f, buf, sizeof(buf), &got) == FR_OK && got > 0)
            fwrite(buf, 1, got, stdout);
        f_close(&f);
    } else if (strcmp(argv[3], "ls") == 0) {
        FF_DIR d;
        FILINFO fi;
        if (f_opendir(&fs, &d, argv[4]) != FR_OK)
            return 1;
        while (f_readdir(&d, &fi) == FR_OK && fi.fname[0])
            printf("%s%s\t%llu\n", fi.fname, (fi.fattrib & AM_DIR) ? "/" : "",
                   (unsigned long long)fi.fsize);
        f_closedir(&d);
    } else {
        return 2;
    }

    f_umount(&fs);
    fclose(g_img);
    return 0;
}
