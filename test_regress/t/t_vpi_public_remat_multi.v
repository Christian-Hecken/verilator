// DESCRIPTION: Verilator: Verilog Test module
//
// This file ONLY is placed under the Creative Commons Public Domain, for
// any use, without warranty, 2024 by Wilson Snyder.
// SPDX-License-Identifier: CC0-1.0

module testbench (
   input clk
   );

   // These signals will have computed getters and trigger the scope name mismatch
   wire [1:0] dma_data_ready_and_lo /*verilator public_flat_rw*/ = {clk, clk};
   wire [1:0] dma_data_v_li /*verilator public_flat_rw*/ = {clk, clk};
   wire [1:0] dma_data_v_lo /*verilator public_flat_rw*/ = {clk, clk};
   wire [1:0] dma_data_yumi_li /*verilator public_flat_rw*/ = {clk, clk};

endmodule
