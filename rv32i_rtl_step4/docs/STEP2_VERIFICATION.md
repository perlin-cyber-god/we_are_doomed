# Step 2 verification — 23 November 2026

## Actual results

| Check | Result |
|---|---|
| Preserved Step 1 suite | 18,563 checks, zero failures |
| Decoder controls/immediates | 23,783 instruction cases, zero failures |
| Independent C-model legality cross-check | All 23,783 classifications agree |
| Instruction-driven slice | 36 issued cases; 1,252 checks, zero failures |
| Short decode demo | PASS; x6 receives 40 then 39 from real instruction words |
| Compilation | Icarus 11.0, `-g2012 -Wall`; no diagnostics |
| C reference helper | GCC, C11, -Wall -Wextra -Wpedantic -Werror -Wconversion -Wshadow -O2 |
| Runner shell syntax | `bash -n test.sh` passed |

All new decoder/demonstration runs are bounded, headless and short. No GUI,
Doom, full processor, synthesis, formal proof, place-and-route or GDSII was
run. Vivado executables are not available here; its supplied route remains
unexecuted locally. The user's uploaded screenshot matches the earlier demo
waveform but does not establish that the comprehensive Vivado suite passed.

## Actual invocation

```bash
IVERILOG=/workspace/scratch/6bae74b1c81c/toolchains/rtl-step1-tools/local/usr/bin/iverilog \
IVERILOG_BASE=/workspace/scratch/6bae74b1c81c/toolchains/rtl-step1-tools/local/usr/lib/x86_64-linux-gnu/ivl \
VVP=/workspace/scratch/6bae74b1c81c/toolchains/rtl-step1-tools/local/usr/bin/vvp \
./test.sh iverilog
bash -n test.sh
```

The task-local simulator is the same official Debian package recorded in
docs/STEP1_VERIFICATION.md. It is not bundled in this source ZIP.
Current evidence is under verification/step2; older logs remain historical.

## Corpus construction and failure checks

Golden controls are constructed from named ISA cases and expected constants
are retained from their original numeric inputs. Literal anchors check key
encoders. Coverage includes all signed 12-bit I/S values, all representable
even B offsets, U/J bit walking and range limits, all RV32I names, all shift
amounts, all R funct7/funct3 combinations and shift-immediate upper fields.
Reserved load/store/branch/jump controls, unknown opcodes, M, CSR, FENCE.I,
ECALL/EBREAK and FENCE ignored-field policy are included.

The C core independently checks legal/illegal classification, not the exact
new decoder's controls. It does not execute instructions for the RTL slice.
The instruction-driven slice has separately hand-specified arithmetic
results and register-state checks, including commit disabled, reset, x0,
and recognized-but-unimplemented/illegal words attempting to commit.

Two temporary build-only mutants were rejected: dropping branch immediate
bit 11 failed the decoder corpus; removing executable-class write gating
failed the blocked-load check. Normal source files were never mutated.
Evidence is retained in verification/step2/negative-tests.txt.

Step 1 RTL, benches, vector generator and vectors were compared byte-for-byte
against the previous folder. The C reference was compared unchanged against
the working interpreter. Current hashes are recorded with the evidence.

## Limits

Decode recognition does not equal complete CPU execution. Only arithmetic,
LUI and AUIPC execute in the externally driven slice. Alignment/access
faults, PC storage and sequencing, memory protocols, jumps/branches and
ECALL/EBREAK delivery belong to subsequent milestones. Unknown-valued
simulation inputs are outside the known-input functional contract. This is
not ISA certification or a transistor-level/timing result.

Primary specification checked: official RV32I v2.1,
https://docs.riscv.org/reference/isa/v20260120/unpriv/rv32.html .
