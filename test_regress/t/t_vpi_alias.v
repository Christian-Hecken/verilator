// DESCRIPTION: Verilator: Verilog Test module
//
// This file ONLY is placed under the Creative Commons Public Domain.
// SPDX-FileCopyrightText: 2026 Wilson Snyder
// SPDX-License-Identifier: CC0-1.0

module Inner(input logic a, output logic y);
  logic tmp /*verilator public_flat_rw*/;
  assign tmp = a;
  assign y = tmp;
endmodule

module top(
  input logic a,
  output logic y
);
  Inner in(.a(a), .y(y));
endmodule
