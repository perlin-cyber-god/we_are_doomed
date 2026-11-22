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
    bgeu t0,t1,2f
    sw zero,0(t0)
    addi t0,t0,4
    j 1b
2:
    call square_main
    li a7,5
    ecall
3:
    j 3b

    .section .text,"ax",@progbits
    .globl platform_call
platform_call:
    # C arguments a0=service, a1=argument -> demo ABI a7=service, a0=argument.
    mv a7,a0
    mv a0,a1
    ecall
    ret
