// DESCRIPTION: Verilator: Test Life assignment deletion with public_flat_rd
// SPDX-License-Identifier: CC0-1.0

module t (/*AUTOARG*/);
   logic unused /* verilator public_flat_rd */;
   
   initial begin
      unused = 1'b0;
      unused = 1'b1;  // Second assignment blocked by public_flat_rd
      
      $write("*-* All Finished *-*\n");
      $finish;
   end
endmodule
