// Minimal test_regress placement for force/write/release matrix experiments
// SPDX-License-Identifier: CC0-1.0

`ifdef VERILATOR
`define PUBLIC_FORCEABLE /*verilator public_flat_rw*/ /*verilator forceable*/
`else
`define PUBLIC_FORCEABLE
`endif

module t;
  // Clock is driven from the C++ test runner to avoid # delays / infinite loops
  reg clk = 0; // driven by C++ test main

  Test test (.clk(clk));
endmodule

module Test(input logic clk);
  // Signals for matrix tests
  logic in_nc /*verilator public_flat_rw*/; // non-continuously assigned (driven in always_ff)
  logic out_nc `PUBLIC_FORCEABLE;
  wire out_c `PUBLIC_FORCEABLE;

  // Model: drive out_nc from in_nc on clock; continuous assignment for out_c
  always_ff @(posedge clk) begin
    out_nc = in_nc;
  end
  assign out_c = in_nc;

  // Exported SV helpers for C++-driven test runner
  // Write a value into the non-continuously assigned input
  export "DPI-C" task sv_write_in_nc;
  task sv_write_in_nc(input bit val);
    in_nc = val;
  endtask

  // Force / release helpers for the non-continuously assigned output
  export "DPI-C" task sv_force_out_nc;
  task sv_force_out_nc(input bit val);
    force out_nc = val;
  endtask

  export "DPI-C" task sv_release_out_nc;
  task sv_release_out_nc();
    release out_nc;
  endtask

  // Force / release helpers for the continuously assigned output
  export "DPI-C" task sv_force_out_c;
  task sv_force_out_c(input bit val);
    force out_c = val;
  endtask

  export "DPI-C" task sv_release_out_c;
  task sv_release_out_c();
    release out_c;
  endtask

  // Simple accessors for checks from C++
  export "DPI-C" function sv_get_in_nc;
  function int sv_get_in_nc(); sv_get_in_nc = in_nc ? 1 : 0; endfunction

  export "DPI-C" function sv_get_out_nc;
  function int sv_get_out_nc(); sv_get_out_nc = out_nc ? 1 : 0; endfunction

  export "DPI-C" function sv_get_out_c;
  function int sv_get_out_c(); sv_get_out_c = out_c ? 1 : 0; endfunction

  initial begin
    $display("t_force_matrix: sv testbench ready");
  end
endmodule
