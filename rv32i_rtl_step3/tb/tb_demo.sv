`timescale 1ns/1ps
`default_nettype none

// Start here in Vivado: no vector file and no complete CPU required.
module tb_demo;
    logic clk = 0;
    always #5 clk = ~clk;
    logic reset, write_enable;
    logic [31:0] write_data, x6_value, x0_value, alu_result;
    logic valid, eq, lts, ltu;
    string vcd_path;

    rv32_regfile u_rf (
        .clk_i(clk), .reset_i(reset), .write_i(write_enable),
        .waddr_i(5'd6), .wdata_i(write_data),
        .raddr_a_i(5'd6), .raddr_b_i(5'd0),
        .rdata_a_o(x6_value), .rdata_b_o(x0_value)
    );
    rv32_alu u_alu (
        .a_i(x6_value), .b_i(32'hffffffff), .op_i(4'h0),
        .result_o(alu_result), .op_valid_o(valid),
        .equal_o(eq), .less_signed_o(lts), .less_unsigned_o(ltu)
    );

    initial begin
        reset = 1; write_enable = 0; write_data = 0;
        if ($value$plusargs("VCD=%s", vcd_path)) begin
            $dumpfile(vcd_path);
            $dumpvars(0, tb_demo);
        end
        @(posedge clk); #1;
        if (x6_value !== 0 || x0_value !== 0) $fatal(1,"demo reset");
        @(negedge clk);
        reset = 0; write_enable = 1; write_data = 40;
        @(posedge clk); #1;
        if (x6_value !== 40 || alu_result !== 39) $fatal(1,"demo load 40");
        $display("DEMO t=%0t x6=40 ALU=39", $time);
        @(negedge clk); write_enable = 0;
        repeat (2) begin
            @(posedge clk); #1;
            if (x6_value !== 40) $fatal(1,"demo must hold 40");
        end
        @(negedge clk);
        write_data = alu_result; // snapshot 39; testbench chooses writeback
        write_enable = 1;
        @(posedge clk); #1;
        if (x6_value !== 39 || alu_result !== 38) $fatal(1,"demo writeback");
        $display("DEMO t=%0t x6=39 ALU=38 (combinational feedback)", $time);
        @(negedge clk); write_enable = 0;
        repeat (2) begin
            @(posedge clk); #1;
            if (x6_value !== 39 || x0_value !== 0) $fatal(1,"demo must hold 39");
        end
        $display("PASS RTL_DEMO: clocked write 40->39; disabled write holds state");
        $finish;
    end
    initial begin
        #1000;
        $fatal(1,"demo watchdog expired");
    end
endmodule
`default_nettype wire
