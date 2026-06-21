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

  logic clockedDriven;
  reg   clk;
  always @(posedge clk) clockedDriven = driver0;

  logic delayDriven;
  always_comb delayDriven = #1 driver0;

  logic onceDriven;
  initial onceDriven = driver0;

  wire arrayDriver0[31:0];
  wire arrayDriver1[15:0];
  wire partiallyDriven0[15:0];
  wire partiallyDriven1[31:0];
  wire partiallyDriven2[15:0];
  assign partiallyDriven0[15:0] = arrayDriver0[15:0];
  assign partiallyDriven1[15:0] = arrayDriver1;
  assign partiallyDriven2 = arrayDriver0[15:0];

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

    // VPI check initial aliases
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

    // Toggle signals back to 0
    #10;
    driver0 = 0;
    driver1 = 0;
    chainDriver = 0;
    #1;
    `checkh(alias0, 1'b0);
    `checkh(alias0_n, 1'b1);
    `checkh(alias1, 1'b0);
    `checkh(alias1_n, 1'b1);
    `checkh(chainAlias2, 1'b0);
    `checkh(chainAlias2_n, 1'b1);

    // VPI modify driver
    #10;
    if (vpi_modify_driver() != 0) $stop;
    
    // Wait for propagation and check
    #1;
    if (vpi_check_modified() != 0) $stop;
    `checkh(driver0, 1'b1);
    `checkh(alias0, 1'b1);
    `checkh(alias0_n, 1'b0);

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
