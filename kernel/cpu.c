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
 * 64-битному режиму нужны всего два дескриптора (сегментация в
 * long mode почти не работает - базы и лимиты игнорируются):
 *   0x08 - код:   L=1 (64-битный), P=1, DPL=0, исполняемый
 *   0x10 - данные: P=1, DPL=0, запись разрешена
 * TSS (нужен для отдельного стека на double fault и для
 * перехода в ring 3) - следующий шаг, пока не заводим.
 */
UINT64 g_kgdt[3] __attribute__((aligned(16))) = {
    0x0000000000000000ull,
    0x00AF9A000000FFFFull,
    0x00CF92000000FFFFull
};

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


/*
 * Экран "паники" - исключение процессора (деление на ноль,
 * обращение по неверному адресу и т.п.) в нашем коде. Раньше в
 * такой ситуации машина либо зависала, либо молча
 * перезагружалась (triple fault) - теперь видно, ЧТО и ГДЕ
 * случилось: номер исключения, адрес инструкции (RIP), для
 * Page Fault - адрес, к которому обращались (CR2), и регистры.
 * Этого достаточно, чтобы по скриншоту найти ошибку.
 */
void kx_panic(KX_ISR_FRAME *f)
{
    kx_cli();

    if (g_kfb == NULL) {
        for (;;)
            kx_hlt();
    }

    UINT32 bg = gui_pack(g_kfb_fmt, 120, 0, 0);
    UINT32 fg = gui_pack(g_kfb_fmt, 255, 255, 255);
    UINT32 hl = gui_pack(g_kfb_fmt, 255, 220, 90);

    UINTN bw = 8u * 64u;
    UINTN bh = 16u * 20u;

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

    char line[80];
    char num[20];
    UINTN p;

    /* "Vector N: имя" */
    p = 0;
    {
        const char *a = "Vector 0x";
        for (UINTN i = 0; a[i]; i++) line[p++] = a[i];
        kx_hex_str(f->vector, 2, num);
        for (UINTN i = 0; num[i]; i++) line[p++] = num[i];
        line[p++] = ':';
        line[p++] = ' ';
        const char *n = kx_exception_name(f->vector);
        for (UINTN i = 0; n[i] && p < 78; i++) line[p++] = n[i];
        line[p] = '\0';
    }
    kx_raw_text(x, y, line, fg, bg);
    y += 20;

    /* Пары "ИМЯ = значение" */
    const char *names[] = {
        "ERROR", "RIP  ", "CR2  ", "RSP  ", "RFLAGS",
        "CS   ", "RAX  ", "RBX  ", "RCX  ", "RDX  ",
        "RSI  ", "RDI  ", "RBP  "
    };

    UINT64 vals[13];

    vals[0] = f->error;
    vals[1] = f->rip;
    vals[2] = kx_read_cr2();
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

        p = 0;

        for (UINTN i = 0; names[k][i]; i++)
            line[p++] = names[k][i];

        line[p++] = ' ';
        line[p++] = '=';
        line[p++] = ' ';
        line[p++] = '0';
        line[p++] = 'x';

        kx_hex_str(vals[k], 16, num);

        for (UINTN i = 0; num[i]; i++)
            line[p++] = num[i];

        line[p] = '\0';

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
 * Единая C-точка входа всех прерываний (вызывается из
 * kx_isr_common). Не static и с "used", потому что вызывается
 * только из ассемблера - иначе компилятор решил бы, что она
 * никому не нужна, и выбросил бы её.
 */
__attribute__((used, visibility("hidden")))
void kx_isr_dispatch(KX_ISR_FRAME *f)
{
    UINT64 v = f->vector;

    if (v == KX_VEC_TIMER) {

        g_kticks = g_kticks + 1;
        kx_lapic_eoi();
        return;
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

        kx_panic(f);
        return;
    }

    /* Неожиданное внешнее прерывание (все источники, о которых
       мы знаем, замаскированы - но на всякий случай не
       зависаем, а считаем и подтверждаем) */
    g_kstray = g_kstray + 1;
    g_kstray_last = v;
    kx_lapic_eoi();
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
 * Маскируем все записи. Стандартный адрес первого I/O APIC на
 * PC - 0xFEC00000 (строго говоря, его надо брать из ACPI-
 * таблицы MADT - это следующий шаг, вместе с разбором ACPI).
 * Возвращает число замаскированных записей, 0 если I/O APIC
 * по этому адресу не отвечает.
 */
UINTN kx_ioapic_mask_all(void)
{
    UINT64 base = 0xFEC00000ull;

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
