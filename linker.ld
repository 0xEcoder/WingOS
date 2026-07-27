/* Tell the linker we want an x86_64 ELF file */
OUTPUT_FORMAT("elf64-x86-64")
OUTPUT_ARCH(i386:x86-64)

ENTRY(_start)

/* Explicitly define PT_LOAD program headers with exact memory permissions */
PHDRS
{
    limine_requests PT_LOAD FLAGS(4); /* Read-Only (PF_R = 4) */
    text            PT_LOAD FLAGS(5); /* Read + Execute (PF_R | PF_X = 5) */
    rodata          PT_LOAD FLAGS(4); /* Read-Only (PF_R = 4) */
    data            PT_LOAD FLAGS(6); /* Read + Write (PF_R | PF_W = 6) */
}

SECTIONS
{
    /* Map kernel to higher half (-2GB region) */
    . = 0xffffffff80000000;

    .limine_requests : {
        KEEP(*(.requests_start_marker))
        KEEP(*(.requests))
        KEEP(*(.requests_end_marker))
    } :limine_requests

    . = ALIGN(0x1000);

    .text : {
        *(.text .text.*)
    } :text

    . = ALIGN(0x1000);

    .rodata : {
        *(.rodata .rodata.*)
    } :rodata

    . = ALIGN(0x1000);

    .data : {
        *(.data .data.*)
        *(.bss .bss.*)
        *(COMMON)
    } :data

    /DISCARD/ : {
        *(.eh_frame*)
        *(.note*)
    }
}