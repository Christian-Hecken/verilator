// DESCRIPTION: Verilator: Verilog Test module
//
// This file ONLY is placed under the Creative Commons Public Domain.
// SPDX-FileCopyrightText: 2026 Wilson Snyder
// SPDX-License-Identifier: CC0-1.0

module t (
  input clk
);
  int cyc;

  logic [7:0] source;
  logic [7:0] aliased_sig /*verilator public_flat_rw*/;
  
  assign aliased_sig = source;

  always @(posedge clk) begin
    cyc <= cyc + 1;
    source <= 8'(cyc * 3 + 17);
    
    if (cyc == 10) begin
      $write("*-* All Finished *-*\n");
      $finish;
    end
  end
endmodule
