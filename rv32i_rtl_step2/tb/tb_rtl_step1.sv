`timescale 1ns/1ps
`default_nettype none

module tb_rtl_step1;
    logic clk = 1'b0;
    always #5 clk = ~clk; // Testbench-only 10 ns clock; not a timing claim.
    logic reset_rf, write_rf;
    logic [4:0] wa, ra, rb;
    logic [31:0] wd, qa, qb;
    logic [31:0] alu_a, alu_b, alu_y;
    logic [3:0] alu_op;
    logic alu_valid, alu_eq, alu_lts, alu_ltu;
    logic [31:0] model [0:31];
    int checks = 0;
    int alu_rows = 0;
    // Handy waveform marker: 0 reset, 1 demo, 2 regfile, 3 ALU, 4 done.
    logic [2:0] phase;

    rv32_alu u_alu (
        .a_i(alu_a), .b_i(alu_b), .op_i(alu_op), .result_o(alu_y),
        .op_valid_o(alu_valid), .equal_o(alu_eq),
        .less_signed_o(alu_lts), .less_unsigned_o(alu_ltu)
    );
    rv32_regfile u_rf (
        .clk_i(clk), .reset_i(reset_rf), .write_i(write_rf),
        .waddr_i(wa), .wdata_i(wd), .raddr_a_i(ra), .raddr_b_i(rb),
        .rdata_a_o(qa), .rdata_b_o(qb)
    );

    task automatic check_reads(input string label_text);
        checks += 2;
        if (qa !== model[ra] || qb !== model[rb])
            $fatal(1, "%s: ra=%0d got=%08x expected=%08x rb=%0d got=%08x expected=%08x",
                   label_text, ra, qa, model[ra], rb, qb, model[rb]);
    endtask

    task automatic scan_registers;
        // No pending write: read changes do not need another rising edge.
        for (int index = 0; index < 32; index++) begin
            ra = 5'(index);
            rb = 5'(31 - index);
            #1;
            check_reads("scan both ports");
        end
    endtask

    task automatic transaction(input logic rst, input logic enable,
                               input logic [4:0] addr, input logic [31:0] data);
        // Drive at the falling edge; sample after NBA updates at rising edge.
        @(negedge clk);
        reset_rf = rst; write_rf = enable; wa = addr; wd = data;
        ra = addr; rb = ~addr;
        #1;
        check_reads("before rising edge: no early write/reset");
        @(posedge clk);
        #1;
        if (rst)
            for (int index = 0; index < 32; index++) model[index] = 32'b0;
        else if (enable && addr != 5'd0) model[addr] = data;
        write_rf = 1'b0; reset_rf = 1'b0;
        check_reads("after rising edge");
        scan_registers();
    endtask

    function automatic logic [31:0] prng(input logic [31:0] state_in);
        logic [31:0] value;
        value = state_in ^ (state_in << 13);
        value = value ^ (value >> 17);
        return value ^ (value << 5);
    endfunction

    initial begin : run_checks
        int handle, fields;
        logic [31:0] want_y, random_state;
        logic want_valid, want_eq, want_lts, want_ltu;
        string vector_path, vcd_path;
        phase = 0;
        reset_rf = 1; write_rf = 0; wa = 0; wd = 0; ra = 0; rb = 0;
        alu_a = 0; alu_b = 0; alu_op = 0;
        if ($value$plusargs("VCD=%s", vcd_path)) begin
            $dumpfile(vcd_path);
            $dumpvars(0, tb_rtl_step1);
        end
        // x0 must read zero even before any reset edge.
        #1;
        checks += 2;
        if (qa !== 32'b0 || qb !== 32'b0) $fatal(1, "x0 before reset");
        @(posedge clk); #1;
        for (int index = 0; index < 32; index++) model[index] = 0;
        reset_rf = 0;
        scan_registers();

        phase = 1;
        transaction(0, 1, 5'd6, 32'd40);
        ra = 6; rb = 0; #1;
        alu_a = qa; alu_b = 32'hffffffff; alu_op = 4'h0; #1;
        checks++;
        if (alu_y !== 32'd39 || qa !== 32'd40) $fatal(1, "ADDI-style demo calculation");
        $display("DEMO before write: x6=%0d ALU(40 + -1)=%0d", qa, alu_y);
        transaction(0, 1, 5'd6, alu_y);
        ra = 6; #1;
        $display("DEMO after rising-edge write: x6=%0d", qa);
        $display("NOTE demo controls are driven by the testbench; no instruction decoder exists yet.");

        phase = 2;
        transaction(0, 0, 5'd6, 32'hdeadbeef); // write disabled
        transaction(0, 1, 5'd0, 32'hffffffff); // x0 write discarded
        for (int index = 1; index < 32; index++)
            transaction(0, 1, 5'(index), 32'h01010101 * 32'(index));
        transaction(1, 1, 5'd31, 32'hffffffff); // reset beats write
        random_state = 32'h52563332;
        for (int index = 0; index < 128; index++) begin
            random_state = prng(random_state);
            transaction((index % 37) == 36, random_state[5],
                        random_state[4:0], random_state);
        end
        $display("PASS register file: both read ports, all addresses, clocked writes, enable, x0, reset priority");

        phase = 3;
        if (!$value$plusargs("VECTORS=%s", vector_path))
            vector_path = "vectors/alu_vectors.txt";
        handle = $fopen(vector_path, "r");
        if (handle == 0) $fatal(1, "cannot open %s", vector_path);
        while (!$feof(handle)) begin
            fields = $fscanf(handle, "%h %h %h %h %h %h %h %h\n",
                             alu_op, alu_a, alu_b, want_y,
                             want_valid, want_eq, want_lts, want_ltu);
            if (fields != 8) $fatal(1, "malformed vector after row %0d", alu_rows);
            #1;
            checks++; alu_rows++;
            if ({alu_valid,alu_eq,alu_lts,alu_ltu,alu_y} !==
                {want_valid,want_eq,want_lts,want_ltu,want_y})
                $fatal(1, "ALU row=%0d op=%x a=%08x b=%08x y=%08x want=%08x flags=%b%b%b%b want=%b%b%b%b",
                       alu_rows, alu_op, alu_a, alu_b, alu_y, want_y,
                       alu_valid,alu_eq,alu_lts,alu_ltu,
                       want_valid,want_eq,want_lts,want_ltu);
        end
        $fclose(handle);
        if (alu_rows != 7344) $fatal(1, "wrong vector count %0d", alu_rows);
        $display("PASS ALU: %0d vectors, all operations, signedness, wrapping, shifts, invalid controls", alu_rows);
        phase = 4;
        $display("PASS RTL_STEP1 checks=%0d failures=0", checks);
        $finish;
    end

    initial begin
        #1000000;
        $fatal(1, "testbench watchdog expired");
    end
endmodule
`default_nettype wire
