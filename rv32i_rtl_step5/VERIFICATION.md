# Step 5 verification — 25 November 2026

## Actual results

| Check executed here | Observed result |
|---|---|
| CPU/MMIO integration | 18 runs passed |
| Independent architectural replay | 483,693 numeric fields matched, including every register and final RAM |
| Device-only unit benches | wait0: 64 checks; wait3: 105 checks; all passed |
| Console output | All four normal O0/O2 runs emitted the exact 26-byte greeting, sum2023 |
| Backpressure | No missing/duplicate bytes; request held until completion |
| Timer | Per-clock increments, completion-time reads, wraparound and reset checked |
| Invalid access policy | Wrong widths/directions, unknown/high addresses, MMIO fetch and alignment rejected correctly |
| x0 loads | Valid timer read still occurred; illegal console read still faulted |
| Reset during waiting console write | Pending unaccepted byte canceled; restart output correct |
| Budget | 20-step prefix checked and not accepted as application completion |
| Negative timer mutation | Increment-by-two mutant rejected by independent clock model |
| C replay sanitizer | All18 cases passed UBSan and produced identical reference traces |
| Whole project | Full ./test.sh iverilog passed, including inherited Step3/Step4 suites |
| Previous work | All279 original Step4 files unchanged under documented root file renames |
| Compilation | Strict native/cross C and Icarus logs clear; all new executable words scan as base RV32I |

The previous six RTL files and reference/rv32i.c remain byte-identical. New
hardware is rtl/rv32_mmio.sv; the old CPU is not patched to recognize PRINT
or TIMER instructions. Console/timer are selected by the bus address map.

## Recorded ordinary workloads

| Compiled guest / waiting mode | Retirements | Counted cycles | Console bytes | Polling elapsed ticks |
|---|---:|---:|---:|---:|
| O0 / no added waits | 1325 | 5953 | 26 | 66 |
| O0 / delayed | 1361 | 11257 | 26 | 119 |
| O2 / no added waits | 843 | 3703 | 26 | 122 |
| O2 / delayed | 904 | 7138 | 26 | 109 |

Different wait/readiness schedules cause different polling-loop iteration
counts and timer values. These are measured simulation observations, not
fixed golden constants, hardware MHz or Doom FPS claims.

## Actual invocation

```bash
RISCV_PREFIX=/workspace/scratch/6bae74b1c81c/toolchains/rv32i-local/usr/bin/riscv64-unknown-elf- \
IVERILOG=/workspace/scratch/6bae74b1c81c/toolchains/rtl-step1-tools/local/usr/bin/iverilog \
IVERILOG_BASE=/workspace/scratch/6bae74b1c81c/toolchains/rtl-step1-tools/local/usr/lib/x86_64-linux-gnu/ivl \
VVP=/workspace/scratch/6bae74b1c81c/toolchains/rtl-step1-tools/local/usr/bin/vvp \
./test.sh iverilog
```

This full command exited0. The focused ./run_io.sh iverilog also passed.
Tools were the already available task-local Icarus11.0 and RISC-V GCC14.2.0,
plus native GCC14.2.0. No new toolchain download/install was required.

Native flags: -std=c11 -Wall -Wextra -Wpedantic -Werror -Wconversion -Wshadow
-O2. Cross flags retain Step4's freestanding RV32I/ILP32 settings at -O0/-O2.
Icarus flags: -g2012 -Wall. UBSan adds -O1 -g -fsanitize=undefined and
-fno-sanitize-recover=all to strict native flags. All seven shell runners
passed bash -n. Exact build/simulation commands are retained in each log.

The user's actual logs establish completed Step3 and Step4 Vivado2025.2
passes. Vivado is unavailable in this build environment, so the **new Step5
Vivado route remains pending on the user's laptop**. No GUI test was run.

## What the oracle does and does not prove

Actual RTL executes first and records retirement states, every completed
non-RAM transaction, final RAM and the console byte stream. The original C
interpreter then executes the same image with a separate MMIO adapter. This
adapter checks request direction/width/address/order/data/access-error policy
and supplies recorded timer/status inputs to legal loads. All ordinary
instruction semantics remain in the unchanged reference CPU.

Python compares every numeric trace field and every final RAM byte. The
normal application must have DONE at the expected EBREAK site, matching
signature/console/byte globals, restored stack and intact code/guards. Fault
programs must have the expected trap with no console output or RAM change.

Replaying device inputs alone would not prove they were generated correctly.
Independent clock models therefore check the timer on every edge and at
completion, and the direct unit bench tests wrap/reset/backpressure/bad-mask
behaviour without relying on any C replay. A temporary timer-by-two mutant
failed that unit model. The normal source was not mutated. This is practical
simulation coverage, not formal proof or official ISA certification.

The original reference has no timing model or device bus, so its MMIO adapter
is new platform code and requires review like the RTL. Final byte-exact RAM
comparison plus retirement/bus-event checks are used; no per-step full-RAM
hash is computed in these larger benches. External stream inputs can change
between clocks; only a valid completion/handshake edge is significant.

## Evidence included

- verification/step5/full-test.log and summary.json: complete final results.
- verification/step5/*-wait*.log: actual CPU runs and reference replays.
- verification/step5/tb_mmio_unit*.log: direct device results.
- verification/step5/O0 and O2: cross build, symbol and disassembly logs.
- verification/step5/negative-timer.txt: independent rejection of bad timer.
- verification/step5/ubsan.txt: sanitized replay command/result.
- verification/step5/preserved-step4.sha256: preserved original files.
- io_guest/prebuilt/O0 and O2: ELF/binary/map/disassembly, image, actual and
  expected delayed-run traces, completed device events and console output.

Prebuilt .actual.trace/.console files are recorded RTL outputs; the runner
still rebuilds fresh source and regenerates them. Task-local compilers and
native executables are excluded from the downloadable source archive.

## Limits

This console is a synchronous byte-stream interface with a testbench file
sink, not a serial UART. The timer measures simulation clock ticks, not host
wall time. There is no keyboard, framebuffer, interrupt controller, clock
crossing, hardware bus timeout, cache, pipeline or M extension. RAM backend
arrays and file I/O are simulation infrastructure, not synthesized SRAM.
No Doom-on-RTL, synthesis, timing closure, physical design, DRC/LVS or GDSII
was performed. Reset cannot undo an already accepted external byte.
