// DESCRIPTION: Verilator: Verilog Test module
//
// This file ONLY is placed under the Creative Commons Public Domain.
// SPDX-FileCopyrightText: 2026 Wilson Snyder
// SPDX-License-Identifier: CC0-1.0

// Unified testbench for gate optimization features:
// - Aliasing optimization (assignment chains should be optimized)
// - Dead code elimination (unused signals should be removed)
// - VPI alias access (public signals should be accessible via VPI)

module Inner(input logic a, output logic y);
  logic tmp /*verilator public_flat_rw*/;
  assign tmp = a;
  assign y = tmp;
endmodule

module t(
  input logic in,
  output logic out_alias,
  output logic out_dead,
  output logic out_vpi
);
  // Test 1: Aliasing optimization - chain of assignments
  // These should be optimized to direct connections
  logic chain0, chain1, chain2, chain3, chain4;
  assign chain0 = in;
  assign chain1 = chain0;
  assign chain2 = chain1;
  assign chain3 = chain2;
  assign chain4 = chain3;
  assign out_alias = chain4;

  // Test 2: Dead code elimination - signals driven but never read
  // These should be eliminated by V3Dead
  logic dead0, dead1, dead2, dead3;
  assign dead0 = in ^ 1'b0;
  assign dead1 = in ^ 1'b1;
  assign dead2 = in & 1'b1;
  assign dead3 = in | 1'b0;
  assign out_dead = in;

  // Test 3: VPI alias - hierarchical module with public signal
  Inner inner_inst(.a(in), .y(out_vpi));

  integer cyc;
  initial begin
    for (cyc = 0; cyc < 8; cyc = cyc + 1) begin
      #1;
      if (out_alias !== in) begin
        $display("%%Error: Aliasing test failed at cycle %0d: in=%b out_alias=%b", cyc, in, out_alias);
        $stop;
      end
      if (out_dead !== in) begin
        $display("%%Error: Dead elim test failed at cycle %0d: in=%b out_dead=%b", cyc, in, out_dead);
        $stop;
      end
      if (out_vpi !== in) begin
        $display("%%Error: VPI alias test failed at cycle %0d: in=%b out_vpi=%b", cyc, in, out_vpi);
        $stop;
      end
    end
    $write("*-* All Finished *-*\n");
    $finish;
  end
endmodule