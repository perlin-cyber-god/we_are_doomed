# Lesson 23: first integrated teaching core

Contains regfile.v, alu.v, tiny_core.v, tb.v and run_vivado.tcl.
Supports only ADD, ADDI, LBU and SB. No branches, full ISA, peripherals,
caches or Doom executable yet. Unrecognized instructions enter a debug trap.
The four-instruction program is encoded in the testbench; the core fetches
and decodes it. The testbench never performs the CPU's register updates.

## Vivado
Run from this directory:

    vivado -mode batch -source run_vivado.tcl

Or add the three design .v files to Design Sources, tb.v to Simulation Sources,
select tb as simulation top, and run behavioural simulation / Run All.
No target board is required for this behavioural test. If your installation
requires a target part during project creation, select an available part;
this lesson does not claim synthesis or timing validation for that part.

Expected outcome is a PASS line followed by simulation completion.
Any assertion failure must be investigated before proceeding.
Repeat with WAIT=3 SOURCE=103 DEST=201 in the Tcl generic setting.
SOURCE may be 100..103; DEST may be 200..203; WAIT must be >= 0.
These variations exercise byte lanes as well as delayed memory responses.

## Interface and checks
Unified aligned-word read bus. mem_addr is a byte address; mem_rdata is the
32-bit little-endian word starting at mem_addr & ~3. Stores use lane strobes
and lane-positioned data. Accepted transfer: valid && ready at a rising edge.
CPU holds the request stable until accepted. Memory accepts writes once.

Checks: registers x1/x2/x3, final PC 16, copied byte 5, neighbour preservation,
store lane/address, one store, request stability, timeout, unexpected trap.
Register-file x0 behaviour and the unused ALU operations need separate unit
tests. This is not an exhaustive CPU verification suite.

## Clock schedule
The simple test memory registers each request before acknowledging it,
adding WAIT+1 wait intervals to each FETCH/MEMORY state. Six transfers occur
(four instruction reads, one data read, one store). The predicted total is
17 + 6*(WAIT+1) clocks: 23 with WAIT=0 and 41 with WAIT=3. These counts are
predictions, not recorded simulator measurements. The testbench stops after
checking the completed store, before another instruction can be accepted.

## Validation status
Source and expected behaviour prepared; no RTL simulator was available in
the authoring environment. Vivado simulation has NOT been run. Treat this
as the first runnable exercise pending verification, not a validated CPU.
