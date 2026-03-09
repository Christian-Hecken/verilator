// DESCRIPTION: Verilator: Ghost variable optimization test
//
// Test that intermediate public_flat_rd signals are ghost-eligible
// and that gate reduction inlines them into consumers.
//
// This file ONLY is placed under the Creative Commons Public Domain.
// SPDX-FileCopyrightText: 2026 Verilator Contributors
// SPDX-License-Identifier: CC0-1.0

module t (
    input clk
);

    logic [7:0] a;
    logic [7:0] b;
    // This combinational intermediate signal should be ghost-eligible:
    // it has a single combinational driver and is used internally.
    logic [7:0] c /* verilator public_flat_rd */ ;
    logic [7:0] z;

    // Single combinational driver for ghost candidate 'c'
    assign c = ~a;
    // Consumer of 'c' — V3Gate should inline ~a here
    assign z = c & b;

    int count = 0;

    always_ff @(posedge clk) begin
        count <= count + 1;
        a <= count[7:0];
        b <= ~count[7:0];

        // Verify correctness: z should equal (~a) & b
        if (count > 1) begin
            if (z !== (~a & b)) begin
                $display("%%Error: z=%0h, expected %0h (a=%0h b=%0h c=%0h)",
                         z, (~a & b), a, b, c);
                $stop;
            end
            if (c !== ~a) begin
                $display("%%Error: c=%0h, expected %0h (a=%0h)", c, ~a, a);
                $stop;
            end
        end

        if (count == 20) begin
            $write("*-* All Finished *-*\n");
            $finish;
        end
    end
endmodule
