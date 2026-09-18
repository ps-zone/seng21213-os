/* =============================================================================
 * SENG21213-OS :: Linker Script
 * File   : linker.ld
 *
 * Places the kernel binary starting at physical address 0x10000.
 * The bootloader loads the kernel to 0x1000:0x0000 = physical 0x10000.
 * ============================================================================*/

OUTPUT_FORMAT(elf32-i386)
ENTRY(_start)

PHDRS {
    text PT_LOAD FLAGS(5);    /* Read + Execute */
    data PT_LOAD FLAGS(6);    /* Read + Write */
}

SECTIONS {
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
    } :data

    /DISCARD/ : {
        *(.eh_frame)
        *(.comment)
        *(.note.GNU-stack)
    }
}
