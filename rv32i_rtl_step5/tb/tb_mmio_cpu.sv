`timescale 1ns/1ps
`default_nettype none
// Actual RTL run; external responses are recorded for independent architectural replay.
module tb_mmio_cpu #(parameter integer WAIT=0);
    logic clk_i=0,reset_i=1;
    always #5 clk_i=~clk_i;
    wire mem_valid_o,mem_instruction_o;
    wire [31:0] mem_address_o,mem_write_data_o;
    wire [3:0] mem_read_mask_o,mem_write_mask_o;
    logic mem_done_i,mem_error_i;
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
    wire ram_valid,ram_instruction;
    wire [31:0] ram_address,ram_write_data;
    wire [3:0] ram_rm,ram_wm;
    logic ram_done,ram_error;
    logic [31:0] ram_read_data;
    wire tx_valid;
    wire [7:0] tx_data;
    wire [31:0] timer_value;
    integer cycles=0,transactions=0,ram_stores=0,tx_count=0,io_count=0;
    integer wait_count=0,retired=0,retired_stores=0,max_steps=10000;
    wire tx_ready=(WAIT==0) || ((cycles%7)>=3);
    rv32_mmio #(.IO_WAIT_CYCLES(WAIT)) fabric(
        .clk_i(clk_i),.reset_i(reset_i),.valid_i(mem_valid_o),.instruction_i(mem_instruction_o),
        .address_i(mem_address_o),.write_data_i(mem_write_data_o),
        .read_mask_i(mem_read_mask_o),.write_mask_i(mem_write_mask_o),
        .done_o(mem_done_i),.error_o(mem_error_i),.read_data_o(mem_read_data_i),
        .ram_valid_o(ram_valid),.ram_instruction_o(ram_instruction),
        .ram_address_o(ram_address),.ram_write_data_o(ram_write_data),
        .ram_read_mask_o(ram_rm),.ram_write_mask_o(ram_wm),
        .ram_done_i(ram_done),.ram_error_i(ram_error),.ram_read_data_i(ram_read_data),
        .tx_valid_o(tx_valid),.tx_data_o(tx_data),.tx_ready_i(tx_ready),.timer_value_o(timer_value)
    );
    logic [7:0] ram[0:16383];
    logic [32:0] lane_address;
    integer limit;
    logic waiting=0;
    logic [72:0] saved_request;
    wire [72:0] request_bits={mem_instruction_o,mem_address_o,mem_write_data_o,mem_read_mask_o,mem_write_mask_o};
    logic [31:0] model_timer=0;
    integer trace_fd,event_fd,console_fd,n;
    string image_path,trace_path,event_path,console_path;
    always_comb begin
        limit=(WAIT==0)?0:1+(transactions%4);
        ram_done=ram_valid && wait_count>=limit;
        ram_error=0;ram_read_data=0;lane_address=0;
        for(integer lane=0;lane<4;lane=lane+1) begin
            lane_address={1'b0,ram_address}+lane;
            if(ram_valid && (ram_rm[lane] || ram_wm[lane])) begin
                if(lane_address>=16384) ram_error=1;
                else if(ram_rm[lane]) ram_read_data[lane*8+:8]=ram[lane_address];
            end
        end
    end
    always @(posedge clk_i) begin
        if(reset_i) begin
            cycles<=0;transactions<=0;wait_count<=0;waiting=0;model_timer=0;
        end else begin
            if(timer_value!==model_timer) $fatal(1,"free-running timer drift");
            if(waiting && (!mem_valid_o || saved_request!==request_bits))
                $fatal(1,"CPU request changed while waiting");
            waiting=mem_valid_o && !mem_done_i;saved_request=request_bits;
            if(mem_valid_o && mem_done_i && mem_address_o>=16384) begin
                $fdisplay(event_fd,"I %08h %h %h %08h %08h %h %h",mem_address_o,
                          mem_read_mask_o,mem_write_mask_o,mem_write_data_o,
                          mem_read_data_i,mem_error_i,mem_instruction_o);
                io_count=io_count+1;
                if(mem_address_o==32'h10000010 && !mem_error_i && mem_read_mask_o==15)
                    if(mem_read_data_i!==model_timer) $fatal(1,"timer read not completion-time tick");
                if(mem_address_o==32'h10000004 && !mem_error_i && mem_read_mask_o==15)
                    if(mem_read_data_i!=={31'b0,tx_ready}) $fatal(1,"bad readiness response");
            end
            if(tx_valid && tx_ready) begin
                if(!mem_valid_o || !mem_done_i || mem_error_i || mem_write_mask_o!=1 ||
                   mem_address_o!=32'h10000000 || ram_valid)
                    $fatal(1,"console side effect outside legitimate completed store");
                $fwrite(console_fd,"%c",tx_data);tx_count=tx_count+1;
            end
            if(ram_valid && ram_done) begin
                if(!ram_error && ram_wm!=0) begin
                    for(integer lane=0;lane<4;lane=lane+1)
                        if(ram_wm[lane]) ram[ram_address+lane]<=ram_write_data[lane*8+:8];
                    ram_stores=ram_stores+1;
                end
                transactions<=transactions+1;wait_count<=0;
            end else if(ram_valid) wait_count<=wait_count+1;
            else wait_count<=0;
            cycles<=cycles+1;model_timer=model_timer+32'd1;
        end
        #1;
        if(timer_value!==model_timer) $fatal(1,"timer increment mismatch after edge");
    end
    task automatic write_state;
        begin
            $fwrite(trace_fd," 00000000 00000000"); // Marker and x0.
            for(integer i=1;i<32;i=i+1) $fwrite(trace_fd," %08h",dut.u_rf.regs_q[i]);
            $fwrite(trace_fd,"\n");
        end
    endtask
    task automatic finish_run(input string ending);
        begin
            if(retired_stores!=ram_stores+tx_count) $fatal(1,"store effects duplicated/lost");
            $fwrite(trace_fd,"M");
            for(integer i=0;i<16384;i=i+1) $fwrite(trace_fd," %02h",ram[i]);
            $fwrite(trace_fd,"\n");
            $fclose(trace_fd);$fclose(event_fd);$fclose(console_fd);
            $display("PASS MMIO_CPU retired=%0d cycles=%0d tx=%0d io=%0d wait=%0d %s",
                      retired,cycles,tx_count,io_count,WAIT,ending);$finish;
        end
    endtask
    initial begin
        if(!$value$plusargs("IMAGE=%s",image_path) || !$value$plusargs("TRACE=%s",trace_path) ||
           !$value$plusargs("EVENTS=%s",event_path) || !$value$plusargs("CONSOLE=%s",console_path))
            $fatal(1,"run via ./run_io.sh for required paths");
        n=$value$plusargs("MAX_STEPS=%d",max_steps);
        if(max_steps<1 || max_steps>20000) $fatal(1,"invalid step budget");
        $readmemh(image_path,ram);
        trace_fd=$fopen(trace_path,"w");event_fd=$fopen(event_path,"w");console_fd=$fopen(console_path,"w");
        if(trace_fd==0 || event_fd==0 || console_fd==0) $fatal(1,"cannot open output files");
        repeat(2) @(negedge clk_i);reset_i=0;
    end
    always @(posedge clk_i) begin
        #1;
        if(!reset_i) begin
            if(retire_valid_o) begin
                $fwrite(trace_fd,"R %08h %08h %08h %h %08h %08h %h %h %08h",
                        retire_pc_o,retire_instruction_o,retire_next_pc_o,retire_rd_o,
                        retire_write_data_o,retire_memory_address_o,retire_read_mask_o,
                        retire_write_mask_o,retire_memory_data_o);
                write_state();retired=retired+1;
                if(retire_write_mask_o!=0) retired_stores=retired_stores+1;
                if(retired>=max_steps) begin
                    $fwrite(trace_fd,"B %08h",pc_o);write_state();finish_run("BUDGET_PREFIX");
                end
            end
            if(trap_valid_o) begin
                if(retired+1>max_steps || mem_valid_o || tx_valid || ram_valid)
                    $fatal(1,"bad trap/budget state");
                $fwrite(trace_fd,"T %h %08h %08h %08h",trap_code_o,trap_pc_o,trap_instruction_o,trap_address_o);
                write_state();finish_run($sformatf("TRAP=%0d",trap_code_o));
            end
        end
    end
    initial begin #2000000;$fatal(1,"MMIO CPU cycle watchdog");end
endmodule
module tb_mmio_cpu_wait;
    tb_mmio_cpu #(.WAIT(3)) test();
endmodule
`default_nettype wire
