# Square platform verification — 21 November 2026

New copy: /workspace/scratch/6bae74b1c81c/output/rv32i_square.
Previous project folders/ZIPs are preserved. rv32i.c and rv32i.h are unchanged.
Host compiler GCC 14.2.0; guest compiler RISC-V GCC 14.2.0; target RV32I/ILP32.

## Observed checks

| Check | Result |
|---|---|
| Earlier milestones + original compiled C guest | Pass, including all 12,711 RV32I assertions |
| New square guest image | 536 bytes, cross-compiled and executed by interpreter |
| Frames and movement | Every framebuffer pixel checked at expected positions |
| Input | Direction keys, release and opposite-key cancellation pass |
| Bounds | Both corners reached and clamped; all four edges checked |
| Timer | 15 ms stays asleep, 16 ms resumes; 32-bit wrap works |
| Host control | Bounded slices, invalid service, invalid sleep, illegal instruction, loader bounds pass |
| Main headless movement sequence | 16,612,328 successful core instructions, excluding ECALL traps |
| Full strict-warning build | Pass, no warnings |
| ASan + UBSan | Full suite and final platform tests pass; leak detection disabled |
| Prebuilt-only route | Both original and square guest images pass hash checks and execute |
| Tk GUI smoke | Pass under a task-local Xvfb virtual display with Tk 9.0 |

Actual commands from the workspace root (compiler unpacked under toolchains/):

```bash
PATH="$PWD/toolchains/rv32i-local/usr/bin:$PATH" ./output/rv32i_square/test.sh
ASAN_OPTIONS=detect_leaks=0 SANITIZE=1 PATH="$PWD/toolchains/rv32i-local/usr/bin:$PATH" ./output/rv32i_square/test.sh
```

The final prebuilt route and GUI smoke ran together with an active virtual
DISPLAY and GUI_SMOKE=1, without the cross compiler on PATH:

```bash
GUI_SMOKE=1 ./test.sh
```

A final sanitizer run also exercised the invalid-sleep test added after the
first full run. Logs are provided beside this report. Leak detection is
incompatible with this environment's ptrace wrapper and was disabled.

The GUI test created a real Tk window under Xvfb, presented guest pixels,
generated press/release events for D, checked the moved and vacated pixels,
then generated Escape and confirmed guest exit after six frames. This was
an automated virtual-display test, not a human physical-keyboard test on the
user's laptop. Xvfb initially could not create local Unix sockets in this
environment; a temporary TCP display allowed the smoke test, then was stopped.
No GUI test tools are needed to use the project on a normal desktop.

verification/square.png is exported from the guest RAM framebuffer. It is not
an artist's illustration or a host-rendered substitute for guest drawing.

## Limits

There is no Doom port yet. This demo uses our explicit ECALL ABI, not real
memory-mapped device hardware or a standard OS syscall ABI. The CPU still
has no privileged trap-vector implementation. The display/input/time handler
lives in the host execution environment. All guest drawing and position math
execute through the interpreter. Memory regions are reserved by convention;
there is no MPU, cache, pipeline or cycle model. No FPS performance claim is
made. Tk GUI and ASan shared-library loading are tested separately.
