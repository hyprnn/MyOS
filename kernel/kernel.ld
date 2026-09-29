/*
 * kernel/kernel.ld - как раскладывать ядро в памяти (скрипт линкера).
 *
 * Ядро живёт в "верхней половине" адресного пространства, с адреса
 * 0xFFFFFFFF80000000 (последние 2 ГиБ из 16 ЭиБ): нижняя половина
 * остаётся будущим программам (этап 6), а с -mcmodel=kernel
 * компилятор может адресовать всё ядро 32-битными смещениями.
 *
 * Разделы идут в таком порядке и каждый начинается с новой 4-КиБ
 * страницы - чтобы ядро (kernel/vmm.c) могло дать им разные права:
 *   .text              код       - чтение + исполнение
 *   .rodata            константы - только чтение, без исполнения
 *   .data, .bss        данные    - чтение + запись, без исполнения
 * Символы __kernel_* ниже - "метки" этих границ для C-кода.
 */
OUTPUT_FORMAT("elf64-x86-64")
OUTPUT_ARCH(i386:x86-64)
ENTRY(kmain)

PHDRS
{
    text   PT_LOAD FLAGS(5);   /* R X */
    rodata PT_LOAD FLAGS(4);   /* R   */
    data   PT_LOAD FLAGS(6);   /* R W */
}

SECTIONS
{
    . = 0xFFFFFFFF80000000;
    __kernel_start = .;

    .text : ALIGN(4096) {
        *(.text.kmain)
        *(.text .text.*)
    } :text
    . = ALIGN(4096);
    __kernel_text_end = .;

    .rodata : ALIGN(4096) {
        *(.rodata .rodata.*)
    } :rodata
    .eh_frame : { *(.eh_frame) } :rodata
    . = ALIGN(4096);
    __kernel_rodata_end = .;

    .data : ALIGN(4096) {
        *(.data .data.*)
    } :data
    .bss : {
        *(COMMON)
        *(.bss .bss.*)
    } :data
    . = ALIGN(4096);
    __kernel_end = .;

    /DISCARD/ : {
        *(.comment)
        *(.note .note.*)
    }
}
