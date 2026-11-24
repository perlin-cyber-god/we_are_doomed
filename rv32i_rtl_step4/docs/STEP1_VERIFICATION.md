# Verification — RTL Step 1

**Actual result:** Icarus Verilog 11.0, zero compilation diagnostics with
`-g2012 -Wall`, demo passed and **18,563 checks, zero failures** in the main
suite. Its 7,344 ALU vectors were reproduced by the independent Python
generator. Python 3 and Bash syntax checks completed. Full console output,
compiler logs, simulator version, negative-test evidence and the demo VCD
are retained in verification/.

## Command executed here

The environment had no installed HDL simulator. An official Debian package
was downloaded and extracted only into a task-local tools directory; no
system package installation was performed. The local invocation was:

```bash
IVERILOG=/workspace/scratch/6bae74b1c81c/toolchains/rtl-step1-tools/local/usr/bin/iverilog \
IVERILOG_BASE=/workspace/scratch/6bae74b1c81c/toolchains/rtl-step1-tools/local/usr/lib/x86_64-linux-gnu/ivl \
VVP=/workspace/scratch/6bae74b1c81c/toolchains/rtl-step1-tools/local/usr/bin/vvp \
./test.sh iverilog
bash -n test.sh
```

The package URL was
https://deb.debian.org/debian/pool/main/i/iverilog/iverilog_11.0-1.1+b1_amd64.deb
and its downloaded SHA-256 was
`14d9d14dc8256fd89667fb68c3bd93f1c57310b9c95b33fb0d4b7719c887ae2f`.
The simulator binary/package is not redistributed in this project.

## Tests with real failure sensitivity

In temporary build-only copies, two deliberately introduced faults were
compiled and run through the same checker:

- Changing signed arithmetic right shift to logical right shift failed an
  independently generated vector for 0x80000000 shifted by one.
- Ignoring register write-enable failed the disabled-write check.

Each mutant exited with failure; neither mutation is in rtl/. See
verification/negative-tests.txt. The final normal sources subsequently
passed again after adding the standalone demo. This is directed block
verification, not a formal proof or ISA certification.

## What was not run

Vivado/xvlog/xelab/xsim were not available here, so the supplied Vivado route
has not been executed. No GUI was opened. No Doom, complete processor,
physical synthesis, formal proof, place-and-route or GDSII generation was
run. Functional synthesizable-style RTL does not establish physical timing,
area or power. FPGA/ASIC synthesis portability must be checked in those tools.

## Demo waveform

The VCD and PNG come from the actual short headless simulation. x6 becomes
40 at the 15 ns rising edge. Its combinational ALU output is then 39. At the
45 ns rising edge, an enabled write captures 39; the ALU subsequently
calculates 38, and x6 holds 39 after writes are disabled. Simulation ends at
66 ns. Initial X values before the first synchronous reset edge are expected.

Older Doom sources, compiled guests, archives and the earlier tiny Verilog
teaching prototype remain in their own folders and were not modified.
