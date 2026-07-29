// DESCRIPTION: Verilator: Test statistics for loop unroll const bindings (non-public)
// SPDX-License-Identifier: CC0-1.0

module t;
  initial begin
    int x;
    for (int i = 0; i < 3; i++) begin
      x = i;
      $display(x);
    end
    $write("*-* All Finished *-*\n");
    $finish;
  end
endmodule
