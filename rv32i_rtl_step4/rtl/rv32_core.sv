`timescale 1ns/1ps
`default_nettype none

// Single-hart multi-cycle RV32I core, without privileged CSR machinery.
// A trap halts this version until reset. See SPEC.md for the memory contract.
module rv32_core #(
    parameter logic [31:0] RESET_PC = 32'b0
) (
    input wire logic clk_i, reset_i,
    output logic mem_valid_o, mem_instruction_o,
    output logic [31:0] mem_address_o, mem_write_data_o,
    output logic [3:0] mem_read_mask_o, mem_write_mask_o,
    input wire logic mem_done_i, mem_error_i,
    input wire logic [31:0] mem_read_data_i,
    output logic trap_valid_o,
    output logic [3:0] trap_code_o,
    output logic [31:0] trap_pc_o, trap_instruction_o, trap_address_o,
    output logic retire_valid_o,
    output logic [31:0] retire_pc_o, retire_instruction_o, retire_next_pc_o,
    output logic [4:0] retire_rd_o,
    output logic [31:0] retire_write_data_o, retire_memory_address_o,
    output logic [3:0] retire_read_mask_o, retire_write_mask_o,
    output logic [31:0] retire_memory_data_o,
    output logic [31:0] pc_o, instruction_o,
    output logic [2:0] state_o
);
    localparam logic [2:0] FETCH=0, DECODE=1, EXECUTE=2,
                           MEMORY=3, COMMIT=4, TRAP=5;
    localparam logic [3:0] K_ALU=1,K_LOAD=2,K_STORE=3,K_BRANCH=4,
                           K_JAL=5,K_JALR=6,K_LUI=7,K_AUIPC=8,
                           K_FENCE=9,K_ECALL=10,K_EBREAK=11;
    logic [2:0] state_q;
    logic [31:0] pc_q, ir_q, sequential_pc_q, next_pc_q;
    logic [31:0] a_q,b_q,result_q,address_q;
    logic branch_taken_q;
    logic [3:0] kind, decoded_op;
    logic illegal,write_intent,uses1,uses2,alu_immediate,unsigned_load;
    logic [4:0] rs1,rs2,rd;
    logic [2:0] immediate_format,branch_f3;
    logic [1:0] memory_size;
    logic [31:0] immediate,rdata_a,rdata_b;
    logic [31:0] alu_a,alu_b,alu_result,target,shifted_load,load_value;
    logic [3:0] alu_operation,lane_mask;
    logic alu_valid,alu_equal,alu_less_signed,alu_less_unsigned;
    logic branch_condition,misaligned,register_write;
    logic [4:0] lane_shift;

    assign pc_o=pc_q;
    assign instruction_o=ir_q;
    assign state_o=state_q;
    rv32_decode u_decode (
        .instruction_i(ir_q),.kind_o(kind),.illegal_o(illegal),
        .rs1_o(rs1),.rs2_o(rs2),.rd_o(rd),.immediate_o(immediate),
        .immediate_format_o(immediate_format),.uses_rs1_o(uses1),.uses_rs2_o(uses2),
        .reg_write_o(write_intent),.alu_immediate_o(alu_immediate),.alu_op_o(decoded_op),
        .memory_size_o(memory_size),.load_unsigned_o(unsigned_load),
        .branch_funct3_o(branch_f3)
    );
    assign register_write=!reset_i && state_q==COMMIT && write_intent && rd!=0;
    rv32_regfile u_rf (
        .clk_i(clk_i),.reset_i(reset_i),.write_i(register_write),
        .waddr_i(rd),.wdata_i(result_q),.raddr_a_i(rs1),.raddr_b_i(rs2),
        .rdata_a_o(rdata_a),.rdata_b_o(rdata_b)
    );
    // Reuse the ALU: PC+4 in FETCH; source comparison in DECODE;
    // arithmetic/effective-address/target calculation in EXECUTE.
    assign alu_a=(state_q==FETCH)?pc_q:(state_q==DECODE)?rdata_a:
                 (kind==K_LUI)?32'b0:
                 (kind==K_AUIPC || kind==K_BRANCH || kind==K_JAL)?pc_q:a_q;
    assign alu_b=(state_q==FETCH)?32'd4:(state_q==DECODE)?rdata_b:
                 (kind==K_ALU && !alu_immediate)?b_q:immediate;
    assign alu_operation=(state_q!=FETCH && state_q!=DECODE && kind==K_ALU)?decoded_op:4'b0;
    rv32_alu u_alu (
        .a_i(alu_a),.b_i(alu_b),.op_i(alu_operation),.result_o(alu_result),
        .op_valid_o(alu_valid),.equal_o(alu_equal),
        .less_signed_o(alu_less_signed),.less_unsigned_o(alu_less_unsigned)
    );
    always_comb begin
        branch_condition=0;
        case(branch_f3)
            0:branch_condition=alu_equal;
            1:branch_condition=!alu_equal;
            4:branch_condition=alu_less_signed;
            5:branch_condition=!alu_less_signed;
            6:branch_condition=alu_less_unsigned;
            7:branch_condition=!alu_less_unsigned;
            default:begin end
        endcase
    end
    assign target=(kind==K_JALR)?(alu_result & 32'hfffffffe):alu_result;
    assign misaligned=(memory_size==1 && alu_result[0]) ||
                      (memory_size==2 && alu_result[1:0]!=0);
    assign lane_shift={address_q[1:0],3'b000};
    assign lane_mask=(memory_size==0)?(4'b0001<<address_q[1:0]):
                     (memory_size==1)?(4'b0011<<address_q[1:0]):4'b1111;
    assign shifted_load=mem_read_data_i>>lane_shift;
    // Separate wires avoid tool-specific constant-select always_comb warnings.
    wire [31:0] byte_value=unsigned_load?{24'b0,shifted_load[7:0]}:
                                                    {{24{shifted_load[7]}},shifted_load[7:0]};
    wire [31:0] half_value=unsigned_load?{16'b0,shifted_load[15:0]}:
                                                    {{16{shifted_load[15]}},shifted_load[15:0]};
    assign load_value=(memory_size==0)?byte_value:(memory_size==1)?half_value:shifted_load;
    wire fetch_request=!reset_i && state_q==FETCH && pc_q[1:0]==0;
    wire data_request=!reset_i && state_q==MEMORY;
    wire store_request=data_request && kind==K_STORE;
    assign mem_valid_o=fetch_request || data_request;
    assign mem_instruction_o=fetch_request;
    assign mem_address_o=fetch_request?pc_q:data_request?(address_q & 32'hfffffffc):32'b0;
    assign mem_write_data_o=store_request?(b_q<<lane_shift):32'b0;
    assign mem_write_mask_o=store_request?lane_mask:4'b0;
    assign mem_read_mask_o=fetch_request?4'b1111:(data_request && !store_request)?lane_mask:4'b0;

    task automatic raise_trap(input logic [3:0] code,
                              input logic [31:0] word_value,address_value);
        state_q<=TRAP;trap_valid_o<=1;
        trap_code_o<=code;trap_pc_o<=pc_q;
        trap_instruction_o<=word_value;trap_address_o<=address_value;
    endtask

    always_ff @(posedge clk_i) begin
        if(reset_i) begin
            state_q<=FETCH;pc_q<=RESET_PC;ir_q<=0;
            sequential_pc_q<=0;next_pc_q<=0;a_q<=0;b_q<=0;
            result_q<=0;address_q<=0;branch_taken_q<=0;
            trap_valid_o<=0;trap_code_o<=0;trap_pc_o<=0;
            trap_instruction_o<=0;trap_address_o<=0;
            retire_valid_o<=0;retire_pc_o<=0;retire_instruction_o<=0;
            retire_next_pc_o<=0;retire_rd_o<=0;retire_write_data_o<=0;
            retire_memory_address_o<=0;retire_read_mask_o<=0;
            retire_write_mask_o<=0;retire_memory_data_o<=0;
        end else begin
            retire_valid_o<=0;
            case(state_q)
                FETCH:begin
                    if(pc_q[1:0]!=0) raise_trap(1,0,pc_q);
                    else if(mem_done_i) begin
                        if(mem_error_i) raise_trap(2,0,pc_q);
                        else begin ir_q<=mem_read_data_i;sequential_pc_q<=alu_result;state_q<=DECODE;end
                    end
                end
                DECODE:begin
                    a_q<=rdata_a;b_q<=rdata_b;
                    branch_taken_q<=(kind==K_BRANCH)&&branch_condition;
                    state_q<=EXECUTE;
                end
                EXECUTE:begin
                    next_pc_q<=sequential_pc_q;
                    if(illegal || !alu_valid) raise_trap(3,ir_q,0);
                    else if(kind==K_ECALL) raise_trap(8,ir_q,0);
                    else if(kind==K_EBREAK) raise_trap(9,ir_q,0);
                    else if(kind==K_LOAD || kind==K_STORE) begin
                        if(misaligned) raise_trap((kind==K_STORE)?4'd6:4'd4,ir_q,alu_result);
                        else begin address_q<=alu_result;state_q<=MEMORY;end
                    end else if(kind==K_JAL || kind==K_JALR || (kind==K_BRANCH && branch_taken_q)) begin
                        if(target[1:0]!=0) raise_trap(1,ir_q,target);
                        else begin next_pc_q<=target;result_q<=sequential_pc_q;state_q<=COMMIT;end
                    end else begin
                        // FENCE and untaken branches have no register write.
                        result_q<=alu_result;state_q<=COMMIT;
                    end
                end
                MEMORY:begin
                    if(mem_done_i) begin
                        if(mem_error_i) raise_trap((kind==K_STORE)?4'd7:4'd5,ir_q,address_q);
                        else begin
                            if(kind==K_LOAD) result_q<=load_value;
                            state_q<=COMMIT;
                        end
                    end
                end
                COMMIT:begin
                    retire_valid_o<=1;
                    retire_pc_o<=pc_q;retire_instruction_o<=ir_q;retire_next_pc_o<=next_pc_q;
                    retire_rd_o<=register_write?rd:5'b0;
                    retire_write_data_o<=register_write?result_q:32'b0;
                    retire_memory_address_o<=(kind==K_LOAD || kind==K_STORE)?address_q:32'b0;
                    retire_read_mask_o<=(kind==K_LOAD)?lane_mask:4'b0;
                    retire_write_mask_o<=(kind==K_STORE)?lane_mask:4'b0;
                    retire_memory_data_o<=(kind==K_STORE)?(b_q<<lane_shift):32'b0;
                    pc_q<=next_pc_q;state_q<=FETCH;
                end
                TRAP:begin end // Sticky until reset; no implicit trap resume.
                default:raise_trap(3,ir_q,0);
            endcase
        end
    end
endmodule
`default_nettype wire
