# Run the console/timer milestone

In an Ubuntu terminal, with RISC-V GCC/binutils and native GCC/Python3:

```bash
source /home/perlin/Documents/projects/2025.2/Vivado/settings64.sh
cd ~/Documents/DOOMed/rv32i_rtl_step5
./run_io.sh vivado
```

This compiles two C guests, one host replay checker and four simulation tops.
It runs20 short, bounded headless simulations:2 device-unit runs and18 CPU
runs. There is no GUI or Doom window. It prints RUN before each launch, then
checks actual RTL traces/events/console output. Final marker: PASS RTL_MMIO:.

Normal console output:

```text
CONSOLE: Hello from our RV32I CPU!
```

Expected access-fault/budget cases are successful negative tests. A PASS for
them means the requested bad operation was rejected correctly, not that all
device accesses are allowed. Logs/events/console files are under build/io/.

To include earlier suites too: `./test.sh vivado`. The focused command avoids
repeating Step3/Step4 you already verified. With Icarus, use
`./run_io.sh iverilog`. Optional RISCV_PREFIX works as documented in GUEST.md.

For waveform exploration later: add all seven rtl/*.sv Design Sources and
tb/tb_mmio_unit.sv or tb/tb_mmio_cpu.sv as Simulation Sources. The direct
tb_mmio_unit needs no external files. Its wait wrapper is tb_mmio_unit_wait.
The compiled CPU bench needs IMAGE/TRACE/EVENTS/CONSOLE/MAX_STEPS paths; use
the exact generated arguments in build/io/O0-wait0.log. Choose only the
intended simulation top. The CPU waiting wrapper is tb_mmio_cpu_wait.

Useful signals: CPU pc_o/state_o; fabric timer_value_o; tx_valid/tx_ready/
tx_data; mem_address_o/read/write masks/done; RAM forwarding signals.
The console store uses high address10000000 and does not index RAM.

Step5 was verified with Icarus here. Your earlier Vivado Step3/Step4 passes
are recorded separately. This new Vivado route still needs your actual run.
