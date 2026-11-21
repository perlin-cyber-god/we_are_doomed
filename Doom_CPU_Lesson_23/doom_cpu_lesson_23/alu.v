module alu(input wire [31:0] a,b,input wire [2:0] op,
 output reg [31:0] y);
 always @* begin
  case(op)
   0:y=a+b; 1:y=a-b; 2:y=a&b; 3:y=a|b; 4:y=a^b;
   default:y=32'b0;
  endcase
 end
endmodule
