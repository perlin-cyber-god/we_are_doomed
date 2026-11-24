`timescale 1ns/1ps
`default_nettype none
module tb_mmio_unit #(parameter integer WAIT=0);
    logic clk_i=0,reset_i=1;
    always #5 clk_i=~clk_i;
    logic valid_i=0,instruction_i=0;
    logic [31:0] address_i=0,write_data_i=0;
    logic [3:0] read_mask_i=0,write_mask_i=0;
    wire done_o,error_o;
    wire [31:0] read_data_o;
    wire ram_valid_o,ram_instruction_o;
    wire [31:0] ram_address_o,ram_write_data_o;
    wire [3:0] ram_read_mask_o,ram_write_mask_o;
    logic ram_done_i=1,ram_error_i=0;
    logic [31:0] ram_read_data_i=32'h12345678;
    wire tx_valid_o;
    wire [7:0] tx_data_o;
    logic tx_ready_i=1;
    wire [31:0] timer_value_o;
    rv32_mmio #(.IO_WAIT_CYCLES(WAIT),.TIMER_RESET_VALUE(32'hfffffffc)) dut(.*);
    logic [31:0] model_timer=32'hfffffffc;
    integer checks=0,tx_count=0,wraps=0;
    always @(posedge clk_i) begin
        if(reset_i) model_timer=32'hfffffffc;
        else begin
            if(timer_value_o!==model_timer) $fatal(1,"timer before edge mismatch");
            if(done_o && !error_o && address_i==32'h10000010 && read_mask_i==15)
                if(read_data_o!==model_timer) $fatal(1,"timer completion sampled wrong tick");
            if(tx_valid_o && tx_ready_i) begin
                if(!done_o || error_o || ram_valid_o) $fatal(1,"bad console handshake");
                if(tx_data_o!==((tx_count==0)?8'h41:8'h43)) $fatal(1,"wrong/duplicate console byte");
                tx_count=tx_count+1;
            end
            if(model_timer==32'hffffffff) wraps=wraps+1;
            model_timer=model_timer+32'd1;
        end
        #1;
        checks=checks+1;
        if(timer_value_o!==model_timer) $fatal(1,"timer increment/wrap/reset mismatch");
    end
    task automatic access_bus(input logic [31:0] addr,data,
                              input logic [3:0] rm,wm,
                              input logic fetch,expected_error,
                              input integer release_after);
        integer elapsed;
        logic complete,err;
        logic [31:0] response;
        begin
            @(negedge clk_i);valid_i=1;address_i=addr;write_data_i=data;
            read_mask_i=rm;write_mask_i=wm;instruction_i=fetch;
            elapsed=0;complete=0;err=0;response=0;
            while(!complete) begin
                @(posedge clk_i);
                if(done_o) begin complete=1;err=error_o;response=read_data_o;end
                #1;
                if(!complete) begin
                    @(negedge clk_i);elapsed=elapsed+1;
                    if(elapsed>=release_after) begin tx_ready_i=1;ram_done_i=1;end
                    if(elapsed>30) $fatal(1,"unit request never completed");
                end
            end
            checks=checks+1;
            if(err!==expected_error) $fatal(1,"error policy addr=%08x got=%b",addr,err);
            if(expected_error && response!==0) $fatal(1,"failed read leaked response");
            if(addr==32'h20 && !expected_error && response!==32'h12345678) $fatal(1,"RAM response changed");
            @(negedge clk_i);valid_i=0;instruction_i=0;read_mask_i=0;write_mask_i=0;
        end
    endtask
    initial begin
        repeat(2) @(negedge clk_i);reset_i=0;
        // RAM forwarding under delay, including propagation of an error.
        ram_done_i=0;
        access_bus(32'h20,0,15,0,0,0,3);
        ram_error_i=1;access_bus(32'h20,0,15,0,0,1,0);ram_error_i=0;
        // Hold console ready low beyond IO_WAIT: exactly one byte may emerge.
        tx_ready_i=0;access_bus(32'h10000000,32'h41,0,1,0,0,6);
        if(tx_count!=1) $fatal(1,"console did not complete once");
        access_bus(32'h10000004,0,15,0,0,0,0);
        access_bus(32'h10000010,0,15,0,0,0,0);
        // Wrong direction, size, masks, unknown offsets and instruction fetch.
        access_bus(32'h10000000,0,1,0,0,1,0);
        access_bus(32'h10000000,32'h42,0,15,0,1,0);
        access_bus(32'h10000000,32'h42,0,2,0,1,0);
        access_bus(32'h10000004,0,0,1,0,1,0);
        access_bus(32'h10000010,0,3,0,0,1,0);
        access_bus(32'h10000010,0,0,15,0,1,0);
        access_bus(32'h10000008,0,15,0,0,1,0);
        access_bus(32'h10000010,0,15,0,1,1,0);
        access_bus(32'h4000,0,15,0,0,1,0);
        access_bus(32'h10000000,0,15,1,0,1,0);
        if(tx_count!=1) $fatal(1,"rejected access emitted a byte");
        // Reset cancels a console store that has not yet been accepted.
        @(negedge clk_i);valid_i=1;address_i=32'h10000000;write_data_i=32'h42;
        read_mask_i=0;write_mask_i=1;instruction_i=0;tx_ready_i=0;
        wait(tx_valid_o);@(negedge clk_i);reset_i=1;
        #1;if(tx_valid_o || done_o || ram_valid_o) $fatal(1,"reset did not gate request");
        repeat(2) @(negedge clk_i);
        valid_i=0;write_mask_i=0;reset_i=0;tx_ready_i=1;
        access_bus(32'h10000000,32'h43,0,1,0,0,0);
        @(negedge clk_i);
        if(tx_count!=2 || wraps<1) $fatal(1,"reset/console/wrap coverage missing");
        $display("PASS MMIO_UNIT wait=%0d checks=%0d outputs=%0d wraps=%0d",WAIT,checks,tx_count,wraps);$finish;
    end
    initial begin #10000;$fatal(1,"MMIO unit watchdog");end
endmodule
module tb_mmio_unit_wait;
    tb_mmio_unit #(.WAIT(3)) test();
endmodule
`default_nettype wire
