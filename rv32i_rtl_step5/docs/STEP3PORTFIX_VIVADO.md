# Run Step 3 in Vivado

Use a new project/folder to avoid duplicate module definitions from older
projects. Choose any device appropriate for a simulation project; this
milestone does not claim device-specific synthesis or timing results.

## Tiny demo first

Add these Design Sources:

- rtl/rv32_alu.sv
- rtl/rv32_regfile.sv
- rtl/rv32_immediate.sv
- rtl/rv32_decode.sv
- rtl/rv32_decode_slice.sv (preserved; unused by the new core)
- **rtl/rv32_core.sv (new)**

Add **tb/tb_core_demo.sv** as a Simulation Source. Select **tb_core_demo**
as simulation top, replacing tb_decode_demo. Run Behavioral Simulation,
then Run All. This short demo needs no external files and has a watchdog.

Expected ending:

```text
PASS RTL_CORE_DEMO: fetched 4 instructions; x6=39 RAM[512]=39; explicit EBREAK trap
```

The recorded Icarus run finishes at 226 ns. Vivado has not been executed
here; report its actual PASS/error output rather than assuming success.
The old Step 2 demo still finishes at 76 ns and tests a different top.

Watch **pc_o**, **instruction_o**, **state_o**, **x6_value**, **ram_value**,
**retire_valid_o**, **mem_valid_o**, **mem_read_mask_o**,
**mem_write_mask_o**, **trap_valid_o**, **trap_code_o**.
Decimal x6/RAM, hexadecimal PC/instruction, unsigned decimal state are useful.

Zoom around LW: PC=4 remains held through FETCH/DECODE/EXECUTE/MEMORY;
COMMIT updates x6 to40 and PC to8. After ADDI commits, x6 is39 while RAM
is40. The successful SW completion changes RAM to39; its COMMIT advances PC.

## Complete bounded regression

From an Ubuntu terminal with Vivado settings loaded and native GCC/Python3:

```bash
source /home/perlin/Documents/projects/2025.2/Vivado/settings64.sh
cd ~/Documents/DOOMed/rv32i_rtl_step3_portfix
./test.sh vivado
```

The runner invokes xvlog/xelab/xsim headlessly, first runs preserved block
tests, regenerates machine-code programs and C reference traces, then runs
each core program with zero-wait and delayed memory. It launches no windows.
Every process has a timeout; benches also have simulated-time watchdogs.
Full logs go to build/; the supplied run evidence is in verification/step3/.
The full regression requires GCC; it does not silently skip the C comparison.

Alternative, with Icarus installed: `./test.sh iverilog`.
Do not add multiple tb_core_* tops together as the active top for a run.
Use the runner for the differential bench: it supplies IMAGE/TRACE/RAM_BYTES
and STALL paths/arguments, and selects reset-PC wrapper tops when needed.

No synthesis, FPGA programming, GUI smoke test or long Doom simulation is
part of these commands. Historical guides are preserved in docs/.
