// DESCRIPTION: Verilator: Test Life constant propagation with public_flat_rw
// SPDX-License-Identifier: CC0-1.0

module t (/*AUTOARG*/);
   logic [7:0] value /* verilator public_flat_rw */;
   logic [7:0] result;
   
   initial begin
      value = 8'h42;
      result = value;  // Constant propagation blocked by public_flat_rw
      
      $write("*-* All Finished *-*\n");
      $finish;
   end
endmodule
