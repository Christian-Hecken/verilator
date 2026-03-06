// DESCRIPTION: Verilator: Microbenchmark for dead variable elimination
// Exercises V3Dead: signals that are assigned but never consumed.
// Without --public-flat-rw these are pruned together with their driving logic.
// With    --public-flat-rw they are preserved, preventing dead-code elimination.

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

    // Dead signal array: each element is driven from 'in' but never read.
    // V3Dead should eliminate all of these (and their driving always blocks)
    // unless the signals are made public.
    generate
        for (genvar i = 0; i < DEPTH; i++) begin : DEAD
            logic dead_tmp;
            assign dead_tmp = in ^ i[0];  // non-trivial expr so gate opt doesn't fold it away
        end
    endgenerate

    // Trivial live output so the module has observable behaviour
    assign out = in;

    integer i;
    initial begin
        for (i = 0; i < `SIM_CYCLES; i++) begin
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
