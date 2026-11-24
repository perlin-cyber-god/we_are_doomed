# Actual correction checks

24 November 2026. Source change: explicit wire net kind on module input
declarations in all six rtl/*.sv files. No output/task declarations changed.
The original Step3 folder and ZIP were not edited.

Executed:

```bash
IVERILOG=/workspace/scratch/6bae74b1c81c/toolchains/rtl-step1-tools/local/usr/bin/iverilog \
IVERILOG_BASE=/workspace/scratch/6bae74b1c81c/toolchains/rtl-step1-tools/local/usr/lib/x86_64-linux-gnu/ivl \
VVP=/workspace/scratch/6bae74b1c81c/toolchains/rtl-step1-tools/local/usr/bin/vvp \
./test.sh iverilog
```

Exit0. The complete inherited block suite and72 core differential runs passed,
including201,438 core checks,1,034 retirements and5,874 counted clocks.
Both self-fetching demo and pending-request-reset test passed. Native C and
Icarus compilation logs had no diagnostics. Strict warning flags are retained
in the runners and compile logs.

Every module's input header was checked for explicit wire logic declarations.
Removing only the added wire tokens and ignoring whitespace reproduces the
original RTL text. All existing non-documentation/non-RTL files are identical
to the original archive, including every testbench, reference source and vector.

Vivado/xvlog are unavailable in this environment. The user's original tiny
GUI demo passed at226ns, while the original full command-line run stopped
with VRFC10-1103. The corrected Vivado route is supplied but not claimed passed.
No new architectural/compiled-guest/Doom/GUI/synthesis/GDS milestone was run.

Language reference for this declaration issue: Accellera-hosted industry
SystemVerilog design tutorial, page40 (PDF page41), explicitly contrasting
input logic with input wire logic under default_nettype none:
https://eda.org/images/resources/videos/systemverilog-design-tutorial-2015.pdf
