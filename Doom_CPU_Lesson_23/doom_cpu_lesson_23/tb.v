`timescale 1ns/1ps
module tb;
 parameter WAIT=0, SOURCE=100, DEST=200;
 reg clk=0,reset=1;
 always #5 clk=~clk;
 wire valid,ready,trapped;
 wire [31:0] addr,wdata,rdata,pc;
 wire [3:0] wstrb;
 reg [7:0] ram[0:255];
 reg pending=0;
 integer remaining=0,stores=0,cycles=0,i;
 reg [31:0] held_addr,held_data;
 reg [3:0] held_mask;
 wire [31:0] aligned_addr={addr[31:2],2'b00};
 assign ready=pending && remaining==0;
 assign rdata={ram[aligned_addr+3],ram[aligned_addr+2],
               ram[aligned_addr+1],ram[aligned_addr]};
 tiny_core dut(.clk(clk),.reset(reset),.mem_valid(valid),
  .mem_addr(addr),.mem_wdata(wdata),.mem_wstrb(wstrb),
  .mem_ready(ready),.mem_rdata(rdata),.pc(pc),.trapped(trapped));
 task put_word;
 input integer at;input [31:0] word;
 begin
  ram[at]=word[7:0];ram[at+1]=word[15:8];
  ram[at+2]=word[23:16];ram[at+3]=word[31:24];
 end
 endtask
 initial begin
  if(WAIT<0 || SOURCE<100 || SOURCE>103 || DEST<200 || DEST>203)
   $fatal(1,"Use WAIT >= 0, SOURCE 100..103, DEST 200..203");
  for(i=0;i<256;i=i+1) ram[i]=8'hAA;
  put_word(0,(SOURCE<<20)|32'h00000093); // addi x1,x0,SOURCE
  put_word(4,32'h0000C183);            // lbu x3,0(x1)
  put_word(8,(DEST<<20)|32'h00000113);  // addi x2,x0,DEST
  put_word(12,32'h00310023);           // sb x3,0(x2)
  ram[SOURCE]=5;ram[DEST]=0;
  repeat(2) @(negedge clk);
  reset=0;
  wait(stores==1);
  @(negedge clk);
  if(ram[DEST]!==8'd5 || pc!==32'd16) $fatal(1,"Wrong store or PC");
  if(dut.rf.regs[1]!==SOURCE || dut.rf.regs[2]!==DEST ||
     dut.rf.regs[3]!==32'd5) $fatal(1,"Wrong register result");
  if(ram[DEST-1]!==8'hAA || ram[DEST+1]!==8'hAA)
   $fatal(1,"Store damaged neighbour byte");
  if(stores!=1 || trapped) $fatal(1,"Duplicate store or unexpected trap");
  $display("PASS WAIT=%0d SOURCE=%0d DEST=%0d cycles=%0d",WAIT,SOURCE,DEST,cycles);
  $finish;
 end
 always @(posedge clk) begin
  if(reset) begin pending<=0;remaining<=0;stores<=0;cycles<=0;end
  else begin
   cycles<=cycles+1;
   if(cycles>1000) $fatal(1,"Timeout");
   if(trapped) $fatal(1,"Unexpected unsupported instruction");
   if(pending) begin
    if(!valid || addr!==held_addr || wdata!==held_data || wstrb!==held_mask)
     $fatal(1,"Request changed before acknowledgement");
    if(remaining>0) remaining<=remaining-1;
    else if(valid && ready) begin
     pending<=0;
     if(|wstrb) begin
      if(addr!==DEST || wstrb!==(4'b0001<<addr[1:0]))
       $fatal(1,"Unexpected store address or byte lanes");
      for(i=0;i<4;i=i+1) if(wstrb[i]) ram[aligned_addr+i]<=wdata[i*8 +: 8];
      stores<=stores+1;
     end
    end
   end else if(valid) begin
    if(addr>=256) $fatal(1,"Memory out of range");
    pending<=1;remaining<=WAIT;
    held_addr<=addr;held_data<=wdata;held_mask<=wstrb;
   end
  end
 end
endmodule
