`timescale 1ns/1ps
`default_nettype none
// Bus router + small devices. RAM remains external; the RV32I core is unchanged.
// One transaction at a time. Completion means the effect happened at that edge.
module rv32_mmio #(
    parameter logic [31:0] RAM_BYTES=32'h4000,
    parameter integer IO_WAIT_CYCLES=0,
    parameter logic [31:0] TIMER_RESET_VALUE=0
) (
    input wire logic clk_i,reset_i,
    input wire logic valid_i,instruction_i,
    input wire logic [31:0] address_i,write_data_i,
    input wire logic [3:0] read_mask_i,write_mask_i,
    output logic done_o,error_o,
    output logic [31:0] read_data_o,
    output logic ram_valid_o,ram_instruction_o,
    output logic [31:0] ram_address_o,ram_write_data_o,
    output logic [3:0] ram_read_mask_o,ram_write_mask_o,
    input wire logic ram_done_i,ram_error_i,
    input wire logic [31:0] ram_read_data_i,
    output logic tx_valid_o,
    output logic [7:0] tx_data_o,
    input wire logic tx_ready_i,
    output logic [31:0] timer_value_o
);
    localparam logic [31:0] TX_DATA=32'h10000000,
                            TX_STATUS=32'h10000004,
                            TIMER=32'h10000010;
    localparam integer WAIT_WIDTH=(IO_WAIT_CYCLES<1)?1:$clog2(IO_WAIT_CYCLES+1);
    logic [WAIT_WIDTH-1:0] wait_q;
    logic [31:0] timer_q;
    wire shape_ok=address_i[1:0]==0 && ((read_mask_i!=0) ^ (write_mask_i!=0));
    wire ram_selected=shape_ok && address_i<RAM_BYTES;
    wire io_request=!reset_i && valid_i && !ram_selected;
    wire eligible=(wait_q>=IO_WAIT_CYCLES);
    wire tx_access=shape_ok && !instruction_i && address_i==TX_DATA &&
                   read_mask_i==0 && write_mask_i==4'b0001;
    wire status_access=shape_ok && !instruction_i && address_i==TX_STATUS &&
                       read_mask_i==4'b1111 && write_mask_i==0;
    wire timer_access=shape_ok && !instruction_i && address_i==TIMER &&
                      read_mask_i==4'b1111 && write_mask_i==0;
    wire io_legal=tx_access || status_access || timer_access;

    assign timer_value_o=timer_q;
    assign ram_valid_o=!reset_i && valid_i && ram_selected;
    assign ram_instruction_o=ram_valid_o && instruction_i;
    assign ram_address_o=ram_valid_o?address_i:32'b0;
    assign ram_write_data_o=ram_valid_o?write_data_i:32'b0;
    assign ram_read_mask_o=ram_valid_o?read_mask_i:4'b0;
    assign ram_write_mask_o=ram_valid_o?write_mask_i:4'b0;
    // A sink accepts a character only at a rising edge with valid && ready.
    // Waiting for ready cannot duplicate an output or complete the CPU store.
    assign tx_valid_o=io_request && eligible && tx_access;
    assign tx_data_o=tx_valid_o?write_data_i[7:0]:8'b0;
    assign done_o=ram_valid_o?ram_done_i:
                  (io_request && eligible && (!tx_access || tx_ready_i));
    assign error_o=done_o && (ram_selected?ram_error_i:!io_legal);
    assign read_data_o=(!done_o || error_o)?32'b0:
                        ram_selected?ram_read_data_i:
                        status_access?{31'b0,tx_ready_i}:
                        timer_access?timer_q:32'b0;

    always_ff @(posedge clk_i) begin
        if(reset_i) begin
            wait_q<=0;
            timer_q<=TIMER_RESET_VALUE;
        end else begin
            timer_q<=timer_q+32'd1; // Clock ticks, NOT host wall time/milliseconds.
            if(!io_request || done_o) wait_q<=0;
            else if(!eligible) wait_q<=wait_q+1'b1;
        end
    end
endmodule
`default_nettype wire
