# Doom engine port verification — 22 November 2026

Working copy: /workspace/scratch/6bae74b1c81c/output/rv32i_doom.
Earlier project folders and ZIPs remain untouched. rv32i.c and rv32i.h are
unchanged from the tested runtime milestone. No QEMU or native Doom binary
was used: native code is the interpreter/dispatcher; engine and libraries are
RV32I guest code.

## Actual final-image results

| Check | Observed result |
|---|---|
| Engine image | 430,052 raw bytes, ELF32 little endian, entry 0 |
| Game data | Bundled Freedoom Phase 1 0.13.0 IWAD, 28,795,080 bytes |
| Retained executable scan | 67,124 non-padding words; no M, compressed, CSR or floating-point opcodes |
| First frame | 1,880,989,392 successful core instructions |
| Idle frame 48 | 1,996,670,190 instructions; player x/y unchanged |
| Idle frame 56 | 2,045,535,753 instructions |
| Forward-input frame 56 | 2,045,589,068 instructions; player position and rendered view changed |
| Deterministic prefix | Idle and forward runs match through frame 48 |
| Idle pose | (-27262976,16777216), signed Q16.16 |
| Forward pose | (-25771308,16777757), computed by guest engine |
| Host guard tests | Loader spans, input press/release/weapon encoding, queue overflow, invalid ECALL/framebuffer/sleep, timer wrap and budgets pass |
| TLS initialization | tp equals linked TLS base; TLS does not overlap ordinary BSS; correct startup preserves boot code |
| TLS regression | Temporarily removing tp setup exposes errno corruption as expected |
| Earlier regressions | 12,711 CPU assertions, compiled guest, square and 191-assertion runtime guest all pass |
| Own host/port warnings | Strict builds including -Werror -Wconversion -Wshadow pass |
| Upstream engine warnings | 109 retained diagnostics; not a warning-free engine build |
| Sanitizers | Host suite plus actual guest startup through first frame pass ASan/UBSan |
| Tk GUI | Automatic presentation + W key press/release + guest movement tested under virtual display |
| Prebuilt route | Source/image/asset manifest checks and engine tests pass without a cross compiler on PATH |

Counts exclude serviced ECALL traps; they are not CPU cycles or FPGA timing.
The game test uses deterministic virtual milliseconds; the window uses real
monotonic milliseconds. About 6.1 million successful instructions per frame
were observed over this short settled-scene interval. No FPS performance or
hardware clock claim is inferred from those counts.

## Commands actually run

From the project directory (task-local GCC 14.2.0 was used for cross builds):

```bash
# Build with bundled Picolibc SDK and inspect emitted instructions.
./doom/build.sh
# Full prior tests, host guards and two actual gameplay runs.
DOOM_GAME_TEST=1 ./test.sh
# Full host sanitizer suite (Python loads ASan before the instrumented library).
ASAN_OPTIONS=detect_leaks=0 SANITIZE=1 ./test.sh
# Window/input test with an active Xvfb DISPLAY.
DOOM_GUI_SMOKE=1 ./test.sh
```

A separate headless startup to frame 1 ran with the instrumented host library
and LD_PRELOAD set to GCC's libasan.so. It completed without diagnostics.
Leak detection was disabled for this environment's ptrace limitation. These
sanitizers instrument host code, not the freestanding RV32I engine C itself.
Guest-side behaviour has separate frame/position/boot-integrity checks.

Compiler-free tests use the bundled prebuilt image after checking its source,
SDK and IWAD hashes. Rebuilding is also verified with the bundled Picolibc
headers/libraries and GNU RV32I/ILP32 libgcc. No system installation is made.

## Issue found and fixed during integration

The initial startup omitted x4/tp initialization even though Picolibc's errno
is thread-local. A unified writable RAM model permits the resulting wrong
store without a CPU bounds fault. The final image explicitly initializes tp,
reserves TLS separately from ordinary BSS, zeros the TLS/BSS range and checks
its linked layout. A negative regression removes the tp instructions only in
a temporary test guest and detects boot-code corruption. The final engine's
boot instruction stays intact throughout the frame tests. The missing-tp
build is not the distributed image.

The engine's ELF attributes include an inherited Zicsr superset from a library.
An actual executable-word scan confirms no CSR instructions are retained; the
CPU still rejects CSR operations. The binary still executes only supported
base instruction families. This is a focused scan, not ISA certification.

## Source and feature limits

DoomGeneric is pinned at e257a09527062bfb986b6e5e0780839b6cbd5b71. Two
framebuffer debug-print argument lists were cast to int to match their format
strings. Other upstream warnings remain logged, including temporary-sentinel
sprite-sort diagnostics. No complete guest-language/portability audit is claimed.

This is a first-level engine port, not a complete campaign compatibility test.
It starts E1M1, skill 2, with monsters enabled. Audio, multiplayer, persistent
saves/settings, arbitrary mods and a Linux guest OS are not provided. The
engine's original Freedoom compatibility warning remains visible in its log.
No M hardware, cache, pipeline, RTL or tapeout work was added. The remaining
performance task is profiling startup and frame hotspots before ISA changes.

The preview PNG is exported from the actual guest framebuffer, not an artist's
rendering or a host implementation of the game. Automated Tk events were used;
manual keyboard play on the user's laptop remains their next local check.
