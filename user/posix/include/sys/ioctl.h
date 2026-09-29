/*
 * sys/ioctl.h - ioctl из picolibc плюс номер FIONBIO (как в Linux):
 * неблокирующий сокет. Реализация ioctl - user/posix/os.c.
 */
#ifndef _MYOS_SYS_IOCTL_H
#define _MYOS_SYS_IOCTL_H

#include_next <sys/ioctl.h>

#define FIONBIO  0x5421

#endif
