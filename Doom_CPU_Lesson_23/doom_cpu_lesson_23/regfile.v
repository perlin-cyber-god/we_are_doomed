module regfile(input wire clk,we,input wire [4:0] rs1,rs2,rd,
 input wire [31:0] wd,output wire [31:0] rv1,rv2);
 reg [31:0] regs[0:31];
 assign rv1=(rs1==0)?32'b0:regs[rs1];
 assign rv2=(rs2==0)?32'b0:regs[rs2];
 always @(posedge clk) if(we && rd!=0) regs[rd]<=wd;
endmodule
