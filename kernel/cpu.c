/*
 * kernel/cpu.c - GDT, IDT, обработчики прерываний, экран паники, PIC, I/O APIC.
 * Часть MyOS; общие объявления - в myos.h.
 */
#include "myos.h"


/*
 * GDT. После ExitBootServices мы всё ещё сидим на GDT, которую
 * когда-то загрузила прошивка - и она лежит в памяти прошивки
 * (EfiBootServicesData), которая по правилам UEFI теперь
 * принадлежит ОС и может быть переиспользована. Своя GDT в
 * нашем собственном образе - первое, что делает любая ОС.
 *
 * Сегментация в long mode почти не работает (базы и лимиты
 * игнорируются), но дескрипторы нужны - в них записан УРОВЕНЬ
 * ПРИВИЛЕГИЙ (DPL): 0 - ядро, 3 - программы (этап 6).
 *   0x08 - код ядра:     L=1 (64-битный), DPL=0
 *   0x10 - данные ядра:  DPL=0
 *   0x18 - код программ, 32-битный - не используется, но нужен
 *          ради порядка, который требует инструкция SYSRET: она
 *          берёт селекторы программы как база+8 (данные) и
 *          база+16 (64-битный код), база = 0x18 (регистр STAR)
 *   0x20 - данные программ: DPL=3  (селектор с RPL: 0x23)
 *   0x28 - код программ:    L=1, DPL=3 (селектор 0x2B)
 *   0x30 - TSS (две записи по 8 байт), заполняется в kx_load_tss
 */
UINT64 g_kgdt[8] __attribute__((aligned(16))) = {
    0x0000000000000000ull,
    0x00AF9A000000FFFFull,
    0x00CF92000000FFFFull,
    0x00CFFA000000FFFFull,
    0x00CFF2000000FFFFull,
    0x00AFFA000000FFFFull,
    0, 0
};

/*
 * TSS (Task State Segment). В 64-битном режиме от "задач" в нём
 * осталось одно полезное: таблица запасных стеков.
 *   rsp0    - стек, на который процессор переключится при
 *             прерывании из программы (ring 3) - понадобится на
 *             этапе 6;
 *   ist[0..6] - Interrupt Stack Table: если в записи IDT указан
 *             номер IST, процессор при этом исключении ВСЕГДА
 *             переходит на этот стек, даже если текущий стек
 *             сломан или кончился.
 * Мы даём свой стек Double Fault (IST1), NMI (IST2) и Machine
 * Check (IST3): Double Fault - это как раз то, что происходит при
 * переполнении стека ядра (Page Fault на защитной странице, а
 * положить рамку исключения некуда). Без IST процессор попытался
 * бы положить рамку на тот же мёртвый стек -> Triple Fault ->
 * мгновенная перезагрузка без единого слова. С IST - экран
 * паники "stack overflow".
 */
KX_TSS g_ktss __attribute__((aligned(16)));

KX_IDT_ENTRY g_kidt[256] __attribute__((aligned(16)));

void kx_load_gdt(void)
{
    KX_DTR gdtr;

    gdtr.limit = (UINT16)(sizeof(g_kgdt) - 1);
    gdtr.base = (UINT64)(UINTN)&g_kgdt[0];

    /*
     * lgdt сам по себе не меняет уже загруженные сегментные
     * регистры - их надо перезагрузить. CS нельзя загрузить
     * обычным mov, только дальним переходом/возвратом:
     * кладём в стек новый селектор кода и адрес метки "1:" и
     * делаем lretq - процессор "возвращается" на следующую же
     * инструкцию, но уже с CS=0x08 из нашей GDT.
     */
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
        :
        : "m"(gdtr)
        : "rax", "memory"
    );
}


/*
 * Записать TSS в GDT (селектор 0x30) и загрузить его (ltr).
 * ist1..ist3 - вершины стеков для #DF, NMI, #MC (0 - не ставить).
 */
void kx_load_tss(UINT64 ist_df, UINT64 ist_nmi, UINT64 ist_mc, UINT64 rsp0)
{
    UINT8 *t = (UINT8 *)&g_ktss;

    for (UINTN i = 0; i < sizeof(g_ktss); i++)
        t[i] = 0;

    g_ktss.rsp0 = rsp0;
    g_ktss.ist[0] = ist_df;
    g_ktss.ist[1] = ist_nmi;
    g_ktss.ist[2] = ist_mc;
    g_ktss.iomap_base = (UINT16)sizeof(g_ktss);  /* карты портов нет */

    UINT64 base = (UINT64)(UINTN)&g_ktss;
    UINT64 limit = sizeof(g_ktss) - 1u;

    /* 16-байтный системный дескриптор: тип 0x9 (доступный 64-битный
       TSS), P=1; база разбросана по кусочкам, как в 1985 году */
    g_kgdt[6] = (limit & 0xFFFFu) |
                ((base & 0xFFFFFFull) << 16) |
                (0x89ull << 40) |
                (((limit >> 16) & 0xFu) << 48) |
                (((base >> 24) & 0xFFull) << 56);
    g_kgdt[7] = base >> 32;

    __asm__ __volatile__("ltr %w0" : : "r"(0x30) : "memory");

    /* номера IST в записях IDT (1..3 = ist[0..2]) */
    if (ist_df)  g_kidt[8].ist = 1;
    if (ist_nmi) g_kidt[2].ist = 2;
    if (ist_mc)  g_kidt[18].ist = 3;
}


/*
 * Точки входа прерываний (ISR stubs) - на ассемблере, потому
 * что процессор при прерывании кладёт в стек свою рамку и
 * прыгает по адресу из IDT, а обычная C-функция так вызываться
 * не умеет (не знает, что нужно сохранить ВСЕ регистры и
 * вернуться через iretq).
 *
 * 256 одинаковых кусочков, каждый ровно по 16 байт (.balign 16)
 * - поэтому адрес заглушки вектора N = начало + 16*N, и таблицу
 * адресов не нужно хранить отдельно. Каждый кладёт в стек код
 * ошибки (для векторов, где процессор его НЕ кладёт сам, -
 * фиктивный 0, чтобы рамка всегда была одинаковой) и номер
 * вектора, и прыгает в общий kx_isr_common.
 *
 * kx_isr_common сохраняет все регистры общего назначения и
 * состояние SSE (fxsave, 512 байт) - C-код, который мы
 * вызываем, компилятор волен собирать с SSE-инструкциями, и без
 * этого прерывание таймера посреди, например, копирования
 * памяти в шелле тихо портило бы xmm-регистры прерванного кода.
 */
__asm__(
    ".text\n"
    ".macro KX_ISR_NOERR v\n"
    "  .balign 16\n"
    "  pushq $0\n"
    "  pushq $\\v\n"
    "  jmp kx_isr_common\n"
    ".endm\n"
    ".macro KX_ISR_ERR v\n"
    "  .balign 16\n"
    "  pushq $\\v\n"
    "  jmp kx_isr_common\n"
    ".endm\n"
    ".balign 16\n"
    ".globl kx_isr_stubs\n"
    ".hidden kx_isr_stubs\n"
    "kx_isr_stubs:\n"
    "  KX_ISR_NOERR 0\n"
    "  KX_ISR_NOERR 1\n"
    "  KX_ISR_NOERR 2\n"
    "  KX_ISR_NOERR 3\n"
    "  KX_ISR_NOERR 4\n"
    "  KX_ISR_NOERR 5\n"
    "  KX_ISR_NOERR 6\n"
    "  KX_ISR_NOERR 7\n"
    "  KX_ISR_ERR 8\n"
    "  KX_ISR_NOERR 9\n"
    "  KX_ISR_ERR 10\n"
    "  KX_ISR_ERR 11\n"
    "  KX_ISR_ERR 12\n"
    "  KX_ISR_ERR 13\n"
    "  KX_ISR_ERR 14\n"
    "  KX_ISR_NOERR 15\n"
    "  KX_ISR_NOERR 16\n"
    "  KX_ISR_ERR 17\n"
    "  KX_ISR_NOERR 18\n"
    "  KX_ISR_NOERR 19\n"
    "  KX_ISR_NOERR 20\n"
    "  KX_ISR_ERR 21\n"
    "  KX_ISR_NOERR 22\n"
    "  KX_ISR_NOERR 23\n"
    "  KX_ISR_NOERR 24\n"
    "  KX_ISR_NOERR 25\n"
    "  KX_ISR_NOERR 26\n"
    "  KX_ISR_NOERR 27\n"
    "  KX_ISR_NOERR 28\n"
    "  KX_ISR_ERR 29\n"
    "  KX_ISR_ERR 30\n"
    "  KX_ISR_NOERR 31\n"
    "  KX_ISR_NOERR 32\n"
    "  KX_ISR_NOERR 33\n"
    "  KX_ISR_NOERR 34\n"
    "  KX_ISR_NOERR 35\n"
    "  KX_ISR_NOERR 36\n"
    "  KX_ISR_NOERR 37\n"
    "  KX_ISR_NOERR 38\n"
    "  KX_ISR_NOERR 39\n"
    "  KX_ISR_NOERR 40\n"
    "  KX_ISR_NOERR 41\n"
    "  KX_ISR_NOERR 42\n"
    "  KX_ISR_NOERR 43\n"
    "  KX_ISR_NOERR 44\n"
    "  KX_ISR_NOERR 45\n"
    "  KX_ISR_NOERR 46\n"
    "  KX_ISR_NOERR 47\n"
    "  KX_ISR_NOERR 48\n"
    "  KX_ISR_NOERR 49\n"
    "  KX_ISR_NOERR 50\n"
    "  KX_ISR_NOERR 51\n"
    "  KX_ISR_NOERR 52\n"
    "  KX_ISR_NOERR 53\n"
    "  KX_ISR_NOERR 54\n"
    "  KX_ISR_NOERR 55\n"
    "  KX_ISR_NOERR 56\n"
    "  KX_ISR_NOERR 57\n"
    "  KX_ISR_NOERR 58\n"
    "  KX_ISR_NOERR 59\n"
    "  KX_ISR_NOERR 60\n"
    "  KX_ISR_NOERR 61\n"
    "  KX_ISR_NOERR 62\n"
    "  KX_ISR_NOERR 63\n"
    "  KX_ISR_NOERR 64\n"
    "  KX_ISR_NOERR 65\n"
    "  KX_ISR_NOERR 66\n"
    "  KX_ISR_NOERR 67\n"
    "  KX_ISR_NOERR 68\n"
    "  KX_ISR_NOERR 69\n"
    "  KX_ISR_NOERR 70\n"
    "  KX_ISR_NOERR 71\n"
    "  KX_ISR_NOERR 72\n"
    "  KX_ISR_NOERR 73\n"
    "  KX_ISR_NOERR 74\n"
    "  KX_ISR_NOERR 75\n"
    "  KX_ISR_NOERR 76\n"
    "  KX_ISR_NOERR 77\n"
    "  KX_ISR_NOERR 78\n"
    "  KX_ISR_NOERR 79\n"
    "  KX_ISR_NOERR 80\n"
    "  KX_ISR_NOERR 81\n"
    "  KX_ISR_NOERR 82\n"
    "  KX_ISR_NOERR 83\n"
    "  KX_ISR_NOERR 84\n"
    "  KX_ISR_NOERR 85\n"
    "  KX_ISR_NOERR 86\n"
    "  KX_ISR_NOERR 87\n"
    "  KX_ISR_NOERR 88\n"
    "  KX_ISR_NOERR 89\n"
    "  KX_ISR_NOERR 90\n"
    "  KX_ISR_NOERR 91\n"
    "  KX_ISR_NOERR 92\n"
    "  KX_ISR_NOERR 93\n"
    "  KX_ISR_NOERR 94\n"
    "  KX_ISR_NOERR 95\n"
    "  KX_ISR_NOERR 96\n"
    "  KX_ISR_NOERR 97\n"
    "  KX_ISR_NOERR 98\n"
    "  KX_ISR_NOERR 99\n"
    "  KX_ISR_NOERR 100\n"
    "  KX_ISR_NOERR 101\n"
    "  KX_ISR_NOERR 102\n"
    "  KX_ISR_NOERR 103\n"
    "  KX_ISR_NOERR 104\n"
    "  KX_ISR_NOERR 105\n"
    "  KX_ISR_NOERR 106\n"
    "  KX_ISR_NOERR 107\n"
    "  KX_ISR_NOERR 108\n"
    "  KX_ISR_NOERR 109\n"
    "  KX_ISR_NOERR 110\n"
    "  KX_ISR_NOERR 111\n"
    "  KX_ISR_NOERR 112\n"
    "  KX_ISR_NOERR 113\n"
    "  KX_ISR_NOERR 114\n"
    "  KX_ISR_NOERR 115\n"
    "  KX_ISR_NOERR 116\n"
    "  KX_ISR_NOERR 117\n"
    "  KX_ISR_NOERR 118\n"
    "  KX_ISR_NOERR 119\n"
    "  KX_ISR_NOERR 120\n"
    "  KX_ISR_NOERR 121\n"
    "  KX_ISR_NOERR 122\n"
    "  KX_ISR_NOERR 123\n"
    "  KX_ISR_NOERR 124\n"
    "  KX_ISR_NOERR 125\n"
    "  KX_ISR_NOERR 126\n"
    "  KX_ISR_NOERR 127\n"
    "  KX_ISR_NOERR 128\n"
    "  KX_ISR_NOERR 129\n"
    "  KX_ISR_NOERR 130\n"
    "  KX_ISR_NOERR 131\n"
    "  KX_ISR_NOERR 132\n"
    "  KX_ISR_NOERR 133\n"
    "  KX_ISR_NOERR 134\n"
    "  KX_ISR_NOERR 135\n"
    "  KX_ISR_NOERR 136\n"
    "  KX_ISR_NOERR 137\n"
    "  KX_ISR_NOERR 138\n"
    "  KX_ISR_NOERR 139\n"
    "  KX_ISR_NOERR 140\n"
    "  KX_ISR_NOERR 141\n"
    "  KX_ISR_NOERR 142\n"
    "  KX_ISR_NOERR 143\n"
    "  KX_ISR_NOERR 144\n"
    "  KX_ISR_NOERR 145\n"
    "  KX_ISR_NOERR 146\n"
    "  KX_ISR_NOERR 147\n"
    "  KX_ISR_NOERR 148\n"
    "  KX_ISR_NOERR 149\n"
    "  KX_ISR_NOERR 150\n"
    "  KX_ISR_NOERR 151\n"
    "  KX_ISR_NOERR 152\n"
    "  KX_ISR_NOERR 153\n"
    "  KX_ISR_NOERR 154\n"
    "  KX_ISR_NOERR 155\n"
    "  KX_ISR_NOERR 156\n"
    "  KX_ISR_NOERR 157\n"
    "  KX_ISR_NOERR 158\n"
    "  KX_ISR_NOERR 159\n"
    "  KX_ISR_NOERR 160\n"
    "  KX_ISR_NOERR 161\n"
    "  KX_ISR_NOERR 162\n"
    "  KX_ISR_NOERR 163\n"
    "  KX_ISR_NOERR 164\n"
    "  KX_ISR_NOERR 165\n"
    "  KX_ISR_NOERR 166\n"
    "  KX_ISR_NOERR 167\n"
    "  KX_ISR_NOERR 168\n"
    "  KX_ISR_NOERR 169\n"
    "  KX_ISR_NOERR 170\n"
    "  KX_ISR_NOERR 171\n"
    "  KX_ISR_NOERR 172\n"
    "  KX_ISR_NOERR 173\n"
    "  KX_ISR_NOERR 174\n"
    "  KX_ISR_NOERR 175\n"
    "  KX_ISR_NOERR 176\n"
    "  KX_ISR_NOERR 177\n"
    "  KX_ISR_NOERR 178\n"
    "  KX_ISR_NOERR 179\n"
    "  KX_ISR_NOERR 180\n"
    "  KX_ISR_NOERR 181\n"
    "  KX_ISR_NOERR 182\n"
    "  KX_ISR_NOERR 183\n"
    "  KX_ISR_NOERR 184\n"
    "  KX_ISR_NOERR 185\n"
    "  KX_ISR_NOERR 186\n"
    "  KX_ISR_NOERR 187\n"
    "  KX_ISR_NOERR 188\n"
    "  KX_ISR_NOERR 189\n"
    "  KX_ISR_NOERR 190\n"
    "  KX_ISR_NOERR 191\n"
    "  KX_ISR_NOERR 192\n"
    "  KX_ISR_NOERR 193\n"
    "  KX_ISR_NOERR 194\n"
    "  KX_ISR_NOERR 195\n"
    "  KX_ISR_NOERR 196\n"
    "  KX_ISR_NOERR 197\n"
    "  KX_ISR_NOERR 198\n"
    "  KX_ISR_NOERR 199\n"
    "  KX_ISR_NOERR 200\n"
    "  KX_ISR_NOERR 201\n"
    "  KX_ISR_NOERR 202\n"
    "  KX_ISR_NOERR 203\n"
    "  KX_ISR_NOERR 204\n"
    "  KX_ISR_NOERR 205\n"
    "  KX_ISR_NOERR 206\n"
    "  KX_ISR_NOERR 207\n"
    "  KX_ISR_NOERR 208\n"
    "  KX_ISR_NOERR 209\n"
    "  KX_ISR_NOERR 210\n"
    "  KX_ISR_NOERR 211\n"
    "  KX_ISR_NOERR 212\n"
    "  KX_ISR_NOERR 213\n"
    "  KX_ISR_NOERR 214\n"
    "  KX_ISR_NOERR 215\n"
    "  KX_ISR_NOERR 216\n"
    "  KX_ISR_NOERR 217\n"
    "  KX_ISR_NOERR 218\n"
    "  KX_ISR_NOERR 219\n"
    "  KX_ISR_NOERR 220\n"
    "  KX_ISR_NOERR 221\n"
    "  KX_ISR_NOERR 222\n"
    "  KX_ISR_NOERR 223\n"
    "  KX_ISR_NOERR 224\n"
    "  KX_ISR_NOERR 225\n"
    "  KX_ISR_NOERR 226\n"
    "  KX_ISR_NOERR 227\n"
    "  KX_ISR_NOERR 228\n"
    "  KX_ISR_NOERR 229\n"
    "  KX_ISR_NOERR 230\n"
    "  KX_ISR_NOERR 231\n"
    "  KX_ISR_NOERR 232\n"
    "  KX_ISR_NOERR 233\n"
    "  KX_ISR_NOERR 234\n"
    "  KX_ISR_NOERR 235\n"
    "  KX_ISR_NOERR 236\n"
    "  KX_ISR_NOERR 237\n"
    "  KX_ISR_NOERR 238\n"
    "  KX_ISR_NOERR 239\n"
    "  KX_ISR_NOERR 240\n"
    "  KX_ISR_NOERR 241\n"
    "  KX_ISR_NOERR 242\n"
    "  KX_ISR_NOERR 243\n"
    "  KX_ISR_NOERR 244\n"
    "  KX_ISR_NOERR 245\n"
    "  KX_ISR_NOERR 246\n"
    "  KX_ISR_NOERR 247\n"
    "  KX_ISR_NOERR 248\n"
    "  KX_ISR_NOERR 249\n"
    "  KX_ISR_NOERR 250\n"
    "  KX_ISR_NOERR 251\n"
    "  KX_ISR_NOERR 252\n"
    "  KX_ISR_NOERR 253\n"
    "  KX_ISR_NOERR 254\n"
    "  KX_ISR_NOERR 255\n"
    ".balign 16\n"
    "kx_isr_common:\n"
    /* прерывали программу (CS в рамке с RPL 3) - регистр GS сейчас её:
       swapgs делает его "ядерным" (структура этого ядра процессора,
       smp.c). В рамке: +0 вектор, +8 код ошибки, +16 rip, +24 cs */
    "  testb $3, 24(%rsp)\n"
    "  jz 1f\n"
    "  swapgs\n"
    "1:\n"
    "  pushq %rax\n"
    "  pushq %rbx\n"
    "  pushq %rcx\n"
    "  pushq %rdx\n"
    "  pushq %rsi\n"
    "  pushq %rdi\n"
    "  pushq %rbp\n"
    "  pushq %r8\n"
    "  pushq %r9\n"
    "  pushq %r10\n"
    "  pushq %r11\n"
    "  pushq %r12\n"
    "  pushq %r13\n"
    "  pushq %r14\n"
    "  pushq %r15\n"
    "  movq %rsp, %rbx\n"
    "  subq $512, %rsp\n"
    "  andq $-16, %rsp\n"
    "  fxsave (%rsp)\n"
    "  movq %rbx, %rdi\n"
    "  cld\n"
    "  call kx_isr_dispatch\n"
    "  fxrstor (%rsp)\n"
    "  movq %rbx, %rsp\n"
    "  popq %r15\n"
    "  popq %r14\n"
    "  popq %r13\n"
    "  popq %r12\n"
    "  popq %r11\n"
    "  popq %r10\n"
    "  popq %r9\n"
    "  popq %r8\n"
    "  popq %rbp\n"
    "  popq %rdi\n"
    "  popq %rsi\n"
    "  popq %rdx\n"
    "  popq %rcx\n"
    "  popq %rbx\n"
    "  popq %rax\n"
    "  addq $16, %rsp\n"
    /* возвращаемся в программу - вернуть ей её GS (+8 - cs) */
    "  testb $3, 8(%rsp)\n"
    "  jz 2f\n"
    "  swapgs\n"
    "2:\n"
    "  iretq\n"
);

/* Счётчики для команды kinfo */
volatile UINT64 g_kticks = 0;        /* миллисекунды от старта
                                                таймера */
volatile UINT64 g_kspurious = 0;
volatile UINT64 g_kstray = 0;
volatile UINT64 g_kstray_last = 0;
volatile UINT64 g_kbreakpoints = 0;
volatile UINT64 g_kbp_rip = 0;


const char *kx_exception_name(UINT64 v)
{
    static const char *names[32] = {
        "#DE Divide Error",
        "#DB Debug",
        "NMI",
        "#BP Breakpoint",
        "#OF Overflow",
        "#BR BOUND Range",
        "#UD Invalid Opcode",
        "#NM Device Not Available",
        "#DF Double Fault",
        "Coprocessor Segment Overrun",
        "#TS Invalid TSS",
        "#NP Segment Not Present",
        "#SS Stack-Segment Fault",
        "#GP General Protection",
        "#PF Page Fault",
        "(reserved 15)",
        "#MF x87 FPU Error",
        "#AC Alignment Check",
        "#MC Machine Check",
        "#XM SIMD Exception",
        "#VE Virtualization",
        "#CP Control Protection",
        "(reserved 22)", "(reserved 23)", "(reserved 24)",
        "(reserved 25)", "(reserved 26)", "(reserved 27)",
        "#HV Hypervisor Injection",
        "#VC VMM Communication",
        "#SX Security",
        "(reserved 31)"
    };

    if (v < 32)
        return names[v];

    return "?";
}


/* Человеческое объяснение исключения - две строки для экрана
   паники и лога. Для Page Fault разбираем код ошибки:
     бит 0 P   - 0: страницы нет, 1: страница есть, но нельзя так
     бит 1 W   - это была запись (иначе чтение)
     бит 2 U   - из программы (ring 3)
     бит 3 RSVD- испорченная запись таблицы страниц
     бит 4 I   - это была выборка инструкции (исполнение) */
static void kx_explain(KX_ISR_FRAME *f, UINT64 cr2, char *l1, char *l2, UINTN cap)
{
    l1[0] = '\0';
    l2[0] = '\0';

    if (f->vector == 14) {

        UINT64 e = f->error;
        const char *op = (e & 0x10u) ? "EXECUTE" : (e & 0x2u) ? "WRITE" : "READ";
        const char *why;

        if (!(e & 0x1u))
            why = "the page is not mapped";
        else if (e & 0x8u)
            why = "a page table entry is corrupted (reserved bit set)";
        else if (e & 0x10u)
            why = "the page is not executable (NX)";
        else if (e & 0x2u)
            why = "the page is READ-ONLY";
        else
            why = "access not allowed";

        ksnprintf(l1, cap, "Page Fault: %s of 0x%016llx - %s", op, cr2, why);
        ksnprintf(l2, cap, "  that address is: %s", vmm_describe(cr2));

    } else if (f->vector == 8) {

        const char *d = vmm_describe(cr2);
        BOOLEAN overflow = (d[0] == 'G');   /* "GUARD PAGE ..." */

        if (overflow) {
            ksnprintf(l1, cap, "KERNEL STACK OVERFLOW (hit the guard page at 0x%016llx)", cr2);
            ksnprintf(l2, cap, "  too deep recursion or a huge local array");
        } else {
            ksnprintf(l1, cap, "Double Fault: an exception while handling another one");
            ksnprintf(l2, cap, "  last page fault address (CR2) is: %s", d);
        }

    } else if (f->vector == 13) {

        ksnprintf(l1, cap, "General Protection: bad pointer (non-canonical), bad segment");
        ksnprintf(l2, cap, "  or privileged instruction; error code 0x%llx", f->error);

    } else if (f->vector == 6) {

        ksnprintf(l1, cap, "Invalid Opcode: the CPU does not know this instruction");
        ksnprintf(l2, cap, "  ('kpanic' does this on purpose with ud2)");

    } else if (f->vector == 0) {

        ksnprintf(l1, cap, "Division by zero");
    }
}


/*
 * Экран "паники" - исключение процессора (деление на ноль,
 * обращение по неверному адресу и т.п.) в нашем коде. Раньше в
 * такой ситуации машина либо зависала, либо молча
 * перезагружалась (triple fault) - теперь видно, ЧТО и ГДЕ
 * случилось: номер исключения, адрес инструкции (RIP), для
 * Page Fault - адрес, к которому обращались (CR2), что именно
 * пошло не так (запись в память только для чтения? страницы
 * нет?) и чья это память (код ядра, стек, NULL...), и регистры.
 * Всё то же уходит в COM1 - в QEMU это текст в терминале.
 */
void kx_panic(KX_ISR_FRAME *f)
{
    kx_cli();

    /* остальные ядра процессора - остановить: экран паники - наш */
    smp_halt_others();

    UINT64 cr2 = kx_read_cr2();
    char ex1[112], ex2[112];

    kx_explain(f, cr2, ex1, ex2, sizeof(ex1));

    klog("*** KERNEL PANIC: vector %llu %s\n", f->vector,
         kx_exception_name(f->vector));
    if (ex1[0]) klog("%s\n", ex1);
    if (ex2[0]) klog("%s\n", ex2);
    klog("RIP=0x%016llx (%s) ERR=0x%llx CR2=0x%016llx RSP=0x%016llx\n",
         f->rip, vmm_describe(f->rip), f->error, cr2, f->rsp);

    if (g_kfb == NULL) {
        for (;;)
            kx_hlt();
    }

    UINT32 bg = gui_pack(g_kfb_fmt, 120, 0, 0);
    UINT32 fg = gui_pack(g_kfb_fmt, 255, 255, 255);
    UINT32 hl = gui_pack(g_kfb_fmt, 255, 220, 90);

    UINTN bw = 8u * 90u;
    UINTN bh = 16u * 26u;

    if (bw > g_kfb_w)
        bw = g_kfb_w;

    if (bh > g_kfb_h)
        bh = g_kfb_h;

    gui_fill_rect(
        g_kfb, g_kfb_stride, g_kfb_w, g_kfb_h,
        0, 0, bw, bh, bg
    );

    UINTN x = 16;
    UINTN y = 12;

    kx_raw_text(x, y, "*** MyOS KERNEL PANIC: CPU EXCEPTION ***", hl, bg);
    y += 24;

    char line[112];

    ksnprintf(line, sizeof(line), "Vector 0x%02llx: %s", f->vector,
              kx_exception_name(f->vector));
    kx_raw_text(x, y, line, fg, bg);
    y += 20;

    if (ex1[0]) {
        kx_raw_text(x, y, ex1, hl, bg);
        y += 16;
    }

    if (ex2[0]) {
        kx_raw_text(x, y, ex2, hl, bg);
        y += 16;
    }

    ksnprintf(line, sizeof(line), "  the instruction is in: %s", vmm_describe(f->rip));
    kx_raw_text(x, y, line, fg, bg);
    y += 24;

    /* Пары "ИМЯ = значение" */
    const char *names[] = {
        "ERROR", "RIP  ", "CR2  ", "RSP  ", "RFLAGS",
        "CS   ", "RAX  ", "RBX  ", "RCX  ", "RDX  ",
        "RSI  ", "RDI  ", "RBP  "
    };

    UINT64 vals[13];

    vals[0] = f->error;
    vals[1] = f->rip;
    vals[2] = cr2;
    vals[3] = f->rsp;
    vals[4] = f->rflags;
    vals[5] = f->cs;
    vals[6] = f->rax;
    vals[7] = f->rbx;
    vals[8] = f->rcx;
    vals[9] = f->rdx;
    vals[10] = f->rsi;
    vals[11] = f->rdi;
    vals[12] = f->rbp;

    for (UINTN k = 0; k < 13; k++) {

        ksnprintf(line, sizeof(line), "%s = 0x%016llx", names[k], vals[k]);
        kx_raw_text(x, y, line, fg, bg);
        y += 16;
    }

    y += 8;
    kx_raw_text(
        x, y,
        "System halted. Take a screenshot, then reset the machine.",
        hl, bg
    );

    for (;;)
        kx_hlt();
}


/*
 * Часы в миллисекундах (g_kticks) - по TSC, а не счётом прерываний:
 * на нескольких ядрах обработчик таймера может подождать большой
 * замок ядра дольше тика, и счёт прерываний отставал бы от времени.
 * Ведёт загрузочное ядро; раз в секунду - пересчёт загрузки.
 */
static void kx_advance_ticks(void)
{
    static UINT64 base_ms;
    static BOOLEAN have_base;

    UINT64 now = kx_uptime_us() / 1000u;

    if (!have_base) {
        base_ms = now - g_kticks;
        have_base = TRUE;
    }

    UINT64 t = now - base_ms;
    UINT64 old = g_kticks;

    if (t <= old)
        return;                 /* та же миллисекунда */

    g_kticks = t;

    if (t / 1000u != old / 1000u)
        kx_load_tick();
}

/* Разбор прерывания по вектору (зовётся из kx_isr_dispatch ниже) */
static void kx_isr_dispatch_inner(KX_ISR_FRAME *f)
{
    UINT64 v = f->vector;

    if (v == KX_VEC_TIMER) {

        /* у каждого ядра процессора свой таймер; часы (g_kticks) и
           учёт загрузки ведёт только загрузочное */
        kx_cpu()->ticks++;

        if (kx_cpu_index() == 0)
            kx_advance_ticks();

        /* планировщик: разбудить тех, у кого вышел сон, и
           отсчитать квант текущего потока (sched.c) */
        sched_tick();

        kx_lapic_eoi();
        return;
    }

    if (v == KX_VEC_RESCHED) {

        /* другое ядро: "для тебя есть готовый поток" - сменить поток
           на выходе из прерывания (sched_isr_exit) */
        kx_cpu()->need_resched = TRUE;
        kx_lapic_eoi();
        return;
    }

    if (v == KX_VEC_HALT) {

        /* паника на другом ядре - остановиться, не мешать экрану */
        kx_lapic_eoi();
        for (;;)
            __asm__ __volatile__("cli; hlt" ::: "memory");
    }

    if (v == KX_VEC_SPURIOUS) {

        /* "ложное" прерывание Local APIC - по спеке на него
           EOI не посылается */
        g_kspurious = g_kspurious + 1;
        return;
    }

    if (v == 3) {

        /*
         * #BP (инструкция int3) - "ловушка": процессор кладёт в
         * RIP адрес СЛЕДУЮЩЕЙ инструкции, поэтому можно просто
         * вернуться и продолжить. Используется командой "int3"
         * как безопасная живая проверка, что наша IDT реально
         * работает.
         */
        g_kbreakpoints = g_kbreakpoints + 1;
        g_kbp_rip = f->rip;
        return;
    }

    if (v < 32) {

        /* исключение в ПРОГРАММЕ (ring 3): виновата она, а не ядро -
           программа будет завершена, ОС работает дальше (proc.c) */
        if ((f->cs & 3u) == 3u) {
            kx_user_fault(f);
            return;
        }

        kx_panic(f);
        return;
    }

    /* Прерывание устройства, у которого есть обработчик
       (kernel/irq.c: PS/2, USB) */
    if (kx_irq_dispatch((UINT8)v))
        return;

    /* Неожиданное внешнее прерывание (все источники, о которых
       мы знаем, замаскированы - но на всякий случай не
       зависаем, а считаем и подтверждаем) */
    g_kstray = g_kstray + 1;
    g_kstray_last = v;
    kx_lapic_eoi();
}

/*
 * Общий вход из ассемблерной заглушки kx_isr_common.
 *
 * g_kx_isr_depth > 0 означает "мы в обработчике прерывания": там
 * нельзя спать и ждать (sched.c это проверяет). В самом конце, уже
 * после EOI, - точка вытеснения: если таймер сказал "квант вышел"
 * или прерывание разбудило поток, планировщик переключает поток
 * прямо отсюда. Рамка прерывания остаётся в стеке прерванного
 * потока; он "вернётся из прерывания", когда до него дойдёт очередь.
 *
 * Не static и с "used", потому что вызывается только из ассемблера
 * - иначе компилятор решил бы, что она никому не нужна, и выбросил
 * бы её.
 */
__attribute__((used, visibility("hidden")))
void kx_isr_dispatch(KX_ISR_FRAME *f)
{
    /* прерывания здесь запрещены: ядро процессора не сменится */
    KX_CPU *c = kx_cpu();

    /* код ядра ОС - под большим замком (SMP, sched.c). Остановка
       по панике - без него: владелец замка, может быть, и есть тот,
       кто паникует */
    if (f->vector == KX_VEC_HALT)
        kx_isr_dispatch_inner(f);

    kx_bkl_enter();

    c->isr_depth++;

    kx_isr_dispatch_inner(f);

    c->isr_depth--;

    /* только внешние прерывания (таймер, устройства): исключения -
       это ошибка или int3, переключаться там незачем */
    if (f->vector >= 32)
        sched_isr_exit();

    /* возвращаемся в программу, а её попросили завершиться
       (упала, Ctrl+C, закрыли окно) - завершить прямо здесь */
    if ((f->cs & 3u) == 3u)
        proc_check_kill();

    /* прервали код ядра ОС в месте, где прерывания были разрешены
       (там его и так мог сменить любой поток) - если другое ядро
       ждёт большой замок, пропустить его вперёд (smp.c) */
    if ((f->cs & 3u) == 0 && (f->rflags & (1u << 9)))
        kx_bkl_relax();

    kx_bkl_exit();
}

/*
 * Исключение в программе: записать понятное объяснение (то же, что
 * на экране паники ядра) и пометить программу "завершить".
 */
void kx_user_fault(KX_ISR_FRAME *f)
{
    char l1[128], l2[128];
    UINT64 cr2 = (f->vector == 14) ? kx_read_cr2() : 0;

    kx_explain(f, cr2, l1, l2, sizeof(l1));

    if (f->vector == 13)
        ksnprintf(l1, sizeof(l1), "General Protection: only the kernel may do that "
                                  "(privileged instruction) or a bad address");
    else if (f->vector == 14 && cr2 >= 0xFFFF800000000000ull)
        ksnprintf(l1, sizeof(l1), "Page Fault: tried to touch KERNEL memory at 0x%llx - "
                                  "programs are not allowed there", cr2);

    if (l1[0] == '\0')
        ksnprintf(l1, sizeof(l1), "%s", kx_exception_name(f->vector));

    proc_fault(l1, f->rip);
}


void kx_idt_set(UINTN vec, UINT64 handler)
{
    g_kidt[vec].off_lo = (UINT16)(handler & 0xFFFFu);
    g_kidt[vec].selector = 0x08;
    g_kidt[vec].ist = 0;
    /* 0x8E: P=1, DPL=0, тип 0xE = 64-битный interrupt gate
       (процессор сам сбрасывает IF при входе - обработчик не
       прерывается следующим прерыванием) */
    g_kidt[vec].type_attr = 0x8E;
    g_kidt[vec].off_mid = (UINT16)((handler >> 16) & 0xFFFFu);
    g_kidt[vec].off_hi = (UINT32)(handler >> 32);
    g_kidt[vec].zero = 0;
}


void kx_load_idt(void)
{
    UINT64 base = (UINT64)(UINTN)kx_isr_stubs;

    for (UINTN v = 0; v < 256; v++)
        kx_idt_set(v, base + (UINT64)v * 16u);

    KX_DTR idtr;

    idtr.limit = (UINT16)(sizeof(g_kidt) - 1);
    idtr.base = (UINT64)(UINTN)&g_kidt[0];

    __asm__ __volatile__("lidt %0" : : "m"(idtr) : "memory");
}


/*
 * Старый контроллер прерываний 8259 (PIC). Мы им не
 * пользуемся (таймер - Local APIC), но он физически есть
 * (или эмулируется чипсетом), и после включения прерываний
 * мог бы прислать что-нибудь. По умолчанию его векторы 0x08-
 * 0x0F совпадают с векторами ИСКЛЮЧЕНИЙ процессора (например,
 * 0x08 = Double Fault) - поэтому даже замаскированный PIC
 * положено сначала перенастроить на безопасные 0x20-0x2F
 * (классическая последовательность ICW1-ICW4), и только потом
 * замаскировать все его линии.
 */
void kx_pic_disable(void)
{
    io_out8(0x20, 0x11); io_wait();   /* ICW1: init + ICW4 */
    io_out8(0xA0, 0x11); io_wait();
    io_out8(0x21, 0x20); io_wait();   /* ICW2: master -> 0x20 */
    io_out8(0xA1, 0x28); io_wait();   /* ICW2: slave  -> 0x28 */
    io_out8(0x21, 0x04); io_wait();   /* ICW3: slave на IRQ2 */
    io_out8(0xA1, 0x02); io_wait();
    io_out8(0x21, 0x01); io_wait();   /* ICW4: режим 8086 */
    io_out8(0xA1, 0x01); io_wait();
    io_out8(0x21, 0xFF);              /* замаскировать все */
    io_out8(0xA1, 0xFF);
}


/*
 * I/O APIC - современный "распределитель" внешних прерываний
 * (от чипсета, ACPI, HPET и т.п.). Прошивка могла оставить в
 * нём включённые записи, нацеленные на свои векторы, - после
 * включения прерываний они прилетали бы к нам, а уровневые (как
 * ACPI SCI) прилетали бы бесконечно, пока их не обслужат.
 * Маскируем все записи. Адрес каждого I/O APIC - из ACPI-
 * таблицы MADT (если её нет - стандартный 0xFEC00000).
 * Возвращает число замаскированных записей, 0 если I/O APIC
 * по этому адресу не отвечает.
 */
UINTN kx_ioapic_mask_all(UINT64 base)
{
    /* регистры I/O APIC - некэшируемые */
    if (!vmm_ensure_mapped(base, 0x20, VMM_UC))
        return 0;

    mmio_write32(base + 0x00, 0x01);   /* IOREGSEL = версия */

    UINT32 ver = mmio_read32(base + 0x10);

    if (ver == 0xFFFFFFFFu)
        return 0;

    UINTN max_entry = (ver >> 16) & 0xFFu;

    if (max_entry > 239)
        return 0;

    for (UINTN i = 0; i <= max_entry; i++) {

        mmio_write32(base + 0x00, (UINT32)(0x10u + 2u * i));

        UINT32 lo = mmio_read32(base + 0x10);

        mmio_write32(base + 0x10, lo | (1u << 16));
    }

    return max_entry + 1;
}
