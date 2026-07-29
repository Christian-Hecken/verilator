// DESCRIPTION: Verilator: Test loop unroll const binding independent rejection (forced + public)
// SPDX-License-Identifier: CC0-1.0

module t;
  int x /* verilator public_flat_rw */;
  
  initial begin
    for (int i = 0; i < 3; i++) begin
      x = i;
      force x = i + 10;  // Force makes variable non-bindable, takes precedence over public
      $display(x);
      release x;
    end
    $write("*-* All Finished *-*\n");
    $finish;
  end
endmodule