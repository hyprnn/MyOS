/*
 * kernel/kmalloc.c - куча ядра: kmalloc/kfree.
 * Часть MyOS; общие объявления - в myos.h.
 *
 * Раньше любая, даже 20-байтная, просьба о памяти (AllocatePool)
 * получала целую страницу 4 КиБ. Теперь - как в настоящих ядрах:
 *
 *   * МЕЛКИЕ блоки (до 1024 байт) - из "слабов". Слаб - это одна
 *     страница, нарезанная на одинаковые кусочки одного размера:
 *     16, 32, 64, 128, 256, 512 или 1024 байта. Просьба округляется
 *     вверх до ближайшего размера и получает свободный кусочек.
 *     Свободные кусочки связаны в список прямо внутри себя (первые
 *     8 байт свободного кусочка - адрес следующего свободного),
 *     так что учёт ничего не стоит.
 *   * КРУПНЫЕ блоки (больше 1024 байт) - сразу целыми страницами
 *     из страничного аллокатора (pmm), подряд.
 *
 * В начале каждой страницы - заголовок (64 байта) с "магическим"
 * числом и типом. kfree по адресу блока находит начало его
 * страницы (просто обнуляет младшие 12 бит адреса) и по заголовку
 * понимает, что это и куда вернуть. Чужой или уже освобождённый
 * указатель узнаётся по неправильному магическому числу /
 * отметке "свободно" - такое пишется в лог, а не портит память.
 *
 * Вся память - из прямого отображения (P2V), то есть блок кучи
 * можно отдать и устройству: физический адрес = V2P(указатель).
 */
#include "myos.h"

#define KH_MAGIC_SLAB  0x42414C534D4F594Dull   /* "MYOSMSLAB" */
#define KH_MAGIC_BIG   0x4749424D534F594Dull   /* "MYOSMBIG" */
#define KH_FREE_MARK   0xF4EEF4EEF4EEF4EEull   /* во 2-м слове свободного кусочка */
#define KH_HDR         64u                     /* заголовок страницы */

typedef struct KH_SLAB {
    UINT64 magic;
    UINT32 size;              /* размер кусочка */
    UINT32 cls;               /* номер класса */
    UINT32 total;             /* кусочков в странице */
    UINT32 used;              /* из них занято */
    struct KH_SLAB *next;     /* следующий слаб того же класса */
    VOID  *free;              /* список свободных кусочков */
    UINT64 pages;             /* для крупных блоков: сколько страниц */
} KH_SLAB;

#define KH_CLASSES 7
static const UINT32 kh_sizes[KH_CLASSES] = { 16, 32, 64, 128, 256, 512, 1024 };
static KH_SLAB *kh_slabs[KH_CLASSES];

/* статистика для команды mem */
UINT64 g_kheap_slab_pages = 0;
UINT64 g_kheap_big_pages = 0;
UINT64 g_kheap_live = 0;          /* сколько блоков сейчас выдано */
UINT64 g_kheap_live_bytes = 0;    /* их запрошенный суммарный размер (прибл.) */
UINT64 g_kheap_bad_frees = 0;

static KH_SLAB *kh_new_slab(UINTN cls)
{
    UINT64 phys = pmm_alloc_pages(1, 0);

    if (phys == 0)
        return NULL;

    KH_SLAB *s = (KH_SLAB *)P2V(phys);
    UINT32 size = kh_sizes[cls];

    s->magic = KH_MAGIC_SLAB;
    s->size = size;
    s->cls = (UINT32)cls;
    s->total = (4096u - KH_HDR) / size;
    s->used = 0;
    s->pages = 1;
    s->free = NULL;

    /* нарезать и связать кусочки (с конца - чтобы первым выдавался
       кусочек с меньшим адресом) */
    for (UINT32 i = s->total; i > 0; i--) {
        UINT64 *obj = (UINT64 *)((UINT8 *)s + KH_HDR + (i - 1u) * size);
        obj[0] = (UINT64)(UINTN)s->free;
        if (size >= 16)
            obj[1] = KH_FREE_MARK;
        s->free = obj;
    }

    s->next = kh_slabs[cls];
    kh_slabs[cls] = s;

    g_kheap_slab_pages++;

    return s;
}

VOID *kmalloc(UINTN size)
{
    if (size == 0)
        size = 1;

    /* крупный блок - целыми страницами */
    if (size > kh_sizes[KH_CLASSES - 1]) {

        UINT64 pages = (size + KH_HDR + 4095u) / 4096u;
        UINT64 phys = pmm_alloc_pages(pages, 0);

        if (phys == 0)
            return NULL;

        KH_SLAB *h = (KH_SLAB *)P2V(phys);

        h->magic = KH_MAGIC_BIG;
        h->pages = pages;
        h->size = (UINT32)size;
        h->used = 1;

        g_kheap_big_pages += pages;
        g_kheap_live++;
        g_kheap_live_bytes += size;

        return (UINT8 *)h + KH_HDR;
    }

    UINTN cls = 0;

    while (kh_sizes[cls] < size)
        cls++;

    KH_SLAB *s = kh_slabs[cls];

    while (s != NULL && s->free == NULL)
        s = s->next;

    if (s == NULL) {
        s = kh_new_slab(cls);
        if (s == NULL)
            return NULL;
    }

    UINT64 *obj = (UINT64 *)s->free;

    s->free = (VOID *)(UINTN)obj[0];
    s->used++;

    obj[0] = 0;
    if (s->size >= 16)
        obj[1] = 0;

    g_kheap_live++;
    g_kheap_live_bytes += s->size;

    return obj;
}

/* То же, но память обнулена */
VOID *kzalloc(UINTN size)
{
    UINT8 *p = (UINT8 *)kmalloc(size);

    if (p != NULL)
        for (UINTN i = 0; i < size; i++)
            p[i] = 0;

    return p;
}

BOOLEAN kfree(VOID *ptr)
{
    if (ptr == NULL)
        return TRUE;

    UINT64 a = (UINT64)(UINTN)ptr;

    /* указатель кучи всегда в прямом отображении */
    if (a < MYOS_HHDM_BASE || a >= MYOS_KSTACK_BASE) {
        g_kheap_bad_frees++;
        klog("kfree: %p is not a heap pointer\n", ptr);
        return FALSE;
    }

    KH_SLAB *s = (KH_SLAB *)(UINTN)(a & ~4095ull);

    if (s->magic == KH_MAGIC_BIG && a == (UINT64)(UINTN)s + KH_HDR) {

        UINT64 pages = s->pages;

        s->magic = 0;
        pmm_free_pages(V2P(s), pages);

        g_kheap_big_pages -= pages;
        g_kheap_live--;
        g_kheap_live_bytes -= s->size;

        return TRUE;
    }

    if (s->magic != KH_MAGIC_SLAB ||
        a < (UINT64)(UINTN)s + KH_HDR ||
        ((a - (UINT64)(UINTN)s - KH_HDR) % s->size) != 0) {
        g_kheap_bad_frees++;
        klog("kfree: %p was not returned by kmalloc\n", ptr);
        return FALSE;
    }

    UINT64 *obj = (UINT64 *)ptr;

    if (s->size >= 16 && obj[1] == KH_FREE_MARK) {
        /* уже свободен? проверим честно по списку */
        for (VOID *f = s->free; f != NULL; f = (VOID *)(UINTN)((UINT64 *)f)[0]) {
            if (f == ptr) {
                g_kheap_bad_frees++;
                klog("kfree: double free of %p\n", ptr);
                return FALSE;
            }
        }
    }

    obj[0] = (UINT64)(UINTN)s->free;
    if (s->size >= 16)
        obj[1] = KH_FREE_MARK;
    s->free = obj;
    s->used--;

    g_kheap_live--;
    g_kheap_live_bytes -= s->size;

    /* Слаб опустел, и он не единственный в своём классе - страницу
       обратно в pmm (один пустой оставляем про запас) */
    if (s->used == 0) {

        KH_SLAB **pp = &kh_slabs[s->cls];
        UINTN n = 0;

        for (KH_SLAB *t = kh_slabs[s->cls]; t != NULL; t = t->next)
            n++;

        if (n > 1) {
            while (*pp != s)
                pp = &(*pp)->next;
            *pp = s->next;
            s->magic = 0;
            pmm_free_pages(V2P(s), 1);
            g_kheap_slab_pages--;
        }
    }

    return TRUE;
}

/*
 * Самопроверка для команды mem: выделить пачку блоков разных
 * размеров, записать в каждый свой узор, проверить, освободить и
 * убедиться, что куча вернулась в прежнее состояние.
 */
BOOLEAN kmalloc_selftest(char *report, UINTN cap)
{
    static VOID *ptrs[64];
    UINT64 live0 = g_kheap_live;
    BOOLEAN ok = TRUE;

    for (UINTN i = 0; i < 64; i++) {

        UINTN sz = 8u + (i * 37u) % 1500u;

        ptrs[i] = kmalloc(sz);

        if (ptrs[i] == NULL) {
            ok = FALSE;
            break;
        }

        for (UINTN k = 0; k < sz; k++)
            ((UINT8 *)ptrs[i])[k] = (UINT8)(i + k);
    }

    for (UINTN i = 0; ok && i < 64; i++) {

        UINTN sz = 8u + (i * 37u) % 1500u;

        for (UINTN k = 0; k < sz; k++)
            if (((UINT8 *)ptrs[i])[k] != (UINT8)(i + k))
                ok = FALSE;
    }

    for (UINTN i = 0; i < 64; i++) {
        if (ptrs[i] != NULL)
            kfree(ptrs[i]);
        ptrs[i] = NULL;
    }

    if (g_kheap_live != live0)
        ok = FALSE;

    ksnprintf(report, cap, "64 blocks of 8..1507 bytes: %s",
              ok ? "written, verified, freed: OK" : "FAILED");

    return ok;
}
