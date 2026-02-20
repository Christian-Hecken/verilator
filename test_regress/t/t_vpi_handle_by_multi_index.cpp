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

int errors = 0;

extern "C" int mon_check() {
    vpiHandle root = vpi_handle(vpiScope, nullptr);

    printf("Testing vpi_handle_by_multi_index\n");

    // Test 1: Get quads array, then access quads[2] using multi_index
    vpiHandle vh_quads = vpi_handle_by_name((PLI_BYTE8*)"t.quads", root);
    if (!vh_quads) {
        printf("ERROR: Could not find quads\n");
        errors++;
        return 1;
    }

    // Single index: quads[2]
    PLI_INT32 indices_single[1] = {2};
    vpiHandle vh_quads_2 = vpi_handle_by_multi_index(vh_quads, 1, indices_single);
    if (!vh_quads_2) {
        printf("ERROR: vpi_handle_by_multi_index failed for quads[2]\n");
        errors++;
        return 1;
    }
    printf("SUCCESS: Got quads[2]\n");

    // Verify it's the same as using vpi_handle_by_index
    vpiHandle vh_quads_2_single = vpi_handle_by_index(vh_quads, 2);
    if (!vh_quads_2_single) {
        printf("ERROR: vpi_handle_by_index(quads, 2) failed\n");
        errors++;
        return 1;
    }

    // Both should provide the same data
    s_vpi_value v;
    v.format = vpiVectorVal;
    s_vpi_vecval vv[2];
    v.value.vector = vv;

    vpi_get_value(vh_quads_2, &v);
    uint64_t val_multi = ((uint64_t)vv[1].aval << 32) | vv[0].aval;

    vpi_get_value(vh_quads_2_single, &v);
    uint64_t val_single = ((uint64_t)vv[1].aval << 32) | vv[0].aval;

    if (val_multi != val_single) {
        printf("ERROR: Multi-index and single-index values differ: 0x%lx vs 0x%lx\n", val_multi,
               val_single);
        errors++;
    } else {
        printf("SUCCESS: quads[2] values match: 0x%lx\n", val_multi);
    }

    // Test 2: Try quads[3]
    PLI_INT32 indices_3[1] = {3};
    vpiHandle vh_quads_3 = vpi_handle_by_multi_index(vh_quads, 1, indices_3);
    if (!vh_quads_3) {
        printf("ERROR: vpi_handle_by_multi_index failed for quads[3]\n");
        errors++;
    } else {
        printf("SUCCESS: Got quads[3]\n");

        vpi_get_value(vh_quads_3, &v);
        uint64_t val = ((uint64_t)vv[1].aval << 32) | vv[0].aval;
        printf("SUCCESS: quads[3] value: 0x%lx\n", val);
    }

    // Test 3: Test 2D array access mem_2d[1][3]
    vpiHandle vh_mem_2d = vpi_handle_by_name((PLI_BYTE8*)"t.mem_2d", root);
    if (!vh_mem_2d) {
        printf("ERROR: Could not find mem_2d\n");
        errors++;
    } else {
        PLI_INT32 indices_2d[2] = {1, 3};
        vpiHandle vh_mem_2d_1_3 = vpi_handle_by_multi_index(vh_mem_2d, 2, indices_2d);
        if (!vh_mem_2d_1_3) {
            printf("ERROR: vpi_handle_by_multi_index failed for mem_2d[1][3]\n");
            errors++;
        } else {
            printf("SUCCESS: Got mem_2d[1][3]\n");

            v.format = vpiIntVal;
            vpi_get_value(vh_mem_2d_1_3, &v);
            int val_mem = v.value.integer;
            // mem_2d[1][3] should be 1*8 + 3 = 11
            if (val_mem != 11) {
                printf("ERROR: mem_2d[1][3] expected 11, got %d\n", val_mem);
                errors++;
            } else {
                printf("SUCCESS: mem_2d[1][3] = %d\n", val_mem);
            }

            // Test 4: Compare with sequential single-index calls
            vpiHandle vh_mem_1st = vpi_handle_by_index(vh_mem_2d, 1);
            if (!vh_mem_1st) {
                printf("ERROR: vpi_handle_by_index(mem_2d, 1) failed\n");
                errors++;
            } else {
                vpiHandle vh_mem_sequential = vpi_handle_by_index(vh_mem_1st, 3);
                if (!vh_mem_sequential) {
                    printf("ERROR: vpi_handle_by_index(vh_mem_1st, 3) failed\n");
                    errors++;
                } else {
                    v.format = vpiIntVal;
                    vpi_get_value(vh_mem_sequential, &v);
                    int val_sequential = v.value.integer;
                    if (val_sequential != 11) {
                        printf("ERROR: Sequential indexing mem_2d[1][3] expected 11, got %d\n",
                               val_sequential);
                        errors++;
                    } else {
                        printf("SUCCESS: Sequential indexing matches multi-index result\n");
                    }
                }
            }
        }
    }

    // Test 5: Test error handling - out of bounds
    PLI_INT32 indices_oob[1] = {99};  // Out of bounds for quads which is [2:3]
    vpiHandle vh_quads_oob = vpi_handle_by_multi_index(vh_quads, 1, indices_oob);
    if (vh_quads_oob != nullptr) {
        printf("ERROR: vpi_handle_by_multi_index should return null for out-of-bounds index\n");
        errors++;
    } else {
        printf("SUCCESS: Out-of-bounds check works correctly\n");
    }

    // Test 6: Test error handling - null index array
    vpiHandle vh_null = vpi_handle_by_multi_index(vh_quads, 1, nullptr);
    if (vh_null != nullptr) {
        printf("ERROR: vpi_handle_by_multi_index should return null for null index_array\n");
        errors++;
    } else {
        printf("SUCCESS: Null index_array check works correctly\n");
    }

    // Test 7: Test error handling - zero num_index
    PLI_INT32 indices_dummy[1] = {2};
    vpiHandle vh_zero = vpi_handle_by_multi_index(vh_quads, 0, indices_dummy);
    if (vh_zero != nullptr) {
        printf("ERROR: vpi_handle_by_multi_index should return null for num_index=0\n");
        errors++;
    } else {
        printf("SUCCESS: Zero num_index check works correctly\n");
    }

    if (errors == 0) {
        printf("\nAll tests passed!\n");
        return 0;
    } else {
        printf("\n%d errors found\n", errors);
        return 1;
    }
}
