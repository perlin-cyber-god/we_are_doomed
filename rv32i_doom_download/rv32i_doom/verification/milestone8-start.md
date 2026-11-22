# Runtime milestone: memory, WAD data and software maths

Extract this folder beside your earlier projects and run:

```bash
cd ~/Documents/DOOMed/rv32i_runtime
./test.sh
```

The suite retains all previous CPU, compiled-C and square checks, then runs
a new RV32I guest that tests memory/string functions, allocation/free/realloc,
a synthetic WAD and integer/fixed-point maths. No cross compiler is required
for the included hash-checked prebuilt guests. GCC and Python 3 are required
for the host. With a RISC-V GNU compiler installed, guests rebuild locally.

Expected runtime summary for the included fixture:

```text
checks=191 failures=0 asset_sum=24 heap_free=262128 diagnosed_invalid_frees=2
PASS interpreted runtime guest
```

The two invalid-free diagnostics are intentional tests of invalid/double-free
handling. They are not failed assertions. Four changed/malformed fixture tests
also correctly produce guest FAIL and are verified by the host test script.

This milestone has no new window. The working square is still available:

```bash
./run_square.sh
```

Read runtime/README.md for implementation contracts and runtime/DOOM_AUDIT.md
for the inspected source requirements. See VERIFICATION.md for actual results.
The new memory map and 256 KiB heap are runtime tests, not enough RAM for the
inspected Doom port's default zone request. This is not a full libc or Doom port.
