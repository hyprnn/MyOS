/*
 * kernel/pmm.c - карта памяти и страничный аллокатор (pmm =
 * physical memory manager).
 * Часть MyOS; общие объявления - в myos.h.
 *
 * Учёт - битовая карта: один бит на каждую 4-КиБ страницу
 * физической памяти (1 = занята). Выдаются ФИЗИЧЕСКИЕ адреса;
 * чтобы что-то туда записать, адрес переводят в указатель через
 * P2V() (прямое отображение, см. vmm.c).
 *
 * Что считается свободным (раньше - только EfiConventionalMemory):
 *   * Conventional - свободная RAM;
 *   * BootServicesCode/Data - память прошивки, которая после
 *     ExitBootServices по правилам UEFI принадлежит ОС (раньше её
 *     трогать было нельзя: там жили наш стек и таблицы страниц
 *     прошивки - теперь у ядра всё своё);
 *   * LoaderCode/Data - загрузчик своё дело сделал;
 * НО: внутри LoaderData лежит то, что загрузчик выделил для ядра, -
 * образ ядра, паспорт, карта памяти, стартовый стек, временные
 * таблицы страниц. Их список загрузчик передаёт явно
 * (g_boot.reserved): KEEP не отдаём никогда, TEMP (таблицы
 * загрузчика) - отдаём, когда ядро включит свои
 * (pmm_release_loader_temp).
 * Не трогаем: Runtime-память прошивки, таблицы ACPI, MMIO.
 */
#include "myos.h"

KMM_REGION g_kmm_map[KMM_MAX_REGIONS];
UINTN  g_kmm_map_count = 0;
UINTN  g_kmm_map_dropped = 0;

UINT64 *g_kmm_bitmap = NULL;
UINT64 g_kmm_bitmap_phys = 0;
UINT64 g_kmm_bitmap_pages = 0;
UINT64 g_kmm_total_pages = 0;   /* сколько страниц покрывает
                                           битовая карта */
UINT64 g_kmm_usable_pages = 0;  /* сколько из них было
                                           свободной RAM */
UINT64 g_kmm_free_pages = 0;
UINT64 g_kmm_reclaimed_pages = 0;  /* из них - бывшая память
                                           прошивки и загрузчика */
BOOLEAN g_kmm_ready = FALSE;

/* Эти типы памяти ядро отдаёт в аллокатор сразу */
BOOLEAN pmm_free_type(UINT32 t)
{
    return t == 7 ||            /* Conventional */
           t == 1 || t == 2 ||  /* LoaderCode / LoaderData */
           t == 3 || t == 4;    /* BootServicesCode / Data */
}


/* Скопировать итоговую карту памяти (её собрал загрузчик) в наш
   статический массив */
void pmm_save_map(
    VOID *map_buf,
    UINTN map_size,
    UINTN desc_size
)
{
    g_kmm_map_count = 0;
    g_kmm_map_dropped = 0;

    if (map_buf == NULL || desc_size == 0)
        return;

    UINTN n = map_size / desc_size;

    for (UINTN i = 0; i < n; i++) {

        UINT8 *d = (UINT8 *)map_buf + i * desc_size;

        if (g_kmm_map_count >= KMM_MAX_REGIONS) {
            g_kmm_map_dropped++;
            continue;
        }

        g_kmm_map[g_kmm_map_count].type =
            *(UINT32 *)(d + KMM_DESC_TYPE);
        g_kmm_map[g_kmm_map_count].phys =
            *(UINT64 *)(d + KMM_DESC_PHYS);
        g_kmm_map[g_kmm_map_count].pages =
            *(UINT64 *)(d + KMM_DESC_PAGES);

        g_kmm_map_count++;
    }
}


BOOLEAN pmm_init(void)
{
    UINT64 max_end = 0;

    for (UINTN i = 0; i < g_kmm_map_count; i++) {

        if (!pmm_free_type(g_kmm_map[i].type))
            continue;

        UINT64 end =
            g_kmm_map[i].phys + g_kmm_map[i].pages * KMM_PAGE;

        if (end > max_end)
            max_end = end;
    }

    if (max_end == 0)
        return FALSE;

    g_kmm_total_pages = max_end / KMM_PAGE;

    UINT64 words = (g_kmm_total_pages + 63u) / 64u;
    UINT64 bytes = words * 8u;

    g_kmm_bitmap_pages = (bytes + KMM_PAGE - 1u) / KMM_PAGE;

    /* Где разместить саму битовую карту: первый свободный
       (Conventional) регион выше 1 МиБ, в который она влезает */
    g_kmm_bitmap_phys = 0;

    for (UINTN i = 0; i < g_kmm_map_count; i++) {

        if (g_kmm_map[i].type != 7)
            continue;

        UINT64 start = g_kmm_map[i].phys;
        UINT64 end = start + g_kmm_map[i].pages * KMM_PAGE;

        if (start < 0x100000ull)
            start = 0x100000ull;

        if (start >= end)
            continue;

        /* не поверх того, что загрузчик выделил ядру (обычно это
           другой тип памяти, но проверим - дёшево) */
        BOOLEAN clash = FALSE;
        UINT64 bm_end = start + g_kmm_bitmap_pages * KMM_PAGE;

        for (UINT32 r = 0; r < g_boot.nreserved && r < MYOS_MAX_RESERVED; r++) {
            UINT64 rs = g_boot.reserved[r].phys;
            UINT64 re = rs + g_boot.reserved[r].pages * KMM_PAGE;
            if (rs < bm_end && re > start)
                clash = TRUE;
        }

        if (clash)
            continue;

        if ((end - start) / KMM_PAGE >= g_kmm_bitmap_pages) {
            g_kmm_bitmap_phys = start;
            break;
        }
    }

    if (g_kmm_bitmap_phys == 0)
        return FALSE;

    g_kmm_bitmap = (UINT64 *)P2V(g_kmm_bitmap_phys);

    /* всё занято... */
    for (UINT64 w = 0; w < words; w++)
        g_kmm_bitmap[w] = ~0ull;

    /* ...кроме свободной RAM и памяти прошивки/загрузчика */
    g_kmm_usable_pages = 0;
    g_kmm_reclaimed_pages = 0;

    for (UINTN i = 0; i < g_kmm_map_count; i++) {

        if (!pmm_free_type(g_kmm_map[i].type))
            continue;

        UINT64 first = g_kmm_map[i].phys / KMM_PAGE;
        UINT64 count = g_kmm_map[i].pages;

        for (UINT64 p = first; p < first + count; p++) {

            if (p < 256u)       /* первый мегабайт не выдаём */
                continue;

            if (p >= g_kmm_total_pages)
                break;

            if (pmm_test(p)) {
                pmm_clear(p);
                g_kmm_usable_pages++;
                if (g_kmm_map[i].type != 7)
                    g_kmm_reclaimed_pages++;
            }
        }
    }

    g_kmm_free_pages = g_kmm_usable_pages;

    /* страницы самой битовой карты - заняты */
    UINT64 bm_first = g_kmm_bitmap_phys / KMM_PAGE;

    for (UINT64 p = bm_first; p < bm_first + g_kmm_bitmap_pages; p++) {
        if (p < g_kmm_total_pages && !pmm_test(p)) {
            pmm_set(p);
            g_kmm_free_pages--;
        }
    }

    /* всё, что загрузчик выделил для ядра (образ, паспорт, карта,
       стек, таблицы загрузчика), - тоже занято */
    for (UINT32 r = 0; r < g_boot.nreserved && r < MYOS_MAX_RESERVED; r++) {

        UINT64 first = g_boot.reserved[r].phys / KMM_PAGE;

        for (UINT64 p = first; p < first + g_boot.reserved[r].pages; p++) {
            if (p < g_kmm_total_pages && !pmm_test(p)) {
                pmm_set(p);
                g_kmm_free_pages--;
                g_kmm_usable_pages--;
                if (g_kmm_reclaimed_pages > 0)
                    g_kmm_reclaimed_pages--;
            }
        }
    }
    g_kmm_ready = TRUE;

    return TRUE;
}


/*
 * Выделить count подряд идущих страниц. limit - верхняя граница
 * физического адреса (0 = без ограничения): например, xHCI без
 * поддержки 64-битной адресации (бит AC64) может обращаться
 * только к памяти ниже 4 ГиБ. Первый подходящий участок (first
 * fit), полностью занятые 64-страничные слова пропускаются
 * целиком. Возвращает физический адрес или 0.
 */
UINT64 pmm_alloc_pages(UINT64 count, UINT64 limit)
{
    if (!g_kmm_ready || count == 0 || count > g_kmm_free_pages)
        return 0;

    UINT64 limit_pages = g_kmm_total_pages;

    if (limit != 0 && limit / KMM_PAGE < limit_pages)
        limit_pages = limit / KMM_PAGE;

    UINT64 run = 0;
    UINT64 run_start = 0;

    for (UINT64 p = 256; p < limit_pages; p++) {

        if (
            (p & 63u) == 0 &&
            run == 0 &&
            g_kmm_bitmap[p >> 6] == ~0ull
        ) {
            p += 63;
            continue;
        }

        if (pmm_test(p)) {
            run = 0;
            continue;
        }

        if (run == 0)
            run_start = p;

        run++;

        if (run == count) {

            for (UINT64 q = run_start; q < run_start + count; q++)
                pmm_set(q);

            g_kmm_free_pages -= count;

            return run_start * KMM_PAGE;
        }
    }

    return 0;
}


/*
 * Одна страница НИЖЕ 1 МБ - для трамплина запуска ядер процессора
 * (smp.c): сигнал SIPI умеет указать только такой адрес. Обычный
 * pmm_alloc_pages первый мегабайт не выдаёт никогда (там живёт
 * всякое старое - таблицы BIOS, EBDA), поэтому здесь - отдельный
 * поиск по карте памяти: только свободная RAM (Conventional) и
 * бывшая память прошивки (BootServices), не страница 0 и не то, что
 * загрузчик оставил ядру. Выданная страница больше никому не нужна.
 */
UINT64 pmm_alloc_low_page(void)
{
    static UINT64 given;

    if (given != 0)
        return given;

    for (UINTN i = 0; i < g_kmm_map_count; i++) {

        UINT32 t = g_kmm_map[i].type;

        if (t != 7 && t != 3 && t != 4)
            continue;

        UINT64 first = g_kmm_map[i].phys / KMM_PAGE;
        UINT64 end = first + g_kmm_map[i].pages;

        for (UINT64 p = first; p < end && p < 0x9Fu; p++) {

            if (p < 8u)          /* первые 32 КБ - не трогаем (таблицы BIOS) */
                continue;

            BOOLEAN busy = FALSE;

            for (UINT32 r = 0; r < g_boot.nreserved && r < MYOS_MAX_RESERVED; r++) {
                UINT64 rf = g_boot.reserved[r].phys / KMM_PAGE;
                if (p >= rf && p < rf + g_boot.reserved[r].pages)
                    busy = TRUE;
            }

            if (!busy) {
                given = p * KMM_PAGE;
                return given;
            }
        }
    }

    return 0;
}

void pmm_free_pages(UINT64 phys, UINT64 count)
{
    if (!g_kmm_ready)
        return;

    UINT64 first = phys / KMM_PAGE;

    for (UINT64 p = first; p < first + count; p++) {

        if (p < 256u || p >= g_kmm_total_pages)
            continue;

        if (pmm_test(p)) {
            pmm_clear(p);
            g_kmm_free_pages++;
        }
    }
}


/* То же + обнулить (для DMA-структур xHCI это обязательно:
   контроллер трактует мусор в Cycle-битах как настоящие TRB) */
UINT64 pmm_alloc_zeroed(UINT64 count, UINT64 limit)
{
    UINT64 phys = pmm_alloc_pages(count, limit);

    if (phys == 0)
        return 0;

    volatile UINT64 *q = (volatile UINT64 *)P2V(phys);
    UINT64 n = count * KMM_PAGE / 8u;

    for (UINT64 i = 0; i < n; i++)
        q[i] = 0;

    return phys;
}

/*
 * Отдать аллокатору временные таблицы страниц загрузчика (TEMP из
 * списка паспорта). Вызывать ТОЛЬКО после того, как ядро включило
 * свои таблицы: до этого по ним работает процессор.
 */
UINT64 pmm_release_loader_temp(void)
{
    UINT64 n = 0;

    for (UINT32 r = 0; r < g_boot.nreserved && r < MYOS_MAX_RESERVED; r++) {

        if (g_boot.reserved[r].kind != MYOS_RES_TEMP)
            continue;

        UINT64 first = g_boot.reserved[r].phys / KMM_PAGE;

        for (UINT64 p = first; p < first + g_boot.reserved[r].pages; p++) {

            if (p < 256u || p >= g_kmm_total_pages || !pmm_test(p))
                continue;

            pmm_clear(p);
            g_kmm_free_pages++;
            g_kmm_usable_pages++;
            g_kmm_reclaimed_pages++;
            n++;
        }
    }

    return n;
}


const char *kmm_type_name(UINT32 t)
{
    switch (t) {
    case 0:  return "Reserved";
    case 1:  return "LoaderCode";
    case 2:  return "LoaderData";
    case 3:  return "BootServicesCode";
    case 4:  return "BootServicesData";
    case 5:  return "RuntimeServicesCode";
    case 6:  return "RuntimeServicesData";
    case 7:  return "Conventional (free RAM)";
    case 8:  return "Unusable";
    case 9:  return "ACPIReclaim";
    case 10: return "ACPINVS";
    case 11: return "MMIO";
    case 12: return "MMIOPortSpace";
    case 13: return "PalCode";
    case 14: return "Persistent";
    default: return "Other";
    }
}
