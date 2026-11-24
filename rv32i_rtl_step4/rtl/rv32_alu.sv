`timescale 1ns/1ps
`default_nettype none

// Combinational RV32I arithmetic. These op codes are OUR internal controls,
// not instruction opcode/funct3 encodings. A future decoder selects them.
module rv32_alu (
    input wire logic [31:0] a_i,
    input wire logic [31:0] b_i,
    input wire logic [3:0]  op_i,
    output logic [31:0] result_o,
    output logic        op_valid_o,
    output logic        equal_o,
    output logic        less_signed_o,
    output logic        less_unsigned_o
);
    localparam logic [3:0] OP_ADD  = 4'h0, OP_SUB  = 4'h1,
                           OP_AND  = 4'h2, OP_OR   = 4'h3,
                           OP_XOR  = 4'h4, OP_SLL  = 4'h5,
                           OP_SRL  = 4'h6, OP_SRA  = 4'h7,
                           OP_SLT  = 4'h8, OP_SLTU = 4'h9;
    logic [4:0] shift_amount;
    assign shift_amount   = b_i[4:0];
    assign equal_o        = (a_i == b_i);
    assign less_signed_o  = ($signed(a_i) < $signed(b_i));
    assign less_unsigned_o = (a_i < b_i);

    always_comb begin
        result_o   = 32'b0;
        op_valid_o = 1'b1;
        case (op_i)
            OP_ADD:  result_o = a_i + b_i; // 32-bit modulo arithmetic
            OP_SUB:  result_o = a_i - b_i;
            OP_AND:  result_o = a_i & b_i;
            OP_OR:   result_o = a_i | b_i;
            OP_XOR:  result_o = a_i ^ b_i;
            OP_SLL:  result_o = a_i << shift_amount;
            OP_SRL:  result_o = a_i >> shift_amount;
            OP_SRA:  result_o = $signed(a_i) >>> shift_amount;
            OP_SLT:  result_o = {31'b0, less_signed_o};
            OP_SLTU: result_o = {31'b0, less_unsigned_o};
            default: op_valid_o = 1'b0; // default result remains zero
        endcase
    end
endmodule
`default_nettype wire
