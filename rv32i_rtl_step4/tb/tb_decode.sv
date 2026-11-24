`timescale 1ns/1ps
`default_nettype none
module tb_decode;
    logic [31:0] instruction, imm, expected_imm;
    logic [3:0] kind, alu_op;
    logic illegal, reg_write, use1, use2, alu_immediate, unsigned_load;
    logic [4:0] rs1,rs2,rd;
    logic [2:0] format,branch;
    logic [1:0] size;
    logic [36:0] controls,expected_controls;
    integer rows=0;
    rv32_decode dut (
        .instruction_i(instruction),.kind_o(kind),.illegal_o(illegal),
        .rs1_o(rs1),.rs2_o(rs2),.rd_o(rd),.immediate_o(imm),
        .immediate_format_o(format),.uses_rs1_o(use1),.uses_rs2_o(use2),
        .reg_write_o(reg_write),.alu_immediate_o(alu_immediate),.alu_op_o(alu_op),
        .memory_size_o(size),.load_unsigned_o(unsigned_load),.branch_funct3_o(branch)
    );
    assign controls={kind,illegal,reg_write,use1,use2,alu_immediate,alu_op,
                     size,unsigned_load,branch,format,rs1,rs2,rd};
    initial begin
        string path;
        integer handle,fields;
        if (!$value$plusargs("DECODE_VECTORS=%s",path)) path="vectors/decode_vectors.txt";
        handle=$fopen(path,"r");
        if (handle==0) $fatal(1,"cannot open decoder vectors: %s",path);
        while (!$feof(handle)) begin
            fields=$fscanf(handle,"%h %h %h\n",instruction,expected_controls,expected_imm);
            if (fields!=3) $fatal(1,"bad vector after decoder row %0d",rows);
            #1; rows++;
            if ({controls,imm} !== {expected_controls,expected_imm})
                $fatal(1,"decode row=%0d word=%08x controls=%010x expected=%010x imm=%08x expected=%08x",
                       rows,instruction,controls,expected_controls,imm,expected_imm);
        end
        $fclose(handle);
        if (rows<20000) $fatal(1,"decoder corpus unexpectedly short: %0d",rows);
        $display("PASS RTL_DECODE rows=%0d failures=0",rows);
        $finish;
    end
    initial begin #1000000; $fatal(1,"decoder watchdog"); end
endmodule
`default_nettype wire
