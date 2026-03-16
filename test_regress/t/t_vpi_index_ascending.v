module t;

  `ifdef VERILATOR
  `systemc_header
    extern "C" int printValues();
    extern "C" int printSingleElements();
  `verilog
  `endif

  // verilator lint_off ASCRANGE
  reg [-2:-1][0:1][1:0] ascPackedC  /*verilator public_flat_rw*/;  // CData
  reg [-2:-1][0:1][3:0] ascPackedS  /*verilator public_flat_rw*/;  // SData
  reg [-2:-1][0:1][7:0] ascPackedI  /*verilator public_flat_rw*/;  // IData
  reg [-2:-1][0:1][15:0] ascPackedQ  /*verilator public_flat_rw*/;  // QData
  reg [-2:-1][0:1][31:0] ascPackedW  /*verilator public_flat_rw*/;  // VlWide
  // verilator lint_on ASCRANGE

  reg [1:0] ascUnpackedC[-2:-1][0:1]  /*verilator public_flat_rw*/;  // CData
  reg [3:0] ascUnpackedS[-2:-1][0:1]  /*verilator public_flat_rw*/;  // SData
  reg [7:0] ascUnpackedI[-2:-1][0:1]  /*verilator public_flat_rw*/;  // IData
  reg [15:0] ascUnpackedQ[-2:-1][0:1]  /*verilator public_flat_rw*/;  // QData
  reg [31:0] ascUnpackedW[-2:-1][0:1]  /*verilator public_flat_rw*/;  // VlWide

  reg [-1:-2][1:0][1:0] descPackedC  /*verilator public_flat_rw*/;  // CData
  reg [-1:-2][1:0][3:0] descPackedS  /*verilator public_flat_rw*/;  // SData
  reg [-1:-2][1:0][7:0] descPackedI  /*verilator public_flat_rw*/;  // IData
  reg [-1:-2][1:0][15:0] descPackedQ  /*verilator public_flat_rw*/;  // QData
  reg [-1:-2][1:0][31:0] descPackedW  /*verilator public_flat_rw*/;  // VlWide

  reg [1:0] descUnpackedC[-1:-2][1:0]  /*verilator public_flat_rw*/;  // CData
  reg [3:0] descUnpackedS[-1:-2][1:0]  /*verilator public_flat_rw*/;  // SData
  reg [7:0] descUnpackedI[-1:-2][1:0]  /*verilator public_flat_rw*/;  // IData
  reg [15:0] descUnpackedQ[-1:-2][1:0]  /*verilator public_flat_rw*/;  // QData
  reg [31:0] descUnpackedW[-1:-2][1:0]  /*verilator public_flat_rw*/;  // VlWide

  task automatic vpiPrintValues();
    integer vpiStatus = 1;
`ifdef VERILATOR
    vpiStatus = $c32("printValues()");
`else
    vpiStatus = $printValues;
`endif
    if (vpiStatus != 0) begin
      $write("%%Error: t_vpi_index_ascending.cpp:%0d:", vpiStatus);
      $stop;
    end
  endtask

  task automatic svPrintAllValues();
    $display("[SV] ascPackedC : %h", ascPackedC);
    $display("[SV] ascPackedS : %h", ascPackedS);
    $display("[SV] ascPackedI : %h", ascPackedI);
    $display("[SV] ascPackedQ : %h", ascPackedQ);
    $display("[SV] ascPackedW : %h", ascPackedW);
    $display("[SV] descPackedC : %h", descPackedC);
    $display("[SV] descPackedS : %h", descPackedS);
    $display("[SV] descPackedI : %h", descPackedI);
    $display("[SV] descPackedQ : %h", descPackedQ);
    $display("[SV] descPackedW : %h", descPackedW);
    $display("[SV] ascUnpackedC : %h", ascUnpackedC);
    $display("[SV] ascUnpackedS : %h", ascUnpackedS);
    $display("[SV] ascUnpackedI : %h", ascUnpackedI);
    $display("[SV] ascUnpackedQ : %h", ascUnpackedQ);
    $display("[SV] ascUnpackedW : %h", ascUnpackedW);
    $display("[SV] descUnpackedC : %h", descUnpackedC);
    $display("[SV] descUnpackedS : %h", descUnpackedS);
    $display("[SV] descUnpackedI : %h", descUnpackedI);
    $display("[SV] descUnpackedQ : %h", descUnpackedQ);
    $display("[SV] descUnpackedW : %h", descUnpackedW);
  endtask

  task automatic svPrintOneDim(integer i);
    $display("[SV] ascPackedC[%0d] : %h", i, ascPackedC[i]);
    $display("[SV] ascPackedS[%0d] : %h", i, ascPackedS[i]);
    $display("[SV] ascPackedI[%0d] : %h", i, ascPackedI[i]);
    $display("[SV] ascPackedQ[%0d] : %h", i, ascPackedQ[i]);
    $display("[SV] ascPackedW[%0d] : %h", i, ascPackedW[i]);
    $display("[SV] descPackedC[%0d] : %h", i, descPackedC[i]);
    $display("[SV] descPackedS[%0d] : %h", i, descPackedS[i]);
    $display("[SV] descPackedI[%0d] : %h", i, descPackedI[i]);
    $display("[SV] descPackedQ[%0d] : %h", i, descPackedQ[i]);
    $display("[SV] descPackedW[%0d] : %h", i, descPackedW[i]);
    $display("[SV] ascUnpackedC[%0d] : %h", i, ascUnpackedC[i]);
    $display("[SV] ascUnpackedS[%0d] : %h", i, ascUnpackedS[i]);
    $display("[SV] ascUnpackedI[%0d] : %h", i, ascUnpackedI[i]);
    $display("[SV] ascUnpackedQ[%0d] : %h", i, ascUnpackedQ[i]);
    $display("[SV] ascUnpackedW[%0d] : %h", i, ascUnpackedW[i]);
    $display("[SV] descUnpackedC[%0d] : %h", i, descUnpackedC[i]);
    $display("[SV] descUnpackedS[%0d] : %h", i, descUnpackedS[i]);
    $display("[SV] descUnpackedI[%0d] : %h", i, descUnpackedI[i]);
    $display("[SV] descUnpackedQ[%0d] : %h", i, descUnpackedQ[i]);
    $display("[SV] descUnpackedW[%0d] : %h", i, descUnpackedW[i]);
  endtask

  task automatic svPrintSingleElements(integer i, integer j);
    $display("[SV] ascPackedC[%0d][%0d] : %h", i, j, ascPackedC[i][j]);
    $display("[SV] ascPackedS[%0d][%0d] : %h", i, j, ascPackedS[i][j]);
    $display("[SV] ascPackedI[%0d][%0d] : %h", i, j, ascPackedI[i][j]);
    $display("[SV] ascPackedQ[%0d][%0d] : %h", i, j, ascPackedQ[i][j]);
    $display("[SV] ascPackedW[%0d][%0d] : %h", i, j, ascPackedW[i][j]);
    $display("[SV] descPackedC[%0d][%0d] : %h", i, j, descPackedC[i][j]);
    $display("[SV] descPackedS[%0d][%0d] : %h", i, j, descPackedS[i][j]);
    $display("[SV] descPackedI[%0d][%0d] : %h", i, j, descPackedI[i][j]);
    $display("[SV] descPackedQ[%0d][%0d] : %h", i, j, descPackedQ[i][j]);
    $display("[SV] descPackedW[%0d][%0d] : %h", i, j, descPackedW[i][j]);
    $display("[SV] ascUnpackedC[%0d][%0d] : %h", i, j, ascUnpackedC[i][j]);
    $display("[SV] ascUnpackedS[%0d][%0d] : %h", i, j, ascUnpackedS[i][j]);
    $display("[SV] ascUnpackedI[%0d][%0d] : %h", i, j, ascUnpackedI[i][j]);
    $display("[SV] ascUnpackedQ[%0d][%0d] : %h", i, j, ascUnpackedQ[i][j]);
    $display("[SV] ascUnpackedW[%0d][%0d] : %h", i, j, ascUnpackedW[i][j]);
    $display("[SV] descUnpackedC[%0d][%0d] : %h", i, j, descUnpackedC[i][j]);
    $display("[SV] descUnpackedS[%0d][%0d] : %h", i, j, descUnpackedS[i][j]);
    $display("[SV] descUnpackedI[%0d][%0d] : %h", i, j, descUnpackedI[i][j]);
    $display("[SV] descUnpackedQ[%0d][%0d] : %h", i, j, descUnpackedQ[i][j]);
    $display("[SV] descUnpackedW[%0d][%0d] : %h", i, j, descUnpackedW[i][j]);
  endtask

  task automatic vpiPrintSingleElements();
    integer vpiStatus = 1;
`ifdef VERILATOR
    vpiStatus = $c32("printSingleElements()");
`else
    vpiStatus = $printSingleElements;
`endif
    if (vpiStatus != 0) begin
      $write("%%Error: t_vpi_index_ascending.cpp:%0d:", vpiStatus);
      $stop;
    end
  endtask

  initial begin
    ascPackedC  = '{'{2'h0, 2'h1}, '{2'h2, 2'h3}};
    ascPackedS  = '{'{4'hA, 4'hB}, '{4'hC, 4'hD}};
    ascPackedI  = '{'{8'hAA, 8'hBB}, '{8'hCC, 8'hDD}};
    ascPackedQ  = '{'{16'hAAAA, 16'hBBBB}, '{16'hCCCC, 16'hDDDD}};
    ascPackedW  = '{'{32'hAAAAAAAA, 32'hBBBBBBBB}, '{32'hCCCCCCCC, 32'hDDDDDDDD}};
    descPackedC = '{'{2'h0, 2'h1}, '{2'h2, 2'h3}};
    descPackedS = '{'{4'hA, 4'hB}, '{4'hC, 4'hD}};
    descPackedI = '{'{8'hAA, 8'hBB}, '{8'hCC, 8'hDD}};
    descPackedQ = '{'{16'hAAAA, 16'hBBBB}, '{16'hCCCC, 16'hDDDD}};
    descPackedW = '{'{32'hAAAAAAAA, 32'hBBBBBBBB}, '{32'hCCCCCCCC, 32'hDDDDDDDD}};
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

    $display("Entire content:");
    svPrintAllValues();

    $display("\nSecond element from the left:");
    svPrintOneDim(-1);
    vpiPrintValues();

    $display("\n***Filling with ascending values***\n");
    for (int i = 0; i < 2; i++) begin
      for (int j = 0; j < 2; j++) begin
        ascPackedC[i-2][j]  = 2'(((i * 2) + j));
        ascPackedS[i-2][j]  = 4'(((i * 2) + j));
        ascPackedI[i-2][j]  = 8'(((i * 2) + j));
        ascPackedQ[i-2][j]  = 16'(((i * 2) + j));
        ascPackedW[i-2][j]  = 32'(((i * 2) + j));
        descPackedC[i-2][j] = 2'(((i * 2) + j));
        descPackedS[i-2][j] = 4'(((i * 2) + j));
        descPackedI[i-2][j] = 8'(((i * 2) + j));
        descPackedQ[i-2][j] = 16'(((i * 2) + j));
        descPackedW[i-2][j] = 32'(((i * 2) + j));
        ascUnpackedC[i-2][j]  = 2'(((i * 2) + j));
        ascUnpackedS[i-2][j]  = 4'(((i * 2) + j));
        ascUnpackedI[i-2][j]  = 8'(((i * 2) + j));
        ascUnpackedQ[i-2][j]  = 16'(((i * 2) + j));
        ascUnpackedW[i-2][j]  = 32'(((i * 2) + j));
        descUnpackedC[i-2][j] = 2'(((i * 2) + j));
        descUnpackedS[i-2][j] = 4'(((i * 2) + j));
        descUnpackedI[i-2][j] = 8'(((i * 2) + j));
        descUnpackedQ[i-2][j] = 16'(((i * 2) + j));
        descUnpackedW[i-2][j] = 32'(((i * 2) + j));
      end
    end

    #1;
    $display("Entire content:");
    svPrintAllValues();

    $display("\nSecond element from the left:");
    svPrintOneDim(-1);
    vpiPrintValues();

    $display("\nSingle elements:");
    svPrintSingleElements(-2, 1);
    vpiPrintSingleElements();

    $display("*-* All Finished *-*");
    $finish;
  end

endmodule
