module t;

// True aliases
wire alias0;
wire driver0;
assign alias0 = driver0;
bit alias1;
bit driver1;
always_comb begin
  alias1 = driver1;
end

wire chainDriver;
wire chainAlias0;
wire chainAlias1;
wire chainAlias2;
assign chainAlias0 = chainDriver;
assign chainAlias1 = chainAlias0;
assign chainAlias2 = chainAlias1;

// wire cycle0;
// wire cycle1;
// wire cycle2;
// assign cycle0 = cycle1;
// assign cycle1 = cycle2;
// assign cycle2 = cycle0;

// Not aliases
//wire assignmentMultiDriven;
wire multiDriver0;
wire multiDriver1;
//assign assignmentMultiDriven = multiDriver0;
// TODO: This gets eliminated by V3Tristate
//assign assignmentMultiDriven = multiDriver1;

/* verilator lint_off MULTIDRIVEN */
bit alwaysMultiDriven;
always_comb begin
  alwaysMultiDriven = multiDriver0;
end
always_comb begin
  alwaysMultiDriven = multiDriver1;
end
/* verilator lint_on MULTIDRIVEN */

logic clockedDriven;
wire clk;
always @(posedge clk) clockedDriven = driver0;

logic delayDriven;
always_comb begin
  delayDriven = #1 driver0;
end

logic onceDriven;
initial begin
  onceDriven = driver0;
end

wire arrayDriver[31:0];
wire partiallyDriven[15:0];
assign partiallyDriven[15:0] = arrayDriver[15:0];

endmodule