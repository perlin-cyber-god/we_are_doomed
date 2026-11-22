OUTPUT_ARCH(riscv)
ENTRY(_start)
PHDRS { text PT_LOAD FLAGS(5); data PT_LOAD FLAGS(6); }
MEMORY {
    APP (rwx) : ORIGIN = 0, LENGTH = 0x8000
    FRAME (rw) : ORIGIN = 0x10000, LENGTH = 64000
    STACK (rw) : ORIGIN = 0x3f000, LENGTH = 0x1000
}
SECTIONS {
    .text : { KEEP(*(.text.start)) *(.text .text.*) } > APP :text
    .rodata : { *(.rodata .rodata.*) } > APP :text
    .data : { *(.data .data.* .sdata .sdata.*) } > APP :data
    __global_pointer$ = ADDR(.data)+0x800;
    .bss (NOLOAD) : {
        . = ALIGN(4); __bss_start = .;
        *(.bss .bss.* .sbss .sbss.*) *(COMMON)
        . = ALIGN(4); __bss_end = .;
    } > APP :data
    .frame (NOLOAD) : { . += 64000; } > FRAME :NONE
    .stack (NOLOAD) : { . += 0x1000; __stack_top = .; } > STACK :NONE
    /DISCARD/ : { *(.comment .note.* .eh_frame*) }
    ASSERT(__bss_end<=0x8000,"program overlaps reserved gap")
}
