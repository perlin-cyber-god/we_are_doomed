# Decoder lesson: LW, ADDI and SW

This is an illustrative three-instruction program, not claimed to be the
actual Doom disassembly. It isolates the processor operations behind updating
a variable. All addresses below are guest addresses.

Assume x5/t0 = 0x00201000 and the word at that aligned address contains 40.

| PC | Word | Assembly |
|---|---|---|
| 0x1000 | 0x0002a303 | lw x6,0(x5) |
| 0x1004 | 0xfff30313 | addi x6,x6,-1 |
| 0x1008 | 0x0062a023 | sw x6,0(x5) |

The CPU fetches at PC. The load/store data address comes from x5 plus offset.
Fetching the instruction and accessing its data are separate operations.
The guest's little-endian word 40 is bytes 28 00 00 00 in increasing addresses.

## I-format: load and immediate arithmetic

Bits 31:20 hold imm[11:0]; bits 19:15 select rs1; bits 14:12 are funct3;
bits 11:7 select rd; bits 6:0 are the opcode. These fields have different
interpretations for other instruction formats.

For LW, opcode=0x03, funct3=2, rs1=5, rd=6, immediate=0. Register index 5
selects x5; its CONTENTS are 0x00201000. The ALU computes x5+0. The model
checks four-byte alignment and bounds, reads four bytes, writes 40 to x6 and
sets PC=0x1004. RAM is unchanged.

For ADDI, opcode=0x13, funct3=0, rs1=6, rd=6. The raw 12-bit immediate is
0xfff. Sign-extending gives the 32-bit pattern 0xffffffff, representing -1.
The model uses unsigned addition modulo 2^32: 40+0xffffffff becomes 39.
Read x6 before writing x6. RAM remains 40, and PC becomes 0x1008.

## S-format: store

Bits 31:25 hold imm[11:5], bits 24:20 select rs2, bits 19:15 select rs1,
bits 14:12 are funct3, bits 11:7 hold imm[4:0], and bits 6:0 are opcode.
There is no destination register field for SW.

Here opcode=0x23, funct3=2, rs1=5, rs2=6, offset=0. The ALU computes the
address x5+0. The CPU checks alignment and bounds, then stores x6's value,
39, as little-endian bytes 27 00 00 00. Registers are unchanged and PC
becomes 0x100c.

## How rv_step implements this

rv32i.c reads the word, extracts opcode/funct3/rd and source values, and
sign-extends the immediate. It sets next=pc+4 as a tentative result.
The opcode switch selects a compute path: ADDI uses a+ii; loads set width
and address; stores set width/address/store. Load/store validation occurs
before commit. The common commit code writes RAM for a store, writes a
nonzero destination register when requested and finally updates PC.

Tentative next-PC is not an immediate state update. Rejected instructions
leave architectural state unchanged. The C switch models decoder/control;
the arithmetic models ALU effects; x[] models register state; the RAM array
models memory. These are functional effects, not a pipeline or clock-cycle
schedule. Real optimized code may keep ammo in a register longer, use other
registers or arrange instructions differently.

Recall: after ADDI but before SW, x6 is 39 while the RAM word remains 40.
Next: branches turn a loaded key-state flag into a decision to run firing
logic; calls and the stack connect that decision to the weapon routines.

Reference: official RV32I v2.1,
https://docs.riscv.org/reference/isa/v20260120/unpriv/rv32.html .
