// DESCRIPTION: Verilator: Test statistics for loop unroll const bindings blocked by public_flat_rw
// SPDX-License-Identifier: CC0-1.0

module t;
  int x /* verilator public_flat_rw */;
  
  initial begin
    for (int i = 0; i < 3; i++) begin
      x = i;
      $display(x);
    end
    $write("*-* All Finished *-*\n");
    $finish;
  end
endmodule
