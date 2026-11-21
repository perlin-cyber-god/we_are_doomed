# Observed verification — 21 November 2026

Working directory: `/workspace/scratch/6bae74b1c81c/output/rv32i_milestone`.
Compiler: `gcc (Debian 14.2.0-19) 14.2.0`. Clang and a suitable installed
RISC-V cross toolchain were not found.

## Commands actually executed

```bash
# First: unchanged uploaded milestones, rebuilt and run separately.
gcc -std=c11 -Wall -Wextra -Wpedantic -Werror -O2 -o build/cpu_step1 cpu_step1.c
./build/cpu_step1
# Same flags and execution for cpu_step2.c and cpu_step3.c: all exit 0.

# Before implementing the rest of RV32I:
gcc -std=c11 -Wall -Wextra -Wpedantic -Werror -Wconversion -Wshadow \
  -O2 -o build/cpu_step4 cpu_step4.c
./build/cpu_step4

# Complete build and regression entry point:
./test.sh

# Additional AddressSanitizer / UndefinedBehaviorSanitizer check:
ASAN_OPTIONS=detect_leaks=0 SANITIZE=1 ./test.sh
```

Both complete test runs exit 0. All builds above emit no compiler warnings.
The entry point prints compiler/flags and retains each old milestone's full
output in `build/stepN.log`. New files additionally use `-Wconversion -Wshadow`.

| Check | Observed result |
|---|---|
| Uploaded steps 1–3 | All original checks pass, exit 0 |
| Nested-call milestone, tested first | 12 instructions; a0=6, t0=6, sp=256, ra=12, PC=16; edges pass |
| New RV32I tests | 12,711 assertions, 0 failures |
| RAM completion/budget tests | Pass, including exact-budget success and infinite-loop failure |
| ASan + UBSan | All available tests pass; no diagnostics |
| Original source/README hashes | Unchanged before and after tests |
| Cross-compiled C guest | SKIPPED: toolchain unavailable; no claimed run |
| Shell script syntax (`bash -n`) | Pass |

The first sanitizer attempt enabled LeakSanitizer implicitly; that runtime
refused to run under this environment's ptrace wrapper. Repeating with
`ASAN_OPTIONS=detect_leaks=0` completed ASan and UBSan checks. Leak checking
was not performed. The runner fixture initially exposed an incorrect
hand-encoded LUI constant; that test encoding was corrected before the
successful full runs. Neither issue is hidden as a passing first attempt.

## Remaining verification

Run `./test.sh` with an installed RV32-capable RISC-V compiler. The supplied
startup, linker script and C guest have not been cross-built, linked,
disassembled, or executed here. Any build/runtime issues discovered there
remain to be resolved; expected signature values are not observed results.

The tests are project-local tests. No official architectural test suite,
external emulator differential test, RTL simulation or hardware timing
validation was run. The complete base integer interpreter and the preserved
learning milestones are working; a real compiled guest remains the next
check. README contains the exact build and loading procedure.
