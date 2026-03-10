module top;
    reg r /*verilator public_flat_rw*/ /*verilator forceable*/ = 0;

    import "DPI-C" context function int mon_check();

    integer status;
    initial begin
        status = mon_check();
        if (status != 0) begin
            $stop;
        end
        $finish;
    end
endmodule
