/*
 * user/posix/crt0.S - начало программы на полной libc (picolibc).
 *
 * Ядро прыгает сюда (_start) с rsp, указывающим на argc, за ним -
 * argv[0..], NULL (как у программ на мини-libc, user/lib/crt0.S).
 * Дальше - __myos_start (user/posix/os.c): часовой пояс, конструкторы,
 * main и exit.
 */
    .section .text.start
    .globl _start
_start:
    movq  (%rsp), %rdi          /* argc */
    leaq  8(%rsp), %rsi         /* argv */
    andq  $-16, %rsp            /* выравнивание стека для C */
    xorl  %ebp, %ebp            /* конец цепочки кадров (для отладчика) */
    call  __myos_start
1:  jmp 1b

    .section .note.GNU-stack,"",@progbits
