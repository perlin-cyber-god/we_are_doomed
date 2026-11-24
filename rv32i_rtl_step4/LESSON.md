# A small C program becomes work for our processor

## 1. What is being calculated?

The guest has initialized_global=7. For each value1..6:

```c
inner(value) = (value << 1) + initialized_global;
outer(value) = inner(value) + 1;
```

The array becomes **10,12,14,16,18,20**. Their sum is **90**.
Both normal compiled builds actually produced this result through RTL.
Each call to inner increments nested_count, so its final value is6.

## 2. Why do we need startup before guest_main?

The CPU resets PC/registers, but it does not know C's rules for memory.
start.S therefore does three jobs before calling guest_main:

| Setup | Actual purpose |
|---|---|
| Set sp=x2 to0x4000 | Give functions space to save local values/return addresses |
| Initialize gp=x3 | Establish the toolchain's global-base convention; small-data accesses are disabled here |
| Clear __bss_start..__bss_end | Make zero_global, values[] and nested_count start at zero |

RAM starts with A5 bytes, not zeros. This makes forgetting the BSS clear
observable. Initialized_global is different: its value7 is present in the
binary's .data bytes, loaded at their linked RAM address. There is no ROM
to RAM .data copy in this all-RAM loader. A later boot-ROM system would need
its own load/run-address policy and startup copying if they differ.

## 3. Follow an actual nested call

The following instructions come from the recorded -O0 disassembly. Compiler
versions/options may choose different registers/addresses; inspect the fresh
build/guest/O0/guest.dis after rebuilding.

| Actual assembly in outer | What the processor does |
|---|---|
| `addi sp,sp,-32` | ALU subtracts32; COMMIT writes the new stack pointer |
| `sw ra,28(sp)` | Calculate sp+28; MEMORY saves outer's caller return address |
| `auipc ra,0` at0xd0 | Put current PC0xd0 in ra=x1 |
| `jalr ra,-104(ra)` at0xd4 | Read old ra; target0xd0−104=0x68 (inner); write new ra0xd8; jump |
| `addi a5,a5,1` | After inner returns, add outer's extra1 |
| `lw ra,28(sp)` | Restore the return address that the nested call would otherwise overwrite |
| `addi sp,sp,32` | Release this function's stack frame |
| `jalr zero,0(ra)` | Set PC to the saved return address; discard a new link because rd=x0 |

**Why saving ra matters:** calling inner replaces ra with the place to
continue inside outer. The stack keeps a separate copy of the place to
continue inside guest_main. Restoring that copy lets outer return correctly.

The register names are aliases: ra=x1, sp=x2, s0=x8, a0=x10, a4=x14,
a5=x15. The hardware sees register numbers, not special C function names.
Arguments/results use a0 by calling convention. Calls are ordinary jumps
with a saved return PC; the stack is ordinary RAM addressed through sp.

## 4. Where are the ALU, loads and loops used?

| C operation | Recorded -O0 assembly example | Processor work |
|---|---|---|
| value doubled | `slli a4,a5,1` at0x94 | Shift bits left1; no multiplier required |
| Read initialized_global | `lw a5,600(a5)` at0x9c, with base0 | Read the .data word7 into a register |
| Add the global | `add a5,a4,a5` at0xa0 | ALU computes doubled value+7 |
| Find values[i] | `slli a5,a5,2`, then `add a5,a4,a5` | Four bytes per element: address=base+4*i |
| Write the element | `sw a3,0(a5)` at0x198 | MEMORY writes the function result to the computed address |
| Repeat the loop | `bgeu a5,a4,0x16c` at0x1d4 | If i<=5, choose the earlier PC; otherwise fall through |

For example, values[2] is at608+2*4=616 in the recorded -O0 image. Its value
is outer(3)=14. The CPU computes both that address and that value through
instructions. It has no built-in understanding of arrays or loops.

## 5. How does the host know the guest finished?

The guest writes its result fields into a reserved signature area at0x2000.
Startup publishes DONE or FAIL **last**, then executes EBREAK. The checker
requires the expected EBREAK location, DONE, correct fields/array and intact
code/guard bytes. Seeing an EBREAK alone does not mean the computation passed.

The host also gives the run a finite instruction-attempt budget and the
bench has a cycle watchdog. An incomplete prefix is not a successful guest.
These limits belong to verification, not to a new guest HALT instruction.

## 6. What did compiler optimization change?

| Recorded normal run | -O0 | -O2 |
|---|---:|---:|
| Executable code words | 150 | 116 |
| Instructions retired | 566 | 371 |
| Clocks, zero-wait memory | 2496 | 1605 |
| Clocks, delayed memory | 4486 | 2828 |
| Checksum | 90 | 90 |

The **same RTL** ran both programs. The compiler reduced the amount of work.
Calls remain real nested calls because both helpers are noinline and sibling
call optimization is disabled. Volatile globals/array remain observable.
This is one small example of software affecting performance; it does not
predict Doom's speed or establish a hardware clock frequency.

Recall in your own words: why does .bss need clearing, why does a nested call
save ra, and why does array indexing shift i by2?
