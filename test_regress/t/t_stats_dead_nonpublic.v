// DESCRIPTION: Verilator: Test statistics for dead variable elimination (non-public baseline)
// SPDX-License-Identifier: CC0-1.0

module t (/*AUTOARG*/);
   // Non-public dead variable - should be eliminated
   logic unused_var;
   
   initial begin
      $write("*-* All Finished *-*\n");
      $finish;
   end
endmodule
