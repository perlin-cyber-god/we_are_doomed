# Guest loading and completion contract

## Program and memory layout

This milestone uses the unchanged unprivileged, multi-cycle RV32I core.
There is no operating system, privileged trap handler, device I/O or libc.

| Byte range | Reserved use |
|---|---|
| 0000–17ff | Application capacity: code/rodata/data/BSS, linker bounded |
| 1800–1fff | Guard gap |
| 2000–203f | Signature reservation (first24 bytes currently used) |
| 2040–2fff | Guard gap |
| 3000–3fff | 4096-byte descending stack; initial sp=4000 |

The backing RAM is16384 bytes. Each build's ELF symbols give the exact code,
data and BSS boundaries. Linker assertions prevent application/stack overlap
with the reserved gaps/signature. Host-side guards check unexpected final
writes; they are not hardware memory protection.

Compile flags include **-march=rv32i -mabi=ilp32 -mno-relax**, freestanding
compilation, strict warnings and no automatic library/builtin/stack-protector
runtime. The link uses guest/linker.ld, -nostdlib/-nostartfiles and --no-relax.
No M/C/CSR extensions are enabled. Every executable ELF word is independently
checked against the supported base encodings; ELF32/entry0/RVC/float flags
are checked. There is no native execution of guest.c as a shortcut.

Use GNU objcopy **-O binary** on the linked ELF. This avoids the RISC-V GNU
linker's restriction on changing output format during the link. The flat
binary begins at linked address0 and includes initialized .data bytes at
their VMA/LMA. BSS, signature and stack are NOLOAD and absent from that flat
image. Startup clears BSS; there is no separate .data copy in this RAM loader.

The runner fills the full RAM image with A5, copies the flat binary at0,
then writes a byte-oriented readmemh file (one two-digit byte per line).
The simulation loads those bytes; the C reference receives identical bytes.
Guest addresses remain guest RAM offsets, never native pointers.

## Signature protocol

All fields below are32-bit little-endian words at base0x2000.

| Offset | Field | Normal expectation |
|---:|---|---|
| 0 | status | 444f4e45 = DONE; 4641494c = FAIL |
| 4 | checksum | Sum of the six computed array elements |
| 8 | count | 6 |
| 12 | initialized | 7 from loaded .data |
| 16 | BSS_before | 0, observed by the guest before computing |
| 20 | nested_calls | 6 |

guest_main writes payload fields. start.S selects DONE/FAIL from its return
value, executes FENCE, writes status last, then EBREAK at _guest_stop.
FENCE follows our strongly ordered one-hart/no-device memory model. The core
reports EBREAK as explicit project trap9 and does not resume it.

Host acceptance requires: trap9 at the actual ELF _guest_stop address, DONE,
the correct payload and array, restored stack pointer, and intact code/guard
bytes. The host derives independent expected array elements from their input
formula; it never writes answers into the CPU. The signature is read from
the RTL's actual final RAM dump.

## Trace comparison and budgets

scripts/trace_guest.c uses the unchanged reference/rv32i.c to execute the
image. tb/tb_compiled_guest.sv is a separate, expanded differential bench;
the older 2048-byte bench is retained untouched. The new bench checks every
retired PC/word/nextPC, all32 registers, register writeback, memory address,
read/write lane masks and store data. It checks trap diagnostics and final
RAM byte-for-byte. The per-step hash field is a zero format marker here;
unlike the older small-memory bench, it does not hash16KiB every instruction.
Memory stability, single-store completion, fault behaviour and trap quietness
follow the preserved checker's rules. Selected-lane validation is error-atomic.

MAX_STEPS limits **instruction attempts**: each retirement consumes one; the
terminal trap consumes one more. The normal O0 run needs567 attempts (566
retirements+EBREAK); O2 needs372. The runner first uses a10000-attempt budget,
then regenerates an identical trace at the exact successful bound.
The intentionally insufficient565-attempt O0 run stops as BUDGET_PREFIX.
That diagnostic prefix is checked but is rejected as application completion.
The bench separately enforces the limit and a200000-cycle watchdog. Each
external command has a60-second process timeout.

## Cases

| Case | Modes | Expected result |
|---|---|---|
| Normal O0 | zero-wait, delayed | DONE; checksum90; BSS_before0 |
| Normal O2 | zero-wait, delayed | Same signature with fewer instructions |
| Changed .data7→9 | zero-wait, delayed | Actual checksum102; normal-input protocol rejects it; input9 oracle accepts it |
| Startup skips BSS clear | zero-wait, delayed | Actual guest FAIL; poisoned BSS detected |
| Insufficient execution budget | delayed | Checked prefix; rejected as incomplete |

RUN lines are printed before every simulator launch. Expected protocol
rejections count as passing negative tests. Only the final RTL_COMPILED_C
marker indicates the entire new suite completed. Ctrl+C is reported as an
interruption with no whole-suite success claim.

## Build products

build/guest/O0 and O2 contain guest.elf, guest.bin, guest.map, guest.dis,
symbols/ELF metadata and exact command logs. The runner regenerates all files
from source; guest/prebuilt/ contains recorded normal builds for inspection.
build/guest/*.rtl.mem are dumps written by the RTL bench, not C-model outputs.
The source ZIP excludes task-local compilers/simulators and native executables.

Optional alternate cross-tool prefix example:
`RISCV_PREFIX=/path/to/bin/riscv64-unknown-elf- ./run_guest.sh vivado`.
If no cross compiler is found, the runner fails explicitly and claims no guest
success. Install a suitable toolchain or supply RISCV_PREFIX; the prebuilt ELF
is available for study but is not silently substituted for a fresh build.

Primary tool references:
- https://gcc.gnu.org/onlinedocs/gcc/RISC-V-Options.html
- https://sourceware.org/binutils/docs/binutils/objcopy.html

Instruction alignment/trap/FENCE rules remain in SPEC.md. No CPU instruction
set extension, framebuffer, keyboard bridge, Doom RTL run or GDS flow is added.
