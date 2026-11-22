# Compiled C guest verification — 21 November 2026

This is a new project copy; the earlier rv32i_milestone ZIP and sources are preserved.
The instruction interpreter itself is unchanged. The missing cross-toolchain
check has now been completed with a task-local GCC 14.2.0 and binutils 2.44.
Downloaded Debian package hashes were verified before unpacking; see
verification/toolchain-provenance.txt. No system packages were installed.

## Actual commands and results

Run from /workspace/scratch/6bae74b1c81c:

```bash
PATH="$PWD/toolchains/rv32i-local/usr/bin:$PATH" ./output/rv32i_compiled_guest/test.sh
ASAN_OPTIONS=detect_leaks=0 SANITIZE=1 PATH="$PWD/toolchains/rv32i-local/usr/bin:$PATH" ./output/rv32i_compiled_guest/test.sh
# Without adding the compiler to PATH: verify the bundled binary fallback.
./output/rv32i_compiled_guest/test.sh
```

All three routes exit 0. Strict host/compiler warnings are enabled, including
-Werror; successful cross-builds have no compiler or linker warnings. ASan and
UBSan pass; leak detection is disabled because of this environment's ptrace
wrapper. Logs are included in verification/.

| Check | Observed result |
|---|---|
| Steps 1–3 | Original tests pass and source hashes unchanged |
| Step 4 nested calls | Pass, 12 instructions |
| RV32I unit assertions | 12,711 checks, zero failures |
| C guest machine image | 596 bytes; 565 retired instructions; PC=0x5c |
| Guest RAM signature | checksum=90, count=6, initialized=7, BSS_before=0, nested_calls=6 |
| Exact/short budgets | 565 succeeds, 564 fails |
| Modified initialized data | Guest computes checksum 102 with input 9; host correctly rejects mismatch |
| Removed guest BSS store | Poisoned RAM triggers guest FAIL |
| Compiler-free route | Hash-checked prebuilt C guest executes through freshly built interpreter |
| ELF inspection | ELF32, little endian, RISC-V, entry 0; rv32i2p1; stack alignment 16 |

The real cross-build found two integration issues in the earlier files: GNU
RISC-V ld rejected --oformat=binary, so GNU builds now use target objcopy on
the linked ELF. Explicit RX/RW program headers also remove a linker warning
about an RWX segment. The successful runs occurred after those fixes.

The ELF is supplied for inspection; the runner loads its flat .bin at address
0 and interprets every guest opcode. It does not execute the guest C natively
or use QEMU. Source files, binary, ELF, linker map, no-alias disassembly and
source/image hash manifest are included under guest/ and guest/prebuilt/.

## Limits

GNU toolchain and the prebuilt route are verified. The optional Clang/LLD
route remains untested here. No official architecture certification, RTL
simulation, cycles/FPS measurement, peripherals, Doom, M, cache or pipeline
was added. ELF segment flags are metadata: the interpreter still has ordinary
shared RAM without memory protection or stack-overflow detection.
