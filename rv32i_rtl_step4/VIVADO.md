# Step 4 in your Ubuntu/Vivado setup

Your supplied Step3 log establishes a complete Vivado2025.2 core-regression
pass. This package retains that core and adds a larger compiled-guest bench.

Run the focused new milestone in an Ubuntu terminal:

```bash
source /home/perlin/Documents/projects/2025.2/Vivado/settings64.sh
cd ~/Documents/DOOMed/rv32i_rtl_step4
./run_guest.sh vivado
```

It cross-compiles guest/guest.c and guest/start.S, links/converts the program,
generates reference traces, compiles/elaborates tb_compiled_guest, and runs
nine bounded headless simulations. It prints RUN before each simulator launch.
GCC/Python3 and RISC-V GCC/binutils must be available in that terminal.

Expected final line begins `PASS RTL_COMPILED_C:`. Deliberately changed data,
missing BSS clearing and insufficient budget must print expected rejections.
Only ordinary O0/O2 cases represent successful normal guest completion.

For the full inherited suites plus new guest: `./test.sh vivado`.
No GUI window or Doom smoke test is launched by either script.

For individual waveform exploration later, add the six rtl/*.sv as Design
Sources and tb/tb_compiled_guest.sv as Simulation Source; select top
tb_compiled_guest. Its IMAGE, TRACE, DUMP, RAM_BYTES, STALL and MAX_STEPS
arguments must match a generated case; use the exact command arguments in
build/guest/O0-stall0.log. This bench needs external files; tb_core_demo
remains the earlier self-contained alternative.

Debug paths/values: dut.pc_q, dut.ir_q, state_o, dut.u_rf.regs_q[1] (ra),
regs_q[2] (sp), regs_q[10] (a0), retire_valid_o and memory request masks.
Wait states delay a completion, not its result. A terminal trap is deliberate.

The new guest milestone was verified with Icarus here; Vivado executables
are unavailable in this environment. Record your actual local result.
