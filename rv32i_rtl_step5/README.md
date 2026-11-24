# Step 5 — our processor talks to devices

A compiled RV32I guest now prints:

```text
Hello from our RV32I CPU!
```

It also reads a free-running timer, waits for at least64 ticks, and publishes
checked results in RAM. **The CPU core is unchanged.** A new rv32_mmio block
routes its existing memory requests to RAM, console or timer.

## Run the new milestone

Extract into **~/Documents/DOOMed/rv32i_rtl_step5**, then in Ubuntu:

```bash
source /home/perlin/Documents/projects/2025.2/Vivado/settings64.sh
cd ~/Documents/DOOMed/rv32i_rtl_step5
./run_io.sh vivado
```

Needs native GCC, Python3, and RISC-V GCC/binutils as in Step4. This runs only
the new tests:18 CPU programs/runs plus2 device unit runs. RUN lines show
progress. A complete success ends with **PASS RTL_MMIO:**.

The four ordinary O0/O2 runs must print the greeting. Access-policy tests
deliberately trap; a budget test deliberately stops incomplete. Their PASS
means those expected outcomes were checked, not normal application completion.

`./test.sh vivado` also repeats all inherited suites. No GUI or Doom window
is launched. With Icarus available, use `./run_io.sh iverilog`.

## The essential idea

| Address | Legal guest operation | Effect |
|---|---|---|
| 00000000–00003fff | Existing RAM loads/stores/fetch | Ordinary RAM |
| 10000000 | SB only | Send one byte to the console stream |
| 10000004 | LW only | Bit0 says whether the console sink is ready |
| 10000010 | LW only | Read 32-bit clock-tick counter |

Addresses above are hexadecimal. Other non-RAM accesses, wrong device widths,
wrong directions and instruction fetch from devices return access errors.
The map belongs to our computer design; it is not a RISC-V-defined device map.

The console is a ready/valid byte interface. The testbench receives its bytes
and writes them to a file; the runner displays the verified text. It is not
yet a serial UART or a keyboard/display controller. Timer ticks are simulated
clock cycles, not milliseconds or your laptop's wall time.

## What passed here

All 18 CPU traces and2 unit benches passed under Icarus. **483,693 numeric
replay fields matched** the architectural reference, including final RAM.
Unit tests independently check timer increments/wrap/reset, console waiting
and cancellation, bad requests and error propagation. The full inherited
regression also passed. The six previous RTL files are byte-identical.

Step3 and Step4 passed in your supplied Vivado logs. This new Step5 Vivado
route still needs your local run; Vivado is unavailable here.

Read **LESSON.md** for C→assembly→bus explanations; **MMIO.md** for the exact
interface/replay contract; **VERIFICATION.md** for actual evidence. Historical
guides are retained under docs/ and the previous guest remains under guest/.

Next comes a small input/framebuffer interface and larger runtime workloads.
No Doom-on-RTL, new ISA extension, cache/pipeline or GDS result is claimed.

Recall: the instruction is still a store. **The address selects what receives it.**
