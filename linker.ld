/* Tell the linker we want an x86_64 ELF file */
OUTPUT_FORMAT("elf64-x86-64")
OUTPUT_ARCH(i386:x86-64)

ENTRY(_start)

PHDRS
{
    text PT_LOAD FLAGS(5);          /* Read + Execute */
    rodata PT_LOAD FLAGS(4);        /* Read Only */
    data PT_LOAD FLAGS(6);          /* Read + Write */
}

SECTIONS
{
    /* Map the kernel to the higher half (-2GB region) */
    . = 0xffffffff80000000;

    .text : {
        *(.text .text.*)
    } :text

    /* Move to the next 4KiB page for page alignment protection */
    . = ALIGN(4096);

    .rodata : {
        *(.rodata .rodata.*)
    } :rodata

    . = ALIGN(4096);

    .data : {
        *(.data .data.*)
    } :data

    .bss : {
        *(.bss .bss.*)
        *(COMMON)
    } :data
}
