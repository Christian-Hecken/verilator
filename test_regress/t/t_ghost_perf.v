// t_ghost_perf.v - microbenchmark for ghost variable optimization
//
// Generates a DEPTH-long chain of combinational intermediates.
// Each intermediate has a single combinational driver (ghost-eligible).
// When compiled with +define+GHOST_PUBLIC, intermediates are public_flat_rd.

`ifndef DEPTH
`define DEPTH 256
`endif

`ifndef SIM_CYCLES
`define SIM_CYCLES 8
`endif

module t;
    parameter int DEPTH = `DEPTH;

    logic [31:0] in_a;
    logic [31:0] in_b;

`ifdef GHOST_PUBLIC
  `define PUB /* verilator public_flat_rd */
`else
  `define PUB
`endif

    // Generate a chain of combinational intermediates.
    // Each signal has a single combinational driver and feeds the next stage.
    generate
        for (genvar i = 0; i < DEPTH; i = i + 1) begin : STAGE
            logic [31:0] val `PUB;
            if (i == 0) begin
                assign STAGE[i].val = in_a ^ in_b;
            end else if (i[0] == 0) begin
                assign STAGE[i].val = STAGE[i-1].val + in_a;
            end else begin
                assign STAGE[i].val = STAGE[i-1].val ^ in_b;
            end
        end
    endgenerate

    logic [31:0] result;
    assign result = STAGE[DEPTH-1].val;

    integer i;
    initial begin
        for (i = 0; i < `SIM_CYCLES; i = i + 1) begin
            in_a = i[31:0] ^ 32'hdeadbeef;
            in_b = ~i[31:0] ^ 32'hcafebabe;
            #1;
        end
        $display("*-* All Finished *-* result=%08x", result);
        $finish;
    end
endmodule
