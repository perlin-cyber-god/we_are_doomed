# Program images

core_demo.S explains the demo. demo.bin and demo.mem contain the matching
2048-byte test image (other bytes are A5 sentinels). No cross toolchain is
required: scripts/make_core_programs.py encodes instructions into images,
checking several literal instruction anchors, and regenerates all 36 cases
under build/core/. The self-contained Vivado demo embeds its own small ROM
and RAM; it does not read these files.

The differential bench uses `$readmemh` on **byte** images (one two-digit
hex byte per line). The first four bytes of demo.mem are 93 02 00 20:
little-endian instruction 20000293 at byte address zero. A word-oriented
memory loader would require a different file format.

The unchanged C CPU executes demo.bin to produce a trace; the RTL executes
the same bytes from demo.mem. Neither executes the explanatory assembly
as host code. Completion is an expected explicit trap (usually EBREAK),
not an invented HALT instruction. The infinite-loop test instead compares
a 20-retirement prefix, explicitly labelled a host budget, not completion.

For these small tests, code occupies the low addresses; memory_widths uses
data at1536; the nested-call test starts sp=2016 and saves ra at2012.
No general-purpose linker/memory map or Doom runtime is provided yet.
