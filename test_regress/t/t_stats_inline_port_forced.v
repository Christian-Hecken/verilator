// DESCRIPTION: Verilator: Test port inlining independent rejection (forced + public)
// SPDX-License-Identifier: CC0-1.0

module sub(input int a, output int b /*verilator public_flat_rw*/);
   assign b = a + 1;
endmodule

module t;
   int x, y;
   
   sub s1(.a(x), .b(y));
   
   initial begin
      x = 5;
      force s1.b = 10;  // Force makes port non-inlineable, independent of public
      $display(y);
      release s1.b;
      $write("*-* All Finished *-*\n");
      $finish;
   end
endmodule
