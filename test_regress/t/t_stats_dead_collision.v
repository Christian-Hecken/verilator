// DESCRIPTION: Verilator: Test Dead collision detection with pointer identity
// SPDX-License-Identifier: CC0-1.0

module child_a;
   logic state /* verilator public_flat_rw */;
   initial state = 1'b0;
endmodule

module child_b;
   logic state /* verilator public_flat_rw */;
   initial state = 1'b1;
endmodule

module t (/*AUTOARG*/);
   child_a u1();
   child_b u2();
   
   initial begin
      $write("*-* All Finished *-*\n");
      $finish;
   end
endmodule
