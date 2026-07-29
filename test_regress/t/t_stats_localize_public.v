// DESCRIPTION: Verilator: Test statistics for variable localization blocked by public
// SPDX-License-Identifier: CC0-1.0

module t;
  int x /* verilator public_flat_rw */;
  initial begin
    x = $c32(1);
    $display(x);
    x = $c32(2);
    $display(x);
    $write("*-* All Finished *-*\n");
    $finish;
  end
endmodule
