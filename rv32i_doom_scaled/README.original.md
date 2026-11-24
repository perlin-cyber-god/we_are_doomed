# Educational RV32 C model

A small, readable C11 model of a 32-bit RISC-V CPU, built one milestone
at a time as a step toward understanding how a processor runs Doom.
Each milestone is its own self-contained C file; earlier files are never
modified.

| File          | Milestone | Instructions        |
|---------------|-----------|---------------------|
| `cpu_step1.c` | 1         | `ADDI`              |
| `cpu_step2.c` | 2         | `ADDI`, `LBU`, `SB` |
| `cpu_step3.c` | 3         | `ADDI`, `LBU`, `SB`, `BNE` |

## Machine (same in every milestone)
- 32 registers (`uint32_t`), x0 always reads 0 and ignores writes
- 32-bit PC
- 256 bytes of little-endian RAM; guest addresses are indexes into that
  array, never native pointers
- Deterministic start: all zero, then the demo program is preloaded

## Build and run
    gcc -std=c11 -Wall -Wextra -Wpedantic -o cpu_step1 cpu_step1.c && ./cpu_step1
    gcc -std=c11 -Wall -Wextra -Wpedantic -o cpu_step2 cpu_step2.c && ./cpu_step2
    gcc -std=c11 -Wall -Wextra -Wpedantic -o cpu_step3 cpu_step3.c && ./cpu_step3

(`clang` works the same way.) Each program exits 0 if every check
passes and 1 otherwise.

---

## Milestone 1: `cpu_step1.c` (ADDI only)

Demo program:

| Addr | Word         | Instruction        |
|------|--------------|--------------------|
| 0    | `0x06400093` | `addi x1, x0, 100` |
| 4    | `0xFFC08113` | `addi x2, x1, -4`  |

RAM byte 100 is set to 5 and must stay unchanged. Final state:
x1=100, x2=96, x0=0, PC=8.

How one step works:
1. **Fetch**: four bytes at PC, little-endian, rebuilt into a word.
   PC must be 4-aligned and the whole word must fit inside RAM.
2. **Validate**: opcode `0x13`, funct3 `0`, otherwise rejected.
3. **Extract**: `rd = bits 11..7`, `rs1 = bits 19..15`,
   `imm = sign-extended bits 31..20`.
4. **Sign extension**: a 12-bit value of 2048..4095 becomes
   value − 4096. No signed right shifts are used.
5. **Compute**: `src + (uint32_t)imm` in unsigned arithmetic, which
   wraps modulo 2^32 and has no undefined behaviour.
6. **Writeback**: skipped when `rd` is x0.
7. **Advance**: PC += 4.

The demo stops after two instructions because `main()` counts to two.
That is a host-side test limit, not a guest HALT instruction.

---

## Milestone 2: `cpu_step2.c` (ADDI, LBU, SB)

Memory access arrives: the CPU can now read and write bytes of RAM.

| Instruction        | Opcode | funct3 | Format | Meaning                              |
|--------------------|--------|--------|--------|--------------------------------------|
| `ADDI rd,rs1,imm`  | `0x13` | 0      | I      | `rd = rs1 + imm`                     |
| `LBU rd,off(rs1)`  | `0x03` | 4      | I      | `rd = zero-extended RAM[rs1+off]`    |
| `SB rs2,off(rs1)`  | `0x23` | 0      | S      | `RAM[rs1+off] = low 8 bits of rs2`   |

Anything else is rejected.

Demo program:

| Addr | Word         | Instruction        |
|------|--------------|--------------------|
| 0    | `0x06400093` | `addi x1, x0, 100` |
| 4    | `0x0000C183` | `lbu x3, 0(x1)`    |
| 8    | `0x0C800113` | `addi x2, x0, 200` |
| 12   | `0x00310023` | `sb x3, 0(x2)`     |

Start data: `RAM[100]=5`, `RAM[200]=0`, `RAM[199]=RAM[201]=0xA5`
(neighbour sentinels that must not change). Final state: x1=100,
x2=200, x3=5, x0=0, PC=16, RAM[100]=5, RAM[200]=5, sentinels intact.

### What changed
- **Two immediate formats.** ADDI and LBU use the I immediate (bits
  31:20). SB uses the S immediate: bits 31:25 are `imm[11:5]` and bits
  11:7 are `imm[4:0]`. In SB, bits 11:7 are part of the offset and are
  **not** a destination register. SB changes no register.
- **Effective address** = `rs1 + (uint32_t)offset`, computed modulo 2^32.
  A negative offset becomes a large unsigned number that wraps back
  down, so -4 works naturally.
- **Safe memory access.** The address must be `< 256` before any RAM
  access. Byte accesses need no alignment (instruction fetches still do).
  Address 255 works; 256 and 0xFFFFFFFF are rejected.
- **LBU zero-extends**: byte 0xF0 becomes 240, never a negative value.
- **SB keeps only the low 8 bits**: storing 0x123456AB writes 0xAB.
- **Loads into x0** still validate and perform the read, then the
  register write is discarded. An invalid address still reports the error.
- **Validate before side effects.** `cpu_step()` first computes and
  checks everything, and only then commits (RAM write, register write,
  PC update). A rejected instruction or access leaves PC, all registers
  and all RAM unchanged.

The demo stops after four instructions via a host-side counter in
`main()`; it is not a guest HALT.

### Tests (all in `cpu_step2.c`)
- Retained from milestone 1: ADDI cannot change x0; rejected opcodes,
  funct3 values, misaligned PC, PC past RAM, PC=0xFFFFFFFC.
- New: LBU 0xF0 → 240; SB stores only 0xAB; offset -4 for LBU and SB;
  SB with nonzero bits 11:7 changes no register; byte 255 works while
  256 and 0xFFFFFFFF are rejected; a load to x0 (valid and invalid).
- Every rejection check also proves PC, registers and RAM are unchanged.

---

## Milestone 3: `cpu_step3.c` (ADDI, LBU, SB, BNE)

The PC can now jump, so the guest can run a loop. `cpu_step1.c` and
`cpu_step2.c` are unchanged; `cpu_step3.c` keeps all their tests.

| Instruction          | Opcode | funct3 | Format | Meaning                                  |
|----------------------|--------|--------|--------|------------------------------------------|
| `BNE rs1,rs2,offset` | `0x63` | 1      | B      | if `rs1 != rs2`: `PC = PC + offset`, else `PC + 4` |

Other branch types (BEQ, BLT, BGE, BLTU, BGEU) are rejected.

Guest program (preloaded as little-endian bytes, then really fetched
and decoded):

| Addr | Word         | Instruction                  |
|------|--------------|------------------------------|
| 0    | `0x06400093` | `addi x1, x0, 100`           |
| 4    | `0x0C800113` | `addi x2, x0, 200`           |
| 8    | `0x00300213` | `addi x4, x0, 3`             |
| 12   | `0x0000C183` | `loop: lbu x3, 0(x1)`        |
| 16   | `0x00310023` | `sb x3, 0(x2)`               |
| 20   | `0x00108093` | `addi x1, x1, 1`             |
| 24   | `0x00110113` | `addi x2, x2, 1`             |
| 28   | `0xFFF20213` | `addi x4, x4, -1`            |
| 32   | `0xFE0216E3` | `bne x4, x0, loop` (offset -20) |
| 36   | (nothing)    | host stops here              |

Start data: `RAM[100..102] = 5, 7, 9`, `RAM[200..202] = 0`,
`RAM[199] = RAM[203] = 0xA5` (sentinels).

Result after 21 completed instructions: x1=103, x2=203, x4=0, x3=9,
x0=0, PC=36; `RAM[200..202] = 5, 7, 9`; source bytes and sentinels
unchanged; exactly 3 stores; branch outcomes taken, taken, not taken.
(3 setup instructions + 3 loop passes of 6 instructions = 21.)

### How revisiting addresses makes the loop
The PC is just a number, and each step fetches the four bytes at that
address. Normally PC grows by 4, so execution walks forward through RAM
once. A *taken* BNE puts an earlier address (12) into the PC, so the CPU
fetches and decodes the bytes at 12, 16, 20, ... **again**. The code
exists only once in RAM; the PC simply returns to it. The loop does
different work each time because the registers changed: x1 and x2 each
moved on one byte and x4 counted down, so the same instruction words act
on different data. When x4 reaches 0, BNE is not taken, the PC falls
through to 36, and the loop ends.

### The B-type offset
The offset is a multiple of 2, so imm[0] is not stored. Its other bits
are scattered across the word and `imm_b()` reassembles them:

| Offset bit | Instruction bit(s) |
|------------|--------------------|
| imm[12]    | 31                 |
| imm[11]    | 7                  |
| imm[10:5]  | 30:25              |
| imm[4:1]   | 11:8               |
| imm[0]     | always 0           |

The 13-bit value is sign-extended without signed shifts (if bit 12 is
set the value is `field - 8192`). For `bne x4, x0, -20`: -20 is
`0x1FEC` in 13 bits, giving `0xFE0216E3`. The tests check this word
by hand-derived constants as well as by round trip.

### BNE rules
- Compares the full 32-bit values of rs1 and rs2.
- Writes no register and no data memory; it only chooses the next PC.
- Taken: `next PC = branch PC + offset` modulo 2^32.
  Not taken: `branch PC + 4`.
- A *taken* branch whose target is not 4-byte aligned is rejected with
  nothing changed. An *untaken* branch is accepted even if its unused
  target would be misaligned.
- The target is not range-checked by the branch itself. It is checked
  when the CPU fetches from it, which rejects a PC outside RAM.

### Host-side stop and budget (not HALT)
The demo stops when PC reaches 36, checked before each fetch, so the
instruction at 36 is never run. A maximum-step budget (100) catches
infinite loops. Running out of budget is reported as **failure**, never
as normal completion, and the program exits 1. Neither is a guest HALT
instruction.

### New tests (all in `cpu_step3.c`)
- B-immediate decoding, including hand-derived words for +2048, +4094
  and -4096, and a round trip across the offset range
- BNE taken and not taken (including a difference in only the top bit)
- Forward and backward targets
- A nonzero-numbered rs2 register (x6), and x0 as rs1
- No register or RAM change from BNE, taken or not
- Taken misaligned target rejected; untaken misaligned target accepted
- Out-of-RAM targets rejected at fetch time
- Unsupported branch types rejected
- Step budget: infinite loop reports failure; the demo needs exactly
  21 steps (budget 20 fails, budget 21 completes)
- Every rejection also proves PC, registers and RAM are unchanged

## Out of scope
Other branches, jumps, other loads/stores, caches, pipelines,
peripherals, QEMU, GUI, Doom.
