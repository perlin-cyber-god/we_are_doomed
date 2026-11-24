`timescale 1ns/1ps
`default_nettype none
// Simulation-only RAM and differential checker. No host implementation of guest logic.
module tb_compiled_guest # (parameter logic [31:0] RESET_PC=0);
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
    rv32_core #(.RESET_PC(RESET_PC)) dut(.*);
    logic [7:0] ram[0:16383];
    integer ram_bytes=16384,stall_mode=0,wait_count=0,transactions=0;
    integer retired=0,cycles=0,stores=0,retired_stores=0,checks=0;
    integer fd,n,limit;
    string image_path,trace_path,tag,dump_path;
    integer dump_fd,max_steps=10000;
    logic waiting=0;
    logic [104:0] saved_request;
    wire [104:0] request_bits={mem_instruction_o,mem_address_o,mem_write_data_o,
                              mem_read_mask_o,mem_write_mask_o,32'b0};
    logic [31:0] exp_pc,exp_word,exp_next,exp_rd,exp_data,exp_address;
    logic [31:0] exp_rm,exp_wm,exp_store,exp_hash,exp_reg,exp_code;
    logic [31:0] expected_registers[0:31];
    logic [32:0] lane_address;
    integer lane;

    // mem_done means a COMPLETED access, not a queued request.
    // Check every selected byte before allowing ANY store byte to change.
    always_comb begin
        limit=(stall_mode==0)?0:1+(transactions%4);
        mem_done_i=mem_valid_o && wait_count>=limit;
        mem_error_i=0;mem_read_data_i=0;lane_address=0;
        for(integer k=0;k<4;k=k+1) begin
            lane_address={1'b0,mem_address_o}+k;
            if(mem_valid_o && (mem_read_mask_o[k] || mem_write_mask_o[k])) begin
                if(lane_address>=ram_bytes) mem_error_i=1;
                else if(mem_read_mask_o[k]) mem_read_data_i[k*8+:8]=ram[lane_address];
            end
        end
    end
    always @(posedge clk_i) begin
        if(reset_i) begin wait_count<=0;transactions<=0;waiting=0;end
        else begin
            if(waiting && (!mem_valid_o || request_bits!==saved_request))
                $fatal(1,"request changed before completion");
            if(mem_valid_o) begin
                if(mem_address_o[1:0]!==0 || ((mem_read_mask_o!=0)&&(mem_write_mask_o!=0)))
                    $fatal(1,"malformed memory request");
                if(mem_instruction_o && (mem_read_mask_o!==4'hf || mem_write_mask_o!==0))
                    $fatal(1,"malformed fetch request");
            end
            waiting=mem_valid_o && !mem_done_i;saved_request=request_bits;
            if(mem_valid_o && mem_done_i) begin
                if(!mem_error_i && mem_write_mask_o!=0) begin
                    for(lane=0;lane<4;lane=lane+1)
                        if(mem_write_mask_o[lane]) ram[mem_address_o+lane]<=mem_write_data_o[lane*8+:8];
                    stores=stores+1;
                end
                transactions<=transactions+1;wait_count<=0;
            end else if(mem_valid_o) wait_count<=wait_count+1;
            else wait_count<=0;
        end
    end

    task automatic equal32(input logic [31:0] actual,expected,input string label_text);
        checks=checks+1;
        if(actual!==expected) $fatal(1,"%s retired=%0d PC=%08x got=%08x expected=%08x",
                                      label_text,retired,pc_o,actual,expected);
    endtask
    task automatic check_state;
        logic [31:0] value;
        integer result;
        begin
            result=$fscanf(fd," %h",exp_hash);
            if(result!=1) $fatal(1,"missing expected RAM hash");
            // Per-step memory events checked; final RAM is byte-exact.
            equal32(0,exp_hash,"trace format marker");
            for(integer i=0;i<32;i=i+1) begin
                result=$fscanf(fd," %h",exp_reg);
                if(result!=1) $fatal(1,"missing expected register");
                expected_registers[i]=exp_reg;
                if(i==0) value=0;else value=dut.u_rf.regs_q[i];
                equal32(value,exp_reg,$sformatf("x%0d",i));
            end
        end
    endtask
    task automatic next_tag;
        begin
            n=$fscanf(fd," %s",tag);
            if(n!=1) $fatal(1,"unexpected end of expected trace");
        end
    endtask
    task automatic done_message(input string ending);
        begin
            equal32(stores,retired_stores,"one completed store per retirement");
            next_tag();if(tag!="M") $fatal(1,"missing final byte-exact RAM record");
            for(integer i=0;i<16384;i=i+1) begin
                n=$fscanf(fd," %h",exp_data);if(n!=1) $fatal(1,"incomplete final RAM record");
                equal32({24'b0,ram[i]},exp_data,$sformatf("final RAM[%0d]",i));
            end
            if(!$feof(fd)) begin
                n=$fscanf(fd," %s",tag);
                if(n==1) $fatal(1,"extra expected trace records");
            end
            dump_fd=$fopen(dump_path,"w");
            if(dump_fd==0) $fatal(1,"cannot create RTL RAM dump");
            for(integer i=0;i<16384;i=i+1) $fdisplay(dump_fd,"%02x",ram[i]);
            $fclose(dump_fd);
            $fclose(fd);
            $display("PASS RTL_COMPILED_TRACE retired=%0d cycles=%0d transactions=%0d stores=%0d checks=%0d stall=%0d %s",
                      retired,cycles,transactions,stores,checks,stall_mode,ending);
            $finish;
        end
    endtask
    initial begin
        if(!$value$plusargs("IMAGE=%s",image_path) || !$value$plusargs("TRACE=%s",trace_path))
            $fatal(1,"IMAGE and TRACE required; run ./test.sh");
        n=$value$plusargs("RAM_BYTES=%d",ram_bytes);n=$value$plusargs("STALL=%d",stall_mode);
        if(!$value$plusargs("DUMP=%s",dump_path)) $fatal(1,"DUMP required");
        n=$value$plusargs("MAX_STEPS=%d",max_steps);
        if(ram_bytes!=16384 || max_steps<1 || max_steps>10000) $fatal(1,"bad guest limits");
        $readmemh(image_path,ram);
        fd=$fopen(trace_path,"r");if(fd==0) $fatal(1,"cannot open trace");
        next_tag();
        repeat(2) @(negedge clk_i);
        reset_i=0;
    end
    initial begin
        // Bounded even if the DUT deadlocks with a pending memory request.
        #2000000;$fatal(1,"RTL watchdog: no completion within 200000 cycles");
    end
    always @(posedge clk_i) begin
        #1;
        if(!reset_i) begin
            cycles=cycles+1;
            if(retire_valid_o) begin
                if(trap_valid_o || tag!="R") $fatal(1,"unexpected retirement, expected %s",tag);
                n=$fscanf(fd," %h %h %h %h %h %h %h %h %h",exp_pc,exp_word,exp_next,exp_rd,
                          exp_data,exp_address,exp_rm,exp_wm,exp_store);
                if(n!=9) $fatal(1,"malformed retirement record");
                equal32(retire_pc_o,exp_pc,"retire PC");equal32(retire_instruction_o,exp_word,"instruction");
                equal32(retire_next_pc_o,exp_next,"next PC");equal32(pc_o,exp_next,"architectural PC");
                equal32({27'b0,retire_rd_o},exp_rd,"destination");equal32(retire_write_data_o,exp_data,"writeback");
                equal32(retire_memory_address_o,exp_address,"effective address");
                equal32({28'b0,retire_read_mask_o},exp_rm,"read lanes");
                equal32({28'b0,retire_write_mask_o},exp_wm,"write lanes");
                equal32(retire_memory_data_o,exp_store,"store data");
                if(retire_write_mask_o!=0) retired_stores=retired_stores+1;
                check_state();retired=retired+1;
                if(retired>max_steps) $fatal(1,"host instruction budget exceeded");
                next_tag();
                if(retired==max_steps && tag!="B") $fatal(1,"instruction budget exhausted");
                if(tag=="B") begin
                    n=$fscanf(fd," %h",exp_pc);if(n!=1) $fatal(1,"bad budget record");
                    equal32(pc_o,exp_pc,"budget PC");check_state();
                    done_message("BUDGET_PREFIX (not guest HALT)");
                end
            end
            if(trap_valid_o) begin
                if(retired+1>max_steps) $fatal(1,"step-attempt budget exhausted before trap");
                if(tag!="T") $fatal(1,"unexpected trap %0d; expected %s",trap_code_o,tag);
                n=$fscanf(fd," %h %h %h %h",exp_code,exp_pc,exp_word,exp_address);
                if(n!=4) $fatal(1,"malformed trap record");
                equal32({28'b0,trap_code_o},exp_code,"trap code");equal32(trap_pc_o,exp_pc,"trap PC");
                equal32(trap_instruction_o,exp_word,"trap word");equal32(trap_address_o,exp_address,"trap address");
                equal32(pc_o,exp_pc,"fault cannot advance PC");check_state();
                // Sticky trap must leave bus, PC, registers and RAM alone.
                repeat(3) begin
                    @(posedge clk_i);#1;
                    if(!trap_valid_o || mem_valid_o || retire_valid_o) $fatal(1,"trap not sticky/quiet");
                    equal32(pc_o,exp_pc,"stopped PC");
                    for(integer i=1;i<32;i=i+1)
                        equal32(dut.u_rf.regs_q[i],expected_registers[i],"stopped register");
                end
                done_message($sformatf("TRAP=%0d",trap_code_o));
            end
        end
    end
endmodule
`default_nettype wire
