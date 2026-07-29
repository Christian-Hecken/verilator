// DESCRIPTION: Verilator: Test statistics for constant propagation
// SPDX-License-Identifier: CC0-1.0

module t (/*AUTOARG*/
   input clk
   );
   
   // Public variable with constant value - propagation blocked
   logic [7:0] var_public /*verilator public_flat_rd*/;
   logic [7:0] result;
   
   always @(posedge clk) begin
      var_public = 8'h42;
      result = var_public;  // Would propagate constant if not public
   end
   
   initial begin
      $write("*-* All Finished *-*\n");
      $finish;
   end
endmodule
