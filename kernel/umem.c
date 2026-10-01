/*
 * kernel/umem.c - память программ "по требованию" (этап 11, путь к
 * программам Linux). Часть MyOS; общие объявления - в myos.h.
 *
 * ЗАЧЕМ
 * -----
 * Программы MyOS получали память сразу и целиком: при запуске ядро
 * отображало все страницы кода, данных и стека (proc.c). Программы
 * Linux устроены иначе:
 *   * они просят у ядра области памяти вызовом mmap - часто огромные
 *     (стек потока 8 МБ, "запас" аллокатора в 64 МБ, Firefox -
 *     гигабайты), а трогают малую часть;
 *   * меняют права областям (mprotect: например, JavaScript-движок
 *     пишет код и делает его исполняемым);
 *   * fork копирует ВСЮ память процесса - копировать честно было бы
 *     и долго, и дорого (обычно потомок тут же делает execve).
 *
 * КАК
 * ---
 * У процесса - список областей (UVMA): "с адреса A по адрес B можно
 * читать/писать/исполнять". Сами страницы появляются при первом
 * касании: процессор не находит страницу -> исключение Page Fault ->
 * uvm_fault смотрит, есть ли область с нужными правами, - есть: даёт
 * обнулённую страницу, и программа продолжает, ничего не заметив.
 *
 * fork (uvm_fork): потомок получает те же физические страницы, у
 * обоих - только для чтения с пометкой UPTE_COW ("копировать при
 * записи"). Кто первым пишет - получает Page Fault, и только тогда
 * страница копируется. Кто последний остался хозяином - пишет в неё
 * без копии (счётчик хозяев - pmm.c).
 *
 * Права страницы (биты PTE) выводятся из прав области:
 *   нет прав (PROT_NONE) -> страница есть, но без бита U: программе
 *                           нельзя, ядру (если вдруг) - можно;
 *   нет записи или COW    -> без бита W;
 *   нет исполнения        -> бит NX.
 *
 * TLB. Процессор помнит переводы адресов (TLB). Если у страницы стало
 * меньше прав или она вовсе пропала, а у процесса есть потоки на
 * ДРУГИХ ядрах процессора (этап 11, потоки), их TLB надо сбросить -
 * иначе поток там ещё какое-то время писал бы в уже чужую страницу.
 * uvm_tlb_shootdown просит такие ядра межпроцессорным прерыванием
 * KX_VEC_TLB и ждёт, пока все ответят (cpu.c, smp.c).
 *
 * Программы MyOS (без областей, p->vmas == NULL) работают как раньше:
 * для них uvm_fault ничего не делает.
 */
#include "myos.h"

/* Права PTE для страницы области с правами prot (cow - страница общая
   после fork, писать в неё пока нельзя) */
static UINT64 uvm_bits(UINT32 prot, BOOLEAN cow)
{
    UINT64 b = UPTE_P;

    if (prot & (UVM_R | UVM_W | UVM_X))
        b |= UPTE_U;

    if ((prot & UVM_W) && !cow)
        b |= UPTE_W;

    if (cow)
        b |= UPTE_COW;

    if (!(prot & UVM_X) && g_vmm_nx)
        b |= UPTE_NX;

    return b;
}

static void uvm_invlpg(UINT64 va)
{
    __asm__ __volatile__("invlpg (%0)" : : "r"(va) : "memory");
}


/* ================================================================
 * Сброс TLB на других ядрах
 * ================================================================ */

/*
 * Ядро процессора получило KX_VEC_TLB (cpu.c, без большого замка) или
 * увидело просьбу, пока ждёт замок (smp.c): перезагрузить CR3 - это
 * сбрасывает все переводы адресов программ (у страниц программ нет
 * бита G) - и ответить "сделано".
 */
void uvm_tlb_ipi(void)
{
    KX_CPU *c = kx_cpu();

    if (c->tlb_req) {
        UINT64 cr3;
        __asm__ __volatile__("mov %%cr3, %0; mov %0, %%cr3" : "=r"(cr3) : : "memory");
        __atomic_store_n(&c->tlb_req, 0u, __ATOMIC_RELEASE);
    }
}

/*
 * Память процесса p изменилась (права убавились, страница сменилась
 * или пропала): у себя - сбросить TLB сразу, у ядер, где сейчас идёт
 * поток этого процесса, - попросить и дождаться. Зовётся под большим
 * замком: никто не переключит потоки, пока мы ждём; а те ядра, что
 * ждут замок с запрещёнными прерываниями, отвечают из цикла ожидания.
 */
void uvm_tlb_shootdown(KPROC *p)
{
    UINT64 fl = kx_irq_save();
    KX_CPU *me = kx_cpu();
    UINT64 cr3;

    __asm__ __volatile__("mov %%cr3, %0" : "=r"(cr3));

    if ((cr3 & UPTE_ADDR) == p->pml4)
        __asm__ __volatile__("mov %0, %%cr3" : : "r"(cr3) : "memory");

    UINT32 asked = 0;

    for (UINT32 i = 0; i < KX_MAX_CPUS; i++) {

        KX_CPU *o = &g_cpus[i];

        if (o == me || !o->in_sched || o->kcur == NULL || o->kcur->proc != p)
            continue;

        __atomic_store_n(&o->tlb_req, 1u, __ATOMIC_RELEASE);
        kx_lapic_send_ipi(o->apic_id, KX_VEC_TLB);
        asked++;
    }

    for (UINT32 i = 0; asked > 0 && i < KX_MAX_CPUS; i++) {

        KX_CPU *o = &g_cpus[i];

        while (__atomic_load_n(&o->tlb_req, __ATOMIC_ACQUIRE) != 0)
            cpu_pause();
    }

    kx_irq_restore(fl);
}


/* ================================================================
 * Список областей
 * ================================================================ */

UVMA *uvm_find(KPROC *p, UINT64 va)
{
    for (UVMA *v = p->vmas; v != NULL; v = v->next) {

        if (va < v->start)
            return NULL;            /* список упорядочен - дальше не будет */

        if (va < v->end)
            return v;
    }

    return NULL;
}

/* Вставить область [start, end) - место должно быть свободно */
BOOLEAN uvm_add(KPROC *p, UINT64 start, UINT64 end, UINT32 prot, UINT32 flags)
{
    if (start >= end || (start & 0xFFFu) || (end & 0xFFFu))
        return FALSE;

    UVMA *n = (UVMA *)kzalloc(sizeof(UVMA));

    if (n == NULL)
        return FALSE;

    n->start = start;
    n->end = end;
    n->prot = prot;
    n->flags = flags;

    UVMA **pp = &p->vmas;

    while (*pp != NULL && (*pp)->start < start)
        pp = &(*pp)->next;

    n->next = *pp;
    *pp = n;

    return TRUE;
}

/* Разрезать область v на две по адресу at (start < at < end) */
static BOOLEAN uvm_split(UVMA *v, UINT64 at)
{
    UVMA *n = (UVMA *)kzalloc(sizeof(UVMA));

    if (n == NULL)
        return FALSE;

    n->start = at;
    n->end = v->end;
    n->prot = v->prot;
    n->flags = v->flags;
    n->next = v->next;

    v->end = at;
    v->next = n;

    return TRUE;
}

/* Разрезать области так, чтобы start и end были их границами */
static BOOLEAN uvm_cut(KPROC *p, UINT64 start, UINT64 end)
{
    for (UVMA *v = p->vmas; v != NULL; v = v->next) {

        if (v->start < start && start < v->end && !uvm_split(v, start))
            return FALSE;

        if (v->start < end && end < v->end && !uvm_split(v, end))
            return FALSE;
    }

    return TRUE;
}

/*
 * Убрать страницы [start, end) из таблиц процесса (без изменения
 * списка областей): каждая страница отдаёт свою долю (pmm_page_unref).
 * Таблицы страниц не освобождаем - их уберёт uvm_free в конце.
 */
static BOOLEAN uvm_drop_pages(KPROC *p, UINT64 start, UINT64 end)
{
    BOOLEAN any = FALSE;

    for (UINT64 va = start; va < end; va += 4096u) {

        /* целая пустая таблица верхнего уровня - перескочить */
        UINT64 *e = uvm_pte(p->pml4, va, FALSE);

        if (e == NULL) {
            UINT64 next = (va + 0x200000ull) & ~0x1FFFFFull;
            if (next > va)
                va = next - 4096u;
            continue;
        }

        if (!(*e & UPTE_P))
            continue;

        if (!(*e & UPTE_SHARED))
            pmm_page_unref(*e & UPTE_ADDR);

        *e = 0;
        any = TRUE;

        if (p->pages > 0)
            p->pages--;
    }

    return any;
}

/* munmap: области и их страницы в [start, end) - убрать */
void uvm_unmap(KPROC *p, UINT64 start, UINT64 end)
{
    if (start >= end || !uvm_cut(p, start, end))
        return;

    UVMA **pp = &p->vmas;

    while (*pp != NULL) {

        UVMA *v = *pp;

        if (v->start >= start && v->end <= end) {
            *pp = v->next;
            kfree(v);
            continue;
        }

        pp = &v->next;
    }

    if (uvm_drop_pages(p, start, end))
        uvm_tlb_shootdown(p);
}

/* mprotect: новые права областям [start, end) (вся полоса должна быть
   покрыта областями) */
INTN uvm_protect(KPROC *p, UINT64 start, UINT64 end, UINT32 prot)
{
    /* сначала проверить, что дыр нет */
    UINT64 at = start;

    for (UVMA *v = p->vmas; v != NULL && at < end; v = v->next) {
        if (v->end <= at)
            continue;
        if (v->start > at)
            return MYOS_ENOMEM;
        at = v->end;
    }

    if (at < end)
        return MYOS_ENOMEM;

    if (!uvm_cut(p, start, end))
        return MYOS_ENOMEM;

    for (UVMA *v = p->vmas; v != NULL; v = v->next)
        if (v->start >= start && v->end <= end)
            v->prot = prot;

    /* уже существующие страницы - новые права */
    for (UINT64 va = start; va < end; va += 4096u) {

        UINT64 *e = uvm_pte(p->pml4, va, FALSE);

        if (e == NULL) {
            UINT64 next = (va + 0x200000ull) & ~0x1FFFFFull;
            if (next > va)
                va = next - 4096u;
            continue;
        }

        if (!(*e & UPTE_P) || (*e & UPTE_SHARED))
            continue;

        *e = (*e & UPTE_ADDR) | uvm_bits(prot, (*e & UPTE_COW) != 0);
    }

    uvm_tlb_shootdown(p);
    return 0;
}

/*
 * Свободное место длиной len для mmap: ищем сверху вниз под
 * p->mmap_top (как Linux - библиотеки и куски памяти идут вниз от
 * стека). hint - "хотелось бы здесь" (если свободно - берём).
 */
static BOOLEAN uvm_range_free(KPROC *p, UINT64 start, UINT64 end)
{
    for (UVMA *v = p->vmas; v != NULL; v = v->next)
        if (v->start < end && start < v->end)
            return FALSE;

    return TRUE;
}

UINT64 uvm_find_free(KPROC *p, UINT64 len, UINT64 hint)
{
    if (len == 0 || len > MYOS_USER_LIMIT)
        return 0;

    if (hint != 0 && (hint & 0xFFFu) == 0 && hint >= 0x10000u &&
        hint + len > hint && hint + len <= MYOS_USER_LIMIT && uvm_range_free(p, hint, hint + len))
        return hint;

    UINT64 top = p->mmap_top;

    /* идём вниз: под каждой занятой областью, лежащей ниже top */
    for (;;) {

        if (top < len + 0x10000u)
            return 0;

        UINT64 start = top - len;
        UVMA *clash = NULL;

        for (UVMA *v = p->vmas; v != NULL; v = v->next)
            if (v->start < top && start < v->end)
                clash = v;          /* самая верхняя из мешающих - последняя */

        if (clash == NULL)
            return start;

        top = clash->start;
    }
}

void uvm_free_vmas(KPROC *p)
{
    while (p->vmas != NULL) {
        UVMA *v = p->vmas;
        p->vmas = v->next;
        kfree(v);
    }
}


/* ================================================================
 * Page Fault: дать страницу
 * ================================================================ */

/*
 * Программа (или ядро от её имени) тронула адрес va, а страницы нет
 * или в неё нельзя писать. TRUE - исправили, можно повторить; FALSE -
 * это настоящая ошибка программы (сигнал SIGSEGV / завершение).
 */
BOOLEAN uvm_fault(KPROC *p, UINT64 va, BOOLEAN write)
{
    if (p == NULL || p->vmas == NULL || va >= MYOS_USER_STACK_TOP + 0x10000u)
        return FALSE;

    UVMA *v = uvm_find(p, va);

    if (v == NULL || !(v->prot & (UVM_R | UVM_W | UVM_X)))
        return FALSE;

    if (write && !(v->prot & UVM_W))
        return FALSE;

    UINT64 page = va & ~0xFFFull;
    UINT64 *e = uvm_pte(p->pml4, page, TRUE);

    if (e == NULL)
        return FALSE;               /* нет памяти даже на таблицу */

    if (!(*e & UPTE_P)) {

        /* первое касание: новая обнулённая страница */
        UINT64 phys = pmm_alloc_zeroed(1, 0);

        if (phys == 0) {
            klog("umem: pid %u out of memory at 0x%llx\n", p->pid, va);
            return FALSE;
        }

        *e = phys | uvm_bits(v->prot, FALSE);
        p->pages++;
        return TRUE;
    }

    if (write && (*e & UPTE_COW)) {

        UINT64 old = *e & UPTE_ADDR;

        if (pmm_page_refs(old) <= 1u) {
            /* остались одни - пишем в неё же */
            *e = old | uvm_bits(v->prot, FALSE);
            uvm_invlpg(page);
            return TRUE;
        }

        UINT64 phys = pmm_alloc_pages(1, 0);

        if (phys == 0) {
            klog("umem: pid %u out of memory (copy on write) at 0x%llx\n", p->pid, va);
            return FALSE;
        }

        memcpy(P2V(phys), P2V(old), 4096u);

        *e = phys | uvm_bits(v->prot, FALSE);
        pmm_page_unref(old);

        /* другие потоки этого процесса могли помнить старую страницу */
        uvm_tlb_shootdown(p);
        return TRUE;
    }

    /* страница есть и права позволяют - TLB помнил старое (другой поток
       только что дал страницу); просто повторить */
    if (*e & UPTE_U) {
        if (!write || (*e & UPTE_W)) {
            uvm_invlpg(page);
            return TRUE;
        }
    }

    return FALSE;
}

/*
 * Page Fault с кодом ошибки процессора err: бит 1 - запись, бит 3 -
 * испорченная запись таблицы (это не "нет страницы" - не лечим), бит 4
 * - выборка команды (исполнять можно только из области с UVM_X).
 */
BOOLEAN uvm_fault_err(KPROC *p, UINT64 va, UINT64 err)
{
    if (err & 0x8u)
        return FALSE;

    if (err & 0x10u) {
        UVMA *v = (p != NULL) ? uvm_find(p, va) : NULL;
        if (v == NULL || !(v->prot & UVM_X))
            return FALSE;
    }

    return uvm_fault(p, va, (err & 0x2u) != 0);
}

/*
 * Ядро собирается читать/писать память программы [addr, addr+len)
 * (буфер read/write и т.п.): все страницы должны быть на месте и (для
 * записи) не общими после fork. Так ядро никогда не ловит Page Fault на
 * памяти программы. FALSE - адрес плохой (EFAULT).
 */
BOOLEAN uvm_prefault(KPROC *p, UINT64 addr, UINT64 len, BOOLEAN write)
{
    if (len == 0)
        return TRUE;

    if (addr + len < addr || addr + len > 0x0000800000000000ull)
        return FALSE;

    for (UINT64 va = addr & ~0xFFFull; va < addr + len; va += 4096u) {

        UINT64 *e = uvm_pte(p->pml4, va, FALSE);
        BOOLEAN ok = (e != NULL && (*e & UPTE_P) && (*e & UPTE_U) &&
                      (!write || (*e & UPTE_W)));

        if (ok)
            continue;

        /* страница окна (общая с ядром) - не трогаем, права у неё есть */
        if (e != NULL && (*e & UPTE_P) && (*e & UPTE_SHARED) && (*e & UPTE_U))
            continue;

        if (!uvm_fault(p, va, write))
            return FALSE;
    }

    return TRUE;
}

/*
 * Записать данные ядра в память программы - при загрузке (ELF, стек
 * с аргументами), в том числе в области "только чтение" (код): ядро
 * пишет по физическому адресу, права страницы ему не мешают. Страницы
 * даются по требованию; общую после fork страницу так не пишем.
 */
BOOLEAN uvm_copy_out(KPROC *p, UINT64 va, const void *src, UINT64 n)
{
    const UINT8 *s = (const UINT8 *)src;

    while (n > 0) {

        UINT64 page = va & ~0xFFFull;
        UINT64 *e = uvm_pte(p->pml4, page, FALSE);

        if (e == NULL || !(*e & UPTE_P)) {
            if (!uvm_fault(p, page, FALSE))
                return FALSE;
            e = uvm_pte(p->pml4, page, FALSE);
        }

        if (e == NULL || !(*e & UPTE_P) ||
            ((*e & UPTE_COW) && pmm_page_refs(*e & UPTE_ADDR) > 1u))
            return FALSE;

        UINT64 room = 4096u - (va & 0xFFFu);
        UINT64 k = (n < room) ? n : room;

        memcpy((UINT8 *)P2V(*e & UPTE_ADDR) + (va & 0xFFFu), s, (UINTN)k);

        va += k;
        s += k;
        n -= k;
    }

    return TRUE;
}


/* ================================================================
 * fork: общие страницы с копированием при записи
 * ================================================================ */

/*
 * Потомок получает копию списка областей и ТЕ ЖЕ страницы: частные
 * (не MAP_SHARED) у обоих становятся "только чтение + COW", общие
 * остаются общими. Таблицы страниц - свои у каждого (копия).
 */
BOOLEAN uvm_fork(KPROC *parent, KPROC *child)
{
    /* области */
    UVMA **tail = &child->vmas;

    for (UVMA *v = parent->vmas; v != NULL; v = v->next) {

        UVMA *n = (UVMA *)kzalloc(sizeof(UVMA));

        if (n == NULL)
            return FALSE;

        *n = *v;
        n->next = NULL;
        *tail = n;
        tail = &n->next;
    }

    child->mmap_top = parent->mmap_top;
    child->brk_base = parent->brk_base;
    child->brk = parent->brk;

    /* страницы: обход четырёх уровней нижней половины */
    UINT64 *l4 = (UINT64 *)P2V(parent->pml4);

    for (UINTN i = 0; i < 256; i++) {

        if (!(l4[i] & UPTE_P))
            continue;

        UINT64 *l3 = (UINT64 *)P2V(l4[i] & UPTE_ADDR);

        for (UINTN j = 0; j < 512; j++) {

            if (!(l3[j] & UPTE_P))
                continue;

            UINT64 *l2 = (UINT64 *)P2V(l3[j] & UPTE_ADDR);

            for (UINTN k = 0; k < 512; k++) {

                if (!(l2[k] & UPTE_P))
                    continue;

                UINT64 *l1 = (UINT64 *)P2V(l2[k] & UPTE_ADDR);

                for (UINTN m = 0; m < 512; m++) {

                    UINT64 e = l1[m];

                    /* окна рабочего стола потомку не достаются */
                    if (!(e & UPTE_P) || (e & UPTE_SHARED))
                        continue;

                    UINT64 va = ((UINT64)i << 39) | ((UINT64)j << 30) |
                                ((UINT64)k << 21) | ((UINT64)m << 12);
                    UVMA *v = uvm_find(parent, va);

                    if (v == NULL)
                        continue;   /* страница без области - не наша */

                    UINT64 *ce = uvm_pte(child->pml4, va, TRUE);

                    if (ce == NULL)
                        return FALSE;

                    if (!(v->flags & UVM_SHARED)) {
                        e = (e & UPTE_ADDR) | uvm_bits(v->prot, TRUE);
                        l1[m] = e;
                    }

                    *ce = e;
                    pmm_page_ref(e & UPTE_ADDR);
                    child->pages++;
                }
            }
        }
    }

    /* у родителя страницы стали "только чтение" */
    uvm_tlb_shootdown(parent);
    return TRUE;
}

/* Сколько страниц реально занято (для ps и /proc) */
UINT64 uvm_pages_in(KPROC *p)
{
    return p->pages;
}
