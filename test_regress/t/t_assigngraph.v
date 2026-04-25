module t;
bit a /*verilator public_flat_rw*/;
bit b /*verilator public_flat_rw*/;
bit c /*verilator public_flat_rw*/;

initial begin
a = b;
b = c;
end
endmodule