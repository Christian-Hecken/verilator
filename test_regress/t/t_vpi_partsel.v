// DESCRIPTION: Verilator: Test for vpi_handle_by_name with bit-range part-select
//
// This file ONLY is placed under the Creative Commons Public Domain.
// SPDX-FileCopyrightText: 2025 Wilson Snyder
// SPDX-License-Identifier: CC0-1.0

// Test that vpi_handle_by_name("signal[15:8]", ...) returns a handle with
// correct vpiSize and that vpi_get_value/vpi_put_value operate on the
// selected bits only.

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

   // Parameters for identifier-in-expression tests
   parameter WIDTH `PUBLIC_FLAT_RD = 32;
   parameter BYTE `PUBLIC_FLAT_RD = 8;
   parameter MEM_DEPTH `PUBLIC_FLAT_RD = 4;
   localparam HI_BYTE `PUBLIC_FLAT_RD = 31;
   localparam LO_BYTE `PUBLIC_FLAT_RD = 24;

   // Descending packed range [31:0]
   logic [31:0] sig_desc `PUBLIC_FLAT_RW;

   // verilator lint_off ASCRANGE
   // Ascending packed range [0:31]
   logic [0:31] sig_asc `PUBLIC_FLAT_RW;
   // verilator lint_on ASCRANGE

   // Unpacked array with packed elements
   logic [31:0] mem [0:3] `PUBLIC_FLAT_RW;

   // Wider signal for multi-word part-select tests
   logic [63:0] wide_sig `PUBLIC_FLAT_RW;

   integer status;

`ifdef IVERILOG
   // stop icarus optimizing signals away
   wire redundant = sig_desc[0] ^ sig_asc[0] ^ mem[0][0] ^ wide_sig[0];
`endif

   initial begin
      // Initialize with known bit patterns
      sig_desc = 32'hDEAD_BEEF;
      sig_asc  = 32'h1234_5678;
      mem[0]   = 32'hAAAA_0000;
      mem[1]   = 32'hBBBB_1111;
      mem[2]   = 32'hCCCC_2222;
      mem[3]   = 32'hDDDD_3333;
      wide_sig = 64'hFEDC_BA98_7654_3210;

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
         $write("%%Error: t_vpi_partsel.cpp:%0d: C Test failed\n", status);
         $stop;
      end
      $write("*-* All Finished *-*\n");
      $finish;
   end

endmodule
