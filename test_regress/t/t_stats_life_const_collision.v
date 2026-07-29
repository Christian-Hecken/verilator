// DESCRIPTION: Verilator: Test Life constant propagation collision detection
// SPDX-License-Identifier: CC0-1.0

module child_a;
   logic [7:0] value /* verilator public_flat_rw */;
   logic [7:0] result;
   initial begin
      value = 8'h42;
      result = value;  // Would propagate constant if not public
   end
endmodule

module child_b;
   logic [7:0] value /* verilator public_flat_rw */;
   logic [7:0] result;
   initial begin
      value = 8'h99;
      result = value;  // Would propagate constant if not public
   end
endmodule

module t (/*AUTOARG*/);
   child_a u1();
   child_b u2();
   
   initial begin
      $write("*-* All Finished *-*\n");
      $finish;
   end
endmodule
