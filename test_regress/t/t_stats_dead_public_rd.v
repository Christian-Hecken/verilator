// DESCRIPTION: Verilator: Test Dead with public_flat_rd (read-only public)
// SPDX-License-Identifier: CC0-1.0

module t (/*AUTOARG*/);
   // Public read-only dead variable - should be eliminated (read-only doesn't block)
   logic unused_public_rd /*verilator public_flat_rd*/;
   
   initial begin
      $write("*-* All Finished *-*\n");
      $finish;
   end
endmodule
