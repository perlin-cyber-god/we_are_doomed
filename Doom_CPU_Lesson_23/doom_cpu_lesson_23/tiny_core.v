// Teaching subset: ADD, ADDI, LBU, SB. Not a full RISC-V core.
module tiny_core(input wire clk,reset,
 output reg mem_valid,output reg [31:0] mem_addr,mem_wdata,
 output reg [3:0] mem_wstrb,input wire mem_ready,
 input wire [31:0] mem_rdata,output reg [31:0] pc,
 output wire trapped);
 localparam FETCH=0,DECODE=1,EXECUTE=2,MEMORY=3,WRITEBACK=4,TRAP=5;
 reg [2:0] state;
 reg [31:0] ir,a_reg,b_reg,alu_out,mdr;
 wire is_add=(ir[6:0]==7'h33 && ir[14:12]==0 && ir[31:25]==0);
 wire is_addi=(ir[6:0]==7'h13 && ir[14:12]==0);
 wire is_lbu=(ir[6:0]==7'h03 && ir[14:12]==4);
 wire is_sb=(ir[6:0]==7'h23 && ir[14:12]==0);
 wire supported=is_add|is_addi|is_lbu|is_sb;
 wire [31:0] imm_i={{20{ir[31]}},ir[31:20]};
 wire [31:0] imm_s={{20{ir[31]}},ir[31:25],ir[11:7]};
 wire [31:0] alu_b=is_add?b_reg:(is_sb?imm_s:imm_i);
 wire [31:0] result,rv1,rv2;
 wire rf_we=!reset && state==WRITEBACK && (is_add|is_addi|is_lbu);
 wire [31:0] wd=is_lbu?mdr:alu_out;
 wire [4:0] byte_shift={alu_out[1:0],3'b000};
 wire [31:0] shifted_read=mem_rdata >> byte_shift;
 assign trapped=(state==TRAP);
 regfile rf(.clk(clk),.we(rf_we),.rs1(ir[19:15]),.rs2(ir[24:20]),
  .rd(ir[11:7]),.wd(wd),.rv1(rv1),.rv2(rv2));
 alu arithmetic(.a(a_reg),.b(alu_b),.op(3'd0),.y(result));
 // Unified bus: reads return the aligned 32-bit word containing mem_addr.
 always @* begin
  mem_valid=0;mem_addr=0;mem_wdata=0;mem_wstrb=0;
  if(!reset) begin
   if(state==FETCH) begin mem_valid=1;mem_addr=pc;end
   if(state==MEMORY) begin
    mem_valid=1;mem_addr=alu_out;
    if(is_sb) begin
     mem_wstrb=4'b0001 << alu_out[1:0];
     mem_wdata={24'b0,b_reg[7:0]} << byte_shift;
    end
   end
  end
 end
 always @(posedge clk) begin
  if(reset) begin pc<=0;state<=FETCH;ir<=0;end
  else case(state)
   FETCH: if(mem_valid && mem_ready) begin ir<=mem_rdata;state<=DECODE;end
   DECODE: begin
    if(supported) begin a_reg<=rv1;b_reg<=rv2;state<=EXECUTE;end
    else state<=TRAP;
   end
   EXECUTE: begin
    alu_out<=result;
    state<=(is_lbu|is_sb)?MEMORY:WRITEBACK;
   end
   MEMORY: if(mem_valid && mem_ready) begin
    if(is_lbu) begin mdr<={24'b0,shifted_read[7:0]};state<=WRITEBACK;end
    else begin pc<=pc+4;state<=FETCH;end
   end
   WRITEBACK: begin pc<=pc+4;state<=FETCH;end
   TRAP: state<=TRAP;
   default: state<=TRAP;
  endcase
 end
endmodule
