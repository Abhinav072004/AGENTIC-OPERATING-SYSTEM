OUTPUT_FORMAT(elf64-x86-64)
ENTRY(_start)

PHDRS {
    requests PT_LOAD FLAGS(6); /* Bootloader writes responses: R/W, non-executable. */
    text     PT_LOAD FLAGS(5);
    rodata   PT_LOAD FLAGS(4);
    data     PT_LOAD FLAGS(6);
    stack    PT_GNU_STACK FLAGS(6);
}

SECTIONS {
    . = 0xffffffff80000000;
    __kernel_start = .;
    .limine_requests : {
        KEEP(*(.limine_requests_start))
        KEEP(*(.limine_requests))
        KEEP(*(.limine_requests_end))
    } :requests

    . = ALIGN(0x1000);
    __text_start = .;
    .text : { *(.text .text.*) } :text
    . = ALIGN(0x1000);
    __rodata_start = .;
    .rodata : { *(.rodata .rodata.*) } :rodata
    . = ALIGN(0x1000);
    __data_start = .;
    .data : { *(.data .data.*) } :data
    .bss : { *(COMMON) *(.bss .bss.*) } :data
    __kernel_end = ALIGN(0x1000);

    /DISCARD/ : { *(.eh_frame*) *(.note .note.*) }
}
