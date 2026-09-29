/*
 * kernel/proc.c - программы в пользовательском режиме (этап 6).
 * Часть MyOS; общие объявления - в myos.h, номера вызовов - sysnum.h.
 *
 * ЯДРО И ПРОГРАММА
 * ----------------
 * До этапа 6 все команды были частью ядра: ошибка в любой роняла всю
 * систему (экран паники). Теперь программа - отдельный файл ELF
 * (как в Linux), который ядро загружает в СВОЮ, отдельную память и
 * запускает в "кольце 3" (ring 3) - режиме процессора, где нельзя:
 *   * трогать память ядра (у её страниц нет бита U - "user"),
 *   * выполнять привилегированные инструкции (cli, hlt, in/out,
 *     запись в CR3...) - процессор выдаст исключение #GP.
 * Всё, что программе нужно от мира (вывести текст, открыть файл,
 * поспать), она просит у ядра системным вызовом (syscall.c).
 * Если программа ломается (Page Fault, деление на ноль...), виновата
 * она: ядро пишет понятное сообщение и завершает только её.
 *
 * ПРОЦЕСС = своё адресное пространство + поток (этап 4):
 *   * своя таблица страниц (PML4). Нижняя половина адресов (до
 *     0x0000800000000000) - программа: код с 0x400000, куча после
 *     него, стек под 0x00007FFFFFFF0000. Верхняя половина - ядро,
 *     она одинаковая у всех (копируем 256 верхних записей PML4);
 *   * поток ядра, который "входит" в программу инструкцией iretq и
 *     возвращается в ядро на каждом прерывании и системном вызове
 *     (стек ядра - свой у потока, адрес - в TSS.rsp0);
 *   * таблица открытых файлов и текущая папка.
 *
 * Завершившийся процесс память освобождает не сам (он стоит на своих
 * же таблицах страниц), а тот, кто его ждал: proc_wait -> proc_reap.
 */
#include "myos.h"

KPROC g_procs[PROC_MAX];
KPROC *volatile g_fg_proc = NULL;        /* программа "на переднем плане"
                                            (её завершает Ctrl+C) */
static UINT32 g_next_pid = 1;
const char *g_proc_last_error = "";     /* почему не запустилась (для шелла) */
void (*g_proc_gui_sink)(const char *line) = NULL;   /* куда программы GUI
                                                        пишут строки (терминал) */

/* биты записи таблицы страниц (как в vmm.c) */
#define UPTE_P     (1ull << 0)
#define UPTE_W     (1ull << 1)
#define UPTE_U     (1ull << 2)
#define UPTE_PS    (1ull << 7)
#define UPTE_NX    (1ull << 63)
#define UPTE_ADDR  0x000FFFFFFFFFF000ull

#define MAX_ELF_SIZE   (8u * 1024u * 1024u)


/* ================================================================
 * Адресное пространство программы
 * ================================================================ */

static UINT64 read_cr3(void)
{
    UINT64 v;

    __asm__ __volatile__("mov %%cr3, %0" : "=r"(v));
    return v;
}

/* Верхняя половина (ядро) - как у ядра. Зовётся при каждом входе в
   процесс: если ядро с тех пор завело новую верхнюю запись PML4
   (например, отобразило регистры нового устройства), процесс её
   тоже увидит. 256 записей - 2 КиБ, копия почти бесплатна. */
static void uvm_sync_kernel_half(UINT64 pml4)
{
    UINT64 *dst = (UINT64 *)P2V(pml4);
    UINT64 *src = (UINT64 *)P2V(g_vmm_pml4_phys);

    for (UINTN i = 256; i < 512; i++)
        dst[i] = src[i];
}

/*
 * Запись таблицы страниц программы для адреса va (4 КиБ). create -
 * создать недостающие таблицы (с битом U: иначе процессор не пустит
 * программу ниже, даже если у самой страницы U есть).
 */
static UINT64 *uvm_pte(UINT64 pml4, UINT64 va, BOOLEAN create)
{
    UINT64 *t = (UINT64 *)P2V(pml4);

    for (INTN lvl = 3; lvl >= 1; lvl--) {

        UINTN idx = (UINTN)((va >> (12u + 9u * (UINTN)lvl)) & 511u);
        UINT64 *e = &t[idx];

        if (!(*e & UPTE_P)) {

            if (!create)
                return NULL;

            UINT64 nt = pmm_alloc_zeroed(1, 0);

            if (nt == 0)
                return NULL;

            *e = nt | UPTE_P | UPTE_W | UPTE_U;
        }

        if (*e & UPTE_PS)
            return NULL;            /* больших страниц у программ нет */

        t = (UINT64 *)P2V(*e & UPTE_ADDR);
    }

    return &t[(va >> 12) & 511u];
}

/* Новая обнулённая страница программы по адресу va */
static BOOLEAN uvm_map_new(KPROC *p, UINT64 va, BOOLEAN writable, BOOLEAN exec)
{
    UINT64 *e = uvm_pte(p->pml4, va, TRUE);

    if (e == NULL)
        return FALSE;

    if (*e & UPTE_P) {
        /* уже есть (два сегмента ELF на одной странице) - права
           объединяем */
        if (writable)
            *e |= UPTE_W;
        if (exec)
            *e &= ~UPTE_NX;
        return TRUE;
    }

    UINT64 phys = pmm_alloc_zeroed(1, 0);

    if (phys == 0)
        return FALSE;

    *e = phys | UPTE_P | UPTE_U |
         (writable ? UPTE_W : 0) |
         ((!exec && g_vmm_nx) ? UPTE_NX : 0);

    p->pages++;

    return TRUE;
}

/* Страница кучи (sbrk) */
BOOLEAN proc_map_heap_page(KPROC *p, UINT64 va)
{
    return uvm_map_new(p, va, TRUE, FALSE);
}

/* Физический адрес страницы программы (0 - не отображена) */
static UINT64 uvm_phys(KPROC *p, UINT64 va)
{
    UINT64 *e = uvm_pte(p->pml4, va, FALSE);

    if (e == NULL || !(*e & UPTE_P))
        return 0;

    return (*e & UPTE_ADDR) | (va & 0xFFFu);
}

/* Скопировать данные ядра в память программы (при загрузке) */
static BOOLEAN uvm_copy_to(KPROC *p, UINT64 va, const UINT8 *src, UINT64 n)
{
    while (n > 0) {

        UINT64 phys = uvm_phys(p, va);

        if (phys == 0)
            return FALSE;

        UINT64 room = 4096u - (va & 0xFFFu);
        UINT64 k = (n < room) ? n : room;

        memcpy(P2V(phys), src, (UINTN)k);

        va += k;
        src += k;
        n -= k;
    }

    return TRUE;
}

/*
 * Можно ли ядру читать (или писать) [addr, addr+len) от имени
 * программы: всё в нижней половине, всё отображено с битом U (и W
 * для записи). Иначе программа могла бы попросить ядро прочитать
 * СВОЮ память (ядра) - классическая дыра.
 */
BOOLEAN uptr_ok(KPROC *p, UINT64 addr, UINT64 len, BOOLEAN write)
{
    if (p == NULL)
        return FALSE;

    if (len == 0)
        return TRUE;

    if (addr + len < addr || addr + len > 0x0000800000000000ull)
        return FALSE;

    for (UINT64 va = addr & ~0xFFFull; va < addr + len; va += 4096u) {

        UINT64 *t = (UINT64 *)P2V(p->pml4);
        UINT64 e = 0;

        for (INTN lvl = 3; lvl >= 0; lvl--) {

            e = t[(va >> (12u + 9u * (UINTN)lvl)) & 511u];

            if (!(e & UPTE_P) || !(e & UPTE_U) || (write && !(e & UPTE_W)))
                return FALSE;

            if (lvl > 0)
                t = (UINT64 *)P2V(e & UPTE_ADDR);
        }
    }

    return TRUE;
}

/* Освободить всю нижнюю половину и саму PML4 */
static void uvm_free(UINT64 pml4)
{
    UINT64 *l4 = (UINT64 *)P2V(pml4);

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

                for (UINTN m = 0; m < 512; m++)
                    if (l1[m] & UPTE_P)
                        pmm_free_pages(l1[m] & UPTE_ADDR, 1);

                pmm_free_pages(l2[k] & UPTE_ADDR, 1);
            }

            pmm_free_pages(l3[j] & UPTE_ADDR, 1);
        }

        pmm_free_pages(l4[i] & UPTE_ADDR, 1);
    }

    pmm_free_pages(pml4, 1);
}


/* ================================================================
 * Переключение потоков: стек ядра для прерываний и syscall, CR3
 * ================================================================ */

/*
 * Зовётся планировщиком (sched.c) перед тем, как отдать процессор
 * потоку next (прерывания запрещены):
 *   * TSS.rsp0 и g_sc_kstack - вершина стека ядра next: сюда
 *     процессор положит рамку прерывания, пришедшего, пока работает
 *     программа, и сюда же перейдёт вход syscall;
 *   * CR3 - таблицы страниц: у потока программы свои, у потоков
 *     ядра - общие ядерные.
 */
void proc_switch_hook(KTHREAD *next)
{
    g_ktss.rsp0 = next->stack_top;
    g_sc_kstack = next->stack_top;

    UINT64 want = next->cr3 ? next->cr3 : g_vmm_pml4_phys;

    if (next->cr3)
        uvm_sync_kernel_half(next->cr3);

    if (want != 0 && (read_cr3() & UPTE_ADDR) != want)
        __asm__ __volatile__("mov %0, %%cr3" : : "r"(want) : "memory");
}


/* ================================================================
 * Загрузка ELF
 * ================================================================ */

/* Заголовки ELF64 (по спецификации System V ABI) */
typedef struct {
    UINT8  ident[16];
    UINT16 type, machine;
    UINT32 version;
    UINT64 entry, phoff, shoff;
    UINT32 flags;
    UINT16 ehsize, phentsize, phnum, shentsize, shnum, shstrndx;
} ELF64_EHDR;

typedef struct {
    UINT32 type, flags;
    UINT64 offset, vaddr, paddr, filesz, memsz, align;
} ELF64_PHDR;

#define PT_LOAD  1
#define PF_X     1
#define PF_W     2

static INTN proc_load_elf(KPROC *p, const UINT8 *img, UINTN size, const char **why)
{
    const ELF64_EHDR *h = (const ELF64_EHDR *)img;

    if (size < sizeof(*h) || img[0] != 0x7F || img[1] != 'E' || img[2] != 'L' || img[3] != 'F') {
        *why = "not an ELF file";
        return VFS_EINVAL;
    }

    if (img[4] != 2 || img[5] != 1 || h->machine != 62) {
        *why = "not a 64-bit x86 program";
        return VFS_EINVAL;
    }

    if (h->type != 2) {
        *why = "not an executable (ET_EXEC) - link it statically, see user/user.ld";
        return VFS_EINVAL;
    }

    if (h->phentsize != sizeof(ELF64_PHDR) || h->phoff > size ||
        (UINT64)h->phnum * sizeof(ELF64_PHDR) > size - h->phoff) {
        *why = "broken program headers";
        return VFS_EINVAL;
    }

    UINT64 top = 0;
    UINTN loads = 0;

    for (UINT16 i = 0; i < h->phnum; i++) {

        const ELF64_PHDR *ph = (const ELF64_PHDR *)(img + h->phoff + (UINTN)i * sizeof(ELF64_PHDR));

        if (ph->type != PT_LOAD || ph->memsz == 0)
            continue;

        if (ph->filesz > ph->memsz || ph->offset > size || ph->filesz > size - ph->offset ||
            ph->vaddr < 0x1000u || ph->vaddr + ph->memsz < ph->vaddr ||
            ph->vaddr + ph->memsz > MYOS_USER_LIMIT) {
            *why = "a segment is outside the program area";
            return VFS_EINVAL;
        }

        BOOLEAN w = (ph->flags & PF_W) != 0;
        BOOLEAN x = (ph->flags & PF_X) != 0;

        for (UINT64 va = ph->vaddr & ~0xFFFull; va < ph->vaddr + ph->memsz; va += 4096u)
            if (!uvm_map_new(p, va, w, x)) {
                *why = "out of memory";
                return VFS_ENOSPC;
            }

        /* содержимое из файла; остаток (memsz > filesz - это .bss)
           уже нулевой - страницы выдаются обнулёнными */
        if (!uvm_copy_to(p, ph->vaddr, img + ph->offset, ph->filesz)) {
            *why = "copy failed";
            return VFS_EIO;
        }

        if (ph->vaddr + ph->memsz > top)
            top = ph->vaddr + ph->memsz;

        loads++;
    }

    if (loads == 0 || h->entry < MYOS_USER_BASE / 4u || h->entry >= top) {
        *why = "no code to run";
        return VFS_EINVAL;
    }

    p->entry = h->entry;
    p->brk_base = (top + 4095u) & ~0xFFFull;
    p->brk = p->brk_base;

    return VFS_OK;
}

/*
 * Стек программы и аргументы - так, как их ждёт _start по
 * соглашению System V (так же делает Linux):
 *   [rsp]      argc
 *   [rsp+8]    argv[0] ... argv[argc-1], NULL
 *              envp: NULL
 *   выше       сами строки
 */
static BOOLEAN proc_setup_stack(KPROC *p, const char *args)
{
    UINT64 top = MYOS_USER_STACK_TOP;
    UINT64 bottom = top - MYOS_USER_STACK_SIZE;

    for (UINT64 va = bottom; va < top; va += 4096u)
        if (!uvm_map_new(p, va, TRUE, FALSE))
            return FALSE;

    /* строки: argv[0] = имя, дальше - слова args */
    char buf[512];
    UINTN n = 0;
    UINT64 offs[32];
    UINTN argc = 0;

    offs[argc++] = 0;
    for (UINTN i = 0; p->name[i] && n + 1 < sizeof(buf); i++)
        buf[n++] = p->name[i];
    buf[n++] = '\0';

    const char *a = args ? args : "";

    while (*a && argc < 31 && n + 2 < sizeof(buf)) {

        while (*a == ' ')
            a++;

        if (!*a)
            break;

        offs[argc++] = n;

        if (*a == '"') {
            a++;
            while (*a && *a != '"' && n + 2 < sizeof(buf))
                buf[n++] = *a++;
            if (*a == '"')
                a++;
        } else {
            while (*a && *a != ' ' && n + 2 < sizeof(buf))
                buf[n++] = *a++;
        }

        buf[n++] = '\0';
    }

    UINT64 strs = (top - n) & ~0xFull;

    if (!uvm_copy_to(p, strs, (const UINT8 *)buf, n))
        return FALSE;

    /* argc, argv[], NULL, envp NULL - rsp должен быть кратен 16 */
    UINT64 words = 1u + argc + 1u + 1u;
    UINT64 sp = (strs - words * 8u) & ~0xFull;
    UINT64 vec[40];
    UINTN k = 0;

    vec[k++] = argc;
    for (UINTN i = 0; i < argc; i++)
        vec[k++] = strs + offs[i];
    vec[k++] = 0;
    vec[k++] = 0;

    if (!uvm_copy_to(p, sp, (const UINT8 *)vec, k * 8u))
        return FALSE;

    p->user_rsp = sp;

    return TRUE;
}


/* ================================================================
 * Вход в программу
 * ================================================================ */

/*
 * Прыжок в ring 3: iretq берёт из стека SS, RSP, RFLAGS, CS, RIP.
 * CS = 0x2B (код программ, DPL 3), SS = 0x23 (данные программ).
 * RFLAGS = 0x202: IF=1 (прерывания работают - иначе таймер не смог
 * бы отнять процессор у зациклившейся программы). Все регистры
 * обнуляем: программе не должно достаться ничего от ядра.
 */
static void __attribute__((noreturn)) kx_enter_user(UINT64 entry, UINT64 rsp)
{
    __asm__ __volatile__(
        "cli\n"
        "pushq $0x23\n"
        "pushq %1\n"
        "pushq $0x202\n"
        "pushq $0x2B\n"
        "pushq %0\n"
        "xorl %%eax, %%eax\n"
        "xorl %%ebx, %%ebx\n"
        "xorl %%ecx, %%ecx\n"
        "xorl %%edx, %%edx\n"
        "xorl %%esi, %%esi\n"
        "xorl %%edi, %%edi\n"
        "xorl %%ebp, %%ebp\n"
        "xorl %%r8d, %%r8d\n"
        "xorl %%r9d, %%r9d\n"
        "xorl %%r10d, %%r10d\n"
        "xorl %%r11d, %%r11d\n"
        "xorl %%r12d, %%r12d\n"
        "xorl %%r13d, %%r13d\n"
        "xorl %%r14d, %%r14d\n"
        "xorl %%r15d, %%r15d\n"
        "iretq\n"
        :
        : "r"(entry), "r"(rsp)
        : "memory"
    );

    __builtin_unreachable();
}

/* Поток программы: поставить свои таблицы страниц и войти в неё */
static void proc_thread_main(void *arg)
{
    KPROC *p = (KPROC *)arg;
    UINT64 fl = kx_irq_save();

    g_kcur->cr3 = p->pml4;
    g_kcur->proc = p;
    proc_switch_hook(g_kcur);

    kx_irq_restore(fl);

    klog("proc: pid %u '%s' enters ring 3 at 0x%llx\n", p->pid, p->name, p->entry);

    kx_enter_user(p->entry, p->user_rsp);
}


/* ================================================================
 * Запуск, ожидание, завершение
 * ================================================================ */

/* Короткое имя из пути: "/bin/hello" -> "hello" */
static void proc_name_from(const char *path, char *out)
{
    const char *b = path;

    for (const char *s = path; *s; s++)
        if (*s == '/')
            b = s + 1;

    UINTN i = 0;

    for (; b[i] && b[i] != '.' && i + 1 < KT_NAME_LEN; i++) {
        char c = b[i];
        if (c >= 'A' && c <= 'Z')
            c = (char)(c - 'A' + 'a');
        out[i] = c;
    }

    out[i] = '\0';
}

/* Освободить всё, что осталось от завершившегося процесса */
void proc_reap(KPROC *p)
{
    if (p == NULL || !p->used)
        return;

    for (UINTN i = 0; i < PROC_FDS; i++)
        if (p->fds[i] >= 0) {
            vfs_close(p->fds[i]);
            p->fds[i] = -1;
        }

    if (p->pml4 != 0)
        uvm_free(p->pml4);

    p->pml4 = 0;
    p->used = FALSE;
}

/*
 * Запустить программу из файла path с аргументами args. io - куда
 * она пишет (консоль шелла или окно GUI). NULL - не вышло, *err -
 * код ошибки VFS (и сообщение в журнале).
 */
KPROC *proc_spawn(const char *path, const char *args, UINT32 io, INTN *err)
{
    *err = VFS_OK;

    if (!g_sched_on) {
        *err = VFS_ENOSYS;
        return NULL;
    }

    VFS_DIRENT st;
    INTN r = vfs_stat(path, &st);

    if (r != VFS_OK) {
        *err = r;
        return NULL;
    }

    if (st.node.is_dir) {
        *err = VFS_EISDIR;
        return NULL;
    }

    if (st.node.size == 0 || st.node.size > MAX_ELF_SIZE) {
        *err = VFS_EINVAL;
        return NULL;
    }

    UINT8 *img = (UINT8 *)kmalloc((UINTN)st.node.size);

    if (img == NULL) {
        *err = VFS_ENOSPC;
        return NULL;
    }

    UINTN got = 0;

    r = vfs_read_file(path, img, (UINTN)st.node.size, &got);

    if (r != VFS_OK || got != st.node.size) {
        kfree(img);
        *err = (r != VFS_OK) ? r : VFS_EIO;
        return NULL;
    }

    /* слот */
    UINT64 fl = kx_irq_save();
    KPROC *p = NULL;

    for (UINTN i = 0; i < PROC_MAX; i++)
        if (!g_procs[i].used) {
            p = &g_procs[i];
            break;
        }

    if (p != NULL) {
        raw_zero_mem((volatile UINT8 *)p, sizeof(*p));
        p->used = TRUE;
        p->pid = g_next_pid++;
    }

    kx_irq_restore(fl);

    if (p == NULL) {
        kfree(img);
        *err = VFS_EMFILE;
        return NULL;
    }

    for (UINTN i = 0; i < PROC_FDS; i++)
        p->fds[i] = -1;

    proc_name_from(path, p->name);
    ksnprintf(p->path, sizeof(p->path), "%s", path);
    ksnprintf(p->cwd, sizeof(p->cwd), "%s", g_cwd);
    p->io = io;
    p->gui_line = (io == PROC_IO_GUI) ? g_proc_gui_sink : NULL;
    p->started_ms = g_kticks;

    /* своя PML4: нижняя половина пустая, верхняя - ядро */
    p->pml4 = pmm_alloc_zeroed(1, 0);

    const char *why = "out of memory";

    if (p->pml4 == 0) {
        r = VFS_ENOSPC;
    } else {
        uvm_sync_kernel_half(p->pml4);
        r = proc_load_elf(p, img, got, &why);
        if (r == VFS_OK && !proc_setup_stack(p, args)) {
            r = VFS_ENOSPC;
            why = "out of memory for the stack";
        }
    }

    kfree(img);

    if (r != VFS_OK) {
        klog("proc: cannot start %s: %s\n", path, why);
        g_proc_last_error = why;
        ksnprintf(p->why, sizeof(p->why), "%s", why);
        proc_reap(p);
        *err = r;
        return NULL;
    }

    p->thread = kthread_create(p->name, proc_thread_main, p, 16);

    if (p->thread == NULL) {
        proc_reap(p);
        *err = VFS_ENOSPC;
        return NULL;
    }

    p->tid = p->thread->tid;

    klog("proc: started pid %u '%s' (%s), %llu KiB of memory\n",
         p->pid, p->name, path, p->pages * 4u);

    return p;
}

/*
 * Дождаться конца программы, освободить её память и вернуть код
 * выхода (-1 - её завершило ядро: ошибка или Ctrl+C).
 */
INT64 proc_wait(KPROC *p)
{
    KTHREAD *t = p->thread;
    UINT32 tid = p->tid;
    UINT64 fl = kx_irq_save();

    while (kthread_alive(t, tid))
        sched_block(t, "program", 0);

    kx_irq_restore(fl);

    INT64 code = p->exit_code;

    proc_reap(p);

    return code;
}

/* Завершить текущую программу (из системного вызова или обработчика
   исключения); сюда не возвращаются */
void proc_exit_current(INT64 code)
{
    KPROC *p = g_kcur->proc;

    if (p != NULL) {

        /* остаток вывода в окно GUI */
        if (p->io == PROC_IO_GUI && p->outlen > 0 && p->gui_line) {
            p->outline[p->outlen] = '\0';
            p->gui_line(p->outline);
            p->outlen = 0;
        }

        p->exit_code = code;
        p->exited = TRUE;

        if (g_fg_proc == p)
            g_fg_proc = NULL;

        klog("proc: pid %u '%s' exited with code %lld%s%s\n", p->pid, p->name, code,
             p->why[0] ? " - " : "", p->why);
    }

    /* память освободит тот, кто ждёт (proc_wait): мы ещё стоим на
       этих таблицах страниц */
    kthread_exit();
}

/* Исключение в программе (из cpu.c) */
void proc_fault(const char *what, UINT64 rip)
{
    KPROC *p = g_kcur->proc;

    if (p == NULL)
        return;

    ksnprintf(p->why, sizeof(p->why), "%s (at address 0x%llx)", what, rip);
    p->killed = TRUE;
}

/* Конец прерывания/вызова, дальше - программа: а её не завершают? */
void proc_check_kill(void)
{
    KPROC *p = g_kcur->proc;

    if (p != NULL && p->killed)
        proc_exit_current(-1);
}

void proc_kill(KPROC *p)
{
    if (p != NULL && p->used && !p->exited) {
        if (p->why[0] == '\0')
            ksnprintf(p->why, sizeof(p->why), "stopped by the user");
        p->killed = TRUE;
        /* ждёт ввода или спит - разбудить, чтобы увидел */
        sched_wake_all(p);
    }
}

/* Ctrl+C (из обработчика клавиатуры) */
void proc_ctrl_c(void)
{
    KPROC *p = g_fg_proc;

    if (p != NULL) {
        ksnprintf(p->why, sizeof(p->why), "stopped with Ctrl+C");
        proc_kill(p);
    }
}

/* Строка, набранная в окне GUI, - программе на ввод */
BOOLEAN proc_gui_input(KPROC *p, const char *line)
{
    if (p == NULL || p->exited)
        return FALSE;

    UINT64 fl = kx_irq_save();
    UINTN n = 0;

    for (; line[n] && n + 1 < PROC_IN_MAX - 1; n++)
        p->inbuf[n] = line[n];

    p->inbuf[n++] = '\n';
    p->inlen = n;
    p->inready = TRUE;

    kx_irq_restore(fl);

    sched_wake_all(p);

    return TRUE;
}


/* ================================================================
 * Поиск программы и запуск из шелла
 * ================================================================ */

/*
 * Имя команды -> путь к программе: если в имени есть '/', это уже
 * путь; иначе ищем /bin/<имя>. TRUE - нашли.
 */
BOOLEAN proc_find_program(const char *name, char *path, UINTN cap)
{
    VFS_DIRENT st;
    BOOLEAN has_slash = FALSE;

    for (const char *s = name; *s; s++)
        if (*s == '/')
            has_slash = TRUE;

    if (has_slash) {
        ksnprintf(path, cap, "%s", name);
        return vfs_stat(path, &st) == VFS_OK && !st.node.is_dir;
    }

    ksnprintf(path, cap, "/bin/%s", name);

    if (vfs_stat(path, &st) == VFS_OK && !st.node.is_dir)
        return TRUE;

    /* программа, лежащая в текущей папке, - как "./имя" */
    ksnprintf(path, cap, "%s", name);

    if (vfs_stat(path, &st) == VFS_OK && !st.node.is_dir) {
        /* только если это ELF - иначе "cat notes.txt" тоже
           запустился бы */
        char magic[4];
        UINTN got = 0;
        vfs_read_file(path, magic, 4, &got);
        return got == 4 && magic[0] == 0x7F && magic[1] == 'E' &&
               magic[2] == 'L' && magic[3] == 'F';
    }

    return FALSE;
}

/* Запустить программу в консоли шелла и дождаться её */
void kernel_cmd_run(EFI_SYSTEM_TABLE *st, const char *path, const char *args, BOOLEAN quiet)
{
    SIMPLE_TEXT_OUTPUT_INTERFACE *out = st->ConOut;
    INTN err;

    /* клавиши, нажатые до запуска (Enter самой команды), программе
       не нужны */
    {
        EFI_INPUT_KEY k;
        kx_lock();
        while (kbd_dequeue(&k)) { }
        kx_unlock();
    }

    KPROC *p = proc_spawn(path, args, PROC_IO_CONSOLE, &err);

    if (p == NULL) {
        kprintf(out, "Cannot run %s: %s%s%s\n", path, vfs_strerror(err),
                g_proc_last_error[0] ? " - " : "", g_proc_last_error);
        g_proc_last_error = "";
        return;
    }

    g_fg_proc = p;

    INT64 code = proc_wait(p);

    g_fg_proc = NULL;

    kcon_flush();

    if (code == -1 && p->why[0]) {
        set_color(out, 0x0C);
        kprintf(out, "\n*** %s (pid %u) was stopped: %s\n", p->name, p->pid, p->why);
        set_color(out, g_color);
        print(out, "    MyOS keeps running - only the program was closed.\n");
    } else if (!quiet || code != 0) {
        kprintf(out, "[%s exited with code %lld]\n", p->name, code);
    }
}

/*
 * Шелл: команда не встроенная - может быть, это программа из /bin
 * (или путь к файлу программы). TRUE - запустили.
 */
BOOLEAN proc_shell_try(EFI_SYSTEM_TABLE *st, const CHAR16 *line)
{
    char buf[LINE_MAX];
    UINTN n = 0;

    for (; line[n] && n + 1 < LINE_MAX; n++)
        buf[n] = (line[n] < 128) ? (char)line[n] : '?';

    buf[n] = '\0';

    char *s = buf;

    while (*s == ' ')
        s++;

    if (*s == '\0')
        return FALSE;

    /* "run путь аргументы" - явно */
    BOOLEAN explicit_run = (s[0] == 'r' && s[1] == 'u' && s[2] == 'n' && s[3] == ' ');

    if (explicit_run) {
        s += 4;
        while (*s == ' ')
            s++;
    }

    char *e = s;

    while (*e && *e != ' ')
        e++;

    char *args = e;

    if (*e) {
        *e = '\0';
        args = e + 1;
    }

    char path[VFS_PATH_MAX];

    if (!proc_find_program(s, path, sizeof(path))) {
        if (explicit_run) {
            kprintf(st->ConOut, "No such program: %s (programs are in /bin: ls /bin)\n", s);
            return TRUE;
        }
        return FALSE;
    }

    kernel_cmd_run(st, path, args, TRUE);

    return TRUE;
}


/* ================================================================
 * Запуск подсистемы
 * ================================================================ */

extern void kx_syscall_entry(void);

void proc_init(SIMPLE_TEXT_OUTPUT_INTERFACE *out)
{
    /*
     * Инструкция syscall настраивается регистрами MSR:
     *   EFER.SCE (бит 0)  - разрешить syscall/sysret;
     *   STAR              - селекторы: [47:32] ядро (0x08, стек 0x10),
     *                       [63:48] база программы 0x18 (sysret
     *                       возьмёт код 0x18+16=0x28 и стек 0x18+8=0x20);
     *   LSTAR             - куда прыгать (kx_syscall_entry, syscall.c);
     *   FMASK             - какие флаги сбросить при входе: IF
     *                       (прерывания - пока не перешли на стек
     *                       ядра), DF, TF, AC.
     */
    kx_wrmsr(0xC0000080u, kx_rdmsr(0xC0000080u) | 1u);
    kx_wrmsr(0xC0000081u, (0x18ull << 48) | (0x08ull << 32));
    kx_wrmsr(0xC0000082u, (UINT64)(UINTN)kx_syscall_entry);
    kx_wrmsr(0xC0000084u, 0x200u | 0x400u | 0x100u | 0x40000u);

    g_sc_kstack = g_kcur->stack_top;
    g_ktss.rsp0 = g_kcur->stack_top;

    /* SMAP (CR4 бит 21) запрещает ядру трогать память программ без
       особых инструкций stac/clac. Наши системные вызовы читают
       буферы программы напрямую (после проверки uptr_ok), поэтому,
       если прошивка оставила SMAP включённым, - выключаем. SMEP (бит
       20, "ядру нельзя исполнять код программ") не мешает - пусть. */
    {
        UINT64 cr4;
        __asm__ __volatile__("mov %%cr4, %0" : "=r"(cr4));
        if (cr4 & (1ull << 21))
            __asm__ __volatile__("mov %0, %%cr4" : : "r"(cr4 & ~(1ull << 21)) : "memory");
    }

    binfs_mount();

    print(out, "  Programs run in ring 3 with their own memory; system calls via 'syscall'.\n");
    print(out, "  /bin - built-in programs (ls /bin); a program file can also live on a disk.\n");
}
