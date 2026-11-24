# RTL Step 2 — let instruction bits choose the work

**Step 1:** the testbench selected an ALU operation by hand.
**Step 2:** an RV32I instruction word selects registers, a constant and an
operation. An external commit pulse authorizes the register write.

## First Vivado demo

1. Add all five `rtl/*.sv` files as **Design Sources**.
2. Add `tb/tb_decode_demo.sv` as **Simulation Sources**.
3. Set simulation top to **tb_decode_demo**.
4. Run Behavioral Simulation, then **Run All**.

No external files are needed by this demo. Look for `PASS RTL_DECODE_DEMO`.

| Word | Meaning | What you see |
|---|---|---|
| `02800313` | ADDI x6,x0,40 | x6 becomes 40 at an enabled edge |
| `FFF30313` | ADDI x6,x6,-1 | immediate is FFFFFFFF; x6 later becomes 39 |
| `00002303` | LW x6,0(x0) | Recognized, but blocked in this arithmetic-only slice |
| `00000073` | ECALL | Explicit class; blocked here pending trap handling |
| `02630333` | MUL | Illegal because M is unsupported |

## Read these in order

| File | Job |
|---|---|
| `rtl/rv32_immediate.sv` | Reassemble/sign-extend instruction constants |
| `rtl/rv32_decode.sv` | Recognize RV32I and produce control intent |
| `rtl/rv32_decode_slice.sv` | Connect decoder, ALU and register file |
| `tb/tb_decode_demo.sv` | Short waveform example |

The ALU, register file and previous tests are included unchanged. Full suite
from a terminal with Vivado commands available:

```bash
cd ~/Documents/DOOMed/rv32i_rtl_step2
./test.sh vivado
```

Or use `./test.sh iverilog`. See VIVADO.md and VERIFICATION.md.

**Boundary:** all 40 base instruction names are decoded, but only arithmetic,
LUI and AUIPC can update registers in this slice. There is no PC register,
instruction fetch, data memory, branch execution or trap controller yet.
PC is supplied externally for AUIPC. This is not the complete CPU.

**Remember:** decoding describes the work; commit permits a supported result
to be stored. Next build: stored PC/IR and the multi-cycle controller.
