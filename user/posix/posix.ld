/*
 * user/posix/posix.ld - раскладка программы на полной libc (picolibc).
 * Как user/user.ld (статический ELF с 0x400000, код R+X, данные R+W),
 * плюс таблицы конструкторов .init_array / .fini_array, которые libc
 * обходит при старте и в exit.
 */
ENTRY(_start)

PHDRS
{
    text PT_LOAD FLAGS(5);    /* R + X */
    rodata PT_LOAD FLAGS(4);  /* R */
    data PT_LOAD FLAGS(6);    /* R + W */
    myos 0x6D794F53 FLAGS(0); /* метка "программа MyOS" (этап 11: иначе ядро
                                 решит, что это программа Linux) */
}

SECTIONS
{
    . = 0x400000;

    .text : { *(.text.start) *(.text .text.*) } :text

    . = ALIGN(4096);
    .rodata : { *(.rodata .rodata.*) } :rodata

    . = ALIGN(4096);
    .data : {
        PROVIDE_HIDDEN(__bothinit_array_start = .);
        PROVIDE_HIDDEN(__preinit_array_start = .);
        KEEP(*(.preinit_array))
        PROVIDE_HIDDEN(__preinit_array_end = .);
        PROVIDE_HIDDEN(__init_array_start = .);
        KEEP(*(SORT_BY_INIT_PRIORITY(.init_array.*) SORT_BY_INIT_PRIORITY(.ctors.*)))
        KEEP(*(.init_array .ctors))
        PROVIDE_HIDDEN(__init_array_end = .);
        PROVIDE_HIDDEN(__bothinit_array_end = .);
        PROVIDE_HIDDEN(__fini_array_start = .);
        KEEP(*(SORT_BY_INIT_PRIORITY(.fini_array.*) SORT_BY_INIT_PRIORITY(.dtors.*)))
        KEEP(*(.fini_array .dtors))
        PROVIDE_HIDDEN(__fini_array_end = .);
        *(.data .data.*)
    } :data
    .bss : { *(.bss .bss.*) *(COMMON) } :data

    /DISCARD/ : { *(.comment) *(.note*) *(.eh_frame*) }
}
