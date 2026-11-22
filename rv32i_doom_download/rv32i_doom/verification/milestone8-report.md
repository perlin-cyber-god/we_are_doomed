# Runtime foundation verification — 21 November 2026

Project copy: /workspace/scratch/6bae74b1c81c/output/rv32i_runtime.
Earlier folders/ZIPs are untouched. The interpreter, header and square platform
implementation are byte-for-byte unchanged from rv32i_square. This milestone
adds a separately compiled guest, new host loading map and runtime functions.

| Check | Actual result |
|---|---|
| Prior CPU assertions | 12,711 checks, zero failures |
| Prior C guest / square tests | All pass |
| Runtime image | 14,292 bytes, RV32I/ILP32 |
| Runtime guest | 191 assertions, zero failures, 216,856 successful guest instructions |
| Asset fixture | 55-byte synthetic PWAD, parsed checksum 24 |
| Heap | Full 262,128 payload bytes recovered after coalescing |
| Invalid-free diagnostics | Exactly 2 intentional invalid/double-free test cases |
| Malformed/changed files | Wrong magic, truncation, invalid directory and changed numbers rejected by guest |
| Static image scan | 3,476 non-padding code words; base opcode families, no M or RVC instructions |
| ELF attributes | rv32i2p1, stack alignment 16 |
| Undefined symbols | None in linked runtime ELF |
| Strict host/guest builds | Pass with -Werror -Wconversion -Wshadow |
| Host ASan/UBSan suite | Pass with leak detection disabled for ptrace environment |
| No-cross-compiler route | Prebuilt manifests validated and all guest suites execute successfully |

Commands actually run from the workspace root:

```bash
PATH="$PWD/toolchains/rv32i-local/usr/bin:$PATH" ./output/rv32i_runtime/test.sh
ASAN_OPTIONS=detect_leaks=0 SANITIZE=1 PATH="$PWD/toolchains/rv32i-local/usr/bin:$PATH" ./output/rv32i_runtime/test.sh
# Without adding the task-local cross compiler to PATH:
./output/rv32i_runtime/test.sh
```

Host GCC 14.2.0, RISC-V GCC 14.2.0 and the RV32I/ILP32 libgcc multilib were
used. The compiler was already available task-locally from the preceding
milestone. Actual full logs are included under verification/.

Guest tests cover overlap copies, bounded strings, unsigned comparisons,
allocator alignment/reuse/split/coalesce/exhaustion/overflow/zeroing/realloc,
invalid/double free, WAD directory bounds and duplicate entries, EOF reads,
positive/negative arithmetic, 64-bit operations, fixed-point rounding,
divide-by-zero and checked overflow. Compiler helpers execute through rv_step;
there are no host arithmetic shortcuts. Asset mutations are host-side test
inputs, not fabricated guest results. The guest reports failure gracefully
without an instruction access trap for every malformed fixture tested.

Sanitizers instrument the host interpreter/runners/platform tests. They do not
instrument the freestanding RV32I guest C or certify it against every C-language
error; guest-side assertions and interpreter bounds checks supply separate
functional verification. No official architecture certification was performed.

## Remaining work

This is not full Doom, complete libc, full standard I/O or an engine-integrated
WAD backend. Our asset window is only 4 KiB; test heap is 256 KiB. The pinned
engine config requests a 6 MiB zone by default, plus other game memory. Our
checked fixed-point API differs from Doom's overflow policy and needs a
specific adapter. The existing square GUI is unchanged and was not manually
retested in this runtime milestone; its headless regressions pass. Doom FPS,
hardware timing, M, caches, pipelines, multiplayer and audio are not added.
