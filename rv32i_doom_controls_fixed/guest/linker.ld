OUTPUT_ARCH(riscv)
ENTRY(_start)
PHDRS {
    text PT_LOAD FLAGS(5);
    data PT_LOAD FLAGS(6);
}
MEMORY {
    APP (rwx) : ORIGIN = 0x0000, LENGTH = 0xc000
    SIGNATURE (rw) : ORIGIN = 0xc000, LENGTH = 0x40
    STACK (rw) : ORIGIN = 0xf000, LENGTH = 0x1000
}
SECTIONS {
    .text : { KEEP(*(.text.start)) *(.text .text.*) } > APP :text
    .rodata : { *(.rodata .rodata.*) } > APP :text
    .data : {
        . = ALIGN(4);
        __data_start = .;
        *(.sdata .sdata.* .data .data.*)
        __data_end = .;
    } > APP :data
    __global_pointer$ = __data_start + 0x800;
    .bss (NOLOAD) : {
        . = ALIGN(4);
        __bss_start = .;
        *(.sbss .sbss.* .bss .bss.*) *(COMMON)
        . = ALIGN(4);
        __bss_end = .;
    } > APP :data
    .signature (NOLOAD) : { . += 0x40; } > SIGNATURE :NONE
    .stack (NOLOAD) : {
        __stack_bottom = .;
        . += 0x1000;
        __stack_top = .;
    } > STACK :NONE
    /DISCARD/ : { *(.comment .note.* .eh_frame .eh_frame.*) }
    ASSERT(__bss_end <= ORIGIN(APP)+LENGTH(APP), "program overlaps signature")
    ASSERT(__stack_top == 0x10000, "unexpected stack address")
}
