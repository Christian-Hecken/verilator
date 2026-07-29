// DESCRIPTION: Verilator: Test port inlining statistics (public_flat_rd variant)
// SPDX-License-Identifier: CC0-1.0

module sub(input int a, output int b /*verilator public_flat_rd*/);
   assign b = a + 1;
endmodule

module t;
   int x, y;
   
   sub s1(.a(x), .b(y));
   
   initial begin
      x = 5;
      $display(y);
      $write("*-* All Finished *-*\n");
      $finish;
   end
endmodule
