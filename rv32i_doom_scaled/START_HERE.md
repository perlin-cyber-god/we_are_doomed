Display update: default 3x (960x600). See DISPLAY_SCALING.md.

Read CONTROLS_FIX.md first for this corrected input-adapter release.

# Doom engine on our RV32I interpreter

This project boots the Doom engine with bundled Freedoom Phase 1 game data.
The engine and C library are cross-compiled to guest instructions. The native
host executes the interpreter, displays the framebuffer and supplies input/time.

Extract beside your earlier project folders:

```bash
cd ~/Documents/DOOMed/rv32i_doom_scaled
./run_controls_fixed.sh --scale 3
```

The first build can take some time. Then allow roughly 20–90 seconds for the
first game frame on a typical laptop; the window shows instruction progress.
Startup is expensive in this baseline RV32I software-math build. It starts
Episode 1, Map 1, skill 2, with monsters enabled. Sound is disabled.

Controls:

- WASD / arrows: forward/backward and turn.
- Ctrl/F: fire; Space or E: use/open doors.
- 1–7: select a weapon you own; Shift: run; Alt: strafe modifier.
- Escape: game menu; Enter: select; Y/N: confirmation.
- Close the window to stop the emulator.

Escape opens Doom's menu; it does not immediately close this demo like the
square did. Configuration/save writes are intentionally unavailable in this
read-only prototype. Do not rely on save-game menu options.

GCC and Python 3 are needed. The window needs Tkinter and a desktop display.
A hash-checked prebuilt engine is included if a RISC-V GNU compiler is absent.
A matching Picolibc SDK is bundled for local rebuilding; no dependencies are
installed or downloaded by the launch scripts.

Without a window:

```bash
./run_doom.sh --headless
# Longer actual-game test: two deterministic runs, idle vs forward input.
DOOM_GAME_TEST=1 ./test.sh
```

The headless launch saves build/doom/frame.ppm and engine.log. The long game
test saves idle/forward frames and JSON evidence. The ordinary test.sh runs
all prior regressions plus Doom loader/ABI tests and an instruction-image scan;
it explicitly reports when the longer game test was not requested.

Read doom/README.md and VERIFICATION.md for architecture, results and limits.
The square is preserved and still runs via ./run_square.sh. No hardware FPS
or whole-campaign compatibility claim is made for this first engine port.
