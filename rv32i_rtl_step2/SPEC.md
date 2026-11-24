# Step 2 contract

## Immediate generator

| format_i | Form | Value |
|---|---|---|
| 0 | None | zero |
| 1 | I | Signed instruction[31:20] |
| 2 | S | Signed instruction[31:25] joined with [11:7] |
| 3 | B | Signed branch byte offset, including its implicit low zero |
| 4 | U | instruction[31:12] followed by twelve zeros |
| 5 | J | Signed jump byte offset, including its implicit low zero |
| 6 | Shift | Zero-extended instruction[24:20] |
| 7 | Reserved | zero |

SRAI is important: its funct7 bits select the operation, not a large shift
amount. Its effective immediate output is shamt 0..31, not the raw 12-bit
I field. B and J outputs are already byte offsets: do not shift them again.

## Decoder class kind_o

| Value | Class | Register-write intent |
|---|---|---|
| 0 | Illegal | No |
| 1 | Arithmetic/logic/shift/compare | Yes |
| 2 | Load | Yes, after future memory response |
| 3 | Store | No |
| 4 | Branch | No |
| 5, 6 | JAL, JALR | Yes, future return-link value |
| 7, 8 | LUI, AUIPC | Yes |
| 9 | FENCE | No |
| 10, 11 | ECALL, EBREAK | No; require explicit trap/environment handling |

Raw rs1, rs2 and rd fields always expose instruction bits, even when those
bits are not register operands (for example an immediate occupies rs2's bit
positions in ADDI). uses_rs1/uses_rs2 specify whether they are meaningful
source IDs. reg_write_o is intent, including attempts to write x0. Actual
write authorization remains a separate decision.

alu_immediate_o selects the ALU's immediate B input for OP-IMM, LUI and AUIPC
in this arithmetic slice. It is not a general "instruction has an immediate"
flag. Other classes use their own future controller paths. ALU operation
numbers match Step 1. For other classes alu_op defaults to ADD; this alone
does not authorize their execution.

For loads/stores, memory_size=0/1/2 means byte/halfword/word. It is only
meaningful in those classes. load_unsigned distinguishes LBU/LHU from LB/LH;
LW is a full word and needs no sign-extension selector. branch_funct3
identifies BEQ/BNE/BLT/BGE/BLTU/BGEU, only in branch class.

Invalid encodings clear all semantic intent controls and immediate output;
raw register fields remain visible. Unsupported M/A/C/F/CSR instructions,
FENCE.I, WFI, MRET and reserved fields are rejected. FENCE with funct3=0 is
recognized while ignoring rd/rs1 and conservatively treating reserved fm as
FENCE, matching the preserved single-hart C-model policy. Recognition is not
execution: this slice does not silently run fences or trap instructions.

Address alignment, branch decisions/targets, memory bounds/access errors,
JALR bit clearing and exception delivery are future core responsibilities.
An otherwise valid branch with a two-byte offset is not an illegal opcode;
a future taken branch may instead fault for instruction-address alignment.

## Arithmetic slice

instruction_i, pc_i and commit_i are externally supplied. Supported classes
are 1, 7 and 8 only: all 19 RV32I arithmetic instructions plus LUI/AUIPC.
LUI selects ALU input A=0; AUIPC selects A=pc_i; others read rs1.
can_execute_o indicates this subset. write_enable_o requires no reset,
commit_i, an executable instruction, write intent and rd!=0.

The instruction is not captured in an IR yet and must remain stable across
the accepting clock edge. A self-updating instruction can produce a new
combinational ALU value immediately after its register write. The controller
must not keep commit high across extra edges unless repeat execution is
intended. Rejected/blocked classes do not update any register.

## Verification vector packing (checker detail)

Each line contains instruction_word, expected_controls and expected_imm.
The 37-bit control packing, MSB first, is:
kind[4], illegal[1], reg_write[1], uses_rs1[1], uses_rs2[1], alu_immediate[1],
alu_op[4], memory_size[2], load_unsigned[1], branch_funct3[3],
immediate_format[3], rs1[5], rs2[5], rd[5].

Expected constants are retained from the input instruction constructors, not
decoded back out using the RTL expressions. Literal words anchor encoders.
The C reference independently checks legal/illegal classification only; it
does not execute missing instructions for the RTL slice.
