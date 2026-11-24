`timescale 1ns/1ps
`default_nettype none

// Format selection is an INTERNAL control, not instruction opcode bits.
// 0 none, 1 I, 2 S, 3 B, 4 U, 5 J, 6 shift amount; 7 reserved -> zero.
module rv32_immediate (
    input wire logic [31:0] instruction_i,
    input wire logic [2:0]  format_i,
    output logic [31:0] immediate_o
);
    assign immediate_o =
        (format_i == 3'd1) ? {{20{instruction_i[31]}}, instruction_i[31:20]} :
        (format_i == 3'd2) ? {{20{instruction_i[31]}}, instruction_i[31:25], instruction_i[11:7]} :
        (format_i == 3'd3) ? {{19{instruction_i[31]}}, instruction_i[31], instruction_i[7],
                              instruction_i[30:25], instruction_i[11:8], 1'b0} :
        (format_i == 3'd4) ? {instruction_i[31:12], 12'b0} :
        (format_i == 3'd5) ? {{11{instruction_i[31]}}, instruction_i[31], instruction_i[19:12],
                              instruction_i[20], instruction_i[30:21], 1'b0} :
        (format_i == 3'd6) ? {27'b0, instruction_i[24:20]} : 32'b0;
endmodule
`default_nettype wire
