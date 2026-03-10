module top;
    reg r /*verilator public_flat_rw*/ /*verilator forceable*/;
    integer status;
    initial r = 0;

    import "DPI-C" context function int sv_check();

    initial begin
        // SV force/release instead of VPI
        force r = 1;
        release r;
        #1;
        status = sv_check();
        if (status != 0) begin
            $write("%%Error: sv-force sequence produced wrong value\n");
            $stop;
        end
        $finish;
    end
endmodule
