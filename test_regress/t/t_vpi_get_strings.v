// ======================================================================
// This file ONLY is placed under the Creative Commons Public Domain, for
// any use, without warranty.
// SPDX-License-Identifier: CC0-1.0
// ======================================================================

module t;

  `systemc_header
    extern "C" int getBinStrings();
  `verilog

  logic [ 7:0]  binString1 /*verilator public_flat_rw*/ = 8'b10101010;
  logic [ 7:0]  binString2 /*verilator public_flat_rw*/ = 8'b00001111;
  logic [ 7:0]  binString3 /*verilator public_flat_rw*/ = 8'b10101010;
  logic [ 7:0]  decString1 /*verilator public_flat_rw*/ = 8'd123;

  integer vpiStatus = 1;

  initial begin
    vpiStatus = $c32("getBinStrings()");

    if (vpiStatus != 0) begin
      $stop;
    end

    $display("*-* All Finished *-*");
    $finish;
  end

endmodule
