// ======================================================================
// This file ONLY is placed under the Creative Commons Public Domain, for
// any use, without warranty.
// SPDX-License-Identifier: CC0-1.0
// ======================================================================

`define STRINGIFY(x) `"x`"

// TODO: Comes from vpi_get -> define in .py
`ifdef VERILATOR_COMMENTS
 `define PUBLIC_FORCEABLE /*verilator public_flat_rw*/  /*verilator forceable*/
`else
 `define PUBLIC_FORCEABLE
`endif

module t;

  reg clk;

  initial begin
    clk = 0;
    forever #1 clk = ~clk;
  end

  Test test (.clk(clk));


endmodule

module Test (
    input clk
);

`ifdef IVERILOG
`elsif USE_VPI_NOT_DPI
`ifdef VERILATOR
  `systemc_header
    extern "C" int tryCheckingForceableString();
    extern "C" int forceValues();
    extern "C" int releaseValues();
    extern "C" int checkValuesForced();
    extern "C" int checkValuesPartiallyForced();
    extern "C" int checkValuesReleased();
  `verilog
`endif
`else
`ifdef VERILATOR
  import "DPI-C" context function int tryCheckingForceableString();
`endif
  import "DPI-C" context function int forceValues();
  import "DPI-C" context function int releaseValues();
  import "DPI-C" context function int checkValuesPartiallyForced();
  import "DPI-C" context function int checkValuesForced();
  import "DPI-C" context function int checkValuesReleased();
`endif




// TODO: Confirm data types in __Syms
  // TODO: vpiVectorVal

  // Force with vpiIntVal
  logic         onebit    `PUBLIC_FORCEABLE; // CData
  logic [ 31:0] intval    `PUBLIC_FORCEABLE; // IData
  logic [ 61:0] quad      `PUBLIC_FORCEABLE; // QData

  // Force with vpiRealVal
  real          real1     `PUBLIC_FORCEABLE; // double

  // Force with vpiStringVal
  logic [ 15:0] textHalf  `PUBLIC_FORCEABLE; // SData
  logic [ 63:0] textLong  `PUBLIC_FORCEABLE; // QData
  logic [511:0] text      `PUBLIC_FORCEABLE; // VlWide
  string        str1      `PUBLIC_FORCEABLE; // std::string

  // Force with vpiBinStrVal, vpiOctStrVal, vpiDecStrVal, vpiHexStrVal
  logic [ 7:0]  binString `PUBLIC_FORCEABLE; // CData
  logic [ 14:0] octString `PUBLIC_FORCEABLE; // SData // TODO: ?
  logic [ 63:0] decString `PUBLIC_FORCEABLE; // QData
  logic [ 63:0] hexString `PUBLIC_FORCEABLE; // QData

  always @(posedge clk) begin
    onebit <= 1;
    intval <= 32'hAAAAAAAA;
    quad <= 62'h2AAAAAAA_AAAAAAAA;

    real1 <= 1.0;

    textHalf <= "Hf";
    textLong <= "Long64b";
    text <= "Verilog Test module";

    binString <= 8'b10101010;
    octString <= 15'o25252; // 0b1010...
    decString <= 64'd12297829382473034410; // 0b1010...
    hexString <= 64'hAAAAAAAAAAAAAAAA; // 0b1010...
  end

  task automatic svForceValues ();
    force onebit = 0;
    force intval = 32'h55555555;
    force quad = 62'h15555555_55555555;
    force real1 = 123456.789;
    force textHalf = "T2";
    force textLong = "44Four44";
    force text = "lorem ipsum";
    force binString = 8'b01010101;
    force octString = 15'o52525;
    force decString = 64'd6148914691236517205;
    force hexString = 64'h5555555555555555;
  endtask

  task automatic svPartiallyForceValues ();
    force intval[15:0] = 16'h5555;
    force quad[30:0] = 31'h55555555;
    force textHalf[7:0] = "2";
    force textLong[31:0] = "ur44";
    force text[63:0] = "em ipsum";
    force binString[3:0] = 4'b0101;

    force octString[6:0] = 7'o125;
    force decString[31:0] = 32'd1431655765;
    force hexString[31:0] = 32'h55555555;
  endtask

  task automatic vpiTryCheckingForceableString ();
  integer vpiStatus = 1; // Default to failed status to ensure that a function *not* getting
                         // called also causes simulation termination
`ifdef VERILATOR
`ifdef USE_VPI_NOT_DPI
    vpiStatus = $c32("tryCheckingForceableString()");
`else
    vpiStatus = tryCheckingForceableString();
`endif
`else
    $stop; // This task only makes sense with Verilator, since other simulators ignore the "verilator forceable" metacomment.
`endif

    if (vpiStatus != 0) begin
      $write("%%Error: t_vpi_force.cpp:%0d:", vpiStatus);
      $display("C Test failed (forcing string either succeeded, but it should have failed, or produced unexpected error message)");
      $stop;
    end
  endtask

  task automatic vpiForceValues ();
  integer vpiStatus = 1;
`ifdef VERILATOR
`ifdef USE_VPI_NOT_DPI
    vpiStatus = $c32("forceValues()");
`else
    vpiStatus = forceValues();
`endif
`elsif IVERILOG
    vpiStatus = $forceValues;
`elsif USE_VPI_NOT_DPI
    vpiStatus = $forceValues;
`else
    vpiStatus = forceValues();
`endif

    if (vpiStatus != 0) begin
      $write("%%Error: t_vpi_force.cpp:%0d:", vpiStatus);
      $display("C Test failed (could not force value)");
      $stop;
    end
  endtask

  task automatic svReleaseValues ();
    release onebit;
    release intval;
    release quad;
    release real1;
    release textHalf;
    release textLong;
    release text;
    release binString;
    release octString;
    release decString;
    release hexString;
  endtask

  task automatic vpiReleaseValues ();
  integer vpiStatus = 1;
`ifdef VERILATOR
`ifdef USE_VPI_NOT_DPI
    vpiStatus = $c32("releaseValues()");
`else
    vpiStatus = releaseValues();
`endif
`elsif IVERILOG
    vpiStatus = $releaseValues;
`elsif USE_VPI_NOT_DPI
    vpiStatus = $releaseValues;
`else
    vpiStatus = releaseValues();
`endif

    if (vpiStatus != 0) begin
      $write("%%Error: t_vpi_force.cpp:%0d:", vpiStatus);
      $display("C Test failed (could not release value)");
      $stop;
    end
  endtask

  task automatic svCheckValuesForced ();
    if(onebit != 0) $stop;
    if(intval != 32'h55555555) $stop;
    if(quad != 62'h15555555_55555555) $stop;
    if(real1 != 123456.789) $stop;
    if(textHalf != "T2") $stop;
    if(textLong != "44Four44") $stop;
    if(text != "lorem ipsum") $stop;
    if(binString != 8'b01010101) $stop;
    if(octString != 15'o52525) $stop;
    if(decString != 64'd6148914691236517205) $stop;
    if(hexString != 64'h5555555555555555) $stop;
  endtask

  task automatic vpiCheckValuesForced ();
  integer vpiStatus = 1;
`ifdef VERILATOR
`ifdef USE_VPI_NOT_DPI
    vpiStatus = $c32("checkValuesForced()");
`else
    vpiStatus = checkValuesForced();
`endif
`elsif IVERILOG
    vpiStatus = $checkValuesForced;
`elsif USE_VPI_NOT_DPI
    vpiStatus = $checkValuesForced;
`else
    vpiStatus = checkValuesForced();
`endif

    if (vpiStatus != 0) begin
      $write("%%Error: t_vpi_force.cpp:%0d:", vpiStatus);
      $display("C Test failed (value after forcing does not match expectation)");
      $stop;
    end
  endtask

  task automatic svCheckValuesPartiallyForced ();
    if(intval != 32'hAAAA_5555) $stop;
    if(quad != 62'h2AAAAAAAD5555555) $stop;
    if(textHalf != "H2") $stop;
    if(textLong != "Lonur44") $stop;
    if(text != "Verilog Tesem ipsum") $stop;
    if(binString != 8'b1010_0101) $stop;
    if(octString != 15'b01010101_1010101) $stop;
    if(decString != 64'hAAAAAAAA_55555555) $stop;
    if(hexString != 64'hAAAAAAAA_55555555) $stop;
  endtask

  task automatic vpiCheckValuesPartiallyForced ();
  integer vpiStatus = 1;
`ifdef VERILATOR
`ifdef USE_VPI_NOT_DPI
    vpiStatus = $c32("checkValuesPartiallyForced()");
`else
    vpiStatus = checkValuesPartiallyForced();
`endif
`elsif IVERILOG
    vpiStatus = $checkValuesPartiallyForced;
`elsif USE_VPI_NOT_DPI
    vpiStatus = $checkValuesPartiallyForced;
`else
    vpiStatus = checkValuesPartiallyForced();
`endif

    if (vpiStatus != 0) begin
      $write("%%Error: t_vpi_force.cpp:%0d:", vpiStatus);
      $display("C Test failed (value after partial forcing does not match expectation)");
      $stop;
    end
  endtask

  task automatic svCheckValuesReleased ();
    if(onebit != 1) $stop;
    if(intval != -1431655766) $stop;
    if(quad != 62'h2AAAAAAA_AAAAAAAA) $stop;
    if(real1 != 1.0) $stop;
    if(textHalf != "Hf") $stop;
    if(textLong != "Long64b") $stop;
    if(text != "Verilog Test module") $stop;
    if(binString != 8'b10101010) $stop;
    if(octString != 15'o25252) $stop;
    if(decString != 64'd12297829382473034410) $stop;
    if(hexString != 64'hAAAAAAAAAAAAAAAA) $stop;
  endtask

  task automatic vpiCheckValuesReleased ();
  integer vpiStatus = 1;
`ifdef VERILATOR
`ifdef USE_VPI_NOT_DPI
    vpiStatus = $c32("checkValuesReleased()");
`else
    vpiStatus = checkValuesReleased();
`endif
`elsif IVERILOG
    vpiStatus = $checkValuesReleased;
`elsif USE_VPI_NOT_DPI
    vpiStatus = $checkValuesReleased;
`else
    vpiStatus = checkValuesReleased();
`endif

    if (vpiStatus != 0) begin
      $write("%%Error: t_vpi_force.cpp:%0d:", vpiStatus);
      $display("C Test failed (value after releasing does not match expectation)");
      $stop;
    end
  endtask

  initial begin
`ifdef WAVES
    $dumpfile(`STRINGIFY(`TEST_DUMPFILE));
    $dumpvars();
`endif

`ifdef VERILATOR
    vpiTryCheckingForceableString();
`endif

    // Wait a bit before triggering the force to see a change in the traces
    #4 vpiForceValues();

    // Time delay to ensure setting and checking values does not happen
    // at the same time, so that the signals can have their values overwritten
    // by other processes
    #4 vpiCheckValuesForced();
       svCheckValuesForced();
    #4 vpiReleaseValues();
    #4 vpiCheckValuesReleased();
       svCheckValuesReleased();

    // Force through VPI, release through Verilog
    #4 vpiForceValues();
    #4 vpiCheckValuesForced();
       svCheckValuesForced();
    #4 svReleaseValues();
    #4 vpiCheckValuesReleased();
       svCheckValuesReleased();

    // Force through Verilog, release through VPI
    #4 svForceValues();
    #4 vpiCheckValuesForced();
       svCheckValuesForced();
    #4 vpiReleaseValues();
    #4 vpiCheckValuesReleased();
       svCheckValuesReleased();

    // Force only some bits, check if __VforceRd yields correct signal,
    // release through VPI
    #4 svPartiallyForceValues();
    #4 vpiCheckValuesPartiallyForced();
       svCheckValuesPartiallyForced();
    #4 vpiReleaseValues();
    #4 vpiCheckValuesReleased();
       svCheckValuesReleased();

    // Force only some bits, check if __VforceRd yields correct signal,
    // release through Verilog
    #4 svPartiallyForceValues();
    #4 vpiCheckValuesPartiallyForced();
       svCheckValuesPartiallyForced();
    #4 svReleaseValues();
    #4 vpiCheckValuesReleased();
       svCheckValuesReleased();


    #5 $display("*-* All Finished *-*");
    $finish;
  end

`ifdef TEST_VERBOSE
  always @(posedge clk or negedge clk) begin
    $display("time: %0t\tclk:%b",$time,clk);

    $display("onebit: %x", onebit);
    $display("intval: %x", intval);
    $display("quad: %x", quad);
    $display("real1: %f", real1);
    $display("textHalf: %s", textHalf);
    $display("textLong: %s", textLong);
    $display("text: %s", text);
    $display("binString: %x", binString);
    $display("octString: %x", octString);
    $display("decString: %x", decString);
    $display("hexString: %x", hexString);

    $display("========================\n");
  end
`endif

endmodule
