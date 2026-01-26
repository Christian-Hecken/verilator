`ifdef VERILATOR
import "DPI-C" context function int set_onebit_dpi_import();
`endif

module t;
`ifdef VERILATOR
`systemc_header
  extern "C" int set_onebit_vpi_sysfun();
`verilog
`endif

`ifdef VERILATOR
  task automatic set_onebit_dpi_task_wrap(input din);
    // verilator no_inline_task
    integer status;
    status = set_onebit_dpi_import();
    if (status != 0) begin
      $display("%%Error: t_vpi_nodelay_put_get.cpp:%0d: C Test failed", status);
      $stop;
    end
  endtask
`endif

  reg onebit_dpi_import_nodelay  /*verilator public_flat_rw*/  /*verilator forceable*/ = 0;
  reg onebit_vpi_sysfun_nodelay  /*verilator public_flat_rw*/  /*verilator forceable*/ = 0;
  
  // Delay the callback to time step 1, then call vpi_put_value with
  // vpiNoDelay
  reg onebit_vpi_delayed_callback_vpiNoDelay  /*verilator public_flat_rw*/  /*verilator forceable*/ = 0;

  // Call callback immediately upon simulation start, but use vpi_put_value
  // with inertialDelay of 1 timestep
  reg onebit_vpi_immediate_callback_inertialdelay  /*verilator public_flat_rw*/  /*verilator forceable*/ = 0;

  integer status = 1;
  initial begin
`ifdef VERILATOR
    status = $c32("set_onebit_vpi_sysfun()");
`else
    status = $set_onebit_vpi_sysfun;
`endif
    if (status != 0) begin
      $display("%%Error: t_vpi_nodelay_put_get.cpp:%0d: C Test failed", status);
      $stop;
    end

`ifdef VERILATOR
    status = set_onebit_dpi_import();
    if (status != 0) begin
      $display("%%Error: t_vpi_nodelay_put_get.cpp:%0d: C Test failed", status);
      $stop;
    end
    set_onebit_dpi_task_wrap(onebit_dpi_import_nodelay);
`endif

    for(int i=0;i<3;++i) begin
`ifdef VERILATOR
    $display("[%0t] onebit_dpi_import_nodelay: %b", $time, onebit_dpi_import_nodelay);
`endif
    $display("[%0t] onebit_vpi_sysfun_nodelay: %b", $time, onebit_vpi_sysfun_nodelay);
    $display("[%0t] onebit_vpi_delayed_callback_vpiNoDelay: %b", $time, onebit_vpi_delayed_callback_vpiNoDelay);
    $display("[%0t] onebit_vpi_immediate_callback_inertialdelay: %b", $time, onebit_vpi_immediate_callback_inertialdelay);
    #1;
    end

    $finish;
  end
endmodule
