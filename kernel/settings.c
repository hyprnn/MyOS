/*
 * kernel/settings.c - где MyOS хранит свои настройки (этап 9).
 * Часть MyOS; общие объявления - в myos.h.
 *
 * Своего диска у MyOS нет: она живёт двумя файлами на флешке или в
 * папке EFI\MyOS на разделе EFI ноутбука (рядом с Arch). Туда же, в
 * папку EFI\MyOS того тома, С КОТОРОГО ЗАГРУЗИЛИСЬ, кладём и настройки
 * (сейчас - запомненная сеть Wi-Fi). Больше никуда на чужие диски
 * MyOS сама не пишет, и пишет только по команде (wifi save).
 *
 * Какой том "наш"? Загрузчик сообщил путь к ядру (\EFI\MYOS\KERNEL.ELF
 * или \EFI\BOOT\KERNEL.ELF) и его размер. Смотрим на всех дисках FAT,
 * где лежит файл с таким путём и ровно такого размера.
 */
#include "myos.h"

static char g_settings_vol[20];    /* "sata0", "nvme0p1"... ("" - не нашли) */
static BOOLEAN g_settings_looked;

/* путь ядра от загрузчика ("\EFI\BOOT\KERNEL.ELF") -> "/EFI/BOOT/KERNEL.ELF" */
static void kernel_path_unix(char *out, UINTN cap)
{
    UINTN n = 0;

    for (UINTN i = 0; g_boot.kernel_path[i] && n + 1 < cap; i++) {
        CHAR16 c = g_boot.kernel_path[i];
        out[n++] = (c == '\\') ? '/' : (c < 128 ? (char)c : '?');
    }

    out[n] = '\0';
}

/* Найти том загрузки. TRUE - нашли, его имя в vol. */
BOOLEAN settings_boot_volume(char *vol, UINTN cap)
{
    if (!g_settings_looked || g_settings_vol[0] == '\0') {

        char kp[80];
        kernel_path_unix(kp, sizeof(kp));

        g_settings_vol[0] = '\0';

        for (UINTN i = 0; i < VFS_MAX_MOUNTS && kp[0] == '/'; i++) {

            VFS_MOUNT *m = &g_mounts[i];

            if (!m->used || m->gone || !vfs_is_disk(m))
                continue;

            char path[120];
            VFS_DIRENT e;

            ksnprintf(path, sizeof(path), "/%s%s", m->name, kp);

            if (vfs_stat(path, &e) == 0 && !e.node.is_dir &&
                e.node.size == g_boot.kernel_file_size) {
                ksnprintf(g_settings_vol, sizeof(g_settings_vol), "%s", m->name);
                break;
            }
        }

        g_settings_looked = TRUE;
    }

    if (g_settings_vol[0] == '\0')
        return FALSE;

    ksnprintf(vol, cap, "%s", g_settings_vol);
    return TRUE;
}

/* Полный путь файла настроек: "/sata0/EFI/MyOS/<name>". FALSE - том
   загрузки не найден (например, MyOS загружена с диска, который она
   не видит - IDE в QEMU) */
BOOLEAN settings_path(const char *name, char *out, UINTN cap)
{
    char vol[20];

    if (!settings_boot_volume(vol, sizeof(vol)))
        return FALSE;

    ksnprintf(out, cap, "/%s/EFI/MyOS/%s", vol, name);
    return TRUE;
}

/* Записать файл настроек (создав папку EFI/MyOS, если её нет - так
   бывает на флешке, где MyOS лежит в EFI/BOOT). 0 - получилось,
   иначе код ошибки VFS. */
INTN settings_write(const char *name, const void *data, UINTN n, char *where, UINTN cap)
{
    char vol[20], dir[48];

    if (!settings_boot_volume(vol, sizeof(vol)))
        return -1;

    ksnprintf(dir, sizeof(dir), "/%s/EFI/MyOS", vol);

    VFS_DIRENT e;

    if (vfs_stat(dir, &e) != 0) {
        INTN r = vfs_mkdir(dir);
        if (r != 0)
            return r;
    }

    ksnprintf(where, cap, "%s/%s", dir, name);

    return vfs_write_file(where, data, n, FALSE);
}
