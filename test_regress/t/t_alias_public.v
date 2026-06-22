// DESCRIPTION: Verilator: Verilog Test module
//
// This file ONLY is placed under the Creative Commons Public Domain, for
// any use, without warranty, 2024 by Wilson Snyder.
// SPDX-License-Identifier: CC0-1.0

`define checkh(gotv,expv) do if ((gotv) !== (expv)) begin $write("%%Error: %s:%0d:  got='h%x exp='h%x\n", `__FILE__,`__LINE__, (gotv), (expv)); $stop; end while(0);

`ifdef USE_VPI_NOT_DPI
//We call it via $c so we can verify DPI isn't required
`else
import "DPI-C" context function int vpi_check_aliases();
import "DPI-C" context function int vpi_modify_driver();
import "DPI-C" context function int vpi_check_modified();
`endif

// Submodule to test port aliases
module sub (
  input wire in_a,
  input wire in_b,
  output wire out_x,
  output wire out_y
);
  assign out_x = in_a & in_b;
  assign out_y = in_a | in_b;
endmodule

module t;

`ifdef VERILATOR
`systemc_header
extern "C" int vpi_check_aliases();
extern "C" int vpi_modify_driver();
extern "C" int vpi_check_modified();
`verilog
`endif

  // True aliases
  wire alias0  /*verilator public_flat_rw*/;
  reg driver0;
  assign alias0 = driver0;
  
  bit alias1  /*verilator public_flat_rw*/;
  bit driver1;
  always_comb alias1 = driver1;

  // Combinatorial dependents to check propagation (inverters, not aliases)
  wire alias0_n = ~alias0;
  wire alias1_n = ~alias1;

  wire chainAlias0;
  wire chainAlias1;
  wire chainAlias2  /*verilator public_flat_rw*/;
  reg chainDriver;
  assign chainAlias0 = chainDriver;
  assign chainAlias1 = chainAlias0;
  assign chainAlias2 = chainAlias1;

  // Combinatorial dependent to check chain propagation (inverter)
  wire chainAlias2_n = ~chainAlias2;

  /* verilator lint_off UNOPTFLAT */
  wire cycle0;
  wire cycle1;
  wire cycle2  /*verilator public_flat_rw*/;
  assign cycle0 = cycle1;
  assign cycle1 = cycle2;
  assign cycle2 = cycle0;
  /* verilator lint_on UNOPTFLAT */

  // Not aliases
  wire multiDriver0;
  wire multiDriver1;

  /* verilator lint_off MULTIDRIVEN */
  bit  alwaysMultiDriven;
  always_comb alwaysMultiDriven = multiDriver0;
  always_comb alwaysMultiDriven = multiDriver1;
  /* verilator lint_on MULTIDRIVEN */

  // Use alias0 as driver to ensure that it stays aliased, and only signals on the lhs inside
  // aliasing-ineligible environments are affected
  logic clockedDriven;
  reg   clk;
  always @(posedge clk) clockedDriven = alias0;

  logic delayDriven;
  always_comb delayDriven = #1 alias0;

  logic onceDriven;
  initial onceDriven = alias0;

  wire [31:0]arrayDriver0;
  wire [15:0]arrayDriver1;
  wire [15:0]partiallyDriven0;
  wire [31:0]partiallyDriven1;
  wire [15:0]partiallyDriven2;
  wire [15:0]partiallyDriven3;
  wire [15:0]partiallyDriven4;
  wire [15:0]partiallyDriven5;
  assign partiallyDriven0[15:0] = arrayDriver0[15:0];
  assign partiallyDriven1[15:0] = arrayDriver1;
  assign partiallyDriven2 = arrayDriver0[15:0];
  assign {partiallyDriven3,partiallyDriven4} = arrayDriver0;

  // Port alias testing - signals connected to submodule ports
  wire port_in_a  /*verilator public_flat_rw*/;
  wire port_in_b  /*verilator public_flat_rw*/;
  wire port_out_x  /*verilator public_flat_rw*/;
  wire port_out_y  /*verilator public_flat_rw*/;
  
  reg port_driver_a;
  reg port_driver_b;
  
  assign port_in_a = port_driver_a;
  assign port_in_b = port_driver_b;
  
  sub u_sub (
    .in_a(port_in_a),
    .in_b(port_in_b),
    .out_x(port_out_x),
    .out_y(port_out_y)
  );

  // Clock generation
  initial begin
    clk = 0;
    forever #5 clk = ~clk;
  end

  // Self-checking
  initial begin
    driver0 = 0;
    driver1 = 0;
    chainDriver = 0;
    port_driver_a = 0;
    port_driver_b = 0;

    // Check initial state
    #1;
    `checkh(alias0, 1'b0);
    `checkh(alias0_n, 1'b1);
    `checkh(alias1, 1'b0);
    `checkh(alias1_n, 1'b1);
    `checkh(chainAlias0, 1'b0);
    `checkh(chainAlias1, 1'b0);
    `checkh(chainAlias2, 1'b0);
    `checkh(chainAlias2_n, 1'b1);
    `checkh(port_out_x, 1'b0);
    `checkh(port_out_y, 1'b0);

    // VPI check initial aliases (includes port aliases)
    if (vpi_check_aliases() != 0) $stop;

    // Test alias0 and its dependent
    #10;
    driver0 = 1;
    #1;
    `checkh(alias0, 1'b1);
    `checkh(alias0_n, 1'b0);

    // Test alias1 and its dependent
    #10;
    driver1 = 1;
    #1;
    `checkh(alias1, 1'b1);
    `checkh(alias1_n, 1'b0);

    // Test chain propagation
    #10;
    chainDriver = 1;
    #1;
    `checkh(chainAlias0, 1'b1);
    `checkh(chainAlias1, 1'b1);
    `checkh(chainAlias2, 1'b1);
    `checkh(chainAlias2_n, 1'b0);

    // Test port aliases with different input combinations
    #10;
    port_driver_a = 0;
    port_driver_b = 0;
    #1;
    `checkh(port_in_a, 1'b0);
    `checkh(port_in_b, 1'b0);
    `checkh(port_out_x, 1'b0);  // 0 & 0 = 0
    `checkh(port_out_y, 1'b0);  // 0 | 0 = 0

    #10;
    port_driver_a = 1;
    port_driver_b = 0;
    #1;
    `checkh(port_in_a, 1'b1);
    `checkh(port_in_b, 1'b0);
    `checkh(port_out_x, 1'b0);  // 1 & 0 = 0
    `checkh(port_out_y, 1'b1);  // 1 | 0 = 1

    #10;
    port_driver_a = 1;
    port_driver_b = 1;
    #1;
    `checkh(port_in_a, 1'b1);
    `checkh(port_in_b, 1'b1);
    `checkh(port_out_x, 1'b1);  // 1 & 1 = 1
    `checkh(port_out_y, 1'b1);  // 1 | 1 = 1

    // Toggle signals back to 0
    #10;
    driver0 = 0;
    driver1 = 0;
    chainDriver = 0;
    port_driver_a = 0;
    port_driver_b = 0;
    #1;
    `checkh(alias0, 1'b0);
    `checkh(alias0_n, 1'b1);
    `checkh(alias1, 1'b0);
    `checkh(alias1_n, 1'b1);
    `checkh(chainAlias2, 1'b0);
    `checkh(chainAlias2_n, 1'b1);
    `checkh(port_out_x, 1'b0);
    `checkh(port_out_y, 1'b0);

    // VPI modify drivers (includes port drivers)
    #10;
    if (vpi_modify_driver() != 0) $stop;
    
    // Wait for propagation and check (includes port outputs)
    #1;
    if (vpi_check_modified() != 0) $stop;
    `checkh(driver0, 1'b1);
    `checkh(alias0, 1'b1);
    `checkh(alias0_n, 1'b0);
    `checkh(port_driver_a, 1'b1);
    `checkh(port_driver_b, 1'b1);
    `checkh(port_in_a, 1'b1);
    `checkh(port_in_b, 1'b1);
    `checkh(port_out_x, 1'b1);
    `checkh(port_out_y, 1'b1);

    // Multiple transitions
    #10;
    driver0 = 1;
    #1;
    `checkh(alias0, 1'b1);
    `checkh(alias0_n, 1'b0);
    #5;
    driver0 = 0;
    #1;
    `checkh(alias0, 1'b0);
    `checkh(alias0_n, 1'b1);

    $write("*-* All Finished *-*\n");
    $finish;
  end

endmodule
