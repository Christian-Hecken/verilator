// DESCRIPTION: Verilator: Test dead variable elimination independent rejection (used + public)
// SPDX-License-Identifier: CC0-1.0

module t (/*AUTOARG*/);
   // Public variable that is actually used - not dead
   // Should not increment either counter
   logic used_public /*verilator public_flat_rw*/;
   
   initial begin
      used_public = 1'b1;  // Variable is used, not dead
      $display(used_public);
      $write("*-* All Finished *-*\n");
      $finish;
   end
endmodule
