// DESCRIPTION: Verilator: Test Dead with public_flat_rw (read-write public)
// SPDX-License-Identifier: CC0-1.0

module t (/*AUTOARG*/);
   // Public read-write dead variable - blocked by public
   logic unused_public_rw /*verilator public_flat_rw*/;
   
   initial begin
      $write("*-* All Finished *-*\n");
      $finish;
   end
endmodule
