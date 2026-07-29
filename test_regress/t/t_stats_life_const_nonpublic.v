// DESCRIPTION: Verilator: Test statistics for constant propagation
// SPDX-License-Identifier: CC0-1.0

module t (/*AUTOARG*/
   input clk
   );
   
   // Non-public variable with constant value - propagation should succeed
   logic [7:0] var_nonpublic;
   logic [7:0] result;
   
   always @(posedge clk) begin
      var_nonpublic = 8'h42;
      result = var_nonpublic;  // Should propagate constant
   end
   
   initial begin
      $write("*-* All Finished *-*\n");
      $finish;
   end
endmodule
