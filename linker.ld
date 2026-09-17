/* =============================================================================
 * SENG21213-OS :: Linker Script
 *
 * Kernel load address: 0x10000
 * Separates executable and writable ELF segments.
 * ============================================================================ */

OUTPUT_FORMAT(elf32-i386)
ENTRY(_start)

PHDRS
{
    text PT_LOAD FLAGS(5);   /* Read + Execute */
    data PT_LOAD FLAGS(6);   /* Read + Write   */
}

SECTIONS
{
    . = 0x10000;

    .text : {
        *(.text)
        *(.text.*)
    } :text

    .rodata : {
        *(.rodata)
        *(.rodata.*)
    } :text

    .data : {
        *(.data)
        *(.data.*)
    } :data

    .bss : {
    *(COMMON)
    *(.bss)
    *(.bss.*)

       /* Physical end of the kernel, including all BSS data. */
       __kernel_end = .;
    } :data

    /DISCARD/ : {
        *(.eh_frame)
        *(.comment)
        *(.note.GNU-stack)
    }
}
