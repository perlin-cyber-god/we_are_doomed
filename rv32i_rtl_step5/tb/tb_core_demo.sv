`timescale 1ns/1ps
`default_nettype none
// Tiny self-contained Vivado demo; requires no external image/vector files.
module tb_core_demo;
    logic clk_i=0,reset_i=1;
    always #5 clk_i=~clk_i;
    wire mem_valid_o,mem_instruction_o;
    wire [31:0] mem_address_o,mem_write_data_o;
    wire [3:0] mem_read_mask_o,mem_write_mask_o;
    wire mem_done_i=mem_valid_o;
    logic mem_error_i;
    logic [31:0] mem_read_data_i;
    wire trap_valid_o,retire_valid_o;
    wire [3:0] trap_code_o;
    wire [31:0] trap_pc_o,trap_instruction_o,trap_address_o;
    wire [31:0] retire_pc_o,retire_instruction_o,retire_next_pc_o;
    wire [4:0] retire_rd_o;
    wire [31:0] retire_write_data_o,retire_memory_address_o,retire_memory_data_o;
    wire [3:0] retire_read_mask_o,retire_write_mask_o;
    wire [31:0] pc_o,instruction_o;
    wire [2:0] state_o;
    rv32_core dut(.*);
    logic [31:0] words[0:255];
    wire [31:0] x6_value=dut.u_rf.regs_q[6];
    wire [31:0] ram_value=words[128];
    integer retired=0,stores=0;
    string vcd_path;
    always_comb begin
        mem_error_i=(mem_address_o>=1024);mem_read_data_i=0;
        if(mem_valid_o && !mem_error_i) mem_read_data_i=words[mem_address_o>>2];
    end
    always @(posedge clk_i) begin
        if(!reset_i && mem_valid_o && mem_done_i && !mem_error_i && mem_write_mask_o!=0) begin
            for(integer lane=0;lane<4;lane=lane+1)
                if(mem_write_mask_o[lane]) words[mem_address_o>>2][lane*8+:8]<=mem_write_data_o[lane*8+:8];
            stores=stores+1;
        end
        #1;
        if(!reset_i && retire_valid_o) begin
            retired=retired+1;
            $display("CORE DEMO pc=%08x word=%08x next=%08x x6=%0d RAM[512]=%0d state=%0d",
                      retire_pc_o,retire_instruction_o,pc_o,x6_value,ram_value,state_o);
        end
        if(!reset_i && trap_valid_o) begin
            if(trap_code_o!==9 || pc_o!==16 || x6_value!==39 || ram_value!==39 || retired!=4 || stores!=1)
                $fatal(1,"demo final state wrong");
            $display("PASS RTL_CORE_DEMO: fetched 4 instructions; x6=39 RAM[512]=39; explicit EBREAK trap");
            $finish;
        end
    end
    initial begin
        for(integer i=0;i<256;i=i+1) words[i]=0;
        words[0]=32'h20000293; // ADDI x5,x0,512
        words[1]=32'h0002a303; // LW x6,0(x5)
        words[2]=32'hfff30313; // ADDI x6,x6,-1
        words[3]=32'h0062a023; // SW x6,0(x5)
        words[4]=32'h00100073; // EBREAK: explicit trap, not normal retirement
        words[128]=40;
        if($value$plusargs("VCD=%s",vcd_path)) begin $dumpfile(vcd_path);$dumpvars(0,tb_core_demo);end
        repeat(2) @(negedge clk_i);reset_i=0;
        #2000;$fatal(1,"demo watchdog");
    end
endmodule
`default_nettype wire
