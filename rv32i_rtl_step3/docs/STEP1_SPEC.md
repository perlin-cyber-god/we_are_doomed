# Exact block contract

## ALU

Two 32-bit inputs, a four-bit internal operation selector, a 32-bit result,
valid-control indicator and three comparison outputs. Combinational only:
there is no clock, reset or stored state. No multiplication/division.

| op_i | Operation | Result |
|---|---|---|
| 0 | ADD | a + b modulo 2^32 |
| 1 | SUB | a - b modulo 2^32 |
| 2 | AND | a & b |
| 3 | OR | a \| b |
| 4 | XOR | a ^ b |
| 5 | SLL | Left shift by b[4:0] |
| 6 | SRL | Logical right shift by b[4:0] |
| 7 | SRA | Signed arithmetic right shift by b[4:0] |
| 8 | SLT | 32-bit 0 or 1, signed a < b |
| 9 | SLTU | 32-bit 0 or 1, unsigned a < b |
| A..F | Invalid control | result=0, op_valid=0 |

For operations 0..9, op_valid=1. Comparison outputs always independently
compare a and b, regardless of the selected operation. Invalid ALU control
is not a complete illegal-instruction trap; the future decoder/controller
must validate instructions. These op numbers are our internal encoding,
not RISC-V machine instruction opcodes.

Known 0/1 inputs are the functional contract. Unknown/uninitialized SV inputs
are not given architectural meaning. No overflow flag or saturation is used.

## Register file

- x0 always reads zero and has no storage element in this implementation.
- x1..x31 store 32 bits each; initially unspecified until reset is clocked.
- Two independent combinational read ports select registers with 5-bit IDs.
- One write port captures wdata at the rising edge when write=1 and waddr!=0.
- Active-high **synchronous** reset clears all stored registers at a rising
  edge and takes priority over writes. Asserting reset between edges does
  not immediately clear them.
- No explicit write-through bypass: before an edge reads show old data;
  after a write and simulator updates settle, reads show the new data.
- Testbench drives control at falling edges and samples after rising-edge
  nonblocking updates, avoiding races.

This reset choice matches the C model's initial zeroed registers, but RISC-V
does not generally require every nonzero register to reset to zero. Reset
cost and register-file mapping must be reviewed before ASIC physical design.
No FPGA vendor primitives or testbench delays appear in the synthesizable RTL.

## Verification coverage

Python independently computes expected 32-bit results using integer arithmetic.
The checked-in vector file covers corner operands, all low-five-bit shift
amounts (including aliases 32..63), signed/unsigned boundaries, seeded random
operands, and all invalid internal controls. HDL checks every result and
comparison output. Register tests scan both ports/all addresses and check
pre-edge stability, post-edge changes, write enable, x0 and reset priority.

This is block-level verification. It is not full CPU/ISA certification,
formal proof, a synthesis run, an achievable 100 MHz claim, or a GDS layout.
