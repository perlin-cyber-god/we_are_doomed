`timescale 1ns/1ps
`default_nettype none
module tb_decode_slice;
    logic clk=0;
    always #5 clk=~clk;
    logic reset,commit,illegal,can_execute,write_enable;
    logic [31:0] instruction,pc,imm,r1,r2,result;
    logic [4:0] rs1,rs2,rd;
    logic [3:0] kind,op;
    logic [31:0] model[0:31];
    integer checks=0,issued=0;
    rv32_decode_slice dut (
        .clk_i(clk),.reset_i(reset),.commit_i(commit),.instruction_i(instruction),.pc_i(pc),
        .kind_o(kind),.alu_op_o(op),.illegal_o(illegal),.can_execute_o(can_execute),
        .write_enable_o(write_enable),.rs1_o(rs1),.rs2_o(rs2),.rd_o(rd),
        .immediate_o(imm),.rs1_value_o(r1),.rs2_value_o(r2),.alu_result_o(result)
    );
    task automatic scan;
        // Read-only probes: ADDI x0,xN,0 with commit disabled. No added RF port.
        commit=0;
        for (int index=0;index<32;index++) begin
            instruction=(32'(index)<<15)|32'h00000013;
            #1;checks++;
            if (r1 !== model[index] || r2 !== 0 || write_enable !== 0)
                $fatal(1,"slice scan x%0d got=%08x expected=%08x",index,r1,model[index]);
        end
    endtask
    task automatic issue(input logic [31:0] word,input logic [31:0] expected_value,
                         input logic [3:0] expected_kind,input logic expected_can,
                         input logic expected_illegal,input logic apply_commit);
        @(negedge clk);
        instruction=word;commit=apply_commit;
        #1;checks++;
        if ({kind,can_execute,illegal} !== {expected_kind,expected_can,expected_illegal})
            $fatal(1,"slice word=%08x class/can/illegal mismatch",word);
        if (write_enable !== (apply_commit && expected_can && rd!=0))
            $fatal(1,"slice word=%08x write-enable gating",word);
        if (r1 !== model[rs1] || r2 !== model[rs2])
            $fatal(1,"slice source register selection");
        if (expected_can && result !== expected_value)
            $fatal(1,"slice word=%08x result=%08x expected=%08x",word,result,expected_value);
        @(posedge clk);#1;
        if (apply_commit && expected_can && rd!=0) model[rd]=expected_value;
        issued++;
        scan();
    endtask
    initial begin
        reset=1;commit=0;instruction=0;pc=32'h1000;
        @(posedge clk);#1;
        for (int i=0;i<32;i++)model[i]=0;
        reset=0;scan();
        issue(32'h02800313,40,1,1,0,1); // ADDI x6,x0,40
        issue(32'hfff30313,39,1,1,0,0); // calculation without commit: x6 stays 40
        issue(32'hfff30313,39,1,1,0,1);
        issue(32'h006303b3,78,1,1,0,1); // ADD x7,x6,x6
        issue(32'h40638433,39,1,1,0,1); // SUB x8,x7,x6
        issue(32'h0063c4b3,105,1,1,0,1); // XOR 78^39
        issue(32'h0063e533,111,1,1,0,1); // OR
        issue(32'h0063f5b3,6,1,1,0,1); // AND
        issue(32'hfff00613,32'hffffffff,1,1,0,1);
        issue(32'h00231693,156,1,1,0,1); // SLLI x13,x6,2
        issue(32'h00465713,32'h0fffffff,1,1,0,1);
        issue(32'h40465793,32'hffffffff,1,1,0,1);
        issue(32'h00062833,1,1,1,0,1); // signed -1 < 0
        issue(32'h000638b3,0,1,1,0,1); // unsigned -1 < 0 is false
        issue(32'h00062913,1,1,1,0,1); // SLTI
        issue(32'hfff03993,1,1,1,0,1); // SLTIU x19,x0,-1
        issue(32'h0ff64a13,32'hffffff00,1,1,0,1);
        issue(32'h01006a93,16,1,1,0,1);
        issue(32'h0ff67b13,255,1,1,0,1);
        issue(32'h01531bb3,32'h00270000,1,1,0,1); // SLL by x21=16
        issue(32'h01565c33,32'h0000ffff,1,1,0,1);
        issue(32'h41565cb3,32'hffffffff,1,1,0,1);
        issue(32'h123452b7,32'h12345000,7,1,0,1); // LUI x5
        issue(32'h00001517,32'h00002000,8,1,0,1); // AUIPC x10,0x1; external pc=0x1000
        issue(32'h00730013,46,1,1,0,1); // attempt x0 write
        // Recognized but not executable here: never accidentally write rd.
        issue(32'h00002303,0,2,0,0,1); // LW x6
        issue(32'h00602023,0,3,0,0,1); // SW
        issue(32'h00031063,0,4,0,0,1); // BNE
        issue(32'h0000036f,0,5,0,0,1); // JAL x6
        issue(32'h00030367,0,6,0,0,1); // JALR x6
        issue(32'h0000000f,0,9,0,0,1);
        issue(32'h00000073,0,10,0,0,1);
        issue(32'h00100073,0,11,0,0,1);
        issue(32'h02630333,0,0,0,1,1); // M extension
        issue(32'h0000100f,0,0,0,1,1); // FENCE.I
        issue(32'hc0002373,0,0,0,1,1); // CSR
        @(negedge clk);reset=1;commit=1;instruction=32'h02800313;
        #1;if(write_enable!==0)$fatal(1,"slice reset gates write");
        @(posedge clk);#1;
        for(int i=0;i<32;i++)model[i]=0;
        reset=0;scan();
        $display("PASS RTL_DECODE_SLICE issued=%0d checks=%0d failures=0",issued,checks);
        $finish;
    end
    initial begin #1000000;$fatal(1,"slice watchdog");end
endmodule
`default_nettype wire
