    .option norvc
    .option norelax
    .section .text.start,"ax",@progbits
    .globl _start
_start:
    la sp,__stack_top
    la gp,__global_pointer$
    la tp,__tls_base
    la t0,__bss_start
    la t1,__bss_end
1:
    bgeu t0,t1,2f
    sw zero,0(t0)
    addi t0,t0,4
    j 1b
2:
    call port_main
    li a7,5
    ecall
3:
    j 3b
    .section .text,"ax",@progbits
    .globl dg_call
dg_call:
    mv a7,a0
    mv a0,a1
    mv a1,a2
    mv a2,a3
    ecall
    ret
