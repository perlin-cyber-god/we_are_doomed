`timescale 1ns/1ps
`default_nettype none

// First Vivado top for Step 2: real instruction WORDS drive the datapath.
// The testbench supplies the word/PC/commit. There is no instruction fetch yet.
module tb_decode_demo;
    logic clk=0;
    always #5 clk=~clk;
    logic reset,commit,illegal,can_execute,write_enable;
    logic [31:0] instruction,pc,imm,r1,r2,result;
    logic [4:0] rs1,rs2,rd;
    logic [3:0] kind,op;
    wire [31:0] x6_value=dut.u_rf.regs_q[6]; // Testbench-only observation.
    rv32_decode_slice dut (
        .clk_i(clk),.reset_i(reset),.commit_i(commit),.instruction_i(instruction),.pc_i(pc),
        .kind_o(kind),.alu_op_o(op),.illegal_o(illegal),.can_execute_o(can_execute),
        .write_enable_o(write_enable),.rs1_o(rs1),.rs2_o(rs2),.rd_o(rd),
        .immediate_o(imm),.rs1_value_o(r1),.rs2_value_o(r2),.alu_result_o(result)
    );
    initial begin
        string path;
        if ($value$plusargs("VCD=%s",path)) begin
            $dumpfile(path);$dumpvars(0,tb_decode_demo);
        end
        reset=1;commit=0;instruction=0;pc=32'h1000;
        @(posedge clk);#1;
        @(negedge clk);reset=0;commit=1;instruction=32'h02800313;
        #1;if(rd!==6 || imm!==40 || !write_enable)$fatal(1,"ADDI decode");
        @(posedge clk);#1;
        if(x6_value!==40)$fatal(1,"first instruction write");
        $display("DECODE DEMO word=02800313 -> ADDI x6,x0,40 -> x6=40");
        @(negedge clk);commit=0;instruction=32'hfff30313;
        #1;if(imm!==32'hffffffff || result!==39)$fatal(1,"negative immediate");
        repeat(2)begin @(posedge clk);#1;if(x6_value!==40)$fatal(1,"commit must gate write");end
        @(negedge clk);commit=1;
        @(posedge clk);#1;
        if(x6_value!==39)$fatal(1,"second instruction write");
        $display("DECODE DEMO word=fff30313 -> ADDI x6,x6,-1 -> x6=39");
        @(negedge clk);instruction=32'h00002303; // LW is recognized, not executed.
        #1;if(kind!==2 || illegal || can_execute || write_enable)$fatal(1,"unimplemented load guard");
        @(posedge clk);#1;
        if(x6_value!==39)$fatal(1,"load must not corrupt x6");
        @(negedge clk);instruction=32'h00000073;
        #1;if(kind!==10 || illegal || can_execute || write_enable)$fatal(1,"ECALL classification");
        @(posedge clk);#1;
        if(x6_value!==39)$fatal(1,"ECALL must not corrupt x6");
        @(negedge clk);instruction=32'h02630333;
        #1;if(!illegal || can_execute || write_enable)$fatal(1,"M must reject");
        @(posedge clk);#1;
        if(x6_value!==39)$fatal(1,"illegal must not corrupt x6");
        commit=0;
        $display("PASS RTL_DECODE_DEMO: instruction bits drive ALU/registers; blocked words cannot write");
        $finish;
    end
    initial begin #1000;$fatal(1,"decode demo watchdog");end
endmodule
`default_nettype wire
