/*
 * user/user.ld - раскладка программы MyOS в памяти (этап 6).
 * Программа - статический ELF, начинается с 0x400000 (как в Linux).
 * Код - только чтение и исполнение, данные - чтение и запись без
 * исполнения: ядро ставит права страниц по флагам сегментов.
 */
ENTRY(_start)

PHDRS
{
    text PT_LOAD FLAGS(5);    /* R + X */
    rodata PT_LOAD FLAGS(4);  /* R */
    data PT_LOAD FLAGS(6);    /* R + W */
}

SECTIONS
{
    . = 0x400000;

    .text : { *(.text.start) *(.text .text.*) } :text

    . = ALIGN(4096);
    .rodata : { *(.rodata .rodata.*) } :rodata

    . = ALIGN(4096);
    .data : { *(.data .data.*) } :data
    .bss : { *(.bss .bss.*) *(COMMON) } :data

    /DISCARD/ : { *(.comment) *(.note*) *(.eh_frame*) }
}
