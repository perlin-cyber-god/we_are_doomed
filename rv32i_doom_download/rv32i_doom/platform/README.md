# Milestone 7: display, input and time

The RV32I decoder is unchanged. This optional execution environment handles
specific ECALL traps, services the request and resumes at PC+4. The original
rv32-run example retains its original trap/completion behaviour. No new ISA
instruction or privilege/CSR support was added.

## Files and responsibilities

| File | Job |
|---|---|
| guest/square.c | Guest position, key interpretation, drawing loops and pacing |
| guest/start.S | Stack/BSS startup, ECALL wrapper, exit |
| guest/linker.ld | Program/framebuffer/stack reservations |
| abi.h | Shared constants for this demo's interface |
| platform.c | Interpreter slices, ECALL dispatch and sleeping state |
| window.py | Tk display, held-key capture, real host milliseconds |
| test_platform.c | Deterministic virtual-clock and scripted-input checks |

The test and window never supply position coordinates to the guest. The
guest computes them and paints its own RAM. The headless test independently
checks all 16,000 pixels against its expected position. The window converts
the guest's little-endian colour values to RGB PPM for Tk and scales by four;
that is format conversion/presentation, not guest rendering computation.

## Demo ABI (not a standard operating-system ABI)

At ECALL: a7/x17 selects the service; a0/x10 is its argument/result. Successful
services advance guest PC by 4 and set a0. Other registers and RAM remain
unchanged by the dispatcher. ECALL still returns RV_ECALL without changing
guest state inside rv_step itself. Unknown/invalid calls fail without advancing
PC or changing guest registers/RAM; host diagnostic state records the error.

| a7 | Service | Argument / result |
|---|---|---|
| 1 | Ticks | Return low 32 bits of elapsed milliseconds |
| 2 | Keys | Return held-key bits: left=1, right=2, up=4, down=8, quit=16 |
| 3 | Present | Fixed framebuffer below; return 0 and notify frontend |
| 4 | Sleep | a0=0..1000 ms; return 0, suspend guest until elapsed |
| 5 | Exit | a0=exit code; notify host to stop this guest |

The guest's platform_call(service,argument) is a normal C call; the assembly
wrapper moves its arguments into the ABI registers, executes ECALL and returns.
This is where the existing instruction set connects to the outside world.

## RAM map (256 KiB)

| Inclusive addresses | Purpose |
|---|---|
| 0x00000–0x07FFF | Text/data/BSS; flat binary loaded at zero |
| 0x08000–0x0FFFF | Reserved gap |
| 0x10000–0x1F9FF | 160×100 framebuffer: 64,000 bytes |
| 0x1FA00–0x3EFFF | Reserved gap |
| 0x3F000–0x3FFFF | 4 KiB descending stack; initial sp=0x40000 |

Pixel value is 0x00RRGGBB, stored little-endian. Pixel (x,y) begins at
0x10000 + 4*(160*y+x). The guest writes background 0x00102030 and an 8×8
square of 0x00FFCC00, initially at (76,46). It clamps x to 0..152 and y to
0..92. Opposing direction keys cancel. RAM starts poisoned; the guest draws
the entire buffer before presenting. Memory reservations are conventional;
the CPU has no protection against a guest overwriting them.

## Timing and execution budgets

The real window supplies monotonic host milliseconds. Headless tests supply
deterministic virtual milliseconds. A slice's time and keys are fixed for that
slice. Movement updates at most once per guest iteration after at least 16 ms;
sleep requests 16 ms between frames. This is a simple demo pacing policy,
not a guaranteed refresh rate or a clocked CPU simulation. No catch-up motion
is performed after a long stall. Both guest elapsed-time arithmetic and host
sleep arithmetic work across 32-bit millisecond wrap, provided the clock is
serviced regularly and not skipped by a full 2^32 ms period.

The frontend executes bounded 20,000-attempt slices then returns to Tk's event
loop. A ten-second no-frame watchdog reports a stalled guest. The headless
test has a finite per-frame attempt budget. pl_steps counts successful
rv_step instructions; serviced ECALL traps are excluded from that count.

## Verification commands

```bash
./test.sh
ASAN_OPTIONS=detect_leaks=0 SANITIZE=1 ./test.sh
./run_square.sh --headless
# On an active display, with a normal (non-sanitizer) build:
GUI_SMOKE=1 ./test.sh
```

The GUI smoke generates real Tk key events and checks presented image colours;
it is separate from scripted C input tests. Run GUI and sanitizer modes
separately: a sanitizer shared library needs special runtime loading inside
Python that this launcher does not configure. run_square.sh normally rebuilds
a plain shared library before opening a window.

Sources: [official Python Tkinter documentation](https://docs.python.org/3/library/tkinter.html)
for PhotoImage PPM input, keyboard events and after callbacks. This demo ABI
is our own, not an implementation of doomgeneric's hooks yet.

Next: connect a larger runtime and asset-loading plan to the Doom port. No
Doom run, audio, multiplier extension, cache, pipeline or FPS claim is made.
