// DESCRIPTION: Verilator: Test loop unroll const bindings with public_flat_rd (not blocked)
// SPDX-License-Identifier: CC0-1.0

module t;
  int x /* verilator public_flat_rd */;
  
  initial begin
    for (int i = 0; i < 3; i++) begin
      x = i;
      $display(x);
    end
    $write("*-* All Finished *-*\n");
    $finish;
  end
endmodule
