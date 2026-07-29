// DESCRIPTION: Verilator: Test statistics for assignment deletion
// SPDX-License-Identifier: CC0-1.0

module t (/*AUTOARG*/
   input clk
   );
   
   // Test case 1: Non-public redundant assignment - should be deleted
   logic [7:0] var_nonpublic;
   
   // Test case 2: Public redundant assignment - blocked by public
   logic [7:0] var_public /*verilator public_flat_rw*/;
   
   always @(posedge clk) begin
      // First assignments (redundant, will be overwritten)
      var_nonpublic = 8'h00;
      var_public = 8'h00;
      
      // Second assignments (these remain)
      var_nonpublic = 8'h42;
      var_public = 8'h43;
   end
   
   initial begin
      $write("*-* All Finished *-*\n");
      $finish;
   end
endmodule
