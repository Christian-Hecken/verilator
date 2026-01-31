// ======================================================================
// This file ONLY is placed under the Creative Commons Public Domain, for
// any use, without warranty.
// SPDX-License-Identifier: CC0-1.0
// ======================================================================

import "DPI-C" context function int checkForcedSignal();

module t;

  wire data  /*verilator public_flat_rd*/ = 1;

  integer vpiStatus = 1;
  initial begin
    force data = 0;
    vpiStatus = #1 checkForcedSignal();
    if (vpiStatus != 0) begin
      $write("%%Error: t_vpi_get_forced_non_forceable_signal.cpp:%0d:", vpiStatus);
      $display("C Test failed (vpi_get_value yielded wrong result)");
      $stop;
    end
    $finish;
  end

endmodule
