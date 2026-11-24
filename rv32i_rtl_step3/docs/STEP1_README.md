# RTL Step 1 — the ALU and register file

**Goal:** build and understand two real SystemVerilog CPU blocks.
This is not the complete CPU yet. Keep the working Doom folders as they are.

## Run

Extract this folder beside your older projects. In a terminal with Vivado
commands available (after loading your installation's `settings64.sh`):

```bash
cd ~/Documents/DOOMed/rv32i_rtl_step1
./test.sh vivado
```

For Icarus Verilog instead: `./test.sh iverilog`. Python 3 is required by the
runner. All tests are headless, bounded and stop automatically. A successful
run prints `PASS RTL_STEP1 checks=18563 failures=0`; the final numbers and local
verification status are recorded in VERIFICATION.md.

## What to open first

| File | Read it for |
|---|---|
| `rtl/rv32_alu.sv` | Combinational calculation; no clock or stored result |
| `rtl/rv32_regfile.sv` | Registers; rising-edge writes and constant x0 |
| `tb/tb_demo.sv` | **First waveform:** x6 holds 40, then captures 39 |
| `tb/tb_rtl_step1.sv` | The larger self-checking testbench |
| `VIVADO.md` | Add the files and inspect waveforms in your existing project |
| `SPEC.md` | Exact ports, operation codes and reset/timing rules |

## The simple picture

1. Read a register value, such as 40.
2. The ALU calculates 40 + (-1) = 39.
3. Nothing is stored merely because the answer exists.
4. At a rising edge with write enabled, the register file captures 39.

The testbench demonstrates this using x6. It drives the controls itself;
there is no instruction fetch or decoder in this milestone.

**Remember:** the ALU calculates; the register file remembers.

## What comes next

Immediate generation and instruction decoding, then PC/IR/intermediate
registers, a multi-cycle controller and memory interface. GDSII follows CPU
verification and synthesis. These files have not been physically implemented.
