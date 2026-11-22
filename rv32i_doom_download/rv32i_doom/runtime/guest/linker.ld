OUTPUT_ARCH(riscv)
ENTRY(_start)
PHDRS { text PT_LOAD FLAGS(5); data PT_LOAD FLAGS(6); }
MEMORY {
 APP (rwx) : ORIGIN = 0, LENGTH = 0x10000
 REPORT (rw) : ORIGIN = 0x10000, LENGTH = 0x1000
 ASSET (r) : ORIGIN = 0x20000, LENGTH = 0x1000
 HEAP (rw) : ORIGIN = 0x30000, LENGTH = 0x40000
 STACK (rw) : ORIGIN = 0x7f000, LENGTH = 0x1000
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
 .report (NOLOAD) : { . += 0x1000; } > REPORT :NONE
 .asset (NOLOAD) : { . += 0x1000; } > ASSET :NONE
 .heap (NOLOAD) : { . += 0x40000; } > HEAP :NONE
 .stack (NOLOAD) : { . += 0x1000; __stack_top = .; } > STACK :NONE
 /DISCARD/ : { *(.comment .note.* .eh_frame*) }
 ASSERT(__bss_end<=0x10000,"program exceeds APP region")
}
