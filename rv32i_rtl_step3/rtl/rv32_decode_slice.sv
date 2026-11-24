`timescale 1ns/1ps
`default_nettype none

// Teaching integration: external instruction/PC inputs and commit pulse.
// Executes ALU instructions, LUI and AUIPC ONLY. No fetch, stored PC, data
// memory, jumps, branch execution or trap controller exists in this slice.
module rv32_decode_slice (
    input  logic clk_i, reset_i, commit_i,
    input  logic [31:0] instruction_i, pc_i,
    output logic [3:0] kind_o, alu_op_o,
    output logic illegal_o, can_execute_o, write_enable_o,
    output logic [4:0] rs1_o, rs2_o, rd_o,
    output logic [31:0] immediate_o, rs1_value_o, rs2_value_o,
    output logic [31:0] alu_result_o
);
    logic [2:0] imm_format, branch_f3;
    logic use_rs1, use_rs2, write_intent, alu_immediate, load_unsigned;
    logic [1:0] memory_size;
    logic [31:0] alu_a, alu_b;
    logic alu_valid, eq, lts, ltu;
    rv32_decode u_decode (
        .instruction_i(instruction_i), .kind_o(kind_o), .illegal_o(illegal_o),
        .rs1_o(rs1_o), .rs2_o(rs2_o), .rd_o(rd_o), .immediate_o(immediate_o),
        .immediate_format_o(imm_format), .uses_rs1_o(use_rs1), .uses_rs2_o(use_rs2),
        .reg_write_o(write_intent), .alu_immediate_o(alu_immediate), .alu_op_o(alu_op_o),
        .memory_size_o(memory_size), .load_unsigned_o(load_unsigned),
        .branch_funct3_o(branch_f3)
    );
    assign alu_a = (kind_o == 4'd7) ? 32'b0 :
                   (kind_o == 4'd8) ? pc_i : rs1_value_o;
    assign alu_b = alu_immediate ? immediate_o : rs2_value_o;
    assign can_execute_o = !illegal_o && alu_valid &&
                           (kind_o == 4'd1 || kind_o == 4'd7 || kind_o == 4'd8);
    assign write_enable_o = !reset_i && commit_i && can_execute_o &&
                           write_intent && rd_o != 5'd0;
    rv32_regfile u_rf (
        .clk_i(clk_i), .reset_i(reset_i), .write_i(write_enable_o),
        .waddr_i(rd_o), .wdata_i(alu_result_o), .raddr_a_i(rs1_o), .raddr_b_i(rs2_o),
        .rdata_a_o(rs1_value_o), .rdata_b_o(rs2_value_o)
    );
    rv32_alu u_alu (
        .a_i(alu_a), .b_i(alu_b), .op_i(alu_op_o), .result_o(alu_result_o),
        .op_valid_o(alu_valid), .equal_o(eq),
        .less_signed_o(lts), .less_unsigned_o(ltu)
    );
endmodule
`default_nettype wire
