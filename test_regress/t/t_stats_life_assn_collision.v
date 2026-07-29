// DESCRIPTION: Verilator: Test Life assignment deletion collision detection
// SPDX-License-Identifier: CC0-1.0

module child_a;
   logic unused /* verilator public_flat_rw */;
   initial begin
      unused = 1'b0;
      unused = 1'b1;  // Second assignment would be deletable if not public
   end
endmodule

module child_b;
   logic unused /* verilator public_flat_rw */;
   initial begin
      unused = 1'b0;
      unused = 1'b1;  // Second assignment would be deletable if not public
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
