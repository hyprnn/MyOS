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

/*
 * Встроенные файлы: программа может вшить в себя файлы (шрифты,
 * стили) таблицей __myos_embedded_files - libc покажет их как файлы
 * только для чтения "/embed/<name>" (user/posix/os.c). Конец таблицы -
 * запись с name == NULL.
 */
struct myos_embed_file {
    const char          *name;     /* "fonts/DejaVuSans.ttf" */
    const unsigned char *data;
    unsigned long        size;
};

/* Звук (этап 10, user/posix/audio.c): 48000 Гц, 16 бит, стерео.
   Ответы - как у ядра: 0 / отрицательная ошибка MYOS_E*. */
int  audio_open(void);                               /* занять звук */
long audio_write(const void *buf, unsigned long n);  /* ждёт места -> байт */
int  audio_close(void);
int  audio_drain(void);                              /* дождаться конца */
int  audio_info(struct myos_audio_info *i);
int  audio_volume(int v);                            /* -1 узнать, -2 вкл/выкл */

/* Ошибку MYOS_E* - в errno (для своих обёрток): errno = ..., ответ -1 */
int myos_set_errno(long myos_err);

#endif
