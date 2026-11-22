/* RV32I only. All pseudo-instructions expand to base integer instructions. */
    .option norvc
    .option norelax
    .section .text.start,"ax",@progbits
    .globl _start
_start:
    la sp, __stack_top
    la gp, __global_pointer$
    la t0, __bss_start
    la t1, __bss_end
1:
    bgeu t0, t1, 2f
    sw zero, 0(t0)
    addi t0, t0, 4
    j 1b
2:
    call guest_main
    bne a0, zero, 3f
    li t1, 0x444f4e45           /* DONE */
    j 4f
3:
    li t1, 0x4641494c           /* FAIL */
4:
    li t0, 0xc000              /* reserved signature RAM */
    fence rw, rw
    sw t1, 0(t0)               /* publish only after payload */
5:
    j 5b                      /* host observes completion before next step */
