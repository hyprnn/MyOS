/*
 * kernel/smp.c - запуск остальных ядер процессора (SMP, этап 10).
 * Часть MyOS; общие объявления - в myos.h.
 *
 * После включения компьютера работает ОДНО ядро - "загрузочное"
 * (BSP). Остальные (AP) стоят и ждут: прошивка их не трогала, они в
 * том же состоянии, что при подаче питания - 16-битный "реальный
 * режим" 1978 года, без страниц, без нашего ядра. Разбудить их можно
 * только сигналами через Local APIC:
 *
 *   INIT          - "сбросься и жди";
 *   SIPI (дважды) - Startup IPI: "начни выполнять код с физического
 *                   адреса V * 4096" - и адрес этот обязан быть ниже
 *                   1 МБ (номер V - 8 бит).
 *
 * Поэтому у нас есть "трамплин": маленький кусок кода, который мы
 * копируем в свободную страницу ниже 1 МБ. Он проводит ядро по той
 * же лестнице, по которой когда-то прошла прошивка для BSP:
 *
 *   16 бит  -> загрузить свою маленькую GDT, включить защищённый режим
 *   32 бита -> включить PAE, таблицы страниц (временные, см. ниже),
 *              длинный режим (EFER.LME) и страничную адресацию
 *   64 бита -> взять стек и прыгнуть в C-функцию ap_main в ядре
 *
 * Временные таблицы страниц: в момент включения страниц процессор
 * исполняет код трамплина по НИЗКОМУ физическому адресу, а в таблицах
 * ядра нижняя половина пуста (она - для программ). Поэтому трамплину
 * даём копию таблиц ядра, в которой вдобавок первые 2 МБ отображены
 * "один в один". Попав в ap_main (верхняя половина), ядро переходит на
 * настоящие таблицы ядра.
 *
 * Проснувшись и настроив себя (GDT, TSS со своими аварийными стеками,
 * IDT, APIC, NX, PAT, syscall), ядро ждёт, пока BSP запустит всех, и
 * входит в общий планировщик (sched.c) - с "большим замком ядра",
 * который тоже здесь.
 */
#include "myos.h"

KX_CPU g_cpus[KX_MAX_CPUS];
UINT32 g_ncpus = 1;           /* сколько ядер работает (BSP + проснувшиеся) */
UINT32 g_ncpus_found = 1;     /* сколько ядер в таблице MADT */
volatile BOOLEAN g_smp_go;    /* BSP: "все запущены - в планировщик" */

/* ================================================================
 * Большой замок ядра (BKL) - см. объяснение в начале sched.c.
 *
 * Сам замок - одно слово: 0 свободен, иначе номер ядра + 1. Кто его
 * держит "логически" - счётчик bkl_depth у текущего потока. Ждём
 * замок с запрещёнными прерываниями (на своём ядре обработчик не
 * придёт за тем же замком).
 *
 * Заодно - TLB: если ядро ОС поменяло отображение страницы, которая
 * уже была отображена (vmm.c, g_tlb_gen++), остальные ядра процессора
 * могли запомнить старое. Проверяем при каждом взятии замка: код ядра
 * ОС выполняется только под ним, так что до чужих страниц ядро
 * процессора со старым TLB не доберётся.
 * ================================================================ */

/* Замок "с талончиками" (ticket lock): кто пришёл раньше - получит
   раньше. Обычный замок-флаг на нескольких ядрах бывает нечестным:
   отпустившее ядро тут же хватает его снова (у него он в кэше), а
   ждущее может не дождаться никогда. */
static volatile UINT32 g_bkl_next;       /* следующий талончик */
static volatile UINT32 g_bkl_serving;    /* чей черёд */
static volatile UINT32 g_bkl_owner;      /* номер ядра + 1 (для отладки) */
static volatile UINT64 g_bkl_since;      /* rdtsc: когда его взяли */
volatile UINT64 g_tlb_gen;
UINT64 g_bkl_spins;            /* сколько раз пришлось ждать (для cpu) */
UINT64 g_bkl_breaks;           /* сколько раз пропустили ждущих вперёд */

static void bkl_acquire(void)
{
    UINT32 my = __atomic_fetch_add(&g_bkl_next, 1u, __ATOMIC_RELAXED);

    if (__atomic_load_n(&g_bkl_serving, __ATOMIC_ACQUIRE) != my) {
        g_bkl_spins++;
        while (__atomic_load_n(&g_bkl_serving, __ATOMIC_ACQUIRE) != my) {
            /* ждём с запрещёнными прерываниями - просьбу "сбрось TLB"
               (umem.c) выполняем прямо здесь, иначе тот, кто держит
               замок и ждёт нашего ответа, ждал бы вечно */
            uvm_tlb_ipi();
            cpu_pause();
        }
    }

    g_bkl_owner = kx_cpu_index() + 1u;
    g_bkl_since = rdtsc();

    KX_CPU *c = kx_cpu();
    UINT64 gen = g_tlb_gen;

    if (c->tlb_gen != gen) {
        UINT64 cr3;
        __asm__ __volatile__("mov %%cr3, %0; mov %0, %%cr3" : "=r"(cr3) : : "memory");
        c->tlb_gen = gen;
    }
}

static void bkl_release(void)
{
    g_bkl_owner = 0;
    __atomic_store_n(&g_bkl_serving, g_bkl_serving + 1u, __ATOMIC_RELEASE);
}

/* Ждёт ли замка кто-то ещё */
static BOOLEAN bkl_contended(void)
{
    return __atomic_load_n(&g_bkl_next, __ATOMIC_RELAXED) -
           __atomic_load_n(&g_bkl_serving, __ATOMIC_RELAXED) > 1u;
}

/*
 * "Пропустить вперёд": если замка ждёт другое ядро - отдать его и
 * встать в очередь снова. Звать можно только там, где текущий поток
 * и так мог быть прерван и заменён любым другим (прерывания у
 * прерванного кода были разрешены) - тогда то, что другое ядро
 * выполнит кусок кода ядра ОС, ничем не отличается от "таймер
 * переключил на другой поток". Так ни один поток ядра, который долго
 * считает, не держит замок дольше миллисекунды (тика таймера).
 *
 * Но и не меньше миллисекунды: ядро, которое долго ждало замок,
 * накопило за это время тик таймера - он сработает сразу, как только
 * ядро получит замок и разрешит прерывания. Отдай оно замок на этом
 * тике - оно не успело бы поработать вовсе, и одни потоки получали
 * бы почти всё время, а другие - почти ничего.
 */
void kx_bkl_relax(void)
{
    if (!bkl_contended())
        return;

    if (g_tsc_hz != 0 && rdtsc() - g_bkl_since < g_tsc_hz / 1000u)
        return;

    UINT64 fl = kx_irq_save();
    UINT32 d = kx_bkl_release_all();

    if (d > 0) {
        g_bkl_breaks++;
        kx_bkl_reacquire(d);
    }

    kx_irq_restore(fl);
}

/* Вход в код ядра ОС (прерывание, системный вызов) */
void kx_bkl_enter(void)
{
    UINT64 fl = kx_irq_save();
    KTHREAD *t = kx_cur();

    if (t->bkl_depth++ == 0)
        bkl_acquire();

    kx_irq_restore(fl);
}

/* Выход из него; последний выход отпускает замок */
void kx_bkl_exit(void)
{
    UINT64 fl = kx_irq_save();
    KTHREAD *t = kx_cur();

    if (t->bkl_depth > 0 && --t->bkl_depth == 0)
        bkl_release();

    kx_irq_restore(fl);
}

/* Отпустить совсем (перед hlt) - вернуть, сколько было; прерывания
   должны быть запрещены */
UINT32 kx_bkl_release_all(void)
{
    KTHREAD *t = kx_cur();
    UINT32 d = t ? t->bkl_depth : 0;

    if (d > 0) {
        t->bkl_depth = 0;
        bkl_release();
    }

    return d;
}

void kx_bkl_reacquire(UINT32 depth)
{
    KTHREAD *t = kx_cur();

    if (depth > 0) {
        bkl_acquire();
        t->bkl_depth = depth;
    }
}

/* Загрузочное ядро - в самом начале kmain (после своей GDT):
   регистр GS указывает на g_cpus[0], поток "shell" держит замок */
void smp_early_init(void)
{
    KX_CPU *c = &g_cpus[0];

    c->self = c;
    c->index = 0;
    c->kcur = &g_kthreads[0];
    c->tss_ptr = &g_ktss;
    c->bsp = TRUE;
    c->used = TRUE;
    c->online = TRUE;

    kx_wrmsr(0xC0000101u, (UINT64)(UINTN)c);   /* GS base */
    kx_wrmsr(0xC0000102u, 0);                  /* KernelGSBase: GS программ */

    /* Системные вызовы на этом ядре - сразу, здесь. Раньше их включал
       proc_init, но он работает уже после запуска остальных ядер, когда
       поток kmain мог переехать на другое ядро: настройка доставалась
       ему, а у загрузочного ядра syscall оставался выключенным - и
       первая программа, попавшая на него, падала на sysretq (#UD). */
    kx_syscall_cpu_init();

    /* большой замок держит ядро 0 (поток "shell", bkl_depth = 1) */
    g_bkl_next = 1u;
    g_bkl_serving = 0u;
    g_bkl_owner = 1u;
}

/* Паника: остановить остальные ядра, чтобы экран паники не затёрли */
void smp_halt_others(void)
{
    if (g_ncpus < 2 || (g_lapic_base == 0 && !g_lapic_x2))
        return;

    UINT32 me = kx_cpu_index();

    for (UINT32 i = 0; i < KX_MAX_CPUS; i++)
        if (i != me && g_cpus[i].online)
            kx_lapic_send_ipi(g_cpus[i].apic_id, KX_VEC_HALT);
}

/* ================================================================
 * Трамплин. Кладётся в .rodata ядра как кусок байт, копируется в
 * страницу ниже 1 МБ; выполняется ТАМ. Все адреса внутри - смещения
 * от начала (tr_start), абсолютные значения вписывает smp_start.
 * ================================================================ */

__asm__(
    ".section .rodata\n"
    ".balign 16\n"
    ".global kx_tr_start\n"
    ".global kx_tr_end\n"
    ".global kx_tr_gdtr\n"
    ".global kx_tr_gdt\n"
    ".global kx_tr_pm32\n"
    ".global kx_tr_lm64\n"
    ".global kx_tr_pm32_ptr\n"
    ".global kx_tr_lm64_ptr\n"
    ".global kx_tr_cr3\n"
    ".global kx_tr_cr4\n"
    ".global kx_tr_cr0\n"
    ".global kx_tr_efer\n"
    ".global kx_tr_stack\n"
    ".global kx_tr_entry\n"
    ".global kx_tr_arg\n"

    ".code16\n"
    "kx_tr_start:\n"
    "  cli\n"
    "  cld\n"
    /* ds = cs (= адрес трамплина / 16): данные трамплина по смещениям */
    "  movw %cs, %ax\n"
    "  movw %ax, %ds\n"
    /* ebx = физический адрес трамплина - понадобится в 32 битах */
    "  xorl %ebx, %ebx\n"
    "  movw %ax, %bx\n"
    "  shll $4, %ebx\n"
    "  lgdtl (kx_tr_gdtr - kx_tr_start)\n"
    "  movl %cr0, %eax\n"
    "  orl $1, %eax\n"                       /* PE: защищённый режим */
    "  movl %eax, %cr0\n"
    "  ljmpl *(kx_tr_pm32_ptr - kx_tr_start)\n"

    ".code32\n"
    "kx_tr_pm32:\n"
    "  movw $0x10, %ax\n"
    "  movw %ax, %ds\n"
    "  movw %ax, %es\n"
    "  movw %ax, %ss\n"
    "  movw %ax, %fs\n"
    "  movw %ax, %gs\n"
    "  movl (kx_tr_cr4 - kx_tr_start)(%ebx), %eax\n"   /* PAE и др. */
    "  movl %eax, %cr4\n"
    "  movl (kx_tr_cr3 - kx_tr_start)(%ebx), %eax\n"   /* временные таблицы */
    "  movl %eax, %cr3\n"
    "  movl $0xC0000080, %ecx\n"                       /* EFER */
    "  rdmsr\n"
    "  orl (kx_tr_efer - kx_tr_start)(%ebx), %eax\n"   /* LME, NXE, SCE */
    "  wrmsr\n"
    "  movl (kx_tr_cr0 - kx_tr_start)(%ebx), %eax\n"   /* PG, WP... */
    "  movl %eax, %cr0\n"
    "  ljmpl *(kx_tr_lm64_ptr - kx_tr_start)(%ebx)\n"

    ".code64\n"
    "kx_tr_lm64:\n"
    "  movl %ebx, %ebx\n"                    /* верхние 32 бита - нули */
    "  movq (kx_tr_stack - kx_tr_start)(%rbx), %rsp\n"
    "  movq (kx_tr_arg - kx_tr_start)(%rbx), %rdi\n"
    "  movq (kx_tr_entry - kx_tr_start)(%rbx), %rax\n"
    "  xorl %ebp, %ebp\n"
    "  pushq $0\n"                           /* "адрес возврата" - никуда */
    "  jmpq *%rax\n"

    ".balign 8\n"
    "kx_tr_gdt:\n"
    "  .quad 0\n"
    "  .quad 0x00CF9A000000FFFF\n"           /* 0x08: код, 32 бита */
    "  .quad 0x00CF92000000FFFF\n"           /* 0x10: данные */
    "  .quad 0x00AF9A000000FFFF\n"           /* 0x18: код, 64 бита */
    "kx_tr_gdtr:\n"
    "  .word 31\n"
    "  .long 0\n"                            /* база: вписывается */
    "kx_tr_pm32_ptr:\n"
    "  .long 0\n"                            /* адрес kx_tr_pm32 */
    "  .word 0x08\n"
    "kx_tr_lm64_ptr:\n"
    "  .long 0\n"                            /* адрес kx_tr_lm64 */
    "  .word 0x18\n"
    "kx_tr_cr3:  .long 0\n"
    "kx_tr_cr4:  .long 0\n"
    "kx_tr_cr0:  .long 0\n"
    "kx_tr_efer: .long 0\n"
    ".balign 8\n"
    "kx_tr_stack: .quad 0\n"
    "kx_tr_entry: .quad 0\n"
    "kx_tr_arg:   .quad 0\n"
    "kx_tr_end:\n"
    ".text\n"
);

extern const UINT8 kx_tr_start[], kx_tr_end[], kx_tr_gdtr[], kx_tr_gdt[],
                   kx_tr_pm32[], kx_tr_lm64[], kx_tr_pm32_ptr[], kx_tr_lm64_ptr[],
                   kx_tr_cr3[], kx_tr_cr4[], kx_tr_cr0[], kx_tr_efer[],
                   kx_tr_stack[], kx_tr_entry[], kx_tr_arg[];

#define TR_OFF(sym) ((UINTN)((sym) - kx_tr_start))

static UINT64 g_tr_phys;        /* страница трамплина (ниже 1 МБ) */
static UINT64 g_tr_cr3;         /* временные таблицы страниц */

/* значения регистров BSP - такими же настраиваются AP */
static UINT64 g_bsp_cr0, g_bsp_cr4, g_bsp_pat, g_bsp_efer, g_bsp_xcr0;

/* ================================================================
 * Сигналы между ядрами (IPI) через Local APIC: регистр ICR.
 * xAPIC - две половины по MMIO (0x310 - кому, 0x300 - что);
 * x2APIC - одно 64-битное MSR 0x830.
 * ================================================================ */

void kx_lapic_send_ipi(UINT32 apic_id, UINT32 low)
{
    if (g_lapic_x2) {
        kx_wrmsr(0x830u, ((UINT64)apic_id << 32) | low);
        return;
    }

    kx_lapic_write(0x310, apic_id << 24);
    kx_lapic_write(0x300, low);

    /* бит 12 "ещё доставляется" */
    for (UINTN i = 0; i < 100000u && (kx_lapic_read(0x300) & (1u << 12)); i++)
        cpu_pause();
}

/* ================================================================
 * Код, с которым просыпается каждое AP
 * ================================================================ */

/* GDT этого ядра: общие сегменты + дескриптор СВОЕГО TSS (у каждого
   ядра свой TSS - свои аварийные стеки, свой rsp0 для программ) */
static void ap_load_gdt_tss(KX_CPU *c)
{
    for (UINTN i = 0; i < 6; i++)
        c->gdt[i] = g_kgdt[i];

    UINT8 *t = (UINT8 *)&c->tss;
    for (UINTN i = 0; i < sizeof(c->tss); i++)
        t[i] = 0;

    c->tss.ist[0] = c->ist_df;
    c->tss.ist[1] = c->ist_nmi;
    c->tss.ist[2] = c->ist_mc;
    c->tss.rsp0 = c->stack_top;
    c->tss.iomap_base = (UINT16)sizeof(c->tss);

    UINT64 base = (UINT64)(UINTN)&c->tss;
    UINT64 limit = sizeof(c->tss) - 1u;

    c->gdt[6] = (limit & 0xFFFFu) | ((base & 0xFFFFFFull) << 16) | (0x89ull << 40) |
                (((limit >> 16) & 0xFu) << 48) | (((base >> 24) & 0xFFull) << 56);
    c->gdt[7] = base >> 32;

    KX_DTR gdtr;
    gdtr.limit = (UINT16)(sizeof(c->gdt) - 1);
    gdtr.base = (UINT64)(UINTN)&c->gdt[0];

    /* как kx_load_gdt: CS - через lretq, остальные - mov */
    __asm__ __volatile__(
        "lgdt %0\n\t"
        "pushq $0x08\n\t"
        "leaq 1f(%%rip), %%rax\n\t"
        "pushq %%rax\n\t"
        "lretq\n\t"
        "1:\n\t"
        "movw $0x10, %%ax\n\t"
        "movw %%ax, %%ds\n\t"
        "movw %%ax, %%es\n\t"
        "movw %%ax, %%ss\n\t"
        "movw %%ax, %%fs\n\t"
        "movw %%ax, %%gs\n\t"
        "ltr %w1\n\t"
        :
        : "m"(gdtr), "r"(0x30)
        : "rax", "memory"
    );
}

static void ap_main(UINT64 idx) __attribute__((noreturn, used));
static void ap_main(UINT64 idx)
{
    KX_CPU *c = &g_cpus[idx];

    c->self = c;
    c->index = (UINT32)idx;
    c->tss_ptr = &c->tss;

    /* 1. настоящие таблицы страниц ядра (трамплин жил на временных) */
    __asm__ __volatile__("mov %0, %%cr3" : : "r"(g_vmm_pml4_phys) : "memory");

    /* 2. регистры управления - как у BSP (SMAP BSP уже выключил) */
    __asm__ __volatile__("mov %0, %%cr4" : : "r"(g_bsp_cr4) : "memory");
    __asm__ __volatile__("mov %0, %%cr0" : : "r"(g_bsp_cr0) : "memory");

    if (g_vmm_pat) {
        __asm__ __volatile__("wbinvd" ::: "memory");
        kx_wrmsr(0x277u, g_bsp_pat);
        __asm__ __volatile__("wbinvd" ::: "memory");
    }

    kx_wrmsr(0xC0000080u, g_bsp_efer);

    if (g_bsp_xcr0 != 0)
        __asm__ __volatile__("xsetbv" : : "a"((UINT32)g_bsp_xcr0),
                             "d"((UINT32)(g_bsp_xcr0 >> 32)), "c"(0));

    /* 3. своя GDT и TSS, общая IDT */
    ap_load_gdt_tss(c);

    KX_DTR idtr;
    idtr.limit = (UINT16)(sizeof(g_kidt) - 1);
    idtr.base = (UINT64)(UINTN)&g_kidt[0];
    __asm__ __volatile__("lidt %0" : : "m"(idtr) : "memory");

    /* регистр GS - на структуру этого ядра (после lgdt: загрузка
       сегмента обнуляет базу); "GS программ" - 0 */
    kx_wrmsr(0xC0000101u, (UINT64)(UINTN)c);
    kx_wrmsr(0xC0000102u, 0);

    /* системные вызовы (и SMAP выключен) - как у BSP (proc.c) */
    kx_syscall_cpu_init();

    /* 4. сопроцессор (SSE): чистое состояние */
    __asm__ __volatile__("fninit" ::: "memory");

    /* 5. свой Local APIC: включить (в том же режиме, что у BSP),
          принимать всё, таймер пока выключен */
    UINT64 apic = kx_rdmsr(0x1B);
    apic |= (1ull << 11);
    if (g_lapic_x2)
        apic |= (1ull << 10);
    kx_wrmsr(0x1B, apic);

    kx_lapic_write(0x0F0, 0x100u | KX_VEC_SPURIOUS);
    kx_lapic_write(0x080, 0);
    kx_lapic_write(0x320, 1u << 16);

    c->apic_id_seen = acpi_current_apic_id();
    c->started_tsc = rdtsc();

    /* 6. "я проснулось" - BSP ждёт этого флага */
    __atomic_store_n(&c->online, TRUE, __ATOMIC_RELEASE);

    /* 7. ждать, пока BSP запустит всех (и не передумает) */
    while (!__atomic_load_n(&g_smp_go, __ATOMIC_ACQUIRE))
        cpu_pause();

    /* 8. свой таймер (1000 Гц, как у BSP: частота та же - шина общая) */
    kx_lapic_write(0x3E0, 0x3);
    kx_lapic_write(0x320, (1u << 17) | KX_VEC_TIMER);
    kx_lapic_write(0x380, (UINT32)(g_lapic_hz / 1000u));

    /* 9. в общий планировщик - под большим замком */
    bkl_acquire();
    sched_ap_enter();
}

/* ================================================================
 * Подготовка (на BSP)
 * ================================================================ */

/* Временные таблицы для трамплина (ниже 4 ГБ - в 32 битах cr3 32-битный):
   верхняя половина - как у ядра, нижние 2 МБ - один в один */
static BOOLEAN smp_make_tramp_tables(void)
{
    UINT64 pml4 = pmm_alloc_zeroed(1, 0x100000000ull);
    UINT64 pdpt = pmm_alloc_zeroed(1, 0x100000000ull);
    UINT64 pd = pmm_alloc_zeroed(1, 0x100000000ull);

    if (pml4 == 0 || pdpt == 0 || pd == 0)
        return FALSE;

    UINT64 *k = (UINT64 *)P2V(g_vmm_pml4_phys);
    UINT64 *t = (UINT64 *)P2V(pml4);

    for (UINTN i = 256; i < 512; i++)
        t[i] = k[i];

    /* P | W; большая страница (PS) 2 МБ по адресу 0 */
    t[0] = pdpt | 0x3u;
    ((UINT64 *)P2V(pdpt))[0] = pd | 0x3u;
    ((UINT64 *)P2V(pd))[0] = 0x0u | 0x83u;

    g_tr_cr3 = pml4;
    return TRUE;
}

/* Разбудить одно ядро; TRUE - проснулось */
static BOOLEAN smp_boot_ap(UINT32 idx)
{
    KX_CPU *c = &g_cpus[idx];
    UINT8 *tr = (UINT8 *)P2V(g_tr_phys);

    if (!c->stack_top || !c->ist_df || !c->ist_nmi || !c->ist_mc) {
        c->why = "no memory for stacks";
        return FALSE;
    }

    *(UINT64 *)(tr + TR_OFF(kx_tr_stack)) = c->stack_top;
    *(UINT64 *)(tr + TR_OFF(kx_tr_entry)) = (UINT64)(UINTN)ap_main;
    *(UINT64 *)(tr + TR_OFF(kx_tr_arg)) = idx;
    __atomic_thread_fence(__ATOMIC_SEQ_CST);

    /* INIT, пауза 10 мс, затем SIPI дважды (так велит Intel: второй -
       на случай, если первый не дошёл) */
    kx_lapic_send_ipi(c->apic_id, 0x4500u);
    busy_wait_ms(10);

    UINT32 sipi = 0x4600u | (UINT32)(g_tr_phys >> 12);

    for (UINTN k = 0; k < 2 && !__atomic_load_n(&c->online, __ATOMIC_ACQUIRE); k++) {

        kx_lapic_send_ipi(c->apic_id, sipi);

        /* ждать до 200 мс (обычно - микросекунды) */
        UINT64 t0 = kx_uptime_us();
        while (!__atomic_load_n(&c->online, __ATOMIC_ACQUIRE) &&
               kx_uptime_us() - t0 < (k == 0 ? 1000u : 200000u))
            cpu_pause();
    }

    if (!__atomic_load_n(&c->online, __ATOMIC_ACQUIRE)) {
        c->why = "did not answer the startup signal";
        return FALSE;
    }

    return TRUE;
}

void smp_start(SIMPLE_TEXT_OUTPUT_INTERFACE *out)
{
    UINT32 me = acpi_current_apic_id();

    /* ядро 0 - BSP (мы сами) */
    g_cpus[0].used = TRUE;
    g_cpus[0].online = TRUE;
    g_cpus[0].bsp = TRUE;
    g_cpus[0].apic_id = me;
    g_cpus[0].apic_id_seen = me;
    g_ncpus = 1;
    g_ncpus_found = 1;

    /* остальные - из MADT (только включённые прошивкой) */
    UINT32 n = 1;

    for (UINTN i = 0; i < g_acpi.ncpus && n < KX_MAX_CPUS; i++) {
        ACPI_CPU *a = &g_acpi.cpus[i];
        if (!a->enabled || a->apic_id == me)
            continue;
        g_cpus[n].used = TRUE;
        g_cpus[n].apic_id = a->apic_id;
        n++;
    }

    g_ncpus_found = n;

    if (n == 1) {
        print(out, "  CPU cores: 1 (MADT lists no other cores)\n");
        return;
    }

    if (g_lapic_base == 0 && !g_lapic_x2) {
        print(out, "  CPU cores: other cores not started (no Local APIC)\n");
        return;
    }

    /* стеки: основной (16 КБ) и аварийные - Double Fault, NMI, Machine
       Check. До временных таблиц: трамплин видит отображения ядра
       такими, какими они были при их создании. */
    for (UINT32 i = 1; i < n; i++) {
        KX_CPU *c = &g_cpus[i];
        c->stack_top = vmm_alloc_stack(4, "cpu stack");
        c->ist_df = vmm_alloc_stack(2, "cpu #DF stack");
        c->ist_nmi = vmm_alloc_stack(2, "cpu NMI stack");
        c->ist_mc = vmm_alloc_stack(2, "cpu #MC stack");
    }

    /* страница трамплина ниже 1 МБ */
    g_tr_phys = pmm_alloc_low_page();

    if (g_tr_phys == 0 || !smp_make_tramp_tables()) {
        print(out, "  CPU cores: other cores not started (no low memory for the trampoline)\n");
        return;
    }

    vmm_ensure_writable(g_tr_phys, 4096u, 0);

    UINT8 *tr = (UINT8 *)P2V(g_tr_phys);
    UINTN len = (UINTN)(kx_tr_end - kx_tr_start);

    for (UINTN i = 0; i < len; i++)
        tr[i] = kx_tr_start[i];

    /* абсолютные адреса внутри трамплина */
    *(UINT32 *)(tr + TR_OFF(kx_tr_gdtr) + 2) = (UINT32)(g_tr_phys + TR_OFF(kx_tr_gdt));
    *(UINT32 *)(tr + TR_OFF(kx_tr_pm32_ptr)) = (UINT32)(g_tr_phys + TR_OFF(kx_tr_pm32));
    *(UINT32 *)(tr + TR_OFF(kx_tr_lm64_ptr)) = (UINT32)(g_tr_phys + TR_OFF(kx_tr_lm64));

    /* регистры BSP */
    __asm__ __volatile__("mov %%cr0, %0" : "=r"(g_bsp_cr0));
    __asm__ __volatile__("mov %%cr4, %0" : "=r"(g_bsp_cr4));
    g_bsp_efer = kx_rdmsr(0xC0000080u);
    g_bsp_pat = g_vmm_pat ? kx_rdmsr(0x277u) : 0;

    /* XCR0 - какие наборы регистров (x87/SSE/AVX) включены; читается
       только если прошивка включила XSAVE (CR4.OSXSAVE, бит 18) */
    if (g_bsp_cr4 & (1ull << 18)) {
        UINT32 lo, hi;
        __asm__ __volatile__("xgetbv" : "=a"(lo), "=d"(hi) : "c"(0));
        g_bsp_xcr0 = ((UINT64)hi << 32) | lo;
    }

    /* трамплину: CR4 без PCIDE (бит 17, до длинного режима нельзя) и
       без LA57 (бит 12, у нас 4 уровня таблиц), но с PAE (бит 5) */
    *(UINT32 *)(tr + TR_OFF(kx_tr_cr4)) =
        (UINT32)((g_bsp_cr4 & ~((1ull << 17) | (1ull << 12))) | (1ull << 5));
    *(UINT32 *)(tr + TR_OFF(kx_tr_cr3)) = (UINT32)g_tr_cr3;
    *(UINT32 *)(tr + TR_OFF(kx_tr_cr0)) = (UINT32)g_bsp_cr0;
    *(UINT32 *)(tr + TR_OFF(kx_tr_efer)) =
        (1u << 8) | (1u << 0) | (g_vmm_nx ? (1u << 11) : 0u);

    UINT64 t0 = kx_uptime_us();

    for (UINT32 i = 1; i < n; i++) {
        if (smp_boot_ap(i)) {
            g_ncpus++;
        } else {
            klog("smp: cpu %u (APIC id %u): %s\n", i, g_cpus[i].apic_id, g_cpus[i].why);
        }
    }

    kprintf(out, "  CPU cores: %u of %u running (%u ms to start them); programs run on all\n"
                 "  of them at once, the kernel itself - on one at a time (big kernel lock)\n",
            g_ncpus, g_ncpus_found, (UINT32)((kx_uptime_us() - t0) / 1000u));

    /* все - в планировщик */
    __atomic_store_n(&g_smp_go, TRUE, __ATOMIC_RELEASE);

    klog("smp: %u of %u cores online, trampoline at 0x%llx\n", g_ncpus, g_ncpus_found, g_tr_phys);
}

/* Для команды cpu */
void smp_describe(SIMPLE_TEXT_OUTPUT_INTERFACE *out)
{
    kprintf(out, "CPU cores: %u running of %u in the ACPI table (MADT)\n", g_ncpus, g_ncpus_found);

    for (UINT32 i = 0; i < g_ncpus_found && i < KX_MAX_CPUS; i++) {

        KX_CPU *c = &g_cpus[i];

        kprintf(out, "  cpu%u: APIC id %u, %s", i, c->apic_id,
                c->bsp ? "boot core" : c->online ? "online" : "NOT started");

        if (!c->online && c->why)
            kprintf(out, " - %s", c->why);

        if (c->in_sched && c->kcur != NULL)
            kprintf(out, ", now: %s, timer ticks %llu", c->kcur->name, c->ticks);

        print(out, "\n");
    }

    if (g_ncpus > 1)
        kprintf(out, "Big kernel lock: waited for it %llu times, let others go first %llu times\n",
                g_bkl_spins, g_bkl_breaks);

    {
        extern volatile UINT64 kx_gs_fix_kernel, kx_gs_fix_user;
        if (kx_gs_fix_kernel || kx_gs_fix_user)
            kprintf(out, "GS register was wrong on interrupt entry: kernel %llu, program %llu times\n",
                    kx_gs_fix_kernel, kx_gs_fix_user);
    }
}

/* ================================================================
 * smptest: правда ли программы идут на нескольких ядрах сразу.
 * Одна программа burn, потом столько же программ, сколько ядер, -
 * все сразу. Если ядра работают параллельно, "все сразу" заняли
 * почти столько же времени, сколько одна.
 * ================================================================ */

void kernel_cmd_smptest(SIMPLE_TEXT_OUTPUT_INTERFACE *out, const char *arg)
{
    UINT32 work = 0;

    for (; *arg >= '0' && *arg <= '9'; arg++)
        work = work * 10u + (UINT32)(*arg - '0');

    if (work == 0 || work > 1000u)
        work = 200;

    char args[16];
    ksnprintf(args, sizeof(args), "%u", work);

    UINT32 n = g_ncpus;

    if (n > 8)
        n = 8;

    INTN err;
    UINT64 t0 = kx_uptime_us();
    KPROC *one = proc_spawn("/bin/burn", args, PROC_IO_CONSOLE, &err);

    if (one == NULL) {
        kprintf(out, "Cannot start /bin/burn: %s\n", vfs_strerror(err));
        return;
    }

    proc_wait(one);

    UINT64 t_one = (kx_uptime_us() - t0) / 1000u;

    kprintf(out, "1 program:  %llu ms\n", t_one);

    KPROC *ps[8];
    UINT32 started = 0;

    t0 = kx_uptime_us();

    for (UINT32 i = 0; i < n; i++) {
        ps[i] = proc_spawn("/bin/burn", args, PROC_IO_CONSOLE, &err);
        if (ps[i] != NULL)
            started++;
    }

    for (UINT32 i = 0; i < n; i++)
        if (ps[i] != NULL)
            proc_wait(ps[i]);

    UINT64 t_all = (kx_uptime_us() - t0) / 1000u;

    if (t_all == 0)
        t_all = 1;

    /* во сколько раз быстрее, чем если бы шли по очереди (x100) */
    UINT64 speed = (t_one * started * 100u) / t_all;

    kprintf(out, "%u programs at once: %llu ms -> %llu.%02llu times faster than one after\n"
                 "another (%u cores; the ideal is %u.00)\n",
            started, t_all, speed / 100u, speed % 100u, g_ncpus, started);
}
