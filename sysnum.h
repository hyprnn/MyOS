/*
 * sysnum.h - номера системных вызовов MyOS и структуры, которыми
 * обмениваются ядро и программы (этап 6). Этот файл включают И ядро
 * (myos.h), И программы (user/include/myos.h) - поэтому здесь только
 * простые типы C, без UINT64 и прочих типов ядра.
 *
 * Как программа зовёт ядро (соглашение как в Linux x86-64):
 *   rax = номер вызова, аргументы: rdi, rsi, rdx, r10, r8, r9;
 *   инструкция syscall; ответ - в rax. Отрицательный ответ - ошибка
 *   (коды - как VFS_E* в ядре, см. MYOS_E* ниже).
 */
#ifndef MYOS_SYSNUM_H
#define MYOS_SYSNUM_H

#define SYS_EXIT      0   /* exit(code)                         - не возвращается */
#define SYS_WRITE     1   /* write(fd, buf, n)   1,2 - экран    -> записано байт */
#define SYS_READ      2   /* read(fd, buf, n)    0 - клавиатура -> прочитано байт */
#define SYS_OPEN      3   /* open(path, flags)                  -> fd (от 3) */
#define SYS_CLOSE     4   /* close(fd) */
#define SYS_SLEEP     5   /* sleep(ms) */
#define SYS_UPTIME    6   /* uptime()                           -> мс со старта */
#define SYS_SBRK      7   /* sbrk(прибавка)                     -> старый конец кучи */
#define SYS_GETPID    8   /* getpid() */
#define SYS_TIME      9   /* time(struct myos_time *)           - местное время */
#define SYS_READDIR  10   /* readdir(path, номер, struct myos_dirent *) -> 1 / 0 (конец) */
#define SYS_MKDIR    11   /* mkdir(path) */
#define SYS_UNLINK   12   /* unlink(path)  - файл или пустая папка */
#define SYS_RENAME   13   /* rename(from, to) */
#define SYS_YIELD    14   /* уступить процессор */
#define SYS_STAT     15   /* stat(path, struct myos_dirent *) */
#define SYS_GETKEY   16   /* getkey() -> символ, 0 - нет нажатия (не ждёт) */
#define SYS_COUNT    17

/* флаги open - те же, что VFS_O_* в ядре */
#define O_READ    0x01
#define O_WRITE   0x02
#define O_CREATE  0x04
#define O_TRUNC   0x08
#define O_APPEND  0x10

/* ошибки (отрицательные ответы) - те же, что VFS_E* в ядре */
#define MYOS_ENOENT     -2
#define MYOS_EEXIST     -3
#define MYOS_ENOTDIR    -4
#define MYOS_EISDIR     -5
#define MYOS_ENOTEMPTY  -6
#define MYOS_ENOSPC     -7
#define MYOS_EROFS      -8
#define MYOS_EIO        -9
#define MYOS_EINVAL    -10
#define MYOS_EBADF     -11
#define MYOS_EMFILE    -12
#define MYOS_ENOSYS    -13
#define MYOS_EGONE     -14
#define MYOS_EXDEV     -15
#define MYOS_EFAULT    -16   /* плохой указатель от программы */

struct myos_time {
    unsigned short year;
    unsigned char  month, day, hour, minute, second;
    unsigned char  pad;
};

struct myos_dirent {
    char               name[128];
    unsigned long long size;
    unsigned int       is_dir;
    unsigned int       pad;
};

/* Адреса программы (нижняя половина адресного пространства) */
#define MYOS_USER_BASE       0x0000000000400000ull   /* сюда линкуются программы */
#define MYOS_USER_STACK_TOP  0x00007FFFFFFF0000ull   /* вершина стека */
#define MYOS_USER_STACK_SIZE (256ull * 1024ull)
#define MYOS_USER_LIMIT      0x00007FFF00000000ull   /* выше - только стек */

#endif
