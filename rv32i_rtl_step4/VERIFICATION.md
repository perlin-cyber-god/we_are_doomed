# Step 4 verification — 24 November 2026

## Scope and actual results

Your supplied terminal log establishes that the port-fixed Step3 core passed
all72 differential runs/201,438 checks in Vivado2025.2, including reset and
the self-fetching demo. That is the baseline for this milestone. All206
files from that ZIP are preserved unchanged, with old root documentation/
test.sh explicitly moved to historical names. All six RTL files, including
the core, are byte-identical. The C interpreter is also unchanged.

| New check run here | Actual result |
|---|---|
| Cross compilation | RISC-V GCC14.2.0, RV32I/ILP32, -O0 and -O2 |
| Executable-word scan | O0:150 words; O2:116; no-BSS mutation:146; no M/RVC/unsupported code words |
| Compiled guest through RTL | Nine differential runs passed: four accepted normal cases, five deliberate rejections |
| New differential assertions | 347,876 checks, zero failures |
| Normal O0 | 566 retirements, checksum90, nested_calls6, BSS_before0 |
| Normal O2 | 371 retirements, identical accepted signature |
| Changed initialized data | Actual RTL checksum102 after7→9; normal-input protocol rejects; input9 oracle accepts |
| Missing BSS clear | Actual RTL guest FAIL, BSS_before=A5A5A5A5 |
| Too-small execution budget | Checked565-retirement prefix; rejected as incomplete |
| Native reference sanitizer | Five traces under UBSan, identical to normal reference traces |
| Full ./test.sh | All preserved block/72-case tests plus nine new runs passed in Icarus |
| Compiler diagnostics | Clear native C, cross C/assembly/link and Icarus compile logs |
| Shell syntax | All five entry-point/retained runners passed bash -n |

Local tools: Icarus11.0 and GCC14.2.0 (native and cross). RISC-V GCC/binutils
were already available in the task-local extracted toolchain, although not
on the default PATH. No installation or download was needed for this step.
Vivado is unavailable here: Step4's new Vivado route has not been executed
locally and needs your laptop run. Step3's Vivado pass is user-provided evidence.

## Actual full invocation

```bash
RISCV_PREFIX=/workspace/scratch/6bae74b1c81c/toolchains/rv32i-local/usr/bin/riscv64-unknown-elf- \
IVERILOG=/workspace/scratch/6bae74b1c81c/toolchains/rtl-step1-tools/local/usr/bin/iverilog \
IVERILOG_BASE=/workspace/scratch/6bae74b1c81c/toolchains/rtl-step1-tools/local/usr/lib/x86_64-linux-gnu/ivl \
VVP=/workspace/scratch/6bae74b1c81c/toolchains/rtl-step1-tools/local/usr/bin/vvp \
./test.sh iverilog
```

Native checker flags: C11, -Wall -Wextra -Wpedantic -Werror -Wconversion
-Wshadow -O2. Cross guest additionally uses freestanding RV32I/ILP32 flags
recorded in scripts/test_guest.py and verification/step4/O0/compile-c.log.
Icarus uses -g2012 -Wall. UBSan adds -O1 -g -fsanitize=undefined and
-fno-sanitize-recover=all to strict native flags. Every process has a timeout.

The first exploratory linker script contained a malformed += token and the
link failed before guest execution. It was corrected; the complete final
run above exited0 with clean compilation/link logs. No core RTL workaround
was introduced for the compiled program.

## What execution proves

The guest binary is genuinely produced by the cross compiler from guest.c
and start.S. Its ELF, map, flat binary and mixed source/disassembly are
included for inspection. The exact bytes are loaded into simulated RAM;
rv32_core fetches/decodes/executes them. Native GCC only builds the C reference
checker. Neither guest source nor its answers run as native host guest code.

The differential bench checks retirement events/all registers, store/read
addresses and lanes, final RAM bytes and explicit trap diagnostics. The
signature/array/guard checks inspect the RTL's actual RAM dump. Function
entry PCs demonstrate guest_main→outer→inner six times; the trace includes
stack writes and a restored stack pointer. The poisoned BSS test establishes
that startup initialization matters, and changed data establishes dependence
on real loaded input. Budget-prefix success is not application completion.

The new trace's per-step hash slot is intentionally a zero format marker,
not a whole-RAM hash. Final RAM is still compared byte-exact; per-step memory
events and all32 registers are checked. This is a practical regression, not
formal proof, official ISA certification or exhaustive toolchain validation.

## Evidence

- verification/step4/full-test.log: final complete run and markers.
- verification/step4/summary.json: all nine runs with counters/signatures.
- verification/step4/*-stall*.log: actual RTL commands/results.
- verification/step4/O0 and O2: compile/link/disassembly/ELF logs and symbols.
- verification/step4/ubsan.txt: sanitizer command/result.
- verification/step4/preserved-step3-portfix.sha256: all206 original files.
- guest/prebuilt/O0 and O2: ELF/binary/map/disassembly/metadata and exact input
  images/reference traces from the verified builds.

The prebuilt reference.trace files are expected observations, not RTL results.
The runner always cross-builds fresh source and produces fresh RTL dumps.
Task-local toolchains/native executables are excluded from the source archive.

## Remaining work

Step4's Vivado run is pending. No Doom, display/keyboard bridge, M extension,
cache, pipeline, peripherals, GUI smoke test, synthesis, timing closure,
physical layout or GDSII was run/added. These clock counts do not establish
MHz or Doom FPS. The enlarged RAM is simulation infrastructure, not a new
silicon SRAM implementation. See GUEST.md for exact loading/budget semantics.
