module t (
    input clk_i
    , output logic s_external_irq_o
);

  logic [31:0] addr_lo;
  logic [31:0] data_lo;

  localparam plic_els_lp = 2;
  localparam lg_plic_els_lp = 3;
  logic [plic_els_lp-1:0] plic_n, plic_r;
  wire [lg_plic_els_lp-1:0] plic_addr_li = addr_lo[3+:lg_plic_els_lp];

  always_comb begin
    plic_n = plic_r;
    plic_n[plic_addr_li[0]] = data_lo[0];
  end

  bsg_dff_reset_en #(
      .width_p(plic_els_lp)
  ) plic_reg (
        .clk_i (clk_i)
      , .data_i(plic_n)
      , .data_o(plic_r)
  );
  assign s_external_irq_o = plic_r[1];

endmodule





module bsg_dff_reset_en #(
    parameter width_p = 32
) (
    input clk_i
    , input [width_p-1:0] data_i
    , output logic [width_p-1:0] data_o
);

  logic [width_p-1:0] data_r;

  assign data_o = data_r;

  always_ff @(posedge clk_i) begin
    data_r <= data_i;
  end

endmodule
