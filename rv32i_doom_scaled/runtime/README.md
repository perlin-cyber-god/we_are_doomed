# Milestone 8: a tested runtime foundation

The CPU and square platform are unchanged. This is a separate bare-metal guest
that tests memory services, a small heap, read-only WAD access and software
arithmetic on the same RV32I interpreter. It is not the full Doom port or a
complete libc. Run `./test.sh` from the project root.

## What is implemented

| File | Capability | Important contract |
|---|---|---|
| memory.c | memcpy, memmove, memset, memcmp, strlen, strcmp | Standard valid-buffer/string preconditions; memcpy forbids overlap, memmove supports it |
| memory.c | rt_strlcpy | Bounded copy, NUL terminates if capacity > 0, returns full source length |
| heap.c | malloc/free/calloc/realloc | 16-byte aligned first-fit arena, split blocks, coalesce on free |
| assets.c | rt_read_at | Immutable memory-backed file; random reads, short reads at EOF |
| assets.c | rt_wad_open/find | Validate IWAD/PWAD header/directory/lump spans; exact eight-byte names; later duplicate wins |
| math.c | Checked Q16.16 multiply/divide | bool result; invalid/overflow leaves output unchanged |
| linked libgcc | Integer multiply/divide/remainder helpers | Software sequences compiled for RV32I/ILP32, no M instructions |

The memory/string functions are exported with familiar C names, but this does
not supply standard library headers or every libc function. The asset API is
a foundation for a future WAD backend, not a replacement fopen implementation
already wired into Doom. Unlike the engine's case-normalized lookup layer,
our small lookup accepts an exact eight-byte on-disk name.

## Heap rules

Initialize one valid, writable, 16-aligned arena with rt_heap_init. Blocks have
a 16-byte header and a payload rounded up to 16 bytes. Free blocks are searched
in address order; splitting avoids unusable fragments, and freeing joins
adjacent free blocks. Invalid/double frees increment a diagnostic counter.
Those cases are intentionally tested: diagnosed_invalid_frees=2 is expected,
not an unexplained allocator failure. There is no concurrency or hostile-heap
corruption recovery guarantee. Do not reinitialize while allocations are live.

malloc(0) and zero-sized calloc return NULL. calloc rejects size overflow and
zeros its allocation. realloc(NULL,n) allocates, realloc(p,0) frees, shrinking
retains capacity, and failed growth preserves the original block. Ordinary
valid C buffer preconditions still apply to memory/string functions. This
allocator is an educational general arena; Doom's Z_* zone allocator remains
an engine layer of its own.

## Asset fixture and parsing

fixture.wad is our own 55-byte synthetic PWAD, not Doom game data. It has two
lumps: GREETING contains HELLO! plus NUL; NUMBERS contains 3,5,7,9, whose sum
is 24. verify.py independently constructs the exact expected file bytes.
Directory fields are read bytewise, so unaligned on-disk fields work. A parser
error leaves the output view unchanged. All directory entries are validated
before exposing a view. Zero-size markers at EOF are accepted; a nonempty
span past EOF is rejected. The file must stay immutable while the view is used.

The host only loads the binary and fixture bytes. Guest instructions parse the
header, find entries, read payloads and calculate the sum. Host mutations test
wrong magic, truncation, bad directory and changed payload. Each is rejected
by the guest with a FAIL signature, not an accidental CPU access fault.

## Arithmetic without adding a multiplier

The cross linker selects GCC's rv32i/ilp32 libgcc.a. For example, it supplies
__mulsi3, __muldi3, __divdi3 and remainder helpers. These are guest instruction
sequences, not host arithmetic shortcuts or interpreter special cases. The
interpreter executes the helpers normally. The compiler runtime notice and
exception are included in GCC_COPYRIGHT.txt; the compiler itself is not bundled.

Our checked fixed helpers use 16 fractional bits. Multiplication rounds negative
fractional products as an arithmetic right shift would; division truncates
toward zero. Overflow and divide-by-zero are rejected. These APIs are **not
drop-in implementations of Doom's overflow/saturation behaviour**. A future
adapter needs explicit engine-compatible semantics. No C signed left shift of
a negative value is used in these helpers.

## Separate runtime-test memory map: 512 KiB

| Inclusive bytes | Reservation |
|---|---|
| 0x00000–0x0FFFF | Text, data and BSS; binary entry 0 |
| 0x10000–0x10FFF | Completion report and asset descriptor |
| 0x20000–0x20FFF | Up to 4 KiB of read-only-by-convention asset bytes |
| 0x30000–0x6FFFF | 256 KiB heap, initial payload capacity 262,128 bytes |
| 0x7F000–0x7FFFF | 4 KiB stack; initial sp=0x80000 |

Other gaps are reserved. There is no MPU enforcing read-only regions. At
0x10080 the host writes the asset base; at 0x10084 it writes asset length.
The guest startup sets sp/gp and clears BSS; it initializes its own heap.
Host RAM begins poisoned. All addresses are guest addresses, never host
pointers. These test-map sizes are not a claim of sufficient memory for Doom.

## Completion protocol

At 0x10000: word 0 is zero while running and DONE/FAIL when published last.
Words 1..5 are assertion count, failures, asset checksum, available heap bytes
and intentional invalid-free diagnostics. Existing rv_run_signature() watches
this block with a 10,000,000-instruction host budget. ECALL/EBREAK remain traps.
The runtime guest does not use the square demo's ECALL services or window.

The linked ELF and raw binary are both supplied. The runner loads only the
raw binary; check_elf.py inspects ELF32 metadata and executable sections for
non-32-bit instructions, non-base opcode families and M instructions. Padding
words are distinguished from code. Dynamic guest tests verify the paths used;
this is not official architectural certification.

## Build and run

```bash
./test.sh
# Run the runtime component alone:
./runtime/build.sh
python3 runtime/verify.py
# The square still works independently:
./run_square.sh
```

GCC and Python 3 suffice for the hash-checked prebuilt route. With a suitable
RISC-V GNU compiler on PATH, all runtime sources rebuild with strict warnings,
RV32I/ILP32 flags and -lgcc. No compiler is installed by the scripts. The
optional older Clang route is not implemented for this runtime build script.

Next work: complete the selected port's actual dependency closure, supply its
formatting/file interfaces, adapt the WAD backend, match fixed-point overflow
semantics and choose a larger memory map before attempting the engine.
