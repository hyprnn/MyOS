/*
 * user/posix/os.c - полная libc (picolibc) поверх ядра MyOS (этап 9).
 * Часть MyOS.
 *
 * ЗАЧЕМ
 * -----
 * picolibc - готовая libc для систем "без Linux" (лицензия BSD):
 * printf с дробными числами, FILE *, fopen, qsort, strtod, math.h,
 * time.h, locale, регулярные выражения... Сама она ничего не знает
 * про ядро: всё, что ей нужно от ОС, она зовёт по именам POSIX -
 * read, write, open, lseek, fstat, gettimeofday, sbrk... Этот файл и
 * даёт эти функции, переводя их в системные вызовы MyOS (sysnum.h).
 * Так программы (и чужие библиотеки: браузер, curl, FreeType) пишутся
 * как для Linux, а работают на MyOS.
 *
 * Ошибки: ядро отвечает отрицательным кодом MYOS_E*, а POSIX хочет
 * -1 и код в errno (ENOENT...) - переводим таблицей.
 *
 * Чего нет в MyOS (fork, exec, pipe, сигналы) - честно отвечаем
 * ENOSYS: у MyOS один поток на программу и нет порождения процессов
 * из программ.
 */
#include <errno.h>
#include <fcntl.h>
#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <dirent.h>
#include <signal.h>
#include <termios.h>
#include <sys/ioctl.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/times.h>
#include <sys/types.h>
#include <sys/utsname.h>
#include "myos_sys.h"

/* ================================================================
 * Системные вызовы
 * ================================================================ */

long myos_syscall3(long nr, long a1, long a2, long a3)
{
    long ret;

    __asm__ __volatile__(
        "syscall"
        : "=a"(ret)
        : "a"(nr), "D"(a1), "S"(a2), "d"(a3)
        : "rcx", "r11", "memory"
    );

    return ret;
}

long myos_syscall4(long nr, long a1, long a2, long a3, long a4)
{
    long ret;
    register long r10 __asm__("r10") = a4;

    __asm__ __volatile__(
        "syscall"
        : "=a"(ret)
        : "a"(nr), "D"(a1), "S"(a2), "d"(a3), "r"(r10)
        : "rcx", "r11", "memory"
    );

    return ret;
}

/* Код MyOS -> errno */
static int errno_of(long e)
{
    switch (e) {
    case MYOS_ENOENT:        return ENOENT;
    case MYOS_EEXIST:        return EEXIST;
    case MYOS_ENOTDIR:       return ENOTDIR;
    case MYOS_EISDIR:        return EISDIR;
    case MYOS_ENOTEMPTY:     return ENOTEMPTY;
    case MYOS_ENOSPC:        return ENOSPC;
    case MYOS_EROFS:         return EROFS;
    case MYOS_EIO:           return EIO;
    case MYOS_EINVAL:        return EINVAL;
    case MYOS_EBADF:         return EBADF;
    case MYOS_EMFILE:        return EMFILE;
    case MYOS_ENOSYS:        return ENOSYS;
    case MYOS_EGONE:         return ENODEV;      /* флешку вынули */
    case MYOS_EXDEV:         return EXDEV;
    case MYOS_EFAULT:        return EFAULT;
    case MYOS_ENOGUI:        return ENODEV;
    case MYOS_ETIMEDOUT:     return ETIMEDOUT;
    case MYOS_ECONNREFUSED:  return ECONNREFUSED;
    case MYOS_ECONNRESET:    return ECONNRESET;
    case MYOS_ENETUNREACH:   return ENETUNREACH;
    case MYOS_EADDRINUSE:    return EADDRINUSE;
    case MYOS_ENOTCONN:      return ENOTCONN;
    case MYOS_EHOSTNOTFOUND: return EHOSTUNREACH;
    case MYOS_EAGAIN:        return EAGAIN;
    case MYOS_EINTR:         return EINTR;
    case MYOS_EINPROGRESS:   return EINPROGRESS;
    default:                 return EIO;
    }
}

int myos_set_errno(long myos_err)
{
    errno = errno_of(myos_err);
    return -1;
}

/* Ответ ядра -> ответ POSIX: ошибка - -1 и errno */
static long ret_of(long r)
{
    return (r < 0) ? myos_set_errno(r) : r;
}

/* ================================================================
 * Процесс
 * ================================================================ */

void _exit(int code)
{
    myos_syscall3(SYS_EXIT, code, 0, 0);
    for (;;) { }
}

pid_t getpid(void)
{
    return (pid_t)myos_syscall3(SYS_GETPID, 0, 0, 0);
}

/* пользователь в MyOS один - "root" */
uid_t getuid(void)  { return 0; }
uid_t geteuid(void) { return 0; }
gid_t getgid(void)  { return 0; }
gid_t getegid(void) { return 0; }

/* Имя системы (браузер пишет его в User-Agent) */
int uname(struct utsname *u)
{
    memset(u, 0, sizeof(*u));
    strcpy(u->sysname, "MyOS");
    strcpy(u->nodename, "myos");
    strcpy(u->release, "9");
    strcpy(u->version, "MyOS stage 9");
    strcpy(u->machine, "x86_64");
    return 0;
}

/* Куча: malloc из picolibc берёт память кусками через sbrk */
void *sbrk(ptrdiff_t inc)
{
    long r = myos_syscall3(SYS_SBRK, (long)inc, 0, 0);

    if (r < 0) {
        errno = ENOMEM;
        return (void *)-1;
    }

    return (void *)r;
}

/* Процессы из программ MyOS порождать не умеет */
pid_t fork(void)
{
    errno = ENOSYS;
    return -1;
}

int execve(const char *path, char *const argv[], char *const envp[])
{
    (void)path; (void)argv; (void)envp;
    errno = ENOSYS;
    return -1;
}

pid_t waitpid(pid_t pid, int *status, int options)
{
    (void)pid; (void)status; (void)options;
    errno = ECHILD;
    return -1;
}

int pipe(int fds[2])
{
    (void)fds;
    errno = ENOSYS;
    return -1;
}

int dup2(int oldfd, int newfd)
{
    (void)oldfd; (void)newfd;
    errno = ENOSYS;
    return -1;
}

/* ================================================================
 * Сигналы
 *
 * Настоящих сигналов в MyOS нет (ядро не прерывает программу ради её
 * обработчика). Но libc и чужие программы зовут raise/abort/signal:
 * обработчик запоминаем и вызываем сами, когда программа делает
 * raise(); "по умолчанию" - завершить программу с сообщением.
 * ================================================================ */

static _sig_func_ptr g_sig[_NSIG];
static const char *g_progname = "program";     /* argv[0] - для сообщений */

_sig_func_ptr signal(int sig, _sig_func_ptr h)
{
    if (sig <= 0 || sig >= _NSIG || sig == SIGKILL || sig == SIGSTOP) {
        errno = EINVAL;
        return SIG_ERR;
    }

    _sig_func_ptr old = g_sig[sig];

    g_sig[sig] = h;
    return old;
}

int sigaction(int sig, const struct sigaction *restrict act, struct sigaction *restrict old)
{
    if (sig <= 0 || sig >= _NSIG) {
        errno = EINVAL;
        return -1;
    }

    if (old != NULL) {
        memset(old, 0, sizeof(*old));
        old->sa_handler = g_sig[sig];
    }

    if (act != NULL)
        g_sig[sig] = act->sa_handler;

    return 0;
}

int sigprocmask(int how, const sigset_t *restrict set, sigset_t *restrict old)
{
    (void)how; (void)set;

    if (old != NULL)
        *old = 0;              /* ничего не заблокировано */
    return 0;
}

int raise(int sig)
{
    if (sig <= 0 || sig >= _NSIG) {
        errno = EINVAL;
        return -1;
    }

    _sig_func_ptr h = g_sig[sig];

    if (h == SIG_IGN)
        return 0;

    if (h != SIG_DFL) {
        h(sig);
        return 0;
    }

    /* действие по умолчанию: завершиться (как в Linux - код 128 + номер) */
    fprintf(stderr, "%s: terminated by signal %d%s\n", g_progname, sig,
            sig == SIGABRT ? " (abort)" : "");
    fflush(NULL);
    _exit(128 + sig);
}

int kill(pid_t pid, int sig)
{
    if (pid == getpid() || pid == 0)
        return raise(sig);

    errno = EPERM;             /* другим программам сигналы не шлём */
    return -1;
}

unsigned alarm(unsigned seconds)
{
    (void)seconds;             /* таймерного сигнала нет */
    return 0;
}

/* ================================================================
 * Файлы
 * ================================================================ */

/*
 * Сокеты (user/posix/socket.c) - "слабые" ссылки: программа без сети
 * socket.c не подключает, и тогда эти указатели - NULL.
 */
int     __myos_is_socket(int fd) __attribute__((weak));
void    __myos_sock_forget(int fd) __attribute__((weak));
int     __myos_sock_nonblock(int fd, int set, int on) __attribute__((weak));
ssize_t __myos_sock_read(int fd, void *buf, size_t n) __attribute__((weak));

static int is_socket(int fd)
{
    return __myos_is_socket != NULL && __myos_is_socket(fd);
}

/* ================================================================
 * Встроенные файлы (/embed/...)
 *
 * Большой программе (браузер) нужны свои файлы: шрифты, стили,
 * тексты сообщений. Чтобы не зависеть от флешки, они вшиты в саму
 * программу таблицей __myos_embedded_files (myos_sys.h), а libc
 * показывает их как обычные файлы только для чтения по путям
 * "/embed/имя": open/read/lseek/fstat/stat/close работают как с диском.
 * Номера таких файлов - EMBED_FD_BASE + i (ядро их не видит).
 * ================================================================ */

extern const struct myos_embed_file __myos_embedded_files[] __attribute__((weak));

#define EMBED_PREFIX   "/embed/"
#define EMBED_FD_BASE  0x6000
#define EMBED_FD_MAX   16

static struct {
    const struct myos_embed_file *f;       /* NULL - место свободно */
    size_t                        pos;
} g_efd[EMBED_FD_MAX];

static const struct myos_embed_file *embed_find(const char *path)
{
    if (__myos_embedded_files == NULL ||
        strncmp(path, EMBED_PREFIX, sizeof(EMBED_PREFIX) - 1) != 0)
        return NULL;

    const char *name = path + sizeof(EMBED_PREFIX) - 1;

    for (const struct myos_embed_file *f = __myos_embedded_files; f->name != NULL; f++)
        if (strcmp(f->name, name) == 0)
            return f;

    return NULL;
}

/* "/embed" или "/embed/папка" - есть ли встроенные файлы внутри */
static int embed_is_dir(const char *path)
{
    size_t pl = sizeof(EMBED_PREFIX) - 2;          /* "/embed" без '/' */

    if (__myos_embedded_files == NULL || strncmp(path, EMBED_PREFIX, pl) != 0)
        return 0;

    const char *rest = path + pl;                  /* "" или "/en" или "/en/" */

    while (*rest == '/')
        rest++;

    size_t rl = strlen(rest);

    while (rl > 0 && rest[rl - 1] == '/')
        rl--;

    if (rl == 0)
        return 1;                                  /* сам /embed */

    for (const struct myos_embed_file *f = __myos_embedded_files; f->name != NULL; f++)
        if (strncmp(f->name, rest, rl) == 0 && f->name[rl] == '/')
            return 1;

    return 0;
}

static int embed_slot(int fd)
{
    int i = fd - EMBED_FD_BASE;

    return (i >= 0 && i < EMBED_FD_MAX && g_efd[i].f != NULL) ? i : -1;
}

static void embed_stat(const struct myos_embed_file *f, struct stat *st)
{
    memset(st, 0, sizeof(*st));
    st->st_mode = S_IFREG | 0444;
    st->st_nlink = 1;
    st->st_blksize = 4096;
    st->st_size = (off_t)f->size;
    st->st_blocks = (blkcnt_t)((f->size + 511u) / 512u);
}

ssize_t read(int fd, void *buf, size_t n)
{
    int e = embed_slot(fd);

    if (e >= 0) {
        size_t left = g_efd[e].f->size - g_efd[e].pos;
        size_t c = (n < left) ? n : left;
        memcpy(buf, g_efd[e].f->data + g_efd[e].pos, c);
        g_efd[e].pos += c;
        return (ssize_t)c;
    }

    /* у сокета могли остаться "подсмотренные" (MSG_PEEK) байты */
    if (is_socket(fd)) {
        ssize_t r = __myos_sock_read(fd, buf, n);
        if (r != -2)
            return r;
    }

    return (ssize_t)ret_of(myos_syscall3(SYS_READ, fd, (long)buf, (long)n));
}

ssize_t write(int fd, const void *buf, size_t n)
{
    return (ssize_t)ret_of(myos_syscall3(SYS_WRITE, fd, (long)buf, (long)n));
}

int open(const char *path, int flags, ...)
{
    int acc = flags & O_ACCMODE;
    long mf = 0;
    struct myos_dirent d;

    /* права (mode) третьим аргументом MyOS не хранит - не читаем его */

    const struct myos_embed_file *ef = embed_find(path);

    if (ef != NULL) {
        if (acc != O_RDONLY) {
            errno = EROFS;
            return -1;
        }
        for (int i = 0; i < EMBED_FD_MAX; i++)
            if (g_efd[i].f == NULL) {
                g_efd[i].f = ef;
                g_efd[i].pos = 0;
                return EMBED_FD_BASE + i;
            }
        errno = EMFILE;
        return -1;
    }

    if (acc == O_RDONLY)
        mf = MYOS_O_READ;
    else if (acc == O_WRONLY)
        mf = MYOS_O_WRITE;
    else
        mf = MYOS_O_READ | MYOS_O_WRITE;

    if (flags & O_CREAT)
        mf |= MYOS_O_CREATE;
    if (flags & O_TRUNC)
        mf |= MYOS_O_TRUNC;
    if (flags & O_APPEND)
        mf |= MYOS_O_APPEND;

    long st = myos_syscall3(SYS_STAT, (long)path, (long)&d, 0);

    /* O_EXCL: "создать, но только если такого ещё нет" */
    if ((flags & O_CREAT) && (flags & O_EXCL) && st == 0) {
        errno = EEXIST;
        return -1;
    }

    if (st == 0 && d.is_dir) {
        errno = EISDIR;
        return -1;
    }

    if ((flags & O_DIRECTORY) && !(st == 0 && d.is_dir)) {
        errno = (st == 0) ? ENOTDIR : errno_of(st);
        return -1;
    }

    return (int)ret_of(myos_syscall3(SYS_OPEN, (long)path, mf, 0));
}

int creat(const char *path, mode_t mode)
{
    (void)mode;
    return open(path, O_WRONLY | O_CREAT | O_TRUNC);
}

int close(int fd)
{
    if (fd >= 0 && fd <= 2)
        return 0;              /* экран и клавиатуру не закрываем */

    int e = embed_slot(fd);

    if (e >= 0) {
        g_efd[e].f = NULL;
        return 0;
    }

    if (is_socket(fd))
        __myos_sock_forget(fd);

    return (int)ret_of(myos_syscall3(SYS_CLOSE, fd, 0, 0));
}

off_t lseek(int fd, off_t off, int whence)
{
    if (fd >= 0 && fd <= 2) {
        errno = ESPIPE;
        return -1;
    }

    int e = embed_slot(fd);

    if (e >= 0) {
        off_t base = (whence == SEEK_SET) ? 0 :
                     (whence == SEEK_CUR) ? (off_t)g_efd[e].pos :
                     (whence == SEEK_END) ? (off_t)g_efd[e].f->size : -1;
        off_t np = base + off;
        if (base < 0 || np < 0 || (size_t)np > g_efd[e].f->size) {
            errno = EINVAL;
            return -1;
        }
        g_efd[e].pos = (size_t)np;
        return np;
    }

    long r = myos_syscall3(SYS_SEEK, fd, (long)off, whence);

    /* сокет: как в Linux, "по нему не перемещаются" */
    if (r == MYOS_EINVAL) {
        struct myos_dirent d;
        if (myos_syscall3(SYS_FSTAT, fd, (long)&d, 0) == 0 && d.is_dir == MYOS_FT_SOCKET) {
            errno = ESPIPE;
            return -1;
        }
    }

    return (off_t)ret_of(r);
}

/* struct myos_dirent (что даёт ядро) -> struct stat */
static void to_stat(const struct myos_dirent *d, struct stat *st)
{
    memset(st, 0, sizeof(*st));
    st->st_nlink = 1;
    st->st_blksize = 4096;
    st->st_size = (off_t)d->size;
    st->st_blocks = (blkcnt_t)((d->size + 511u) / 512u);

    switch (d->is_dir) {
    case MYOS_FT_DIR:     st->st_mode = S_IFDIR | 0755; break;
    case MYOS_FT_CONSOLE: st->st_mode = S_IFCHR | 0620; break;
    case MYOS_FT_SOCKET:  st->st_mode = S_IFSOCK | 0666; break;
    default:              st->st_mode = S_IFREG | 0644; break;
    }
}

int stat(const char *path, struct stat *st)
{
    struct myos_dirent d;
    const struct myos_embed_file *ef = embed_find(path);

    if (ef != NULL) {
        embed_stat(ef, st);
        return 0;
    }

    if (embed_is_dir(path)) {
        memset(st, 0, sizeof(*st));
        st->st_mode = S_IFDIR | 0555;
        st->st_nlink = 2;
        return 0;
    }

    long r = myos_syscall3(SYS_STAT, (long)path, (long)&d, 0);

    if (r < 0)
        return myos_set_errno(r);

    to_stat(&d, st);
    return 0;
}

int lstat(const char *path, struct stat *st)
{
    return stat(path, st);     /* ссылок в файловых системах MyOS нет */
}

int fstat(int fd, struct stat *st)
{
    struct myos_dirent d;
    int e = embed_slot(fd);

    if (e >= 0) {
        embed_stat(g_efd[e].f, st);
        return 0;
    }

    long r = myos_syscall3(SYS_FSTAT, fd, (long)&d, 0);

    if (r < 0)
        return myos_set_errno(r);

    to_stat(&d, st);
    return 0;
}

int access(const char *path, int mode)
{
    struct stat st;

    (void)mode;                /* прав доступа в MyOS нет: есть - значит можно */
    return stat(path, &st);
}

int unlink(const char *path)
{
    return (int)ret_of(myos_syscall3(SYS_UNLINK, (long)path, 0, 0));
}

int rmdir(const char *path)
{
    return unlink(path);       /* ядро само удаляет пустую папку */
}

int mkdir(const char *path, mode_t mode)
{
    (void)mode;
    return (int)ret_of(myos_syscall3(SYS_MKDIR, (long)path, 0, 0));
}

int rename(const char *from, const char *to)
{
    /* POSIX: если "куда" уже есть - заменить; ядро MyOS так не умеет */
    struct stat st;

    if (strcmp(from, to) != 0 && stat(to, &st) == 0 && !S_ISDIR(st.st_mode))
        unlink(to);

    return (int)ret_of(myos_syscall3(SYS_RENAME, (long)from, (long)to, 0));
}

int chdir(const char *path)
{
    return (int)ret_of(myos_syscall3(SYS_CHDIR, (long)path, 0, 0));
}

char *getcwd(char *buf, size_t size)
{
    char tmp[256];
    long r = myos_syscall3(SYS_GETCWD, (long)tmp, sizeof(tmp), 0);

    if (r < 0) {
        myos_set_errno(r);
        return NULL;
    }

    if (buf == NULL) {
        /* расширение glibc: getcwd(NULL, 0) - выделить самой */
        return strdup(tmp);
    }

    if ((size_t)r + 1 > size) {
        errno = ERANGE;
        return NULL;
    }

    memcpy(buf, tmp, (size_t)r + 1);
    return buf;
}

int fsync(int fd)
{
    (void)fd;                  /* ядро пишет на диск сразу */
    return 0;
}

/* fcntl: неблокирующий режим сокетов (O_NONBLOCK); флаги "закрыть
   при exec" в MyOS ничего не значат */
int fcntl(int fd, int cmd, ...)
{
    va_list ap;
    int arg = 0;

    va_start(ap, cmd);
    if (cmd == F_SETFL || cmd == F_SETFD || cmd == F_DUPFD)
        arg = va_arg(ap, int);
    va_end(ap);

    switch (cmd) {
    case F_GETFD:
    case F_SETFD:
        return 0;
    case F_GETFL:
        if (is_socket(fd))
            return O_RDWR | (__myos_sock_nonblock(fd, 0, 0) ? O_NONBLOCK : 0);
        return O_RDWR;
    case F_SETFL:
        if (is_socket(fd))
            return __myos_sock_nonblock(fd, 1, (arg & O_NONBLOCK) != 0);
        return 0;
    default:
        errno = EINVAL;
        return -1;
    }
}

/* ioctl: только FIONBIO (неблокирующий сокет) */
int ioctl(int fd, unsigned long op, void *param)
{
    if (op == FIONBIO && is_socket(fd))
        return __myos_sock_nonblock(fd, 1, param != NULL && *(int *)param != 0);

    errno = ENOTTY;
    return -1;
}

int ftruncate(int fd, off_t len)
{
    (void)fd; (void)len;
    errno = ENOSYS;
    return -1;
}

/* Терминал: 0, 1, 2 - это консоль, остальное - нет */
int isatty(int fd)
{
    if (fd >= 0 && fd <= 2)
        return 1;

    errno = ENOTTY;
    return 0;
}

int tcgetattr(int fd, struct termios *t)
{
    if (fd < 0 || fd > 2) {
        errno = ENOTTY;
        return -1;
    }

    memset(t, 0, sizeof(*t));
    return 0;
}

int tcsetattr(int fd, int act, const struct termios *t)
{
    (void)act; (void)t;

    if (fd < 0 || fd > 2) {
        errno = ENOTTY;
        return -1;
    }

    return 0;                  /* режимы терминала MyOS не меняет */
}

/* ================================================================
 * Папки: opendir / readdir
 *
 * У ядра - readdir(путь, номер записи): берём записи по номеру.
 * Путь храним в буфере самого DIR (512 байт, путь MyOS - до 256).
 * ================================================================ */

DIR *opendir(const char *path)
{
    struct stat st;

    if (stat(path, &st) < 0)
        return NULL;

    if (!S_ISDIR(st.st_mode)) {
        errno = ENOTDIR;
        return NULL;
    }

    if (strlen(path) + 1 > sizeof(((DIR *)0)->buf)) {
        errno = ENAMETOOLONG;
        return NULL;
    }

    DIR *d = calloc(1, sizeof(DIR));

    if (d == NULL) {
        errno = ENOMEM;
        return NULL;
    }

    d->fd = -1;
    strcpy(d->buf, path);
    return d;
}

struct dirent *readdir(DIR *d)
{
    struct myos_dirent e;
    long r = myos_syscall3(SYS_READDIR, (long)d->buf, (long)d->offset, (long)&e);

    if (r <= 0) {
        if (r < 0)
            myos_set_errno(r);
        return NULL;           /* конец (или ошибка) */
    }

    d->offset++;
    d->dirent.d_ino = (ino_t)d->offset;
    d->dirent.d_type = e.is_dir ? DT_DIR : DT_REG;
    strncpy(d->dirent.d_name, e.name, sizeof(d->dirent.d_name) - 1);
    d->dirent.d_name[sizeof(d->dirent.d_name) - 1] = '\0';
    return &d->dirent;
}

void rewinddir(DIR *d)
{
    d->offset = 0;
}

void __myos_dir_forget(DIR *d) __attribute__((weak));   /* fs.c: dirfd */

int closedir(DIR *d)
{
    if (__myos_dir_forget != NULL)
        __myos_dir_forget(d);
    free(d);
    return 0;
}

/* ================================================================
 * Время
 *
 * Ядро даёт календарное время UTC (с точностью до секунды, из часов
 * CMOS) и миллисекунды со старта. "Сейчас" = время UTC при первом
 * вопросе + сколько прошло миллисекунд с тех пор: так время идёт
 * плавно и никогда не прыгает назад внутри программы.
 * ================================================================ */

/* дней от 1970-01-01 до даты (алгоритм Howard Hinnant, "days_from_civil") */
static int64_t days_from_civil(int y, unsigned m, unsigned d)
{
    y -= (m <= 2);
    int64_t era = (y >= 0 ? y : y - 399) / 400;
    unsigned yoe = (unsigned)(y - era * 400);
    unsigned doy = (153u * (m + (m > 2 ? (unsigned)-3 : 9u)) + 2u) / 5u + d - 1u;
    unsigned doe = yoe * 365u + yoe / 4u - yoe / 100u + doy;

    return era * 146097 + (int64_t)doe - 719468;
}

static int64_t tm_to_epoch(const struct myos_time *t)
{
    return days_from_civil(t->year, t->month, t->day) * 86400 +
           t->hour * 3600 + t->minute * 60 + t->second;
}

static int64_t g_epoch0_ms = -1;     /* время UTC (мс) при ... */
static uint64_t g_uptime0_ms;        /* ... этом значении uptime */

static uint64_t uptime_ms(void)
{
    return (uint64_t)myos_syscall3(SYS_UPTIME, 0, 0, 0);
}

static int64_t realtime_ms(void)
{
    if (g_epoch0_ms < 0) {
        struct myos_time t;
        g_uptime0_ms = uptime_ms();
        if (myos_syscall3(SYS_TIME, (long)&t, 1, 0) == 0)
            g_epoch0_ms = tm_to_epoch(&t) * 1000;
        else
            g_epoch0_ms = 0;
    }

    return g_epoch0_ms + (int64_t)(uptime_ms() - g_uptime0_ms);
}

int gettimeofday(struct timeval *restrict tv, void *restrict tz)
{
    (void)tz;

    if (tv != NULL) {
        int64_t ms = realtime_ms();
        tv->tv_sec = (time_t)(ms / 1000);
        tv->tv_usec = (suseconds_t)((ms % 1000) * 1000);
    }

    return 0;
}

int clock_gettime(clockid_t id, struct timespec *ts)
{
    int64_t ms;

    if (id == CLOCK_REALTIME)
        ms = realtime_ms();
    else if (id == CLOCK_MONOTONIC)
        ms = (int64_t)uptime_ms();
    else {
        errno = EINVAL;
        return -1;
    }

    ts->tv_sec = (time_t)(ms / 1000);
    ts->tv_nsec = (long)((ms % 1000) * 1000000);
    return 0;
}

int clock_getres(clockid_t id, struct timespec *ts)
{
    (void)id;

    if (ts != NULL) {
        ts->tv_sec = 0;
        ts->tv_nsec = 1000000;     /* таймер ядра тикает раз в 1 мс */
    }

    return 0;
}

clock_t times(struct tms *t)
{
    /* своё время процессора ядро не считает - отдаём время со старта
       программы (CLOCKS_PER_SEC у picolibc - см. <time.h>) */
    static uint64_t start = 0;
    uint64_t now = uptime_ms();

    if (start == 0)
        start = now;

    clock_t c = (clock_t)((now - start) * (CLOCKS_PER_SEC / 1000));

    if (t != NULL) {
        t->tms_utime = c;
        t->tms_stime = 0;
        t->tms_cutime = 0;
        t->tms_cstime = 0;
    }

    return c;
}

int nanosleep(const struct timespec *req, struct timespec *rem)
{
    uint64_t ms = (uint64_t)req->tv_sec * 1000u + (uint64_t)(req->tv_nsec + 999999) / 1000000u;

    if (rem != NULL) {
        rem->tv_sec = 0;
        rem->tv_nsec = 0;
    }

    return (int)ret_of(myos_syscall3(SYS_SLEEP, (long)ms, 0, 0));
}

/* ================================================================
 * Случайные числа (arc4random, ключи TLS)
 * ================================================================ */

int getentropy(void *buf, size_t n)
{
    if (n > 256) {
        errno = EIO;           /* как в POSIX: больше 256 байт за раз нельзя */
        return -1;
    }

    long r = myos_syscall3(SYS_GETRANDOM, (long)buf, (long)n, 0);

    return (r < 0) ? myos_set_errno(r) : 0;
}

/* ================================================================
 * Старт программы
 * ================================================================ */

int main(int argc, char **argv);
void __libc_init_array(void);

/*
 * Часовой пояс: ядро знает, сколько местное время отличается от UTC
 * (команда tz); libc узнаёт это из переменной TZ. Делаем строку
 * POSIX вида "MSK-3" (знак у POSIX обратный: "-3" = UTC+3).
 */
static void set_tz(void)
{
    struct myos_time l, u;

    if (getenv("TZ") != NULL)
        return;

    if (myos_syscall3(SYS_TIME, (long)&l, 0, 0) != 0 ||
        myos_syscall3(SYS_TIME, (long)&u, 1, 0) != 0)
        return;

    /* разница, округлённая до 15 минут (между двумя вызовами могла
       смениться секунда) */
    int64_t diff = tm_to_epoch(&l) - tm_to_epoch(&u);
    int64_t q = (diff >= 0 ? diff + 450 : diff - 450) / 900 * 900;
    char tz[32];
    int64_t a = (q < 0) ? -q : q;

    snprintf(tz, sizeof(tz), "LOC%c%d:%02d", (q > 0) ? '-' : '+',
             (int)(a / 3600), (int)(a % 3600 / 60));
    setenv("TZ", tz, 1);
    tzset();
}

/* Сюда прыгает crt0.S */
void __myos_start(int argc, char **argv) __attribute__((noreturn));

void __myos_start(int argc, char **argv)
{
    if (argc > 0 && argv[0] != NULL)
        g_progname = argv[0];
    set_tz();
    __libc_init_array();       /* конструкторы (__attribute__((constructor))) */
    exit(main(argc, argv));    /* exit сбросит буферы stdio и закроет файлы */
}
