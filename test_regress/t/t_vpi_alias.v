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
