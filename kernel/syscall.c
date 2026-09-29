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

volatile UINT64 g_sc_kstack = 0;      /* вершина стека ядра текущего потока */
volatile UINT64 g_sc_user_rsp = 0;    /* временно: rsp программы при входе */

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
    "  movq %rsp, g_sc_user_rsp(%rip)\n"
    "  movq g_sc_kstack(%rip), %rsp\n"
    "  pushq g_sc_user_rsp(%rip)\n"
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

        /* окно GUI: копим строку, отдаём по '\n'; длинную -
           переносим по последнему пробелу (окно узкое) */
        for (UINTN i = 0; i < n; i++) {

            char c = s[i];

            if (c == '\r')
                continue;

            if (c == '\n') {
                p->outline[p->outlen] = '\0';
                if (p->gui_line)
                    p->gui_line(p->outline);
                p->outlen = 0;
                continue;
            }

            if (p->outlen >= GUI_TERM_LINE_LEN || p->outlen + 1 >= sizeof(p->outline)) {

                UINTN cut = p->outlen;

                for (UINTN k = p->outlen; k > p->outlen / 2; k--)
                    if (p->outline[k - 1] == ' ') {
                        cut = k;
                        break;
                    }

                char rest[64];
                UINTN rn = 0;

                for (UINTN k = cut; k < p->outlen; k++)
                    rest[rn++] = p->outline[k];

                p->outline[cut] = '\0';
                if (p->gui_line)
                    p->gui_line(p->outline);

                for (UINTN k = 0; k < rn; k++)
                    p->outline[k] = rest[k];
                p->outlen = rn;
            }

            p->outline[p->outlen++] = c;
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

/* Строка из окна GUI (её кладёт proc_gui_input) */
static INTN proc_read_gui(KPROC *p, char *dst, UINTN n)
{
    /* подсказка без '\n' ("Your guess: ") - показать сейчас, иначе
       человек не увидит вопроса */
    if (p->outlen > 0 && p->gui_line) {
        p->outline[p->outlen] = '\0';
        p->gui_line(p->outline);
        p->outlen = 0;
    }

    UINT64 fl = kx_irq_save();

    while (!p->inready && !p->killed)
        sched_block(p, "keyboard (GUI)", 0);

    if (p->killed) {
        kx_irq_restore(fl);
        return 0;
    }

    UINTN k = (p->inlen < n) ? p->inlen : n;

    for (UINTN i = 0; i < k; i++)
        dst[i] = p->inbuf[i];

    /* остаток строки - на следующее чтение */
    for (UINTN i = k; i < p->inlen; i++)
        p->inbuf[i - k] = p->inbuf[i];

    p->inlen -= k;

    if (p->inlen == 0)
        p->inready = FALSE;

    kx_irq_restore(fl);

    return (INTN)k;
}

static INTN fd_kernel(KPROC *p, INT64 fd)
{
    if (fd < 3 || fd >= 3 + PROC_FDS)
        return -1;

    return p->fds[fd - 3];
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
    c->out->pad = 0;
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

INT64 kx_syscall_dispatch(UINT64 *f)
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
        if (a1 == 1 || a1 == 2) {
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
            r = (p->io == PROC_IO_GUI) ? proc_read_gui(p, (char *)(UINTN)a2, (UINTN)a3)
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

    default:
        r = MYOS_ENOSYS;
        break;
    }

    /* попросили завершиться (Ctrl+C, закрыли окно) - не возвращаемся */
    if (p->killed)
        proc_exit_current(-1);

    return r;
}
