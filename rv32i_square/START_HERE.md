# Move a square on your own RV32I computer

Extract this folder beside your earlier milestones. In a terminal:

```bash
cd ~/Documents/DOOMed/rv32i_square
./test.sh
./run_square.sh
```

Hold **WASD or arrow keys** to move. **Escape** exits. Losing window focus
releases held keys. Close the window normally to stop the host application.
You should see a small yellow square on a dark blue background. Its native
160×100 framebuffer is displayed at 4× scale.

The guest is a real cross-compiled RV32I C program. It reads keys and time,
updates its position, clears/draws the framebuffer with guest stores and
requests presentation. Python only displays the framebuffer, captures key
events and supplies a monotonic clock. It never draws or moves the square.

GCC and Python 3 are required. The window additionally needs Python Tkinter
and an active desktop display. For Ubuntu's system Python, the usual Tk
package is `python3-tk`; see the official Python Tkinter documentation linked
in platform/README.md if your Python installation lacks it.

Without a window, run:

```bash
./run_square.sh --headless
```

This verifies scripted guest movement, timing and boundaries, then saves
the guest's initial framebuffer as `build/square.ppm`. It does not claim a
physical keyboard test. A separate Tk smoke test was run under a virtual
display; details and actual results are in VERIFICATION.md.

An installed RISC-V GNU compiler rebuilds the guest. Otherwise the script
checks source/image hashes and uses the included prebuilt RV32I image.
Earlier CPU/guest tests remain part of `./test.sh`.

Read [platform/README.md](platform/README.md) to understand the memory map,
ECALL interface and host/guest responsibilities. Doom is not integrated yet.
