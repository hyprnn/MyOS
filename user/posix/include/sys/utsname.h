/*
 * sys/utsname.h - uname(): имя и версия системы (в picolibc этого нет).
 * Реализация - user/posix/os.c.
 */
#ifndef _MYOS_SYS_UTSNAME_H
#define _MYOS_SYS_UTSNAME_H

#define _UTSNAME_LENGTH 65

struct utsname {
    char sysname[_UTSNAME_LENGTH];     /* "MyOS" */
    char nodename[_UTSNAME_LENGTH];    /* имя компьютера */
    char release[_UTSNAME_LENGTH];
    char version[_UTSNAME_LENGTH];
    char machine[_UTSNAME_LENGTH];     /* "x86_64" */
};

int uname(struct utsname *u);

#endif
