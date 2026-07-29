// DESCRIPTION: Verilator: Test statistics for assignment deletion (independent rejection)
// SPDX-License-Identifier: CC0-1.0

module t (/*AUTOARG*/);
   // Public variable that is also read by DPI - independently rejected
   logic unused_public /*verilator public_flat_rw*/;

   export "DPI-C" function dpi_read;
   function void dpi_read();
      automatic logic temp = unused_public;
   endfunction

   initial begin
      unused_public = 1'b0;
      $write("*-* All Finished *-*\n");
      $finish;
   end
endmodule
