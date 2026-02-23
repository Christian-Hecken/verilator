// DESCRIPTION: Verilator: Verilog Test module for VPI unpacked array slice access
//
// This file ONLY is placed under the Creative Commons Public Domain.
// SPDX-FileCopyrightText: 2025 Wilson Snyder
// SPDX-License-Identifier: CC0-1.0

// Minimal test for vpi_handle_by_name with unpacked array slice syntax.
// Tests whether vpi_handle_by_name("mem[1:2][3]", ...) works,
// where [1:2] is a slice on the first unpacked dimension and [3] is
// a scalar index on the second. Cross-check against other simulators.

`ifdef USE_VPI_NOT_DPI
//We call it via $c so we can verify DPI isn't required - see bug572
`else
import "DPI-C" context function int mon_check();
`endif

`ifdef VERILATOR_COMMENTS
 `define PUBLIC_FLAT_RD /*verilator public_flat_rd*/
 `define PUBLIC_FLAT_RW /*verilator public_flat_rw @(posedge clk)*/
`else
 `define PUBLIC_FLAT_RD
 `define PUBLIC_FLAT_RW
`endif

module t (/*AUTOARG*/
   // Inputs
   input clk `PUBLIC_FLAT_RD
   );

`ifdef VERILATOR
`systemc_header
extern "C" int mon_check();
`verilog
`endif

   // 2D unpacked array: mem2d[1:2][3:4], each element is 32 bits
   logic [31:0] mem2d [1:2][3:4] `PUBLIC_FLAT_RW;

   integer status;

`ifdef IVERILOG
   // stop icarus optimizing signals away
   wire redundant = mem2d[1][3][0];
`endif

   initial begin
      // Initialize with known values for VPI to read
      mem2d[1][3] = 32'hAAAA_1111;
      mem2d[1][4] = 32'hAAAA_2222;
      mem2d[2][3] = 32'hBBBB_3333;
      mem2d[2][4] = 32'hBBBB_4444;

`ifdef VERILATOR
      status = $c32("mon_check()");
`endif
`ifdef IVERILOG
      status = $mon_check();
`endif
`ifndef USE_VPI_NOT_DPI
      status = mon_check();
`endif
      if (status != 0) begin
         $write("%%Error: t_vpi_multi_index.cpp:%0d: C Test failed\n", status);
         $stop;
      end
      $write("*-* All Finished *-*\n");
      $finish;
   end

endmodule : t
