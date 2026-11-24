# Milestone 9: first working Doom engine port

The unchanged RV32I interpreter runs the Doom engine, guest C library and
software arithmetic helpers. The host does not execute Doom natively or
delegate rendering/gameplay to a native game engine. No QEMU is used.

## Guest versus host

| Component | Execution |
|---|---|
| Engine gameplay, enemy AI, collision and rendering | RV32I guest instructions |
| Picolibc allocation, formatting, memory/string functions and file streams | RV32I guest instructions |
| libgcc multiplication/division helpers | RV32I guest instructions |
| WAD reading | Guest libc reads the supplied RAM-backed IWAD |
| Native C interpreter and ECALL dispatcher | Host CPU |
| Window/image format conversion and keyboard capture | Python Tk frontend |

The milestone-8 educational runtime remains independently tested. The full
engine links Picolibc for its wider dependency set; it does not pretend that
our small earlier libc subset was already sufficient. All linked library
code still executes through rv_step. The original zone allocator remains
inside the engine, obtaining its 6 MiB arena from guest malloc.

## Open-source inputs and modifications

- DoomGeneric engine: commit e257a09527062bfb986b6e5e0780839b6cbd5b71.
  Source is under engine/; license under ENGINE_LICENSE.
- Picolibc 1.8.10-2 Debian package: selected headers and RV32I/ILP32 libc.a
  and libm.a are under picolibc/. Copyright/permissions are included there.
- Freedoom 0.13.0-2 Debian package: Phase 1 IWAD under assets/ with copyright
  and credits. No commercial Doom game assets are bundled.
- GCC RV32I/ILP32 libgcc: linked compiler arithmetic support. The compiler
  runtime copyright/exception is retained in runtime/GCC_COPYRIGHT.txt.

The engine code is pinned, with one small source adjustment: two debug-print
argument lists in i_video.c explicitly cast framebuffer fields to int, matching
their format strings under Picolibc's uint32_t typedef. The casts preserve the
actual binary rendering behaviour. Port-specific changes live in port/.

Own host/port code builds with strict warnings including -Werror. The engine
build retains -Wall/-Wextra diagnostics in build/doom/compile.log and currently
has 109 upstream warnings, mostly unused parameters and signed comparisons.
These are not silently reported as a warning-free engine build. The sprite
sorting sentinel also attracts two dangling-pointer warnings; the function
removes nodes from that temporary list and relinks them to a persistent list
before returning. No complete guest-C language/portability audit is claimed.

## Execution environment ABI

ECALL traps in rv_step as before. This optional dispatcher recognizes:

| a7/x17 | Service | Inputs / output |
|---|---|---|
| 1 | Milliseconds | Return low 32 bits in a0 |
| 2 | Keyboard event | Return 0 if none, else 0x10000 plus pressed<<8 plus Doom key byte |
| 3 | Present framebuffer | a0=guest address, a1=320, a2=200; checked before host access |
| 4 | Sleep | a0=0..1000 ms; suspend until elapsed |
| 5 | Exit | a0=status; stop the guest |
| 6 | Console character | Low byte of a0 appended to a bounded host log |

Successful services return a0 and advance PC by 4. Unknown/invalid services
report an error without committing guest PC/register/RAM changes. The core's
ECALL/EBREAK/illegal-instruction behaviour is unchanged outside this dispatcher.
No generic SYSTEM no-op exists. This is a custom execution-environment ABI,
not a Linux kernel or privileged RISC-V platform.

Keyboard press/release edges are queued in a 64-slot ring. Queue overflow is
an explicit error. Mapped bits represent arrows, Ctrl, use, Enter, Escape,
Tab, Shift, Alt, weapon keys 1–7, Y/N and Backspace. The Python frontend also
maps WASD to arrows and E to use. Losing focus releases held keys.

## Read-only guest filesystem and heap

Guest fopen/seek/read/close use a tiny descriptor layer in port/libc_glue.c.
Only the basename freedoom1.wad is exposed, and its bytes come from a reserved
guest RAM region. No guest path reaches the host filesystem. Config/save
writes, mkdir, unlink and rename fail explicitly with read-only errors.
Shell commands are unsupported. getcwd supplies a fixed directory in a caller
buffer; this is a restricted subset, not a full POSIX implementation.

The guest sbrk implementation reserves memory within a bounded guest heap;
Picolibc malloc runs on the guest. File reads and seeks stay within the asset.
The native loader validates the IWAD directory/lump spans before copying it,
but does not validate every semantic structure in arbitrary custom maps. Use
the bundled tested Phase 1 asset; arbitrary IWAD/mod support is not claimed.

## 64 MiB guest memory map

| Inclusive region | Purpose |
|---|---|
| 0x0000000–0x017FFFF | Engine text, initialized data and BSS |
| 0x01F0000–0x01F0FFF | Boot descriptor and guest-written player diagnostic report |
| 0x0200000–0x1FFFFFF | 30 MiB guest heap |
| 0x2000000–0x3EFFFFF | Up to 31 MiB IWAD data |
| 0x3F00000–0x3FFFFFF | 1 MiB descending stack; initial sp=0x4000000 |

Other gaps are reserved. RAM is ordinary byte-addressed memory without MPU
protection. The guest clears TLS/BSS and initializes sp/gp/tp at startup. Raw binary
loads at zero. The 320×200 framebuffer is allocated by the guest in its heap;
it is not a fixed host-created image. Pixels are 0xAARRGGBB words; the frontend
extracts RGB and displays at 2× scale.

For testing only, the guest copies its actual player x/y/angle to a reserved
report at 0x1F0010. The host reads this to verify input caused guest-computed
movement. It never sets player coordinates or skips engine calculations.

## Rebuild and tests

```bash
./test.sh
./run_doom.sh
./run_doom.sh --headless
DOOM_GAME_TEST=1 ./test.sh
# Active desktop display, normal non-sanitizer build:
DOOM_GUI_SMOKE=1 ./test.sh
```

The scripts use an installed RISC-V GNU compiler if available and the bundled
Picolibc SDK. Otherwise source/image/asset hashes are checked and the included
guest image is used. No download or installation occurs at launch. Python
and the host shared library are rebuilt locally; prebuilt game code is RV32I.

The long deterministic test runs an idle machine and a forward-input machine
to frame 56. Their prefixes through frame 48 must match; the idle player must
stay fixed while the forward-input player moves and the rendered view differs.
The GUI test generates actual Tk key events under a display and checks movement.
These are functional tests, not complete campaign or hardware timing tests.

The ELF carries an inherited rv32i2p1_zicsr2p0 library attribute. check_image.py
scans the retained executable words and rejects M, compressed, CSR, privileged
SYSTEM and floating-point opcode families. The merged tag is not evidence
that our CPU implements CSR. Dynamic game tests use the unmodified RV32I core.
The scan is focused validation, not official ISA certification.

## Current limits and optimization work

Startup does about 1.88 billion guest instructions before the first frame.
Expect a noticeable wait and watch the window's instruction counter. The
headless test supplies deterministic virtual time; the window uses real
monotonic milliseconds. Neither instruction counts nor headless rendering
rates are FPGA cycle counts. No smooth-FPS or hardware-frequency claim is made.

The default starts E1M1 at skill 2, with monsters enabled and sound disabled.
Only short first-level startup/movement tests have been performed. The engine
prints an upstream Freedoom-compatibility warning; whole-campaign compatibility
is not established. Saves/settings are unavailable, audio and multiplayer are
not connected, and no M unit/cache/pipeline was added. The next step is profiling
initialization and frame work before choosing architecture optimizations.

### Thread-local storage startup regression

Picolibc stores errno in TLS. Startup sets x4/tp to the linked TLS base, and
the linker reserves TLS separately from ordinary BSS. The image checker
verifies the TLS program-header base, tp target and non-overlap. A regression
executes correct startup and a temporary guest with the tp setup removed;
the latter exposes an errno write to address zero. It is an intentionally
broken test fixture, never the distributed engine image. Boot-code integrity
is also checked on every tested game frame. This is ABI startup work, not
new ISA instructions or a multithreaded CPU.


## Controls correction

The host uses engine/doomkeys.h KEY_FIRE=0xa3 and KEY_USE=0xa2; Ctrl/F and
Space/E respectively. See ../CONTROLS_FIX.md for focused tests and limits.
