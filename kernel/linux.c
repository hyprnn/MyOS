/*
 * kernel/linux.c - программы Linux на MyOS (этап 11). Часть MyOS.
 *
 * ИДЕЯ
 * ----
 * Программа, собранная для Linux, - это ELF-файл, который общается с
 * ядром только через инструкцию syscall: номер вызова в rax (номера
 * Linux: 0 - read, 1 - write, 9 - mmap...), аргументы в rdi, rsi, rdx,
 * r10, r8, r9, ответ - в rax (отрицательный - код ошибки errno). Если
 * ядро MyOS отвечает на эти вызовы так же, как ядро Linux, программа не
 * заметит разницы. Так устроены WSL1 в Windows и "Linuxulator" FreeBSD.
 *
 * Чем программа Linux отличается от программы MyOS:
 *   * ELF без метки MyOS (программы MyOS линкуются с особым заголовком
 *     программы 0x6D794F53 - "MyOS", см. user/user.ld);
 *   * память - областями по требованию (umem.c), mmap/munmap/mprotect;
 *   * при старте на стеке, кроме argc/argv, - переменные окружения и
 *     "вспомогательный вектор" auxv (где заголовки программы, размер
 *     страницы, 16 случайных байт...) - libc без них не стартует;
 *   * регистр FS - адрес данных потока (TLS), arch_prctl;
 *   * потоки (clone с CLONE_THREAD) и замки futex;
 *   * fork / execve / wait4, каналы, сигналы (lxsig.c);
 *   * файлы - описания с общим местом чтения (lxfile.c).
 *
 * Номера вызовов, флаги и структуры - kernel/linux.h. Неизвестный
 * вызов отвечает ENOSYS (и пишется в журнал один раз), - обычно libc
 * тогда пробует другой путь.
 */
#include "linux.h"

/* Рамка системного вызова (syscall.c): индексы регистров */
#define SF_R15  0
#define SF_R14  1
#define SF_R13  2
#define SF_R12  3
#define SF_RBP  4
#define SF_RBX  5
#define SF_R9   6
#define SF_R8   7
#define SF_R10  8
#define SF_RDX  9
#define SF_RSI 10
#define SF_RDI 11
#define SF_RAX 12
#define SF_RCX 13
#define SF_R11 14
#define SF_RSP 15

/* Где что лежит в памяти программы Linux */
#define LX_STACK_TOP     MYOS_USER_STACK_TOP           /* вершина стека */
#define LX_STACK_SIZE    (8ull * 1024u * 1024u)        /* 8 МБ, как ulimit -s */
#define LX_MMAP_TOP      0x00007F0000000000ull         /* mmap - вниз отсюда */
#define LX_PIE_BASE      0x0000555555554000ull         /* программа ET_DYN */
#define LX_MAX_ARGS      (256u * 1024u)                /* argv + envp вместе */

/* Начальное состояние FPU/SSE (fxsave): FCW 0x37F, MXCSR 0x1F80 */
static UINT8 g_fx_default[512] __attribute__((aligned(16)));

static void fx_default_init(void)
{
    if (g_fx_default[0] != 0)
        return;

    memset(g_fx_default, 0, sizeof(g_fx_default));
    g_fx_default[0] = 0x7F;                 /* FCW = 0x037F */
    g_fx_default[1] = 0x03;
    g_fx_default[24] = 0x80;                /* MXCSR = 0x1F80 */
    g_fx_default[25] = 0x1F;
    g_fx_default[28] = 0xFF;                /* MXCSR_MASK */
    g_fx_default[29] = 0xFF;
}


/* ================================================================
 * Вход в программу со всеми регистрами (iretq)
 * ================================================================ */

/*
 * kx_lx_iret(r, fx): восстановить SSE из fx (512 байт, выровнены на
 * 16), все регистры из r и вернуться в ring 3 инструкцией iretq (её
 * рамка - последние 5 полей LX_REGS). Стек ядра бросаем: следующий
 * вход в ядро начнёт его заново с вершины (TSS.rsp0).
 */
__asm__(
    ".text\n"
    ".globl kx_lx_iret\n"
    ".hidden kx_lx_iret\n"
    "kx_lx_iret:\n"
    "  cli\n"
    "  fxrstor (%rsi)\n"
    "  movq %rdi, %rsp\n"
    "  popq %r15\n"
    "  popq %r14\n"
    "  popq %r13\n"
    "  popq %r12\n"
    "  popq %rbp\n"
    "  popq %rbx\n"
    "  popq %r11\n"
    "  popq %r10\n"
    "  popq %r9\n"
    "  popq %r8\n"
    "  popq %rax\n"
    "  popq %rcx\n"
    "  popq %rdx\n"
    "  popq %rsi\n"
    "  popq %rdi\n"
    "  swapgs\n"
    "  iretq\n"
);

/*
 * Войти в программу с регистрами r. Адрес команды обязан быть в
 * нижней половине: iretq на "неканонический" адрес упал бы в ядре.
 */
static void __attribute__((noreturn)) lx_enter(LX_REGS *r, const void *fx)
{
    if (r->rip >= 0x00007FFFFFFFF000ull)
        r->rip = 0x1000;                    /* программа упадёт у себя, не в ядре */

    r->cs = 0x2B;
    r->ss = 0x23;
    /* флаги: только то, что программе можно (CF PF AF ZF SF TF DF OF
       AC ID), плюс IF и всегда-единица */
    r->rflags = (r->rflags & (0xFD5ull | (1ull << 18) | (1ull << 21))) | 0x202ull;

    kx_cli();
    kx_bkl_exit();
    kx_lx_iret(r, fx);
}

/* Регистры из рамки системного вызова (rax - ответ вызова) */
static void regs_from_syscall(const UINT64 *f, LX_REGS *r, INT64 rax)
{
    memset(r, 0, sizeof(*r));
    r->r15 = f[SF_R15]; r->r14 = f[SF_R14]; r->r13 = f[SF_R13]; r->r12 = f[SF_R12];
    r->rbp = f[SF_RBP]; r->rbx = f[SF_RBX];
    r->r9 = f[SF_R9]; r->r8 = f[SF_R8]; r->r10 = f[SF_R10];
    r->rdx = f[SF_RDX]; r->rsi = f[SF_RSI]; r->rdi = f[SF_RDI];
    r->rax = (UINT64)rax;
    r->rcx = f[SF_RCX];
    r->r11 = f[SF_R11];
    r->rip = f[SF_RCX];
    r->rflags = f[SF_R11];
    r->rsp = f[SF_RSP];
}

/* fxsave рамки: вход (syscall.c / cpu.c) кладёт его под регистры */
static const void *frame_fx(const void *f)
{
    return (const void *)((((UINT64)(UINTN)f) - 512u) & ~0xFull);
}

UINT32 lx_tid(KTHREAD *t)
{
    return t->lx_tid ? t->lx_tid : t->tid;
}


/* ================================================================
 * Коды ошибок MyOS -> errno Linux
 * ================================================================ */

INT64 linux_errno(INT64 e)
{
    if (e >= 0)
        return e;

    switch (e) {
    case MYOS_ENOENT:        return -LX_ENOENT;
    case MYOS_EEXIST:        return -LX_EEXIST;
    case MYOS_ENOTDIR:       return -LX_ENOTDIR;
    case MYOS_EISDIR:        return -LX_EISDIR;
    case MYOS_ENOTEMPTY:     return -LX_ENOTEMPTY;
    case MYOS_ENOSPC:        return -LX_ENOSPC;
    case MYOS_EROFS:         return -LX_EROFS;
    case MYOS_EIO:           return -LX_EIO;
    case MYOS_EINVAL:        return -LX_EINVAL;
    case MYOS_EBADF:         return -LX_EBADF;
    case MYOS_EMFILE:        return -LX_EMFILE;
    case MYOS_ENOSYS:        return -LX_ENOSYS;
    case MYOS_EGONE:         return -LX_ENODEV;
    case MYOS_EXDEV:         return -LX_EXDEV;
    case MYOS_EFAULT:        return -LX_EFAULT;
    case MYOS_ENOGUI:        return -LX_ENODEV;
    case MYOS_ETIMEDOUT:     return -LX_ETIMEDOUT;
    case MYOS_ECONNREFUSED:  return -LX_ECONNREFUSED;
    case MYOS_ECONNRESET:    return -LX_ECONNRESET;
    case MYOS_ENETUNREACH:   return -LX_ENETUNREACH;
    case MYOS_EADDRINUSE:    return -LX_EADDRINUSE;
    case MYOS_ENOTCONN:      return -LX_ENOTCONN;
    case MYOS_EAGAIN:        return -LX_EAGAIN;
    case MYOS_EINTR:         return -LX_EINTR;
    case MYOS_EINPROGRESS:   return -LX_EINPROGRESS;
    case MYOS_EBUSY:         return -LX_EBUSY;
    case MYOS_ENODEV:        return -LX_ENODEV;
    case MYOS_ENOMEM:        return -LX_ENOMEM;
    }

    return -LX_EIO;
}


/* ================================================================
 * Время
 * ================================================================ */

static UINT64 g_epoch_base_ns;      /* "сейчас" по RTC минус время от старта */

/* Дата по григорианскому календарю -> дни с 1970-01-01 */
static UINT64 days_from_civil(UINT32 y, UINT32 m, UINT32 d)
{
    INT64 yy = (INT64)y - (m <= 2 ? 1 : 0);
    INT64 era = yy / 400;
    INT64 yoe = yy - era * 400;
    INT64 mp = (m + 9) % 12;
    INT64 doy = (153 * mp + 2) / 5 + d - 1;
    INT64 doe = yoe * 365 + yoe / 4 - yoe / 100 + doy;

    return (UINT64)(era * 146097 + doe - 719468);
}

UINT64 lx_now_ns(BOOLEAN realtime)
{
    UINT64 mono = kx_uptime_us() * 1000u;

    if (!realtime)
        return mono;

    if (g_epoch_base_ns == 0) {
        EFI_TIME t;
        if (rtc_read(&t)) {
            UINT64 secs = days_from_civil(t.Year, t.Month, t.Day) * 86400u +
                          (UINT64)t.Hour * 3600u + (UINT64)t.Minute * 60u + t.Second;
            g_epoch_base_ns = secs * 1000000000ull - mono;
        } else {
            g_epoch_base_ns = 1767225600ull * 1000000000ull;    /* 2026-01-01 */
        }
    }

    return g_epoch_base_ns + mono;
}

/* Поспать ns наносекунд; прерывается сигналом. 0 или -ERESTARTSYS */
static INT64 lx_sleep_ns(KPROC *p, UINT64 ns)
{
    UINT64 until = lx_now_ns(FALSE) + ns;

    for (;;) {

        UINT64 now = lx_now_ns(FALSE);

        if (now >= until)
            return 0;

        if (p->killed || lx_signal_pending())
            return -LX_EINTR;

        UINT64 left_ms = (until - now + 999999u) / 1000000u;

        /* меньше миллисекунды - уступить процессор (точнее таймер не
           умеет); дольше - спать кусками, чтобы заметить сигнал */
        if (left_ms <= 1 && until - now < 1000000u) {
            sched_yield();
            continue;
        }

        sched_sleep_ms(left_ms > 20 ? 20 : left_ms);
    }
}


/* ================================================================
 * Загрузка ELF программы Linux
 * ================================================================ */

typedef struct {
    UINT8  ident[16];
    UINT16 type, machine;
    UINT32 version;
    UINT64 entry, phoff, shoff;
    UINT32 flags;
    UINT16 ehsize, phentsize, phnum, shentsize, shnum, shstrndx;
} LX_EHDR;

typedef struct {
    UINT32 type, flags;
    UINT64 offset, vaddr, paddr, filesz, memsz, align;
} LX_PHDR;

#define LPT_LOAD     1
#define LPT_DYNAMIC  2
#define LPT_INTERP   3
#define LPT_NOTE     4
#define LPT_PHDR     6
#define LPT_TLS      7
#define LPT_MYOS     0x6D794F53u      /* метка программы MyOS (user.ld) */

/*
 * Программа для Linux или для MyOS? У программ MyOS есть заголовок-
 * метка LPT_MYOS. Старые программы MyOS (до метки) - только три
 * PT_LOAD и ничего больше; у программ Linux всегда есть что-то ещё:
 * PT_GNU_STACK, PT_NOTE, PT_TLS, PT_INTERP...
 */
BOOLEAN linux_elf_is_linux(const UINT8 *img, UINTN size)
{
    const LX_EHDR *h = (const LX_EHDR *)img;

    if (size < sizeof(*h) || img[0] != 0x7F || img[1] != 'E' || img[2] != 'L' || img[3] != 'F')
        return FALSE;

    if (h->phentsize != sizeof(LX_PHDR) || h->phoff > size ||
        (UINT64)h->phnum * sizeof(LX_PHDR) > size - h->phoff)
        return FALSE;

    BOOLEAN other = (h->type == 3);         /* ET_DYN - всегда Linux */

    for (UINT16 i = 0; i < h->phnum; i++) {

        const LX_PHDR *ph = (const LX_PHDR *)(img + h->phoff + (UINTN)i * sizeof(LX_PHDR));

        if (ph->type == LPT_MYOS)
            return FALSE;

        if (ph->type != LPT_LOAD)
            other = TRUE;
    }

    return other;
}

/* Прочитать кусок файла (kfd) с места off */
static BOOLEAN kfd_pread(INTN kfd, UINT64 off, void *buf, UINTN n)
{
    UINT64 np = 0;

    if (vfs_seek(kfd, (INT64)off, 0, &np) != VFS_OK)
        return FALSE;

    UINTN got = 0;

    while (got < n) {
        INTN r = vfs_read(kfd, (UINT8 *)buf + got, n - got);
        if (r <= 0)
            return FALSE;
        got += (UINTN)r;
    }

    return TRUE;
}

/* Что загрузчик узнал о программе (для auxv) */
typedef struct {
    UINT64 entry;            /* куда прыгнуть (у динамической - в ld.so) */
    UINT64 prog_entry;       /* AT_ENTRY - начало самой программы */
    UINT64 phdr, phnum;      /* AT_PHDR, AT_PHNUM */
    UINT64 interp_base;      /* AT_BASE */
    UINT64 brk;              /* конец данных - начало кучи */
} LX_LOADINFO;

/* Права области из флагов сегмента (PF_X 1, PF_W 2, PF_R 4) */
static UINT32 seg_prot(UINT32 pf)
{
    UINT32 prot = 0;

    if (pf & 4) prot |= UVM_R;
    if (pf & 2) prot |= UVM_W | UVM_R;
    if (pf & 1) prot |= UVM_X | UVM_R;

    return prot;
}

/*
 * Загрузить один ELF (программу или интерпретатор ld.so) в память
 * процесса p. base - куда (для ET_DYN; 0 - выбрать самим). Файл
 * читается кусками прямо в страницы программы - без копии целиком.
 */
static INTN lx_load_one(KPROC *p, const char *path, BOOLEAN is_interp, LX_LOADINFO *li,
                        char *interp, UINTN interp_cap, const char **why)
{
    INTN kfd = vfs_open(path, VFS_O_READ);

    if (kfd < 0) {
        *why = "cannot open the program file";
        return kfd;
    }

    LX_EHDR h;
    LX_PHDR *ph = NULL;
    UINT8 *buf = NULL;
    INTN r = VFS_EINVAL;
    UOBJ *obj = uobj_from_fd(kfd);      /* NULL - FAT: читаем сегменты целиком */

    if (!kfd_pread(kfd, 0, &h, sizeof(h)) || h.ident[0] != 0x7F || h.ident[1] != 'E' ||
        h.ident[2] != 'L' || h.ident[3] != 'F') {
        *why = "not an ELF file";
        goto out;
    }

    if (h.ident[4] != 2 || h.ident[5] != 1 || h.machine != 62) {
        *why = "not a 64-bit x86 program";
        goto out;
    }

    if ((h.type != 2 && h.type != 3) || h.phentsize != sizeof(LX_PHDR) || h.phnum == 0 ||
        h.phnum > 64) {
        *why = "not an executable";
        goto out;
    }

    ph = (LX_PHDR *)kmalloc(sizeof(LX_PHDR) * h.phnum);
    buf = (UINT8 *)kmalloc(65536);

    if (ph == NULL || buf == NULL) {
        *why = "out of memory";
        r = VFS_ENOSPC;
        goto out;
    }

    if (!kfd_pread(kfd, h.phoff, ph, sizeof(LX_PHDR) * h.phnum)) {
        *why = "broken program headers";
        goto out;
    }

    /* размах сегментов - чтобы выбрать место для ET_DYN */
    UINT64 lo = ~0ull, hi = 0;

    for (UINT16 i = 0; i < h.phnum; i++) {

        if (ph[i].type == LPT_INTERP && !is_interp && interp != NULL) {
            UINTN n = (ph[i].filesz < interp_cap) ? (UINTN)ph[i].filesz : interp_cap - 1;
            if (!kfd_pread(kfd, ph[i].offset, interp, n)) {
                *why = "cannot read PT_INTERP";
                goto out;
            }
            interp[n] = '\0';
        }

        if (ph[i].type != LPT_LOAD || ph[i].memsz == 0)
            continue;

        if (ph[i].vaddr < lo)
            lo = ph[i].vaddr;
        if (ph[i].vaddr + ph[i].memsz > hi)
            hi = ph[i].vaddr + ph[i].memsz;
    }

    if (hi <= lo) {
        *why = "no segments to load";
        goto out;
    }

    lo &= ~0xFFFull;
    hi = (hi + 0xFFFu) & ~0xFFFull;

    UINT64 base = 0;

    if (h.type == 3) {
        base = is_interp ? uvm_find_free(p, hi - lo, 0) : LX_PIE_BASE;
        if (base == 0) {
            *why = "no room for the program";
            r = VFS_ENOSPC;
            goto out;
        }
        base -= lo;
    }

    if (base + hi > MYOS_USER_LIMIT || base + lo < 0x10000u) {
        *why = "a segment is outside the program area";
        goto out;
    }

    UINT64 top = 0;

    for (UINT16 i = 0; i < h.phnum; i++) {

        if (ph[i].type != LPT_LOAD || ph[i].memsz == 0)
            continue;

        if (ph[i].filesz > ph[i].memsz) {
            *why = "broken segment";
            goto out;
        }

        UINT64 va = base + ph[i].vaddr;
        UINT64 s = va & ~0xFFFull;
        UINT64 e = (va + ph[i].memsz + 0xFFFu) & ~0xFFFull;
        UINT32 prot = seg_prot(ph[i].flags);

        /* файл можно отобразить (ext4, tmpfs) и сегмент ни с кем не
           делит страниц: страницы файла - по требованию из кэша
           (общие у всех, кто запустил эту программу), хвост - нули */
        BOOLEAN clash = FALSE;

        for (UVMA *w = p->vmas; w != NULL; w = w->next)
            if (w->start < e && s < w->end)
                clash = TRUE;

        if (obj != NULL && !clash && ph[i].filesz > 0 &&
            (va & 0xFFFu) == (ph[i].offset & 0xFFFu)) {

            UINT64 fend = (va + ph[i].filesz + 0xFFFu) & ~0xFFFull;
            UINT64 zero_at = va + ph[i].filesz;

            if (!uvm_add_obj(p, s, fend, prot, 0, obj, ph[i].offset & ~0xFFFull) ||
                (fend < e && !uvm_add(p, fend, e, prot, 0))) {
                *why = "out of memory";
                r = VFS_ENOSPC;
                goto out;
            }

            /* последняя страница с данными файла: за filesz - .bss,
               там должны быть нули (а в файле там уже что-то другое) */
            if (ph[i].memsz > ph[i].filesz && (zero_at & 0xFFFu) != 0) {
                UINT8 *k = uvm_own_page(p, zero_at & ~0xFFFull);
                if (k == NULL) {
                    *why = "cannot load a segment";
                    r = VFS_EIO;
                    goto out;
                }
                memset(k + (zero_at & 0xFFFu), 0, 4096u - (zero_at & 0xFFFu));
            }

            if (va + ph[i].memsz > top)
                top = va + ph[i].memsz;
            continue;
        }

        /* сегменты могут делить страницу на стыке (-z noseparate-code):
           общая часть - с объединёнными правами */
        UVMA *ov = uvm_find(p, s);

        if (ov != NULL) {
            UINT64 cut = (ov->end < e) ? ov->end : e;
            uvm_protect(p, s, cut, ov->prot | prot);
            s = cut;
        }

        if (s < e && !uvm_add(p, s, e, prot, 0)) {
            *why = "out of memory";
            r = VFS_ENOSPC;
            goto out;
        }

        /* содержимое из файла - кусками по 64 КиБ */
        UINT64 done = 0;

        while (done < ph[i].filesz) {

            UINTN k = (UINTN)((ph[i].filesz - done < 65536u) ? (ph[i].filesz - done) : 65536u);

            if (!kfd_pread(kfd, ph[i].offset + done, buf, k) ||
                !uvm_copy_out(p, va + done, buf, k)) {
                *why = "cannot load a segment";
                r = VFS_EIO;
                goto out;
            }

            done += k;
        }

        if (va + ph[i].memsz > top)
            top = va + ph[i].memsz;
    }

    /* где заголовки программы в памяти (AT_PHDR) */
    UINT64 phdr = 0;

    for (UINT16 i = 0; i < h.phnum; i++) {
        if (ph[i].type == LPT_PHDR)
            phdr = base + ph[i].vaddr;
    }

    for (UINT16 i = 0; i < h.phnum && phdr == 0; i++) {
        if (ph[i].type == LPT_LOAD && h.phoff >= ph[i].offset &&
            h.phoff < ph[i].offset + ph[i].filesz)
            phdr = base + ph[i].vaddr + (h.phoff - ph[i].offset);
    }

    if (is_interp) {
        li->interp_base = base + lo;
        li->entry = base + h.entry;
    } else {
        li->prog_entry = base + h.entry;
        li->entry = li->prog_entry;
        li->phdr = phdr;
        li->phnum = h.phnum;
        li->brk = (top + 0xFFFu) & ~0xFFFull;
    }

    r = VFS_OK;

out:
    uobj_put(obj);
    kfree(ph);
    kfree(buf);
    vfs_close(kfd);
    return r;
}

/* Программа + (если динамическая) её интерпретатор ld.so */
static INTN lx_load_program(KPROC *p, const char *path, LX_LOADINFO *li, const char **why)
{
    char interp[VFS_PATH_MAX];

    interp[0] = '\0';
    memset(li, 0, sizeof(*li));

    INTN r = lx_load_one(p, path, FALSE, li, interp, sizeof(interp), why);

    if (r != VFS_OK)
        return r;

    if (interp[0] != '\0') {

        char real[VFS_PATH_MAX];

        /* путь интерпретатора - как видит его программа Linux
           (/lib64/ld-linux-x86-64.so.2 -> ссылка lib64 -> usr/lib):
           ищем в корне Linux, проходя ссылки */
        if (lx_resolve_kpath(p, LX_AT_FDCWD, interp, TRUE, real, sizeof(real)) < 0)
            ksnprintf(real, sizeof(real), "%s", interp);
        r = lx_load_one(p, real, TRUE, li, NULL, 0, why);

        if (r != VFS_OK) {
            *why = "dynamic program: its loader (ld.so) is not found";
            return r;
        }
    }

    return VFS_OK;
}

/*
 * Стек новой программы - как его строит Linux:
 *   [rsp]  argc
 *          argv[0..argc-1], NULL
 *          envp[...], NULL
 *          auxv: пары (тип, значение), AT_NULL
 *   выше   16 случайных байт, строки
 */
#define AT_NULL     0
#define AT_PHDR     3
#define AT_PHENT    4
#define AT_PHNUM    5
#define AT_PAGESZ   6
#define AT_BASE     7
#define AT_FLAGS    8
#define AT_ENTRY    9
#define AT_UID      11
#define AT_EUID     12
#define AT_GID      13
#define AT_EGID     14
#define AT_PLATFORM 15
#define AT_HWCAP    16
#define AT_CLKTCK   17
#define AT_SECURE   23
#define AT_RANDOM   25
#define AT_HWCAP2   26
#define AT_EXECFN   31

static INTN lx_build_stack(KPROC *p, const LX_LOADINFO *li, const char *strs, UINTN strs_len,
                           UINTN argc, UINTN envc, const char *execfn, UINT64 *sp_out)
{
    if (!uvm_add(p, LX_STACK_TOP - LX_STACK_SIZE, LX_STACK_TOP, UVM_R | UVM_W, UVM_STACK))
        return VFS_ENOSPC;

    /* строки: "x86_64", имя файла, argv, envp - одним куском сверху */
    UINT64 at = LX_STACK_TOP - 16u;
    UINT8 rnd[16];

    krandom_fill(rnd, sizeof(rnd));
    at -= 16;
    UINT64 rnd_at = at;

    if (!uvm_copy_out(p, rnd_at, rnd, 16))
        return VFS_ENOSPC;

    at -= 8;
    UINT64 plat_at = at;

    if (!uvm_copy_out(p, plat_at, "x86_64", 7))
        return VFS_ENOSPC;

    UINTN fnl = 0;
    while (execfn[fnl])
        fnl++;

    at -= fnl + 1;
    UINT64 fn_at = at;

    if (!uvm_copy_out(p, fn_at, execfn, fnl + 1))
        return VFS_ENOSPC;

    at -= strs_len;
    at &= ~0xFull;
    UINT64 strs_at = at;

    if (!uvm_copy_out(p, strs_at, strs, strs_len))
        return VFS_ENOSPC;

    /* таблица указателей */
    UINT32 a, b, c, d;
    __asm__ __volatile__("cpuid" : "=a"(a), "=b"(b), "=c"(c), "=d"(d) : "a"(1), "c"(0));

    UINT64 aux[][2] = {
        { AT_PHDR,   li->phdr },
        { AT_PHENT,  sizeof(LX_PHDR) },
        { AT_PHNUM,  li->phnum },
        { AT_PAGESZ, 4096 },
        { AT_BASE,   li->interp_base },
        { AT_FLAGS,  0 },
        { AT_ENTRY,  li->prog_entry },
        { AT_UID,    0 }, { AT_EUID, 0 }, { AT_GID, 0 }, { AT_EGID, 0 },
        { AT_HWCAP,  d },
        { AT_HWCAP2, 0 },
        { AT_CLKTCK, 100 },
        { AT_SECURE, 0 },
        { AT_RANDOM, rnd_at },
        { AT_PLATFORM, plat_at },
        { AT_EXECFN, fn_at },
        { AT_NULL,   0 },
    };
    UINTN naux = sizeof(aux) / sizeof(aux[0]);
    UINTN words = 1u + argc + 1u + envc + 1u + naux * 2u;
    UINT64 *vec = (UINT64 *)kmalloc(words * 8u);

    if (vec == NULL)
        return VFS_ENOSPC;

    UINTN k = 0, off = 0;

    vec[k++] = argc;

    for (UINTN i = 0; i < argc + envc; i++) {

        vec[k++] = strs_at + off;

        while (strs[off])
            off++;
        off++;

        if (i + 1 == argc)
            vec[k++] = 0;
    }

    if (argc == 0)
        vec[k++] = 0;

    vec[k++] = 0;

    for (UINTN i = 0; i < naux; i++) {
        vec[k++] = aux[i][0];
        vec[k++] = aux[i][1];
    }

    UINT64 sp = (at - k * 8u) & ~0xFull;
    BOOLEAN ok = uvm_copy_out(p, sp, vec, k * 8u);

    kfree(vec);

    if (!ok)
        return VFS_ENOSPC;

    *sp_out = sp;
    return VFS_OK;
}

/* Начальное состояние "Linux" процесса (до загрузки программы) */
static BOOLEAN lx_proc_init(KPROC *p)
{
    if (p->lx == NULL) {
        p->lx = (LXPROC *)kzalloc(sizeof(LXPROC));
        if (p->lx == NULL)
            return FALSE;
    }

    p->is_linux = TRUE;
    p->mmap_top = LX_MMAP_TOP;
    p->lx->umask = 022;
    p->lx->exit_signal = LX_SIGCHLD;

    fx_default_init();
    return TRUE;
}

/* Разбить строку аргументов шелла MyOS на слова (кавычки "...") ->
   подряд идущие строки с нулями */
static UINTN split_args(const char *name, const char *args, char *out, UINTN cap, UINTN *len)
{
    UINTN n = 0, argc = 0;

    for (UINTN i = 0; name[i] && n + 1 < cap; i++)
        out[n++] = name[i];
    out[n++] = '\0';
    argc++;

    const char *a = args ? args : "";

    while (*a && n + 2 < cap) {

        while (*a == ' ')
            a++;

        if (!*a)
            break;

        if (*a == '"') {
            a++;
            while (*a && *a != '"' && n + 2 < cap)
                out[n++] = *a++;
            if (*a == '"')
                a++;
        } else {
            while (*a && *a != ' ' && n + 2 < cap)
                out[n++] = *a++;
        }

        out[n++] = '\0';
        argc++;
    }

    *len = n;
    return argc;
}

/* Окружение по умолчанию для программы, запущенной из шелла MyOS */
static UINTN default_env(KPROC *p, char *out, UINTN cap, UINTN *len)
{
    char line[VFS_PATH_MAX + 8];
    /* домашняя папка: при корне Linux - в /tmp (раздел Linux только
       для чтения, а программы пишут в ~ настройки и историю) */
    BOOLEAN has_root = p->lx != NULL && p->lx->root[0] != '\0';
    const char *const envs[] = {
        "PATH=/usr/local/bin:/usr/bin:/bin:/usr/sbin:/sbin",
        has_root ? "HOME=/tmp/root" : "HOME=/ram",
        "LANG=C.UTF-8",
        "USER=root",
        "LOGNAME=root",
        "SHELL=/bin/sh",
        NULL
    };
    UINTN n = *len, envc = 0;

    for (UINTN i = 0; envs[i]; i++) {
        for (UINTN k = 0; envs[i][k] && n + 1 < cap; k++)
            out[n++] = envs[i][k];
        out[n++] = '\0';
        envc++;
    }

    /* окно-терминал понимает ESC-последовательности VT100/xterm (цвета,
       курсор), текстовая консоль ядра - нет */
    const char *term = (p->io == PROC_IO_TTY) ? "TERM=xterm" : "TERM=dumb";

    for (UINTN k = 0; term[k] && n + 1 < cap; k++)
        out[n++] = term[k];
    out[n++] = '\0';
    envc++;

    {
        char lcwd[VFS_PATH_MAX];
        lx_path_to_linux(p, p->cwd, lcwd, sizeof(lcwd));
        ksnprintf(line, sizeof(line), "PWD=%s", lcwd);
    }

    for (UINTN k = 0; line[k] && n + 1 < cap; k++)
        out[n++] = line[k];
    out[n++] = '\0';
    envc++;

    *len = n;
    return envc;
}

/*
 * Запуск программы Linux из шелла MyOS (proc_spawn_ex): загрузить,
 * построить стек; поток программы войдёт в неё как обычно
 * (proc_thread_main -> p->entry, p->user_rsp).
 */
INTN linux_proc_start(KPROC *p, const char *path, const char *args, const char **why)
{
    if (!lx_proc_init(p)) {
        *why = "out of memory";
        return VFS_ENOSPC;
    }

    p->lx->pgid = p->pid;
    p->lx->sid = p->pid;
    ksnprintf(p->lx->exe, sizeof(p->lx->exe), "%s", path);
    lx_choose_root(p);
    lx_files_init(p);

    if (p->lx->root[0] != '\0') {
        vfs_mkdir("/tmp/root");             /* HOME (см. default_env) */
        klog("linux: pid %u: Linux root is /%s\n", p->pid, p->lx->root);
    }

    /* путь программы, как его видит она сама (argv[0], AT_EXECFN) */
    char lpath[VFS_PATH_MAX];
    lx_path_to_linux(p, path, lpath, sizeof(lpath));

    LX_LOADINFO li;
    INTN r = lx_load_program(p, path, &li, why);

    if (r != VFS_OK)
        return r;

    p->brk_base = p->brk = li.brk;

    char *strs = (char *)kmalloc(8192);

    if (strs == NULL) {
        *why = "out of memory";
        return VFS_ENOSPC;
    }

    UINTN len = 0;
    UINTN argc = split_args(p->name, args, strs, 4096, &len);

    /* argv[0] - как набрали (путь), а не короткое имя */
    {
        char tmp[8192];
        UINTN n0 = 0;
        while (strs[n0])
            n0++;
        UINTN pl = 0;
        while (lpath[pl])
            pl++;
        if (pl + 1 + (len - n0 - 1) < sizeof(tmp)) {
            memcpy(tmp, lpath, pl + 1);
            memcpy(tmp + pl + 1, strs + n0 + 1, len - n0 - 1);
            len = pl + 1 + (len - n0 - 1);
            memcpy(strs, tmp, len);
        }
    }

    UINTN envc = default_env(p, strs, 8192, &len);
    UINT64 sp = 0;

    r = lx_build_stack(p, &li, strs, len, argc, envc, lpath, &sp);
    kfree(strs);

    if (r != VFS_OK) {
        *why = "out of memory for the stack";
        return r;
    }

    p->entry = li.entry;
    p->user_rsp = sp;

    klog("linux: pid %u '%s' - Linux program, entry 0x%llx%s\n", p->pid, path, li.entry,
         li.interp_base ? " (dynamic)" : "");
    return VFS_OK;
}

void linux_proc_free(KPROC *p)
{
    if (p->lx == NULL)
        return;

    lx_files_free(p);
    kfree(p->lx);
    p->lx = NULL;
    p->is_linux = FALSE;
}


/* ================================================================
 * Память: mmap, munmap, mprotect, brk, mremap, madvise
 * ================================================================ */

static UINT32 prot_from_linux(UINT64 prot)
{
    UINT32 r = 0;

    if (prot & LX_PROT_READ)  r |= UVM_R;
    if (prot & LX_PROT_WRITE) r |= UVM_W | UVM_R;
    if (prot & LX_PROT_EXEC)  r |= UVM_X | UVM_R;

    return r;
}

static INT64 sys_mmap(KPROC *p, UINT64 addr, UINT64 len, UINT64 prot, UINT64 flags,
                      INT64 fd, UINT64 off)
{
    if (len == 0 || (off & 0xFFFu))
        return -LX_EINVAL;

    len = (len + 0xFFFu) & ~0xFFFull;

    if (len > MYOS_USER_LIMIT)
        return -LX_ENOMEM;

    LFILE *f = NULL;

    if (!(flags & LX_MAP_ANONYMOUS)) {
        f = lx_fd_get(p, fd);
        if (f == NULL)
            return -LX_EBADF;
        if (f->type != LF_VFS && f->type != LF_MEM && f->type != LF_ZERO)
            return -LX_ENODEV;
    }

    UINT64 at;

    if (flags & (LX_MAP_FIXED | LX_MAP_FIXED_NOREPLACE)) {

        if ((addr & 0xFFFu) || addr < 0x1000u || addr + len > MYOS_USER_LIMIT || addr + len < addr)
            return -LX_EINVAL;

        if (flags & LX_MAP_FIXED_NOREPLACE) {
            for (UVMA *v = p->vmas; v != NULL; v = v->next)
                if (v->start < addr + len && addr < v->end)
                    return -LX_EEXIST;
        } else {
            uvm_unmap(p, addr, addr + len);
        }

        at = addr;

    } else {

        at = uvm_find_free(p, len, addr & ~0xFFFull);

        if (at == 0)
            return -LX_ENOMEM;
    }

    BOOLEAN shared = (flags & LX_MAP_SHARED) != 0;

    /* что будет в области: файл ext4 / tmpfs - объект (страницы по
       требованию, общие с другими процессами, pcache.c); общая
       анонимная память - безымянный узел tmpfs (после fork у родителя
       и потомка - одни страницы); остальное - как раньше */
    UOBJ *obj = NULL;

    if (f != NULL && f->type == LF_VFS) {
        obj = uobj_from_fd(f->kfd);
        /* файл раздела Linux - только чтение: писать через общую
           область некуда */
        if (obj != NULL && obj->kind == UOBJ_FILE && shared && (prot & LX_PROT_WRITE)) {
            uobj_put(obj);
            return -LX_EACCES;
        }
    } else if (f == NULL && shared) {
        obj = uobj_anon(len);
        if (obj == NULL)
            return -LX_ENOMEM;
    }

    UINT32 vflags = (shared && (f == NULL || obj != NULL)) ? UVM_SHARED : 0;
    BOOLEAN added = uvm_add_obj(p, at, at + len, prot_from_linux(prot), vflags, obj,
                                (f == NULL) ? 0 : off);

    uobj_put(obj);                      /* у области - своя ссылка */

    if (!added)
        return -LX_ENOMEM;

    /* файл без объекта (FAT, exFAT, /proc): содержимое сразу */
    if (f != NULL && f->type != LF_ZERO && obj == NULL) {

        UINT8 *buf = (UINT8 *)kmalloc(65536);

        if (buf == NULL) {
            uvm_unmap(p, at, at + len);
            return -LX_ENOMEM;
        }

        UINT64 done = 0;

        while (done < len) {

            UINTN k = (UINTN)((len - done < 65536u) ? (len - done) : 65536u);
            INTN got = lx_file_read_kernel(f, buf, k, off + done);

            if (got <= 0)
                break;

            /* PROT_NONE + файл бывает у ld.so как "резерв" - тогда
               страницы появятся позже другим mmap */
            if (!uvm_copy_out(p, at + done, buf, (UINT64)got))
                break;

            done += (UINT64)got;

            if ((UINTN)got < k)
                break;
        }

        kfree(buf);
    }

    return (INT64)at;
}

static INT64 sys_brk(KPROC *p, UINT64 want)
{
    if (want < p->brk_base || want > p->brk_base + (64ull << 30))
        return (INT64)p->brk;

    UINT64 old_end = (p->brk + 0xFFFu) & ~0xFFFull;
    UINT64 new_end = (want + 0xFFFu) & ~0xFFFull;

    if (new_end > old_end) {
        for (UVMA *v = p->vmas; v != NULL; v = v->next)
            if (v->start < new_end && old_end < v->end)
                return (INT64)p->brk;           /* впереди занято */
        if (!uvm_add(p, old_end, new_end, UVM_R | UVM_W, UVM_HEAP))
            return (INT64)p->brk;
    } else if (new_end < old_end) {
        uvm_unmap(p, new_end, old_end);
    }

    p->brk = want;
    return (INT64)want;
}

static INT64 sys_mremap(KPROC *p, UINT64 old, UINT64 olen, UINT64 nlen, UINT64 flags, UINT64 naddr)
{
    (void)naddr;

    if ((old & 0xFFFu) || nlen == 0)
        return -LX_EINVAL;

    olen = (olen + 0xFFFu) & ~0xFFFull;
    nlen = (nlen + 0xFFFu) & ~0xFFFull;

    UVMA *v = uvm_find(p, old);

    if (v == NULL)
        return -LX_EFAULT;

    if (flags & LX_MREMAP_FIXED)
        return -LX_EINVAL;

    if (nlen <= olen) {
        if (nlen < olen)
            uvm_unmap(p, old + nlen, old + olen);
        return (INT64)old;
    }

    /* вырасти на месте */
    BOOLEAN free_after = TRUE;

    for (UVMA *w = p->vmas; w != NULL; w = w->next)
        if (w->start < old + nlen && old + olen < w->end)
            free_after = FALSE;

    if (free_after && old + nlen <= MYOS_USER_LIMIT) {
        if (!uvm_add_obj(p, old + olen, old + nlen, v->prot, v->flags, v->obj,
                         v->off + (old - v->start) + olen))
            return -LX_ENOMEM;
        return (INT64)old;
    }

    if (!(flags & LX_MREMAP_MAYMOVE))
        return -LX_ENOMEM;

    /* переехать: новые адреса, те же физические страницы */
    UINT64 at = uvm_find_free(p, nlen, 0);

    if (at == 0 || !uvm_add_obj(p, at, at + nlen, v->prot, v->flags, v->obj,
                                v->off + (old - v->start)))
        return -LX_ENOMEM;

    for (UINT64 o = 0; o < olen; o += 4096u) {

        UINT64 *e = uvm_pte(p->pml4, old + o, FALSE);

        if (e == NULL || !(*e & UPTE_P))
            continue;

        UINT64 *ne = uvm_pte(p->pml4, at + o, TRUE);

        if (ne == NULL)
            break;

        *ne = *e;
        *e = 0;
    }

    uvm_unmap(p, old, old + olen);      /* страниц там уже нет - только области */
    return (INT64)at;
}

/* MADV_DONTNEED: страницы - прочь, следующее касание даст нули */
static void madv_dontneed(KPROC *p, UINT64 s, UINT64 e)
{
    BOOLEAN any = FALSE;

    for (UINT64 va = s; va < e; va += 4096u) {

        UVMA *v = uvm_find(p, va);

        if (v == NULL || (v->flags & UVM_SHARED))
            continue;

        UINT64 *pte = uvm_pte(p->pml4, va, FALSE);

        if (pte == NULL || !(*pte & UPTE_P) || (*pte & UPTE_SHARED))
            continue;

        pmm_page_unref(*pte & UPTE_ADDR);
        *pte = 0;
        if (p->pages > 0)
            p->pages--;
        any = TRUE;
    }

    if (any)
        uvm_tlb_shootdown(p);
}


/* ================================================================
 * futex
 * ================================================================ */

/* Ключ futex - физический адрес слова: одинаков у всех, кто видит
   эту память (потоки процесса; процессы с общей памятью) */
static const void *futex_key(KPROC *p, UINT64 uaddr)
{
    if ((uaddr & 3u) || !uptr_ok(p, uaddr, 4, FALSE))
        return NULL;

    UINT64 *e = uvm_pte(p->pml4, uaddr & ~0xFFFull, FALSE);

    if (e == NULL || !(*e & UPTE_P))
        return NULL;

    return (const void *)(UINTN)((*e & UPTE_ADDR) | (uaddr & 0xFFFu));
}

static INT64 futex_wake(const void *key, UINT64 n)
{
    INT64 woke = 0;

    while ((UINT64)woke < n && sched_wake_one(key))
        woke++;

    return woke;
}

/* Ждать на key, пока не разбудят, не выйдет срок (deadline_ns, 0 -
   без срока; abs_rt - срок по CLOCK_REALTIME) или не придёт сигнал */
static INT64 futex_wait(KPROC *p, UINT64 uaddr, UINT32 val, UINT64 deadline_ns, BOOLEAN abs_rt)
{
    const void *key = futex_key(p, uaddr);

    if (key == NULL)
        return -LX_EFAULT;

    UINT64 fl = kx_irq_save();

    if (*(volatile UINT32 *)(UINTN)uaddr != val) {
        kx_irq_restore(fl);
        return -LX_EAGAIN;
    }

    for (;;) {

        UINT64 slice = 100;

        if (deadline_ns != 0) {
            UINT64 now = lx_now_ns(abs_rt);
            if (now >= deadline_ns) {
                kx_irq_restore(fl);
                return -LX_ETIMEDOUT;
            }
            UINT64 left = (deadline_ns - now + 999999u) / 1000000u;
            if (left < slice)
                slice = left ? left : 1;
        }

        if (sched_block(key, "futex", slice)) {
            kx_irq_restore(fl);
            return 0;                   /* разбудили (или сигнал - проверим ниже) */
        }

        if (p->killed || lx_signal_pending()) {
            kx_irq_restore(fl);
            return -LX_EINTR;
        }
    }
}

/* Перевесить ждущих с key1 на key2 (FUTEX_REQUEUE) */
static INT64 futex_requeue(const void *k1, const void *k2, UINT64 n)
{
    INT64 moved = 0;

    for (UINTN i = 0; i < KT_MAX && (UINT64)moved < n; i++) {
        KTHREAD *t = &g_kthreads[i];
        if (t->state == KT_BLOCKED && t->wait_on == k1) {
            t->wait_on = k2;
            moved++;
        }
    }

    return moved;
}

static INT64 sys_futex(KPROC *p, UINT64 uaddr, UINT64 op, UINT64 val, UINT64 utime,
                       UINT64 uaddr2, UINT64 val3)
{
    UINT32 cmd = (UINT32)op & 0x7Fu;
    BOOLEAN rt = (op & LX_FUTEX_CLOCK_REALTIME) != 0;

    switch (cmd) {

    case LX_FUTEX_WAIT:
    case LX_FUTEX_WAIT_BITSET: {
        UINT64 deadline = 0;
        if (utime != 0) {
            if (!uptr_ok(p, utime, sizeof(LX_TIMESPEC), FALSE))
                return -LX_EFAULT;
            LX_TIMESPEC ts;
            memcpy(&ts, (const void *)(UINTN)utime, sizeof(ts));
            if (ts.tv_sec < 0 || ts.tv_nsec < 0 || ts.tv_nsec >= 1000000000)
                return -LX_EINVAL;
            UINT64 ns = (UINT64)ts.tv_sec * 1000000000ull + (UINT64)ts.tv_nsec;
            if (cmd == LX_FUTEX_WAIT) {
                deadline = lx_now_ns(FALSE) + ns;      /* относительный срок */
                rt = FALSE;
            } else {
                deadline = ns ? ns : 1;                /* абсолютный */
            }
        }
        return futex_wait(p, uaddr, (UINT32)val, deadline, rt);
    }

    case LX_FUTEX_WAKE:
    case LX_FUTEX_WAKE_BITSET: {
        const void *key = futex_key(p, uaddr);
        if (key == NULL)
            return -LX_EFAULT;
        return futex_wake(key, val);
    }

    case LX_FUTEX_REQUEUE:
    case LX_FUTEX_CMP_REQUEUE: {
        const void *k1 = futex_key(p, uaddr), *k2 = futex_key(p, uaddr2);
        if (k1 == NULL || k2 == NULL)
            return -LX_EFAULT;
        if (cmd == LX_FUTEX_CMP_REQUEUE && *(volatile UINT32 *)(UINTN)uaddr != (UINT32)val3)
            return -LX_EAGAIN;
        INT64 w = futex_wake(k1, val);
        return w + futex_requeue(k1, k2, utime);    /* utime здесь - val2 */
    }

    case LX_FUTEX_WAKE_OP: {
        const void *k1 = futex_key(p, uaddr), *k2 = futex_key(p, uaddr2);
        if (k1 == NULL || k2 == NULL || !uptr_ok(p, uaddr2, 4, TRUE))
            return -LX_EFAULT;
        UINT32 opk = ((UINT32)val3 >> 28) & 7u, cmp = ((UINT32)val3 >> 24) & 15u;
        INT32 oparg = (INT32)(((UINT32)val3 >> 12) & 0xFFFu);
        INT32 cmparg = (INT32)((UINT32)val3 & 0xFFFu);
        if (oparg & 0x800) oparg |= ~0xFFF;
        if (cmparg & 0x800) cmparg |= ~0xFFF;
        if ((val3 >> 28) & 8u)
            oparg = 1 << (oparg & 31);
        volatile INT32 *w2 = (volatile INT32 *)(UINTN)uaddr2;
        INT32 old = *w2;
        switch (opk) {
        case 0: *w2 = oparg; break;
        case 1: *w2 = old + oparg; break;
        case 2: *w2 = old | oparg; break;
        case 3: *w2 = old & ~oparg; break;
        case 4: *w2 = old ^ oparg; break;
        }
        INT64 n = futex_wake(k1, val);
        BOOLEAN c = (cmp == 0) ? old == cmparg : (cmp == 1) ? old != cmparg :
                    (cmp == 2) ? old < cmparg : (cmp == 3) ? old <= cmparg :
                    (cmp == 4) ? old > cmparg : old >= cmparg;
        if (c)
            n += futex_wake(k2, utime);
        return n;
    }
    }

    return -LX_ENOSYS;
}


/* ================================================================
 * Потоки и процессы: clone, fork, execve, exit, wait4
 * ================================================================ */

/* Что нужно новому потоку, чтобы войти в программу */
typedef struct {
    UINT8   fx[512] __attribute__((aligned(16)));
    LX_REGS regs;
    KPROC  *p;
    UINT64  fs_base;
    UINT32  tid;
    UINT64  clear_tid;
    UINT64  sig_mask;
} LX_START;

static void lx_thread_entry(void *arg)
{
    LX_START *s = (LX_START *)arg;
    LX_REGS r = s->regs;
    UINT8 fx[512] __attribute__((aligned(16)));

    memcpy(fx, s->fx, sizeof(fx));

    UINT64 fl = kx_irq_save();
    KTHREAD *t = g_kcur;

    t->proc = s->p;
    t->cr3 = s->p->pml4;
    t->fs_base = s->fs_base;
    t->lx_tid = s->tid;
    t->clear_tid = s->clear_tid;
    t->sig_mask = s->sig_mask;
    proc_switch_hook(t);

    kx_irq_restore(fl);
    kfree(s);

    lx_enter(&r, fx);
}

/* Новый поток ядра для программы: регистры r, fx - состояние SSE */
static KTHREAD *lx_spawn_thread(KPROC *p, const LX_REGS *r, const void *fx, UINT64 fs,
                                UINT32 tid, UINT64 clear_tid, UINT64 mask)
{
    LX_START *s = (LX_START *)kmalloc(sizeof(LX_START) + 16u);

    if (s == NULL)
        return NULL;

    /* fx внутри должен быть выровнен на 16 - kmalloc выравнивает на 16 */
    memcpy(s->fx, fx, 512);
    s->regs = *r;
    s->p = p;
    s->fs_base = fs;
    s->tid = tid;
    s->clear_tid = clear_tid;
    s->sig_mask = mask;

    KTHREAD *t = kthread_create(p->name, lx_thread_entry, s, 16);

    if (t == NULL) {
        kfree(s);
        return NULL;
    }

    /* поток уже принадлежит процессу (см. proc_spawn_ex) */
    t->proc = p;
    t->cr3 = p->pml4;
    t->lx_tid = tid;
    return t;
}

/* Записать слово в память другого процесса (CLONE_CHILD_SETTID у
   потомка после fork: его страница - общая, нужна своя копия) */
static void write_u32_in(KPROC *q, UINT64 va, UINT32 v)
{
    if (va == 0 || (va & 3u) || !uvm_fault(q, va, TRUE)) {
        UINT64 *e0 = uvm_pte(q->pml4, va & ~0xFFFull, FALSE);
        if (e0 == NULL || !(*e0 & UPTE_P) || !(*e0 & UPTE_W))
            return;
    }

    UINT64 *e = uvm_pte(q->pml4, va & ~0xFFFull, FALSE);

    if (e != NULL && (*e & UPTE_P) && (*e & UPTE_W))
        *(volatile UINT32 *)((UINT8 *)P2V(*e & UPTE_ADDR) + (va & 0xFFFu)) = v;
}

static INT64 sys_clone(KPROC *p, UINT64 *f, UINT64 flags, UINT64 newsp, UINT64 ptid,
                       UINT64 ctid, UINT64 tls)
{
    KTHREAD *me = g_kcur;
    LX_REGS r;

    regs_from_syscall(f, &r, 0);

    if (newsp != 0)
        r.rsp = newsp;

    const void *fx = frame_fx(f);

    if (flags & LX_CLONE_THREAD) {

        /* поток: та же память, те же файлы и обработчики */
        if (!(flags & LX_CLONE_VM) || !(flags & LX_CLONE_SIGHAND))
            return -LX_EINVAL;

        UINT32 tid = proc_next_pid();

        if ((flags & LX_CLONE_PARENT_SETTID) && uptr_ok(p, ptid, 4, TRUE))
            *(volatile UINT32 *)(UINTN)ptid = tid;
        if ((flags & LX_CLONE_CHILD_SETTID) && uptr_ok(p, ctid, 4, TRUE))
            *(volatile UINT32 *)(UINTN)ctid = tid;

        KTHREAD *t = lx_spawn_thread(p, &r, fx,
                                     (flags & LX_CLONE_SETTLS) ? tls : me->fs_base, tid,
                                     (flags & LX_CLONE_CHILD_CLEARTID) ? ctid : 0, me->sig_mask);

        return (t == NULL) ? -LX_EAGAIN : (INT64)tid;
    }

    /* новый процесс (fork; vfork и CLONE_VM без потока - тоже копия
       памяти, а родитель при CLONE_VFORK ждёт) */
    KPROC *c = proc_alloc_slot();

    if (c == NULL)
        return -LX_EAGAIN;

    ksnprintf(c->name, sizeof(c->name), "%s", p->name);
    ksnprintf(c->path, sizeof(c->path), "%s", p->path);
    ksnprintf(c->cwd, sizeof(c->cwd), "%s", p->cwd);
    c->io = p->io;
    c->parent = p;
    c->raw_keys = p->raw_keys;

    if (p->tty != NULL) {
        c->tty = p->tty;
        tty_ref(c->tty);
    }

    c->pml4 = proc_new_pml4();

    if (c->pml4 == 0 || !lx_proc_init(c)) {
        proc_reap(c);
        return -LX_ENOMEM;
    }

    /* "Linux"-часть: обработчики сигналов, группа, файлы */
    memcpy(c->lx->act, p->lx->act, sizeof(c->lx->act));
    c->lx->umask = p->lx->umask;
    c->lx->pgid = p->lx->pgid;
    c->lx->sid = p->lx->sid;
    c->lx->exit_signal = (INT32)(flags & 0xFFu);
    ksnprintf(c->lx->exe, sizeof(c->lx->exe), "%s", p->lx->exe);
    ksnprintf(c->lx->root, sizeof(c->lx->root), "%s", p->lx->root);
    lx_files_fork(p, c);

    if (!uvm_fork(p, c)) {
        proc_reap(c);
        return -LX_ENOMEM;
    }

    c->pages = p->pages;

    if (flags & LX_CLONE_CHILD_SETTID)
        write_u32_in(c, ctid, c->pid);
    if ((flags & LX_CLONE_PARENT_SETTID) && uptr_ok(p, ptid, 4, TRUE))
        *(volatile UINT32 *)(UINTN)ptid = c->pid;

    if (flags & LX_CLONE_VFORK)
        c->lx->vfork_wait = TRUE;

    KTHREAD *t = lx_spawn_thread(c, &r, fx, (flags & LX_CLONE_SETTLS) ? tls : me->fs_base,
                                 c->pid, (flags & LX_CLONE_CHILD_CLEARTID) ? ctid : 0,
                                 me->sig_mask);

    if (t == NULL) {
        proc_reap(c);
        return -LX_EAGAIN;
    }

    c->thread = t;
    c->tid = t->tid;

    klog("linux: pid %u forked pid %u\n", p->pid, c->pid);

    /* vfork: ждать, пока потомок не сделает execve или не завершится */
    if (flags & LX_CLONE_VFORK) {
        UINT64 fl = kx_irq_save();
        while (c->used && c->lx != NULL && c->lx->vfork_wait && proc_alive(c))
            sched_block(c->lx, "vfork", 100);
        kx_irq_restore(fl);
    }

    return c->pid;
}

/* Поток программы Linux уходит: CLONE_CHILD_CLEARTID - записать 0 и
   разбудить ждущего (так pthread_join узнаёт о конце потока) */
void linux_thread_gone(KTHREAD *t)
{
    KPROC *p = t->proc;

    if (p == NULL || t->clear_tid == 0)
        return;

    UINT64 a = t->clear_tid;
    t->clear_tid = 0;

    if (uptr_ok(p, a, 4, TRUE)) {
        *(volatile UINT32 *)(UINTN)a = 0;
        const void *key = futex_key(p, a);
        if (key != NULL)
            futex_wake(key, 1);
    }
}

/* Процесс Linux закончился (последний поток, proc.c) */
void linux_proc_exited(KPROC *p, INT64 code)
{
    if (p->lx == NULL)
        return;

    if (!p->lx->stopped_by_signal)
        p->lx->wstatus = (INT32)((code & 0xFF) << 8);

    /* родитель ждал vfork - отпустить */
    if (p->lx->vfork_wait) {
        p->lx->vfork_wait = FALSE;
        sched_wake_all(p->lx);
    }

    /* файлы закрыть сразу: читающий конец канала должен увидеть "конец" */
    lx_files_free(p);

    KPROC *par = p->parent;

    if (par != NULL && par->is_linux && p->lx->exit_signal > 0)
        lx_signal_send(par, (UINT32)p->lx->exit_signal);

    if (par != NULL)
        sched_wake_all(par);
}

/* exit (только этот поток) */
static void __attribute__((noreturn)) lx_exit_thread(KPROC *p, INT64 code)
{
    (void)p;
    proc_exit_current(code);
}

/* exit_group: весь процесс */
static void __attribute__((noreturn)) lx_exit_group(KPROC *p, INT64 code)
{
    if (!p->group_exit) {
        p->group_exit = TRUE;
        p->exit_code = code;
    }

    p->killed = TRUE;

    for (UINTN i = 0; i < KT_MAX; i++) {
        KTHREAD *t = &g_kthreads[i];
        if (t != g_kcur && t->proc == p)
            sched_wake_thread(t);
    }

    proc_exit_current(code);
}

/* Ребёнок подходит под pid из wait4 */
static BOOLEAN wait_match(KPROC *p, KPROC *c, INT64 pid)
{
    if (!c->used || c->parent != p)
        return FALSE;

    if (pid > 0)
        return (INT64)c->pid == pid;
    if (pid == -1)
        return TRUE;

    UINT32 pg = (pid == 0) ? p->lx->pgid : (UINT32)(-pid);
    return c->lx != NULL && c->lx->pgid == pg;
}

static INT32 child_status(KPROC *c)
{
    if (c->lx != NULL)
        return c->lx->wstatus;

    /* программа MyOS: -1 - её остановили */
    return (c->exit_code < 0) ? LX_SIGKILL : (INT32)((c->exit_code & 0xFF) << 8);
}

static INT64 sys_wait4(KPROC *p, INT64 pid, UINT64 ustatus, UINT64 options, UINT64 urusage)
{
    if (ustatus && !uptr_ok(p, ustatus, 4, TRUE))
        return -LX_EFAULT;

    if (urusage && uptr_ok(p, urusage, 144, TRUE))
        memset((void *)(UINTN)urusage, 0, 144);

    UINT64 fl = kx_irq_save();

    for (;;) {

        BOOLEAN any = FALSE;

        for (UINTN i = 0; i < PROC_MAX; i++) {

            KPROC *c = &g_procs[i];

            if (!wait_match(p, c, pid))
                continue;

            any = TRUE;

            if (proc_alive(c) || !c->exited)
                continue;

            INT32 st = child_status(c);
            INT64 cpid = c->pid;

            kx_irq_restore(fl);

            if (ustatus)
                *(volatile INT32 *)(UINTN)ustatus = st;

            proc_reap(c);
            return cpid;
        }

        if (!any) {
            kx_irq_restore(fl);
            return -LX_ECHILD;
        }

        if (options & LX_WNOHANG) {
            kx_irq_restore(fl);
            return 0;
        }

        if (p->killed || lx_signal_pending()) {
            kx_irq_restore(fl);
            return -LX_ERESTARTSYS;
        }

        sched_block(p, "wait4", 100);
    }
}

/* waitid: то же, ответ - в siginfo */
static INT64 sys_waitid(KPROC *p, UINT64 idtype, UINT64 id, UINT64 uinfo, UINT64 options)
{
    INT64 pid = (idtype == 0) ? -1 : (idtype == 1) ? (INT64)id : (idtype == 2) ? -(INT64)id : -2;

    if (pid == -2)
        return -LX_EINVAL;

    if (idtype == 2 && id == 0)
        pid = 0;

    UINT64 fl = kx_irq_save();

    for (;;) {

        BOOLEAN any = FALSE;

        for (UINTN i = 0; i < PROC_MAX; i++) {

            KPROC *c = &g_procs[i];

            if (!wait_match(p, c, pid))
                continue;

            any = TRUE;

            if (proc_alive(c) || !c->exited)
                continue;

            INT32 st = child_status(c);
            UINT32 cpid = c->pid;

            kx_irq_restore(fl);

            if (uinfo && uptr_ok(p, uinfo, 128, TRUE)) {
                INT32 *si = (INT32 *)(UINTN)uinfo;
                memset(si, 0, 128);
                si[0] = LX_SIGCHLD;
                si[2] = ((st & 0x7F) == 0) ? 1 : 2;     /* CLD_EXITED / CLD_KILLED */
                si[4] = (INT32)cpid;
                si[5] = 0;
                si[6] = ((st & 0x7F) == 0) ? ((st >> 8) & 0xFF) : (st & 0x7F);
            }

            if (!(options & 0x01000000u))           /* WNOWAIT - не убирать */
                proc_reap(c);
            return 0;
        }

        if (!any) {
            kx_irq_restore(fl);
            return -LX_ECHILD;
        }

        if (options & LX_WNOHANG) {
            kx_irq_restore(fl);
            if (uinfo && uptr_ok(p, uinfo, 128, TRUE))
                memset((void *)(UINTN)uinfo, 0, 128);
            return 0;
        }

        if (p->killed || lx_signal_pending()) {
            kx_irq_restore(fl);
            return -LX_ERESTARTSYS;
        }

        sched_block(p, "waitid", 100);
    }
}

/* Строки из массива указателей в памяти программы (argv/envp) */
static INTN copy_strv(KPROC *p, UINT64 uvec, char *out, UINTN cap, UINTN *len, UINTN *count)
{
    if (uvec == 0)
        return 0;

    for (UINTN i = 0; ; i++) {

        if (!uptr_ok(p, uvec + i * 8u, 8, FALSE))
            return -LX_EFAULT;

        UINT64 s = *(volatile UINT64 *)(UINTN)(uvec + i * 8u);

        if (s == 0)
            break;

        for (UINTN k = 0; ; k++) {

            if (*len + 1 >= cap)
                return -LX_E2BIG;

            if (k == 0 || ((s + k) & 0xFFFu) == 0)
                if (!uptr_ok(p, s + k, 1, FALSE))
                    return -LX_EFAULT;

            char c = *(volatile char *)(UINTN)(s + k);
            out[(*len)++] = c;

            if (c == '\0')
                break;
        }

        (*count)++;
    }

    return 0;
}

/* Убрать остальные потоки процесса (перед execve) и дождаться */
static void kill_other_threads(KPROC *p)
{
    UINT64 fl = kx_irq_save();

    for (;;) {

        BOOLEAN any = FALSE;

        for (UINTN i = 0; i < KT_MAX; i++) {
            KTHREAD *t = &g_kthreads[i];
            if (t != g_kcur && t->proc == p && t->state != KT_DEAD && t->state != KT_UNUSED) {
                t->lx_die = TRUE;
                sched_wake_thread(t);
                any = TRUE;
            }
        }

        if (!any)
            break;

        sched_block(p, "execve", 10);
    }

    kx_irq_restore(fl);
}

static INT64 sys_execve(KPROC *p, INT64 dirfd, UINT64 upath, UINT64 uargv, UINT64 uenvp)
{
    char path[VFS_PATH_MAX], real[VFS_PATH_MAX];
    INT64 r = lx_resolve_path(p, dirfd, upath, path, sizeof(path));

    if (r < 0)
        return r;

    lx_lookup_exec(p, path, real, sizeof(real));

    /* argv и envp - в ядро (старая память вот-вот исчезнет) */
    char *args = (char *)kmalloc(LX_MAX_ARGS);
    char *envs = (char *)kmalloc(LX_MAX_ARGS);

    if (args == NULL || envs == NULL) {
        kfree(args);
        kfree(envs);
        return -LX_ENOMEM;
    }

    UINTN alen = 0, argc = 0, elen = 0, envc = 0;

    r = copy_strv(p, uargv, args, LX_MAX_ARGS, &alen, &argc);
    if (r == 0)
        r = copy_strv(p, uenvp, envs, LX_MAX_ARGS, &elen, &envc);

    /* сценарий "#!интерпретатор [аргумент]": запустить интерпретатор
       с путём сценария (до 4 уровней) */
    for (UINTN depth = 0; r == 0 && depth < 4; depth++) {

        INTN kfd = vfs_open(real, VFS_O_READ);

        if (kfd < 0) {
            r = linux_errno(kfd);
            break;
        }

        char head[256];
        INTN got = vfs_read(kfd, head, sizeof(head) - 1);
        vfs_close(kfd);

        if (got < 4) {
            r = -LX_ENOEXEC;
            break;
        }

        head[got] = '\0';

        if (head[0] == 0x7F && head[1] == 'E' && head[2] == 'L' && head[3] == 'F')
            break;

        if (head[0] != '#' || head[1] != '!') {
            r = -LX_ENOEXEC;
            break;
        }

        /* разобрать строку #! */
        char *s = head + 2;
        while (*s == ' ' || *s == '\t')
            s++;
        char *ip = s;
        while (*s && *s != ' ' && *s != '\t' && *s != '\n')
            s++;
        char save = *s;
        *s = '\0';
        char interp[VFS_PATH_MAX];
        ksnprintf(interp, sizeof(interp), "%s", ip);
        char opt[128];
        opt[0] = '\0';
        if (save == ' ' || save == '\t') {
            s++;
            while (*s == ' ' || *s == '\t')
                s++;
            char *o = s;
            while (*s && *s != '\n')
                s++;
            while (s > o && (s[-1] == ' ' || s[-1] == '\t' || s[-1] == '\r'))
                s--;
            *s = '\0';
            ksnprintf(opt, sizeof(opt), "%s", o);
        }

        /* новый argv: interp [opt] путь argv[1..] (путь - как его
           видит Linux: интерпретатор откроет его сам) */
        char *na = (char *)kmalloc(LX_MAX_ARGS);
        if (na == NULL) {
            r = -LX_ENOMEM;
            break;
        }
        char lreal[VFS_PATH_MAX];
        lx_path_to_linux(p, real, lreal, sizeof(lreal));
        UINTN nl = 0, nc = 0;
        const char *parts[3] = { interp, opt[0] ? opt : NULL, lreal };
        for (UINTN k = 0; k < 3; k++) {
            if (parts[k] == NULL)
                continue;
            UINTN m = 0;
            while (parts[k][m])
                m++;
            if (nl + m + 1 >= LX_MAX_ARGS)
                break;
            memcpy(na + nl, parts[k], m + 1);
            nl += m + 1;
            nc++;
        }
        /* пропустить старый argv[0] */
        UINTN skip = 0;
        if (argc > 0) {
            while (args[skip])
                skip++;
            skip++;
        }
        if (nl + (alen - skip) < LX_MAX_ARGS) {
            memcpy(na + nl, args + skip, alen - skip);
            nl += alen - skip;
            nc += (argc > 0) ? argc - 1 : 0;
        }
        kfree(args);
        args = na;
        alen = nl;
        argc = nc;

        ksnprintf(path, sizeof(path), "%s", interp);
        if (lx_resolve_kpath(p, LX_AT_FDCWD, interp, TRUE, real, sizeof(real)) < 0)
            ksnprintf(real, sizeof(real), "%s", interp);
    }

    if (r != 0) {
        kfree(args);
        kfree(envs);
        return r;
    }

    /* программа MyOS из процесса Linux пока не запускается */
    {
        INTN kfd = vfs_open(real, VFS_O_READ);
        UINT8 *hdr = (UINT8 *)kmalloc(4096);
        BOOLEAN lin = FALSE;
        if (kfd >= 0 && hdr != NULL) {
            INTN got = vfs_read(kfd, hdr, 4096);
            lin = (got > 0) && linux_elf_is_linux(hdr, (UINTN)got);
        }
        if (kfd >= 0)
            vfs_close(kfd);
        kfree(hdr);
        if (!lin) {
            kfree(args);
            kfree(envs);
            return -LX_ENOEXEC;
        }
    }

    /* новое адресное пространство рядом со старым */
    UINT64 old_pml4 = p->pml4;
    UVMA *old_vmas = p->vmas;
    UINT64 old_pages = p->pages, old_top = p->mmap_top, old_bb = p->brk_base, old_brk = p->brk;

    p->pml4 = proc_new_pml4();
    p->vmas = NULL;
    p->pages = 0;
    p->mmap_top = LX_MMAP_TOP;

    const char *why = "out of memory";
    LX_LOADINFO li;
    INTN lr = (p->pml4 == 0) ? VFS_ENOSPC : lx_load_program(p, real, &li, &why);
    UINT64 sp = 0;

    if (lr == VFS_OK) {

        /* одна строка подряд: argv, потом envp */
        char *all = (char *)kmalloc(alen + elen + 1);

        if (all == NULL) {
            lr = VFS_ENOSPC;
        } else {
            char lreal[VFS_PATH_MAX];
            lx_path_to_linux(p, real, lreal, sizeof(lreal));
            memcpy(all, args, alen);
            memcpy(all + alen, envs, elen);
            lr = lx_build_stack(p, &li, all, alen + elen, argc, envc, lreal, &sp);
            kfree(all);
        }
    }

    kfree(args);
    kfree(envs);

    if (lr != VFS_OK) {
        /* не вышло - вернуть старую память как была */
        klog("linux: execve %s failed: %s\n", real, why);
        if (p->pml4 != 0)
            uvm_free(p->pml4);
        uvm_free_vmas(p);
        p->pml4 = old_pml4;
        p->vmas = old_vmas;
        p->pages = old_pages;
        p->mmap_top = old_top;
        p->brk_base = old_bb;
        p->brk = old_brk;
        return linux_errno(lr);
    }

    /* точка невозврата: остальные потоки - прочь, старая память - тоже */
    kill_other_threads(p);

    UINT64 fl = kx_irq_save();
    KTHREAD *t = g_kcur;
    t->cr3 = p->pml4;
    t->fs_base = 0;
    proc_switch_hook(t);
    kx_irq_restore(fl);

    {
        /* освободить старое (CR3 уже новый) */
        UVMA *keep = p->vmas;
        p->vmas = old_vmas;
        uvm_free_vmas(p);
        p->vmas = keep;
        uvm_free(old_pml4);
    }

    p->brk_base = p->brk = li.brk;
    t->clear_tid = 0;
    t->alt_sp = t->alt_size = 0;
    t->alt_flags = LX_SS_DISABLE;

    lx_files_exec(p);
    lx_sig_exec_reset(p);

    ksnprintf(p->lx->exe, sizeof(p->lx->exe), "%s", real);
    ksnprintf(p->path, sizeof(p->path), "%s", real);
    {
        const char *b = real;
        for (const char *s = real; *s; s++)
            if (*s == '/')
                b = s + 1;
        ksnprintf(p->name, sizeof(p->name), "%s", b);
        kthread_rename(t, p->name);
    }

    if (p->lx->vfork_wait) {
        p->lx->vfork_wait = FALSE;
        sched_wake_all(p->lx);
    }

    klog("linux: pid %u execve %s\n", p->pid, real);

    LX_REGS regs;
    memset(&regs, 0, sizeof(regs));
    regs.rip = li.entry;
    regs.rsp = sp;
    regs.rflags = 0x202;

    lx_enter(&regs, g_fx_default);
}


/* ================================================================
 * Сведения о системе
 * ================================================================ */

static INT64 sys_uname(KPROC *p, UINT64 ubuf)
{
    if (!uptr_ok(p, ubuf, 65u * 6u, TRUE))
        return -LX_EFAULT;

    char *b = (char *)(UINTN)ubuf;
    memset(b, 0, 65u * 6u);
    ksnprintf(b + 0 * 65, 65, "Linux");
    ksnprintf(b + 1 * 65, 65, "myos");
    ksnprintf(b + 2 * 65, 65, "6.1.0-myos");
    ksnprintf(b + 3 * 65, 65, "#1 SMP MyOS");
    ksnprintf(b + 4 * 65, 65, "x86_64");
    ksnprintf(b + 5 * 65, 65, "(none)");
    return 0;
}

static INT64 put_timespec(KPROC *p, UINT64 u, UINT64 ns)
{
    if (!uptr_ok(p, u, sizeof(LX_TIMESPEC), TRUE))
        return -LX_EFAULT;

    LX_TIMESPEC ts = { (INT64)(ns / 1000000000ull), (INT64)(ns % 1000000000ull) };
    memcpy((void *)(UINTN)u, &ts, sizeof(ts));
    return 0;
}

/* Время процессора потока (или всего процесса) в наносекундах */
static UINT64 cpu_ns(KPROC *p, BOOLEAN whole)
{
    UINT64 tsc = 0;

    for (UINTN i = 0; i < KT_MAX; i++) {
        KTHREAD *t = &g_kthreads[i];
        if ((whole && t->proc == p) || (!whole && t == g_kcur))
            tsc += t->cpu_tsc;
    }

    if (g_tsc_hz == 0)
        return 0;

    return (tsc / (g_tsc_hz / 1000000u ? g_tsc_hz / 1000000u : 1)) * 1000u;
}

static UINT64 clock_ns(KPROC *p, UINT64 clk)
{
    switch (clk) {
    case 0: case 5: case 8: case 11:            /* REALTIME (_COARSE, _ALARM), TAI */
        return lx_now_ns(TRUE);
    case 2:                                     /* PROCESS_CPUTIME_ID */
        return cpu_ns(p, TRUE);
    case 3:                                     /* THREAD_CPUTIME_ID */
        return cpu_ns(p, FALSE);
    default:                                    /* MONOTONIC (_RAW, _COARSE), BOOTTIME */
        return lx_now_ns(FALSE);
    }
}

static INT64 sys_prlimit(KPROC *p, UINT64 res, UINT64 unew, UINT64 uold)
{
    (void)unew;

    if (uold != 0) {
        if (!uptr_ok(p, uold, 16, TRUE))
            return -LX_EFAULT;
        UINT64 v[2] = { ~0ull, ~0ull };
        if (res == 3)                   /* RLIMIT_STACK */
            v[0] = LX_STACK_SIZE;
        else if (res == 7)              /* RLIMIT_NOFILE */
            v[0] = v[1] = LX_FDS;
        else if (res == 6)              /* RLIMIT_NPROC */
            v[0] = v[1] = KT_MAX;
        else if (res == 4)              /* RLIMIT_CORE */
            v[0] = 0;
        memcpy((void *)(UINTN)uold, v, 16);
    }

    return 0;
}


/* ================================================================
 * Диспетчер
 * ================================================================ */

enum {
    NR_mmap = 9, NR_mprotect = 10, NR_munmap = 11, NR_brk = 12, NR_rt_sigreturn = 15,
    NR_sched_yield = 24, NR_mremap = 25, NR_msync = 26, NR_mincore = 27, NR_madvise = 28,
    NR_pause = 34, NR_nanosleep = 35, NR_getitimer = 36, NR_alarm = 37, NR_setitimer = 38,
    NR_getpid = 39, NR_socket = 41, NR_socketpair = 53, NR_clone = 56, NR_fork = 57,
    NR_vfork = 58, NR_execve = 59, NR_exit = 60, NR_wait4 = 61, NR_uname = 63,
    NR_gettimeofday = 96, NR_getrlimit = 97, NR_getrusage = 98, NR_sysinfo = 99, NR_times = 100,
    NR_syslog = 103, NR_getuid = 102, NR_getgid = 104, NR_setuid = 105, NR_setgid = 106,
    NR_geteuid = 107, NR_getegid = 108, NR_setpgid = 109, NR_getppid = 110, NR_getpgrp = 111,
    NR_setsid = 112, NR_setreuid = 113, NR_setregid = 114, NR_getgroups = 115,
    NR_setgroups = 116, NR_setresuid = 117, NR_getresuid = 118, NR_setresgid = 119,
    NR_getresgid = 120, NR_getpgid = 121, NR_setfsuid = 122, NR_setfsgid = 123,
    NR_getsid = 124, NR_capget = 125, NR_capset = 126, NR_personality = 135,
    NR_getpriority = 140, NR_setpriority = 141, NR_sched_setparam = 142,
    NR_sched_getparam = 143, NR_sched_setscheduler = 144, NR_sched_getscheduler = 145,
    NR_sched_get_priority_max = 146, NR_sched_get_priority_min = 147,
    NR_mlock = 149, NR_munlock = 150, NR_mlockall = 151, NR_munlockall = 152,
    NR_prctl = 157, NR_arch_prctl = 158, NR_setrlimit = 160, NR_gettid = 186,
    NR_time = 201, NR_futex = 202, NR_sched_setaffinity = 203, NR_sched_getaffinity = 204,
    NR_set_tid_address = 218, NR_clock_gettime = 228, NR_clock_getres = 229,
    NR_clock_nanosleep = 230, NR_exit_group = 231, NR_waitid = 247, NR_set_robust_list = 273,
    NR_get_robust_list = 274, NR_prlimit64 = 302, NR_getcpu = 309, NR_getrandom = 318,
    NR_memfd_create = 319, NR_execveat = 322, NR_membarrier = 324, NR_rseq = 334,
    NR_clone3 = 435,
};

/* Вызовы, о которых уже написали в журнал "не умеем" (по одному разу) */
static UINT8 g_lx_unknown_said[512];

static INT64 lx_dispatch(KPROC *p, UINT64 *f, UINT64 nr, UINT64 *a)
{
    KTHREAD *me = g_kcur;
    BOOLEAN handled;
    INT64 r = lx_file_syscall(p, nr, a, &handled);

    if (handled)
        return r;

    r = lx_sig_syscall(p, nr, a, f, &handled);

    if (handled)
        return r;

    r = lx_sock_syscall(p, nr, a, &handled);

    if (handled)
        return r;

    switch (nr) {

    /* ---- память ---- */
    case NR_mmap:
        return sys_mmap(p, a[0], a[1], a[2], a[3], (INT64)a[4], a[5]);

    case NR_munmap:
        if ((a[0] & 0xFFFu) || a[1] == 0)
            return -LX_EINVAL;
        uvm_unmap(p, a[0], a[0] + ((a[1] + 0xFFFu) & ~0xFFFull));
        return 0;

    case NR_mprotect: {
        if (a[0] & 0xFFFu)
            return -LX_EINVAL;
        UINT64 e = a[0] + ((a[1] + 0xFFFu) & ~0xFFFull);
        if (e == a[0])
            return 0;
        INTN rr = uvm_protect(p, a[0], e, prot_from_linux(a[2]));
        return (rr < 0) ? -LX_ENOMEM : 0;
    }

    case NR_brk:
        return sys_brk(p, a[0]);

    case NR_mremap:
        return sys_mremap(p, a[0], a[1], a[2], a[3], a[4]);

    case NR_madvise:
        if (a[2] == LX_MADV_DONTNEED || a[2] == LX_MADV_FREE)
            madv_dontneed(p, a[0] & ~0xFFFull, (a[0] + a[1] + 0xFFFu) & ~0xFFFull);
        return 0;

    case NR_mincore: {
        UINT64 pages = (a[1] + 0xFFFu) / 4096u;
        if (!uptr_ok(p, a[2], pages, TRUE))
            return -LX_EFAULT;
        memset((void *)(UINTN)a[2], 1, (UINTN)pages);
        return 0;
    }

    case NR_msync: case NR_mlock: case NR_munlock: case NR_mlockall: case NR_munlockall:
        return 0;

    /* ---- потоки и процессы ---- */
    case NR_clone:
        return sys_clone(p, f, a[0], a[1], a[2], a[3], a[4]);

    case NR_fork:
        return sys_clone(p, f, LX_SIGCHLD, 0, 0, 0, 0);

    case NR_vfork:
        return sys_clone(p, f, LX_CLONE_VFORK | LX_CLONE_VM | LX_SIGCHLD, 0, 0, 0, 0);

    case NR_clone3:
        return -LX_ENOSYS;              /* glibc тогда зовёт обычный clone */

    case NR_execve:
        return sys_execve(p, LX_AT_FDCWD, a[0], a[1], a[2]);

    case NR_execveat:
        return sys_execve(p, (INT64)a[0], a[1], a[2], a[3]);

    case NR_exit:
        lx_exit_thread(p, (INT64)(a[0] & 0xFF));

    case NR_exit_group:
        lx_exit_group(p, (INT64)(a[0] & 0xFF));

    case NR_wait4:
        return sys_wait4(p, (INT64)(INT32)a[0], a[1], a[2], a[3]);

    case NR_waitid:
        return sys_waitid(p, a[0], a[1], a[2], a[3]);

    case NR_getpid:
        return p->pid;

    case NR_gettid:
        return lx_tid(me);

    case NR_getppid:
        return p->parent ? p->parent->pid : 1;

    case NR_set_tid_address:
        me->clear_tid = a[0];
        return lx_tid(me);

    case NR_set_robust_list:
        return 0;

    case NR_get_robust_list:
    case NR_rseq:
        return -LX_ENOSYS;

    case NR_futex:
        return sys_futex(p, a[0], a[1], a[2], a[3], a[4], a[5]);

    case NR_arch_prctl:
        switch (a[0]) {
        case 0x1002:                            /* ARCH_SET_FS */
            if (a[1] >= 0x0000800000000000ull)
                return -LX_EPERM;
            {
                /* без прерываний: иначе поток мог бы переехать на другое
                   ядро между записью MSR и запоминанием (proc_switch_hook
                   сверяет с запомненным) */
                UINT64 fl = kx_irq_save();
                me->fs_base = a[1];
                kx_wrmsr(0xC0000100u, a[1]);
                kx_cpu()->fs_base = a[1];
                kx_irq_restore(fl);
            }
            return 0;
        case 0x1003:                            /* ARCH_GET_FS */
            if (!uptr_ok(p, a[1], 8, TRUE))
                return -LX_EFAULT;
            *(volatile UINT64 *)(UINTN)a[1] = me->fs_base;
            return 0;
        case 0x1011:                            /* ARCH_GET_CPUID */
            return 1;
        case 0x1012:                            /* ARCH_SET_CPUID */
            return 0;
        }
        return -LX_EINVAL;

    case NR_prctl:
        switch (a[0]) {
        case 15: {                              /* PR_SET_NAME */
            char nm[16];
            for (UINTN i = 0; i < 15; i++) {
                if (!uptr_ok(p, a[1] + i, 1, FALSE))
                    return -LX_EFAULT;
                nm[i] = *(volatile char *)(UINTN)(a[1] + i);
                if (nm[i] == '\0')
                    break;
            }
            nm[15] = '\0';
            kthread_rename(me, nm);
            return 0;
        }
        case 16:                                /* PR_GET_NAME */
            if (!uptr_ok(p, a[1], 16, TRUE))
                return -LX_EFAULT;
            memcpy((void *)(UINTN)a[1], me->name, 16);
            return 0;
        case 1: case 38: case 4: case 0x59616d61: case 0x53564d41:
            return 0;                           /* PDEATHSIG, NO_NEW_PRIVS, DUMPABLE, PTRACER, SET_VMA */
        case 3:
            return 1;                           /* PR_GET_DUMPABLE */
        case 2:                                 /* PR_GET_PDEATHSIG */
            if (uptr_ok(p, a[1], 4, TRUE))
                *(volatile INT32 *)(UINTN)a[1] = 0;
            return 0;
        }
        return -LX_EINVAL;

    /* ---- кто я ---- */
    case NR_getuid: case NR_getgid: case NR_geteuid: case NR_getegid:
        return 0;

    case NR_setuid: case NR_setgid: case NR_setreuid: case NR_setregid:
    case NR_setresuid: case NR_setresgid: case NR_setgroups:
    case NR_setfsuid: case NR_setfsgid:
        return 0;

    case NR_getresuid:
    case NR_getresgid:
        for (UINTN i = 0; i < 3; i++)
            if (a[i] && uptr_ok(p, a[i], 4, TRUE))
                *(volatile UINT32 *)(UINTN)a[i] = 0;
        return 0;

    case NR_getgroups:
        return 0;

    case NR_capget:
        if (a[1] && uptr_ok(p, a[1], 24, TRUE))
            memset((void *)(UINTN)a[1], 0, 24);
        return 0;

    case NR_capset:
        return 0;

    case NR_getpgid:
    case NR_getsid: {
        KPROC *q = p;
        if (a[0] != 0) {
            q = NULL;
            for (UINTN i = 0; i < PROC_MAX; i++)
                if (g_procs[i].used && g_procs[i].pid == (UINT32)a[0])
                    q = &g_procs[i];
        }
        if (q == NULL)
            return -LX_ESRCH;
        if (q->lx == NULL)
            return q->pid;
        return (nr == NR_getpgid) ? q->lx->pgid : q->lx->sid;
    }

    case NR_getpgrp:
        return p->lx->pgid;

    case NR_setpgid: {
        KPROC *q = p;
        if (a[0] != 0 && a[0] != p->pid) {
            q = NULL;
            for (UINTN i = 0; i < PROC_MAX; i++)
                if (g_procs[i].used && g_procs[i].pid == (UINT32)a[0] && g_procs[i].parent == p)
                    q = &g_procs[i];
        }
        if (q == NULL || q->lx == NULL)
            return -LX_ESRCH;
        q->lx->pgid = a[1] ? (UINT32)a[1] : q->pid;
        return 0;
    }

    case NR_setsid:
        p->lx->sid = p->pid;
        p->lx->pgid = p->pid;
        return p->pid;

    case NR_personality:
        return 0;

    /* ---- время ---- */
    case NR_clock_gettime:
        return put_timespec(p, a[1], clock_ns(p, a[0]));

    case NR_clock_getres:
        return a[1] ? put_timespec(p, a[1], 1000u) : 0;

    case NR_gettimeofday: {
        if (a[0] != 0) {
            if (!uptr_ok(p, a[0], sizeof(LX_TIMEVAL), TRUE))
                return -LX_EFAULT;
            UINT64 ns = lx_now_ns(TRUE);
            LX_TIMEVAL tv = { (INT64)(ns / 1000000000ull), (INT64)((ns / 1000u) % 1000000u) };
            memcpy((void *)(UINTN)a[0], &tv, sizeof(tv));
        }
        if (a[1] != 0 && uptr_ok(p, a[1], 8, TRUE))
            memset((void *)(UINTN)a[1], 0, 8);
        return 0;
    }

    case NR_time: {
        INT64 s = (INT64)(lx_now_ns(TRUE) / 1000000000ull);
        if (a[0] != 0 && uptr_ok(p, a[0], 8, TRUE))
            *(volatile INT64 *)(UINTN)a[0] = s;
        return s;
    }

    case NR_nanosleep:
    case NR_clock_nanosleep: {
        UINT64 uts = (nr == NR_nanosleep) ? a[0] : a[2];
        UINT64 urem = (nr == NR_nanosleep) ? a[1] : a[3];
        if (!uptr_ok(p, uts, sizeof(LX_TIMESPEC), FALSE))
            return -LX_EFAULT;
        LX_TIMESPEC ts;
        memcpy(&ts, (const void *)(UINTN)uts, sizeof(ts));
        if (ts.tv_sec < 0 || ts.tv_nsec < 0 || ts.tv_nsec >= 1000000000)
            return -LX_EINVAL;
        UINT64 ns = (UINT64)ts.tv_sec * 1000000000ull + (UINT64)ts.tv_nsec;
        if (nr == NR_clock_nanosleep && (a[1] & 1)) {       /* TIMER_ABSTIME */
            UINT64 now = clock_ns(p, a[0]);
            ns = (ns > now) ? ns - now : 0;
        }
        UINT64 t0 = lx_now_ns(FALSE);
        INT64 rr = lx_sleep_ns(p, ns);
        if (rr < 0 && urem != 0 && !(nr == NR_clock_nanosleep && (a[1] & 1))) {
            UINT64 spent = lx_now_ns(FALSE) - t0;
            put_timespec(p, urem, (spent < ns) ? ns - spent : 0);
        }
        return rr;
    }

    case NR_sched_yield:
        sched_yield();
        return 0;

    case NR_getrlimit:
        return sys_prlimit(p, a[0], 0, a[1]);

    case NR_setrlimit:
        return 0;

    case NR_prlimit64:
        return sys_prlimit(p, a[1], a[2], a[3]);

    case NR_getrusage:
        if (!uptr_ok(p, a[1], 144, TRUE))
            return -LX_EFAULT;
        memset((void *)(UINTN)a[1], 0, 144);
        {
            UINT64 ns = cpu_ns(p, a[0] != 1);
            LX_TIMEVAL tv = { (INT64)(ns / 1000000000ull), (INT64)((ns / 1000u) % 1000000u) };
            memcpy((void *)(UINTN)a[1], &tv, sizeof(tv));
        }
        return 0;

    case NR_times: {
        UINT64 ticks = lx_now_ns(FALSE) / 10000000ull;
        if (a[0] != 0) {
            if (!uptr_ok(p, a[0], 32, TRUE))
                return -LX_EFAULT;
            UINT64 v[4] = { cpu_ns(p, TRUE) / 10000000ull, 0, 0, 0 };
            memcpy((void *)(UINTN)a[0], v, 32);
        }
        return (INT64)ticks;
    }

    case NR_sysinfo: {
        if (!uptr_ok(p, a[0], 112, TRUE))
            return -LX_EFAULT;
        UINT64 s[14];
        memset(s, 0, sizeof(s));
        s[0] = g_kticks / 1000u;                /* uptime */
        s[4] = g_kmm_usable_pages * 4096u;      /* totalram */
        s[5] = g_kmm_free_pages * 4096u;        /* freeram */
        memset((void *)(UINTN)a[0], 0, 112);
        memcpy((void *)(UINTN)a[0], s, 8u * 10u);
        *(volatile UINT16 *)(UINTN)(a[0] + 80) = 1;     /* procs */
        *(volatile UINT32 *)(UINTN)(a[0] + 104) = 1;    /* mem_unit */
        return 0;
    }

    case NR_uname:
        return sys_uname(p, a[0]);

    case NR_getrandom: {
        UINT64 n = (a[1] > 1048576u) ? 1048576u : a[1];
        if (!uptr_ok(p, a[0], n, TRUE))
            return -LX_EFAULT;
        krandom_fill((void *)(UINTN)a[0], (UINTN)n);
        return (INT64)n;
    }

    case NR_getcpu:
        if (a[0] && uptr_ok(p, a[0], 4, TRUE))
            *(volatile UINT32 *)(UINTN)a[0] = kx_cpu_index();
        if (a[1] && uptr_ok(p, a[1], 4, TRUE))
            *(volatile UINT32 *)(UINTN)a[1] = 0;
        return 0;

    case NR_sched_getaffinity: {
        UINTN n = (a[1] < 8) ? (UINTN)a[1] : 8;
        if (n == 0 || !uptr_ok(p, a[2], a[1], TRUE))
            return -LX_EINVAL;
        memset((void *)(UINTN)a[2], 0, (UINTN)a[1]);
        UINT64 m = (g_ncpus >= 64) ? ~0ull : ((1ull << g_ncpus) - 1u);
        memcpy((void *)(UINTN)a[2], &m, n);
        return 8;
    }

    case NR_sched_setaffinity: case NR_sched_setparam: case NR_sched_setscheduler:
    case NR_setpriority:
        return 0;

    case NR_sched_getparam:
        if (uptr_ok(p, a[1], 4, TRUE))
            *(volatile INT32 *)(UINTN)a[1] = 0;
        return 0;

    case NR_sched_getscheduler: case NR_sched_get_priority_max: case NR_sched_get_priority_min:
        return 0;

    case NR_getpriority:
        return 20;

    case NR_membarrier:
        return 0;

    case NR_syslog:
        return 0;



    /* расширенные атрибуты (xattr: ACL, SELinux): у нас их нет - как
       у файловой системы без xattr ("не поддерживается"); ls -l и cp
       спрашивают и спокойно идут дальше */
    case 188: case 189: case 190:       /* setxattr, lsetxattr, fsetxattr */
    case 191: case 192: case 193:       /* getxattr, lgetxattr, fgetxattr */
    case 194: case 195: case 196:       /* listxattr ... */
    case 197: case 198: case 199:       /* removexattr ... */
        return -LX_EOPNOTSUPP;
    }

    if (nr < sizeof(g_lx_unknown_said) && !g_lx_unknown_said[nr]) {
        g_lx_unknown_said[nr] = 1;
        klog("linux: pid %u '%s': system call %llu is not supported yet\n", p->pid, p->name, nr);
    }

    return -LX_ENOSYS;
}

/*
 * Вход из syscall.c для программы Linux. После вызова: не попросили ли
 * завершиться, нет ли сигнала (тогда - в обработчик, не возвращаясь
 * обычным путём).
 */
INT64 linux_syscall(KPROC *p, UINT64 *f)
{
    UINT64 nr = f[SF_RAX];
    UINT64 a[6] = { f[SF_RDI], f[SF_RSI], f[SF_RDX], f[SF_R10], f[SF_R8], f[SF_R9] };
    INT64 r = lx_dispatch(p, f, nr, a);

    if (p->killed)
        proc_exit_current(-1);

    if (g_kcur->lx_die)
        lx_exit_thread(p, 0);

    if (lx_signal_ready(p)) {
        LX_REGS regs;
        regs_from_syscall(f, &regs, r);
        lx_signal_deliver(p, &regs, frame_fx(f), r, nr, TRUE);
    }

    if (r == -LX_ERESTARTSYS)
        r = -LX_EINTR;

    return r;
}

/*
 * Конец прерывания, дальше - программа Linux (cpu.c): таймер alarm,
 * "умри" после execve, сигналы (в том числе от ошибки - SIGSEGV).
 */
void linux_isr_return(KX_ISR_FRAME *f)
{
    KPROC *p = g_kcur->proc;

    if (p == NULL || !p->is_linux)
        return;

    if (g_kcur->lx_die)
        lx_exit_thread(p, 0);

    if (!lx_signal_ready(p))
        return;

    LX_REGS r;

    r.r15 = f->r15; r.r14 = f->r14; r.r13 = f->r13; r.r12 = f->r12;
    r.r11 = f->r11; r.r10 = f->r10; r.r9 = f->r9; r.r8 = f->r8;
    r.rbp = f->rbp; r.rdi = f->rdi; r.rsi = f->rsi; r.rdx = f->rdx;
    r.rcx = f->rcx; r.rbx = f->rbx; r.rax = f->rax;
    r.rip = f->rip; r.cs = f->cs; r.rflags = f->rflags; r.rsp = f->rsp; r.ss = f->ss;

    lx_signal_deliver(p, &r, frame_fx(f), 0, 0, FALSE);
}

/* Для lxsig.c: вход в программу с регистрами */
void lx_enter_regs(LX_REGS *r, const void *fx)
{
    lx_enter(r, fx);
}

const void *lx_fx_default(void)
{
    fx_default_init();
    return g_fx_default;
}
