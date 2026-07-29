// DESCRIPTION: Verilator: Test variable localization independent rejection (string type + public)
// SPDX-License-Identifier: CC0-1.0

module t (/*AUTOARG*/);
  // String variables are explicitly excluded in isOptimizable()
  // Combined with public, this tests independent rejection
  // Use module-level variable to avoid compiler-generated temps
  string x /* verilator public_flat_rw */;
  
  initial begin
    x = "hello";
    $display("%s", x);
    $write("*-* All Finished *-*\n");
    $finish;
  end
endmodule
