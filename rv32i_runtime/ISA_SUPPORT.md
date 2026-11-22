# RV32I support checklist

All 40 base RV32I operations are implemented in `rv32i.c`. "Tested" below
means this project's self-checking tests, not official architectural
certification. The additional compiled-guest integration is noted below.

| Instructions | Implemented | Tested behaviour (`test_rv32i.c`) |
|---|---|---|
| ADDI, ADD, SUB | Yes | Wraparound, signed boundary, immediate limits, source/destination overlap, x0 |
| SLTI, SLTIU, SLT, SLTU | Yes | Signed/unsigned disagreement, negative immediate sign extension, equality, x0 |
| XORI, ORI, ANDI, XOR, OR, AND | Yes | Bit patterns, negative immediates, x0 |
| SLLI, SRLI, SRAI | Yes | All counts 0..31, negative data, positive arithmetic shifts, illegal high bits, x0 |
| SLL, SRL, SRA | Yes | All counts 0..31, ignored high count bits, sign filling, illegal funct7, x0 |
| LUI, AUIPC | Yes | Upper bits, x0, current PC at 0 and 64, 32-bit wrapping |
| BEQ, BNE, BLT, BGE, BLTU, BGEU | Yes | 25 signed-edge operand pairs per branch, both outcomes, alignment, offset limits |
| JAL | Yes | Forward/backward, offset extremes and golden encodings, link/x0, target alignment/access |
| JALR | Yes | Old rs1 when rd=rs1, negative offset, low-bit clearing, wrap, x0, alignment/access |
| LB, LH, LW, LBU, LHU | Yes | Sign/zero extension, negative offset, endianness, x0 reads/faults, access bounds/alignment |
| SB, SH, SW | Yes | Low bytes, negative offset, sentinels, no register writes, alignment/bounds, no partial stores |
| FENCE | Yes | In-order no-op except PC, all fm values, nonzero ignored fields, PAUSE hint |
| ECALL, EBREAK | Yes: traps | Distinct statuses, unchanged PC/registers/RAM |

Additional tests:

- All non-base OP funct7/funct3 combinations rejected, including M.
- Illegal shift-immediate high bits rejected, even writes to x0.
- Reserved branch/load/store/JALR/MISC-MEM field combinations rejected.
- Every rejected instruction snapshot-compared for PC, 32 registers and RAM.
- Literal-word step-3 byte loop and step-4 nested calls execute on the new core.
- Original tests retained without editing in `cpu_step1.c` through `cpu_step3.c`.
- `test_run.c`: decoded completion store, exact budgets, infinite loop, trap,
  guest-reported failure and invalid signature handling.

## Unsupported behaviour

| Feature | Behaviour / verification |
|---|---|
| M multiply/divide | Illegal instruction; all funct7=1 OP combinations tested |
| Zicsr CSR instructions / counters | Illegal instruction; all six CSR forms tested |
| Zifencei FENCE.I | Illegal instruction; rejection tested |
| Privileged instructions | Illegal instruction; WFI and MRET rejection tested; no privilege modes/CSRs |
| A atomics, F/D floating point, compressed instructions | Illegal instruction; representative encodings tested |
| Other unknown/reserved opcodes | Illegal instruction; zero/all-ones examples tested |
| Unaligned halfword/word accesses | Report load/store misalignment, no invisible emulation; tested |
| ECALL services, guest trap handlers | No services or redirection; host receives explicit status |
| Multiple harts, interrupts, virtual memory | Not modeled |
| Display/input/time | Optional demo execution environment in platform/; core ISA unchanged |
| Cache, pipeline, cycle counts, FPS | Not modeled; retired steps are not hardware cycles |
| Cross-compiled C guest | Verified RV32I/ILP32: 565 retired instructions; arrays, loop, nested calls, .data and .bss |

FENCE is the exception to reserved-field rejection: base ISA requires
conservative fencing for reserved fm/pred/succ configurations and ignoring
rd/rs1. Strong ordering satisfies it without extra work here. This does not
claim a multihart or Ztso implementation.

## Runtime milestone

M instructions remain illegal. The new runtime guest links RV32I/ILP32
compiler helper code for integer multiplication/division; it does not extend
the ISA or intercept arithmetic in the host. Static ELF scans and dynamic
runtime tests verify this path. See runtime/README.md.
