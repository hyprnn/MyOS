/*
 * third_party/fatfs/ffconf.h - настройки FatFs (ooFatFs R0.13c) для
 * MyOS. Собрано по образцу ffconf.h из MicroPython; MyOS нужен от
 * FatFs только exFAT (FAT16/32 обслуживает свой fs/fat.c), но
 * библиотека умеет и их.
 */
#define FFCONF_DEF  86604   /* Revision ID */

#define FF_FS_READONLY  0       /* чтение и запись */
#define FF_FS_MINIMIZE  0
#define FF_USE_STRFUNC  0
#define FF_USE_FIND     0
#define FF_USE_MKFS     0       /* форматировать не умеем (и не надо) */
#define FF_USE_FASTSEEK 0
#define FF_USE_EXPAND   0
#define FF_USE_CHMOD    0
#define FF_USE_LABEL    1       /* метка тома - для ls / */
#define FF_USE_FORWARD  0

#define FF_CODE_PAGE    437     /* короткие имена (у exFAT их нет вовсе) */
#define FF_USE_LFN      3       /* длинные имена, буфер - из кучи ядра */
#define FF_MAX_LFN      255
#define FF_LFN_UNICODE  2       /* имена в API - UTF-8, как во всей MyOS */
#define FF_LFN_BUF      255
#define FF_SFN_BUF      12
#define FF_STRF_ENCODE  3
#define FF_FS_RPATH     0

#define FF_VOLUMES      1
#define FF_STR_VOLUME_ID 0
#define FF_VOLUME_STRS  "RAM"
#define FF_MULTI_PARTITION 0    /* раздел уже выделен блочным слоем MyOS */

#define FF_MIN_SS       512
#define FF_MAX_SS       512
#define FF_USE_TRIM     0
#define FF_FS_NOFSINFO  0

#define FF_FS_TINY      0
#define FF_FS_EXFAT     1       /* главное, ради чего FatFs здесь */
#define FF_FS_NORTC     0       /* время - get_fattime() (fs/exfat.c) */
#define FF_NORTC_MON    1
#define FF_NORTC_MDAY   1
#define FF_NORTC_YEAR   2026
#define FF_FS_LOCK      0
#define FF_FS_REENTRANT 0       /* снаружи - замок g_vfs_mutex */
#define FF_FS_TIMEOUT   1000
#define FF_SYNC_t       void *
