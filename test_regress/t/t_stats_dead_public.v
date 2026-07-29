// DESCRIPTION: Verilator: Test statistics for dead variable elimination (public variant)
// SPDX-License-Identifier: CC0-1.0

module t (/*AUTOARG*/);
   // Public dead variable - blocked by public
   logic unused_public /*verilator public_flat_rw*/;
   
   initial begin
      $write("*-* All Finished *-*\n");
      $finish;
   end
endmodule
