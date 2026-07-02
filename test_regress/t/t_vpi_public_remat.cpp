// -*- mode: C++; c-file-style: "cc-mode" -*-
//*************************************************************************
//
// Test for public signal rematerialization feature
// Verifies that public signals with simple combinatorial logic can be
// optimized away but still accessed via VPI through generated getters
//
// SPDX-License-Identifier: CC0-1.0
//
//*************************************************************************

#include "verilated.h"
#include "verilated_vpi.h"

#include "Vt_vpi_public_remat.h"
#include "Vt_vpi_public_remat__Dpi.h"

#include <cstdio>
#include <cstdlib>

// These require the above
#include "TestCheck.h"
#include "TestVpi.h"

int errors = 0;

// Drive inputs for a test case
extern "C" int mon_drive(int test_case, int a_val, int b_val) {
    // Get handles to input signals
    TestVpiHandle ah = vpi_handle_by_name((PLI_BYTE8*)"t.a", NULL);
    CHECK_RESULT_NZ(ah);
    TestVpiHandle bh = vpi_handle_by_name((PLI_BYTE8*)"t.b", NULL);
    CHECK_RESULT_NZ(bh);

    s_vpi_value v;
    v.format = vpiIntVal;

    printf("Test case %d: a=%d, b=%d\n", test_case, a_val, b_val);
    v.value.integer = a_val;
    vpi_put_value(ah, &v, NULL, vpiNoDelay);
    v.value.integer = b_val;
    vpi_put_value(bh, &v, NULL, vpiNoDelay);

    return errors;
}

// Check outputs for current test case
extern "C" int mon_check(int test_case) {
    // Get handles to rematerialized signals
    TestVpiHandle sumh = vpi_handle_by_name((PLI_BYTE8*)"t.sum", NULL);
    CHECK_RESULT_NZ(sumh);
    TestVpiHandle producth = vpi_handle_by_name((PLI_BYTE8*)"t.product", NULL);
    CHECK_RESULT_NZ(producth);
    TestVpiHandle shiftedh = vpi_handle_by_name((PLI_BYTE8*)"t.shifted", NULL);
    CHECK_RESULT_NZ(shiftedh);
    TestVpiHandle wide_sumh = vpi_handle_by_name((PLI_BYTE8*)"t.wide_sum", NULL);
    CHECK_RESULT_NZ(wide_sumh);
    TestVpiHandle complexh = vpi_handle_by_name((PLI_BYTE8*)"t.complex", NULL);
    CHECK_RESULT_NZ(complexh);
    TestVpiHandle rw_signalh = vpi_handle_by_name((PLI_BYTE8*)"t.rw_signal", NULL);
    CHECK_RESULT_NZ(rw_signalh);

    s_vpi_value v;
    v.format = vpiIntVal;

    if (test_case == 1) {
        // Test case 1: a=5, b=3
        // Read and verify sum (5 + 3 = 8)
        vpi_get_value(sumh, &v);
        CHECK_RESULT(v.value.integer, 8);
        printf("  sum = %d (expected 8)\n", v.value.integer);

        // Read and verify product (5 * 3 = 15)
        vpi_get_value(producth, &v);
        CHECK_RESULT(v.value.integer, 15);
        printf("  product = %d (expected 15)\n", v.value.integer);

        // Read and verify shifted (5 << 2 = 20)
        vpi_get_value(shiftedh, &v);
        CHECK_RESULT(v.value.integer, 20);
        printf("  shifted = %d (expected 20)\n", v.value.integer);

        // Read and verify wide_sum (5 + 3 = 8)
        vpi_get_value(wide_sumh, &v);
        CHECK_RESULT(v.value.integer, 8);
        printf("  wide_sum = %d (expected 8)\n", v.value.integer);

        // Read and verify complex ((5 & 3) | (5 ^ 3) = 1 | 6 = 7)
        vpi_get_value(complexh, &v);
        CHECK_RESULT(v.value.integer, 7);
        printf("  complex = %d (expected 7)\n", v.value.integer);
    } else if (test_case == 2) {
        // Test case 2: a=255, b=1
        // Read and verify sum (255 + 1 = 256, but 8-bit wraps to 0)
        vpi_get_value(sumh, &v);
        CHECK_RESULT(v.value.integer, 0);
        printf("  sum = %d (expected 0, wrapped)\n", v.value.integer);

        // Read and verify product (255 * 1 = 255)
        vpi_get_value(producth, &v);
        CHECK_RESULT(v.value.integer, 255);
        printf("  product = %d (expected 255)\n", v.value.integer);

        // Read and verify shifted (255 << 2 = 1020, but 8-bit keeps lower 8 bits = 252)
        vpi_get_value(shiftedh, &v);
        CHECK_RESULT(v.value.integer, 252);
        printf("  shifted = %d (expected 252)\n", v.value.integer);

        // Read and verify wide_sum (255 + 1 = 256, 16-bit)
        vpi_get_value(wide_sumh, &v);
        CHECK_RESULT(v.value.integer, 256);
        printf("  wide_sum = %d (expected 256)\n", v.value.integer);
    } else if (test_case == 3) {
        // Test case 3: a=0xAA, b=0x55
        // Read and verify sum (0xAA + 0x55 = 0xFF)
        vpi_get_value(sumh, &v);
        CHECK_RESULT(v.value.integer, 0xFF);
        printf("  sum = 0x%x (expected 0xFF)\n", v.value.integer);

        // Read and verify complex ((0xAA & 0x55) | (0xAA ^ 0x55) = 0 | 0xFF = 0xFF)
        vpi_get_value(complexh, &v);
        CHECK_RESULT(v.value.integer, 0xFF);
        printf("  complex = 0x%x (expected 0xFF)\n", v.value.integer);

        // Verify RW signal still has its initial value
        vpi_get_value(rw_signalh, &v);
        CHECK_RESULT(v.value.integer, 0x42);
        printf("  rw_signal = 0x%x (expected 0x42)\n", v.value.integer);

        // Check signal properties
        PLI_INT32 objType = vpi_get(vpiType, sumh);
        // CHECK_RESULT(objType, vpiNet);
        CHECK_RESULT(objType, vpiReg);

        // Check that we can get the full name
        PLI_BYTE8* fullname = vpi_get_str(vpiFullName, sumh);
        CHECK_RESULT_CSTR(fullname, "t.sum");
        printf("  sum full name: %s\n", fullname);
    }

    if (test_case == 3 && errors == 0) { printf("All VPI rematerialization tests passed!\n"); }

    return errors;
}
