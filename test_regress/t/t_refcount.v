module t;
m m0();
m m1();
arr #(.PARAM(0)) arr0();
arr #(.PARAM(0)) arr1();
arr #(.PARAM(1)) arr2();
endmodule

module m;
bit s0;
bit s1;
bit [3:0] s2;
bit [7:0] s3;

assign s1 = s0;
assign s2 = {4{s0 ^ s1}};
assign s3 = {8{s1 & s2[0]}};
endmodule

module arr;
  parameter PARAM = 1;

  wire sig;
  assign sig = 0;
endmodule : arr
