`ifdef ENABLE_EVAL
`define EVAL #0;
`else
`define EVAL
`endif

module top;
    reg [7:0] sig /*verilator public_flat_rw*/ /*verilator forceable*/ = 8'h00;
    wire [7:0] not_sig = ~sig;
    int vpiStatus = 1;

`ifdef USE_VPI_NOT_DPI
`ifdef VERILATOR
`systemc_header
    extern "C" int mon_check();
`verilog
`endif
`else
        import "DPI-C" context function int mon_check();
`endif

    // Wrap into non-inlined task as per connecting.rst
    task automatic vpiMonCheck(input [7:0] sig_arg, output int status);
        // verilator no_inline_task
`ifdef VERILATOR
`ifdef USE_VPI_NOT_DPI
        status = $c32("mon_check()");
`else
        status = mon_check();
`endif
`elsif IVERILOG
        status = $mon_check();
`elsif USE_VPI_NOT_DPI
        status = $mon_check();
`else
        status = mon_check();
`endif
    endtask

    initial begin
        `EVAL
        if (not_sig != 8'hFF) $stop;

        // Direct SystemVerilog assignment
        sig = 8'h00;
        sig = 8'hFF;
        `EVAL
        if (sig != 8'hFF) $stop;
        if (not_sig != 8'h00) $stop;


        // SystemVerilog force/release
        sig = 8'h00;
        force sig = 8'hAA;
        `EVAL
        if (sig != 8'hAA) $stop;
        if (not_sig != 8'h55) $stop;
        release sig;
        `EVAL
        if (sig != 8'hAA) $stop;
        if (not_sig != 8'h55) $stop;


        // Set value through VPI
        sig = 8'h00;
        vpiMonCheck(sig, vpiStatus);
        if (vpiStatus != 0) $stop;
        `EVAL
        if (sig != 8'hCC) $stop;
        if (not_sig != 8'h33) $stop;

        $write("*-* All Finished *-*\\n");
        $finish;
    end
endmodule
