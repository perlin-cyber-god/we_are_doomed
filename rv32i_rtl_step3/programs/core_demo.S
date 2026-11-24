# Explanatory source for the self-contained testbench's literal ROM words.
# Not a bootable application/runtime; EBREAK deliberately traps this core.
# .org offsets are byte addresses; do not execute this file natively.
    .option norvc
    .option norelax
    .section .text
    .org 0
    addi x5,x0,512
    lw   x6,0(x5)
    addi x6,x6,-1
    sw   x6,0(x5)
    ebreak
    .org 512
    .word 40
