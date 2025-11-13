`define STRINGIFY(x) `"x`"

module t ();

`ifdef VERILATOR
`systemc_header
extern "C" int baselineValue();
extern "C" int forceValues();
extern "C" int releaseValues();
extern "C" int checkValuesForced();
extern "C" int checkValuesReleased();
`verilog
`endif

  reg clk;

  initial begin
    clk = 0;
    forever #1 clk = ~clk;
  end

  Test test (.clk(clk));

  integer vpiStatus;

  initial begin
`ifndef VERILATOR
`ifdef WAVES
    $dumpfile(`STRINGIFY(`TEST_DUMPFILE));
    $dumpvars();
`endif
`endif

    #3;  // Wait a bit before triggering the force to see a change in the traces

`ifdef VERILATOR
    vpiStatus = $c32("forceValues()");
`else
    vpiStatus = $forceValues;
`endif

    if (vpiStatus != 0) begin
      $write("%%Error: t_vpi_force_minimal.cpp:%0d:", vpiStatus);
      $display("C Test failed (could not force value)");
      $stop;
    end

    #4;  // Time delay to ensure setting and checking values does not happen
         // at the same time, so that the signals can have their values overwritten
         // by other processes

`ifdef VERILATOR
    vpiStatus = $c32("checkValuesForced()");
`else
    vpiStatus = $checkValuesForced;
`endif

    if (vpiStatus != 0) begin
      $write("%%Error: t_vpi_force_minimal.cpp:%0d:", vpiStatus);
      $display("C Test failed (value after forcing does not match expectation)");
      $stop;
    end

    #3;


`ifdef VERILATOR
    vpiStatus = $c32("releaseValues()");
`else
    vpiStatus = $releaseValues;
`endif

    if (vpiStatus != 0) begin
      $write("%%Error: t_vpi_force_minimal.cpp:%0d:", vpiStatus);
      $display("C Test failed (could not release value)");
      $stop;
    end

    #4;

`ifdef VERILATOR
    vpiStatus = $c32("checkValuesReleased()");
`else
    vpiStatus = $checkValuesReleased;
`endif

    if (vpiStatus != 0) begin
      $write("%%Error: t_vpi_force_minimal.cpp:%0d:", vpiStatus);
      $display("C Test failed (value after releasing does not match expectation)");
      $stop;
    end

    #5 $display("*-* All Finished *-*");
    $finish;
  end

endmodule

module Test (
    input clk
);

  logic clockedReg  /*verilator public_flat_rw*/  /*verilator forceable*/, assignedWire;
`ifdef VERILATOR
  assign assignedWire = $c32("baselineValue()");
`else
  initial begin
    assignedWire = $baselineValue;
  end
`endif
  always @(posedge clk) begin
    clockedReg <= assignedWire;
  end

`ifdef TEST_VERBOSE
  initial begin
    $display("[time]\t\tclk\t\tassignedWire\tclockedReg");
    forever #1 $display("[%0t]\t\t%b\t\t%b\t\t%b", $time, clk, assignedWire, clockedReg);
  end
`endif

endmodule
