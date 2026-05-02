// DESCRIPTION: Verilator: Verilog Test module
//
// This file ONLY is placed under the Creative Commons Public Domain.
// SPDX-FileCopyrightText: 2020 Yutetsu TAKATSUKASA
// SPDX-License-Identifier: Unlicense

module t;

  wire [7:0] out;

  sub0 i_sub0 (
      .in (8),
      .out(out)
  );

  initial begin
    #0
    if (out != 8) begin
      $write("Mismatch\n");
      $stop;
    end else begin
      $finish;
    end
  end

endmodule

module sub0 (
    input  wire [7:0] in,
    output wire [7:0] out
);  /*verilator hier_block*/
  assign out = in;
endmodule
