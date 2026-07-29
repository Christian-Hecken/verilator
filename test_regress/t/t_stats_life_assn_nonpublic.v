// DESCRIPTION: Verilator: Test statistics for assignment deletion
// SPDX-License-Identifier: CC0-1.0

module t (/*AUTOARG*/
   input clk
   );
   
   // Non-public redundant assignment - should be deleted
   logic [7:0] var_nonpublic;
   
   always @(posedge clk) begin
      // First assignment (redundant, will be overwritten)
      var_nonpublic = 8'h00;
      
      // Second assignment (this remains)
      var_nonpublic = 8'h42;
   end
   
   initial begin
      $write("*-* All Finished *-*\n");
      $finish;
   end
endmodule
