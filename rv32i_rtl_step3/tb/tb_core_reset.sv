`timescale 1ns/1ps
`default_nettype none
// Cancel a waiting fetch and a waiting store by reset; then restart cleanly.
module tb_core_reset;
    logic clk=0,reset=1,allow_memory=0;
    always #5 clk=~clk;
    wire valid,instruction_request,trap,retired;
    wire [31:0] address,write_data,pc;
    wire [3:0] rm,wm,trap_code;
    wire [2:0] state_value;
    wire done=valid && allow_memory;
    logic [31:0] read_data;
    integer writes=0;
    logic [31:0] data_word=0;
    rv32_core dut(.clk_i(clk),.reset_i(reset),.mem_valid_o(valid),.mem_instruction_o(instruction_request),
        .mem_address_o(address),.mem_write_data_o(write_data),.mem_read_mask_o(rm),.mem_write_mask_o(wm),
        .mem_done_i(done),.mem_error_i(1'b0),.mem_read_data_i(read_data),.trap_valid_o(trap),
        .trap_code_o(trap_code),.retire_valid_o(retired),.pc_o(pc),.state_o(state_value));
    always_comb begin
        case(address)
            0:read_data=32'h04d00093; // ADDI x1,x0,77
            4:read_data=32'h08102023; // SW x1,128(x0)
            8:read_data=32'h00100073;
            default:read_data=data_word;
        endcase
    end
    always @(posedge clk) begin
        if(!reset && valid && done && wm!=0) begin
            if(address!=128 || wm!=15) $fatal(1,"bad store");
            data_word<=write_data;writes=writes+1;
        end
    end
    initial begin
        repeat(2) @(negedge clk);reset=0;
        repeat(3) @(negedge clk);
        if(!valid || !instruction_request || pc!=0) $fatal(1,"fetch did not wait");
        reset=1;#1;if(valid || wm!=0) $fatal(1,"reset did not suppress pending fetch");
        @(negedge clk);reset=0;allow_memory=1;
        // Stop memory before EXECUTE enters the store's MEMORY state.
        wait(pc==4 && state_value==2);@(negedge clk);allow_memory=0;
        wait(valid && wm!=0);repeat(3) @(negedge clk);
        if(writes!=0 || data_word!=0 || dut.u_rf.regs_q[1]!==77) $fatal(1,"store was not pending");
        reset=1;#1;if(valid || wm!=0) $fatal(1,"reset did not suppress pending store");
        @(negedge clk);
        if(pc!==0 || dut.u_rf.regs_q[1]!==0 || writes!=0) $fatal(1,"reset state wrong");
        reset=0;allow_memory=1;
        wait(trap);#1;
        if(trap_code!==9 || pc!==8 || data_word!==77 || writes!=1) $fatal(1,"restart failed");
        $display("PASS RTL_CORE_RESET: canceled pending fetch/store; one store after restart");$finish;
    end
    initial begin #5000;$fatal(1,"reset test watchdog");end
endmodule
`default_nettype wire
