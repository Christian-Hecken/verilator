// DESCRIPTION: Verilator: Verilog Test implementation
//
// This program is free software; you can redistribute it and/or modify it
// under the terms of either the GNU Lesser General Public License Version 3
// or the Perl Artistic License Version 2.0.
// SPDX-FileCopyrightText: 2024 Wilson Snyder
// SPDX-License-Identifier: LGPL-3.0-only OR Artistic-2.0

#include "verilated.h"
#include "verilated_vpi.h"

#include <cstdio>
#include <cstring>
#include <iostream>

#include "TestVpi.h"

int errors = 0;

extern "C" int mon_check() {
    vpiHandle root = vpi_handle(vpiScope, nullptr);

    // Test 1: Get quads array, then access quads[2] using multi_index
    vpiHandle vh_quads = vpi_handle_by_name((PLI_BYTE8*)"t.quads", root);
    CHECK_RESULT_NZ(vh_quads);

    // Single index: quads[2]
    PLI_INT32 indices_single[1] = {2};
    vpiHandle vh_quads_2 = vpi_handle_by_multi_index(vh_quads, 1, indices_single);
    CHECK_RESULT_NZ(vh_quads_2);

    // Verify it's the same as using vpi_handle_by_index
    vpiHandle vh_quads_2_single = vpi_handle_by_index(vh_quads, 2);
    CHECK_RESULT_NZ(vh_quads_2_single);

    // Both should provide the same data
    s_vpi_value v;
    v.format = vpiVectorVal;
    s_vpi_vecval vv[2];
    v.value.vector = vv;

    vpi_get_value(vh_quads_2, &v);
    uint64_t val_multi = ((uint64_t)vv[1].aval << 32) | vv[0].aval;

    vpi_get_value(vh_quads_2_single, &v);
    uint64_t val_single = ((uint64_t)vv[1].aval << 32) | vv[0].aval;

    CHECK_RESULT(val_multi, val_single);

    // Test 2: Try quads[3]
    PLI_INT32 indices_3[1] = {3};
    vpiHandle vh_quads_3 = vpi_handle_by_multi_index(vh_quads, 1, indices_3);
    CHECK_RESULT_NZ(vh_quads_3);

    vpi_get_value(vh_quads_3, &v);
    uint64_t val = ((uint64_t)vv[1].aval << 32) | vv[0].aval;
    (void)val;  // Suppress unused variable warning

    // Test 3: Test 2D array access mem_2d[1][3]
    vpiHandle vh_mem_2d = vpi_handle_by_name((PLI_BYTE8*)"t.mem_2d", root);
    CHECK_RESULT_NZ(vh_mem_2d);

    PLI_INT32 indices_2d[2] = {1, 3};
    vpiHandle vh_mem_2d_1_3 = vpi_handle_by_multi_index(vh_mem_2d, 2, indices_2d);
    CHECK_RESULT_NZ(vh_mem_2d_1_3);

    v.format = vpiIntVal;
    vpi_get_value(vh_mem_2d_1_3, &v);
    int val_mem = v.value.integer;
    // mem_2d[1][3] should be 1*8 + 3 = 11
    CHECK_RESULT(val_mem, 11);

    // Test 4: Compare with sequential single-index calls
    vpiHandle vh_mem_1st = vpi_handle_by_index(vh_mem_2d, 1);
    CHECK_RESULT_NZ(vh_mem_1st);

    vpiHandle vh_mem_sequential = vpi_handle_by_index(vh_mem_1st, 3);
    CHECK_RESULT_NZ(vh_mem_sequential);

    v.format = vpiIntVal;
    vpi_get_value(vh_mem_sequential, &v);
    int val_sequential = v.value.integer;
    CHECK_RESULT(val_sequential, 11);

    // Test 5: Test error handling - out of bounds
    PLI_INT32 indices_oob[1] = {99};  // Out of bounds for quads which is [2:3]
    vpiHandle vh_quads_oob = vpi_handle_by_multi_index(vh_quads, 1, indices_oob);
    CHECK_RESULT_Z(vh_quads_oob);

    // Test 6: Test error handling - null index array
    vpiHandle vh_null = vpi_handle_by_multi_index(vh_quads, 1, nullptr);
    CHECK_RESULT_Z(vh_null);

    // Test 7: Test error handling - zero num_index
    PLI_INT32 indices_dummy[1] = {2};
    vpiHandle vh_zero = vpi_handle_by_multi_index(vh_quads, 0, indices_dummy);
    CHECK_RESULT_Z(vh_zero);

    // Test 8: vpi_handle_by_name with single index
    vpiHandle vh_quads_2_by_name = vpi_handle_by_name((PLI_BYTE8*)"t.quads[2]", root);
    CHECK_RESULT_NZ(vh_quads_2_by_name);

    // Compare values with direct multi-index result
    v.format = vpiVectorVal;
    s_vpi_vecval vv2[2];
    v.value.vector = vv2;

    vpi_get_value(vh_quads_2, &v);
    val_multi = ((uint64_t)vv2[1].aval << 32) | vv2[0].aval;

    vpi_get_value(vh_quads_2_by_name, &v);
    uint64_t val_by_name = ((uint64_t)vv2[1].aval << 32) | vv2[0].aval;

    CHECK_RESULT(val_multi, val_by_name);

    // Test 9: vpi_handle_by_name with 2D array indexing
    vpiHandle vh_mem_2d_by_name = vpi_handle_by_name((PLI_BYTE8*)"t.mem_2d[1][3]", root);
    CHECK_RESULT_NZ(vh_mem_2d_by_name);

    v.format = vpiIntVal;
    vpi_get_value(vh_mem_2d_by_name, &v);
    int val_mem_2d_1_3 = v.value.integer;
    CHECK_RESULT(val_mem_2d_1_3, 11);

    // Test 10: vpi_handle_by_name with 2D array indexing (different index)
    vpiHandle vh_mem_2d_2_5_by_name = vpi_handle_by_name((PLI_BYTE8*)"t.mem_2d[2][5]", root);
    CHECK_RESULT_NZ(vh_mem_2d_2_5_by_name);

    v.format = vpiIntVal;
    vpi_get_value(vh_mem_2d_2_5_by_name, &v);
    int val_2d_2_5 = v.value.integer;
    int expected_2d_2_5 = 2 * 8 + 5;  // 21
    CHECK_RESULT(val_2d_2_5, expected_2d_2_5);

    // Test 11: vpi_handle_by_name with 1D array indexing
    vpiHandle vh_mem_1d_by_name = vpi_handle_by_name((PLI_BYTE8*)"t.mem_1d[7]", root);
    CHECK_RESULT_NZ(vh_mem_1d_by_name);

    v.format = vpiIntVal;
    vpi_get_value(vh_mem_1d_by_name, &v);
    int val_1d_7 = v.value.integer;
    int expected_1d_7 = 7 * 256;
    CHECK_RESULT(val_1d_7, expected_1d_7);

    return 0;
}
