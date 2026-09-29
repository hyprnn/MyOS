/*
 * sys/uio.h - struct iovec и writev/readv (для программ MyOS на полной
 * libc; в picolibc этого нет). Реализация - user/posix/socket.c.
 */
#ifndef _MYOS_SYS_UIO_H
#define _MYOS_SYS_UIO_H

#include <sys/types.h>

struct iovec {
    void  *iov_base;
    size_t iov_len;
};

#define IOV_MAX 1024

ssize_t readv(int fd, const struct iovec *iov, int n);
ssize_t writev(int fd, const struct iovec *iov, int n);

#endif
