// DESCRIPTION: Verilator: minimal reproduction for DPI/VPI immediate-eval issue
// SPDX-FileCopyrightText: 2026 Example
// SPDX-License-Identifier: CC0-1.0

module top;
    // public + forceable so both base/force paths are exercised
    reg [7:0] sig /*verilator public_flat_rw*/ /*verilator forceable*/ = 8'h00;
    wire [7:0] not_sig = ~sig;

`ifdef USE_VPI_NOT_DPI
`ifdef VERILATOR
`systemc_header
    extern "C" int mon_check();
`verilog
`endif
`else
        import "DPI-C" context function int mon_check();
`endif

    initial begin
        $write("%%Info: Starting t_dpi_force_repro\n");
        if (not_sig != 8'hFF) $stop;

        // Case 1: direct SystemVerilog assignment (8-bit values)
        $write("%%Info: Case 1: SV assignment\n");
        sig = 8'h00;
        sig = 8'hFF;
        if (sig != 8'hFF) $stop;
        if (not_sig != 8'h00) $stop;


        // Case 2: SystemVerilog force/release sequence (no #1)
        $write("%%Info: Case 2: force/release\n");
        sig = 8'h00;
        force sig = 8'hAA;
        // immediate check while forced
        if (sig != 8'hAA) $stop;
        if (not_sig != 8'h55) $stop;
        release sig;
        // check after release as well
        if (sig != 8'hAA) $stop;
        if (not_sig != 8'h55) $stop;


        // Case 3: set through DPI/VPI (no #1)
        $write("%%Info: Case 3: DPI/VPI write\n");
        sig = 8'h00;
    `ifdef VERILATOR
    `ifdef USE_VPI_NOT_DPI
        if ($c32("mon_check()") != 0) $stop;
    `else
        if (mon_check() != 0) $stop;
    `endif
    `elsif IVERILOG
        if ($mon_check != 0) $stop;
    `elsif USE_VPI_NOT_DPI
        if ($mon_check != 0) $stop;
    `else
        if (mon_check() != 0) $stop;
    `endif
        if (sig != 8'hCC) $stop;
        if (not_sig != 8'h33) $stop;

        $display("*-* All Finished *-*");
        $finish;
    end
endmodule
