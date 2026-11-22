OUTPUT_ARCH(riscv)
ENTRY(_start)
PHDRS { text PT_LOAD FLAGS(5); data PT_LOAD FLAGS(6); tls PT_TLS FLAGS(4); }
MEMORY {
 APP (rwx) : ORIGIN = 0, LENGTH = 0x180000
 BOOTINFO (rw) : ORIGIN = 0x1f0000, LENGTH = 0x1000
 HEAP (rw) : ORIGIN = 0x200000, LENGTH = 0x1e00000
 ASSET (r) : ORIGIN = 0x2000000, LENGTH = 0x1f00000
 STACK (rw) : ORIGIN = 0x3f00000, LENGTH = 0x100000
}
SECTIONS {
 .text : { KEEP(*(.text.start)) *(.text .text.*) } > APP :text
 .rodata : { *(.rodata .rodata.*) *(.srodata .srodata.*) } > APP :text
 .data : { *(.data .data.* .sdata .sdata.*) } > APP :data
 __global_pointer$ = ADDR(.data)+0x800;
 .tdata ALIGN(16) : { *(.tdata .tdata.*) } > APP :data :tls
 .tbss ALIGN(16) (NOLOAD) : {
  __tls_base = SIZEOF(.tdata)>0 ? ADDR(.tdata):.;
  __bss_start = .; *(.tbss .tbss.*); __tls_zero_end = .;
 } > APP :data :tls
 /* TLS NOBITS must not overlap ordinary BSS. */
 .bss ALIGN(__tls_zero_end,4) (NOLOAD) : {
  *(.bss .bss.* .sbss .sbss.*) *(COMMON)
  . = ALIGN(4); __bss_end = .;
 } > APP :data
 .info (NOLOAD) : { . += 0x1000; } > BOOTINFO :NONE
 .heap (NOLOAD) : { __heap_start = .; . += 0x1e00000; __heap_end = .; } > HEAP :NONE
 .asset (NOLOAD) : { . += 0x1f00000; } > ASSET :NONE
 .stack (NOLOAD) : { . += 0x100000; __stack_top = .; } > STACK :NONE
 /DISCARD/ : { *(.comment .note.* .eh_frame*) }
 ASSERT(__bss_end<=0x180000,"engine exceeds reserved app memory")
}
