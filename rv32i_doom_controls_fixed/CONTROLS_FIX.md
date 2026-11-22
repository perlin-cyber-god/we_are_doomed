# Controls fix — 22 November 2026

Close the old game window, extract this separate folder, then run:

```bash
cd ~/Documents/DOOMed/rv32i_doom_controls_fixed
./run_controls_fixed.sh
```

This launcher rebuilds only the native input adapter and loads the unchanged,
hash-checked prebuilt RV32I guest. It does not run any test suite or GUI smoke
test. The interactive window is for your own play. Startup can still be slow.

## Confirmed fault and fix

The old host sent Ctrl as 0x9d (KEY_RCTRL) and Space/E as ASCII 32. This
DoomGeneric engine binds fire to KEY_FIRE=0xa3 and use to KEY_USE=0xa2.
The adapter now uses the engine's own doomkeys.h constants. Ctrl or F fires;
Space or E opens doors/uses. Movement is unchanged. F is a new fire alias.

## Focused verification performed

`bash ./test_controls.sh` compiled with GCC and C11, -Wall -Wextra -Wpedantic
-Werror -Wconversion -Wshadow -O2. All existing host checks and all 21 keys'
press/release ECALL values passed. Pure Python checks cover both Ctrl keys,
F, Space/E, each weapon key, combined keys and release. The same expanded
C test fails against the original host adapter at the key-ID assertion.
No GUI, full game run or long regression suite was run for this fix.

The original engine/CPU binaries and previous verification logs are retained.
Those old logs are historical evidence, not proof that this fix was played.
There is no guest rebuild or instruction-set change.

## Weapon report still needs local checking

Keys 1 through 7 reach the guest as ASCII '1' through '7'; this is now checked
for press and release. Standard slots are 1=fists/chainsaw, 2=pistol,
3=shotgun (when owned), and higher slots require their respective weapons.
The reported apparent flying and key 3 selecting fists were not reproduced
or diagnosed by these short tests. No claim is made that they are fixed.

Keyboard only: mouse aiming and mouse-button shooting are not implemented.
Click the window to focus it; Esc opens the game menu, and closing the window
stops the emulator. Ctrl+C in its launching terminal can also stop it.
