# Core contract — functional RV32I, Step 3

Primary source: [official RV32I v2.1](https://docs.riscv.org/reference/isa/v20260120/unpriv/rv32.html).
The preserved C model in reference/ is the execution oracle for this milestone.
No official architectural-test suite or formal proof has been run.

## State and controller

32-bit PC and x1–x31; x0 always zero with no backing storage. Clocked state
uses the rising edge. Active-high synchronous reset clears registers and
temporary/trace/trap state, sets PC to RESET_PC (default 0), and selects FETCH.
While reset is asserted, requests and register writes are suppressed.
RAM belongs to the environment and is not reset by the core.

New rtl/rv32_core.sv reuses the unchanged ALU, register file, decoder and
immediate generator. The old decode_slice remains a separate teaching demo.

- FETCH reuses the ALU for PC+4. On successful fetch, IR and sequential_pc
  capture the word and PC+4.
- DECODE captures A/B from the two read ports. ALU comparator outputs form
  the branch decision; this decision is retained for EXECUTE.
- EXECUTE reuses the ALU for arithmetic, address or target calculation.
  Address/target validation precedes the corresponding external effect.
- MEMORY retains address, source data and lane masks while waiting.
  Successful loads capture the extended result. Stores have no destination.
- COMMIT writes an allowed nonzero destination, updates PC and emits one
  retirement pulse. No next instruction overlaps this one.
- TRAP is sticky until reset. No trap handler, resume operation or CSR file.

Internal A/B/result/address/next-PC values are temporary, not new ISA registers.
The 10 ns testbench clock is stimulus, not a hardware frequency claim.

## Memory interface

One shared instruction/data port; one transaction at a time. Address space
is 32-bit byte-addressed, little-endian. The core does not contain RAM.

| Port | Meaning |
|---|---|
| mem_valid_o | A request is active |
| mem_instruction_o | This request is instruction fetch |
| mem_address_o | Four-byte-aligned bus address |
| mem_read_mask_o[3:0] | Read lanes; fetch uses 1111 |
| mem_write_mask_o[3:0] | Write lanes; zero except store |
| mem_write_data_o | Store data shifted into its aligned lanes |
| mem_done_i | Access **completed** at this edge; not just accepted into a queue |
| mem_error_i | Completion failed; meaningful with valid and done |
| mem_read_data_i | Aligned read word; selected lanes valid on successful completion |

Lane 0 is address+0/bits7:0. For LB at byte address 515, bus address=512,
read mask=1000; bits31:24 are selected and extended. SH at 514 uses mask1100.
Only selected lanes need exist: a valid final byte can be read/written even
when the rest of the aligned word lies beyond RAM.

**Environment obligations:** request signals must be sampled with valid.
The core holds them stable until completion. Apply a successful store
exactly once on the edge with valid/done, and apply **no store bytes at all**
on error. Validate all selected lanes before writing any. Do not interpret
every waiting clock as another store. A queue-accepting bus needs an adapter.

A successful store takes external effect in MEMORY, then retires in COMMIT.
There are no later fault checks for that instruction. Reset cancels an
uncompleted request only; it cannot undo an already completed external store.
External RAM must suppress completion/writes while reset is asserted.

The supplied simulation memory is 2048 backing bytes with a configurable
logical limit, including a 2047-byte fault case. The synthesizable core
itself has no fixed RAM capacity. Testbench RAM and file I/O are not ASIC RAM.

## Alignment, faults and unsupported behaviour

Base instructions require PC multiple of four (no compressed extension).
Halfword/word data addresses require multiples of two/four; byte addresses
are unrestricted. Misaligned accesses trap rather than being split.

| trap_code_o | Meaning |
|---:|---|
| 1 | Instruction/branch/jump target misalignment |
| 2 | Instruction fetch access error |
| 3 | Illegal/unsupported instruction |
| 4 / 5 | Load misalignment / access error |
| 6 / 7 | Store misalignment / access error |
| 8 / 9 | Explicit ECALL / EBREAK |

These **project status numbers match the C enum; they are not RISC-V mcause
encodings**. There is no privileged exception controller in this milestone.
trap_pc is the current instruction PC; trap_instruction is zero when fetch
was impossible. trap_address is the effective/failing target/fetch address
for address faults, zero for illegal instructions/ECALL/EBREAK.

Taken misaligned branch/jump targets fault before link-register write.
Untaken branches ignore target misalignment. An aligned unmapped jump
commits its link/PC normally; the following fetch faults. JALR reads the old
source even if rd==rs1, then clears target bit 0 before checking alignment.

Faulting instructions do not retire, advance PC, write registers or change
RAM (subject to the error-atomic memory contract). Internal temporary
registers and diagnostic outputs can change. Trap state issues no requests.
Loads to x0 still perform and validate the memory access.

Arithmetic wraps modulo 2^32 with no overflow trap. Signed comparisons use
two's-complement interpretation; right shift distinguishes logical/arithmetic.
Register shift counts use only low five bits. I/S/B/J immediates are signed;
U immediates supply bits31:12 and zeros below.

FENCE retires as a no-op because this single-hart, no-device environment
already completes accesses strictly in order; no outstanding store buffer
exists. Base FENCE ignored/reserved field policy matches the C reference.
FENCE.I is unsupported. Revisit ordering when adding caches/devices/harts.
ECALL is an explicit stopping trap, **not yet the Doom interpreter's host I/O
ABI**. No privileged traps, interrupts, CSRs, MRET, WFI, M/A/C/F/D/V extensions.
Unsupported and reserved opcodes/fields follow our illegal-instruction policy.
Unknown/X-valued simulation inputs are outside this known-input contract.

## Retirement trace

retire_valid_o pulses for each successful instruction, after the commit edge.
retire_pc/instruction/next_pc describe that instruction. retire_rd and
write_data are zero when no actual register write occurs (including rd=x0).
Memory address is the **effective byte address**, while masks/data use aligned
bus lanes. Memory trace fields are zero for nonmemory instructions; store
data is zero on loads. This teaching interface is **not an RVFI claim**.

## Instruction checklist

All rows are implemented and exercised by the differential instruction corpus.
Decoder-only malformed-encoding coverage is broader than core execution coverage.

| Family | Instructions | Execution check |
|---|---|---|
| Register arithmetic/logic | ADD SUB AND OR XOR SLL SRL SRA SLT SLTU | Directed and deterministic random programs |
| Immediate arithmetic/logic | ADDI ANDI ORI XORI SLTI SLTIU SLLI SRLI SRAI | Signed boundaries; every immediate shift count |
| Upper immediates | LUI AUIPC | Nonzero upper bits, nonzero PC |
| Branches | BEQ BNE BLT BGE BLTU BGEU | Each taken/untaken; backward loop; alignment |
| Jumps | JAL JALR | Nested calls/stack; link/source alias; bit0 clearing |
| Loads | LB LBU LH LHU LW | Lanes, signed/unsigned, x0, bounds, alignment |
| Stores | SB SH SW | Lanes, neighbour bytes, delayed completion, atomic errors |
| Ordering | FENCE | Normal and ignored-field encodings; in-order no-op |
| Environment | ECALL EBREAK | Distinct explicit traps, no retirement |
| Unsupported | M, CSR, FENCE.I, other extensions/reserved words | Selected execution rejections plus inherited decoder corpus |

No cache/pipeline, multiplier, peripherals, display or Doom execution in RTL.
