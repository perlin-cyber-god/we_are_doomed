`timescale 1ns/1ps
`default_nettype none

// Two combinational read ports, one rising-edge write port.
// Active-high synchronous reset clears x1..x31. x0 is never stored.
module rv32_regfile (
    input  logic        clk_i,
    input  logic        reset_i,
    input  logic        write_i,
    input  logic [4:0]  waddr_i,
    input  logic [31:0] wdata_i,
    input  logic [4:0]  raddr_a_i,
    input  logic [4:0]  raddr_b_i,
    output logic [31:0] rdata_a_o,
    output logic [31:0] rdata_b_o
);
    logic [31:0] regs_q [1:31];
    assign rdata_a_o = (raddr_a_i == 5'd0) ? 32'b0 : regs_q[raddr_a_i];
    assign rdata_b_o = (raddr_b_i == 5'd0) ? 32'b0 : regs_q[raddr_b_i];

    always_ff @(posedge clk_i) begin
        if (reset_i) begin
            for (int index = 1; index < 32; index++)
                regs_q[index] <= 32'b0;
        end else if (write_i && waddr_i != 5'd0) begin
            regs_q[waddr_i] <= wdata_i;
        end
    end
endmodule
`default_nettype wire
