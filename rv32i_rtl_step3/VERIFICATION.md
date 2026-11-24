# Verification — Step 3, 24 November 2026

## Actual results

| Check executed here | Observed result |
|---|---|
| Preserved Step 1 | 18,563 block checks passed |
| Preserved Step 2 | 23,783 decoder cases, 23,783 C legality classifications, 1,252 slice checks passed |
| Self-fetching core demo | Four instructions retire; x6/RAM word=39; EBREAK trap at PC16; finishes226ns |
| Differential core execution | 36 programs × two memory modes =72 runs; 201,438 checks; zero failures |
| Executed/trapped instruction coverage | All40 RV32I base names; ECALL/EBREAK explicitly trap |
| Total core retirements in corpus | 1,034 across both memory modes; 5,874 counted clocks |
| Pending-request reset | Fetch and store canceled; restart issues exactly one successful store |
| Golden C driver UBSan | All36 cases passed with no undefined-behaviour report |
| Native C / HDL compile | GCC14.2.0; Icarus11.0; no compiler diagnostics |
| Shell syntax | bash -n passed for all three runners |
| Prior-project preservation | All48 original files identical under documented path renames |
| Negative controller mutations | Missing JALR low-bit clearing and ignoring memory wait both rejected |

These counts are test observations, not proof of complete ISA conformance,
timing closure, achievable MHz or Doom FPS. Vivado/xvlog/xelab/xsim, synthesis,
formal verification, physical design and GDSII were **not** run here.
No GUI, QEMU or Doom test was launched.

## Actual invocation

```bash
IVERILOG=/workspace/scratch/6bae74b1c81c/toolchains/rtl-step1-tools/local/usr/bin/iverilog \
IVERILOG_BASE=/workspace/scratch/6bae74b1c81c/toolchains/rtl-step1-tools/local/usr/lib/x86_64-linux-gnu/ivl \
VVP=/workspace/scratch/6bae74b1c81c/toolchains/rtl-step1-tools/local/usr/bin/vvp \
./test.sh iverilog
bash -n test.sh test_step2.sh test_step1.sh
```

Native C flags: `-std=c11 -Wall -Wextra -Wpedantic -Werror -Wconversion
-Wshadow -O2`. HDL flags: `-g2012 -Wall`. The auxiliary UBSan run uses
`-O1 -g -fsanitize=undefined -fno-sanitize-recover=all` with the strict C flags.
Exact compiler/simulator commands for each new test are retained in its log.
test.sh fails on a nonzero command exit or missing expected PASS marker.

The same official Debian Icarus package recorded in Step1 was extracted
task-locally, not installed globally or bundled in this ZIP. On your laptop,
use installed Icarus or load your Vivado environment and run the supplied
Vivado route. The latter remains unexecuted in this environment.

## What is compared

scripts/make_core_programs.py generates deterministic literal machine-code
images, with instruction anchors independent of execution. The preserved
reference/rv32i.c runs those bytes to generate expected observations.
scripts/trace_reference.c checks that a rejected C step leaves PC, registers
and every RAM byte unchanged.

At **every RTL retirement**, tb_core compares PC, instruction, next PC,
destination/value, effective memory address, lane masks, shifted store data,
all32 registers and a whole-RAM FNV-1a hash against C. At traps it compares
diagnostics and architectural state. At the end it also compares **every
backing RAM byte exactly**, including the byte outside the 2047-byte logical
mapping. The per-step hash is a compact check, not collision-free proof.
Independent hand-checked anchors cover the demo, loop, branch counters and
nested call result42/restored stack pointer. No result is supplied to the RTL.

Every program runs with zero added wait and with one-to-four added wait
clocks per transaction. The bus checker asserts stable outstanding requests,
aligned addresses, mutually exclusive read/write masks and exactly one
successful memory write per retired store. Traps must remain stopped/quiet.
All benches have watchdogs and runners have process timeouts.

The infinite-loop case compares exactly20 retirements and ends by host budget;
it is expressly not guest completion or HALT. Most ordinary programs end with
an expected EBREAK trap. The test oracle's expected trap code is checked
against the independently specified case manifest.

Coverage includes arithmetic wrapping/signedness, all immediate shift counts,
all register arithmetic names, lanes and extension for every load width,
partial last-word mappings, fault-atomic stores, nested stack calls, JALR
aliasing/bit0 handling, branch outcomes, invalid words, fetch/access alignment
and explicit environment traps. Inherited decoder coverage is broader than
the 36 execution programs. This is not exhaustive random/formal testing.

## Files and evidence

- verification/step3/full-test.log: complete final successful runner output.
- verification/step3/summary.json: per-run counts.
- verification/step3/instruction_coverage.json: dynamic instruction-name counts.
- verification/step3/*-stall*.log: each observed result with actual command.
- verification/step3/tb_core_demo.vcd and core-demo.png: recorded demo waveform.
- verification/step3/negative-tests.txt: rejected temporary controller mutations.
- verification/step3/ubsan.txt: native C sanitizer command/result.
- verification/step3/preserved-step2.sha256: unchanged prior files.

Old logs/documentation under verification/ and docs/ retain their historical
scope. Their statements such as "no full CPU yet" refer to those earlier
block milestones, not the new Step3 core.

## Known limits / next work

The RTL has no privileged environment, interrupts, CSR access, MMU, cache,
pipeline, M extension, device peripherals or Doom ECALL host bridge. RAM in
the benches is simulation infrastructure, not a synthesized memory subsystem.
Byte-level error atomicity depends on the documented memory backend contract.
Reset cannot roll back an externally completed store.

The first exploratory integrated demo exposed a simulator zero-time
scheduling loop. The new core's ALU-input and bus selection were rewritten
as explicit continuous mux equations; the final demo and all regressions
passed. Previous RTL blocks were unchanged. Two temporary controller mutants
under build/ were rejected; normal source was not mutated.

Next validate the supplied route in the user's Vivado, then run a small
compiled guest through RTL and its reference trace before the much larger
Doom runtime. A technology-aware synthesis/timing/memory review is necessary
before any area/performance or GDS readiness claim.
