# Vivado Step 2 — short setup

Use a separate project, or avoid compiling the older milestone's duplicate
module files alongside these complete replacement project files.

**Design Sources:**
- rtl/rv32_alu.sv
- rtl/rv32_regfile.sv
- rtl/rv32_immediate.sv
- rtl/rv32_decode.sv
- rtl/rv32_decode_slice.sv

**Simulation Sources:** tb/tb_decode_demo.sv. Set top to **tb_decode_demo**.
Run Behavioral Simulation -> Run All. This demo needs no vector-file path,
stops automatically, and must print `PASS RTL_DECODE_DEMO`.

Watch: clk, instruction, imm, rs1, rd, kind, can_execute, commit,
write_enable, result, x6_value and illegal. Use hexadecimal for instruction
and immediate, unsigned/decimal for x6. FFFFFFFF represents -1 in ADDI.
The testbench's x6 observation is not an extra synthesized register-file port.

For the complete headless suite in a Vivado-enabled terminal:

```bash
./test.sh vivado
```

It also runs all Step 1 regressions and the new decoder/integration benches.
No GUI or Doom is launched. Every simulation has an internal watchdog and
an external process timeout. A native GCC/Clang compiler enables the C
legality cross-check; otherwise that extra check is explicitly skipped.

For individual GUI testing, tb_decode_slice needs no vectors. tb_decode
needs this simulator runtime option, adjusted for your extraction path:

`-testplusarg DECODE_VECTORS=/home/perlin/Documents/DOOMed/rv32i_rtl_step2/vectors/decode_vectors.txt`

Step 1 historical instructions are retained under docs/. Local verification
status and limitations are recorded in VERIFICATION.md; a screenshot alone
does not certify that the comprehensive regression suite passed.
