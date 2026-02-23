// DESCRIPTION: Verilator: Test unpacked array slice with multi-dimensional indexing
//
// This file ONLY is placed under the Creative Commons Public Domain.
// SPDX-FileCopyrightText: 2025 Wilson Snyder
// SPDX-License-Identifier: CC0-1.0

// Test for unpacked array slice syntax per IEEE 1800-2017 Section 7.4.6
// Given: bit my_mem[0:3][0:7]
// Then:  my_mem[0:1][3] should select elements {my_mem[0][3], my_mem[1][3]}
//        i.e., the slice [0:1] applies to the first unpacked dimension,
//        and [3] indexes into the second unpacked dimension.

// verilator lint_off ASCRANGE

module t;
  bit my_mem[0:3][0:7];

  initial begin
    // Initialize known values
    my_mem[0][3] = 1'b0;
    my_mem[1][3] = 1'b0;
    my_mem[2][5] = 1'b0;
    my_mem[3][5] = 1'b0;

    // Test 1: Read via slice - my_mem[0:1][3] should be {my_mem[0][3], my_mem[1][3]}
    $display("= Test 1: Assign through 2D slice");
    my_mem[0:1][3] = '{1'b1, 1'b0};
    if (my_mem[0][3] !== 1'b1) begin
      $display("%%Error: my_mem[0][3] = %0b, expected 1", my_mem[0][3]);
      $stop;
    end
    if (my_mem[1][3] !== 1'b0) begin
      $display("%%Error: my_mem[1][3] = %0b, expected 0", my_mem[1][3]);
      $stop;
    end

    // Test 2: Read via slice
    $display("= Test 2: Read through 2D slice");
    my_mem[0][3] = 1'b1;
    my_mem[1][3] = 1'b1;
    begin
      bit slice_result[0:1];
      slice_result = my_mem[0:1][3];
      if (slice_result[0] !== 1'b1) begin
        $display("%%Error: slice_result[0] = %0b, expected 1", slice_result[0]);
        $stop;
      end
      if (slice_result[1] !== 1'b1) begin
        $display("%%Error: slice_result[1] = %0b, expected 1", slice_result[1]);
        $stop;
      end
    end

    // Test 3: Different slice range
    $display("= Test 3: Assign through different slice range");
    my_mem[2:3][5] = '{1'b1, 1'b0};
    if (my_mem[2][5] !== 1'b1) begin
      $display("%%Error: my_mem[2][5] = %0b, expected 1", my_mem[2][5]);
      $stop;
    end
    if (my_mem[3][5] !== 1'b0) begin
      $display("%%Error: my_mem[3][5] = %0b, expected 0", my_mem[3][5]);
      $stop;
    end

    // Test 4: With wider data types
    $display("= Test 4: Wider data types");
    begin
      logic [31:0] wide_mem[0:3][0:1];
      wide_mem[0][0] = 32'hDEAD_BEEF;
      wide_mem[1][0] = 32'hCAFE_BABE;
      begin
        logic [31:0] wide_slice[0:1];
        wide_slice = wide_mem[0:1][0];
        if (wide_slice[0] !== 32'hDEAD_BEEF) begin
          $display("%%Error: wide_slice[0] = %0h, expected DEAD_BEEF", wide_slice[0]);
          $stop;
        end
        if (wide_slice[1] !== 32'hCAFE_BABE) begin
          $display("%%Error: wide_slice[1] = %0h, expected CAFE_BABE", wide_slice[1]);
          $stop;
        end
      end
    end

    $write("*-* All Finished *-*\n");
    $finish;
  end

endmodule
