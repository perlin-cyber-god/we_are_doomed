# Explicit input-net fix — 24 November 2026

Your Vivado 2025.2 command-line compiler stopped before simulation with
VRFC10-1103: input ports need explicit net types under default_nettype none.

The six rtl/*.sv modules now declare module inputs as `input wire logic`
instead of `input logic`. `wire` states the port's net kind; `logic` states
its four-state data type. The source still uses `default_nettype none` to
catch undeclared signals. Output declarations and task arguments are not
changed. No instruction behaviour or datapath/control equation is changed.

Example:

```systemverilog
input wire logic [31:0] a_i,
```

Original folders and archives remain untouched. This full replacement
project is named rv32i_rtl_step3_portfix. All testbenches, instruction vectors,
C reference source and program generators are preserved unchanged.

## Run on your laptop

Extract into a new folder under ~/Documents/DOOMed, then run:

```bash
source /home/perlin/Documents/projects/2025.2/Vivado/settings64.sh
cd ~/Documents/DOOMed/rv32i_rtl_step3_portfix
./test.sh vivado
```

The user's previous tiny tb_core_demo already passed in Vivado at226ns.
The full command-line regression remains unverified in Vivado until the
corrected source passes there. Icarus regression results for this correction
are recorded separately under verification/portfix/. Old verification/step3/
logs describe the pre-fix package; they are retained as history.

No GUI, long Doom test, synthesis or GDS flow is part of the test command.

## Where the whole project stands

| Part | Current status |
|---|---|
| C instruction interpreter | RV32I base instructions implemented/tested; reference for RTL |
| Playable Doom | Previously run through the C interpreter with host display/keyboard services |
| RTL blocks | ALU, register file, immediate generator and decoder implemented |
| RTL core | Multi-cycle PC/IR/controller, memory handshake and instruction retirement implemented |
| RTL coverage | Arithmetic, all branches/jumps, load/store widths, nested calls, faults and waiting memory tested against C with Icarus |
| Vivado tiny core demo | User log confirms four retirements, RAM40→39 and explicit EBREAK trap at226ns |
| Vivado full regression | Previously stopped at input-port compilation; correction needs local verification |
| Compiled C guest on RTL | Next building milestone; not yet completed |
| Doom on RTL | Runtime/host-service connection and larger memory still to do |
| Performance work | Measure cycles/bottlenecks before selecting optimizations |
| Synthesis to GDSII | Future: technology, memory design, constraints, synthesis, physical design and signoff |

The working Doom interpreter and the RTL processor are two implementations
of instruction execution. Playing Doom through the C interpreter does not
establish Doom execution or FPS on this RTL core.

Recall: ALU calculates; registers retain values; PC selects the instruction;
the decoder interprets its bits; the controller sequences the clocks.
