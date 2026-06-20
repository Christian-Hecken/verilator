module t;
  bit clk_i = 1'b0;
  bit [31:0] din;
  wire [31:0] dout;
  dff dff_inst (
        .clk_i (clk_i)
      , .data_i(din)
      , .data_o(dout)
  );
  reg [31:0] foo;
  always @(negedge clk_i) begin
    foo <= dout;
  end

  initial begin
    $display("foo: %x", foo);
    din = 32'hdeadbeef;
    #5 clk_i = 1'b1;
    #5 clk_i = 1'b0;
    #5 $display("foo: %x", foo);
    #10 $finish;
  end
endmodule

module dff #(
    parameter width_p = 32
) (
    input clk_i
    , input [width_p-1:0] data_i
    , output [width_p-1:0] data_o
);

  reg [width_p-1:0] data_r;

  assign data_o = data_r;

  always @(posedge clk_i) data_r <= data_i;

endmodule
