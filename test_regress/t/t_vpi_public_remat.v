// DESCRIPTION: Verilator: Verilog Test module
//
// Test for public signal rematerialization feature
// Public signals with simple combinatorial logic should be optimized away
// but still accessible via VPI through generated getter functions
//
// SPDX-License-Identifier: CC0-1.0

import "DPI-C" context function int mon_drive(
  int test_case,
  int a_val,
  int b_val
);
import "DPI-C" context function int mon_check(int test_case);

module t (
    input clk,
    input [7:0] a  /*verilator public_flat_rw*/,
    input [7:0] b  /*verilator public_flat_rw*/,
    output reg [7:0] result
);

  // Simple combinatorial signals that should be rematerialized
  wire [ 7:0] sum  /*verilator public_flat_rd*/;
  wire [ 7:0] product  /*verilator public_flat_rd*/;
  wire [ 7:0] shifted  /*verilator public_flat_rd*/;
  wire [15:0] wide_sum  /*verilator public_flat_rd*/;

  // More complex expression
  wire [ 7:0] complex  /*verilator public_flat_rd*/;

  // Cascading signal - depends on other getter-eligible signals
  wire [ 7:0] cascaded  /*verilator public_flat_rd*/;

  // This should NOT be rematerialized (RW access)
  reg  [ 7:0] rw_signal  /*verilator public_flat_rw*/;

  // Combinatorial assignments
  assign sum = a + b;
  assign product = a * b;
  assign shifted = a << 2;
  assign wide_sum = {8'h0, a} + {8'h0, b};
  assign complex = (a & b) | (a ^ b);
  assign cascaded = sum ^ product;  // Uses other getter-eligible signals

  integer status;

  initial begin
    rw_signal = 8'h42;

    // Test case 1: a=5, b=3
    status = mon_drive(1, 5, 3);
    $display("DPI-C drive status: %0d", status);
    if (status != 0) begin
      $write("%%Error: t_vpi_public_remat.cpp:%0d: C Test failed\n", status);
      $stop;
    end
    #10;  // Allow combinatorial logic to settle
    status = mon_check(1);
    if (status != 0) begin
      $write("%%Error: t_vpi_public_remat.cpp:%0d: C Test failed\n", status);
      $stop;
    end

    // Test case 2: a=255, b=1
    status = mon_drive(2, 255, 1);
    if (status != 0) begin
      $write("%%Error: t_vpi_public_remat.cpp:%0d: C Test failed\n", status);
      $stop;
    end
    #10;  // Allow combinatorial logic to settle
    status = mon_check(2);
    if (status != 0) begin
      $write("%%Error: t_vpi_public_remat.cpp:%0d: C Test failed\n", status);
      $stop;
    end

    // Test case 3: a=0xAA, b=0x55
    status = mon_drive(3, 'hAA, 'h55);
    if (status != 0) begin
      $write("%%Error: t_vpi_public_remat.cpp:%0d: C Test failed\n", status);
      $stop;
    end
    #10;  // Allow combinatorial logic to settle
    status = mon_check(3);
    if (status != 0) begin
      $write("%%Error: t_vpi_public_remat.cpp:%0d: C Test failed\n", status);
      $stop;
    end

    $write("*-* All Finished *-*\n");
    $finish;
  end

  always @(posedge clk) begin
    result <= sum;
  end

endmodule

