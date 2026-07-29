// DESCRIPTION: Verilator: Test statistics for const propagation (independent rejection)
// SPDX-License-Identifier: CC0-1.0

module t (/*AUTOARG*/);
   // Public variable that is constant but also written by DPI - independently rejected
   logic unused_public /*verilator public_flat_rw*/;
   
   // Assign a constant value
   initial unused_public = 1'b0;

   export "DPI-C" function dpi_write;
   function void dpi_write();
      // Write directly to the public variable
      unused_public = 1'b1;
   endfunction

   initial begin
      dpi_write();
      $write("*-* All Finished *-*\n");
      $finish;
   end
endmodule
