module t ();

`ifdef VERILATOR
`systemc_header
extern "C" int force_value();
`verilog
`endif

  wire non_forceable_signal  /*verilator public_flat_rw*/ = 1'b0;
  integer vpi_status;

  initial begin

`ifdef VERILATOR
    vpi_status = $c32("force_value()");
`else
    vpi_status = $force_value;
`endif

    if (vpi_status != 0) begin
      $write("%%Error: t_vpi_forceable_bad.cpp:%0d:", vpi_status);
      $display("C Test failed (could not force value)");
      $stop;
    end

    $display("*-* All Finished *-*");
    $finish;
  end

endmodule
