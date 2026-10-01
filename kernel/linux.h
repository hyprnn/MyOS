/*
 * kernel/linux.h - числа из мира Linux (этап 11): номера ошибок,
 * флаги open/mmap/clone, сигналы, структуры, которые программы Linux
 * ждут от ядра. Значения - как у ядра Linux для x86-64 (из его
 * заголовков uapi); программа собрана под них, менять нельзя.
 * Нужен только kernel/linux.c, lxfile.c, lxsig.c.
 */
#ifndef MYOS_LINUX_H
#define MYOS_LINUX_H

#include "myos.h"

/* ---- номера ошибок (errno; ядро возвращает их со знаком минус) ---- */
#define LX_EPERM        1
#define LX_ENOENT       2
#define LX_ESRCH        3
#define LX_EINTR        4
#define LX_EIO          5
#define LX_ENXIO        6
#define LX_E2BIG        7
#define LX_ENOEXEC      8
#define LX_EBADF        9
#define LX_ECHILD      10
#define LX_EAGAIN      11
#define LX_ENOMEM      12
#define LX_EACCES      13
#define LX_EFAULT      14
#define LX_EBUSY       16
#define LX_EEXIST      17
#define LX_EXDEV       18
#define LX_ENODEV      19
#define LX_ENOTDIR     20
#define LX_EISDIR      21
#define LX_EINVAL      22
#define LX_ENFILE      23
#define LX_EMFILE      24
#define LX_ENOTTY      25
#define LX_EFBIG       27
#define LX_ENOSPC      28
#define LX_ESPIPE      29
#define LX_EROFS       30
#define LX_EMLINK      31
#define LX_EPIPE       32
#define LX_ERANGE      34
#define LX_ENAMETOOLONG 36
#define LX_ENOSYS      38
#define LX_ENOTEMPTY   39
#define LX_ELOOP       40
#define LX_ENODATA     61
#define LX_EOVERFLOW   75
#define LX_ENOTSOCK    88
#define LX_EOPNOTSUPP  95
#define LX_EAFNOSUPPORT 97
#define LX_EADDRINUSE  98
#define LX_ENETUNREACH 101
#define LX_ECONNRESET  104
#define LX_ENOTCONN    107
#define LX_ETIMEDOUT   110
#define LX_ECONNREFUSED 111
#define LX_EINPROGRESS 115
/* внутреннее: "прервано сигналом - повторить вызов, если обработчик
   с SA_RESTART" (до программы не доходит) */
#define LX_ERESTARTSYS 512

/* ---- open ---- */
#define LX_O_ACCMODE   0x3
#define LX_O_RDONLY    0x0
#define LX_O_WRONLY    0x1
#define LX_O_RDWR      0x2
#define LX_O_CREAT     0x40
#define LX_O_EXCL      0x80
#define LX_O_NOCTTY    0x100
#define LX_O_TRUNC     0x200
#define LX_O_APPEND    0x400
#define LX_O_NONBLOCK  0x800
#define LX_O_DIRECTORY 0x10000
#define LX_O_NOFOLLOW  0x20000
#define LX_O_CLOEXEC   0x80000
#define LX_O_PATH      0x200000

#define LX_AT_FDCWD          (-100)
#define LX_AT_SYMLINK_NOFOLLOW 0x100
#define LX_AT_REMOVEDIR      0x200
#define LX_AT_EMPTY_PATH     0x1000

/* ---- типы файлов (st_mode) ---- */
#define LX_S_IFMT    0170000
#define LX_S_IFDIR   0040000
#define LX_S_IFREG   0100000
#define LX_S_IFCHR   0020000
#define LX_S_IFIFO   0010000
#define LX_S_IFLNK   0120000

/* ---- mmap ---- */
#define LX_PROT_READ   0x1
#define LX_PROT_WRITE  0x2
#define LX_PROT_EXEC   0x4
#define LX_MAP_SHARED  0x01
#define LX_MAP_PRIVATE 0x02
#define LX_MAP_FIXED   0x10
#define LX_MAP_ANONYMOUS 0x20
#define LX_MAP_GROWSDOWN 0x100
#define LX_MAP_NORESERVE 0x4000
#define LX_MAP_POPULATE  0x8000
#define LX_MAP_FIXED_NOREPLACE 0x100000
#define LX_MREMAP_MAYMOVE 1
#define LX_MREMAP_FIXED   2
#define LX_MADV_DONTNEED  4
#define LX_MADV_FREE      8

/* ---- clone ---- */
#define LX_CLONE_VM             0x00000100
#define LX_CLONE_FS             0x00000200
#define LX_CLONE_FILES          0x00000400
#define LX_CLONE_SIGHAND        0x00000800
#define LX_CLONE_PIDFD          0x00001000
#define LX_CLONE_VFORK          0x00004000
#define LX_CLONE_PARENT         0x00008000
#define LX_CLONE_THREAD         0x00010000
#define LX_CLONE_SYSVSEM        0x00040000
#define LX_CLONE_SETTLS         0x00080000
#define LX_CLONE_PARENT_SETTID  0x00100000
#define LX_CLONE_CHILD_CLEARTID 0x00200000
#define LX_CLONE_CHILD_SETTID   0x01000000

/* ---- wait ---- */
#define LX_WNOHANG    1
#define LX_WUNTRACED  2

/* ---- futex ---- */
#define LX_FUTEX_WAIT            0
#define LX_FUTEX_WAKE            1
#define LX_FUTEX_REQUEUE         3
#define LX_FUTEX_CMP_REQUEUE     4
#define LX_FUTEX_WAKE_OP         5
#define LX_FUTEX_WAIT_BITSET     9
#define LX_FUTEX_WAKE_BITSET    10
#define LX_FUTEX_PRIVATE_FLAG  128
#define LX_FUTEX_CLOCK_REALTIME 256

/* ---- сигналы ---- */
#define LX_SIGHUP     1
#define LX_SIGINT     2
#define LX_SIGQUIT    3
#define LX_SIGILL     4
#define LX_SIGTRAP    5
#define LX_SIGABRT    6
#define LX_SIGBUS     7
#define LX_SIGFPE     8
#define LX_SIGKILL    9
#define LX_SIGUSR1   10
#define LX_SIGSEGV   11
#define LX_SIGUSR2   12
#define LX_SIGPIPE   13
#define LX_SIGALRM   14
#define LX_SIGTERM   15
#define LX_SIGCHLD   17
#define LX_SIGCONT   18
#define LX_SIGSTOP   19
#define LX_SIGTSTP   20
#define LX_SIGTTIN   21
#define LX_SIGTTOU   22
#define LX_SIGURG    23
#define LX_SIGWINCH  28

#define LX_SIG_DFL    0
#define LX_SIG_IGN    1

#define LX_SA_NOCLDSTOP 0x00000001
#define LX_SA_NOCLDWAIT 0x00000002
#define LX_SA_SIGINFO   0x00000004
#define LX_SA_ONSTACK   0x08000000
#define LX_SA_RESTART   0x10000000
#define LX_SA_NODEFER   0x40000000
#define LX_SA_RESETHAND 0x80000000
#define LX_SA_RESTORER  0x04000000

#define LX_SIG_BLOCK    0
#define LX_SIG_UNBLOCK  1
#define LX_SIG_SETMASK  2

#define LX_SS_ONSTACK   1
#define LX_SS_DISABLE   2

#define LX_SIGBIT(n)  (1ull << ((n) - 1))

/* ---- время ---- */
typedef struct {
    INT64 tv_sec;
    INT64 tv_nsec;
} LX_TIMESPEC;

typedef struct {
    INT64 tv_sec;
    INT64 tv_usec;
} LX_TIMEVAL;

/* ---- kernel/linux.c ---- */
UINT64 lx_now_ns(BOOLEAN realtime);
UINT32 lx_tid(KTHREAD *t);

/* ---- kernel/lxsig.c ---- */
BOOLEAN lx_signal_pending(void);
void lx_signal_send(KPROC *p, UINT32 sig);
INT64 lx_signal_send_tid(UINT32 tid, UINT32 sig);
void lx_signal_thread(KTHREAD *t, UINT32 sig);
INT64 lx_sig_syscall(KPROC *p, UINT64 nr, UINT64 *a, UINT64 *f, BOOLEAN *handled);
void lx_signal_deliver(KPROC *p, LX_REGS *r, const void *fx, INT64 sysret, UINT64 sysnr,
                       BOOLEAN from_syscall) __attribute__((noreturn));
BOOLEAN lx_signal_ready(KPROC *p);
void lx_signal_fault(KPROC *p, UINT32 sig, UINT64 addr);
void lx_sig_exec_reset(KPROC *p);
void lx_ctrl_c(KPROC *p);
void sched_wake_thread(KTHREAD *t);

#endif
