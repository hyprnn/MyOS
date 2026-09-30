/*
 * kernel/syscall.c - системные вызовы: как программа просит ядро
 * (этап 6). Часть MyOS; номера вызовов - sysnum.h.
 *
 * Программа кладёт номер вызова в rax, аргументы - в rdi, rsi, rdx,
 * r10, r8, r9 и выполняет инструкцию syscall. Процессор при этом:
 *   * переключается в ring 0 и прыгает по адресу из MSR LSTAR -
 *     сюда, в kx_syscall_entry;
 *   * запоминает адрес возврата в rcx и флаги в r11;
 *   * сбрасывает флаги из MSR FMASK (в том числе IF - прерывания
 *     выключены);
 *   * НО стек не меняет: rsp всё ещё указывает в память программы.
 * Поэтому первым делом переходим на стек ядра текущего потока
 * (g_sc_kstack - его ставит планировщик, proc.c), сохраняем
 * регистры и состояние SSE (C-код ядра может испортить xmm-регистры,
 * а программа вправе ждать, что они не изменятся), включаем
 * прерывания и зовём kx_syscall_dispatch. Обратно - инструкцией
 * sysretq: rip из rcx, флаги из r11, ring 3.
 *
 * Каждый указатель от программы проверяется (uptr_ok): лежит ли
 * он в её памяти. Иначе программа могла бы попросить ядро прочитать
 * или испортить память самого ядра.
 */
#include "myos.h"

/* Стек ядра текущего потока и временное место для rsp программы -
   у каждого ядра процессора свои: поля sc_kstack (%gs:8) и
   sc_user_rsp (%gs:16) структуры KX_CPU (myos.h). swapgs на входе
   делает GS "ядерным", на выходе - возвращает программе. */

/*
 * Рамка, которую строит kx_syscall_entry (от младших адресов):
 *   0 r15, 1 r14, 2 r13, 3 r12, 4 rbp, 5 rbx, 6 r9, 7 r8, 8 r10,
 *   9 rdx, 10 rsi, 11 rdi, 12 rax, 13 rcx (rip), 14 r11 (флаги),
 *   15 rsp программы
 */
#define SF_R9   6
#define SF_R8   7
#define SF_R10  8
#define SF_RDX  9
#define SF_RSI 10
#define SF_RDI 11
#define SF_RAX 12

__asm__(
    ".text\n"
    ".globl kx_syscall_entry\n"
    ".hidden kx_syscall_entry\n"
    "kx_syscall_entry:\n"
    "  swapgs\n"
    "  movq %rsp, %gs:16\n"
    "  movq %gs:8, %rsp\n"
    "  pushq %gs:16\n"
    "  pushq %r11\n"
    "  pushq %rcx\n"
    "  pushq %rax\n"
    "  pushq %rdi\n"
    "  pushq %rsi\n"
    "  pushq %rdx\n"
    "  pushq %r10\n"
    "  pushq %r8\n"
    "  pushq %r9\n"
    "  pushq %rbx\n"
    "  pushq %rbp\n"
    "  pushq %r12\n"
    "  pushq %r13\n"
    "  pushq %r14\n"
    "  pushq %r15\n"
    "  movq %rsp, %rbx\n"
    "  subq $512, %rsp\n"
    "  andq $-16, %rsp\n"
    "  fxsave (%rsp)\n"
    "  sti\n"
    "  movq %rbx, %rdi\n"
    "  cld\n"
    "  call kx_syscall_dispatch\n"
    "  cli\n"
    "  fxrstor (%rsp)\n"
    "  movq %rbx, %rsp\n"
    "  movq %rax, 96(%rsp)\n"
    "  popq %r15\n"
    "  popq %r14\n"
    "  popq %r13\n"
    "  popq %r12\n"
    "  popq %rbp\n"
    "  popq %rbx\n"
    "  popq %r9\n"
    "  popq %r8\n"
    "  popq %r10\n"
    "  popq %rdx\n"
    "  popq %rsi\n"
    "  popq %rdi\n"
    "  popq %rax\n"
    "  popq %rcx\n"
    "  popq %r11\n"
    "  popq %rsp\n"
    "  swapgs\n"
    "  sysretq\n"
);


/* ================================================================
 * Помощники
 * ================================================================ */

/* Строка из памяти программы (с проверкой каждой страницы) */
static INTN copy_in_str(KPROC *p, UINT64 uaddr, char *dst, UINTN cap)
{
    for (UINTN i = 0; i < cap; i++) {

        if (i == 0 || ((uaddr + i) & 0xFFFu) == 0)
            if (!uptr_ok(p, uaddr + i, 1, FALSE))
                return MYOS_EFAULT;

        char c = *(volatile char *)(UINTN)(uaddr + i);

        dst[i] = c;

        if (c == '\0')
            return VFS_OK;
    }

    return VFS_EINVAL;             /* слишком длинная */
}

/* Путь от программы -> полный (относительный - от её папки) */
static INTN user_path(KPROC *p, UINT64 upath, char *out, UINTN cap)
{
    char tmp[VFS_PATH_MAX];
    INTN r = copy_in_str(p, upath, tmp, sizeof(tmp));

    if (r != VFS_OK)
        return r;

    if (tmp[0] == '/')
        ksnprintf(out, cap, "%s", tmp);
    else
        ksnprintf(out, cap, "%s/%s", p->cwd, tmp);

    return VFS_OK;
}

/* Вывод программы на экран (fd 1 и 2) */
static void proc_out(KPROC *p, const char *s, UINTN n)
{
    if (p->io == PROC_IO_GUI) {

        /* графическая программа без терминала (часы, Блокнот...):
           её редкие сообщения - строками в журнал ядра (COM1) */
        for (UINTN i = 0; i < n; i++) {

            char c = s[i];

            if (c == '\r')
                continue;

            if (c != '\n')
                p->outline[p->outlen++] = c;

            if (c == '\n' || p->outlen + 1 >= sizeof(p->outline)) {
                p->outline[p->outlen] = '\0';
                klog("%s: %s\n", p->name, p->outline);
                p->outlen = 0;
            }
        }

        return;
    }

    /* консоль шелла: кусками, как обычную строку */
    char buf[257];

    while (n > 0) {

        UINTN k = (n < 256) ? n : 256;

        for (UINTN i = 0; i < k; i++) {
            char c = s[i];
            buf[i] = (c == 0) ? ' ' : c;
        }

        buf[k] = '\0';
        print(g_st->ConOut, buf);

        s += k;
        n -= k;
    }

    kcon_flush();
}

/* Строка с клавиатуры (консоль): с эхом и забоем, до Enter */
static INTN proc_read_console(KPROC *p, char *dst, UINTN n)
{
    SIMPLE_TEXT_OUTPUT_INTERFACE *out = g_st->ConOut;
    static char line[PROC_IN_MAX];
    UINTN len = 0;

    for (;;) {

        if (p->killed)
            return 0;

        EFI_INPUT_KEY key;

        if (g_st->ConIn->ReadKeyStroke(g_st->ConIn, &key) != EFI_SUCCESS) {
            sched_sleep_ms(10);
            continue;
        }

        if (key.UnicodeChar == CHAR_CARRIAGE_RETURN) {
            print(out, "\n");
            line[len++] = '\n';
            break;
        }

        if (key.UnicodeChar == CHAR_BACKSPACE) {
            if (len > 0) {
                len--;
                print(out, "\b \b");
                kcon_flush();
            }
            continue;
        }

        if (key.UnicodeChar >= 32 && key.UnicodeChar < 127 && len + 2 < sizeof(line)) {
            char e[2] = { (char)key.UnicodeChar, 0 };
            line[len++] = e[0];
            print(out, e);
            kcon_flush();
        }
    }

    if (len > n)
        len = n;          /* не влезло - остаток теряется */

    for (UINTN i = 0; i < len; i++)
        dst[i] = line[i];

    return (INTN)len;
}

static INTN fd_kernel(KPROC *p, INT64 fd)
{
    if (fd < 3 || fd >= 3 + PROC_FDS)
        return -1;

    return p->fds[fd - 3];
}

/*
 * poll: какие из fd программы готовы. Файлы и экран готовы всегда
 * (чтение с диска не "ждёт" в смысле poll), клавиатура - никогда (её
 * программы читают getkey/read), сокеты - как скажет sock_poll.
 * Ждём событий сети (sock_poll_wait) кусками по 100 мс - чтобы
 * заметить Ctrl+C и срок.
 */
#define POLL_MAX 64

typedef struct {
    KPROC              *p;
    struct myos_pollfd *fds;
    UINTN               n;
    INTN                count;
} POLL_CTX;

static BOOLEAN poll_check(void *ctx)
{
    POLL_CTX *c = (POLL_CTX *)ctx;

    c->count = 0;

    for (UINTN i = 0; i < c->n; i++) {

        struct myos_pollfd *f = &c->fds[i];
        UINT32 ev;

        if (f->fd < 0)
            ev = 0;
        else if (f->fd == 1 || f->fd == 2)
            ev = MYOS_POLLOUT;
        else if (f->fd == 0)
            ev = 0;
        else {
            INTN kfd = fd_kernel(c->p, f->fd);
            if (kfd < 0)
                ev = MYOS_POLLNVAL;
            else if (kfd & PROC_FD_SOCK)
                ev = sock_poll(kfd & ~PROC_FD_SOCK);
            else
                ev = MYOS_POLLIN | MYOS_POLLOUT;
        }

        /* ошибки и закрытие сообщаются всегда, остальное - если спросили */
        f->revents = (short)(ev & ((UINT32)(UINT16)f->events |
                                   MYOS_POLLERR | MYOS_POLLHUP | MYOS_POLLNVAL));

        if (f->revents)
            c->count++;
    }

    return c->count > 0;
}

static INT64 sys_poll(KPROC *p, UINT64 ufds, UINT64 n, INT64 timeout_ms)
{
    struct myos_pollfd fds[POLL_MAX];
    POLL_CTX c = { p, fds, (UINTN)n, 0 };

    if (n > POLL_MAX)
        return MYOS_EINVAL;
    if (n > 0 && !uptr_ok(p, ufds, n * sizeof(fds[0]), TRUE))
        return MYOS_EFAULT;

    memcpy(fds, (const void *)(UINTN)ufds, n * sizeof(fds[0]));

    UINT64 deadline = (timeout_ms >= 0) ? g_kticks + (UINT64)timeout_ms : 0;

    for (;;) {

        UINT64 slice = 100;

        if (timeout_ms == 0) {
            poll_check(&c);
            break;
        }

        if (timeout_ms > 0) {
            if (g_kticks >= deadline) {
                poll_check(&c);
                break;
            }
            if (deadline - g_kticks < slice)
                slice = deadline - g_kticks;
        }

        if (sock_poll_wait(poll_check, &c, slice))
            break;

        if (p->killed)
            return MYOS_EINTR;
    }

    memcpy((void *)(UINTN)ufds, fds, n * sizeof(fds[0]));
    return c.count;
}

typedef struct {
    UINTN want, idx;
    struct myos_dirent *out;
    BOOLEAN found;
} RD_CTX;

static INTN rd_cb(void *ctx, const VFS_DIRENT *e)
{
    RD_CTX *c = (RD_CTX *)ctx;

    if (c->idx++ != c->want)
        return 0;

    UINTN k = 0;

    for (; e->name[k] && k + 1 < sizeof(c->out->name); k++)
        c->out->name[k] = e->name[k];

    c->out->name[k] = '\0';
    c->out->size = e->node.size;
    c->out->is_dir = e->node.is_dir ? 1u : 0u;
    c->out->wdate = e->node.wdate;
    c->out->wtime = e->node.wtime;
    c->found = TRUE;

    return 1;       /* дальше не нужно */
}


/* ================================================================
 * Сеть (этап 8): сокеты - в таблице fd программы с флагом
 * PROC_FD_SOCK; сама работа - в net/socket.c
 * ================================================================ */

static INTN sock_of(KPROC *p, UINT64 fd)
{
    INTN kfd = fd_kernel(p, (INT64)fd);

    if (kfd < 0 || !(kfd & PROC_FD_SOCK))
        return -1;

    return kfd & ~PROC_FD_SOCK;
}

static INTN fd_slot(KPROC *p)
{
    for (UINTN i = 0; i < PROC_FDS; i++)
        if (p->fds[i] < 0)
            return (INTN)i;

    return -1;
}

static INT64 sys_net(KPROC *p, UINT64 nr, UINT64 a1, UINT64 a2, UINT64 a3, UINT64 a4)
{
    struct myos_sockaddr sa;
    INTN s;

    switch (nr) {

    case SYS_SOCKET: {
        INTN slot = fd_slot(p);
        if (slot < 0)
            return MYOS_EMFILE;
        s = sock_create((UINT32)a1, p->pid);
        if (s < 0)
            return s;
        p->fds[slot] = s | PROC_FD_SOCK;
        return 3 + slot;
    }

    case SYS_CONNECT:
    case SYS_BIND:
        if ((s = sock_of(p, a1)) < 0)
            return MYOS_EBADF;
        if (!uptr_ok(p, a2, sizeof(sa), FALSE))
            return MYOS_EFAULT;
        memcpy(&sa, (const void *)(UINTN)a2, sizeof(sa));
        return (nr == SYS_CONNECT) ? sock_connect(s, sa.ip, sa.port)
                                   : sock_bind(s, sa.ip, sa.port);

    case SYS_LISTEN:
        if ((s = sock_of(p, a1)) < 0)
            return MYOS_EBADF;
        return sock_listen(s, (UINT32)a2);

    case SYS_ACCEPT: {
        UINT32 ip = 0;
        UINT16 port = 0;
        INTN slot = fd_slot(p);
        if ((s = sock_of(p, a1)) < 0)
            return MYOS_EBADF;
        if (a2 && !uptr_ok(p, a2, sizeof(sa), TRUE))
            return MYOS_EFAULT;
        if (slot < 0)
            return MYOS_EMFILE;
        INTN ns = sock_accept(s, p->pid, &ip, &port);
        if (ns < 0)
            return ns;
        p->fds[slot] = ns | PROC_FD_SOCK;
        if (a2) {
            memset(&sa, 0, sizeof(sa));
            sa.ip = ip;
            sa.port = port;
            memcpy((void *)(UINTN)a2, &sa, sizeof(sa));
        }
        return 3 + slot;
    }

    case SYS_SENDTO:
        if ((s = sock_of(p, a1)) < 0)
            return MYOS_EBADF;
        if (!uptr_ok(p, a2, a3, FALSE) || (a4 && !uptr_ok(p, a4, sizeof(sa), FALSE)))
            return MYOS_EFAULT;
        if (a4) {
            memcpy(&sa, (const void *)(UINTN)a4, sizeof(sa));
            return sock_sendto(s, (const void *)(UINTN)a2, (UINTN)a3, sa.ip, sa.port);
        }
        return sock_send(s, (const void *)(UINTN)a2, (UINTN)a3);

    case SYS_RECVFROM: {
        UINT32 ip = 0;
        UINT16 port = 0;
        UINT8 ttl = 0;
        if ((s = sock_of(p, a1)) < 0)
            return MYOS_EBADF;
        if (!uptr_ok(p, a2, a3, TRUE) || (a4 && !uptr_ok(p, a4, sizeof(sa), TRUE)))
            return MYOS_EFAULT;
        INTN n = sock_recvfrom(s, (void *)(UINTN)a2, (UINTN)a3, &ip, &port, &ttl);
        if (n >= 0 && a4) {
            memset(&sa, 0, sizeof(sa));
            sa.ip = ip;
            sa.port = port;
            sa.ttl = ttl;
            memcpy((void *)(UINTN)a4, &sa, sizeof(sa));
        }
        return n;
    }

    case SYS_SOCKOPT:
        if ((s = sock_of(p, a1)) < 0)
            return MYOS_EBADF;
        return sock_setopt(s, (UINT32)a2, a3);

    case SYS_RESOLVE: {
        char name[128];
        UINT32 ip = 0;
        INTN r = copy_in_str(p, a1, name, sizeof(name));
        if (r != VFS_OK)
            return r;
        if (!uptr_ok(p, a2, 4, TRUE))
            return MYOS_EFAULT;
        r = dns_resolve(name, &ip, 0);
        if (r == 0)
            *(volatile UINT32 *)(UINTN)a2 = ip;
        return r;
    }

    case SYS_NETINFO: {
        struct myos_netif ni;
        if (!uptr_ok(p, a2, sizeof(ni), TRUE))
            return MYOS_EFAULT;
        INTN r = net_sys_info((UINTN)a1, &ni);
        if (r == 1)
            memcpy((void *)(UINTN)a2, &ni, sizeof(ni));
        return r;
    }

    case SYS_NETCTL: {
        struct myos_netctl c;
        if (!uptr_ok(p, a1, sizeof(c), FALSE))
            return MYOS_EFAULT;
        memcpy(&c, (const void *)(UINTN)a1, sizeof(c));
        return net_sys_ctl(&c);
    }
    }

    return MYOS_ENOSYS;
}

/* ================================================================
 * Диспетчер
 * ================================================================ */

/* ================================================================
 * Шелл-программа (этап 10): запуск и ожидание программ, клавиши по
 * одной, встроенные команды ядра
 * ================================================================ */

/* Открыть файл для "> файл" / ">> файл" (путь - от папки программы) */
static INTN open_out_file(KPROC *p, UINT64 upath, BOOLEAN append, INTN *kfd)
{
    char path[VFS_PATH_MAX];
    INTN r = user_path(p, upath, path, sizeof(path));

    if (r != VFS_OK)
        return r;

    INTN fd = vfs_open(path, VFS_O_WRITE | VFS_O_CREATE | (append ? VFS_O_APPEND : VFS_O_TRUNC));

    if (fd < 0)
        return fd;

    *kfd = fd;
    return VFS_OK;
}

static INT64 sys_spawn(KPROC *p, UINT64 usp)
{
    struct myos_spawn sp;

    if (!uptr_ok(p, usp, sizeof(sp), FALSE))
        return MYOS_EFAULT;

    memcpy(&sp, (const void *)(UINTN)usp, sizeof(sp));

    char name[VFS_PATH_MAX], path[VFS_PATH_MAX], args[512];
    INTN r = copy_in_str(p, (UINT64)(UINTN)sp.path, name, sizeof(name));

    if (r != VFS_OK)
        return r;

    args[0] = '\0';

    if (sp.args != NULL) {
        r = copy_in_str(p, (UINT64)(UINTN)sp.args, args, sizeof(args));
        if (r != VFS_OK)
            return r;
    }

    /* где программа: как в шелле ядра - /bin/имя, путь, ELF в папке
       (относительно папки шелла-программы) */
    ksnprintf(g_cwd, sizeof(g_cwd), "%s", p->cwd);

    if (!proc_find_program(name, path, sizeof(path)))
        return MYOS_ENOENT;

    INTN out_kfd = -1;

    if (sp.out_path != NULL) {
        r = open_out_file(p, (UINT64)(UINTN)sp.out_path, (sp.flags & MYOS_SPAWN_APPEND) != 0, &out_kfd);
        if (r != VFS_OK)
            return r;
    }

    INTN err = VFS_OK;
    KPROC *c = proc_spawn_ex(path, args, p->io, p->cwd, p, out_kfd, NULL, &err);

    if (c == NULL)
        return err;

    /* на переднем плане: Ctrl+C - ей (а не шеллу) */
    if (sp.flags & MYOS_SPAWN_FG) {
        c->was_fg = TRUE;
        if (p->io == PROC_IO_TTY)
            tty_set_fg(p->tty, c);
        else
            g_fg_proc = c;
    }

    return c->pid;
}

static INT64 sys_wait(KPROC *p, INT64 pid, UINT64 uinfo, UINT32 flags)
{
    KPROC *c = NULL;

    for (UINTN i = 0; i < PROC_MAX; i++)
        if (g_procs[i].used && (INT64)g_procs[i].pid == pid && g_procs[i].parent == p)
            c = &g_procs[i];

    if (c == NULL)
        return MYOS_ENOENT;

    if (uinfo != 0 && !uptr_ok(p, uinfo, sizeof(struct myos_waitinfo), TRUE))
        return MYOS_EFAULT;

    UINT64 fl = kx_irq_save();

    while (kthread_alive(c->thread, c->tid)) {

        if ((flags & MYOS_WAIT_NOHANG) || p->killed) {
            kx_irq_restore(fl);
            return p->killed ? MYOS_EINTR : 0;
        }

        sched_block(c->thread, "wait", 200);
    }

    kx_irq_restore(fl);

    if (uinfo != 0) {
        struct myos_waitinfo *wi = (struct myos_waitinfo *)(UINTN)uinfo;
        wi->pid = (int)c->pid;
        wi->code = (int)c->exit_code;
        ksnprintf(wi->name, sizeof(wi->name), "%s", c->name);
        ksnprintf(wi->why, sizeof(wi->why), "%s", c->why);
    }

    /* передний план - снова шеллу */
    if (c->was_fg) {
        if (p->io == PROC_IO_TTY) {
            if (tty_get_fg(p->tty) == c || tty_get_fg(p->tty) == NULL)
                tty_set_fg(p->tty, p);
        } else if (g_fg_proc == c || g_fg_proc == NULL) {
            g_fg_proc = p;
        }
    }

    proc_reap(c);
    return 1;
}

/*
 * Клавиша - одна, без эха (строку ввода шелл-программа рисует сама).
 * PageUp/PageDown листают историю экрана здесь же (как в шелле ядра),
 * а программе - MYOS_KEY_REDRAW: "экран сменился, нарисуй строку".
 */
static INT64 sys_readkey(KPROC *p, INT64 timeout_ms)
{
    p->raw_keys = TRUE;

    if (p->io == PROC_IO_TTY && p->tty != NULL)
        return tty_getkey(p->tty, p, timeout_ms);

    if (p->io != PROC_IO_CONSOLE)
        return MYOS_ENOSYS;

    UINT64 t0 = g_kticks;

    for (;;) {

        if (p->killed)
            return -1;

        EFI_INPUT_KEY key;

        if (g_st->ConIn->ReadKeyStroke(g_st->ConIn, &key) == EFI_SUCCESS) {

            if (key.UnicodeChar == 0 && (key.ScanCode == 0x09 || key.ScanCode == 0x0A)) {

                int step = SCROLLBACK_VISIBLE_ROWS - 1;

                if (step < 1)
                    step = 1;

                int view = g_scrollback_view + ((key.ScanCode == 0x09) ? step : -step);

                if (view < 0)
                    view = 0;

                scrollback_render(g_st, view);
                return MYOS_KEY_REDRAW;
            }

            /* листали историю, а теперь печатают - сначала вниз */
            if (g_scrollback_view != 0) {
                scrollback_render(g_st, 0);
            }

            if (key.UnicodeChar != 0)
                return key.UnicodeChar;

            return MYOS_KEY_SPECIAL | key.ScanCode;
        }

        if (timeout_ms >= 0 && g_kticks - t0 >= (UINT64)timeout_ms)
            return -1;

        sched_sleep_ms(10);
    }
}

/* Вывод команды ядра - в файл ("battery > b.txt"): свой "экран",
   который пишет в файл вместо консоли */
typedef struct {
    SIMPLE_TEXT_OUTPUT_INTERFACE o;      /* первым: указатель на него = на всё */
    INTN kfd;
} KCMD_FILE_OUT;

static EFI_STATUS EFIAPI kcmd_file_string(SIMPLE_TEXT_OUTPUT_INTERFACE *this, CHAR16 *s)
{
    KCMD_FILE_OUT *f = (KCMD_FILE_OUT *)this;
    char buf[128];
    UINTN n = 0;

    for (; *s; s++) {

        CHAR16 c = *s;

        if (c == L'\r')
            continue;

        /* UTF-8 */
        if (c < 0x80) {
            buf[n++] = (char)c;
        } else if (c < 0x800) {
            buf[n++] = (char)(0xC0 | (c >> 6));
            buf[n++] = (char)(0x80 | (c & 0x3F));
        } else {
            buf[n++] = (char)(0xE0 | (c >> 12));
            buf[n++] = (char)(0x80 | ((c >> 6) & 0x3F));
            buf[n++] = (char)(0x80 | (c & 0x3F));
        }

        if (n + 4 >= sizeof(buf)) {
            vfs_write(f->kfd, buf, n);
            n = 0;
        }
    }

    if (n > 0)
        vfs_write(f->kfd, buf, n);

    return EFI_SUCCESS;
}

static EFI_STATUS EFIAPI kcmd_file_nop_attr(SIMPLE_TEXT_OUTPUT_INTERFACE *this, UINTN a)
{
    (void)this;
    (void)a;
    return EFI_SUCCESS;
}

static EFI_STATUS EFIAPI kcmd_file_nop(SIMPLE_TEXT_OUTPUT_INTERFACE *this)
{
    (void)this;
    return EFI_SUCCESS;
}

/* строка UTF-8 -> CHAR16 (кириллица в имени сети Wi-Fi и т.п.) */
static void utf8_to_16(const char *s, CHAR16 *d, UINTN cap)
{
    UINTN n = 0;
    const UINT8 *u = (const UINT8 *)s;

    while (*u && n + 1 < cap) {
        UINT32 c = *u++;
        if (c >= 0xC0 && c < 0xE0 && (*u & 0xC0) == 0x80) {
            c = ((c & 0x1F) << 6) | (*u++ & 0x3F);
        } else if (c >= 0xE0 && c < 0xF0 && (u[0] & 0xC0) == 0x80 && (u[1] & 0xC0) == 0x80) {
            c = ((c & 0x0F) << 12) | ((UINT32)(u[0] & 0x3F) << 6) | (u[1] & 0x3F);
            u += 2;
        } else if (c >= 0x80) {
            c = '?';
        }
        d[n++] = (CHAR16)c;
    }

    d[n] = 0;
}

/*
 * Встроенная команда ядра (net, wifi, battery, cpu, disk, start...).
 * Выполняет её тот же разборщик, что и у шелла ядра (run_command), но
 * вывод - туда, куда пишет программа (или в файл), а незнакомая
 * команда ничего не печатает: ответ 0 - "это не команда ядра", и
 * шелл-программа ищет программу с таким именем. 1 - выполнена.
 */
static INT64 sys_kcmd(KPROC *p, UINT64 uline, UINT64 upath, UINT32 flags)
{
    char line8[LINE_MAX];
    INTN r = copy_in_str(p, uline, line8, sizeof(line8));

    if (r != VFS_OK)
        return r;

    if (p->io != PROC_IO_CONSOLE && !(p->io == PROC_IO_TTY && p->tty != NULL))
        return MYOS_ENOSYS;

    KCMD_FILE_OUT fo;
    EFI_SYSTEM_TABLE st = *g_st;
    BOOLEAN to_file = (upath != 0);

    /* Программу саму запустили с "> файл" (например, /bin/ls, которая
       просит ядро "ls /"): вывод команды ядра - туда же, куда и её
       собственный. Файл программы закроет proc_reap, не мы. */
    BOOLEAN to_prog_file = (!to_file && p->out_kfd >= 0);

    /* шелл в окне-терминале: вывод команды - в это окно */
    if (p->io == PROC_IO_TTY)
        st.ConOut = tty_output(p->tty);

    if (to_file) {

        INTN kfd = -1;
        r = open_out_file(p, upath, (flags & MYOS_KCMD_APPEND) != 0, &kfd);

        if (r != VFS_OK)
            return r;

        fo.o = *st.ConOut;
        fo.o.OutputString = kcmd_file_string;
        fo.o.SetAttribute = kcmd_file_nop_attr;
        fo.o.ClearScreen = kcmd_file_nop;
        fo.kfd = kfd;
        st.ConOut = &fo.o;

    } else if (to_prog_file) {

        fo.o = *st.ConOut;
        fo.o.OutputString = kcmd_file_string;
        fo.o.SetAttribute = kcmd_file_nop_attr;
        fo.o.ClearScreen = kcmd_file_nop;
        fo.kfd = p->out_kfd;
        st.ConOut = &fo.o;
    }

    CHAR16 line[LINE_MAX];
    utf8_to_16(line8, line, LINE_MAX);

    /* команды ядра знают одну "текущую папку" - шелла */
    ksnprintf(g_cwd, sizeof(g_cwd), "%s", p->cwd);

    p->kcmd_probe = TRUE;
    p->kcmd_unknown = FALSE;
    p->kcmd_kernel = (flags & MYOS_KCMD_KERNEL) != 0;

    run_command(&st, line);

    p->kcmd_probe = FALSE;
    p->kcmd_kernel = FALSE;

    ksnprintf(p->cwd, sizeof(p->cwd), "%s", g_cwd);

    kcon_flush();

    if (to_file)
        vfs_close(fo.kfd);

    return p->kcmd_unknown ? 0 : 1;
}

static INT64 kx_syscall_dispatch_inner(UINT64 *f)
{
    KPROC *p = g_kcur->proc;
    UINT64 nr = f[SF_RAX];
    UINT64 a1 = f[SF_RDI], a2 = f[SF_RSI], a3 = f[SF_RDX];
    INT64 r = MYOS_ENOSYS;

    UINT64 a4 = f[SF_R10];

    (void)f[SF_R8]; (void)f[SF_R9];

    if (p == NULL)
        return MYOS_ENOSYS;       /* syscall не из программы?! */

    p->syscalls++;

    switch (nr) {

    case SYS_EXIT:
        proc_exit_current((INT64)(INT32)a1);

    case SYS_WRITE: {
        if (!uptr_ok(p, a2, a3, FALSE)) { r = MYOS_EFAULT; break; }
        if ((a1 == 1 || a1 == 2) && p->out_kfd >= 0) {
            /* шелл запустил с "> файл" */
            r = vfs_write(p->out_kfd, (const VOID *)(UINTN)a2, (UINTN)a3);
        } else if ((a1 == 1 || a1 == 2) && p->io == PROC_IO_TTY && p->tty != NULL) {
            tty_write(p->tty, (const char *)(UINTN)a2, (UINTN)a3);
            r = (INT64)a3;
        } else if (a1 == 1 || a1 == 2) {
            proc_out(p, (const char *)(UINTN)a2, (UINTN)a3);
            r = (INT64)a3;
        } else {
            INTN kfd = fd_kernel(p, (INT64)a1);
            if (kfd >= 0 && (kfd & PROC_FD_SOCK))
                r = sock_send(kfd & ~PROC_FD_SOCK, (const VOID *)(UINTN)a2, (UINTN)a3);
            else
                r = (kfd < 0) ? MYOS_EBADF : vfs_write(kfd, (const VOID *)(UINTN)a2, (UINTN)a3);
        }
        break;
    }

    case SYS_READ: {
        if (!uptr_ok(p, a2, a3, TRUE)) { r = MYOS_EFAULT; break; }
        if (a1 == 0) {
            if (p->io == PROC_IO_TTY && p->tty != NULL)
                r = tty_read_line(p->tty, p, (char *)(UINTN)a2, (UINTN)a3);
            else
                /* у графической программы без терминала ввода нет */
                r = (p->io == PROC_IO_GUI) ? 0
                                           : proc_read_console(p, (char *)(UINTN)a2, (UINTN)a3);
        } else {
            INTN kfd = fd_kernel(p, (INT64)a1);
            if (kfd >= 0 && (kfd & PROC_FD_SOCK))
                r = sock_recv(kfd & ~PROC_FD_SOCK, (VOID *)(UINTN)a2, (UINTN)a3);
            else
                r = (kfd < 0) ? MYOS_EBADF : vfs_read(kfd, (VOID *)(UINTN)a2, (UINTN)a3);
        }
        break;
    }

    case SYS_OPEN: {
        char path[VFS_PATH_MAX];
        INTN slot = -1;
        r = user_path(p, a1, path, sizeof(path));
        if (r != VFS_OK)
            break;
        for (UINTN i = 0; i < PROC_FDS; i++)
            if (p->fds[i] < 0) { slot = (INTN)i; break; }
        if (slot < 0) { r = MYOS_EMFILE; break; }
        INTN kfd = vfs_open(path, (UINT32)a2 & 0x1Fu);
        if (kfd < 0) { r = kfd; break; }
        p->fds[slot] = kfd;
        r = 3 + slot;
        break;
    }

    case SYS_CLOSE: {
        INTN kfd = fd_kernel(p, (INT64)a1);
        if (kfd < 0) { r = MYOS_EBADF; break; }
        if (kfd & PROC_FD_SOCK)
            sock_close(kfd & ~PROC_FD_SOCK);
        else
            vfs_close(kfd);
        p->fds[a1 - 3] = -1;
        r = 0;
        break;
    }

    case SYS_SLEEP: {
        /* кусками: чтобы Ctrl+C не ждал конца долгого сна */
        UINT64 ms = a1;
        while (ms > 0 && !p->killed) {
            UINT64 k = (ms < 50) ? ms : 50;
            sched_sleep_ms(k);
            ms -= k;
        }
        r = 0;
        break;
    }

    case SYS_UPTIME:
        r = (INT64)g_kticks;
        break;

    case SYS_SBRK: {
        INT64 inc = (INT64)a1;
        UINT64 old = p->brk;
        if (inc < 0 || (UINT64)inc > MAX_HEAP_BYTES ||
            p->brk + (UINT64)inc > p->brk_base + MAX_HEAP_BYTES) { r = MYOS_ENOSPC; break; }
        UINT64 nb = p->brk + (UINT64)inc;
        UINT64 from = (p->brk + 4095u) & ~0xFFFull;
        BOOLEAN ok = TRUE;
        for (UINT64 va = from; va < nb; va += 4096u)
            if (!proc_map_heap_page(p, va)) { ok = FALSE; break; }
        if (!ok) { r = MYOS_ENOSPC; break; }
        p->brk = nb;
        r = (INT64)old;
        break;
    }

    case SYS_GETPID:
        r = p->pid;
        break;

    case SYS_TIME: {
        struct myos_time t;
        EFI_TIME now;
        if (!uptr_ok(p, a1, sizeof(t), TRUE)) { r = MYOS_EFAULT; break; }
        if (!rtc_read(&now)) { r = MYOS_EIO; break; }
        if (a2 != 1)
            tz_to_local(&now);          /* CMOS хранит UTC; программам - местное */
        t.year = now.Year; t.month = now.Month; t.day = now.Day;
        t.hour = now.Hour; t.minute = now.Minute; t.second = now.Second; t.pad = 0;
        memcpy((void *)(UINTN)a1, &t, sizeof(t));
        r = 0;
        break;
    }

    case SYS_READDIR:
    case SYS_STAT: {
        char path[VFS_PATH_MAX];
        UINT64 dp = (nr == SYS_STAT) ? a2 : a3;
        if (!uptr_ok(p, dp, sizeof(struct myos_dirent), TRUE)) { r = MYOS_EFAULT; break; }
        r = user_path(p, a1, path, sizeof(path));
        if (r != VFS_OK)
            break;
        struct myos_dirent *d = (struct myos_dirent *)(UINTN)dp;
        if (nr == SYS_STAT) {
            VFS_DIRENT e;
            r = vfs_stat(path, &e);
            if (r == VFS_OK) {
                RD_CTX c = { 0, 0, d, FALSE };
                rd_cb(&c, &e);
            }
        } else {
            RD_CTX c = { (UINTN)a2, 0, d, FALSE };
            r = vfs_list(path, rd_cb, &c);
            if (r == VFS_OK)
                r = c.found ? 1 : 0;
        }
        break;
    }

    case SYS_MKDIR:
    case SYS_UNLINK: {
        char path[VFS_PATH_MAX];
        r = user_path(p, a1, path, sizeof(path));
        if (r == VFS_OK)
            r = (nr == SYS_MKDIR) ? vfs_mkdir(path) : vfs_remove(path);
        break;
    }

    case SYS_RENAME: {
        char a[VFS_PATH_MAX], b[VFS_PATH_MAX];
        r = user_path(p, a1, a, sizeof(a));
        if (r == VFS_OK)
            r = user_path(p, a2, b, sizeof(b));
        if (r == VFS_OK)
            r = vfs_rename(a, b);
        break;
    }

    case SYS_YIELD:
        sched_yield();
        r = 0;
        break;

    case SYS_WIN_CREATE:
        r = win_sys_create(p, a1, a2, a3);
        break;

    case SYS_WIN_UPDATE:
        r = win_sys_update(p, a1, a2);
        break;

    case SYS_WIN_EVENT:
        r = win_sys_event(p, a1, a2, a3);
        break;

    case SYS_WIN_CLOSE:
        r = win_sys_close(p, a1);
        break;

    case SYS_WIN_TITLE:
        r = win_sys_title(p, a1, a2);
        break;

    case SYS_GETKEY: {
        EFI_INPUT_KEY key;
        r = 0;
        if (p->io == PROC_IO_CONSOLE &&
            g_st->ConIn->ReadKeyStroke(g_st->ConIn, &key) == EFI_SUCCESS)
            r = key.UnicodeChar ? key.UnicodeChar : (0x100 + key.ScanCode);
        if (p->io == PROC_IO_TTY && p->tty != NULL) {
            INT32 k = tty_getkey(p->tty, p, 0);
            if (k > 0)
                r = (k & MYOS_KEY_SPECIAL) ? 0x100 + (k & 0xFFFF) : k;
        }
        break;
    }

    case SYS_SOCKET:
    case SYS_CONNECT:
    case SYS_BIND:
    case SYS_LISTEN:
    case SYS_ACCEPT:
    case SYS_SENDTO:
    case SYS_RECVFROM:
    case SYS_SOCKOPT:
    case SYS_RESOLVE:
    case SYS_NETINFO:
    case SYS_NETCTL:
        r = sys_net(p, nr, a1, a2, a3, a4);
        break;

    case SYS_GETRANDOM: {
        if (a2 > 4096 || !uptr_ok(p, a1, a2, TRUE)) { r = MYOS_EFAULT; break; }
        krandom_fill((void *)(UINTN)a1, (UINTN)a2);
        r = (INT64)a2;
        break;
    }

    case SYS_SEEK: {
        INTN kfd = fd_kernel(p, (INT64)a1);
        UINT64 np = 0;
        if (kfd < 0 || (kfd & PROC_FD_SOCK)) { r = (kfd < 0) ? MYOS_EBADF : MYOS_EINVAL; break; }
        r = vfs_seek(kfd, (INT64)a2, (UINT32)a3, &np);
        if (r == VFS_OK)
            r = (INT64)np;
        break;
    }

    case SYS_FSTAT: {
        struct myos_dirent d;
        if (!uptr_ok(p, a2, sizeof(d), TRUE)) { r = MYOS_EFAULT; break; }
        memset(&d, 0, sizeof(d));
        if ((INT64)a1 >= 0 && a1 <= 2) {
            d.is_dir = MYOS_FT_CONSOLE;
            r = 0;
        } else {
            INTN kfd = fd_kernel(p, (INT64)a1);
            if (kfd < 0) { r = MYOS_EBADF; break; }
            if (kfd & PROC_FD_SOCK) {
                d.is_dir = MYOS_FT_SOCKET;
                r = 0;
            } else {
                d.is_dir = MYOS_FT_FILE;
                r = vfs_size(kfd, &d.size);
            }
        }
        if (r == 0)
            memcpy((void *)(UINTN)a2, &d, sizeof(d));
        break;
    }

    case SYS_GETCWD: {
        UINTN n = 0;
        while (p->cwd[n])
            n++;
        if (!uptr_ok(p, a1, a2, TRUE)) { r = MYOS_EFAULT; break; }
        if (n + 1 > a2) { r = MYOS_EINVAL; break; }
        memcpy((void *)(UINTN)a1, p->cwd, n + 1);
        r = (INT64)n;
        break;
    }

    case SYS_SPAWN:
        r = sys_spawn(p, a1);
        break;

    case SYS_WAIT:
        r = sys_wait(p, (INT64)a1, a2, (UINT32)a3);
        break;

    case SYS_READKEY:
        r = sys_readkey(p, (INT64)a1);
        break;

    case SYS_KCMD:
        r = sys_kcmd(p, a1, a2, (UINT32)a3);
        break;

    case SYS_CHDIR: {
        char path[VFS_PATH_MAX], norm[VFS_PATH_MAX];
        VFS_DIRENT e;
        r = user_path(p, a1, path, sizeof(path));
        if (r != VFS_OK)
            break;
        r = vfs_normalize(path, norm, sizeof(norm));
        if (r != VFS_OK)
            break;
        /* корень "/" - список томов, в него тоже можно */
        if (!(norm[0] == '/' && norm[1] == '\0')) {
            r = vfs_stat(norm, &e);
            if (r != VFS_OK)
                break;
            if (!e.node.is_dir) { r = MYOS_ENOTDIR; break; }
        }
        ksnprintf(p->cwd, sizeof(p->cwd), "%s", norm);
        r = 0;
        break;
    }

    case SYS_POLL:
        r = sys_poll(p, a1, a2, (INT64)a3);
        break;

    default:
        r = MYOS_ENOSYS;
        break;
    }

    /* попросили завершиться (Ctrl+C, закрыли окно) - не возвращаемся */
    if (p->killed)
        proc_exit_current(-1);

    return r;
}

/* Вход из ассемблера: весь системный вызов - под большим замком ядра
   (SMP, sched.c); программа снаружи работает без него */
__attribute__((used))
INT64 kx_syscall_dispatch(UINT64 *f)
{
    kx_bkl_enter();
    INT64 r = kx_syscall_dispatch_inner(f);
    kx_bkl_exit();
    return r;
}
