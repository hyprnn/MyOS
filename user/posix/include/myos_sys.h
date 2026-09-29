/*
 * user/posix/include/myos_sys.h - то, чего нет в обычной libc: прямые
 * системные вызовы MyOS для программ на полной libc (picolibc, этап 9).
 *
 * Программы на picolibc пишутся как для Linux: printf, fopen, malloc,
 * time, opendir... - всё это в libc, а к ядру MyOS её привязывает
 * user/posix/os.c. Здесь - только особенное для MyOS (окна, сеть в
 * виде "как есть"), для программ, которым оно нужно.
 */
#ifndef MYOS_POSIX_SYS_H
#define MYOS_POSIX_SYS_H

#define MYOS_POSIX 1
#include "../../../sysnum.h"

/* Системный вызов: номер SYS_* и до трёх аргументов; отрицательный
   ответ - ошибка MYOS_E* (не errno!) */
long myos_syscall3(long nr, long a1, long a2, long a3);

/* То же, с четырьмя (sendto/recvfrom: четвёртый - в r10) */
long myos_syscall4(long nr, long a1, long a2, long a3, long a4);

/* Ошибку MYOS_E* - в errno (для своих обёрток): errno = ..., ответ -1 */
int myos_set_errno(long myos_err);

#endif
