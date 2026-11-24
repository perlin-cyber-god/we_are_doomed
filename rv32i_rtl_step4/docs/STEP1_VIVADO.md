# Vivado: run and inspect this milestone

## Terminal route

Load your own Vivado installation's `settings64.sh`, then run:

```bash
./test.sh vivado
```

The runner calls xvlog with `-sv`, xelab and xsim in batch mode. It does not
open a GUI. Logs go under build/; a VCD waveform is also requested. Missing
tools or a timeout are failures, not a passing simulation. The simulator must
print the explicit completion marker. This Vivado route is supplied for your
laptop; consult VERIFICATION.md for which simulator was actually run here.

## Your existing Vivado project

1. Add **Design Sources**: `rtl/rv32_alu.sv`, `rtl/rv32_regfile.sv`.
2. Add **Simulation Sources**: `tb/tb_demo.sv`. Keep it out of synthesis.
3. Ensure the three source files are treated as SystemVerilog.
4. Set simulation top to **tb_demo**.
5. Run Behavioral Simulation, then Run All. The demo finishes at 66 ns and
   needs no external vectors. No physical board is needed.

Do not infer success just because the waveform window opened: find the
`PASS RTL_DEMO` line in the demo console. The test does not fetch or
execute RISC-V instructions; it verifies the component circuits directly.

For the comprehensive suite, use `./test.sh vivado`. Or add
`tb/tb_rtl_step1.sv` as Simulation Sources, choose tb_rtl_step1 as top and
set the runtime option below (adjust the absolute path if needed):

`-testplusarg VECTORS=/home/perlin/Documents/DOOMed/rv32i_rtl_step1/vectors/alu_vectors.txt`

That test must print `PASS RTL_STEP1 checks=18563 failures=0`. Its watchdog
is 1 ms of simulation time. No instruction decoder or full CPU is present.

## Look at these signals

| Signals | What to notice |
|---|---|
| clk, reset, write_enable | Reset/write effects occur on rising edges |
| write_data, x6_value, x0_value | The value presented for writing and stored values |
| alu_result | Always calculates x6 - 1, without waiting for a clock |

Set data radix to unsigned or hexadecimal as appropriate. The demo writes
40 to x6, computes 39, holds 40 while write is disabled, then explicitly
writes 39 at a later rising edge. The combinational ALU then calculates 38,
but the register holds 39 with write disabled. Clock period is 10 ns only
as stimulus; there is no timing analysis behind it.

## References

- AMD UG900, SystemVerilog parsing and simulator command options:
  https://docs.amd.com/r/en-US/ug900-vivado-logic-simulation/Parsing-Design-Files-xvhdl-and-xvlog
- Icarus command-line language selection:
  https://steveicarus.github.io/iverilog/usage/command_line_flags.html
