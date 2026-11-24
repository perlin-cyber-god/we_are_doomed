`timescale 1ns/1ps
`default_nettype none

// Pure RV32I decoding. No PC, clock, RAM request or architectural writes.
module rv32_decode (
    input  logic [31:0] instruction_i,
    output logic [3:0]  kind_o,
    output logic        illegal_o,
    output logic [4:0]  rs1_o, rs2_o, rd_o,
    output logic [31:0] immediate_o,
    output logic [2:0]  immediate_format_o,
    output logic        uses_rs1_o, uses_rs2_o,
    output logic        reg_write_o,
    output logic        alu_immediate_o,
    output logic [3:0]  alu_op_o,
    output logic [1:0]  memory_size_o,
    output logic        load_unsigned_o,
    output logic [2:0]  branch_funct3_o
);
    // kind: 0 illegal; 1 arithmetic; 2 load; 3 store; 4 branch; 5 JAL;
    // 6 JALR; 7 LUI; 8 AUIPC; 9 FENCE; 10 ECALL; 11 EBREAK.
    localparam logic [3:0] K_BAD=0, K_ALU=1, K_LOAD=2, K_STORE=3,
                           K_BRANCH=4, K_JAL=5, K_JALR=6, K_LUI=7,
                           K_AUIPC=8, K_FENCE=9, K_ECALL=10, K_EBREAK=11;
    localparam logic [2:0] IMM_NONE=0, IMM_I=1, IMM_S=2, IMM_B=3,
                           IMM_U=4, IMM_J=5, IMM_SHIFT=6;
    logic [6:0] opcode, funct7;
    logic [2:0] funct3;
    assign opcode = instruction_i[6:0];
    assign funct3 = instruction_i[14:12];
    assign funct7 = instruction_i[31:25];
    // Raw fields stay observable even if the instruction is rejected.
    assign rs1_o = instruction_i[19:15];
    assign rs2_o = instruction_i[24:20];
    assign rd_o  = instruction_i[11:7];
    assign illegal_o = (kind_o == K_BAD);
    rv32_immediate u_immediate (
        .instruction_i(instruction_i), .format_i(immediate_format_o),
        .immediate_o(immediate_o)
    );

    always_comb begin
        kind_o = K_BAD;
        immediate_format_o = IMM_NONE;
        alu_op_o = 4'd0;
        memory_size_o = 2'd0;
        load_unsigned_o = 1'b0;
        branch_funct3_o = 3'd0;
        alu_immediate_o = 1'b0;
        uses_rs1_o = 1'b0; uses_rs2_o = 1'b0; reg_write_o = 1'b0;
        case (opcode)
            7'h13: begin // OP-IMM
                kind_o = K_ALU; immediate_format_o = IMM_I;
                alu_immediate_o = 1'b1;
                case (funct3)
                    0: alu_op_o = 0; // ADDI
                    2: alu_op_o = 8; // SLTI
                    3: alu_op_o = 9; // SLTIU
                    4: alu_op_o = 4; // XORI
                    6: alu_op_o = 3; // ORI
                    7: alu_op_o = 2; // ANDI
                    1: begin
                        immediate_format_o = IMM_SHIFT; alu_op_o = 5;
                        if (funct7 != 0) kind_o = K_BAD; // SLLI
                    end
                    5: begin
                        immediate_format_o = IMM_SHIFT;
                        if (funct7 == 0) alu_op_o = 6; // SRLI
                        else if (funct7 == 7'h20) alu_op_o = 7; // SRAI
                        else kind_o = K_BAD;
                    end
                endcase
            end
            7'h33: begin // OP: M and reserved funct7 encodings are rejected.
                kind_o = K_ALU;
                case (funct3)
                    0: begin
                        if (funct7 == 0) alu_op_o = 0;
                        else if (funct7 == 7'h20) alu_op_o = 1;
                        else kind_o = K_BAD;
                    end
                    1: begin alu_op_o = 5; if (funct7 != 0) kind_o = K_BAD; end
                    2: begin alu_op_o = 8; if (funct7 != 0) kind_o = K_BAD; end
                    3: begin alu_op_o = 9; if (funct7 != 0) kind_o = K_BAD; end
                    4: begin alu_op_o = 4; if (funct7 != 0) kind_o = K_BAD; end
                    5: begin
                        if (funct7 == 0) alu_op_o = 6;
                        else if (funct7 == 7'h20) alu_op_o = 7;
                        else kind_o = K_BAD;
                    end
                    6: begin alu_op_o = 3; if (funct7 != 0) kind_o = K_BAD; end
                    7: begin alu_op_o = 2; if (funct7 != 0) kind_o = K_BAD; end
                endcase
            end
            7'h03: begin
                kind_o = K_LOAD; immediate_format_o = IMM_I;
                case (funct3)
                    0: memory_size_o = 0; // LB
                    1: memory_size_o = 1; // LH
                    2: memory_size_o = 2; // LW
                    4: begin memory_size_o = 0; load_unsigned_o = 1; end
                    5: begin memory_size_o = 1; load_unsigned_o = 1; end
                    default: kind_o = K_BAD;
                endcase
            end
            7'h23: begin
                kind_o = K_STORE; immediate_format_o = IMM_S;
                case (funct3)
                    0: memory_size_o = 0;
                    1: memory_size_o = 1;
                    2: memory_size_o = 2;
                    default: kind_o = K_BAD;
                endcase
            end
            7'h63: begin
                kind_o = K_BRANCH; immediate_format_o = IMM_B;
                branch_funct3_o = funct3;
                case (funct3)
                    0,1,4,5,6,7: begin end
                    default: kind_o = K_BAD;
                endcase
            end
            7'h6f: begin kind_o = K_JAL; immediate_format_o = IMM_J; end
            7'h67: begin
                kind_o = K_JALR; immediate_format_o = IMM_I;
                if (funct3 != 0) kind_o = K_BAD;
            end
            7'h37: begin
                kind_o = K_LUI; immediate_format_o = IMM_U; alu_immediate_o = 1;
            end
            7'h17: begin
                kind_o = K_AUIPC; immediate_format_o = IMM_U; alu_immediate_o = 1;
            end
            7'h0f: begin
                // Ignore rd/rs1 and conservatively recognize reserved fm as
                // FENCE too, matching our single-hart C reference policy.
                if (funct3 == 0) kind_o = K_FENCE; // FENCE.I is not RV32I.
            end
            7'h73: begin
                if (instruction_i == 32'h00000073) kind_o = K_ECALL;
                else if (instruction_i == 32'h00100073) kind_o = K_EBREAK;
            end
            default: begin end
        endcase

        // Derive semantic intent. Actual execution/commit is a later block.
        uses_rs1_o = (kind_o == K_ALU || kind_o == K_LOAD || kind_o == K_STORE ||
                      kind_o == K_BRANCH || kind_o == K_JALR);
        uses_rs2_o = ((kind_o == K_ALU && opcode == 7'h33) ||
                      kind_o == K_STORE || kind_o == K_BRANCH);
        reg_write_o = (kind_o == K_ALU || kind_o == K_LOAD || kind_o == K_JAL ||
                       kind_o == K_JALR || kind_o == K_LUI || kind_o == K_AUIPC);
        if (kind_o == K_BAD) begin
            // Prevent a partly decoded invalid instruction from leaking intent.
            immediate_format_o = IMM_NONE; alu_op_o = 0; alu_immediate_o = 0;
            memory_size_o = 0; load_unsigned_o = 0; branch_funct3_o = 0;
        end
    end
endmodule
`default_nettype wire
