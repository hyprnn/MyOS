/*
 * kernel/vmm.c - виртуальная память: свои таблицы страниц ядра.
 * Часть MyOS; общие объявления - в myos.h, раскладка адресов -
 * в bootinfo.h.
 *
 * Как устроена страничная трансляция x86-64 (4 уровня):
 *
 *   виртуальный адрес (48 значащих бит) режется на куски:
 *     [47:39] индекс в PML4  -> таблица PDPT
 *     [38:30] индекс в PDPT  -> таблица PD   (или сразу 1 ГиБ)
 *     [29:21] индекс в PD    -> таблица PT   (или сразу 2 МиБ)
 *     [20:12] индекс в PT    -> страница 4 КиБ
 *     [11:0]  смещение внутри страницы
 *   Каждая таблица - 512 записей по 8 байт = одна страница.
 *   В записи: физический адрес следующей таблицы (или страницы) и
 *   флаги: P (есть), W (можно писать), U (доступно программам),
 *   PWT/PCD/PAT (тип кэширования), PS (это большая страница),
 *   NX (бит 63: отсюда нельзя исполнять код).
 *
 * Что мы строим (vmm_init):
 *   * образ ядра - 4-КиБ страницами, у каждого раздела свои права:
 *     код только читать и исполнять (запись в код = Page Fault),
 *     константы - только читать, данные - без исполнения;
 *   * прямое отображение (HHDM) всей физической памяти: RAM -
 *     обычная кэшируемая (WB), всё остальное ниже 4 ГиБ и дыры в
 *     карте памяти - некэшируемое (UC, так надо для регистров
 *     устройств), видеопамять - write-combining (WC: запись идёт
 *     пачками, на реальном железе GUI от этого в разы быстрее);
 *   * нижняя половина пуста: NULL и любые "забытые" физические
 *     адреса без P2V дают понятный Page Fault, а не тихую порчу.
 */
#include "myos.h"

/* биты записи таблицы страниц */
#define PTE_P      (1ull << 0)
#define PTE_W      (1ull << 1)
#define PTE_U      (1ull << 2)
#define PTE_PWT    (1ull << 3)
#define PTE_PCD    (1ull << 4)
#define PTE_PS     (1ull << 7)     /* в PD/PDPT: большая страница */
#define PTE_PAT4K  (1ull << 7)     /* в PT: бит PAT */
#define PTE_PAT2M  (1ull << 12)    /* в PD с PS: бит PAT */
#define PTE_NX     (1ull << 63)
#define PTE_ADDR   0x000FFFFFFFFFF000ull

UINT64  g_vmm_pml4_phys = 0;
UINT64  g_vmm_table_pages = 0;     /* сколько страниц ушло на таблицы */
UINT64  g_vmm_hhdm_top = 0;        /* до какого адреса отображена память */
UINT64  g_vmm_pages_2m = 0;        /* статистика для команды vm */
UINT64  g_vmm_pages_4k = 0;
BOOLEAN g_vmm_nx = FALSE;          /* процессор умеет NX */
BOOLEAN g_vmm_pat = FALSE;         /* PAT перенастроен: есть WC */
BOOLEAN g_vmm_ready = FALSE;

/* Стеки ядра с защитными страницами (см. vmm_alloc_stack) */
KSTACK_INFO g_kstacks[KSTACK_MAX];
UINTN g_kstack_count = 0;
static UINT64 g_kstack_next = MYOS_KSTACK_BASE;

/* границы разделов ядра - из kernel/kernel.ld */
extern char __kernel_start[], __kernel_text_end[];
extern char __kernel_rodata_end[], __kernel_end[];


/* ----------------------------------------------------------------
 * Атрибуты страницы: VMM_* -> биты записи
 *
 * PAT (Page Attribute Table) - таблица из 8 типов кэширования в
 * MSR 0x277; биты PAT/PCD/PWT записи выбирают номер типа. По
 * умолчанию там: 0=WB 1=WT 2=UC- 3=UC ... Мы меняем тип №1 на WC
 * (write-combining), и тогда:
 *   WB = ни одного бита, WC = PWT, UC = PCD|PWT.
 * ---------------------------------------------------------------- */
static UINT64 vmm_bits(UINT32 attr, BOOLEAN big)
{
    UINT64 e = PTE_P;

    (void)big;

    if (attr & VMM_W)
        e |= PTE_W;

    if (!(attr & VMM_X) && g_vmm_nx)
        e |= PTE_NX;

    if (attr & VMM_UC)
        e |= PTE_PCD | PTE_PWT;
    else if (attr & VMM_WC)
        e |= g_vmm_pat ? PTE_PWT : (PTE_PCD | PTE_PWT);

    return e;
}

static UINT64 *vmm_table(UINT64 phys)
{
    return (UINT64 *)P2V(phys & PTE_ADDR);
}

static UINT64 vmm_new_table(void)
{
    UINT64 phys = pmm_alloc_zeroed(1, 0);

    if (phys != 0)
        g_vmm_table_pages++;

    return phys;
}

static void vmm_invlpg(UINT64 virt)
{
    __asm__ __volatile__("invlpg (%0)" : : "r"(virt) : "memory");
}

/*
 * Большую (2 МиБ) страницу разбить на 512 маленьких с теми же
 * правами - нужно, когда внутри неё один участок должен получить
 * другие права (например, видеопамять - WC, а соседи - UC).
 */
static BOOLEAN vmm_split_2m(UINT64 *pde)
{
    UINT64 big = *pde;
    UINT64 pt = vmm_new_table();

    if (pt == 0)
        return FALSE;

    UINT64 *t = vmm_table(pt);
    UINT64 base = big & 0x000FFFFFFFE00000ull;
    UINT64 flags = big & (PTE_W | PTE_U | PTE_PWT | PTE_PCD | PTE_NX);

    if (big & PTE_PAT2M)
        flags |= PTE_PAT4K;

    for (UINT64 i = 0; i < 512; i++)
        t[i] = (base + i * 4096u) | PTE_P | flags;

    /* верхний уровень - без ограничений: права решает нижний */
    *pde = pt | PTE_P | PTE_W;

    return TRUE;
}

/*
 * Найти (и при create - создать) запись нужного уровня для
 * виртуального адреса. level: 1 = PT (4 КиБ), 2 = PD (2 МиБ).
 * Промежуточные таблицы создаются с флагами P|W: права доступа
 * ограничивает самая нижняя запись.
 */
static UINT64 *vmm_entry(UINT64 virt, UINTN level, BOOLEAN create)
{
    if (g_vmm_pml4_phys == 0)
        return NULL;

    UINT64 *t = vmm_table(g_vmm_pml4_phys);

    for (UINTN lvl = 4; lvl > level; lvl--) {

        UINTN idx = (UINTN)((virt >> (12u + 9u * (lvl - 1u))) & 511u);
        UINT64 *e = &t[idx];

        if (!(*e & PTE_P)) {

            if (!create)
                return NULL;

            UINT64 nt = vmm_new_table();

            if (nt == 0)
                return NULL;

            *e = nt | PTE_P | PTE_W;

        } else if (*e & PTE_PS) {

            /* большая страница на пути: 2 МиБ умеем разбивать,
               1 ГиБ мы сами не создаём */
            if (!create || lvl != 2 || !vmm_split_2m(e))
                return NULL;
        }

        t = vmm_table(*e);
    }

    return &t[(virt >> (12u + 9u * (level - 1u))) & 511u];
}

/* Отобразить одну 4-КиБ страницу */
BOOLEAN vmm_map_page(UINT64 virt, UINT64 phys, UINT32 attr)
{
    UINT64 *e = vmm_entry(virt, 1, TRUE);

    if (e == NULL)
        return FALSE;

    BOOLEAN was = (*e & PTE_P) != 0;

    *e = (phys & PTE_ADDR) | vmm_bits(attr, FALSE);

    if (g_vmm_ready)
        vmm_invlpg(virt);

    /* страница уже была отображена (другой адрес или права) - другие
       ядра процессора могли запомнить старое: пусть сбросят TLB, когда
       возьмут большой замок (smp.c) */
    if (was)
        g_tlb_gen++;

    return TRUE;
}

/* Убрать отображение страницы (дальше обращение = Page Fault) */
void vmm_unmap_page(UINT64 virt)
{
    UINT64 *e = vmm_entry(virt, 1, FALSE);

    if (e != NULL) {
        *e = 0;
        vmm_invlpg(virt);
        g_tlb_gen++;
    }
}

/* Отобразить 2-МиБ страницу (virt и phys выровнены на 2 МиБ) */
static BOOLEAN vmm_map_2m(UINT64 virt, UINT64 phys, UINT32 attr)
{
    UINT64 *e = vmm_entry(virt, 2, TRUE);

    if (e == NULL)
        return FALSE;

    UINT64 bits = vmm_bits(attr, TRUE);

    *e = (phys & 0x000FFFFFFFE00000ull) | bits | PTE_PS;

    return TRUE;
}

/* Какой физический адрес стоит за виртуальным (0 - не отображён) */
UINT64 vmm_virt_to_phys(UINT64 virt)
{
    if (!g_vmm_ready)
        return 0;

    UINT64 *t = vmm_table(g_vmm_pml4_phys);

    for (UINTN lvl = 4; lvl >= 1; lvl--) {

        UINT64 e = t[(virt >> (12u + 9u * (lvl - 1u))) & 511u];

        if (!(e & PTE_P))
            return 0;

        if (lvl == 1 || (e & PTE_PS)) {
            UINT64 span = 1ull << (12u + 9u * (lvl - 1u));
            return (e & PTE_ADDR & ~(span - 1u)) | (virt & (span - 1u));
        }

        t = vmm_table(e);
    }

    return 0;
}

/* Флаги записи для адреса - для экрана Page Fault ("эта страница
   только для чтения" и т.п.). 0 - не отображён. */
UINT64 vmm_query(UINT64 virt)
{
    if (!g_vmm_ready)
        return 0;

    UINT64 *t = vmm_table(g_vmm_pml4_phys);

    for (UINTN lvl = 4; lvl >= 1; lvl--) {

        UINT64 e = t[(virt >> (12u + 9u * (lvl - 1u))) & 511u];

        if (!(e & PTE_P))
            return 0;

        if (lvl == 1 || (e & PTE_PS))
            return e;

        t = vmm_table(e);
    }

    return 0;
}

/*
 * Поменять тип кэширования участка прямого отображения:
 * регистры устройств - UC, видеопамять - WC. Страницы 4 КиБ
 * (большие при необходимости разбиваются).
 */
BOOLEAN vmm_map_mmio(UINT64 phys, UINT64 size, UINT32 cache)
{
    UINT64 start = phys & ~4095ull;
    UINT64 end = (phys + size + 4095u) & ~4095ull;

    for (UINT64 p = start; p < end; p += 4096u) {
        if (!vmm_map_page(MYOS_HHDM_BASE + p, p, VMM_W | cache))
            return FALSE;
    }

    return TRUE;
}

/*
 * Убедиться, что участок физической памяти виден через прямое
 * отображение; недостающие страницы отобразить с атрибутом cache
 * (0 = обычная кэшируемая, VMM_UC - устройства). Уже отображённое
 * НЕ трогаем (в отличие от vmm_map_mmio). Нужно для таблиц ACPI и
 * окна PCIe (ECAM), которые на некоторых машинах лежат выше
 * последней RAM - туда прямое отображение заранее не доходит.
 */
BOOLEAN vmm_ensure_mapped(UINT64 phys, UINT64 size, UINT32 cache)
{
    UINT64 p = phys & ~4095ull;
    UINT64 end = (phys + size + 4095u) & ~4095ull;

    while (p < end) {

        UINT64 e = vmm_query(MYOS_HHDM_BASE + p);

        if (e != 0) {
            /* большая страница уже есть - прыгаем через неё целиком */
            if (e & PTE_PS)
                p = (p & ~0x1FFFFFull) + 0x200000u;
            else
                p += 4096u;
            continue;
        }

        if (!vmm_map_page(MYOS_HHDM_BASE + p, p, cache))
            return FALSE;

        p += 4096u;
    }

    return TRUE;
}

/*
 * То же, но страницы должны быть доступны для ЗАПИСИ (uACPI пишет в
 * поля AML и в FACS). Страница, уже отображённая только для чтения
 * (так kernel/acpi.c отображает таблицы вне RAM), переотображается с
 * записью, сохраняя свой режим кэша; новая - получает cache
 * (для регистров устройств - VMM_UC).
 */
BOOLEAN vmm_ensure_writable(UINT64 phys, UINT64 size, UINT32 cache)
{
    UINT64 p = phys & ~4095ull;
    UINT64 end = (phys + size + 4095u) & ~4095ull;

    while (p < end) {

        UINT64 e = vmm_query(MYOS_HHDM_BASE + p);

        if (e == 0) {
            if (!vmm_map_page(MYOS_HHDM_BASE + p, p, VMM_W | cache))
                return FALSE;
        } else if (!(e & PTE_W)) {
            /* большие страницы - только RAM ядра, они всегда с записью */
            if (e & PTE_PS)
                return FALSE;
            if (!vmm_map_page(MYOS_HHDM_BASE + p, p,
                              VMM_W | ((e & PTE_PCD) ? VMM_UC : 0)))
                return FALSE;
        } else if (e & PTE_PS) {
            p = (p & ~0x1FFFFFull) + 0x200000u;
            continue;
        }

        p += 4096u;
    }

    return TRUE;
}

/* ---------------------------------------------------------------- */

static BOOLEAN vmm_ram_type(UINT32 t)
{
    /* всё, что физически является памятью: свободная, занятая
       прошивкой (её Runtime-часть и таблицы ACPI живут дальше),
       наша (ядро, паспорт) */
    return t == 1 || t == 2 || t == 3 || t == 4 || t == 5 || t == 6 ||
           t == 7 || t == 9 || t == 10 || t == 14 || t >= 0x80000000u;
}

/* Участки RAM из карты памяти, склеенные и отсортированные */
static UINT64 g_ram_lo[KMM_MAX_REGIONS];
static UINT64 g_ram_hi[KMM_MAX_REGIONS];
static UINTN  g_ram_n = 0;

static void vmm_collect_ram(void)
{
    g_ram_n = 0;

    for (UINTN i = 0; i < g_kmm_map_count; i++) {

        if (!vmm_ram_type(g_kmm_map[i].type))
            continue;

        UINT64 lo = g_kmm_map[i].phys;
        UINT64 hi = lo + g_kmm_map[i].pages * KMM_PAGE;

        /* вставка с сортировкой */
        UINTN k = g_ram_n;

        while (k > 0 && g_ram_lo[k - 1] > lo) {
            g_ram_lo[k] = g_ram_lo[k - 1];
            g_ram_hi[k] = g_ram_hi[k - 1];
            k--;
        }

        g_ram_lo[k] = lo;
        g_ram_hi[k] = hi;
        g_ram_n++;
    }

    /* склеить соседние/перекрывающиеся */
    UINTN w = 0;

    for (UINTN i = 0; i < g_ram_n; i++) {
        if (w > 0 && g_ram_lo[i] <= g_ram_hi[w - 1]) {
            if (g_ram_hi[i] > g_ram_hi[w - 1])
                g_ram_hi[w - 1] = g_ram_hi[i];
        } else {
            g_ram_lo[w] = g_ram_lo[i];
            g_ram_hi[w] = g_ram_hi[i];
            w++;
        }
    }

    g_ram_n = w;
}

/* 0 - в [lo,hi) нет RAM, 1 - всё RAM, 2 - вперемешку */
static UINTN vmm_ram_cover(UINT64 lo, UINT64 hi)
{
    for (UINTN i = 0; i < g_ram_n; i++) {

        if (g_ram_hi[i] <= lo || g_ram_lo[i] >= hi)
            continue;

        if (g_ram_lo[i] <= lo && g_ram_hi[i] >= hi)
            return 1;

        return 2;
    }

    return 0;
}

/* ----------------------------------------------------------------
 * Возможности процессора: NX и PAT
 * ---------------------------------------------------------------- */

static void vmm_cpuid(UINT32 leaf, UINT32 *a, UINT32 *b, UINT32 *c, UINT32 *d)
{
    __asm__ __volatile__("cpuid"
        : "=a"(*a), "=b"(*b), "=c"(*c), "=d"(*d)
        : "a"(leaf), "c"(0));
}

static void vmm_setup_cpu(void)
{
    UINT32 a, b, c, d;

    /* NX: CPUID 0x80000001, EDX бит 20; включается битом NXE
       в регистре EFER (MSR 0xC0000080, бит 11) */
    vmm_cpuid(0x80000000u, &a, &b, &c, &d);

    if (a >= 0x80000001u) {
        vmm_cpuid(0x80000001u, &a, &b, &c, &d);
        if (d & (1u << 20)) {
            kx_wrmsr(0xC0000080u, kx_rdmsr(0xC0000080u) | (1ull << 11));
            g_vmm_nx = TRUE;
        }
    }

    /* PAT: CPUID 1, EDX бит 16. Тип №1 (было WT) -> WC (0x01).
       По правилам Intel перед сменой PAT сбросить кэши. */
    vmm_cpuid(1u, &a, &b, &c, &d);

    if (d & (1u << 16)) {
        UINT64 pat = kx_rdmsr(0x277u);
        pat = (pat & ~(0xFFull << 8)) | (0x01ull << 8);
        __asm__ __volatile__("wbinvd" ::: "memory");
        kx_wrmsr(0x277u, pat);
        __asm__ __volatile__("wbinvd" ::: "memory");
        g_vmm_pat = TRUE;
    }

    /* CR0.WP (бит 16): без него ядру разрешено писать даже в
       страницы "только для чтения" - защита кода не работала бы */
    UINT64 cr0;
    __asm__ __volatile__("mov %%cr0, %0" : "=r"(cr0));
    cr0 |= (1ull << 16);
    __asm__ __volatile__("mov %0, %%cr0" : : "r"(cr0) : "memory");

    /*
     * CR4.OSXSAVE (бит 18) - выключить (этап 11). Прошивка могла его
     * включить, и тогда программы (glibc в программах Linux) видят в
     * CPUID, что можно AVX, и пользуются регистрами ymm. Но при
     * прерываниях и переключении потоков ядро сохраняет только то, что
     * сохраняет fxsave (x87 и SSE: xmm) - верхние половины ymm одного
     * потока портил бы другой. Без OSXSAVE процессор честно говорит
     * программам "AVX нет", и они берут SSE2 (его есть всегда у x86-64).
     * Остальные ядра копируют CR4 загрузочного (smp.c).
     */
    UINT64 cr4;
    __asm__ __volatile__("mov %%cr4, %0" : "=r"(cr4));
    if (cr4 & (1ull << 18))
        __asm__ __volatile__("mov %0, %%cr4" : : "r"(cr4 & ~(1ull << 18)) : "memory");
}

/* ----------------------------------------------------------------
 * Построить таблицы ядра и переключиться на них
 * ---------------------------------------------------------------- */

BOOLEAN vmm_init(void)
{
    vmm_setup_cpu();

    g_vmm_pml4_phys = vmm_new_table();

    if (g_vmm_pml4_phys == 0)
        return FALSE;

    /* --- 1. образ ядра, раздел за разделом --- */
    UINT64 kv0 = (UINT64)(UINTN)__kernel_start;
    UINT64 kv_text = (UINT64)(UINTN)__kernel_text_end;
    UINT64 kv_ro = (UINT64)(UINTN)__kernel_rodata_end;
    UINT64 kv_end = (UINT64)(UINTN)__kernel_end;

    for (UINT64 v = kv0; v < kv_end; v += 4096u) {

        UINT32 attr;

        if (v < kv_text)
            attr = VMM_X;               /* код: читать + исполнять */
        else if (v < kv_ro)
            attr = 0;                   /* константы: только читать */
        else
            attr = VMM_W;               /* данные */

        if (!vmm_map_page(v, g_boot.kernel_phys + (v - kv0), attr))
            return FALSE;
    }

    /* --- 2. прямое отображение физической памяти --- */
    vmm_collect_ram();

    UINT64 top = 4ull << 30;

    for (UINTN i = 0; i < g_ram_n; i++)
        if (g_ram_hi[i] > top)
            top = g_ram_hi[i];

    /* Выше 4 ГиБ отображаем только до конца RAM. Записи "Reserved"
       / MMIO там бывают огромными и далёкими (у QEMU - 12 ГиБ под
       самым 1 ТиБ): отображать их заранее - только тратить
       мегабайты на таблицы. Нужный участок устройства ядро
       отобразит само через vmm_map_mmio. */

    top = (top + 0x1FFFFFu) & ~0x1FFFFFull;

    if (top > (1ull << 40))             /* 1 ТиБ - с большим запасом */
        top = 1ull << 40;

    g_vmm_hhdm_top = top;

    for (UINT64 p = 0; p < top; p += 0x200000u) {

        UINTN cover = vmm_ram_cover(p, p + 0x200000u);

        if (cover == 1) {
            if (!vmm_map_2m(MYOS_HHDM_BASE + p, p, VMM_W))
                return FALSE;
            g_vmm_pages_2m++;
        } else if (cover == 0) {
            if (!vmm_map_2m(MYOS_HHDM_BASE + p, p, VMM_W | VMM_UC))
                return FALSE;
            g_vmm_pages_2m++;
        } else {
            for (UINT64 q = p; q < p + 0x200000u; q += 4096u) {
                UINT32 attr = VMM_W;
                if (vmm_ram_cover(q, q + 4096u) != 1)
                    attr |= VMM_UC;
                if (!vmm_map_page(MYOS_HHDM_BASE + q, q, attr))
                    return FALSE;
                g_vmm_pages_4k++;
            }
        }
    }

    /* --- 3. видеопамять - write-combining (может лежать и выше
       top - тогда просто отобразится отдельно) --- */
    if (g_boot.fb_phys != 0 &&
        !vmm_map_mmio(g_boot.fb_phys, g_boot.fb_size, VMM_WC))
        return FALSE;

    /* --- 4. включить! Код ядра и стек (он в прямом отображении)
       на новых таблицах лежат по тем же виртуальным адресам,
       поэтому выполнение просто продолжается. --- */
    __asm__ __volatile__("mov %0, %%cr3" : : "r"(g_vmm_pml4_phys) : "memory");

    /* Запись в CR3 сбрасывает кэш трансляций (TLB), кроме записей
       с флагом "глобальная" - такие могла оставить прошивка.
       Выключить и включить CR4.PGE - сбросить и их, чтобы старое
       отображение 1:1 гарантированно исчезло. */
    {
        UINT64 cr4;
        __asm__ __volatile__("mov %%cr4, %0" : "=r"(cr4));
        if (cr4 & (1ull << 7)) {
            __asm__ __volatile__("mov %0, %%cr4" : : "r"(cr4 & ~(1ull << 7)) : "memory");
            __asm__ __volatile__("mov %0, %%cr4" : : "r"(cr4) : "memory");
        }
    }

    g_vmm_ready = TRUE;

    return TRUE;
}

/*
 * Стек ядра с защитной страницей.
 *
 * Стеки живут в своём участке адресов (MYOS_KSTACK_BASE), друг за
 * другом, и под каждым оставлена одна НЕотображённая страница.
 * Если стек переполнится (слишком глубокая рекурсия, огромный
 * массив в локальной переменной), процессор наткнётся на эту
 * страницу - будет Page Fault, а так как положить рамку
 * исключения уже некуда - Double Fault, который обрабатывается
 * на своём отдельном стеке (IST, см. cpu.c). В итоге - экран
 * паники "stack overflow", а не молчаливая порча соседней памяти.
 *
 * Возвращает адрес ВЕРХА стека (стек растёт вниз), 0 - ошибка.
 */
UINT64 vmm_alloc_stack(UINTN pages, const char *name)
{
    if (!g_vmm_ready || g_kstack_count >= KSTACK_MAX)
        return 0;

    UINT64 guard = g_kstack_next;
    UINT64 bottom = guard + 4096u;
    UINT64 top = bottom + (UINT64)pages * 4096u;

    for (UINT64 v = bottom; v < top; v += 4096u) {

        UINT64 phys = pmm_alloc_zeroed(1, 0);

        if (phys == 0 || !vmm_map_page(v, phys, VMM_W))
            return 0;
    }

    g_kstacks[g_kstack_count].guard = guard;
    g_kstacks[g_kstack_count].bottom = bottom;
    g_kstacks[g_kstack_count].top = top;
    g_kstacks[g_kstack_count].name = name;
    g_kstack_count++;

    g_kstack_next = top;       /* следующий стек начнётся со своей
                                  защитной страницы прямо здесь */

    return top;
}

/* Чей это адрес - для экрана Page Fault */
const char *vmm_describe(UINT64 a)
{
    if (a < 4096u)
        return "NULL pointer (first page is never mapped)";

    if (a < 0x0000800000000000ull)
        return "lower half: nothing is mapped there (a physical address used without P2V?)";

    if (a < 0xFFFF800000000000ull)
        return "non-canonical address (garbage pointer)";

    for (UINTN i = 0; i < g_kstack_count; i++) {
        if (a >= g_kstacks[i].guard && a < g_kstacks[i].bottom)
            return "GUARD PAGE below a kernel stack: STACK OVERFLOW";
        if (a >= g_kstacks[i].bottom && a < g_kstacks[i].top)
            return "kernel stack";
    }

    UINT64 kv0 = (UINT64)(UINTN)__kernel_start;

    if (a >= kv0 && a < (UINT64)(UINTN)__kernel_text_end)
        return "kernel CODE (read-only + execute)";

    if (a >= kv0 && a < (UINT64)(UINTN)__kernel_rodata_end)
        return "kernel constants (read-only)";

    if (a >= kv0 && a < (UINT64)(UINTN)__kernel_end)
        return "kernel data";

    if (a >= MYOS_HHDM_BASE && a < MYOS_HHDM_BASE + g_vmm_hhdm_top)
        return "direct map of physical memory";

    if (a >= MYOS_KSTACK_BASE && a < MYOS_KERNEL_VIRT)
        return "kernel stack area (unused slot)";

    return "kernel half, not mapped";
}
