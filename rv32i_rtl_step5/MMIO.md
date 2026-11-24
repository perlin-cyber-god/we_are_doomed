# Step 5 platform and verification contract

## CPU and bus

The previous unprivileged RV32I core is unchanged. Its completion-style port
connects to rv32_mmio, which either forwards a RAM request or completes a
device/unmapped request. No queued acceptance response or multiple outstanding
transaction is introduced. The requester holds its address, data and masks
stable until an edge with valid and done. Only that edge may apply a side effect.

Core data alignment checks happen before requesting the bus. Misaligned
instruction/data faults therefore do not reach device logic. Device-width
restrictions are additional platform access rules, not new ISA encodings.

## Address map and access policy

Default RAM_BYTES=4000 hex (16384 bytes), IO_WAIT_CYCLES=0 and timer reset0.
RAM_BYTES must not overlap the device map; tests use the default capacity.
IO_WAIT_CYCLES is a nonnegative constant, tested at 0 and3.

| Address | Read policy | Write policy |
|---|---|---|
| RAM addresses below RAM_BYTES | Existing byte/half/word operations and instruction fetch | Existing byte/half/word stores |
| 10000000, TX_DATA | Access error | SB at exactly this address, lane mask0001 |
| 10000004, TX_STATUS | LW, mask1111; bit0=tx_ready, other bits0 | Access error |
| 10000010, TIMER | LW, mask1111; current32-bit tick counter | Access error |
| Other non-RAM address | Access error | Access error |

All addresses are hexadecimal; the map is a project platform choice, not a
standard RISC-V peripheral layout. All instruction fetches from devices or
unmapped space fail. Wrong direction, size or lane masks fail without console
output or RAM modification. Failed reads return zero response data.

The RAM backend still validates selected lanes before any write. The router
forwards aligned valid-shaped RAM requests and propagates its response/error.
Read/write masks must be nonzero on exactly one direction; malformed MMIO
shapes return errors. A load into x0 still performs/checks the access.

## Console stream

tx_valid_o/tx_data_o/tx_ready_i form a byte stream, not a serial UART.
tx_valid stays asserted while a legitimate eligible TX store waits for ready;
the low8 data bits remain stable because the core holds the request. A rising
edge with valid and ready accepts exactly one byte and completes that CPU
store. No character is emitted on waiting clocks or rejected accesses.

The console status reports current readiness only; a subsequent write must
still handshake. The testbench varies readiness in delayed mode. The external
sink must honor this protocol and run in the same clock domain. No clock-domain
crossing, buffering, serial baud generator, interrupt or hardware timeout exists.

IO_WAIT_CYCLES adds that many waiting clocks before a device access can
complete; a console store can wait longer for the sink. RAM delay belongs
to the separate backend. Default mode has no artificial IO/RAM waiting.

## Timer/reset/order

The timer increments once on every nonreset rising edge, modulo2^32, whether
the CPU is fetching, computing, waiting or stopped in a trap. A load samples
the counter's pre-increment value at its successful completion edge. It is
not snapshotted at request launch. Response data may change while waiting;
only the completion edge is significant.

Synchronous active-high reset sets the timer/reset wait counter and suppresses
RAM requests, console valid and CPU completion while asserted. A pending
unaccepted console byte is canceled; an already accepted byte cannot be undone.
The device unit tests explicitly cover cancellation and restart.

The CPU permits one outstanding access and waits for it to complete. No store
buffer/cache/other hart exists. Its FENCE no-op policy therefore remains valid
for the introduced synchronous stream/counter platform: earlier accesses have
already completed before the next instruction runs. Volatile is not a general
compiler memory barrier; the guest's explicit FENCE has a memory clobber.

ECALL/EBREAK remain explicit stopping traps. Existing trap status numbers and
alignment/atomic-error rules remain in SPEC.md; no privileged mcause claim.

## Guest and completion

io_guest/start.S and linker.ld preserve Step4's all-RAM startup/memory layout.
The new guest initializes BSS, prints the 26-byte greeting, reads the timer,
polls until at least64 ticks have elapsed and publishes the following signature
at 2000 hex. Startup writes status last then EBREAK at _guest_stop.

| Word index | Field |
|---:|---|
| 0 | DONE/FAIL, as before |
| 1 | Accepted byte count,26 |
| 2 | Byte sum,2023 |
| 3 | Timer before printing |
| 4 | Timer baseline for polling |
| 5 | Last timer value used to leave polling |
| 6 | Unsigned word5−word4, at least64 |
| 7 | Guest completion flag1 |

Only the first32 of the reserved64 signature bytes may change. Code/guard
bytes remain intact, and sp is restored to4000 at the final EBREAK. Timer
values and loop iteration counts can differ across memory/readiness modes.

## Architectural reference with external-input replay

The original reference/rv32i.c remains byte-identical. It continues executing
every ordinary instruction. A separate mmio_reference.c adapter supplies
this platform's high-address loads/stores/fetch errors, validates widths and
directions before committing, and consumes the RTL event stream in order.

Every completed non-RAM request records:
`I address read_mask write_mask write_data read_data error instruction_flag`.
Masks and data use aligned bus lanes. Timer/status values are treated as
external inputs; the reference validates request/address/order/write data and
access-error policy, then supplies those inputs to its load operation. A
rejected instruction changes no PC/register/RAM state. Extra/missing/reordered
events reject replay. x0 loads still consume their external event.

RTL and reference traces use the previous R/T/B/M format: every retirement,
terminal trap/budget and final byte-exact RAM, with a zero hash marker.
Python compares all numeric fields, including all32 registers. The actual
console file must match the independently specified greeting exactly.

Replay **does not independently predict device time**. Separate unit/integrated
checks model the counter on every clock and verify completion-time read values,
status readiness, wrap and reset. This is functional device validation, not a
formal proof or cycle-accurate reference microarchitecture.

## New suite

- Four compiled normal runs: O0/O2 with IO wait0 or3; delayed mode also varies
  RAM latency and console readiness.
- Thirteen access-policy programs, including wrong widths/directions, unknown
  addresses, forbidden instruction fetch, high-address bounds, alignment and
  legal/illegal loads into x0. Twelve deliberately fault; one valid x0 timer
  load reaches EBREAK. All have no console/RAM side effect.
- One20-attempt budget prefix, checked as incomplete rather than guest success.
- Two direct device benches, wait0/wait3, covering backpressure, cancellation,
  malformed masks, RAM error forwarding and independent timer wrap/reset.

The CPU bench has a 200000-cycle watchdog and a maximum 20000-attempt budget;
ordinary runs use10000. Every external command has a 60-second timeout. A
permanently unready sink can stall the hardware; verification limits only
bound its simulation. No GUI, host keyboard/framebuffer or Doom run is added.

Primary references for language/ISA rules (the device map itself is our design):
- https://gcc.gnu.org/onlinedocs/gcc/Volatiles.html
- https://docs.riscv.org/reference/isa/v20260120/unpriv/rv32.html
