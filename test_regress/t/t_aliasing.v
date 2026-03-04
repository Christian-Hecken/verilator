// t_aliasing.v - microbenchmark to exercise internal temporaries

`ifndef DEPTH
`define DEPTH 4096
`endif

`ifndef SIM_CYCLES
`define SIM_CYCLES 8
`endif

module t;
  parameter int DEPTH = `DEPTH;

  logic in;
  logic out;

  generate
    for (genvar i = 0; i < DEPTH; i = i + 1) begin : CHAIN
      logic tmp;
      if (i == 0) begin
        assign CHAIN[i].tmp = in;
      end else begin
        assign CHAIN[i].tmp = CHAIN[i-1].tmp;
      end
    end
  endgenerate

  assign out = CHAIN[DEPTH-1].tmp;

  integer i;
  initial begin
    for (i = 0; i < `SIM_CYCLES; i = i + 1) begin
      in = i[0];
      #1;
      if (out !== in) begin
        $display("Mismatch at cycle %0d: in=%b out=%b", i, in, out);
        $finish;
      end
    end
    $display("*-* All Finished *-*");
    $finish;
  end
endmodule
