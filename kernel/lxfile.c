/*
 * kernel/lxfile.c - файлы для программ Linux (этап 11): таблица
 * номеров файлов, обычные файлы (VFS), папки, каналы (pipe), терминал
 * (termios), /dev/null и компания, немного /proc. Часть MyOS.
 *
 * НОМЕР ФАЙЛА И "ОТКРЫТЫЙ ФАЙЛ"
 * ------------------------------
 * В Linux номер (fd) - это ссылка на "описание открытого файла"
 * (LFILE): там место чтения, флаги. dup(), dup2() и fork() дают НОВЫЙ
 * номер на ТО ЖЕ описание - общее место чтения (так шелл и его
 * потомки по очереди пишут в один файл, не затирая друг друга).
 * Поэтому у LFILE счётчик ссылок; закрывается он с последней.
 *
 * ПУТИ
 * ----
 * Программа Linux видит файлы MyOS как есть ("/usb0p1/...", "/ram",
 * "/bin"), плюс то, что она ждёт от любой Linux-системы:
 *   /dev/null, /dev/zero, /dev/urandom, /dev/random, /dev/tty;
 *   /proc/self/exe, /proc/self/fd/N, /proc/self/cwd (readlink);
 *   /proc/cpuinfo, /proc/meminfo, /proc/self/maps, /proc/self/stat...;
 *   /tmp - том tmpfs (файлы в памяти, fs/tmpfs.c), /dev/shm - /tmp/.shm.
 *
 * ТЕРМИНАЛ
 * --------
 * fd 0, 1, 2 при запуске - терминал процесса: консоль шелла или окно-
 * терминал рабочего стола (как у программ MyOS). Программа спрашивает
 * его режим (ioctl TCGETS) и может переключить:
 *   * обычный ("канонический") режим - ядро само собирает строку с
 *     эхом и Backspace, программа получает её целиком после Enter;
 *   * "сырой" (ICANON выключен, так делают шеллы с редактором строки,
 *     vi, less) - программа получает клавиши сразу; стрелки и пр. -
 *     как у терминала VT100 (ESC [ A ...).
 */
#include "linux.h"

/* ================================================================
 * Терминал: режим (termios) и уже прочитанное
 * ================================================================ */

#define LX_NCCS 19

typedef struct {
    UINT32 iflag, oflag, cflag, lflag;
    UINT8  line;
    UINT8  cc[LX_NCCS];
} __attribute__((packed)) LX_TERMIOS;

#define LX_ICRNL   0x100
#define LX_ISIG    0x1
#define LX_ICANON  0x2
#define LX_ECHO    0x8
#define LX_VTIME   5
#define LX_VMIN    6

typedef struct {
    BOOLEAN     used;
    KTTY       *tty;             /* окно-терминал или NULL - консоль */
    LX_TERMIOS  tio;
    UINT8       buf[512];        /* прочитано, но ещё не отдано программе */
    UINT32      len, off;
    INT32       pending_key;     /* консоль: клавиша, увиденная poll (0 - нет) */
    UINT32      pgrp;            /* группа процессов "переднего плана" */
    UINT16      rows, cols;
} LTERM;

static LTERM g_lterm[8];

static void lterm_defaults(LTERM *t)
{
    memset(&t->tio, 0, sizeof(t->tio));
    t->tio.iflag = 0x500;               /* ICRNL | IXON */
    t->tio.oflag = 0x5;                 /* OPOST | ONLCR */
    t->tio.cflag = 0xBF;                /* B38400 | CS8 | CREAD */
    t->tio.lflag = 0x8A3B;              /* ISIG ICANON ECHO ECHOE ECHOK ECHOCTL ECHOKE IEXTEN */
    t->tio.cc[0] = 3;                   /* VINTR  ^C */
    t->tio.cc[1] = 0x1C;                /* VQUIT  ^\ */
    t->tio.cc[2] = 0x7F;                /* VERASE */
    t->tio.cc[3] = 0x15;                /* VKILL  ^U */
    t->tio.cc[4] = 4;                   /* VEOF   ^D */
    t->tio.cc[LX_VMIN] = 1;
    t->tio.cc[8] = 0x11;                /* VSTART */
    t->tio.cc[9] = 0x13;                /* VSTOP */
    t->tio.cc[10] = 0x1A;               /* VSUSP  ^Z */
    t->tio.cc[12] = 0x12;
    t->tio.cc[13] = 0x0F;
    t->tio.cc[14] = 0x17;               /* VWERASE ^W */
    t->tio.cc[15] = 0x16;
    t->rows = 25;
    t->cols = TTY_COLS;
}

/* Терминал процесса (NULL - у графической программы его нет) */
static LTERM *lterm_of(KPROC *p)
{
    KTTY *key = NULL;

    if (p->io == PROC_IO_TTY)
        key = p->tty;
    else if (p->io != PROC_IO_CONSOLE)
        return NULL;

    LTERM *freeslot = NULL;

    for (UINTN i = 0; i < 8; i++) {
        if (g_lterm[i].used && g_lterm[i].tty == key)
            return &g_lterm[i];
        if (!g_lterm[i].used && freeslot == NULL)
            freeslot = &g_lterm[i];
    }

    if (freeslot == NULL) {
        /* все заняты (окна закрывались) - взять первый окна, а не консоли */
        for (UINTN i = 0; i < 8 && freeslot == NULL; i++)
            if (g_lterm[i].tty != NULL)
                freeslot = &g_lterm[i];
    }

    if (freeslot == NULL)
        return NULL;

    memset(freeslot, 0, sizeof(*freeslot));
    freeslot->used = TRUE;
    freeslot->tty = key;
    lterm_defaults(freeslot);
    if (key == NULL)
        freeslot->rows = (UINT16)SCROLLBACK_VISIBLE_ROWS;

    return freeslot;
}

/* Клавиша MyOS -> байты, как их шлёт терминал Linux (xterm/VT100) */
static UINTN key_to_bytes(INT32 k, UINT8 *out)
{
    if (k & MYOS_KEY_SPECIAL) {

        const char *s = NULL;

        switch (k & 0xFFFF) {
        case KEY_UP:    s = "\x1b[A"; break;
        case KEY_DOWN:  s = "\x1b[B"; break;
        case KEY_RIGHT: s = "\x1b[C"; break;
        case KEY_LEFT:  s = "\x1b[D"; break;
        case KEY_HOME:  s = "\x1b[H"; break;
        case KEY_END:   s = "\x1b[F"; break;
        case 0x07:      s = "\x1b[2~"; break;     /* Insert */
        case KEY_DEL:   s = "\x1b[3~"; break;
        case KEY_PGUP:  s = "\x1b[5~"; break;
        case KEY_PGDN:  s = "\x1b[6~"; break;
        case KEY_ESC:   s = "\x1b"; break;
        case 0x0B: s = "\x1bOP"; break;           /* F1..F4 */
        case 0x0C: s = "\x1bOQ"; break;
        case 0x0D: s = "\x1bOR"; break;
        case 0x0E: s = "\x1bOS"; break;
        default: return 0;
        }

        UINTN n = 0;
        while (s[n]) {
            out[n] = (UINT8)s[n];
            n++;
        }
        return n;
    }

    if (k == '\b') {
        out[0] = 0x7F;          /* Backspace терминала Linux - DEL */
        return 1;
    }

    if (k == 0x1B) {
        out[0] = 0x1B;
        return 1;
    }

    return utf8_put((UINT32)k, (char *)out);
}

/* Клавиша без ожидания: консоль (через "заглянувшую" poll) или окно */
static INT32 lterm_getkey(KPROC *p, LTERM *t, INT64 timeout_ms)
{
    if (p->io == PROC_IO_TTY && p->tty != NULL)
        return tty_getkey(p->tty, p, timeout_ms);

    if (t->pending_key != 0) {
        INT32 k = t->pending_key;
        t->pending_key = 0;
        return k;
    }

    UINT64 t0 = g_kticks;

    for (;;) {

        EFI_INPUT_KEY key;

        if (g_st->ConIn->ReadKeyStroke(g_st->ConIn, &key) == EFI_SUCCESS) {
            if (key.UnicodeChar != 0)
                return key.UnicodeChar;
            return MYOS_KEY_SPECIAL | key.ScanCode;
        }

        if (p->killed || (timeout_ms >= 0 && g_kticks - t0 >= (UINT64)timeout_ms))
            return -1;

        sched_sleep_ms(10);
    }
}

/* Есть ли клавиша (для poll), не забирая её */
static BOOLEAN lterm_key_ready(KPROC *p, LTERM *t)
{
    if (t->len > t->off)
        return TRUE;

    if (p->io == PROC_IO_TTY && p->tty != NULL)
        return tty_key_ready(p->tty);

    if (t->pending_key != 0)
        return TRUE;

    EFI_INPUT_KEY key;

    if (g_st->ConIn->ReadKeyStroke(g_st->ConIn, &key) == EFI_SUCCESS) {
        t->pending_key = key.UnicodeChar ? key.UnicodeChar : (MYOS_KEY_SPECIAL | key.ScanCode);
        return TRUE;
    }

    return FALSE;
}

/* Строка с эхом (канонический режим) в t->buf */
static INTN lterm_read_line(KPROC *p, LTERM *t)
{
    if (p->io == PROC_IO_TTY && p->tty != NULL) {
        INTN n = tty_read_line(p->tty, p, (char *)t->buf, sizeof(t->buf));
        if (n > 0) {
            t->len = (UINT32)n;
            t->off = 0;
        }
        return n;
    }

    /* консоль: свой маленький редактор строки (клавиши - через
       lterm_getkey, чтобы не потерять "заглянутую" poll) */
    UINT32 len = 0;
    BOOLEAN echo = (t->tio.lflag & LX_ECHO) != 0;

    for (;;) {

        INT32 k = lterm_getkey(p, t, 100);

        if (k < 0) {
            if (p->killed)
                return 0;
            if (lx_signal_pending())
                return -LX_ERESTARTSYS;
            continue;
        }

        if (k == '\r' || k == '\n') {
            if (echo) {
                print(g_st->ConOut, "\n");
                kcon_flush();
            }
            t->buf[len++] = '\n';
            break;
        }

        if (k == 4 && len == 0)             /* Ctrl+D в пустой строке - конец ввода */
            break;

        if (k == '\b' || k == 0x7F) {
            if (len > 0) {
                while (len > 0 && (t->buf[len - 1] & 0xC0) == 0x80)
                    len--;
                if (len > 0)
                    len--;
                if (echo) {
                    print(g_st->ConOut, "\b \b");
                    kcon_flush();
                }
            }
            continue;
        }

        if (k >= 32 && !(k & MYOS_KEY_SPECIAL) && len + 6 < sizeof(t->buf)) {
            char u[5];
            UINTN m = utf8_put((UINT32)k, u);
            for (UINTN i = 0; i < m; i++)
                t->buf[len++] = (UINT8)u[i];
            if (echo) {
                u[m] = '\0';
                proc_out(p, u, m);
            }
        }
    }

    t->len = len;
    t->off = 0;
    return len;
}

static INT64 lterm_read(KPROC *p, LFILE *f, UINT8 *dst, UINTN n)
{
    LTERM *t = lterm_of(p);

    if (t == NULL || n == 0)
        return 0;               /* у графической программы ввода нет */

    p->raw_keys = TRUE;         /* Ctrl+C - нам (сигнал SIGINT), а не "убить" */

    for (;;) {

        if (t->len > t->off) {
            UINTN k = t->len - t->off;
            if (k > n)
                k = n;
            memcpy(dst, t->buf + t->off, k);
            t->off += (UINT32)k;
            if (t->off >= t->len)
                t->len = t->off = 0;
            return (INT64)k;
        }

        if (p->killed)
            return 0;

        if (t->tio.lflag & LX_ICANON) {
            INTN r = lterm_read_line(p, t);
            if (r < 0)
                return r;
            if (r == 0)
                return 0;           /* Ctrl+D или конец */
            continue;
        }

        /* сырой режим: VMIN/VTIME как у Linux (упрощённо) */
        BOOLEAN nonblock = (f->flags & LX_O_NONBLOCK) != 0;
        UINT8 vmin = t->tio.cc[LX_VMIN], vtime = t->tio.cc[LX_VTIME];
        INT64 wait = nonblock ? 0 : (vmin > 0 ? -1 : (INT64)vtime * 100);
        UINT64 t0 = g_kticks;
        INT32 key = -1;

        for (;;) {
            INT64 slice = (wait < 0) ? 100 : wait - (INT64)(g_kticks - t0);
            if (slice < 0)
                slice = 0;
            if (slice > 100)
                slice = 100;
            key = lterm_getkey(p, t, slice);
            if (key >= 0 || p->killed)
                break;
            if (lx_signal_pending())
                return -LX_ERESTARTSYS;
            if (wait >= 0 && g_kticks - t0 >= (UINT64)wait)
                break;
        }

        if (key < 0)
            return nonblock ? -LX_EAGAIN : 0;

        UINT8 b[8];
        UINTN m = key_to_bytes(key, b);

        if (m == 1 && b[0] == '\r' && (t->tio.iflag & LX_ICRNL))
            b[0] = '\n';

        for (UINTN i = 0; i < m && t->len < sizeof(t->buf); i++)
            t->buf[t->len++] = b[i];

        if ((t->tio.lflag & LX_ECHO) && m > 0 && b[0] >= 32 && b[0] != 0x7F)
            proc_out_tty(p, (const char *)b, m);
    }
}

/* Вывод на терминал процесса */
void proc_out_tty(KPROC *p, const char *s, UINTN n)
{
    if (p->io == PROC_IO_TTY && p->tty != NULL)
        tty_write(p->tty, s, n);
    else
        proc_out(p, s, n);
}

/* Ctrl+C в терминале программы Linux (keyboard.c, gui/tty.c) */
BOOLEAN lx_term_isig(KPROC *p)
{
    LTERM *t = lterm_of(p);
    return t == NULL || (t->tio.lflag & LX_ISIG) != 0;
}


/* ================================================================
 * Каналы (pipe)
 * ================================================================ */

#define LPIPE_SIZE 65536u

typedef struct LPIPE {
    UINT8  *buf;
    UINT32  head, len;          /* данные: [head, head+len) по кругу */
    UINT32  readers, writers;
} LPIPE;

/* Событие "что-то изменилось" для poll/select: каналы, терминал,
   eventfd. Сеть будит своим g_net_any_event - poll смотрит и туда,
   просыпаясь кусками по 10 мс. */
static UINT8 g_lx_poll_event;

void lx_poll_wake(void)
{
    sched_wake_all(&g_lx_poll_event);
}

static INT64 pipe_read(LPIPE *pp, LFILE *f, UINT8 *dst, UINTN n)
{
    UINT64 fl = kx_irq_save();

    for (;;) {

        if (pp->len > 0) {

            UINTN k = (n < pp->len) ? n : pp->len;

            for (UINTN i = 0; i < k; i++)
                dst[i] = pp->buf[(pp->head + i) % LPIPE_SIZE];

            pp->head = (UINT32)((pp->head + k) % LPIPE_SIZE);
            pp->len -= (UINT32)k;

            kx_irq_restore(fl);
            sched_wake_all(pp);
            lx_poll_wake();
            return (INT64)k;
        }

        if (pp->writers == 0) {
            kx_irq_restore(fl);
            return 0;                   /* все пишущие закрыли - конец */
        }

        if (f->flags & LX_O_NONBLOCK) {
            kx_irq_restore(fl);
            return -LX_EAGAIN;
        }

        if (g_kcur->proc->killed || lx_signal_pending()) {
            kx_irq_restore(fl);
            return -LX_ERESTARTSYS;
        }

        sched_block(pp, "pipe read", 100);
    }
}

static INT64 pipe_write(LPIPE *pp, LFILE *f, const UINT8 *src, UINTN n)
{
    UINTN done = 0;
    UINT64 fl = kx_irq_save();

    while (done < n) {

        if (pp->readers == 0) {
            kx_irq_restore(fl);
            lx_signal_send(g_kcur->proc, LX_SIGPIPE);
            return done ? (INT64)done : -LX_EPIPE;
        }

        UINT32 room = LPIPE_SIZE - pp->len;
        UINTN want = n - done;

        /* до 4 КиБ (PIPE_BUF) - целиком или никак: так строки разных
           писателей не перемешиваются */
        if (room > 0 && (want > 4096u || room >= want)) {

            UINTN k = (want < room) ? want : room;

            for (UINTN i = 0; i < k; i++)
                pp->buf[(pp->head + pp->len + i) % LPIPE_SIZE] = src[done + i];

            pp->len += (UINT32)k;
            done += k;

            sched_wake_all(pp);
            lx_poll_wake();
            continue;
        }

        if (f->flags & LX_O_NONBLOCK) {
            kx_irq_restore(fl);
            return done ? (INT64)done : -LX_EAGAIN;
        }

        if (g_kcur->proc->killed || lx_signal_pending()) {
            kx_irq_restore(fl);
            return done ? (INT64)done : -LX_ERESTARTSYS;
        }

        sched_block(pp, "pipe write", 100);
    }

    kx_irq_restore(fl);
    return (INT64)done;
}


/* ================================================================
 * Описания открытых файлов
 * ================================================================ */

static LFILE *lfile_new(UINT32 type, UINT32 flags)
{
    LFILE *f = (LFILE *)kzalloc(sizeof(LFILE));

    if (f == NULL)
        return NULL;

    f->refs = 1;
    f->type = type;
    f->flags = flags;
    f->kfd = -1;
    return f;
}

void lfile_unref(LFILE *f)
{
    if (f == NULL || f->refs == 0)
        return;

    if (--f->refs > 0)
        return;

    switch (f->type) {

    case LF_VFS:
        if (f->kfd >= 0)
            vfs_close(f->kfd);
        break;

    case LF_PIPE_R:
    case LF_PIPE_W: {
        LPIPE *pp = f->pipe;
        if (f->type == LF_PIPE_R)
            pp->readers--;
        else
            pp->writers--;
        sched_wake_all(pp);
        lx_poll_wake();
        if (pp->readers == 0 && pp->writers == 0) {
            kfree(pp->buf);
            kfree(pp);
        }
        break;
    }

    case LF_MEM:
        kfree(f->mem);
        break;
    }

    kfree(f);
}

static LFILE *fd_get(KPROC *p, INT64 fd)
{
    if (p->lx == NULL || fd < 0 || fd >= LX_FDS)
        return NULL;

    return p->lx->fd[fd];
}

/* Самый маленький свободный номер не меньше from */
static INT64 fd_install(KPROC *p, LFILE *f, INT64 from, BOOLEAN cloexec)
{
    for (INT64 i = (from < 0 ? 0 : from); i < LX_FDS; i++)
        if (p->lx->fd[i] == NULL) {
            p->lx->fd[i] = f;
            p->lx->cloexec[i] = cloexec ? 1 : 0;
            return i;
        }

    return -LX_EMFILE;
}

LFILE *lx_fd_get(KPROC *p, INT64 fd)
{
    return fd_get(p, fd);
}

static void fd_close(KPROC *p, INT64 fd)
{
    LFILE *f = p->lx->fd[fd];

    p->lx->fd[fd] = NULL;
    p->lx->cloexec[fd] = 0;
    lfile_unref(f);
}

/* Новый процесс Linux: fd 0, 1, 2 - терминал; вывод "> файл" от шелла
   MyOS (p->out_kfd) становится fd 1 и 2 */
void lx_files_init(KPROC *p)
{
    LFILE *tty = lfile_new(LF_TTY, LX_O_RDWR);

    if (tty == NULL)
        return;

    ksnprintf(tty->path, sizeof(tty->path), "/dev/tty");
    p->lx->fd[0] = tty;

    if (p->out_kfd >= 0) {
        LFILE *o = lfile_new(LF_VFS, LX_O_WRONLY);
        if (o != NULL) {
            o->kfd = p->out_kfd;
            p->out_kfd = -1;            /* теперь файл закроет LFILE */
            o->refs = 2;
            p->lx->fd[1] = o;
            p->lx->fd[2] = o;
            return;
        }
    }

    tty->refs = 3;
    p->lx->fd[1] = tty;
    p->lx->fd[2] = tty;
}

/* fork: потомок видит те же открытые файлы */
BOOLEAN lx_files_fork(KPROC *parent, KPROC *child)
{
    for (UINTN i = 0; i < LX_FDS; i++) {
        LFILE *f = parent->lx->fd[i];
        if (f != NULL) {
            f->refs++;
            child->lx->fd[i] = f;
            child->lx->cloexec[i] = parent->lx->cloexec[i];
        }
    }

    return TRUE;
}

/* execve: файлы с FD_CLOEXEC закрываются */
void lx_files_exec(KPROC *p)
{
    for (UINTN i = 0; i < LX_FDS; i++)
        if (p->lx->fd[i] != NULL && p->lx->cloexec[i])
            fd_close(p, (INT64)i);
}

void lx_files_free(KPROC *p)
{
    if (p->lx == NULL)
        return;

    for (UINTN i = 0; i < LX_FDS; i++)
        if (p->lx->fd[i] != NULL)
            fd_close(p, (INT64)i);
}


/* ================================================================
 * Пути
 * ================================================================ */

/* Начинается ли s с префикса pre (целым элементом пути) */
static BOOLEAN path_is(const char *s, const char *pre, const char **rest)
{
    UINTN n = 0;

    while (pre[n]) {
        if (s[n] != pre[n])
            return FALSE;
        n++;
    }

    if (s[n] != '\0' && s[n] != '/')
        return FALSE;

    if (rest != NULL)
        *rest = s + n;

    return TRUE;
}

/* ================================================================
 * Корень Linux (этап 11, шаг 2)
 *
 * Программам Linux нужен привычный "/": /usr/bin, /usr/lib, /etc.
 * Если в MyOS смонтирован раздел ext4 с Linux (корень Arch на
 * ноутбуке - /nvme0p2), он становится корнем Linux для программ:
 *
 *   путь Linux            путь MyOS
 *   /usr/lib/libc.so.6 -> /nvme0p2/usr/lib/libc.so.6
 *   /tmp/x             -> /tmp/x       (tmpfs MyOS: писать можно)
 *   /dev/null, /proc -> свои   (kernel/lxfile.c)
 *   /home/...          -> том из /etc/fstab этого Linux (UUID=...)
 *   /myos/usb0p1/a     -> /usb0p1/a    (выход к томам MyOS)
 *   /usb0p1/a          -> /usb0p1/a    (имя тома MyOS, которого нет
 *                                       в корне Linux, - тоже можно)
 *
 * Символьные ссылки разбираются ЗДЕСЬ, по правилам Linux: ссылка
 * "/usr/lib/x" - от корня Linux, "../proc/self/mounts" - может уйти
 * из тома в /proc. В VFS приходит путь MyOS уже без ссылок.
 *
 * Корень выбирается при запуске программы из шелла MyOS (у потомков -
 * тот же). Нет раздела Linux - всё как на шаге 1: пути = пути MyOS.
 * ================================================================ */

/* Точки монтирования из /etc/fstab корня Linux */
typedef struct {
    char point[64];             /* "/home" */
    char vol[16];               /* том MyOS: "nvme0p3" */
} LX_MNT;

#define LX_MNT_MAX 8

static LX_MNT g_lx_mnt[LX_MNT_MAX];
static UINTN  g_lx_nmnt;
static char   g_lx_mnt_root[16];        /* для какого корня прочитан fstab */

static BOOLEAN str_eq(const char *a, const char *b)
{
    while (*a && *a == *b) {
        a++;
        b++;
    }

    return *a == *b;
}

/* Есть ли путь MyOS (не проходя последнюю ссылку) */
static BOOLEAN my_exists(const char *my)
{
    VFS_DIRENT *e = (VFS_DIRENT *)kmalloc(sizeof(VFS_DIRENT));

    if (e == NULL)
        return FALSE;

    BOOLEAN ok = vfs_lstat(my, e) == VFS_OK;

    kfree(e);
    return ok;
}

/* Прочитать /etc/fstab корня root: какие ещё тома и куда */
static void read_fstab(const char *root)
{
    char path[64];
    UINTN got = 0;
    char *buf = (char *)kmalloc(8192);

    g_lx_nmnt = 0;
    ksnprintf(g_lx_mnt_root, sizeof(g_lx_mnt_root), "%s", root);

    if (buf == NULL)
        return;

    ksnprintf(path, sizeof(path), "/%s/etc/fstab", root);

    if (vfs_read_file(path, buf, 8191, &got) != VFS_OK)
        got = 0;

    buf[got] = '\0';

    char *line = buf;

    while (*line && g_lx_nmnt < LX_MNT_MAX) {

        char *end = line;

        while (*end && *end != '\n')
            end++;

        char saved = *end;
        *end = '\0';

        /* поля: что, куда, тип, ... */
        char *f[3] = { NULL, NULL, NULL };
        UINTN nf = 0;
        char *c = line;

        while (*c && nf < 3) {
            while (*c == ' ' || *c == '\t')
                c++;
            if (!*c || *c == '#')
                break;
            f[nf++] = c;
            while (*c && *c != ' ' && *c != '\t')
                c++;
            if (*c)
                *c++ = '\0';
        }

        if (nf >= 2 && f[1][0] == '/' && f[1][1] != '\0') {

            /* UUID=... или LABEL=... - ищем такой том ext4 среди томов MyOS */
            BOOLEAN by_uuid = (f[0][0] == 'U' && f[0][1] == 'U' && f[0][2] == 'I' &&
                               f[0][3] == 'D' && f[0][4] == '=');
            BOOLEAN by_label = (f[0][0] == 'L' && f[0][1] == 'A' && f[0][2] == 'B' &&
                                f[0][3] == 'E' && f[0][4] == 'L' && f[0][5] == '=');
            const char *want = by_uuid ? f[0] + 5 : by_label ? f[0] + 6 : NULL;

            for (UINTN i = 0; want != NULL && i < VFS_MAX_MOUNTS; i++) {

                VFS_MOUNT *m = &g_mounts[i];
                char uuid[40];

                if (!m->used || m->ops != &g_ext4_ops || str_eq(m->name, root))
                    continue;

                BOOLEAN hit = by_uuid ? (ext4_uuid(m, uuid, sizeof(uuid)) && str_eq(uuid, want))
                                      : str_eq(ext4_label(m), want);

                if (hit) {
                    LX_MNT *x = &g_lx_mnt[g_lx_nmnt++];
                    ksnprintf(x->point, sizeof(x->point), "%s", f[1]);
                    ksnprintf(x->vol, sizeof(x->vol), "%s", m->name);
                    klog("linux: %s from /etc/fstab -> /%s\n", x->point, x->vol);
                    break;
                }
            }
        }

        *end = saved;
        line = (*end) ? end + 1 : end;
    }

    kfree(buf);
}

/*
 * Том с корнем Linux: первый том ext4, где есть /etc/os-release (или
 * /usr/lib/os-release). FALSE - такого нет.
 */
BOOLEAN lx_root_volume(char *out, UINTN cap)
{
    for (UINTN i = 0; i < VFS_MAX_MOUNTS; i++) {

        VFS_MOUNT *m = &g_mounts[i];
        char path[64];
        VFS_DIRENT *e;

        if (!m->used || m->ops != &g_ext4_ops)
            continue;

        e = (VFS_DIRENT *)kmalloc(sizeof(VFS_DIRENT));

        if (e == NULL)
            return FALSE;

        ksnprintf(path, sizeof(path), "/%s/etc/os-release", m->name);
        BOOLEAN ok = vfs_stat(path, e) == VFS_OK;

        if (!ok) {
            ksnprintf(path, sizeof(path), "/%s/usr/lib/os-release", m->name);
            ok = vfs_stat(path, e) == VFS_OK;
        }

        kfree(e);

        if (ok) {
            ksnprintf(out, cap, "%s", m->name);
            return TRUE;
        }
    }

    return FALSE;
}

/* Корень Linux для программы, запущенной из шелла MyOS (у потомков -
   тот же, см. sys_clone) */
void lx_choose_root(KPROC *p)
{
    if (p->lx == NULL)
        return;

    if (!lx_root_volume(p->lx->root, sizeof(p->lx->root)))
        p->lx->root[0] = '\0';

    if (p->lx->root[0] != '\0' && !str_eq(g_lx_mnt_root, p->lx->root))
        read_fstab(p->lx->root);
}

/* Области, которые MyOS делает сама (не из корня Linux) */
static BOOLEAN lx_own_area(const char *lin)
{
    return path_is(lin, "/dev", NULL) || path_is(lin, "/proc", NULL) ||
           path_is(lin, "/sys", NULL) || path_is(lin, "/tmp", NULL);
}

/* Путь Linux (полный, без ссылок, "." и "..") -> путь MyOS */
static void lin_to_myos(KPROC *p, const char *lin, char *out, UINTN cap)
{
    const char *root = (p->lx != NULL) ? p->lx->root : "";
    const char *rest;

    /* /dev/shm (общая память POSIX) - это папка .shm в /tmp */
    if (path_is(lin, "/dev/shm", &rest)) {
        ksnprintf(out, cap, "/tmp/.shm%s", rest);
        return;
    }

    if (root[0] == '\0' || lx_own_area(lin)) {
        ksnprintf(out, cap, "%s", lin);
        return;
    }

    if (lin[0] == '/' && lin[1] == '\0') {
        ksnprintf(out, cap, "/%s", root);
        return;
    }

    if (path_is(lin, "/myos", &rest)) {
        ksnprintf(out, cap, "%s", rest[0] ? rest : "/");
        return;
    }

    for (UINTN i = 0; i < g_lx_nmnt; i++)
        if (path_is(lin, g_lx_mnt[i].point, &rest)) {
            ksnprintf(out, cap, "/%s%s", g_lx_mnt[i].vol, rest);
            return;
        }

    /* /usb0p1/...: том MyOS, если в корне Linux такого имени нет */
    char first[VFS_NAME_MAX + 2];
    UINTN k = 0;

    for (; lin[k + 1] && lin[k + 1] != '/' && k + 2 < sizeof(first); k++)
        first[k + 1] = lin[k + 1];

    first[0] = '/';
    first[k + 1] = '\0';

    if (my_exists(first)) {
        char in_root[VFS_NAME_MAX + 24];
        ksnprintf(in_root, sizeof(in_root), "/%s%s", root, first);
        if (!my_exists(in_root)) {
            ksnprintf(out, cap, "%s", lin);
            return;
        }
    }

    ksnprintf(out, cap, "/%s%s", root, lin);
}

/* Путь MyOS -> как его видит программа Linux (getcwd, /proc/self/exe) */
void lx_path_to_linux(KPROC *p, const char *my, char *out, UINTN cap)
{
    const char *root = (p->lx != NULL) ? p->lx->root : "";
    const char *rest;
    char pre[24];

    if (root[0] == '\0' || lx_own_area(my)) {
        ksnprintf(out, cap, "%s", my);
        return;
    }

    ksnprintf(pre, sizeof(pre), "/%s", root);

    if (path_is(my, pre, &rest)) {
        ksnprintf(out, cap, "%s", rest[0] ? rest : "/");
        return;
    }

    for (UINTN i = 0; i < g_lx_nmnt; i++) {
        ksnprintf(pre, sizeof(pre), "/%s", g_lx_mnt[i].vol);
        if (path_is(my, pre, &rest)) {
            ksnprintf(out, cap, "%s%s", g_lx_mnt[i].point, rest);
            return;
        }
    }

    /* остальное MyOS: прямо, если имя тома не спорит с корнем Linux
       (/usb0p1), иначе через /myos (/bin - у Linux свой /bin) */
    char first[VFS_NAME_MAX + 24];
    UINTN n = (UINTN)ksnprintf(first, sizeof(first), "/%s", root);

    for (UINTN k = 0; my[k] && (k == 0 || my[k] != '/') && n + 1 < sizeof(first); k++)
        first[n++] = my[k];

    first[n] = '\0';

    if (my[1] == '\0' || my_exists(first))
        ksnprintf(out, cap, "/myos%s", (my[1] == '\0') ? "" : my);
    else
        ksnprintf(out, cap, "%s", my);
}

/*
 * Разобрать путь Linux (полный) по элементам, проходя символьные
 * ссылки по правилам Linux. follow - проходить ли ссылку в самом
 * конце (lstat, readlink, unlink - нет). Результат - путь MyOS.
 */
static INTN lx_walk(KPROC *p, const char *lpath, BOOLEAN follow, char *out, UINTN cap)
{
    char *work = (char *)kmalloc(VFS_PATH_MAX * 2);
    char *nw = (char *)kmalloc(VFS_PATH_MAX * 2);
    VFS_DIRENT *e = (VFS_DIRENT *)kmalloc(sizeof(VFS_DIRENT));
    char lin[VFS_PATH_MAX];
    char my[VFS_PATH_MAX];
    char tgt[VFS_PATH_MAX];
    UINTN hops = 0, i = 0;
    INTN r = 0;

    if (work == NULL || nw == NULL || e == NULL) {
        r = -LX_ENOMEM;
        goto out;
    }

    ksnprintf(work, VFS_PATH_MAX * 2, "%s", lpath);
    lin[0] = '\0';

    for (;;) {

        while (work[i] == '/')
            i++;

        if (work[i] == '\0')
            break;

        UINTN s = i;

        while (work[i] && work[i] != '/')
            i++;

        UINTN len = i - s, j = i;

        while (work[j] == '/')
            j++;

        BOOLEAN last = work[j] == '\0';

        if (len == 1 && work[s] == '.')
            continue;

        UINTN ll = 0;

        while (lin[ll])
            ll++;

        if (len == 2 && work[s] == '.' && work[s + 1] == '.') {
            while (ll > 0 && lin[ll - 1] != '/')
                ll--;
            if (ll > 0)
                ll--;
            lin[ll] = '\0';
            continue;
        }

        if (ll + 1 + len + 1 > sizeof(lin) || len >= VFS_NAME_MAX) {
            r = -LX_ENAMETOOLONG;
            goto out;
        }

        lin[ll] = '/';
        memcpy(lin + ll + 1, work + s, len);
        lin[ll + 1 + len] = '\0';

        if (last && !follow)
            break;

        /* /dev, /proc, /sys, /tmp - ссылок там нет */
        if (p->lx != NULL && p->lx->root[0] != '\0' && lx_own_area(lin))
            continue;

        lin_to_myos(p, lin, my, sizeof(my));

        if (vfs_lstat(my, e) != VFS_OK || !VFS_IS_LINK(&e->node))
            continue;

        /* символьная ссылка: дальше разбираем (цель + остаток пути) */
        if (++hops > 40) {
            r = -LX_ELOOP;
            goto out;
        }

        INTN n = vfs_readlink(my, tgt, sizeof(tgt));

        if (n <= 0) {
            r = -LX_ENOENT;
            goto out;
        }

        ksnprintf(nw, VFS_PATH_MAX * 2, "%s%s", tgt, work + i);
        ksnprintf(work, VFS_PATH_MAX * 2, "%s", nw);
        i = 0;
        lin[ll] = '\0';                     /* сама ссылка - не часть пути */

        if (tgt[0] == '/')
            lin[0] = '\0';                  /* от корня Linux */
    }

    if (lin[0] == '\0') {
        lin[0] = '/';
        lin[1] = '\0';
    }

    lin_to_myos(p, lin, out, cap);

out:
    kfree(work);
    kfree(nw);
    kfree(e);
    return r;
}

/*
 * Путь (строка ядра) -> путь MyOS. dirfd - от какой папки считать
 * относительный (AT_FDCWD - текущая).
 */
INTN lx_resolve_kpath(KPROC *p, INT64 dirfd, const char *kpath, BOOLEAN follow, char *out,
                      UINTN cap)
{
    char joined[VFS_PATH_MAX * 2];

    /* номер папки - int (как у Linux): AT_FDCWD (-100) может прийти и
       без знакового расширения в старшие 32 бита регистра */
    dirfd = (INT32)dirfd;

    if (kpath[0] == '\0')
        return -LX_ENOENT;

    if (kpath[0] == '/') {
        ksnprintf(joined, sizeof(joined), "%s", kpath);
    } else {
        const char *base = p->cwd;
        char lbase[VFS_PATH_MAX];
        if (dirfd != LX_AT_FDCWD) {
            LFILE *d = fd_get(p, dirfd);
            if (d == NULL)
                return -LX_EBADF;
            if (d->type != LF_DIR)
                return -LX_ENOTDIR;
            base = d->path;
        }
        lx_path_to_linux(p, base, lbase, sizeof(lbase));
        ksnprintf(joined, sizeof(joined), "%s/%s", lbase, kpath);
    }

    return lx_walk(p, joined, follow, out, cap);
}

/* Путь из памяти программы -> путь MyOS (последняя ссылка - по follow) */
static INTN lx_resolve_path_ex(KPROC *p, INT64 dirfd, UINT64 upath, BOOLEAN follow, char *out,
                               UINTN cap)
{
    char tmp[VFS_PATH_MAX];

    for (UINTN i = 0; ; i++) {

        if (i + 1 >= sizeof(tmp))
            return -LX_ENAMETOOLONG;

        if (i == 0 || ((upath + i) & 0xFFFu) == 0)
            if (!uptr_ok(p, upath + i, 1, FALSE))
                return -LX_EFAULT;

        tmp[i] = *(volatile char *)(UINTN)(upath + i);

        if (tmp[i] == '\0')
            break;
    }

    return lx_resolve_kpath(p, dirfd, tmp, follow, out, cap);
}

INTN lx_resolve_path(KPROC *p, INT64 dirfd, UINT64 upath, char *out, UINTN cap)
{
    return lx_resolve_path_ex(p, dirfd, upath, TRUE, out, cap);
}

/* /proc/self/... и /proc/<pid>/... -> процесс и остаток пути */
static KPROC *proc_path(KPROC *p, const char *path, const char **rest)
{
    const char *r;

    if (path_is(path, "/proc/self", &r) || path_is(path, "/proc/thread-self", &r)) {
        *rest = r;
        return p;
    }

    if (!path_is(path, "/proc", &r) || r[0] != '/' || r[1] < '0' || r[1] > '9')
        return NULL;

    UINT32 pid = 0;
    r++;

    while (*r >= '0' && *r <= '9')
        pid = pid * 10u + (UINT32)(*r++ - '0');

    if (*r != '\0' && *r != '/')
        return NULL;

    for (UINTN i = 0; i < PROC_MAX; i++)
        if (g_procs[i].used && g_procs[i].pid == pid) {
            *rest = r;
            return &g_procs[i];
        }

    return NULL;
}

/* Путь программы для execve: /proc/self/exe - её собственный файл */
INTN lx_lookup_exec(KPROC *p, const char *path, char *real, UINTN cap)
{
    const char *rest;
    KPROC *q = proc_path(p, path, &rest);

    if (q != NULL && path_is(rest, "/exe", NULL)) {
        ksnprintf(real, cap, "%s", (q->lx && q->lx->exe[0]) ? q->lx->exe : q->path);
        return 0;
    }

    ksnprintf(real, cap, "%s", path);
    return 0;
}


/* ================================================================
 * stat
 * ================================================================ */

typedef struct {
    UINT64 dev, ino, nlink;
    UINT32 mode, uid, gid, pad0;
    UINT64 rdev;
    INT64  size, blksize, blocks;
    UINT64 atime, atime_ns, mtime, mtime_ns, ctime, ctime_ns;
    INT64  reserved[3];
} LX_STAT;

/* Номер "inode": у FAT своих нет - хэш полного пути (FNV-1a), чтобы
   разные файлы различались (ls, find, cp сравнивают ino) */
static UINT64 path_ino(const char *path)
{
    UINT64 h = 1469598103934665603ull;

    for (; *path; path++) {
        char c = *path;
        if (c >= 'A' && c <= 'Z')
            c = (char)(c - 'A' + 'a');      /* FAT не различает регистр */
        h = (h ^ (UINT8)c) * 1099511628211ull;
    }

    return h ? h : 1;
}

/* Дата FAT -> секунды с 1970 года */
static UINT64 fat_to_unix(UINT16 d, UINT16 t)
{
    if (d == 0)
        return 0;

    UINT32 y = 1980u + (d >> 9), m = (d >> 5) & 15u, day = d & 31u;
    static const UINT16 cum[12] = { 0, 31, 59, 90, 120, 151, 181, 212, 243, 273, 304, 334 };

    if (m < 1 || m > 12)
        m = 1;

    UINT64 days = (UINT64)(y - 1970u) * 365u + (y - 1969u) / 4u + cum[m - 1] + (day ? day - 1 : 0);

    if (m > 2 && (y % 4u) == 0)
        days++;

    return days * 86400u + (UINT64)(t >> 11) * 3600u + (UINT64)((t >> 5) & 63u) * 60u +
           (UINT64)(t & 31u) * 2u;
}

static void fill_stat(LX_STAT *st, const char *path, UINT32 mode, UINT64 size, UINT64 mtime)
{
    memset(st, 0, sizeof(*st));
    st->dev = 0x801;
    st->ino = path_ino(path);
    st->nlink = ((mode & LX_S_IFMT) == LX_S_IFDIR) ? 2 : 1;
    st->mode = mode;
    st->size = (INT64)size;
    st->blksize = 4096;
    st->blocks = (INT64)((size + 511u) / 512u);
    st->atime = st->mtime = st->ctime = mtime;
}

/* ---------------- /sys: несколько файлов, которые читают программы ----
   (glibc узнаёт число процессоров из /sys/devices/system/cpu/online) */

static const char *const g_sys_files[] = {
    "/sys/devices/system/cpu/online",
    "/sys/devices/system/cpu/possible",
    "/sys/devices/system/cpu/present",
    "/sys/kernel/mm/transparent_hugepage/enabled",
    NULL
};

/* /sys/...: 1 - файл, 2 - папка (начало пути какого-то файла), 0 - нет */
static UINTN sys_kind(const char *path)
{
    for (UINTN i = 0; g_sys_files[i]; i++) {
        const char *rest;
        if (path_is(g_sys_files[i], path, &rest))
            return (rest[0] == '\0') ? 1 : 2;
    }

    return 0;
}

/* Специальные пути (/dev, /proc, /sys): TRUE - это он, *st заполнен */
static BOOLEAN special_stat(KPROC *p, const char *path, LX_STAT *st, INTN *err)
{
    const char *rest;
    *err = 0;

    if (path_is(path, "/dev", &rest)) {
        if (rest[0] == '\0') {
            fill_stat(st, path, LX_S_IFDIR | 0755, 0, 0);
            return TRUE;
        }
        static const char *const devs[] = { "/null", "/zero", "/urandom", "/random", "/tty",
                                            "/console", "/full", NULL };
        for (UINTN i = 0; devs[i]; i++)
            if (path_is(rest, devs[i], NULL)) {
                fill_stat(st, path, LX_S_IFCHR | 0666, 0, 0);
                st->rdev = 0x100 + i;
                return TRUE;
            }
        if (path_is(rest, "/shm", NULL)) {
            fill_stat(st, path, LX_S_IFDIR | 0777, 0, 0);
            return TRUE;
        }
        *err = -LX_ENOENT;
        return TRUE;
    }

    if (path_is(path, "/proc", &rest)) {
        const char *r2;
        KPROC *q = proc_path(p, path, &r2);
        if (rest[0] == '\0' || (q != NULL && (r2[0] == '\0' || (path_is(r2, "/fd", NULL) && r2[3] == '\0')))) {
            fill_stat(st, path, LX_S_IFDIR | 0555, 0, 0);
            return TRUE;
        }
        if (q != NULL && (path_is(r2, "/exe", NULL) || path_is(r2, "/cwd", NULL))) {
            fill_stat(st, path, LX_S_IFLNK | 0777, 0, 0);
            return TRUE;
        }
        fill_stat(st, path, LX_S_IFREG | 0444, 0, 0);
        return TRUE;
    }

    if (path_is(path, "/sys", NULL)) {
        UINTN k = (path[4] == '\0') ? 2 : sys_kind(path);
        if (k == 0)
            *err = -LX_ENOENT;
        else
            fill_stat(st, path, (k == 2) ? (LX_S_IFDIR | 0555) : (LX_S_IFREG | 0444), 0, 0);
        return TRUE;
    }

    return FALSE;
}

/* Номер "устройства" тома: разный у разных томов (find -xdev, du и cp
   так отличают файловые системы) - по имени тома */
static UINT64 path_dev(const char *path)
{
    UINT32 h = 2166136261u;

    for (UINTN i = 1; path[i] && path[i] != '/'; i++)
        h = (h ^ (UINT8)path[i]) * 16777619u;

    return 0x800u | (h & 0xFFu);
}

/* stat по узлу VFS (путь MyOS - для номера inode, если у ФС его нет) */
static void stat_from_node(LX_STAT *st, const char *path, const VFS_NODE *n)
{
    if (n->mode != 0) {
        /* ext4: всё настоящее */
        fill_stat(st, path, n->mode, n->size, (UINT64)n->mtime);
        st->ino = n->ino;
        st->dev = path_dev(path);
        st->nlink = n->nlink ? n->nlink : 1;
        st->uid = n->uid;
        st->gid = n->gid;
        st->rdev = n->rdev;
        st->atime = (UINT64)n->atime;
        st->ctime = (UINT64)n->ctime;
        return;
    }

    UINT64 mt = fat_to_unix(n->wdate, n->wtime);

    if (n->is_dir)
        fill_stat(st, path, LX_S_IFDIR | 0755, 4096, mt);
    else
        fill_stat(st, path, LX_S_IFREG | 0755, n->size, mt);

    st->dev = path_dev(path);
}

/* stat пути MyOS (ссылки уже разобраны слоем Linux: последняя - сама
   по себе, поэтому lstat) */
static INTN do_stat_path(KPROC *p, const char *path, LX_STAT *st)
{
    INTN err;

    if (special_stat(p, path, st, &err))
        return err;

    VFS_DIRENT *e = (VFS_DIRENT *)kmalloc(sizeof(VFS_DIRENT));

    if (e == NULL)
        return -LX_ENOMEM;

    INTN r = vfs_lstat(path, e);

    if (r == VFS_OK)
        stat_from_node(st, path, &e->node);

    kfree(e);
    return (r == VFS_OK) ? 0 : (INTN)linux_errno(r);
}

static INTN do_stat_file(KPROC *p, LFILE *f, LX_STAT *st)
{
    (void)p;

    switch (f->type) {

    case LF_VFS: {
        UINT64 size = 0;
        vfs_size(f->kfd, &size);
        /* права, inode, время - по пути (файл мог быть удалён - тогда
           хотя бы размер) */
        if (do_stat_path(p, f->path, st) != 0 || (st->mode & LX_S_IFMT) != LX_S_IFREG)
            fill_stat(st, f->path, LX_S_IFREG | 0755, size, 0);
        st->size = (INT64)size;
        st->blocks = (INT64)((size + 511u) / 512u);
        return 0;
    }

    case LF_DIR:
        return do_stat_path(p, f->path, st);

    case LF_PIPE_R:
    case LF_PIPE_W:
        fill_stat(st, "pipe:", LX_S_IFIFO | 0600, 0, 0);
        st->ino = (UINT64)(UINTN)f->pipe;
        return 0;

    case LF_MEM:
        fill_stat(st, f->path, LX_S_IFREG | 0444, f->memlen, 0);
        return 0;

    case LF_EVENTFD:
        fill_stat(st, "anon_inode:[eventfd]", 0600, 0, 0);
        return 0;

    default:
        fill_stat(st, f->path, LX_S_IFCHR | 0620, 0, 0);
        st->rdev = 0x8800;
        return 0;
    }
}

/* statx (glibc и coreutils спрашивают так) из обычного stat */
typedef struct {
    UINT32 mask, blksize;
    UINT64 attributes;
    UINT32 nlink, uid, gid;
    UINT16 mode, pad1;
    UINT64 ino, size, blocks, attributes_mask;
    INT64  atime_s; UINT32 atime_ns, r0;
    INT64  btime_s; UINT32 btime_ns, r1;
    INT64  ctime_s; UINT32 ctime_ns, r2;
    INT64  mtime_s; UINT32 mtime_ns, r3;
    UINT32 rdev_major, rdev_minor, dev_major, dev_minor;
    UINT64 spare[14];
} LX_STATX;

static void stat_to_statx(const LX_STAT *st, LX_STATX *sx)
{
    memset(sx, 0, sizeof(*sx));
    sx->mask = 0x7FF;               /* STATX_BASIC_STATS */
    sx->blksize = (UINT32)st->blksize;
    sx->nlink = (UINT32)st->nlink;
    sx->mode = (UINT16)st->mode;
    sx->ino = st->ino;
    sx->size = (UINT64)st->size;
    sx->blocks = (UINT64)st->blocks;
    sx->atime_s = (INT64)st->atime;
    sx->ctime_s = (INT64)st->ctime;
    sx->mtime_s = (INT64)st->mtime;
    sx->btime_s = (INT64)st->mtime;
    sx->rdev_major = (UINT32)(st->rdev >> 8);
    sx->rdev_minor = (UINT32)(st->rdev & 0xFF);
    sx->dev_major = 8;
    sx->dev_minor = 1;
}


/* ================================================================
 * Файлы /proc
 * ================================================================ */

/* Текстовый буфер, растущий по мере надобности */
typedef struct {
    char  *s;
    UINTN  len, cap;
} TBUF;

static void tb_printf(TBUF *b, const char *fmt, ...)
{
    char line[256];
    va_list ap;

    va_start(ap, fmt);
    kvsnprintf(line, sizeof(line), fmt, ap);
    va_end(ap);

    UINTN n = 0;
    while (line[n])
        n++;

    if (b->len + n + 1 > b->cap) {
        UINTN nc = (b->cap ? b->cap * 2 : 4096);
        while (nc < b->len + n + 1)
            nc *= 2;
        char *ns = (char *)kmalloc(nc);
        if (ns == NULL)
            return;
        if (b->s != NULL) {
            memcpy(ns, b->s, b->len);
            kfree(b->s);
        }
        b->s = ns;
        b->cap = nc;
    }

    memcpy(b->s + b->len, line, n);
    b->len += n;
    b->s[b->len] = '\0';
}

/* Содержимое файла /proc - или FALSE, если такого нет */
static BOOLEAN proc_file_text(KPROC *p, const char *path, TBUF *b)
{
    const char *rest;
    KPROC *q = proc_path(p, path, &rest);

    if (q != NULL) {

        if (path_is(rest, "/maps", NULL)) {
            for (UVMA *v = q->vmas; v != NULL; v = v->next)
                tb_printf(b, "%012llx-%012llx %c%c%c%c 00000000 00:00 0 %s\n",
                          v->start, v->end,
                          (v->prot & UVM_R) ? 'r' : '-', (v->prot & UVM_W) ? 'w' : '-',
                          (v->prot & UVM_X) ? 'x' : '-', (v->flags & UVM_SHARED) ? 's' : 'p',
                          (v->flags & UVM_STACK) ? "[stack]" : (v->flags & UVM_HEAP) ? "[heap]" : "");
            return TRUE;
        }

        if (path_is(rest, "/stat", NULL)) {
            UINT32 ppid = q->parent ? q->parent->pid : 1;
            tb_printf(b, "%u (%s) R %u %u %u 0 -1 0 0 0 0 0 0 0 0 0 20 0 1 0 %llu %llu %llu\n",
                      q->pid, q->name, ppid, q->lx ? q->lx->pgid : q->pid,
                      q->lx ? q->lx->sid : q->pid, q->started_ms / 10u,
                      q->pages * 4096u, q->pages);
            return TRUE;
        }

        if (path_is(rest, "/status", NULL)) {
            tb_printf(b, "Name:\t%s\nState:\tR (running)\nTgid:\t%u\nPid:\t%u\nPPid:\t%u\n"
                         "Uid:\t0\t0\t0\t0\nGid:\t0\t0\t0\t0\nVmRSS:\t%llu kB\nThreads:\t1\n",
                      q->name, q->pid, q->pid, q->parent ? q->parent->pid : 1, q->pages * 4u);
            return TRUE;
        }

        if (path_is(rest, "/cmdline", NULL)) {
            tb_printf(b, "%s", q->path);
            if (b->s != NULL)
                b->len++;               /* с нулём на конце, как в Linux */
            return TRUE;
        }

        if (path_is(rest, "/environ", NULL) || path_is(rest, "/auxv", NULL)) {
            tb_printf(b, "%s", "");
            return TRUE;
        }

        return FALSE;
    }

    if (path_is(path, "/proc/cpuinfo", NULL)) {
        for (UINT32 i = 0; i < g_ncpus; i++)
            tb_printf(b, "processor\t: %u\nvendor_id\t: GenuineIntel\nmodel name\t: x86-64 CPU\n"
                         "cpu MHz\t\t: %llu\nflags\t\t: fpu tsc cx8 cmov mmx fxsr sse sse2 sse3 ssse3\n\n",
                      i, g_tsc_hz / 1000000u);
        return TRUE;
    }

    if (path_is(path, "/proc/meminfo", NULL)) {
        tb_printf(b, "MemTotal:       %llu kB\nMemFree:        %llu kB\nMemAvailable:   %llu kB\n"
                     "Buffers:        0 kB\nCached:         0 kB\nSwapTotal:      0 kB\nSwapFree:       0 kB\n",
                  g_kmm_usable_pages * 4u, g_kmm_free_pages * 4u, g_kmm_free_pages * 4u);
        return TRUE;
    }

    if (path_is(path, "/proc/uptime", NULL)) {
        tb_printf(b, "%llu.%02llu 0.00\n", g_kticks / 1000u, (g_kticks / 10u) % 100u);
        return TRUE;
    }

    if (path_is(path, "/proc/sys/kernel/osrelease", NULL)) {
        tb_printf(b, "6.1.0-myos\n");
        return TRUE;
    }

    if (path_is(path, "/proc/mounts", NULL) || path_is(path, "/proc/self/mounts", NULL)) {
        tb_printf(b, "myos / myos rw 0 0\n");
        return TRUE;
    }

    return FALSE;
}

/* Текст файла /sys (FALSE - такого нет) */
static BOOLEAN sys_file_text(const char *path, TBUF *b)
{
    if (sys_kind(path) != 1)
        return FALSE;

    if (path_is(path, "/sys/kernel/mm/transparent_hugepage/enabled", NULL)) {
        tb_printf(b, "always madvise [never]\n");
        return TRUE;
    }

    /* online / possible / present: все процессоры */
    if (g_ncpus > 1)
        tb_printf(b, "0-%u\n", g_ncpus - 1u);
    else
        tb_printf(b, "0\n");

    return TRUE;
}


/* ================================================================
 * open
 * ================================================================ */

static INT64 do_open(KPROC *p, const char *path, UINT32 flags)
{
    UINT32 acc = flags & LX_O_ACCMODE;
    BOOLEAN cloexec = (flags & LX_O_CLOEXEC) != 0;
    const char *rest;
    LFILE *f = NULL;

    if (path_is(path, "/dev", &rest)) {

        UINT32 type = 0;

        if (path_is(rest, "/null", NULL))
            type = LF_NULL;
        else if (path_is(rest, "/zero", NULL) || path_is(rest, "/full", NULL))
            type = LF_ZERO;
        else if (path_is(rest, "/urandom", NULL) || path_is(rest, "/random", NULL))
            type = LF_RANDOM;
        else if (path_is(rest, "/tty", NULL) || path_is(rest, "/console", NULL))
            type = LF_TTY;
        else if (rest[0] == '\0' || path_is(rest, "/shm", NULL))
            type = LF_DIR;
        else
            return -LX_ENOENT;

        f = lfile_new(type, flags & ~LX_O_CLOEXEC);
        if (f == NULL)
            return -LX_ENOMEM;
        ksnprintf(f->path, sizeof(f->path), "%s", path);
        INT64 fd = fd_install(p, f, 0, cloexec);
        if (fd < 0)
            lfile_unref(f);
        return fd;
    }

    if (path_is(path, "/proc", &rest)) {

        if (rest[0] == '\0' || (flags & LX_O_DIRECTORY)) {
            f = lfile_new(LF_DIR, flags & ~LX_O_CLOEXEC);
        } else {
            TBUF b = { NULL, 0, 0 };
            if (!proc_file_text(p, path, &b)) {
                kfree(b.s);
                return -LX_ENOENT;
            }
            f = lfile_new(LF_MEM, LX_O_RDONLY);
            if (f != NULL) {
                f->mem = b.s;
                f->memlen = b.len;
            } else {
                kfree(b.s);
            }
        }

        if (f == NULL)
            return -LX_ENOMEM;

        ksnprintf(f->path, sizeof(f->path), "%s", path);

        INT64 fd = fd_install(p, f, 0, cloexec);
        if (fd < 0)
            lfile_unref(f);
        return fd;
    }

    if (path_is(path, "/sys", NULL)) {

        UINTN k = (path[4] == '\0') ? 2 : sys_kind(path);

        if (k == 0)
            return -LX_ENOENT;

        if (k == 2) {
            f = lfile_new(LF_DIR, flags & ~LX_O_CLOEXEC);
        } else {
            TBUF b = { NULL, 0, 0 };
            sys_file_text(path, &b);
            f = lfile_new(LF_MEM, LX_O_RDONLY);
            if (f != NULL) {
                f->mem = b.s;
                f->memlen = b.len;
            } else {
                kfree(b.s);
            }
        }

        if (f == NULL)
            return -LX_ENOMEM;

        ksnprintf(f->path, sizeof(f->path), "%s", path);

        INT64 fd = fd_install(p, f, 0, cloexec);
        if (fd < 0)
            lfile_unref(f);
        return fd;
    }

    /* обычный файл или папка VFS */
    if ((flags & LX_O_CREAT) && (flags & LX_O_EXCL)) {
        VFS_DIRENT *e = (VFS_DIRENT *)kmalloc(sizeof(VFS_DIRENT));
        if (e == NULL)
            return -LX_ENOMEM;
        INTN r = vfs_stat(path, e);
        kfree(e);
        if (r == VFS_OK)
            return -LX_EEXIST;
    }

    UINT32 vfl = VFS_O_READ;

    if (acc != LX_O_RDONLY)
        vfl |= VFS_O_WRITE;
    if (flags & LX_O_CREAT)
        vfl |= VFS_O_CREATE;
    if ((flags & LX_O_TRUNC) && acc != LX_O_RDONLY)
        vfl |= VFS_O_TRUNC;
    if (flags & LX_O_APPEND)
        vfl |= VFS_O_APPEND;

    INTN kfd = (flags & LX_O_DIRECTORY) ? VFS_EISDIR : vfs_open(path, vfl);

    if (kfd == VFS_EISDIR || (kfd < 0 && (path[0] == '/' && path[1] == '\0'))) {

        if (acc != LX_O_RDONLY)
            return -LX_EISDIR;

        /* папка: только проверить, что есть, - читают её getdents64 */
        VFS_DIRENT *e = (VFS_DIRENT *)kmalloc(sizeof(VFS_DIRENT));
        if (e == NULL)
            return -LX_ENOMEM;
        INTN r = vfs_stat(path, e);
        BOOLEAN dir = (r == VFS_OK) && e->node.is_dir;
        kfree(e);

        if (r != VFS_OK)
            return linux_errno(r);
        if (!dir)
            return -LX_ENOTDIR;

        f = lfile_new(LF_DIR, flags & ~LX_O_CLOEXEC);

    } else if (kfd < 0) {

        return linux_errno(kfd);

    } else {

        f = lfile_new(LF_VFS, flags & ~LX_O_CLOEXEC);

        if (f == NULL) {
            vfs_close(kfd);
            return -LX_ENOMEM;
        }

        f->kfd = kfd;
    }

    if (f == NULL)
        return -LX_ENOMEM;

    ksnprintf(f->path, sizeof(f->path), "%s", path);

    INT64 fd = fd_install(p, f, 0, cloexec);

    if (fd < 0)
        lfile_unref(f);

    return fd;
}


/* ================================================================
 * read / write
 * ================================================================ */

static INT64 lf_read(KPROC *p, LFILE *f, UINT8 *dst, UINTN n)
{
    if ((f->flags & LX_O_ACCMODE) == LX_O_WRONLY && f->type != LF_TTY)
        return -LX_EBADF;

    switch (f->type) {

    case LF_TTY:
        return lterm_read(p, f, dst, n);

    case LF_VFS: {
        INTN r = vfs_read(f->kfd, dst, n);
        return (r < 0) ? linux_errno(r) : r;
    }

    case LF_DIR:
        return -LX_EISDIR;

    case LF_NULL:
        return 0;

    case LF_ZERO:
        memset(dst, 0, n);
        return (INT64)n;

    case LF_RANDOM:
        krandom_fill(dst, n);
        return (INT64)n;

    case LF_PIPE_R:
        return pipe_read(f->pipe, f, dst, n);

    case LF_PIPE_W:
        return -LX_EBADF;

    case LF_MEM: {
        if (f->pos >= f->memlen)
            return 0;
        UINTN k = (UINTN)(f->memlen - f->pos);
        if (k > n)
            k = n;
        memcpy(dst, f->mem + f->pos, k);
        f->pos += k;
        return (INT64)k;
    }

    case LF_EVENTFD: {
        if (n < 8)
            return -LX_EINVAL;
        UINT64 fl = kx_irq_save();
        while (f->count == 0) {
            if (f->flags & LX_O_NONBLOCK) {
                kx_irq_restore(fl);
                return -LX_EAGAIN;
            }
            if (p->killed || lx_signal_pending()) {
                kx_irq_restore(fl);
                return -LX_ERESTARTSYS;
            }
            sched_block(f, "eventfd", 100);
        }
        UINT64 v = f->count;
        f->count = 0;
        kx_irq_restore(fl);
        memcpy(dst, &v, 8);
        lx_poll_wake();
        return 8;
    }
    }

    return -LX_EINVAL;
}

static INT64 lf_write(KPROC *p, LFILE *f, const UINT8 *src, UINTN n)
{
    if ((f->flags & LX_O_ACCMODE) == LX_O_RDONLY &&
        f->type != LF_TTY && f->type != LF_NULL && f->type != LF_EVENTFD && f->type != LF_PIPE_W)
        return -LX_EBADF;

    switch (f->type) {

    case LF_TTY:
        proc_out_tty(p, (const char *)src, n);
        return (INT64)n;

    case LF_VFS: {
        INTN r = vfs_write(f->kfd, src, n);
        return (r < 0) ? linux_errno(r) : r;
    }

    case LF_NULL:
    case LF_RANDOM:
        return (INT64)n;

    case LF_ZERO:
        return -LX_ENOSPC;

    case LF_PIPE_W:
        return pipe_write(f->pipe, f, src, n);

    case LF_EVENTFD: {
        if (n < 8)
            return -LX_EINVAL;
        UINT64 v;
        memcpy(&v, src, 8);
        f->count += v;
        sched_wake_all(f);
        lx_poll_wake();
        return 8;
    }
    }

    return -LX_EBADF;
}

/* Для ядра (mmap файла, sendfile): прочитать с места off (или с
   текущего, если off == ~0) в буфер ядра */
INTN lx_file_read_kernel(LFILE *f, void *buf, UINTN n, UINT64 off)
{
    if (f->type == LF_VFS) {

        UINT64 cur = 0, np = 0;

        if (off != ~0ull) {
            vfs_seek(f->kfd, 0, 1, &cur);
            if (vfs_seek(f->kfd, (INT64)off, 0, &np) != VFS_OK) {
                vfs_seek(f->kfd, (INT64)cur, 0, &np);
                return 0;           /* за концом файла - пусто */
            }
        }

        INTN r = vfs_read(f->kfd, buf, n);

        if (off != ~0ull)
            vfs_seek(f->kfd, (INT64)cur, 0, &np);

        return (r < 0) ? (INTN)linux_errno(r) : r;
    }

    if (f->type == LF_MEM) {
        UINT64 at = (off != ~0ull) ? off : f->pos;
        if (at >= f->memlen)
            return 0;
        UINTN k = (UINTN)(f->memlen - at);
        if (k > n)
            k = n;
        memcpy(buf, f->mem + at, k);
        if (off == ~0ull)
            f->pos += k;
        return (INTN)k;
    }

    if (f->type == LF_ZERO) {
        memset(buf, 0, n);
        return (INTN)n;
    }

    return -LX_EACCES;
}


/* ================================================================
 * getdents64
 * ================================================================ */

typedef struct {
    UINT64 want, idx;
    UINT8 *out;
    UINTN  cap, used;
    BOOLEAN full;
    const char *dirpath;
} GD_CTX;

/* Тип записи папки (d_type) по узлу */
static UINT8 dt_of(const VFS_NODE *n)
{
    switch (n->mode & LX_S_IFMT) {
    case 0:            return n->is_dir ? 4 : 8;
    case LX_S_IFDIR:   return 4;        /* DT_DIR */
    case LX_S_IFLNK:   return 10;       /* DT_LNK */
    case LX_S_IFCHR:   return 2;        /* DT_CHR */
    case 0060000:      return 6;        /* DT_BLK */
    case LX_S_IFIFO:   return 1;        /* DT_FIFO */
    case 0140000:      return 12;       /* DT_SOCK */
    default:           return 8;        /* DT_REG */
    }
}

static BOOLEAN gd_put(GD_CTX *c, const char *name, UINT8 dtype, UINT64 ino)
{
    UINTN nl = 0;

    while (name[nl])
        nl++;

    UINTN reclen = (19u + nl + 1u + 7u) & ~7u;

    if (c->used + reclen > c->cap) {
        c->full = TRUE;
        return FALSE;
    }

    UINT8 *r = c->out + c->used;

    memset(r, 0, reclen);
    memcpy(r, &ino, 8);
    UINT64 off = c->idx + 1;
    memcpy(r + 8, &off, 8);
    UINT16 rl = (UINT16)reclen;
    memcpy(r + 16, &rl, 2);
    r[18] = dtype;
    memcpy(r + 19, name, nl);

    c->used += reclen;
    return TRUE;
}

static INTN gd_cb(void *ctx, const VFS_DIRENT *e)
{
    GD_CTX *c = (GD_CTX *)ctx;

    if (c->idx < c->want) {
        c->idx++;
        return 0;
    }

    char full[VFS_PATH_MAX];
    ksnprintf(full, sizeof(full), "%s/%s", c->dirpath, e->name);

    if (!gd_put(c, e->name, dt_of(&e->node), e->node.ino ? e->node.ino : path_ino(full)))
        return 1;                   /* буфер полон - хватит */

    c->idx++;
    return 0;
}

static INT64 do_getdents64(KPROC *p, LFILE *f, UINT8 *out, UINTN cap)
{
    (void)p;

    if (f->type != LF_DIR)
        return -LX_ENOTDIR;

    GD_CTX c = { f->pos, 0, out, cap, 0, FALSE, f->path };

    /* "." и ".." - первыми (ls -a, find их ждут) */
    if (c.want == 0) {
        if (!gd_put(&c, ".", 4, path_ino(f->path)))
            return -LX_EINVAL;
        c.idx = ++c.want;
    }
    if (c.want == 1) {
        if (!gd_put(&c, "..", 4, 1)) {
            f->pos = c.idx;
            return (INT64)c.used;
        }
        c.idx = ++c.want;
    }

    c.idx = 2;

    const char *rest;

    if (path_is(f->path, "/dev", &rest) || path_is(f->path, "/proc", &rest)) {

        /* короткий список: что есть в /dev; в /proc - процессы (для ps)
           и несколько файлов */
        static const char *const devs[] = { "null", "zero", "urandom", "random", "tty", NULL };
        static const char *const procs[] = { "self", "cpuinfo", "meminfo", "uptime", "mounts", NULL };
        BOOLEAN is_dev = path_is(f->path, "/dev", NULL);

        if (rest[0] == '\0') {
            const char *const *names = is_dev ? devs : procs;
            for (UINTN i = 0; names[i]; i++) {
                if (c.idx < c.want) {
                    c.idx++;
                    continue;
                }
                if (!gd_put(&c, names[i], is_dev ? 2 : (i == 0) ? 10 : 8, 0x100 + i))
                    break;
                c.idx++;
            }
            for (UINTN i = 0; !is_dev && !c.full && i < PROC_MAX; i++) {
                if (!g_procs[i].used || g_procs[i].exited)
                    continue;
                if (c.idx < c.want) {
                    c.idx++;
                    continue;
                }
                char num[16];
                ksnprintf(num, sizeof(num), "%u", g_procs[i].pid);
                if (!gd_put(&c, num, 4, 0x10000 + g_procs[i].pid))
                    break;
                c.idx++;
            }
        }

    } else if (path_is(f->path, "/sys", NULL)) {

        /* дети папки /sys/...: следующие элементы путей файлов (без повторов) */
        UINTN plen = 0;

        while (f->path[plen])
            plen++;

        for (UINTN i = 0; g_sys_files[i] && !c.full; i++) {

            const char *rest;

            if (!path_is(g_sys_files[i], f->path, &rest) || rest[0] != '/')
                continue;

            char name[64];
            UINTN n = 0;

            for (const char *q = rest + 1; *q && *q != '/' && n + 1 < sizeof(name); q++)
                name[n++] = *q;

            name[n] = '\0';

            /* было ли такое имя у файла раньше в таблице */
            BOOLEAN dup = FALSE;

            for (UINTN j = 0; j < i && !dup; j++) {
                const char *r2;
                if (path_is(g_sys_files[j], f->path, &r2) && r2[0] == '/') {
                    UINTN k = 0;
                    while (name[k] && r2[1 + k] == name[k])
                        k++;
                    dup = (name[k] == '\0' && (r2[1 + k] == '/' || r2[1 + k] == '\0'));
                }
            }

            if (dup)
                continue;

            if (c.idx < c.want) {
                c.idx++;
                continue;
            }

            if (!gd_put(&c, name, (rest[1 + n] == '\0') ? 8 : 4, 0x200 + i))
                break;

            c.idx++;
        }

    } else {

        INTN r = vfs_list(f->path, gd_cb, &c);

        if (r != VFS_OK && c.used == 0)
            return linux_errno(r);
    }

    if (c.used == 0 && c.full)
        return -LX_EINVAL;          /* даже одна запись не влезла */

    f->pos = c.idx;
    return (INT64)c.used;
}


/* ================================================================
 * poll / select
 * ================================================================ */

#define LX_POLLIN   0x001
#define LX_POLLPRI  0x002
#define LX_POLLOUT  0x004
#define LX_POLLERR  0x008
#define LX_POLLHUP  0x010
#define LX_POLLNVAL 0x020

static UINT32 lf_poll(KPROC *p, LFILE *f)
{
    switch (f->type) {

    case LF_TTY: {
        LTERM *t = lterm_of(p);
        UINT32 ev = LX_POLLOUT;
        if (t != NULL && lterm_key_ready(p, t))
            ev |= LX_POLLIN;
        return ev;
    }

    case LF_PIPE_R: {
        UINT32 ev = 0;
        if (f->pipe->len > 0)
            ev |= LX_POLLIN;
        if (f->pipe->writers == 0)
            ev |= LX_POLLHUP | LX_POLLIN;
        return ev;
    }

    case LF_PIPE_W: {
        UINT32 ev = 0;
        if (f->pipe->len < LPIPE_SIZE)
            ev |= LX_POLLOUT;
        if (f->pipe->readers == 0)
            ev |= LX_POLLERR | LX_POLLOUT;
        return ev;
    }

    case LF_EVENTFD:
        return LX_POLLOUT | (f->count > 0 ? LX_POLLIN : 0);

    default:
        return LX_POLLIN | LX_POLLOUT;
    }
}

typedef struct {
    INT32 fd;
    INT16 events, revents;
} LX_POLLFD;

/* Ждать готовности: timeout_ms < 0 - сколько угодно */
static INT64 do_poll(KPROC *p, LX_POLLFD *fds, UINTN n, INT64 timeout_ms)
{
    UINT64 t0 = g_kticks;

    for (;;) {

        INT64 count = 0;

        for (UINTN i = 0; i < n; i++) {

            fds[i].revents = 0;

            if (fds[i].fd < 0)
                continue;

            LFILE *f = fd_get(p, fds[i].fd);
            UINT32 ev = (f == NULL) ? LX_POLLNVAL : lf_poll(p, f);

            ev &= (UINT32)(UINT16)fds[i].events | LX_POLLERR | LX_POLLHUP | LX_POLLNVAL;
            fds[i].revents = (INT16)ev;

            if (ev)
                count++;
        }

        if (count > 0 || timeout_ms == 0)
            return count;

        if (timeout_ms > 0 && g_kticks - t0 >= (UINT64)timeout_ms)
            return 0;

        if (p->killed || lx_signal_pending())
            return -LX_ERESTARTSYS;

        /* ждём "что-то изменилось" у каналов и терминала; клавиатура
           консоли и сеть событий не шлют - смотрим каждые 10 мс */
        UINT64 fl = kx_irq_save();
        sched_block(&g_lx_poll_event, "poll", 10);
        kx_irq_restore(fl);
    }
}

/* select поверх poll: битовые наборы fd -> массив pollfd и обратно */
static INT64 do_select(KPROC *p, INT64 nfds, UINT64 urd, UINT64 uwr, UINT64 uex, INT64 timeout_ms)
{
    if (nfds < 0 || nfds > LX_FDS)
        return -LX_EINVAL;

    UINTN bytes = (UINTN)((nfds + 63) / 64) * 8u;

    if ((urd && !uptr_ok(p, urd, bytes, TRUE)) || (uwr && !uptr_ok(p, uwr, bytes, TRUE)) ||
        (uex && !uptr_ok(p, uex, bytes, TRUE)))
        return -LX_EFAULT;

    UINT64 *rd = (UINT64 *)(UINTN)urd, *wr = (UINT64 *)(UINTN)uwr, *ex = (UINT64 *)(UINTN)uex;
    LX_POLLFD *pf = (LX_POLLFD *)kmalloc(sizeof(LX_POLLFD) * (UINTN)(nfds ? nfds : 1));

    if (pf == NULL)
        return -LX_ENOMEM;

    UINTN n = 0;

    for (INT64 fd = 0; fd < nfds; fd++) {

        UINT64 bit = 1ull << (fd & 63);
        INT16 ev = 0;

        if (rd && (rd[fd / 64] & bit))
            ev |= LX_POLLIN;
        if (wr && (wr[fd / 64] & bit))
            ev |= LX_POLLOUT;
        if (ex && (ex[fd / 64] & bit))
            ev |= LX_POLLPRI;

        if (ev) {
            pf[n].fd = (INT32)fd;
            pf[n].events = ev;
            n++;
        }
    }

    INT64 r = do_poll(p, pf, n, timeout_ms);

    if (r >= 0) {

        if (rd) memset(rd, 0, bytes);
        if (wr) memset(wr, 0, bytes);
        if (ex) memset(ex, 0, bytes);

        r = 0;

        for (UINTN i = 0; i < n; i++) {

            UINT64 bit = 1ull << (pf[i].fd & 63);
            INT16 re = pf[i].revents;

            if (re & LX_POLLNVAL) {
                kfree(pf);
                return -LX_EBADF;
            }

            if (rd && (pf[i].events & LX_POLLIN) && (re & (LX_POLLIN | LX_POLLHUP | LX_POLLERR))) {
                rd[pf[i].fd / 64] |= bit;
                r++;
            }
            if (wr && (pf[i].events & LX_POLLOUT) && (re & (LX_POLLOUT | LX_POLLERR))) {
                wr[pf[i].fd / 64] |= bit;
                r++;
            }
            if (ex && (pf[i].events & LX_POLLPRI) && (re & LX_POLLPRI)) {
                ex[pf[i].fd / 64] |= bit;
                r++;
            }
        }
    }

    kfree(pf);
    return r;
}

static INT64 ts_to_ms(KPROC *p, UINT64 uts)
{
    if (uts == 0)
        return -1;

    if (!uptr_ok(p, uts, sizeof(LX_TIMESPEC), FALSE))
        return -2;

    LX_TIMESPEC ts;
    memcpy(&ts, (const void *)(UINTN)uts, sizeof(ts));

    if (ts.tv_sec < 0 || ts.tv_nsec < 0)
        return -2;

    return ts.tv_sec * 1000 + (ts.tv_nsec + 999999) / 1000000;
}


/* ================================================================
 * Системные вызовы
 * ================================================================ */

/* номера вызовов Linux x86-64 (файловые) */
enum {
    NR_read = 0, NR_write = 1, NR_open = 2, NR_close = 3, NR_stat = 4, NR_fstat = 5,
    NR_lstat = 6, NR_poll = 7, NR_lseek = 8, NR_ioctl = 16, NR_pread64 = 17,
    NR_pwrite64 = 18, NR_readv = 19, NR_writev = 20, NR_access = 21, NR_pipe = 22,
    NR_select = 23, NR_dup = 32, NR_dup2 = 33, NR_sendfile = 40, NR_fcntl = 72,
    NR_flock = 73, NR_fsync = 74, NR_fdatasync = 75, NR_truncate = 76, NR_ftruncate = 77,
    NR_getcwd = 79, NR_chdir = 80, NR_fchdir = 81, NR_rename = 82, NR_mkdir = 83,
    NR_rmdir = 84, NR_creat = 85, NR_link = 86, NR_unlink = 87, NR_symlink = 88,
    NR_readlink = 89, NR_chmod = 90, NR_fchmod = 91, NR_chown = 92, NR_fchown = 93,
    NR_lchown = 94, NR_umask = 95, NR_utime = 132, NR_statfs = 137, NR_fstatfs = 138,
    NR_sync = 162, NR_syncfs = 306, NR_getdents64 = 217, NR_fadvise64 = 221, NR_utimes = 235,
    NR_openat = 257, NR_mkdirat = 258, NR_fchownat = 260, NR_futimesat = 261,
    NR_newfstatat = 262, NR_unlinkat = 263, NR_renameat = 264, NR_linkat = 265,
    NR_symlinkat = 266, NR_readlinkat = 267, NR_fchmodat = 268, NR_faccessat = 269,
    NR_pselect6 = 270, NR_ppoll = 271, NR_utimensat = 280, NR_eventfd = 284,
    NR_memfd_create = 319,
    NR_fallocate = 285, NR_eventfd2 = 290, NR_dup3 = 292, NR_pipe2 = 293,
    NR_renameat2 = 316, NR_copy_file_range = 326, NR_statx = 332, NR_close_range = 436,
    NR_faccessat2 = 439, NR_fchmodat2 = 452,
};

static INT64 do_readlink(KPROC *p, const char *path, UINT64 ubuf, UINT64 cap)
{
    char target[VFS_PATH_MAX];
    const char *rest;
    KPROC *q = proc_path(p, path, &rest);

    target[0] = '\0';

    if (q != NULL && path_is(rest, "/exe", NULL)) {
        lx_path_to_linux(p, (q->lx && q->lx->exe[0]) ? q->lx->exe : q->path, target,
                         sizeof(target));
    } else if (q != NULL && path_is(rest, "/cwd", NULL)) {
        lx_path_to_linux(p, q->cwd, target, sizeof(target));
    } else if (q != NULL && path_is(rest, "/fd", &rest) && rest[0] == '/') {
        INT64 fd = 0;
        for (const char *s = rest + 1; *s >= '0' && *s <= '9'; s++)
            fd = fd * 10 + (*s - '0');
        LFILE *f = fd_get(q, fd);
        if (f == NULL)
            return -LX_ENOENT;
        if (f->type == LF_PIPE_R || f->type == LF_PIPE_W)
            ksnprintf(target, sizeof(target), "pipe:[%llu]", (UINT64)(UINTN)f->pipe & 0xFFFFFF);
        else
            lx_path_to_linux(p, f->path, target, sizeof(target));
    } else {
        /* символьная ссылка тома (ext4): её текст как есть */
        LX_STAT st;
        INTN r = do_stat_path(p, path, &st);
        if (r < 0)
            return r;
        if ((st.mode & LX_S_IFMT) != LX_S_IFLNK)
            return -LX_EINVAL;
        r = vfs_readlink(path, target, sizeof(target));
        if (r < 0)
            return linux_errno(r);
    }

    UINTN n = 0;
    while (target[n])
        n++;

    if (n > cap)
        n = (UINTN)cap;

    if (!uptr_ok(p, ubuf, n, TRUE))
        return -LX_EFAULT;

    memcpy((void *)(UINTN)ubuf, target, n);
    return (INT64)n;
}

static INT64 do_ioctl(KPROC *p, LFILE *f, INT64 fd, UINT64 req, UINT64 arg)
{
    if (req == 0x5451) {                    /* FIOCLEX */
        p->lx->cloexec[fd] = 1;
        return 0;
    }

    if (req == 0x5450) {                    /* FIONCLEX */
        p->lx->cloexec[fd] = 0;
        return 0;
    }

    if (req == 0x5421) {                    /* FIONBIO */
        if (!uptr_ok(p, arg, 4, FALSE))
            return -LX_EFAULT;
        if (*(volatile INT32 *)(UINTN)arg)
            f->flags |= LX_O_NONBLOCK;
        else
            f->flags &= ~(UINT32)LX_O_NONBLOCK;
        return 0;
    }

    if (req == 0x541B) {                    /* FIONREAD */
        if (!uptr_ok(p, arg, 4, TRUE))
            return -LX_EFAULT;
        INT32 n = 0;
        if (f->type == LF_PIPE_R)
            n = (INT32)f->pipe->len;
        else if (f->type == LF_TTY) {
            LTERM *t = lterm_of(p);
            n = t ? (INT32)(t->len - t->off) : 0;
        }
        *(volatile INT32 *)(UINTN)arg = n;
        return 0;
    }

    if (f->type != LF_TTY)
        return -LX_ENOTTY;

    LTERM *t = lterm_of(p);

    if (t == NULL)
        return -LX_ENOTTY;

    switch (req) {

    case 0x5401:                            /* TCGETS */
        if (!uptr_ok(p, arg, sizeof(LX_TERMIOS), TRUE))
            return -LX_EFAULT;
        memcpy((void *)(UINTN)arg, &t->tio, sizeof(LX_TERMIOS));
        return 0;

    case 0x5402: case 0x5403: case 0x5404:  /* TCSETS, TCSETSW, TCSETSF */
        if (!uptr_ok(p, arg, sizeof(LX_TERMIOS), FALSE))
            return -LX_EFAULT;
        memcpy(&t->tio, (const void *)(UINTN)arg, sizeof(LX_TERMIOS));
        if (req == 0x5404)
            t->len = t->off = 0;            /* TCSETSF: выбросить непрочитанное */
        return 0;

    case 0x5413: {                          /* TIOCGWINSZ */
        UINT16 ws[4] = { t->rows, t->cols, 0, 0 };
        if (p->io == PROC_IO_TTY && p->tty != NULL) {
            ws[0] = (UINT16)tty_rows(p->tty);
            ws[1] = TTY_COLS;
        }
        if (!uptr_ok(p, arg, sizeof(ws), TRUE))
            return -LX_EFAULT;
        memcpy((void *)(UINTN)arg, ws, sizeof(ws));
        return 0;
    }

    case 0x5414:                            /* TIOCSWINSZ */
        return 0;

    case 0x540F: {                          /* TIOCGPGRP */
        if (!uptr_ok(p, arg, 4, TRUE))
            return -LX_EFAULT;
        UINT32 g = t->pgrp ? t->pgrp : (p->lx ? p->lx->pgid : p->pid);
        memcpy((void *)(UINTN)arg, &g, 4);
        return 0;
    }

    case 0x5410:                            /* TIOCSPGRP */
        if (!uptr_ok(p, arg, 4, FALSE))
            return -LX_EFAULT;
        memcpy(&t->pgrp, (const void *)(UINTN)arg, 4);
        return 0;

    case 0x5429: {                          /* TIOCGSID */
        if (!uptr_ok(p, arg, 4, TRUE))
            return -LX_EFAULT;
        UINT32 s = p->lx ? p->lx->sid : p->pid;
        memcpy((void *)(UINTN)arg, &s, 4);
        return 0;
    }

    case 0x540E:                            /* TIOCSCTTY */
    case 0x5422:                            /* TIOCNOTTY */
    case 0x540B:                            /* TCFLSH */
    case 0x5409:                            /* TCSBRK (tcdrain) */
    case 0x540A:                            /* TCXONC */
        return 0;
    }

    return -LX_ENOTTY;
}

static INT64 do_fcntl(KPROC *p, LFILE *f, INT64 fd, UINT64 cmd, UINT64 arg)
{
    switch (cmd) {

    case 0:                                 /* F_DUPFD */
    case 1030: {                            /* F_DUPFD_CLOEXEC */
        f->refs++;
        INT64 nf = fd_install(p, f, (INT64)arg, cmd == 1030);
        if (nf < 0)
            f->refs--;
        return nf;
    }

    case 1:                                 /* F_GETFD */
        return p->lx->cloexec[fd] ? 1 : 0;

    case 2:                                 /* F_SETFD */
        p->lx->cloexec[fd] = (arg & 1) ? 1 : 0;
        return 0;

    case 3:                                 /* F_GETFL */
        return f->flags;

    case 4:                                 /* F_SETFL: менять можно только эти */
        f->flags = (f->flags & ~(UINT32)(LX_O_APPEND | LX_O_NONBLOCK)) |
                   ((UINT32)arg & (LX_O_APPEND | LX_O_NONBLOCK));
        return 0;

    case 5: case 6: case 7:                 /* F_GETLK/F_SETLK/F_SETLKW: замков нет */
    case 36: case 37: case 38:              /* F_OFD_* */
        if (cmd == 5 || cmd == 36) {
            /* "никто не держит": l_type = F_UNLCK (2) */
            if (uptr_ok(p, arg, 2, TRUE))
                *(volatile INT16 *)(UINTN)arg = 2;
        }
        return 0;

    case 1031: case 1032:                   /* F_SETPIPE_SZ / F_GETPIPE_SZ */
        return LPIPE_SIZE;

    case 1033: case 1034:                   /* F_ADD_SEALS / F_GET_SEALS */
        return 0;
    }

    return -LX_EINVAL;
}

static INT64 do_pipe(KPROC *p, UINT64 ufds, UINT32 flags)
{
    if (!uptr_ok(p, ufds, 8, TRUE))
        return -LX_EFAULT;

    LPIPE *pp = (LPIPE *)kzalloc(sizeof(LPIPE));
    UINT8 *buf = (UINT8 *)kmalloc(LPIPE_SIZE);
    LFILE *r = lfile_new(LF_PIPE_R, LX_O_RDONLY | (flags & LX_O_NONBLOCK));
    LFILE *w = lfile_new(LF_PIPE_W, LX_O_WRONLY | (flags & LX_O_NONBLOCK));

    if (pp == NULL || buf == NULL || r == NULL || w == NULL) {
        kfree(pp); kfree(buf); kfree(r); kfree(w);
        return -LX_ENOMEM;
    }

    pp->buf = buf;
    pp->readers = 1;
    pp->writers = 1;
    r->pipe = pp;
    w->pipe = pp;
    ksnprintf(r->path, sizeof(r->path), "pipe:");
    ksnprintf(w->path, sizeof(w->path), "pipe:");

    BOOLEAN ce = (flags & LX_O_CLOEXEC) != 0;
    INT64 a = fd_install(p, r, 0, ce);
    INT64 b = (a >= 0) ? fd_install(p, w, 0, ce) : -LX_EMFILE;

    if (a < 0 || b < 0) {
        if (a >= 0)
            fd_close(p, a);
        else
            lfile_unref(r);
        lfile_unref(w);
        return -LX_EMFILE;
    }

    INT32 v[2] = { (INT32)a, (INT32)b };
    memcpy((void *)(UINTN)ufds, v, 8);
    return 0;
}

/* readv/writev/preadv: по кускам iovec */
typedef struct {
    UINT64 base, len;
} LX_IOVEC;

static INT64 do_rwv(KPROC *p, LFILE *f, UINT64 uiov, UINT64 cnt, BOOLEAN write)
{
    if (cnt > 1024)
        return -LX_EINVAL;

    if (!uptr_ok(p, uiov, cnt * sizeof(LX_IOVEC), FALSE))
        return -LX_EFAULT;

    const LX_IOVEC *iov = (const LX_IOVEC *)(UINTN)uiov;
    INT64 total = 0;

    for (UINT64 i = 0; i < cnt; i++) {

        UINT64 b = iov[i].base, l = iov[i].len;

        if (l == 0)
            continue;

        if (!uptr_ok(p, b, l, !write))
            return total ? total : -LX_EFAULT;

        INT64 r = write ? lf_write(p, f, (const UINT8 *)(UINTN)b, (UINTN)l)
                        : lf_read(p, f, (UINT8 *)(UINTN)b, (UINTN)l);

        if (r < 0)
            return total ? total : r;

        total += r;

        if ((UINT64)r < l)
            break;                  /* короткое чтение - дальше не читаем */
    }

    return total;
}

/*
 * Файловые вызовы Linux. *handled = FALSE - "не мой номер".
 * a[0..5] - аргументы (rdi, rsi, rdx, r10, r8, r9).
 */
INT64 lx_file_syscall(KPROC *p, UINT64 nr, UINT64 *a, BOOLEAN *handled)
{
    char path[VFS_PATH_MAX], path2[VFS_PATH_MAX];
    INT64 r;
    LFILE *f;

    *handled = TRUE;

    switch (nr) {

    case NR_read:
        if ((f = fd_get(p, (INT64)a[0])) == NULL)
            return -LX_EBADF;
        if (!uptr_ok(p, a[1], a[2], TRUE))
            return -LX_EFAULT;
        return lf_read(p, f, (UINT8 *)(UINTN)a[1], (UINTN)a[2]);

    case NR_write:
        if ((f = fd_get(p, (INT64)a[0])) == NULL)
            return -LX_EBADF;
        if (!uptr_ok(p, a[1], a[2], FALSE))
            return -LX_EFAULT;
        return lf_write(p, f, (const UINT8 *)(UINTN)a[1], (UINTN)a[2]);

    case NR_readv:
    case NR_writev:
        if ((f = fd_get(p, (INT64)a[0])) == NULL)
            return -LX_EBADF;
        return do_rwv(p, f, a[1], a[2], nr == NR_writev);

    case NR_pread64:
    case NR_pwrite64: {
        if ((f = fd_get(p, (INT64)a[0])) == NULL)
            return -LX_EBADF;
        if (f->type != LF_VFS && f->type != LF_MEM)
            return -LX_ESPIPE;
        if (!uptr_ok(p, a[1], a[2], nr == NR_pread64))
            return -LX_EFAULT;
        if (nr == NR_pread64)
            return lx_file_read_kernel(f, (void *)(UINTN)a[1], (UINTN)a[2], a[3]);
        UINT64 cur = 0, np = 0;
        vfs_seek(f->kfd, 0, 1, &cur);
        if (vfs_seek(f->kfd, (INT64)a[3], 0, &np) != VFS_OK)
            return -LX_EINVAL;
        INTN w = vfs_write(f->kfd, (const void *)(UINTN)a[1], (UINTN)a[2]);
        vfs_seek(f->kfd, (INT64)cur, 0, &np);
        return (w < 0) ? linux_errno(w) : w;
    }

    case NR_open:
    case NR_creat:
    case NR_openat: {
        INT64 dirfd = (nr == NR_openat) ? (INT64)a[0] : LX_AT_FDCWD;
        UINT64 up = (nr == NR_openat) ? a[1] : a[0];
        UINT32 flags = (nr == NR_openat) ? (UINT32)a[2] :
                       (nr == NR_creat) ? (LX_O_CREAT | LX_O_WRONLY | LX_O_TRUNC) : (UINT32)a[1];
        r = lx_resolve_path_ex(p, dirfd, up, !(flags & LX_O_NOFOLLOW), path, sizeof(path));
        if (r < 0)
            return r;
        if (flags & LX_O_NOFOLLOW) {
            LX_STAT st;
            if (do_stat_path(p, path, &st) == 0 && (st.mode & LX_S_IFMT) == LX_S_IFLNK)
                return -LX_ELOOP;
        }
        return do_open(p, path, flags);
    }

    case NR_close:
        if (fd_get(p, (INT64)a[0]) == NULL)
            return -LX_EBADF;
        fd_close(p, (INT64)a[0]);
        return 0;

    case NR_close_range: {
        UINT64 first = a[0], last = a[1];
        for (UINT64 i = first; i <= last && i < LX_FDS; i++)
            if (p->lx->fd[i] != NULL) {
                if (a[2] & 4)               /* CLOSE_RANGE_CLOEXEC */
                    p->lx->cloexec[i] = 1;
                else
                    fd_close(p, (INT64)i);
            }
        return 0;
    }

    case NR_lseek: {
        if ((f = fd_get(p, (INT64)a[0])) == NULL)
            return -LX_EBADF;
        if (f->type == LF_DIR) {
            if (a[1] == 0 && a[2] == 0) {
                f->pos = 0;
                return 0;
            }
            return -LX_EINVAL;
        }
        if (f->type == LF_MEM) {
            INT64 base = (a[2] == 0) ? 0 : (a[2] == 1) ? (INT64)f->pos : (INT64)f->memlen;
            INT64 np = base + (INT64)a[1];
            if (np < 0)
                return -LX_EINVAL;
            f->pos = (UINT64)np;
            return np;
        }
        if (f->type == LF_NULL || f->type == LF_ZERO || f->type == LF_RANDOM)
            return 0;
        if (f->type != LF_VFS)
            return -LX_ESPIPE;
        UINT64 np = 0;
        INTN e = vfs_seek(f->kfd, (INT64)a[1], (UINT32)a[2], &np);
        return (e != VFS_OK) ? linux_errno(e) : (INT64)np;
    }

    case NR_stat:
    case NR_lstat:
    case NR_newfstatat:
    case NR_fstat: {
        LX_STAT st;
        UINT64 ubuf;
        if (nr == NR_fstat) {
            if ((f = fd_get(p, (INT64)a[0])) == NULL)
                return -LX_EBADF;
            r = do_stat_file(p, f, &st);
            ubuf = a[1];
        } else if (nr == NR_newfstatat) {
            ubuf = a[2];
            /* AT_EMPTY_PATH: сам dirfd */
            if ((a[3] & LX_AT_EMPTY_PATH) && uptr_ok(p, a[1], 1, FALSE) &&
                *(volatile char *)(UINTN)a[1] == '\0') {
                if ((f = fd_get(p, (INT64)a[0])) == NULL)
                    return -LX_EBADF;
                r = do_stat_file(p, f, &st);
            } else {
                r = lx_resolve_path_ex(p, (INT64)a[0], a[1], !(a[3] & LX_AT_SYMLINK_NOFOLLOW),
                                       path, sizeof(path));
                if (r == 0)
                    r = do_stat_path(p, path, &st);
            }
        } else {
            ubuf = a[1];
            r = lx_resolve_path_ex(p, LX_AT_FDCWD, a[0], nr != NR_lstat, path, sizeof(path));
            if (r == 0)
                r = do_stat_path(p, path, &st);
        }
        if (r < 0)
            return r;
        if (!uptr_ok(p, ubuf, sizeof(st), TRUE))
            return -LX_EFAULT;
        memcpy((void *)(UINTN)ubuf, &st, sizeof(st));
        return 0;
    }

    case NR_statx: {
        LX_STAT st;
        LX_STATX sx;
        if ((a[2] & LX_AT_EMPTY_PATH) && uptr_ok(p, a[1], 1, FALSE) &&
            *(volatile char *)(UINTN)a[1] == '\0') {
            if ((f = fd_get(p, (INT64)a[0])) == NULL)
                return -LX_EBADF;
            r = do_stat_file(p, f, &st);
        } else {
            r = lx_resolve_path_ex(p, (INT64)a[0], a[1], !(a[2] & LX_AT_SYMLINK_NOFOLLOW), path,
                                   sizeof(path));
            if (r == 0)
                r = do_stat_path(p, path, &st);
        }
        if (r < 0)
            return r;
        stat_to_statx(&st, &sx);
        if (!uptr_ok(p, a[4], sizeof(sx), TRUE))
            return -LX_EFAULT;
        memcpy((void *)(UINTN)a[4], &sx, sizeof(sx));
        return 0;
    }

    case NR_access:
    case NR_faccessat:
    case NR_faccessat2: {
        LX_STAT st;
        if (nr == NR_access)
            r = lx_resolve_path(p, LX_AT_FDCWD, a[0], path, sizeof(path));
        else
            r = lx_resolve_path(p, (INT64)a[0], a[1], path, sizeof(path));
        if (r < 0)
            return r;
        return do_stat_path(p, path, &st);
    }

    case NR_readlink:
    case NR_readlinkat:
        if (nr == NR_readlink) {
            r = lx_resolve_path_ex(p, LX_AT_FDCWD, a[0], FALSE, path, sizeof(path));
            if (r < 0)
                return r;
            return do_readlink(p, path, a[1], a[2]);
        }
        r = lx_resolve_path_ex(p, (INT64)a[0], a[1], FALSE, path, sizeof(path));
        if (r < 0)
            return r;
        return do_readlink(p, path, a[2], a[3]);

    case NR_getdents64:
        if ((f = fd_get(p, (INT64)a[0])) == NULL)
            return -LX_EBADF;
        if (!uptr_ok(p, a[1], a[2], TRUE))
            return -LX_EFAULT;
        return do_getdents64(p, f, (UINT8 *)(UINTN)a[1], (UINTN)a[2]);

    case NR_getcwd: {
        char lcwd[VFS_PATH_MAX];
        UINTN n = 0;
        lx_path_to_linux(p, p->cwd, lcwd, sizeof(lcwd));
        while (lcwd[n])
            n++;
        if (n + 1 > a[1])
            return -LX_ERANGE;
        if (!uptr_ok(p, a[0], n + 1, TRUE))
            return -LX_EFAULT;
        memcpy((void *)(UINTN)a[0], lcwd, n + 1);
        return (INT64)(n + 1);
    }

    case NR_chdir:
    case NR_fchdir: {
        LX_STAT st;
        if (nr == NR_fchdir) {
            if ((f = fd_get(p, (INT64)a[0])) == NULL)
                return -LX_EBADF;
            if (f->type != LF_DIR)
                return -LX_ENOTDIR;
            ksnprintf(path, sizeof(path), "%s", f->path);
        } else {
            r = lx_resolve_path(p, LX_AT_FDCWD, a[0], path, sizeof(path));
            if (r < 0)
                return r;
        }
        r = do_stat_path(p, path, &st);
        if (r < 0)
            return r;
        if ((st.mode & LX_S_IFMT) != LX_S_IFDIR)
            return -LX_ENOTDIR;
        ksnprintf(p->cwd, sizeof(p->cwd), "%s", path);
        return 0;
    }

    case NR_mkdir:
    case NR_mkdirat:
        r = (nr == NR_mkdir) ? lx_resolve_path(p, LX_AT_FDCWD, a[0], path, sizeof(path))
                             : lx_resolve_path(p, (INT64)a[0], a[1], path, sizeof(path));
        if (r < 0)
            return r;
        r = vfs_mkdir(path);
        return (r < 0) ? linux_errno(r) : 0;

    case NR_rmdir:
    case NR_unlink:
    case NR_unlinkat: {
        LX_STAT st;
        BOOLEAN want_dir = (nr == NR_rmdir) || (nr == NR_unlinkat && (a[2] & LX_AT_REMOVEDIR));
        r = (nr == NR_unlinkat) ? lx_resolve_path_ex(p, (INT64)a[0], a[1], FALSE, path, sizeof(path))
                                : lx_resolve_path_ex(p, LX_AT_FDCWD, a[0], FALSE, path, sizeof(path));
        if (r < 0)
            return r;
        r = do_stat_path(p, path, &st);
        if (r < 0)
            return r;
        BOOLEAN is_dir = (st.mode & LX_S_IFMT) == LX_S_IFDIR;
        if (want_dir && !is_dir)
            return -LX_ENOTDIR;
        if (!want_dir && is_dir)
            return -LX_EISDIR;
        r = vfs_remove(path);
        return (r < 0) ? linux_errno(r) : 0;
    }

    case NR_rename:
    case NR_renameat:
    case NR_renameat2:
        if (nr == NR_rename) {
            r = lx_resolve_path_ex(p, LX_AT_FDCWD, a[0], FALSE, path, sizeof(path));
            if (r == 0)
                r = lx_resolve_path_ex(p, LX_AT_FDCWD, a[1], FALSE, path2, sizeof(path2));
        } else {
            r = lx_resolve_path_ex(p, (INT64)a[0], a[1], FALSE, path, sizeof(path));
            if (r == 0)
                r = lx_resolve_path_ex(p, (INT64)a[2], a[3], FALSE, path2, sizeof(path2));
        }
        if (r < 0)
            return r;
        /* Linux молча заменяет существующий файл - VFS так не умеет:
           сначала удалить */
        {
            LX_STAT st;
            if (do_stat_path(p, path2, &st) == 0 && (st.mode & LX_S_IFMT) == LX_S_IFREG)
                vfs_remove(path2);
        }
        r = vfs_rename(path, path2);
        return (r < 0) ? linux_errno(r) : 0;

    case NR_truncate:
    case NR_ftruncate: {
        INTN kfd;
        BOOLEAN mine = FALSE;
        if (nr == NR_ftruncate) {
            if ((f = fd_get(p, (INT64)a[0])) == NULL)
                return -LX_EBADF;
            if (f->type != LF_VFS)
                return -LX_EINVAL;
            kfd = f->kfd;
        } else {
            r = lx_resolve_path(p, LX_AT_FDCWD, a[0], path, sizeof(path));
            if (r < 0)
                return r;
            kfd = vfs_open(path, VFS_O_READ | VFS_O_WRITE);
            if (kfd < 0)
                return linux_errno(kfd);
            mine = TRUE;
        }
        r = vfs_ftruncate(kfd, a[1]);
        if (mine)
            vfs_close(kfd);
        return (r < 0) ? linux_errno(r) : 0;
    }

    case NR_dup: {
        if ((f = fd_get(p, (INT64)a[0])) == NULL)
            return -LX_EBADF;
        f->refs++;
        r = fd_install(p, f, 0, FALSE);
        if (r < 0)
            f->refs--;
        return r;
    }

    case NR_dup2:
    case NR_dup3: {
        INT64 o = (INT64)a[0], n = (INT64)a[1];
        if ((f = fd_get(p, o)) == NULL || n < 0 || n >= LX_FDS)
            return -LX_EBADF;
        if (o == n)
            return (nr == NR_dup3) ? -LX_EINVAL : n;
        if (p->lx->fd[n] != NULL)
            fd_close(p, n);
        f->refs++;
        p->lx->fd[n] = f;
        p->lx->cloexec[n] = (nr == NR_dup3 && (a[2] & LX_O_CLOEXEC)) ? 1 : 0;
        return n;
    }

    case NR_fcntl:
        if ((f = fd_get(p, (INT64)a[0])) == NULL)
            return -LX_EBADF;
        return do_fcntl(p, f, (INT64)a[0], a[1], a[2]);

    case NR_ioctl:
        if ((f = fd_get(p, (INT64)a[0])) == NULL)
            return -LX_EBADF;
        return do_ioctl(p, f, (INT64)a[0], a[1], a[2]);

    case NR_pipe:
        return do_pipe(p, a[0], 0);

    case NR_pipe2:
        return do_pipe(p, a[0], (UINT32)a[1]);

    case NR_eventfd:
    case NR_eventfd2: {
        UINT32 fl = (nr == NR_eventfd2) ? (UINT32)a[1] : 0;
        f = lfile_new(LF_EVENTFD, LX_O_RDWR | (fl & LX_O_NONBLOCK));
        if (f == NULL)
            return -LX_ENOMEM;
        f->count = a[0];
        ksnprintf(f->path, sizeof(f->path), "anon_inode:[eventfd]");
        r = fd_install(p, f, 0, (fl & LX_O_CLOEXEC) != 0);
        if (r < 0)
            lfile_unref(f);
        return r;
    }

    case NR_memfd_create: {
        /* файл в памяти без имени: файл tmpfs, который тут же удалён
           (живёт, пока открыт или отображён в память) - его можно
           писать, ftruncate и mmap(MAP_SHARED): так Wayland и Firefox
           передают картинки между процессами */
        static UINT32 g_memfd_seq;
        char tmp[48], name[64];
        if (!uptr_ok(p, a[0], 1, FALSE))
            return -LX_EFAULT;
        UINTN n = 0;
        for (; n + 1 < sizeof(name); n++) {
            if (((a[0] + n) & 0xFFFu) == 0 && !uptr_ok(p, a[0] + n, 1, FALSE))
                return -LX_EFAULT;
            name[n] = *(volatile char *)(UINTN)(a[0] + n);
            if (name[n] == '\0')
                break;
        }
        name[n] = '\0';
        ksnprintf(tmp, sizeof(tmp), "/tmp/.memfd-%u", ++g_memfd_seq);
        INTN kfd = vfs_open(tmp, VFS_O_READ | VFS_O_WRITE | VFS_O_CREATE | VFS_O_TRUNC);
        if (kfd < 0)
            return linux_errno(kfd);
        vfs_remove(tmp);
        f = lfile_new(LF_VFS, LX_O_RDWR);
        if (f == NULL) {
            vfs_close(kfd);
            return -LX_ENOMEM;
        }
        f->kfd = kfd;
        ksnprintf(f->path, sizeof(f->path), "/memfd:%s (deleted)", name);
        r = fd_install(p, f, 0, (a[1] & 1u) != 0);       /* MFD_CLOEXEC */
        if (r < 0)
            lfile_unref(f);
        return r;
    }

    case NR_poll:
    case NR_ppoll: {
        UINTN n = (UINTN)a[1];
        if (n > LX_FDS)
            return -LX_EINVAL;
        if (n > 0 && !uptr_ok(p, a[0], n * sizeof(LX_POLLFD), TRUE))
            return -LX_EFAULT;
        INT64 ms = (nr == NR_poll) ? (INT64)(INT32)a[2] : ts_to_ms(p, a[2]);
        if (ms == -2)
            return -LX_EFAULT;
        if (ms < -1)
            ms = -1;
        LX_POLLFD *fds = (LX_POLLFD *)(UINTN)a[0];
        LX_POLLFD *kf = (LX_POLLFD *)kmalloc(sizeof(LX_POLLFD) * (n ? n : 1));
        if (kf == NULL)
            return -LX_ENOMEM;
        memcpy(kf, fds, n * sizeof(LX_POLLFD));
        r = do_poll(p, kf, n, ms);
        if (r >= 0)
            for (UINTN i = 0; i < n; i++)
                fds[i].revents = kf[i].revents;
        kfree(kf);
        return r;
    }

    case NR_select:
    case NR_pselect6: {
        INT64 ms = -1;
        if (nr == NR_select && a[4] != 0) {
            if (!uptr_ok(p, a[4], sizeof(LX_TIMEVAL), FALSE))
                return -LX_EFAULT;
            LX_TIMEVAL tv;
            memcpy(&tv, (const void *)(UINTN)a[4], sizeof(tv));
            ms = tv.tv_sec * 1000 + (tv.tv_usec + 999) / 1000;
        } else if (nr == NR_pselect6) {
            ms = ts_to_ms(p, a[4]);
            if (ms == -2)
                return -LX_EFAULT;
        }
        return do_select(p, (INT64)a[0], a[1], a[2], a[3], ms);
    }

    case NR_sendfile: {
        LFILE *out = fd_get(p, (INT64)a[0]), *in = fd_get(p, (INT64)a[1]);
        if (out == NULL || in == NULL)
            return -LX_EBADF;
        UINT64 off = ~0ull;
        if (a[2] != 0) {
            if (!uptr_ok(p, a[2], 8, TRUE))
                return -LX_EFAULT;
            memcpy(&off, (const void *)(UINTN)a[2], 8);
        }
        UINT8 *buf = (UINT8 *)kmalloc(65536);
        if (buf == NULL)
            return -LX_ENOMEM;
        INT64 total = 0;
        while ((UINT64)total < a[3]) {
            UINTN want = (UINTN)((a[3] - (UINT64)total) < 65536u ? (a[3] - (UINT64)total) : 65536u);
            INTN got = lx_file_read_kernel(in, buf, want, off);
            if (got <= 0) {
                if (got < 0 && total == 0)
                    total = got;
                break;
            }
            INT64 w = lf_write(p, out, buf, (UINTN)got);
            if (w < 0) {
                if (total == 0)
                    total = w;
                break;
            }
            total += w;
            if (off != ~0ull)
                off += (UINT64)w;
            if (w < got)
                break;
        }
        kfree(buf);
        if (a[2] != 0 && total >= 0)
            memcpy((void *)(UINTN)a[2], &off, 8);
        return total;
    }

    case NR_umask: {
        UINT32 old = p->lx->umask;
        p->lx->umask = (UINT32)a[0] & 0777u;
        return old;
    }

    case NR_statfs:
    case NR_fstatfs: {
        UINT64 ub = a[1];
        if (!uptr_ok(p, ub, 120, TRUE))
            return -LX_EFAULT;
        UINT64 sf[15];
        memset(sf, 0, sizeof(sf));
        sf[0] = 0x4d44;             /* f_type: MSDOS_SUPER_MAGIC */
        sf[1] = 4096;               /* f_bsize */
        sf[2] = 1u << 20;           /* f_blocks */
        sf[3] = 1u << 19;           /* f_bfree */
        sf[4] = 1u << 19;           /* f_bavail */
        sf[5] = 1u << 16;
        sf[6] = 1u << 15;
        sf[8] = 255;                /* f_namelen */
        sf[9] = 4096;               /* f_frsize */
        memcpy((void *)(UINTN)ub, sf, 120);
        return 0;
    }

    case NR_flock:
    case NR_fsync:
    case NR_fdatasync:
    case NR_fadvise64:
    case NR_fallocate:
        if (fd_get(p, (INT64)a[0]) == NULL)
            return -LX_EBADF;
        return 0;

    case NR_sync:
    case NR_syncfs:
        return 0;

    /* права и владельцы: у FAT их нет - "получилось" */
    case NR_chmod: case NR_fchmod: case NR_chown: case NR_fchown: case NR_lchown:
    case NR_fchownat: case NR_fchmodat: case NR_fchmodat2:
    case NR_utime: case NR_utimes: case NR_futimesat: case NR_utimensat:
        return 0;

    case NR_link: case NR_linkat: case NR_symlink: case NR_symlinkat:
        return -LX_EPERM;

    case NR_copy_file_range:
        return -LX_EXDEV;          /* glibc/coreutils тогда копируют сами */
    }

    *handled = FALSE;
    return -LX_ENOSYS;
}
