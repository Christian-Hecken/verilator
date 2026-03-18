module t;

  reg [ 1:0] ascUnpackedC [0:1][0:1];
  reg [ 3:0] ascUnpackedS [0:1][0:1];
  reg [ 7:0] ascUnpackedI [0:1][0:1];
  reg [15:0] ascUnpackedQ [0:1][0:1];
  reg [31:0] ascUnpackedW [0:1][0:1];

  reg [ 1:0] descUnpackedC[1:0][1:0];
  reg [ 3:0] descUnpackedS[1:0][1:0];
  reg [ 7:0] descUnpackedI[1:0][1:0];
  reg [15:0] descUnpackedQ[1:0][1:0];
  reg [31:0] descUnpackedW[1:0][1:0];

  initial begin
    ascUnpackedC  = '{'{2'h0, 2'h1}, '{2'h2, 2'h3}};
    ascUnpackedS  = '{'{4'hA, 4'hB}, '{4'hC, 4'hD}};
    ascUnpackedI  = '{'{8'hAA, 8'hBB}, '{8'hCC, 8'hDD}};
    ascUnpackedQ  = '{'{16'hAAAA, 16'hBBBB}, '{16'hCCCC, 16'hDDDD}};
    ascUnpackedW  = '{'{32'hAAAAAAAA, 32'hBBBBBBBB}, '{32'hCCCCCCCC, 32'hDDDDDDDD}};
    descUnpackedC = '{'{2'h0, 2'h1}, '{2'h2, 2'h3}};
    descUnpackedS = '{'{4'hA, 4'hB}, '{4'hC, 4'hD}};
    descUnpackedI = '{'{8'hAA, 8'hBB}, '{8'hCC, 8'hDD}};
    descUnpackedQ = '{'{16'hAAAA, 16'hBBBB}, '{16'hCCCC, 16'hDDDD}};
    descUnpackedW = '{'{32'hAAAAAAAA, 32'hBBBBBBBB}, '{32'hCCCCCCCC, 32'hDDDDDDDD}};
    #1;

    $display("ascUnpackedC : %h", ascUnpackedC);
    $display("ascUnpackedS : %h", ascUnpackedS);
    $display("ascUnpackedI : %h", ascUnpackedI);
    $display("ascUnpackedQ : %h", ascUnpackedQ);
    $display("ascUnpackedW : %h", ascUnpackedW);
    // Simulator crashes upon these print statements:
    $display("descUnpackedC : %h", descUnpackedC);
    $display("descUnpackedS : %h", descUnpackedS);
    $display("descUnpackedI : %h", descUnpackedI);
    $display("descUnpackedQ : %h", descUnpackedQ);
    $display("descUnpackedW : %h", descUnpackedW);

    $display("*-* All Finished *-*");
    $finish;
  end

endmodule
