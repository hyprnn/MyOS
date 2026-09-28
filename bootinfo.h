/*
 * bootinfo.h - "паспорт загрузки": то, что загрузчик (loader/,
 * BOOTX64.EFI) передаёт ядру (kernel.elf).
 *
 * Загрузчик - обычная UEFI-программа: пока прошивка жива, он
 * собирает всё, что ядру понадобится и что потом уже не у кого
 * спросить (карта памяти, видеорежим, адрес ACPI, время), кладёт
 * это в одну структуру MYOS_BOOT_INFO, вызывает ExitBootServices
 * и прыгает в ядро, передав адрес структуры в регистре RDI (то
 * есть первым аргументом C-функции kmain).
 *
 * Ядро с первой инструкции о прошивке ничего не знает - только
 * эту структуру. Поэтому все поля здесь - простые числа и
 * массивы, никаких указателей на структуры прошивки.
 *
 * Этот файл общий для загрузчика и ядра: менять поля можно
 * только в обоих сразу (и увеличивать MYOS_BOOT_VERSION).
 */
#ifndef MYOS_BOOTINFO_H
#define MYOS_BOOTINFO_H

/* "MYOSBOOT" в ASCII - ядро проверяет, что ему дали именно
   паспорт, а не мусор */
#define MYOS_BOOT_MAGIC    0x544F4F42534F594Dull
#define MYOS_BOOT_VERSION  2   /* 2: EFI_TIME по спецификации (16 байт) */

/*
 * Раскладка виртуальной памяти (одинаковая для загрузчика и ядра).
 *
 *   0x0000000000000000 .. 0x00007FFFFFFFFFFF  нижняя половина: пусто
 *        (здесь будут жить программы - этап 6; сейчас обращение
 *         сюда, включая NULL, - это Page Fault с понятным экраном)
 *   0xFFFF800000000000 ..  "прямое отображение" (HHDM - higher half
 *        direct map): физический адрес X виден по адресу
 *        HHDM + X. Так ядро добирается до любой RAM и устройств.
 *   0xFFFFFE8000000000 ..  стеки ядра, каждый - с незаполненной
 *        "защитной" страницей снизу (переполнение = Page Fault /
 *        Double Fault, а не тихая порча памяти)
 *   0xFFFFFFFF80000000 ..  сам образ ядра (код, данные)
 */
#define MYOS_HHDM_BASE       0xFFFF800000000000ull
#define MYOS_KSTACK_BASE     0xFFFFFE8000000000ull
#define MYOS_KERNEL_VIRT     0xFFFFFFFF80000000ull

/*
 * Свои типы памяти в карте UEFI. Спецификация отдаёт диапазон
 * 0x80000000..0xFFFFFFFF загрузчикам ОС - прошивка просто хранит
 * наш номер. По нему ядро понимает, что за участок:
 *   KERNEL - образ ядра, паспорт, копия карты памяти, стартовый
 *            стек: не отдавать никогда;
 *   LOADER_TEMP - временные таблицы страниц загрузчика: ядро
 *            забирает их себе, как только включит свои.
 */
#define MYOS_MEM_KERNEL       0x80000001u
#define MYOS_MEM_LOADER_TEMP  0x80000002u

typedef struct {
    UINT64 magic;                /* MYOS_BOOT_MAGIC */
    UINT64 version;              /* MYOS_BOOT_VERSION */

    /* --- экран (Graphics Output Protocol) --- */
    UINT64 fb_phys;              /* физический адрес видеопамяти */
    UINT64 fb_size;              /* байт */
    UINT32 fb_width;
    UINT32 fb_height;
    UINT32 fb_stride;            /* пикселей в строке (>= width) */
    UINT32 fb_format;            /* EFI_GRAPHICS_PIXEL_FORMAT */

    /* --- карта памяти (итоговая, после ExitBootServices она уже
       не меняется). Лежит в памяти типа MYOS_MEM_KERNEL. --- */
    UINT64 mmap_phys;
    UINT64 mmap_size;            /* байт */
    UINT64 mmap_desc_size;       /* размер одной записи */
    UINT32 mmap_desc_version;
    UINT32 _pad0;

    /* --- образ ядра --- */
    UINT64 kernel_phys;          /* где лежит физически */
    UINT64 kernel_virt;          /* = MYOS_KERNEL_VIRT */
    UINT64 kernel_size;          /* байт, кратно 4 КиБ */

    /* --- стартовый стек ядра (физически) --- */
    UINT64 stack_phys;
    UINT64 stack_size;

    /* --- таблицы страниц загрузчика (CR3 в момент прыжка) --- */
    UINT64 loader_cr3;

    /* --- ACPI: адрес RSDP (0 - не нашли) и его версия --- */
    UINT64 rsdp_phys;
    UINT32 acpi_version;         /* 1 или 2 */
    UINT32 _pad1;

    /* --- время --- */
    UINT64 tsc_hz_stall;         /* частота TSC, замеренная по Stall
                                    прошивки (контроль для PIT) */
    UINT64 tsc_at_jump;          /* rdtsc() прямо перед прыжком */
    EFI_TIME boot_time;          /* часы в момент загрузки */
    UINT32 have_boot_time;
    UINT32 _pad2;

    /* --- кто нас загрузил (для fetch) --- */
    CHAR16 fw_vendor[48];
    UINT32 fw_revision;
    UINT32 uefi_revision;        /* SystemTable->Hdr.Revision */

    /* --- что сделал загрузчик с xHCI --- */
    UINT32 xhci_found;
    UINT32 xhci_disconnected;    /* драйвер прошивки отключён */
    UINT32 xhci_handoff_ok;      /* BIOS отдал контроллер */
    UINT32 _pad3;

    /* --- откуда взяли ядро --- */
    CHAR16 kernel_path[64];
    UINT64 kernel_file_size;

    /* --- короткий журнал загрузчика (ASCII, для команды boot) --- */
    char   log[2048];
    UINT32 log_len;
    UINT32 _pad4;
} MYOS_BOOT_INFO;

#endif
