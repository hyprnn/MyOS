/*
 * kernel/pmm.c - карта памяти, страничный аллокатор, пул.
 * Часть MyOS; общие объявления - в myos.h.
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
BOOLEAN g_kmm_ready = FALSE;


/* Скопировать итоговую карту памяти из буфера прошивки в наш
   статический массив (сразу после ExitBootServices) */
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

        if (g_kmm_map[i].type != 7)
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
       регион выше 1 МиБ, в который она целиком влезает */
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

        if ((end - start) / KMM_PAGE >= g_kmm_bitmap_pages) {
            g_kmm_bitmap_phys = start;
            break;
        }
    }

    if (g_kmm_bitmap_phys == 0)
        return FALSE;

    g_kmm_bitmap = (UINT64 *)(UINTN)g_kmm_bitmap_phys;

    /* всё занято... */
    for (UINT64 w = 0; w < words; w++)
        g_kmm_bitmap[w] = ~0ull;

    /* ...кроме свободной RAM (EfiConventionalMemory) */
    g_kmm_usable_pages = 0;

    for (UINTN i = 0; i < g_kmm_map_count; i++) {

        if (g_kmm_map[i].type != 7)
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
            }
        }
    }

    /* страницы самой битовой карты - заняты */
    UINT64 bm_first = g_kmm_bitmap_phys / KMM_PAGE;

    for (UINT64 p = bm_first; p < bm_first + g_kmm_bitmap_pages; p++)
        pmm_set(p);

    g_kmm_free_pages = g_kmm_usable_pages - g_kmm_bitmap_pages;
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

    volatile UINT64 *q = (volatile UINT64 *)(UINTN)phys;
    UINT64 n = count * KMM_PAGE / 8u;

    for (UINT64 i = 0; i < n; i++)
        q[i] = 0;

    return phys;
}

UINT64 g_kpool_allocs = 0;

VOID *kpool_alloc(UINTN size)
{
    UINT64 pages = ((UINT64)size + 16u + KMM_PAGE - 1u) / KMM_PAGE;
    UINT64 phys = pmm_alloc_pages(pages, 0);

    if (phys == 0)
        return NULL;

    UINT64 *hdr = (UINT64 *)(UINTN)phys;

    hdr[0] = KPOOL_MAGIC;
    hdr[1] = pages;

    g_kpool_allocs++;

    return (VOID *)(UINTN)(phys + 16u);
}

BOOLEAN kpool_free(VOID *ptr)
{
    if (ptr == NULL)
        return FALSE;

    UINT64 *hdr = (UINT64 *)((UINT8 *)ptr - 16);

    if (hdr[0] != KPOOL_MAGIC)
        return FALSE;

    UINT64 pages = hdr[1];

    hdr[0] = 0;

    pmm_free_pages((UINT64)(UINTN)hdr, pages);

    if (g_kpool_allocs > 0)
        g_kpool_allocs--;

    return TRUE;
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
