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
 *
 * Внутренний диск ноутбука (на нём Arch) MyOS держит только для
 * чтения. Если MyOS загружена с него (стоит рядом с Arch), настройки
 * пишутся через "окно записи": на время одной записи файла в
 * EFI/MyOS - и только если пользователь подтвердил (wifi save confirm)
 * - разделу разрешается запись (g_blk_write_window в drivers/blk.c).
 * Пишет при этом драйвер FAT: сам файл, его запись в папке и таблицу
 * FAT - больше ничего на диске не меняется.
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

static VFS_MOUNT *settings_mount(void)
{
    char vol[20];

    if (!settings_boot_volume(vol, sizeof(vol)))
        return NULL;

    for (UINTN i = 0; i < VFS_MAX_MOUNTS; i++)
        if (g_mounts[i].used && !g_mounts[i].gone && kstreq(g_mounts[i].name, vol))
            return &g_mounts[i];

    return NULL;
}

/* Том загрузки - только для чтения (внутренний диск)? Тогда запись
   настроек требует подтверждения. */
BOOLEAN settings_readonly(void)
{
    VFS_MOUNT *m = settings_mount();
    return m != NULL && m->readonly;
}

/* Открыть окно записи для тома m (если он только для чтения). Держим
   g_vfs_mutex всё время, пока окно открыто: ни один другой поток не
   успеет записать что-то своё на этот диск. */
static BOOLEAN settings_window_open(VFS_MOUNT *m)
{
    kmutex_lock(&g_vfs_mutex);

    if (!m->readonly)
        return FALSE;

    m->readonly = FALSE;
    g_blk_write_window = (INTN)m->dev;
    klog("settings: write window open on /%s (EFI/MyOS only)\n", m->name);

    return TRUE;
}

static void settings_window_close(VFS_MOUNT *m, BOOLEAN opened)
{
    if (opened) {
        g_blk_write_window = -1;
        m->readonly = TRUE;
        klog("settings: write window closed on /%s\n", m->name);
    }

    kmutex_unlock(&g_vfs_mutex);
}

/* Записать файл настроек (создав папку EFI/MyOS, если её нет - так
   бывает на флешке, где MyOS лежит в EFI/BOOT). 0 - получилось,
   -1 - том загрузки не найден, иначе код ошибки VFS. На томе только
   для чтения - через окно записи: звать только после согласия
   пользователя. */
INTN settings_write(const char *name, const void *data, UINTN n, char *where, UINTN cap)
{
    VFS_MOUNT *m = settings_mount();

    if (m == NULL)
        return -1;

    char dir[48];
    ksnprintf(dir, sizeof(dir), "/%s/EFI/MyOS", m->name);
    ksnprintf(where, cap, "%s/%s", dir, name);

    BOOLEAN opened = settings_window_open(m);
    VFS_DIRENT e;
    INTN r = VFS_OK;

    if (vfs_stat(dir, &e) != VFS_OK)
        r = vfs_mkdir(dir);

    if (r == VFS_OK)
        r = vfs_write_file(where, data, n, FALSE);

    settings_window_close(m, opened);
    return r;
}

/* Удалить файл настроек (тоже через окно записи, если нужно) */
INTN settings_remove(const char *name, char *where, UINTN cap)
{
    VFS_MOUNT *m = settings_mount();

    if (m == NULL)
        return -1;

    ksnprintf(where, cap, "/%s/EFI/MyOS/%s", m->name, name);

    VFS_DIRENT e;

    if (vfs_stat(where, &e) != VFS_OK)
        return VFS_ENOENT;

    BOOLEAN opened = settings_window_open(m);
    INTN r = vfs_remove(where);
    settings_window_close(m, opened);

    return r;
}
