/*
 * user/lib/sys.c - системные вызовы: обёртки над инструкцией syscall.
 * Номер - в rax, аргументы - rdi, rsi, rdx (дальше r10, r8, r9 - нам
 * пока хватает трёх). Инструкция syscall портит rcx и r11.
 */
#include "myos.h"

long syscall3(long nr, long a1, long a2, long a3)
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

void exit(int code)
{
    syscall3(SYS_EXIT, code, 0, 0);
    for (;;) { }
}

long write(int fd, const void *buf, size_t n) { return syscall3(SYS_WRITE, fd, (long)buf, (long)n); }
long read(int fd, void *buf, size_t n)        { return syscall3(SYS_READ, fd, (long)buf, (long)n); }
int  open(const char *path, int flags)        { return (int)syscall3(SYS_OPEN, (long)path, flags, 0); }
int  close(int fd)                            { return (int)syscall3(SYS_CLOSE, fd, 0, 0); }
int  sleep_ms(unsigned long ms)               { return (int)syscall3(SYS_SLEEP, (long)ms, 0, 0); }
unsigned long uptime_ms(void)                 { return (unsigned long)syscall3(SYS_UPTIME, 0, 0, 0); }
int  getpid(void)                             { return (int)syscall3(SYS_GETPID, 0, 0, 0); }
int  gettime(struct myos_time *t)             { return (int)syscall3(SYS_TIME, (long)t, 0, 0); }
int  gettime_utc(struct myos_time *t)         { return (int)syscall3(SYS_TIME, (long)t, 1, 0); }
long getrandom(void *buf, size_t n)           { return syscall3(SYS_GETRANDOM, (long)buf, (long)n, 0); }
int  readdir(const char *p, int i, struct myos_dirent *d) { return (int)syscall3(SYS_READDIR, (long)p, i, (long)d); }
int  stat(const char *p, struct myos_dirent *d) { return (int)syscall3(SYS_STAT, (long)p, (long)d, 0); }
int  mkdir(const char *path)                  { return (int)syscall3(SYS_MKDIR, (long)path, 0, 0); }
int  unlink(const char *path)                 { return (int)syscall3(SYS_UNLINK, (long)path, 0, 0); }
int  rename(const char *a, const char *b)     { return (int)syscall3(SYS_RENAME, (long)a, (long)b, 0); }
int  yield(void)                              { return (int)syscall3(SYS_YIELD, 0, 0, 0); }
int  getkey(void)                             { return (int)syscall3(SYS_GETKEY, 0, 0, 0); }

/* этап 10: для шелла-программы */
int  chdir(const char *path)                  { return (int)syscall3(SYS_CHDIR, (long)path, 0, 0); }
long getcwd_len(char *buf, size_t cap)        { return syscall3(SYS_GETCWD, (long)buf, (long)cap, 0); }
int  spawn(const struct myos_spawn *sp)       { return (int)syscall3(SYS_SPAWN, (long)sp, 0, 0); }
int  waitpid_info(int pid, struct myos_waitinfo *wi, int flags)
{
    return (int)syscall3(SYS_WAIT, pid, (long)wi, flags);
}
int  readkey(long timeout_ms)                 { return (int)syscall3(SYS_READKEY, timeout_ms, 0, 0); }
/* звук (этап 10) */
int  audio_open(void)                           { return (int)syscall3(SYS_AUDIO, MYOS_AUDIO_OPEN, 0, 0); }
long audio_write(const void *buf, size_t n)     { return syscall3(SYS_AUDIO, MYOS_AUDIO_WRITE, (long)buf, (long)n); }
int  audio_close(void)                          { return (int)syscall3(SYS_AUDIO, MYOS_AUDIO_CLOSE, 0, 0); }
int  audio_drain(void)                          { return (int)syscall3(SYS_AUDIO, MYOS_AUDIO_DRAIN, 0, 0); }
int  audio_info(struct myos_audio_info *i)      { return (int)syscall3(SYS_AUDIO, MYOS_AUDIO_INFO, (long)i, 0); }
int  audio_volume(int v)                        { return (int)syscall3(SYS_AUDIO, MYOS_AUDIO_VOLUME, v, 0); }

int  kcmd(const char *line, const char *out_path, int flags)
{
    return (int)syscall3(SYS_KCMD, (long)line, (long)out_path, flags);
}

void *sbrk(long inc)
{
    long r = syscall3(SYS_SBRK, inc, 0, 0);

    return (r < 0) ? (void *)-1 : (void *)r;
}
