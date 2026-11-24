    .option norvc
    .option norelax
    .section .text.start,"ax",@progbits
    .globl _start
_start:
    la sp, __stack_top
    la gp, __global_pointer$
    la t0, __bss_start
    la t1, __bss_end
#ifndef SKIP_BSS_CLEAR
1:
    bgeu t0,t1,2f
    sw zero,0(t0)
    addi t0,t0,4
    j 1b
#endif
2:
    call guest_main
    bne a0,zero,3f
    li t1,0x444f4e45       /* DONE */
    j 4f
3:
    li t1,0x4641494c       /* FAIL */
4:
    la t0,__signature_start
    fence rw,rw
    sw t1,0(t0)           /* Publish status AFTER payload stores. */
    .globl _guest_stop
_guest_stop:
    ebreak                /* Explicit trap; not a new HALT instruction. */
5:
    j 5b                  /* No implicit successful return if a host resumes. */
