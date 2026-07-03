// DESCRIPTION: Verilator: VPI rematerialized public signal persistent handle test
//
// This file ONLY is placed under the Creative Commons Public Domain.
// SPDX-License-Identifier: CC0-1.0

module t;
   import "DPI-C" context function int cache_remat_handle();
   import "DPI-C" context function int check_remat_value(input int expected);

   logic a;
   logic b;

   logic remat /*verilator public_flat_rd*/;

   assign remat = a ^ b;

   initial begin
      integer status;
      a = 1'b0;
      b = 1'b0;
      #1;

      // Acquire the VPI handle exactly once.
      status = cache_remat_handle();
      if (status != 0) $stop;

      // Initial value: 0 ^ 0 = 0
      status = check_remat_value(0);
      if (status != 0) $stop;

      a = 1'b1;
      b = 1'b0;
      #1;

      // Same cached handle must now observe 1 ^ 0 = 1.
      status = check_remat_value(1);
      if (status != 0) $stop;

      a = 1'b1;
      b = 1'b1;
      #1;

      // Same cached handle must now observe 1 ^ 1 = 0.
      status = check_remat_value(0);
      if (status != 0) $stop;

      $write("*-* All Finished *-*\n");
      $finish;
   end
endmodule
