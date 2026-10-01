/*
 * tests/linux/ltest.c - проверка "программ Linux на MyOS" (этап 11).
 *
 * Это обычная программа для Linux: autotest собирает её на хосте
 * (gcc -static с glibc и musl-gcc -static) и запускает в MyOS с
 * флешки. Каждая проверка печатает "LTEST PASS имя" или
 * "LTEST FAIL имя - почему", в конце - "LTEST DONE pass=N fail=M".
 * На настоящем Linux программа тоже проходит все проверки (так её и
 * проверяли перед тем, как ждать того же от MyOS).
 */
#define _GNU_SOURCE
#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <poll.h>
#include <pthread.h>
#include <setjmp.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <sys/epoll.h>
#include <sys/timerfd.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/stat.h>
#include <sys/syscall.h>
#include <sys/utsname.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>

static int g_pass, g_fail;

static void result(const char *name, int ok, const char *why)
{
    if (ok) {
        printf("LTEST PASS %s\n", name);
        g_pass++;
    } else {
        printf("LTEST FAIL %s - %s (errno %d)\n", name, why, errno);
        g_fail++;
    }
    fflush(stdout);
}

static double now_ms(void)
{
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return ts.tv_sec * 1000.0 + ts.tv_nsec / 1e6;
}

/* ---------- память ---------- */

static void t_memory(void)
{
    size_t n = 64u << 20;
    unsigned char *p = malloc(n);

    if (p == NULL) {
        result("malloc-64M", 0, "malloc failed");
        return;
    }

    for (size_t i = 0; i < n; i += 4096)
        p[i] = (unsigned char)(i >> 12);

    int ok = 1;
    for (size_t i = 0; i < n; i += 4096)
        if (p[i] != (unsigned char)(i >> 12))
            ok = 0;

    free(p);
    result("malloc-64M", ok, "wrong data");

    /* много мелких - brk */
    void *v[1000];
    for (int i = 0; i < 1000; i++)
        v[i] = malloc(100 + i);
    ok = 1;
    for (int i = 0; i < 1000; i++) {
        if (v[i] == NULL)
            ok = 0;
        else
            memset(v[i], i & 0xFF, 100 + i);
    }
    for (int i = 0; i < 1000; i++)
        free(v[i]);
    result("malloc-small", ok, "malloc failed");
}

/* ---------- общая память (этап 11, шаг 2) ---------- */

static void t_shm(void)
{
    /* memfd: файл в памяти, его страницы видны через mmap(MAP_SHARED);
       после fork потомок пишет - родитель видит */
    int fd = (int)syscall(SYS_memfd_create, "ltest", 1u);
    int ok = fd >= 0 && ftruncate(fd, 8192) == 0;
    char *m = ok ? mmap(NULL, 8192, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0) : MAP_FAILED;

    if (m != MAP_FAILED) {
        strcpy(m + 4096, "from parent");
        pid_t c = fork();
        if (c == 0) {
            strcpy(m, "from child");
            _exit(0);
        }
        int st = 0;
        waitpid(c, &st, 0);
        char back[16] = { 0 };
        ssize_t got = pread(fd, back, 10, 0);
        ok = strcmp(m, "from child") == 0 && got == 10 && memcmp(back, "from child", 10) == 0;
        munmap(m, 8192);
    } else {
        ok = 0;
    }
    if (fd >= 0)
        close(fd);
    result("memfd-shared", ok, "memfd + MAP_SHARED + fork");

    /* общая анонимная память: страницу трогает только потомок */
    volatile int *a = mmap(NULL, 4096 * 4, PROT_READ | PROT_WRITE, MAP_SHARED | MAP_ANONYMOUS, -1, 0);
    ok = a != MAP_FAILED;
    if (ok) {
        pid_t c = fork();
        if (c == 0) {
            a[2048] = 12345;            /* вторая страница - родитель её не трогал */
            _exit(0);
        }
        int st = 0;
        waitpid(c, &st, 0);
        ok = a[2048] == 12345;
        munmap((void *)a, 4096 * 4);
    }
    result("anon-shared-fork", ok, "MAP_SHARED|MAP_ANONYMOUS after fork");

    /* файл, отображённый MAP_PRIVATE: запись в память не трогает файл */
    int tf = open("/tmp/ltest-map", O_RDWR | O_CREAT | O_TRUNC, 0644);
    ok = tf >= 0 && write(tf, "abcdef", 6) == 6;
    char *pm = ok ? mmap(NULL, 4096, PROT_READ | PROT_WRITE, MAP_PRIVATE, tf, 0) : MAP_FAILED;
    if (pm != MAP_FAILED) {
        int first = memcmp(pm, "abcdef", 6) == 0 && pm[6] == 0;
        pm[0] = 'X';
        char back[8] = { 0 };
        pread(tf, back, 6, 0);
        ok = first && back[0] == 'a' && pm[0] == 'X';
        munmap(pm, 4096);
    } else {
        ok = 0;
    }
    if (tf >= 0)
        close(tf);
    unlink("/tmp/ltest-map");
    result("map-private-cow", ok, "MAP_PRIVATE copy on write");
}

/* ---------- сокеты, epoll, timerfd (этап 11, шаг 2) ---------- */

/* Отправить открытый файл fd через сокет s (SCM_RIGHTS) */
static int send_fd(int s, int fd)
{
    char c = 'F';
    struct iovec iov = { &c, 1 };
    union { struct cmsghdr h; char b[CMSG_SPACE(sizeof(int))]; } u;
    struct msghdr m;

    memset(&m, 0, sizeof(m));
    memset(&u, 0, sizeof(u));
    m.msg_iov = &iov;
    m.msg_iovlen = 1;
    m.msg_control = u.b;
    m.msg_controllen = sizeof(u.b);
    struct cmsghdr *h = CMSG_FIRSTHDR(&m);
    h->cmsg_level = SOL_SOCKET;
    h->cmsg_type = SCM_RIGHTS;
    h->cmsg_len = CMSG_LEN(sizeof(int));
    memcpy(CMSG_DATA(h), &fd, sizeof(int));
    return (int)sendmsg(s, &m, 0);
}

static int recv_fd(int s)
{
    char c = 0;
    struct iovec iov = { &c, 1 };
    union { struct cmsghdr h; char b[CMSG_SPACE(sizeof(int))]; } u;
    struct msghdr m;

    memset(&m, 0, sizeof(m));
    m.msg_iov = &iov;
    m.msg_iovlen = 1;
    m.msg_control = u.b;
    m.msg_controllen = sizeof(u.b);
    if (recvmsg(s, &m, 0) != 1 || c != 'F')
        return -1;
    struct cmsghdr *h = CMSG_FIRSTHDR(&m);
    if (h == NULL || h->cmsg_type != SCM_RIGHTS)
        return -1;
    int fd;
    memcpy(&fd, CMSG_DATA(h), sizeof(int));
    return fd;
}

static void t_sock(void)
{
    /* socketpair + передача канала (pipe) другому процессу */
    int sv[2];
    int ok = socketpair(AF_UNIX, SOCK_STREAM, 0, sv) == 0;
    if (ok) {
        pid_t c = fork();
        if (c == 0) {
            close(sv[0]);
            int fd = recv_fd(sv[1]);
            if (fd < 0)
                _exit(2);
            ssize_t w = write(fd, "via-fd", 6);
            _exit(w == 6 ? 0 : 3);
        }
        close(sv[1]);
        int pf[2];
        ok = pipe(pf) == 0 && send_fd(sv[0], pf[1]) == 1;
        if (ok) {
            close(pf[1]);
            char b[16] = { 0 };
            ssize_t r = read(pf[0], b, sizeof(b) - 1);
            int st = 0;
            waitpid(c, &st, 0);
            ok = r == 6 && strcmp(b, "via-fd") == 0 && WIFEXITED(st) && WEXITSTATUS(st) == 0;
            close(pf[0]);
        }
        close(sv[0]);
    }
    result("socketpair-scm-rights", ok, "socketpair + SCM_RIGHTS + fork");

    /* AF_UNIX по имени: listen / connect / accept */
    struct sockaddr_un sa;
    memset(&sa, 0, sizeof(sa));
    sa.sun_family = AF_UNIX;
    strcpy(sa.sun_path, "/tmp/ltest.sock");
    unlink(sa.sun_path);
    int ls = socket(AF_UNIX, SOCK_STREAM, 0);
    ok = ls >= 0 && bind(ls, (struct sockaddr *)&sa, sizeof(sa)) == 0 && listen(ls, 4) == 0;
    if (ok) {
        struct stat st;
        int is_sock = stat(sa.sun_path, &st) == 0 && S_ISSOCK(st.st_mode);
        int cs = socket(AF_UNIX, SOCK_STREAM | SOCK_CLOEXEC, 0);
        int con = connect(cs, (struct sockaddr *)&sa, sizeof(sa)) == 0;
        int as = accept(ls, NULL, NULL);
        char b[8] = { 0 };
        ok = is_sock && con && as >= 0 && write(cs, "ping", 4) == 4 && read(as, b, 4) == 4 &&
             strcmp(b, "ping") == 0;
        close(cs);
        ok = ok && read(as, b, 4) == 0;         /* собеседник закрыл - конец */
        if (as >= 0)
            close(as);
    }
    if (ls >= 0)
        close(ls);
    unlink(sa.sun_path);
    result("unix-listen-connect", ok, "bind/listen/connect/accept");

    /* epoll: канал станет готов, когда в него напишут */
    int ep = epoll_create1(EPOLL_CLOEXEC);
    int pf[2];
    ok = ep >= 0 && pipe(pf) == 0;
    if (ok) {
        struct epoll_event ev = { .events = EPOLLIN, .data.u64 = 77 };
        struct epoll_event out[4];
        ok = epoll_ctl(ep, EPOLL_CTL_ADD, pf[0], &ev) == 0 && epoll_wait(ep, out, 4, 0) == 0;
        ok = ok && write(pf[1], "x", 1) == 1;
        int n = epoll_wait(ep, out, 4, 1000);
        ok = ok && n == 1 && out[0].data.u64 == 77 && (out[0].events & EPOLLIN);
        close(pf[0]);
        close(pf[1]);
    }
    if (ep >= 0)
        close(ep);
    result("epoll", ok, "epoll_wait on a pipe");

    /* timerfd: 50 мс */
    int tf = timerfd_create(CLOCK_MONOTONIC, 0);
    ok = tf >= 0;
    if (ok) {
        struct itimerspec its;
        memset(&its, 0, sizeof(its));
        its.it_value.tv_nsec = 50 * 1000000;
        double t0 = now_ms();
        unsigned long long cnt = 0;
        ok = timerfd_settime(tf, 0, &its, NULL) == 0 && read(tf, &cnt, 8) == 8 && cnt == 1 &&
             now_ms() - t0 >= 40;
        close(tf);
    }
    result("timerfd", ok, "timerfd 50 ms");

    /* TCP через 127.0.0.1 (сеть MyOS) */
    int srv = socket(AF_INET, SOCK_STREAM, 0);
    struct sockaddr_in in;
    memset(&in, 0, sizeof(in));
    in.sin_family = AF_INET;
    in.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    in.sin_port = 0;
    socklen_t il = sizeof(in);
    ok = srv >= 0 && bind(srv, (struct sockaddr *)&in, sizeof(in)) == 0 && listen(srv, 4) == 0 &&
         getsockname(srv, (struct sockaddr *)&in, &il) == 0 && ntohs(in.sin_port) != 0;
    if (ok) {
        pid_t c = fork();
        if (c == 0) {
            int cl = socket(AF_INET, SOCK_STREAM, 0);
            if (connect(cl, (struct sockaddr *)&in, sizeof(in)) != 0)
                _exit(2);
            ssize_t w = write(cl, "hello tcp", 9);
            close(cl);
            _exit(w == 9 ? 0 : 3);
        }
        int a = accept(srv, NULL, NULL);
        char b[16] = { 0 };
        ssize_t got = 0, r;
        while (a >= 0 && got < 9 && (r = read(a, b + got, sizeof(b) - 1 - got)) > 0)
            got += r;
        int st = 0;
        waitpid(c, &st, 0);
        ok = a >= 0 && got == 9 && strcmp(b, "hello tcp") == 0 && WIFEXITED(st) &&
             WEXITSTATUS(st) == 0;
        if (a >= 0)
            close(a);
    }
    if (srv >= 0)
        close(srv);
    result("tcp-loopback", ok, "TCP over 127.0.0.1");
}

/* ---------- файлы ---------- */

static void t_files(void)
{
    const char *path = "/tmp/ltest.txt";
    int fd = open(path, O_CREAT | O_TRUNC | O_WRONLY, 0644);

    if (fd < 0) {
        result("file-write", 0, "open for write");
        return;
    }

    const char *msg = "hello from a Linux program\nsecond line\n";
    ssize_t w = write(fd, msg, strlen(msg));
    close(fd);
    result("file-write", w == (ssize_t)strlen(msg), "short write");

    char buf[128] = { 0 };
    fd = open(path, O_RDONLY);
    ssize_t r = read(fd, buf, sizeof(buf) - 1);
    struct stat st;
    int fs = fstat(fd, &st);
    off_t pos = lseek(fd, 6, SEEK_SET);
    char c = 0;
    read(fd, &c, 1);
    close(fd);

    result("file-read", r == (ssize_t)strlen(msg) && strcmp(buf, msg) == 0, "content differs");
    result("file-stat", fs == 0 && st.st_size == (off_t)strlen(msg) && S_ISREG(st.st_mode),
           "fstat");
    result("file-lseek", pos == 6 && c == 'f', "lseek");

    /* dup2 на свой номер, запись через него */
    fd = open(path, O_WRONLY | O_APPEND);
    int d = dup2(fd, 10);
    write(10, "third\n", 6);
    close(fd);
    close(10);
    stat(path, &st);
    result("file-dup2", d == 10 && st.st_size == (off_t)strlen(msg) + 6, "dup2/append");

    int rn = rename(path, "/tmp/ltest2.txt");
    int ex = access("/tmp/ltest2.txt", F_OK);
    int un = unlink("/tmp/ltest2.txt");
    int gone = access("/tmp/ltest2.txt", F_OK);
    result("file-rename-unlink", rn == 0 && ex == 0 && un == 0 && gone != 0, "rename/unlink");

    int md = mkdir("/tmp/ltdir", 0755);
    struct stat ds;
    int dst = stat("/tmp/ltdir", &ds);
    int rd = rmdir("/tmp/ltdir");
    result("mkdir-rmdir", md == 0 && dst == 0 && S_ISDIR(ds.st_mode) && rd == 0, "mkdir/rmdir");
}

static void t_dirs(void)
{
    DIR *d = opendir("/bin");
    int n = 0, dot = 0;
    struct dirent *e;

    if (d != NULL) {
        while ((e = readdir(d)) != NULL) {
            n++;
            if (strcmp(e->d_name, ".") == 0)
                dot = 1;
        }
        closedir(d);
    }

    result("readdir", n > 5 && dot, "opendir/readdir /bin");

    char cwd[256];
    int ch = chdir("/tmp");
    char *g = getcwd(cwd, sizeof(cwd));
    result("chdir-getcwd", ch == 0 && g != NULL && strcmp(cwd, "/tmp") == 0, cwd);
    chdir("/");
}

/* ---------- процессы ---------- */

static int g_cow = 1;

static void t_fork_pipe(void)
{
    int fds[2];

    if (pipe(fds) != 0) {
        result("pipe-fork", 0, "pipe");
        return;
    }

    pid_t pid = fork();

    if (pid == 0) {
        close(fds[0]);
        g_cow = 2;
        const char *m = "hi from child";
        write(fds[1], m, strlen(m));
        close(fds[1]);
        _exit(7);
    }

    close(fds[1]);
    char buf[64] = { 0 };
    ssize_t r = 0, k;
    while ((k = read(fds[0], buf + r, sizeof(buf) - 1 - r)) > 0)
        r += k;
    close(fds[0]);

    int st = 0;
    pid_t w = waitpid(pid, &st, 0);

    result("pipe-fork", pid > 0 && strcmp(buf, "hi from child") == 0, buf);
    result("waitpid", w == pid && WIFEXITED(st) && WEXITSTATUS(st) == 7, "exit status");
    result("fork-cow", g_cow == 1, "child changed parent memory");
}

static void t_exec(const char *self)
{
    pid_t pid = fork();

    if (pid == 0) {
        char *av[] = { (char *)self, "exec-child", "arg2", NULL };
        char *ev[] = { "LTEST_VAR=works", NULL };
        execve(self, av, ev);
        _exit(99);
    }

    int st = 0;
    waitpid(pid, &st, 0);
    result("fork-exec", WIFEXITED(st) && WEXITSTATUS(st) == 3, "execve child status");

    /* posix_spawn-подобное: vfork */
    pid = vfork();
    if (pid == 0) {
        char *av[] = { (char *)self, "exec-child", "arg2", NULL };
        char *ev[] = { "LTEST_VAR=works", NULL };
        execve(self, av, ev);
        _exit(99);
    }
    st = 0;
    waitpid(pid, &st, 0);
    result("vfork-exec", WIFEXITED(st) && WEXITSTATUS(st) == 3, "vfork child status");
}

/* ---------- потоки ---------- */

static pthread_mutex_t g_mx = PTHREAD_MUTEX_INITIALIZER;
static long g_counter;
static __thread int g_tls = 5;

static void *th_add(void *arg)
{
    long id = (long)arg;

    g_tls = (int)id * 10;

    for (int i = 0; i < 100000; i++) {
        pthread_mutex_lock(&g_mx);
        g_counter++;
        pthread_mutex_unlock(&g_mx);
    }

    return (void *)(long)(g_tls == (int)id * 10);
}

static void t_threads(void)
{
    pthread_t th[4];
    int ok = 1;

    for (long i = 0; i < 4; i++)
        if (pthread_create(&th[i], NULL, th_add, (void *)(i + 1)) != 0)
            ok = 0;

    int tls_ok = 1;
    for (int i = 0; i < 4; i++) {
        void *r = NULL;
        pthread_join(th[i], &r);
        if (r == NULL)
            tls_ok = 0;
    }

    result("threads-mutex", ok && g_counter == 400000, "counter");
    result("threads-tls", tls_ok && g_tls == 5, "thread-local storage");

    /* условная переменная со сроком: ETIMEDOUT примерно через 50 мс */
    pthread_cond_t cv = PTHREAD_COND_INITIALIZER;
    struct timespec ts;
    clock_gettime(CLOCK_REALTIME, &ts);
    ts.tv_nsec += 50 * 1000000;
    if (ts.tv_nsec >= 1000000000) {
        ts.tv_sec++;
        ts.tv_nsec -= 1000000000;
    }
    double t0 = now_ms();
    pthread_mutex_lock(&g_mx);
    int rc = pthread_cond_timedwait(&cv, &g_mx, &ts);
    pthread_mutex_unlock(&g_mx);
    double dt = now_ms() - t0;
    result("cond-timedwait", rc == ETIMEDOUT && dt >= 30 && dt < 2000, "timeout");
}

/* ---------- сигналы ---------- */

static volatile sig_atomic_t g_sig;
static sigjmp_buf g_jb;

static void on_sig(int s)
{
    g_sig = s;
}

static void on_segv(int s, siginfo_t *si, void *uc)
{
    (void)s;
    (void)uc;
    g_sig = (int)(long)si->si_addr == 0x10 ? 100 : 101;
    siglongjmp(g_jb, 1);
}

static void t_signals(void)
{
    struct sigaction sa;

    memset(&sa, 0, sizeof(sa));
    sa.sa_handler = on_sig;
    sigaction(SIGUSR1, &sa, NULL);
    g_sig = 0;
    raise(SIGUSR1);
    result("signal-raise", g_sig == SIGUSR1, "handler not called");

    sigaction(SIGUSR2, &sa, NULL);
    g_sig = 0;
    kill(getpid(), SIGUSR2);
    result("signal-kill", g_sig == SIGUSR2, "handler not called");

    sigaction(SIGALRM, &sa, NULL);
    g_sig = 0;
    double t0 = now_ms();
    alarm(1);
    while (g_sig == 0 && now_ms() - t0 < 5000)
        pause();
    result("alarm-pause", g_sig == SIGALRM, "no SIGALRM");

    /* ошибка памяти - свой обработчик и выход через siglongjmp */
    memset(&sa, 0, sizeof(sa));
    sa.sa_sigaction = on_segv;
    sa.sa_flags = SA_SIGINFO;
    sigaction(SIGSEGV, &sa, NULL);
    g_sig = 0;
    if (sigsetjmp(g_jb, 1) == 0) {
        *(volatile int *)0x10 = 1;
    }
    result("sigsegv-handler", g_sig == 100, "SIGSEGV not caught");

    /* mprotect: запись в страницу "только чтение" - тоже SIGSEGV */
    char *pg = mmap(NULL, 4096, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    pg[0] = 'x';
    mprotect(pg, 4096, PROT_READ);
    g_sig = 0;
    if (sigsetjmp(g_jb, 1) == 0)
        pg[1] = 'y';
    result("mprotect", g_sig != 0 && pg[0] == 'x', "write to read-only page went through");
    munmap(pg, 4096);

    /* сигнал прерывает долгий вызов: read из канала - EINTR */
    memset(&sa, 0, sizeof(sa));
    sa.sa_handler = on_sig;
    sigaction(SIGALRM, &sa, NULL);
    int fds[2];
    pipe(fds);
    g_sig = 0;
    alarm(1);
    char c;
    ssize_t r = read(fds[0], &c, 1);
    result("eintr", r == -1 && errno == EINTR && g_sig == SIGALRM, "read not interrupted");
    close(fds[0]);
    close(fds[1]);
}

/* ---------- время, poll, /proc ---------- */

static void t_misc(void)
{
    double t0 = now_ms();
    struct timespec ts = { 0, 30 * 1000000 };
    nanosleep(&ts, NULL);
    double dt = now_ms() - t0;
    result("nanosleep", dt >= 25 && dt < 1000, "sleep length");

    time_t t = time(NULL);
    result("time", t > 1700000000, "clock is not set");

    struct utsname u;
    uname(&u);
    result("uname", strcmp(u.sysname, "Linux") == 0 && strcmp(u.machine, "x86_64") == 0,
           u.sysname);

    char exe[256] = { 0 };
    ssize_t n = readlink("/proc/self/exe", exe, sizeof(exe) - 1);
    result("proc-self-exe", n > 0 && strstr(exe, "lt") != NULL, exe);

    int fds[2];
    pipe(fds);
    struct pollfd pf = { fds[0], POLLIN, 0 };
    t0 = now_ms();
    int pr = poll(&pf, 1, 100);
    dt = now_ms() - t0;
    write(fds[1], "z", 1);
    int pr2 = poll(&pf, 1, 1000);
    result("poll", pr == 0 && dt >= 80 && pr2 == 1 && (pf.revents & POLLIN), "poll on a pipe");
    close(fds[0]);
    close(fds[1]);

    int dn = open("/dev/null", O_WRONLY);
    ssize_t dw = write(dn, "abc", 3);
    close(dn);
    int ur = open("/dev/urandom", O_RDONLY);
    unsigned char rb[16] = { 0 };
    ssize_t urr = read(ur, rb, 16);
    close(ur);
    result("dev-files", dw == 3 && urr == 16, "/dev/null or /dev/urandom");
}

int main(int argc, char **argv)
{
    if (argc > 1 && strcmp(argv[1], "exec-child") == 0) {
        const char *v = getenv("LTEST_VAR");
        printf("LTEST exec child: %s %s\n", argv[2], v ? v : "(no env)");
        return (v && strcmp(v, "works") == 0 && strcmp(argv[2], "arg2") == 0) ? 3 : 4;
    }

    printf("LTEST start: %s, pid %d\n", argv[0], (int)getpid());
    result("args-env", argc >= 1 && getenv("PATH") != NULL, "no PATH");

    t_memory();
    t_shm();
    t_sock();
    t_files();
    t_dirs();
    t_fork_pipe();
    t_exec(argv[0]);
    t_threads();
    t_signals();
    t_misc();

    printf("LTEST DONE pass=%d fail=%d\n", g_pass, g_fail);
    return g_fail ? 1 : 0;
}
