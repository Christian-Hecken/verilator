// DESCRIPTION: Verilator: Test state clearing between compilations
// SPDX-License-Identifier: CC0-1.0

module t (/*AUTOARG*/);
   // Dead variable blocked by public
   logic unused_public /*verilator public_flat_rw*/;
   
   initial begin
      $write("*-* All Finished *-*\n");
      $finish;
   end
endmodule
