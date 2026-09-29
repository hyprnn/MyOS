/*
 * user/lib/crt0.S - самое начало любой программы MyOS.
 *
 * Ядро прыгает сюда (_start) с rsp, указывающим на:
 *     [rsp]    argc
 *     [rsp+8]  argv[0], argv[1], ..., NULL
 * Мы зовём main(argc, argv) и отдаём её результат в exit - так
 * программе не нужно помнить про exit самой.
 */
    .section .text.start
    .globl _start
_start:
    movq  (%rsp), %rdi          /* argc */
    leaq  8(%rsp), %rsi         /* argv */
    andq  $-16, %rsp            /* выравнивание стека для C */
    call  main
    movl  %eax, %edi
    call  exit
1:  jmp 1b
