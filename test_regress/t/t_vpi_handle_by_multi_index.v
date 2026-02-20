// DESCRIPTION: Verilator: Verilog Test module
//
// This program is free software; you can redistribute it and/or modify it
// under the terms of either the GNU Lesser General Public License Version 3
// or the Perl Artistic License Version 2.0.
// SPDX-FileCopyrightText: 2024 Wilson Snyder
// SPDX-License-Identifier: LGPL-3.0-only OR Artistic-2.0

module t (/*AUTOARG*/);

   // Two-dimensional array: unpacked array of packed regs
   // quads is an array with indices [2:3], each element is [0:61] bits
   /* verilator lint_off ASCRANGE */
   reg [0:61]   quads[2:3]      /*verilator public_flat_rw */;
   /* verilator lint_on ASCRANGE */

   // Another 2D array for testing
   reg [7:0]    mem_2d[0:3][0:7] /*verilator public_flat_rw */;

   // Single dimension array
   reg [31:0]   mem_1d[0:15]   /*verilator public_flat_rw */;

   import "DPI-C" function int mon_check();

   integer i, j;
   integer status;

   initial begin
      // Initialize the arrays
      quads[2] = 62'h0123456789abcdef;
      quads[3] = 62'hfedcba9876543210;

      for (i = 0; i < 4; i++) begin
         for (j = 0; j < 8; j++) begin
            mem_2d[i][j] = 8'(((i * 8) + j));
         end
      end

      for (i = 0; i < 16; i++) begin
         mem_1d[i] = i * 256;
      end

      status = mon_check();
      if (status != 0) begin
         $write("%%Error: test failed\n");
         $stop;
      end
      $display("*-* All Finished *-*");
      $finish;
   end

endmodule
