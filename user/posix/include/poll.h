/*
 * poll.h - poll из picolibc (её poll.h забывает подключить
 * sys/_types.h для __size_t). Реализация poll - user/posix/socket.c.
 */
#ifndef _MYOS_POLL_H
#define _MYOS_POLL_H

#include <sys/_types.h>
#include_next <poll.h>

#endif
