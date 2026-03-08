module Inner(input logic a, input logic b, output logic y);
  logic tmp /*verilator public_flat_rw*/;
  always_comb tmp = a & b;
  assign y = tmp;
endmodule

module top;
  logic a;
  logic b;
  logic y;
  Inner in(.a(a), .b(b), .y(y));
endmodule
