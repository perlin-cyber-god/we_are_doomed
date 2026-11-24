OUTPUT_ARCH(riscv)
ENTRY(_start)
PHDRS {
    text PT_LOAD FLAGS(5);
    data PT_LOAD FLAGS(6);
}
MEMORY {
    APP (rwx) : ORIGIN = 0, LENGTH = 0x1800
    SIGNATURE (rw) : ORIGIN = 0x2000, LENGTH = 0x40
    STACK (rw) : ORIGIN = 0x3000, LENGTH = 0x1000
}
SECTIONS {
    .text : { KEEP(*(.text.start)) *(.text .text.*) } > APP :text
    .rodata : { *(.rodata .rodata.*) } > APP :text
    .data : {
        .  =  ALIGN(4);
        __data_start = .;
        *(.sdata .sdata.* .data .data.*)
        __data_end = .;
    } > APP :data
    __global_pointer$ = __data_start+0x800;
    .bss (NOLOAD) : {
        .  =  ALIGN(4);
        __bss_start = .;
        *(.sbss .sbss.* .bss .bss.*) *(COMMON)
        .  =  ALIGN(4);
        __bss_end = .;
    } > APP :data
    .signature (NOLOAD) : {
        __signature_start = .;
        . += 0x40;
    } > SIGNATURE :NONE
    .stack (NOLOAD) : {
        __stack_bottom = .;
        . += 0x1000;
        __stack_top = .;
    } > STACK :NONE
    /DISCARD/ : { *(.comment .note.* .eh_frame .eh_frame.*) }
    ASSERT(__bss_end <= 0x1800,"application overlaps guard")
    ASSERT(__stack_top == 0x4000,"stack placement wrong")
}
