/*
 * user/posix/audio.c - звук для программ на полной libc (этап 10):
 * те же вызовы, что у мини-libc (user/lib/sys.c), - play, doom.
 * Формат: 48000 Гц, 16 бит со знаком, стерео.
 */
#include "myos_sys.h"

int  audio_open(void)                       { return (int)myos_syscall3(SYS_AUDIO, MYOS_AUDIO_OPEN, 0, 0); }
long audio_write(const void *buf, unsigned long n)
{
    return myos_syscall3(SYS_AUDIO, MYOS_AUDIO_WRITE, (long)buf, (long)n);
}
int  audio_close(void)                      { return (int)myos_syscall3(SYS_AUDIO, MYOS_AUDIO_CLOSE, 0, 0); }
int  audio_drain(void)                      { return (int)myos_syscall3(SYS_AUDIO, MYOS_AUDIO_DRAIN, 0, 0); }
int  audio_info(struct myos_audio_info *i)  { return (int)myos_syscall3(SYS_AUDIO, MYOS_AUDIO_INFO, (long)i, 0); }
int  audio_volume(int v)                    { return (int)myos_syscall3(SYS_AUDIO, MYOS_AUDIO_VOLUME, v, 0); }
