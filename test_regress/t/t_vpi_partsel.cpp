// -*- mode: C++; c-file-style: "cc-mode" -*-
//*************************************************************************
//
// This file ONLY is placed under the Creative Commons Public Domain.
// SPDX-FileCopyrightText: 2025 Wilson Snyder
// SPDX-License-Identifier: CC0-1.0
//
//*************************************************************************
//
// Test vpi_handle_by_name with bit-range part-select notation [hi:lo].
// Verifies that the returned handle has correct vpiSize and that
// vpi_get_value/vpi_put_value read/write only the selected bits.

#ifdef IS_VPI

#include "vpi_user.h"

#include <cstdlib>

#else

#include "verilated.h"
#include "verilated_vpi.h"

#include "Vt_vpi_partsel.h"
#include "Vt_vpi_partsel__Dpi.h"
#include "svdpi.h"

#endif

#include <cstdio>
#include <cstring>
#include <iostream>

// These require the above. Comment prevents clang-format moving them
#include "TestSimulator.h"
#include "TestVpi.h"

#define TEST_MSG \
    if (0) printf

//======================================================================
// Helper: read vpiIntVal from a handle
static int vpi_get_int(vpiHandle h) {
    s_vpi_value val;
    val.format = vpiIntVal;
    vpi_get_value(h, &val);
    return val.value.integer;
}

// Helper: write vpiIntVal to a handle
static void vpi_put_int(vpiHandle h, int value) {
    s_vpi_value val;
    val.format = vpiIntVal;
    val.value.integer = value;
    s_vpi_time time_s = {vpiSimTime, 0, 0, 0.0};
    vpi_put_value(h, &val, &time_s, vpiNoDelay);
}

//======================================================================
// Test 1: Part-select on a descending-range signal [31:0]
// sig_desc = 32'hDEAD_BEEF
static int test_partsel_descending() {
    printf("== test_partsel_descending ==\n");

    // [15:8] of 0xDEAD_BEEF should give 0xBE (bits 15 down to 8)
    {
        TestVpiHandle vh = vpi_handle_by_name((PLI_BYTE8*)"t.sig_desc[15:8]", nullptr);
        if (!vh) {
            printf("%%Error: vpi_handle_by_name(\"t.sig_desc[15:8]\") returned NULL\n");
            return __LINE__;
        }
        int size = vpi_get(vpiSize, vh);
        if (size != 8) {
            printf("%%Error: vpiSize = %d, expected 8\n", size);
            return __LINE__;
        }
        int val = vpi_get_int(vh);
        if (val != 0xBE) {
            printf("%%Error: value = 0x%02x, expected 0xBE\n", val);
            return __LINE__;
        }
        printf("  sig_desc[15:8] size=%d value=0x%02x OK\n", size, val);
    }

    // [31:24] of 0xDEAD_BEEF should give 0xDE
    {
        TestVpiHandle vh = vpi_handle_by_name((PLI_BYTE8*)"t.sig_desc[31:24]", nullptr);
        if (!vh) {
            printf("%%Error: vpi_handle_by_name(\"t.sig_desc[31:24]\") returned NULL\n");
            return __LINE__;
        }
        int size = vpi_get(vpiSize, vh);
        if (size != 8) {
            printf("%%Error: vpiSize = %d, expected 8\n", size);
            return __LINE__;
        }
        int val = vpi_get_int(vh);
        if (val != 0xDE) {
            printf("%%Error: value = 0x%02x, expected 0xDE\n", val);
            return __LINE__;
        }
        printf("  sig_desc[31:24] size=%d value=0x%02x OK\n", size, val);
    }

    // [7:0] of 0xDEAD_BEEF should give 0xEF
    {
        TestVpiHandle vh = vpi_handle_by_name((PLI_BYTE8*)"t.sig_desc[7:0]", nullptr);
        if (!vh) {
            printf("%%Error: vpi_handle_by_name(\"t.sig_desc[7:0]\") returned NULL\n");
            return __LINE__;
        }
        int size = vpi_get(vpiSize, vh);
        if (size != 8) {
            printf("%%Error: vpiSize = %d, expected 8\n", size);
            return __LINE__;
        }
        int val = vpi_get_int(vh);
        if (val != 0xEF) {
            printf("%%Error: value = 0x%02x, expected 0xEF\n", val);
            return __LINE__;
        }
        printf("  sig_desc[7:0] size=%d value=0x%02x OK\n", size, val);
    }

    // [23:16] of 0xDEAD_BEEF should give 0xAD
    {
        TestVpiHandle vh = vpi_handle_by_name((PLI_BYTE8*)"t.sig_desc[23:16]", nullptr);
        if (!vh) {
            printf("%%Error: vpi_handle_by_name(\"t.sig_desc[23:16]\") returned NULL\n");
            return __LINE__;
        }
        int val = vpi_get_int(vh);
        if (val != 0xAD) {
            printf("%%Error: value = 0x%02x, expected 0xAD\n", val);
            return __LINE__;
        }
        printf("  sig_desc[23:16] value=0x%02x OK\n", val);
    }

    // Single-bit range [5:5] should give size 1
    {
        TestVpiHandle vh = vpi_handle_by_name((PLI_BYTE8*)"t.sig_desc[5:5]", nullptr);
        if (!vh) {
            printf("%%Error: vpi_handle_by_name(\"t.sig_desc[5:5]\") returned NULL\n");
            return __LINE__;
        }
        int size = vpi_get(vpiSize, vh);
        if (size != 1) {
            printf("%%Error: vpiSize = %d, expected 1\n", size);
            return __LINE__;
        }
        // Bit 5 of 0xDEAD_BEEF: 0xEF = 1110_1111, bit 5 = 1
        int val = vpi_get_int(vh);
        if (val != 1) {
            printf("%%Error: value = %d, expected 1\n", val);
            return __LINE__;
        }
        printf("  sig_desc[5:5] size=%d value=%d OK\n", size, val);
    }

    // Full range [31:0] should give size 32 and full value
    {
        TestVpiHandle vh = vpi_handle_by_name((PLI_BYTE8*)"t.sig_desc[31:0]", nullptr);
        if (!vh) {
            printf("%%Error: vpi_handle_by_name(\"t.sig_desc[31:0]\") returned NULL\n");
            return __LINE__;
        }
        int size = vpi_get(vpiSize, vh);
        if (size != 32) {
            printf("%%Error: vpiSize = %d, expected 32\n", size);
            return __LINE__;
        }
        int val = vpi_get_int(vh);
        if (val != (int)0xDEADBEEF) {
            printf("%%Error: value = 0x%08x, expected 0xDEADBEEF\n", val);
            return __LINE__;
        }
        printf("  sig_desc[31:0] size=%d value=0x%08x OK\n", size, val);
    }

    return 0;
}

//======================================================================
// Test 2: Part-select on an ascending-range signal [0:31]
// sig_asc = 32'h1234_5678
// For [0:31], bit 0 is the MSB in storage. So [0:7] selects the
// top 8 bits: 0x12. And [24:31] selects the bottom 8 bits: 0x78.
static int test_partsel_ascending() {
    printf("== test_partsel_ascending ==\n");

    // [0:7] of 0x1234_5678: selects MSB byte = 0x12
    {
        TestVpiHandle vh = vpi_handle_by_name((PLI_BYTE8*)"t.sig_asc[0:7]", nullptr);
        if (!vh) {
            printf("%%Error: vpi_handle_by_name(\"t.sig_asc[0:7]\") returned NULL\n");
            return __LINE__;
        }
        int size = vpi_get(vpiSize, vh);
        if (size != 8) {
            printf("%%Error: vpiSize = %d, expected 8\n", size);
            return __LINE__;
        }
        int val = vpi_get_int(vh);
        if (val != 0x12) {
            printf("%%Error: value = 0x%02x, expected 0x12\n", val);
            return __LINE__;
        }
        printf("  sig_asc[0:7] size=%d value=0x%02x OK\n", size, val);
    }

    // [24:31] of 0x1234_5678: selects LSB byte = 0x78
    {
        TestVpiHandle vh = vpi_handle_by_name((PLI_BYTE8*)"t.sig_asc[24:31]", nullptr);
        if (!vh) {
            printf("%%Error: vpi_handle_by_name(\"t.sig_asc[24:31]\") returned NULL\n");
            return __LINE__;
        }
        int size = vpi_get(vpiSize, vh);
        if (size != 8) {
            printf("%%Error: vpiSize = %d, expected 8\n", size);
            return __LINE__;
        }
        int val = vpi_get_int(vh);
        if (val != 0x78) {
            printf("%%Error: value = 0x%02x, expected 0x78\n", val);
            return __LINE__;
        }
        printf("  sig_asc[24:31] size=%d value=0x%02x OK\n", size, val);
    }

    return 0;
}

//======================================================================
// Test 3: Part-select combined with array index
// mem[1] = 32'hBBBB_1111
static int test_partsel_with_array() {
    printf("== test_partsel_with_array ==\n");

    // mem[1][15:8] should give 0x11 (bits 15:8 of 0xBBBB_1111)
    {
        TestVpiHandle vh = vpi_handle_by_name((PLI_BYTE8*)"t.mem[1][15:8]", nullptr);
        if (!vh) {
            printf("%%Error: vpi_handle_by_name(\"t.mem[1][15:8]\") returned NULL\n");
            return __LINE__;
        }
        int size = vpi_get(vpiSize, vh);
        if (size != 8) {
            printf("%%Error: vpiSize = %d, expected 8\n", size);
            return __LINE__;
        }
        int val = vpi_get_int(vh);
        if (val != 0x11) {
            printf("%%Error: value = 0x%02x, expected 0x11\n", val);
            return __LINE__;
        }
        printf("  mem[1][15:8] size=%d value=0x%02x OK\n", size, val);
    }

    // mem[2][31:16] should give 0xCCCC (top half of 0xCCCC_2222)
    {
        TestVpiHandle vh = vpi_handle_by_name((PLI_BYTE8*)"t.mem[2][31:16]", nullptr);
        if (!vh) {
            printf("%%Error: vpi_handle_by_name(\"t.mem[2][31:16]\") returned NULL\n");
            return __LINE__;
        }
        int size = vpi_get(vpiSize, vh);
        if (size != 16) {
            printf("%%Error: vpiSize = %d, expected 16\n", size);
            return __LINE__;
        }
        int val = vpi_get_int(vh);
        if (val != (int)0xCCCC) {
            printf("%%Error: value = 0x%04x, expected 0xCCCC\n", val);
            return __LINE__;
        }
        printf("  mem[2][31:16] size=%d value=0x%04x OK\n", size, val);
    }

    return 0;
}

//======================================================================
// Test 4: Out-of-range part-select should return NULL
static int test_partsel_out_of_range() {
    printf("== test_partsel_out_of_range ==\n");

    // sig_desc is [31:0], so [99:90] is out of range
    {
        TestVpiHandle vh = vpi_handle_by_name((PLI_BYTE8*)"t.sig_desc[99:90]", nullptr);
        if (vh) {
            printf("%%Error: vpi_handle_by_name(\"t.sig_desc[99:90]\") returned !NULL\n");
            return __LINE__;
        }
        printf("  sig_desc[99:90] = NULL (out of range) OK\n");
    }

    // sig_desc [32:24] - bit 32 is out of range for [31:0]
    {
        TestVpiHandle vh = vpi_handle_by_name((PLI_BYTE8*)"t.sig_desc[32:24]", nullptr);
        if (vh) {
            printf("%%Error: vpi_handle_by_name(\"t.sig_desc[32:24]\") returned !NULL\n");
            return __LINE__;
        }
        printf("  sig_desc[32:24] = NULL (out of range) OK\n");
    }

    // Negative index [7:-1] for [31:0] range
    {
        TestVpiHandle vh = vpi_handle_by_name((PLI_BYTE8*)"t.sig_desc[7:-1]", nullptr);
        if (vh) {
            printf("%%Error: vpi_handle_by_name(\"t.sig_desc[7:-1]\") returned !NULL\n");
            return __LINE__;
        }
        printf("  sig_desc[7:-1] = NULL (out of range) OK\n");
    }

    return 0;
}

//======================================================================
// Test 5: Write via part-selected handle
// Verify that writing to sig_desc[15:8] only changes those bits.
static int test_partsel_write() {
    printf("== test_partsel_write ==\n");

    // sig_desc starts as 0xDEAD_BEEF
    // Write 0x42 to [15:8], result should be 0xDEAD_42EF
    {
        TestVpiHandle vh_partsel = vpi_handle_by_name((PLI_BYTE8*)"t.sig_desc[15:8]", nullptr);
        if (!vh_partsel) {
            printf("%%Error: vpi_handle_by_name(\"t.sig_desc[15:8]\") returned NULL\n");
            return __LINE__;
        }

        // Write 0x42 to the part-selected region
        vpi_put_int(vh_partsel, 0x42);

        // Read back the full signal to verify only bits [15:8] changed
        TestVpiHandle vh_full = vpi_handle_by_name((PLI_BYTE8*)"t.sig_desc", nullptr);
        if (!vh_full) {
            printf("%%Error: vpi_handle_by_name(\"t.sig_desc\") returned NULL\n");
            return __LINE__;
        }
        int full_val = vpi_get_int(vh_full);
        if (full_val != (int)0xDEAD42EF) {
            printf("%%Error: full value = 0x%08x, expected 0xDEAD42EF\n", full_val);
            return __LINE__;
        }
        printf("  Write 0x42 to sig_desc[15:8] -> full = 0x%08x OK\n", full_val);

        // Read back via part-select to verify
        int part_val = vpi_get_int(vh_partsel);
        if (part_val != 0x42) {
            printf("%%Error: readback = 0x%02x, expected 0x42\n", part_val);
            return __LINE__;
        }
        printf("  Readback sig_desc[15:8] = 0x%02x OK\n", part_val);
    }

    // Write 0xFF to [31:24], result should be 0xFFAD_42EF
    {
        TestVpiHandle vh_partsel = vpi_handle_by_name((PLI_BYTE8*)"t.sig_desc[31:24]", nullptr);
        if (!vh_partsel) {
            printf("%%Error: vpi_handle_by_name(\"t.sig_desc[31:24]\") returned NULL\n");
            return __LINE__;
        }

        vpi_put_int(vh_partsel, 0xFF);

        TestVpiHandle vh_full = vpi_handle_by_name((PLI_BYTE8*)"t.sig_desc", nullptr);
        int full_val = vpi_get_int(vh_full);
        if (full_val != (int)0xFFAD42EF) {
            printf("%%Error: full value = 0x%08x, expected 0xFFAD42EF\n", full_val);
            return __LINE__;
        }
        printf("  Write 0xFF to sig_desc[31:24] -> full = 0x%08x OK\n", full_val);
    }

    return 0;
}

//======================================================================
// Test 6: Part-select on array element with write
// mem[3] = 0xDDDD_3333, write 0xAB to mem[3][7:0], expect 0xDDDD_33AB
static int test_partsel_array_write() {
    printf("== test_partsel_array_write ==\n");

    {
        TestVpiHandle vh_partsel = vpi_handle_by_name((PLI_BYTE8*)"t.mem[3][7:0]", nullptr);
        if (!vh_partsel) {
            printf("%%Error: vpi_handle_by_name(\"t.mem[3][7:0]\") returned NULL\n");
            return __LINE__;
        }

        // Write 0xAB to low byte
        vpi_put_int(vh_partsel, 0xAB);

        // Read back full element to verify
        TestVpiHandle vh_full = vpi_handle_by_name((PLI_BYTE8*)"t.mem[3]", nullptr);
        if (!vh_full) {
            printf("%%Error: vpi_handle_by_name(\"t.mem[3]\") returned NULL\n");
            return __LINE__;
        }
        int full_val = vpi_get_int(vh_full);
        if (full_val != (int)0xDDDD33AB) {
            printf("%%Error: full value = 0x%08x, expected 0xDDDD33AB\n", full_val);
            return __LINE__;
        }
        printf("  Write 0xAB to mem[3][7:0] -> full = 0x%08x OK\n", full_val);
    }

    return 0;
}

//======================================================================
// Test 8: Arithmetic expressions in indices and bit-ranges
// sig_desc = 32'hDEAD_BEEF = 0xDEADBEEF
static int test_arithmetic_exprs() {
    printf("== test_arithmetic_exprs ==\n");

    // --- Arithmetic in bit-range bounds ---

    // [8+7:8] should be equivalent to [15:8] -> 0xBE
    {
        TestVpiHandle vh = vpi_handle_by_name((PLI_BYTE8*)"t.sig_desc[8+7:8]", nullptr);
        if (!vh) {
            printf("%%Error: vpi_handle_by_name(\"t.sig_desc[8+7:8]\") returned NULL\n");
            return __LINE__;
        }
        int size = vpi_get(vpiSize, vh);
        if (size != 8) {
            printf("%%Error: vpiSize = %d, expected 8\n", size);
            return __LINE__;
        }
        int val = vpi_get_int(vh);
        if (val != 0xBE) {
            printf("%%Error: value = 0x%02x, expected 0xBE\n", val);
            return __LINE__;
        }
        printf("  sig_desc[8+7:8] size=%d value=0x%02x OK\n", size, val);
    }

    // [4*8-1:3*8] should be equivalent to [31:24] -> 0xDE
    {
        TestVpiHandle vh = vpi_handle_by_name((PLI_BYTE8*)"t.sig_desc[4*8-1:3*8]", nullptr);
        if (!vh) {
            printf("%%Error: vpi_handle_by_name(\"t.sig_desc[4*8-1:3*8]\") returned NULL\n");
            return __LINE__;
        }
        int size = vpi_get(vpiSize, vh);
        if (size != 8) {
            printf("%%Error: vpiSize = %d, expected 8\n", size);
            return __LINE__;
        }
        int val = vpi_get_int(vh);
        if (val != 0xDE) {
            printf("%%Error: value = 0x%02x, expected 0xDE\n", val);
            return __LINE__;
        }
        printf("  sig_desc[4*8-1:3*8] size=%d value=0x%02x OK\n", size, val);
    }

    // [(16-1):(24/3)] should be equivalent to [15:8] -> 0xBE
    {
        TestVpiHandle vh = vpi_handle_by_name((PLI_BYTE8*)"t.sig_desc[(16-1):(24/3)]", nullptr);
        if (!vh) {
            printf("%%Error: vpi_handle_by_name(\"t.sig_desc[(16-1):(24/3)]\") returned NULL\n");
            return __LINE__;
        }
        int val = vpi_get_int(vh);
        if (val != 0xBE) {
            printf("%%Error: value = 0x%02x, expected 0xBE\n", val);
            return __LINE__;
        }
        printf("  sig_desc[(16-1):(24/3)] value=0x%02x OK\n", val);
    }

    // --- Arithmetic in array index ---

    // mem[1+1] should be same as mem[2], which has value 0xCCCC2222
    {
        TestVpiHandle vh = vpi_handle_by_name((PLI_BYTE8*)"t.mem[1+1]", nullptr);
        if (!vh) {
            printf("%%Error: vpi_handle_by_name(\"t.mem[1+1]\") returned NULL\n");
            return __LINE__;
        }
        int val = vpi_get_int(vh);
        if (val != (int)0xCCCC2222) {
            printf("%%Error: value = 0x%08x, expected 0xCCCC2222\n", val);
            return __LINE__;
        }
        printf("  mem[1+1] value=0x%08x OK\n", val);
    }

    // mem[6/2] should be same as mem[3], which has value 0xDDDD3333
    {
        TestVpiHandle vh = vpi_handle_by_name((PLI_BYTE8*)"t.mem[6/2]", nullptr);
        if (!vh) {
            printf("%%Error: vpi_handle_by_name(\"t.mem[6/2]\") returned NULL\n");
            return __LINE__;
        }
        int val = vpi_get_int(vh);
        if (val != (int)0xDDDD3333) {
            printf("%%Error: value = 0x%08x, expected 0xDDDD3333\n", val);
            return __LINE__;
        }
        printf("  mem[6/2] value=0x%08x OK\n", val);
    }

    // --- Combined: arithmetic in both index and bit-range ---

    // mem[3-1][8*2-1:8] should be mem[2][15:8] = byte 1 of 0xCCCC2222 = 0x22
    {
        TestVpiHandle vh = vpi_handle_by_name((PLI_BYTE8*)"t.mem[3-1][8*2-1:8]", nullptr);
        if (!vh) {
            printf("%%Error: vpi_handle_by_name(\"t.mem[3-1][8*2-1:8]\") returned NULL\n");
            return __LINE__;
        }
        int size = vpi_get(vpiSize, vh);
        if (size != 8) {
            printf("%%Error: vpiSize = %d, expected 8\n", size);
            return __LINE__;
        }
        int val = vpi_get_int(vh);
        if (val != 0x22) {
            printf("%%Error: value = 0x%02x, expected 0x22\n", val);
            return __LINE__;
        }
        printf("  mem[3-1][8*2-1:8] size=%d value=0x%02x OK\n", size, val);
    }

    // --- Indexed part-select with +: and -: ---

    // sig_desc[8+:8] should be equivalent to [15:8] -> 0xBE
    {
        TestVpiHandle vh = vpi_handle_by_name((PLI_BYTE8*)"t.sig_desc[8+:8]", nullptr);
        if (!vh) {
            printf("%%Error: vpi_handle_by_name(\"t.sig_desc[8+:8]\") returned NULL\n");
            return __LINE__;
        }
        int size = vpi_get(vpiSize, vh);
        if (size != 8) {
            printf("%%Error: vpiSize = %d, expected 8\n", size);
            return __LINE__;
        }
        int val = vpi_get_int(vh);
        if (val != 0xBE) {
            printf("%%Error: value = 0x%02x, expected 0xBE\n", val);
            return __LINE__;
        }
        printf("  sig_desc[8+:8] size=%d value=0x%02x OK\n", size, val);
    }

    // sig_desc[15-:8] should be equivalent to [15:8] -> 0xBE
    {
        TestVpiHandle vh = vpi_handle_by_name((PLI_BYTE8*)"t.sig_desc[15-:8]", nullptr);
        if (!vh) {
            printf("%%Error: vpi_handle_by_name(\"t.sig_desc[15-:8]\") returned NULL\n");
            return __LINE__;
        }
        int size = vpi_get(vpiSize, vh);
        if (size != 8) {
            printf("%%Error: vpiSize = %d, expected 8\n", size);
            return __LINE__;
        }
        int val = vpi_get_int(vh);
        if (val != 0xBE) {
            printf("%%Error: value = 0x%02x, expected 0xBE\n", val);
            return __LINE__;
        }
        printf("  sig_desc[15-:8] size=%d value=0x%02x OK\n", size, val);
    }

    // sig_desc[0+:16] should be equivalent to [15:0] -> 0xBEEF
    {
        TestVpiHandle vh = vpi_handle_by_name((PLI_BYTE8*)"t.sig_desc[0+:16]", nullptr);
        if (!vh) {
            printf("%%Error: vpi_handle_by_name(\"t.sig_desc[0+:16]\") returned NULL\n");
            return __LINE__;
        }
        int size = vpi_get(vpiSize, vh);
        if (size != 16) {
            printf("%%Error: vpiSize = %d, expected 16\n", size);
            return __LINE__;
        }
        int val = vpi_get_int(vh);
        if (val != (int)0xBEEF) {
            printf("%%Error: value = 0x%04x, expected 0xBEEF\n", val);
            return __LINE__;
        }
        printf("  sig_desc[0+:16] size=%d value=0x%04x OK\n", size, val);
    }

    // sig_desc[31-:16] should be equivalent to [31:16] -> 0xDEAD
    {
        TestVpiHandle vh = vpi_handle_by_name((PLI_BYTE8*)"t.sig_desc[31-:16]", nullptr);
        if (!vh) {
            printf("%%Error: vpi_handle_by_name(\"t.sig_desc[31-:16]\") returned NULL\n");
            return __LINE__;
        }
        int size = vpi_get(vpiSize, vh);
        if (size != 16) {
            printf("%%Error: vpiSize = %d, expected 16\n", size);
            return __LINE__;
        }
        int val = vpi_get_int(vh);
        if (val != (int)0xDEAD) {
            printf("%%Error: value = 0x%04x, expected 0xDEAD\n", val);
            return __LINE__;
        }
        printf("  sig_desc[31-:16] size=%d value=0x%04x OK\n", size, val);
    }

    // +: with arithmetic: sig_desc[BYTE+:BYTE] -> [15:8] = 0xBE
    {
        TestVpiHandle vh = vpi_handle_by_name((PLI_BYTE8*)"t.sig_desc[BYTE+:BYTE]", nullptr);
        if (!vh) {
            printf("%%Error: vpi_handle_by_name(\"t.sig_desc[BYTE+:BYTE]\") returned NULL\n");
            return __LINE__;
        }
        int val = vpi_get_int(vh);
        if (val != 0xBE) {
            printf("%%Error: value = 0x%02x, expected 0xBE\n", val);
            return __LINE__;
        }
        printf("  sig_desc[BYTE+:BYTE] value=0x%02x OK\n", val);
    }

    // +: with array index: mem[1][0+:8] -> low byte of 0xBBBB1111 = 0x11
    {
        TestVpiHandle vh = vpi_handle_by_name((PLI_BYTE8*)"t.mem[1][0+:8]", nullptr);
        if (!vh) {
            printf("%%Error: vpi_handle_by_name(\"t.mem[1][0+:8]\") returned NULL\n");
            return __LINE__;
        }
        int size = vpi_get(vpiSize, vh);
        if (size != 8) {
            printf("%%Error: vpiSize = %d, expected 8\n", size);
            return __LINE__;
        }
        int val = vpi_get_int(vh);
        if (val != 0x11) {
            printf("%%Error: value = 0x%02x, expected 0x11\n", val);
            return __LINE__;
        }
        printf("  mem[1][0+:8] size=%d value=0x%02x OK\n", size, val);
    }

    // --- Edge cases for +:/−: vs arithmetic disambiguation ---

    // "3+5:8" — the '+' is NOT immediately before ':', so it's a plain range [8:8]
    // sig_desc = 0xDEADBEEF, bit 8 = 1 (0xEF = 1110_1111, bit 8 is in 0xBE = 1011_1110)
    {
        TestVpiHandle vh = vpi_handle_by_name((PLI_BYTE8*)"t.sig_desc[3+5:8]", nullptr);
        if (!vh) {
            printf("%%Error: vpi_handle_by_name(\"t.sig_desc[3+5:8]\") returned NULL\n");
            return __LINE__;
        }
        int size = vpi_get(vpiSize, vh);
        if (size != 1) {
            printf("%%Error: vpiSize = %d, expected 1\n", size);
            return __LINE__;
        }
        // Bit 8 of 0xDEADBEEF: 0xBE = 1011_1110, bit 0 of that byte = 0
        int val = vpi_get_int(vh);
        if (val != 0) {
            printf("%%Error: value = %d, expected 0\n", val);
            return __LINE__;
        }
        printf("  sig_desc[3+5:8] (plain range [8:8]) size=%d value=%d OK\n", size, val);
    }

    // "3+5+:8" — the '+' IS immediately before ':', so it's [8 +: 8] = [15:8] -> 0xBE
    {
        TestVpiHandle vh = vpi_handle_by_name((PLI_BYTE8*)"t.sig_desc[3+5+:8]", nullptr);
        if (!vh) {
            printf("%%Error: vpi_handle_by_name(\"t.sig_desc[3+5+:8]\") returned NULL\n");
            return __LINE__;
        }
        int size = vpi_get(vpiSize, vh);
        if (size != 8) {
            printf("%%Error: vpiSize = %d, expected 8\n", size);
            return __LINE__;
        }
        int val = vpi_get_int(vh);
        if (val != 0xBE) {
            printf("%%Error: value = 0x%02x, expected 0xBE\n", val);
            return __LINE__;
        }
        printf("  sig_desc[3+5+:8] (base=8, +: 8) size=%d value=0x%02x OK\n", size, val);
    }

    // "3+5-:8" — '-' IS immediately before ':', so it's [8 -: 8] = [8:1]
    // Bits [8:1] of 0xDEADBEEF: ...1011_1110_1110_1111, bits 8..1 = 0x77
    //   bit 8=0, bit 7=1, bit 6=1, bit 5=1, bit 4=0, bit 3=1, bit 2=1, bit 1=1
    //   = 0111_0111 = 0x77
    {
        TestVpiHandle vh = vpi_handle_by_name((PLI_BYTE8*)"t.sig_desc[3+5-:8]", nullptr);
        if (!vh) {
            printf("%%Error: vpi_handle_by_name(\"t.sig_desc[3+5-:8]\") returned NULL\n");
            return __LINE__;
        }
        int size = vpi_get(vpiSize, vh);
        if (size != 8) {
            printf("%%Error: vpiSize = %d, expected 8\n", size);
            return __LINE__;
        }
        int val = vpi_get_int(vh);
        if (val != 0x77) {
            printf("%%Error: value = 0x%02x, expected 0x77\n", val);
            return __LINE__;
        }
        printf("  sig_desc[3+5-:8] (base=8, -: 8) size=%d value=0x%02x OK\n", size, val);
    }

    // Whitespace tolerance: "8 +: 8" with spaces around +: operator
    {
        TestVpiHandle vh = vpi_handle_by_name((PLI_BYTE8*)"t.sig_desc[ 8 +: 8 ]", nullptr);
        if (!vh) {
            printf("%%Error: vpi_handle_by_name(\"t.sig_desc[ 8 +: 8 ]\") returned NULL\n");
            return __LINE__;
        }
        int val = vpi_get_int(vh);
        if (val != 0xBE) {
            printf("%%Error: value = 0x%02x, expected 0xBE\n", val);
            return __LINE__;
        }
        printf("  sig_desc[ 8 +: 8 ] (whitespace) value=0x%02x OK\n", val);
    }

    // --- Edge cases that should fail ---

    // Width of 0 in +: should fail
    {
        TestVpiHandle vh = vpi_handle_by_name((PLI_BYTE8*)"t.sig_desc[8+:0]", nullptr);
        if (vh) {
            printf("%%Error: vpi_handle_by_name(\"t.sig_desc[8+:0]\") returned !NULL\n");
            return __LINE__;
        }
        printf("  sig_desc[8+:0] = NULL (zero width rejected) OK\n");
    }

    // Negative width in -: should fail
    {
        TestVpiHandle vh = vpi_handle_by_name((PLI_BYTE8*)"t.sig_desc[8+:-1]", nullptr);
        if (vh) {
            printf("%%Error: vpi_handle_by_name(\"t.sig_desc[8+:-1]\") returned !NULL\n");
            return __LINE__;
        }
        printf("  sig_desc[8+:-1] = NULL (negative width rejected) OK\n");
    }

    // -: that results in out-of-range bits should fail
    // [2 -: 8] -> [2:-5], lo=-5 is below signal range [31:0]
    {
        TestVpiHandle vh = vpi_handle_by_name((PLI_BYTE8*)"t.sig_desc[2-:8]", nullptr);
        if (vh) {
            printf("%%Error: vpi_handle_by_name(\"t.sig_desc[2-:8]\") returned !NULL\n");
            return __LINE__;
        }
        printf("  sig_desc[2-:8] = NULL (out of range: lo=-5) OK\n");
    }

    // +: that results in out-of-range bits should fail
    // [28 +: 8] -> [35:28], hi=35 is above signal range [31:0]
    {
        TestVpiHandle vh = vpi_handle_by_name((PLI_BYTE8*)"t.sig_desc[28+:8]", nullptr);
        if (vh) {
            printf("%%Error: vpi_handle_by_name(\"t.sig_desc[28+:8]\") returned !NULL\n");
            return __LINE__;
        }
        printf("  sig_desc[28+:8] = NULL (out of range: hi=35) OK\n");
    }

    // +: on unpacked dimension should fail (existing guard)
    {
        TestVpiHandle vh = vpi_handle_by_name((PLI_BYTE8*)"t.mem[0+:2]", nullptr);
        if (vh) {
            printf("%%Error: vpi_handle_by_name(\"t.mem[0+:2]\") returned !NULL\n");
            return __LINE__;
        }
        printf("  mem[0+:2] = NULL (unpacked +: rejected) OK\n");
    }

    return 0;
}

//======================================================================
// Test 9: Identifiers in index expressions
// Uses parameters WIDTH=32, BYTE=8, MEM_DEPTH=4, HI_BYTE=31, LO_BYTE=24
// sig_desc = 32'hDEAD_BEEF
static int test_identifier_exprs() {
    printf("== test_identifier_exprs ==\n");

    // --- Identifier in bit-range bounds ---

    // sig_desc[HI_BYTE:LO_BYTE] should be equivalent to [31:24] -> 0xDE
    {
        TestVpiHandle vh = vpi_handle_by_name((PLI_BYTE8*)"t.sig_desc[HI_BYTE:LO_BYTE]", nullptr);
        if (!vh) {
            printf("%%Error: vpi_handle_by_name(\"t.sig_desc[HI_BYTE:LO_BYTE]\") returned "
                   "NULL\n");
            return __LINE__;
        }
        int size = vpi_get(vpiSize, vh);
        if (size != 8) {
            printf("%%Error: vpiSize = %d, expected 8\n", size);
            return __LINE__;
        }
        int val = vpi_get_int(vh);
        if (val != 0xDE) {
            printf("%%Error: value = 0x%02x, expected 0xDE\n", val);
            return __LINE__;
        }
        printf("  sig_desc[HI_BYTE:LO_BYTE] size=%d value=0x%02x OK\n", size, val);
    }

    // sig_desc[BYTE-1:0] should be equivalent to [7:0] -> 0xEF
    {
        TestVpiHandle vh = vpi_handle_by_name((PLI_BYTE8*)"t.sig_desc[BYTE-1:0]", nullptr);
        if (!vh) {
            printf("%%Error: vpi_handle_by_name(\"t.sig_desc[BYTE-1:0]\") returned NULL\n");
            return __LINE__;
        }
        int size = vpi_get(vpiSize, vh);
        if (size != 8) {
            printf("%%Error: vpiSize = %d, expected 8\n", size);
            return __LINE__;
        }
        int val = vpi_get_int(vh);
        if (val != 0xEF) {
            printf("%%Error: value = 0x%02x, expected 0xEF\n", val);
            return __LINE__;
        }
        printf("  sig_desc[BYTE-1:0] size=%d value=0x%02x OK\n", size, val);
    }

    // sig_desc[WIDTH-1:WIDTH-BYTE] should be equivalent to [31:24] -> 0xDE
    {
        TestVpiHandle vh
            = vpi_handle_by_name((PLI_BYTE8*)"t.sig_desc[WIDTH-1:WIDTH-BYTE]", nullptr);
        if (!vh) {
            printf("%%Error: vpi_handle_by_name(\"t.sig_desc[WIDTH-1:WIDTH-BYTE]\") returned "
                   "NULL\n");
            return __LINE__;
        }
        int val = vpi_get_int(vh);
        if (val != 0xDE) {
            printf("%%Error: value = 0x%02x, expected 0xDE\n", val);
            return __LINE__;
        }
        printf("  sig_desc[WIDTH-1:WIDTH-BYTE] value=0x%02x OK\n", val);
    }

    // --- Identifier in array index ---

    // mem[MEM_DEPTH-1] should be same as mem[3] = 0xDDDD3333
    {
        TestVpiHandle vh = vpi_handle_by_name((PLI_BYTE8*)"t.mem[MEM_DEPTH-1]", nullptr);
        if (!vh) {
            printf("%%Error: vpi_handle_by_name(\"t.mem[MEM_DEPTH-1]\") returned NULL\n");
            return __LINE__;
        }
        int val = vpi_get_int(vh);
        if (val != (int)0xDDDD3333) {
            printf("%%Error: value = 0x%08x, expected 0xDDDD3333\n", val);
            return __LINE__;
        }
        printf("  mem[MEM_DEPTH-1] value=0x%08x OK\n", val);
    }

    // --- Combined: identifier with arithmetic in both index and bit-range ---

    // mem[MEM_DEPTH-2][BYTE*2-1:BYTE] should be mem[2][15:8] = 0x22
    {
        TestVpiHandle vh
            = vpi_handle_by_name((PLI_BYTE8*)"t.mem[MEM_DEPTH-2][BYTE*2-1:BYTE]", nullptr);
        if (!vh) {
            printf("%%Error: vpi_handle_by_name(\"t.mem[MEM_DEPTH-2][BYTE*2-1:BYTE]\") "
                   "returned NULL\n");
            return __LINE__;
        }
        int size = vpi_get(vpiSize, vh);
        if (size != 8) {
            printf("%%Error: vpiSize = %d, expected 8\n", size);
            return __LINE__;
        }
        int val = vpi_get_int(vh);
        if (val != 0x22) {
            printf("%%Error: value = 0x%02x, expected 0x22\n", val);
            return __LINE__;
        }
        printf("  mem[MEM_DEPTH-2][BYTE*2-1:BYTE] size=%d value=0x%02x OK\n", size, val);
    }

    // --- Unknown identifier should fail gracefully ---
    {
        TestVpiHandle vh = vpi_handle_by_name((PLI_BYTE8*)"t.sig_desc[NONEXISTENT-1:0]", nullptr);
        if (vh) {
            printf("%%Error: vpi_handle_by_name(\"t.sig_desc[NONEXISTENT-1:0]\") returned "
                   "!NULL\n");
            return __LINE__;
        }
        printf("  sig_desc[NONEXISTENT-1:0] = NULL (unknown ident rejected) OK\n");
    }

    return 0;
}

//======================================================================
// Test 7: Part-select on unpacked dimension should fail
// (bit-range on unpacked dim makes no sense)
static int test_partsel_on_unpacked() {
    printf("== test_partsel_on_unpacked ==\n");

    // mem is [0:3], so mem[1:2] looks like an unpacked slice, not a bit-range
    {
        TestVpiHandle vh = vpi_handle_by_name((PLI_BYTE8*)"t.mem[1:2]", nullptr);
        if (vh) {
            printf("%%Error: vpi_handle_by_name(\"t.mem[1:2]\") returned !NULL\n");
            printf("  (unpacked slice should not be supported as bit-range)\n");
            return __LINE__;
        }
        printf("  mem[1:2] = NULL (unpacked slice rejected) OK\n");
    }

    return 0;
}

//======================================================================
extern "C" int mon_check() {
    if (int status = test_partsel_descending()) return status;
    if (int status = test_partsel_ascending()) return status;
    if (int status = test_partsel_with_array()) return status;
    if (int status = test_partsel_out_of_range()) return status;
    if (int status = test_arithmetic_exprs()) return status;
    if (int status = test_identifier_exprs()) return status;
    if (int status = test_partsel_write()) return status;
    if (int status = test_partsel_array_write()) return status;
    if (int status = test_partsel_on_unpacked()) return status;
    return 0;
}

//======================================================================

#ifdef IS_VPI

static int mon_check_vpi() {
    TestVpiHandle href = vpi_handle(vpiSysTfCall, 0);
    s_vpi_value vpi_value;

    vpi_value.format = vpiIntVal;
    vpi_value.value.integer = mon_check();
    vpi_put_value(href, &vpi_value, NULL, vpiNoDelay);

    return 0;
}

static s_vpi_systf_data vpi_systf_data[] = {{vpiSysFunc, vpiIntFunc, (PLI_BYTE8*)"$mon_check",
                                             (PLI_INT32(*)(PLI_BYTE8*))mon_check_vpi, 0, 0, 0},
                                            0};

// cver entry
void vpi_compat_bootstrap(void) {
    p_vpi_systf_data systf_data_p;
    systf_data_p = &(vpi_systf_data[0]);
    while (systf_data_p->type != 0) vpi_register_systf(systf_data_p++);
}

// icarus entry
void (*vlog_startup_routines[])() = {vpi_compat_bootstrap, 0};

#else

int main(int argc, char** argv) {
    const std::unique_ptr<VerilatedContext> contextp{new VerilatedContext};

    uint64_t sim_time = 1100;
    contextp->debug(0);
    contextp->commandArgs(argc, argv);

    const std::unique_ptr<VM_PREFIX> topp{new VM_PREFIX{contextp.get(),
                                                        // Note null name - we're flattening it out
                                                        ""}};

#ifdef VERILATOR
#ifdef TEST_VERBOSE
    contextp->scopesDump();
#endif
#endif

    topp->eval();
    topp->clk = 0;
    contextp->timeInc(10);

    while (contextp->time() < sim_time && !contextp->gotFinish()) {
        contextp->timeInc(1);
        topp->eval();
        VerilatedVpi::callValueCbs();
        topp->clk = !topp->clk;
    }

    if (!contextp->gotFinish()) {
        vl_fatal(__FILE__, __LINE__, "main", "%Error: Timeout; never got a $finish");
    }
    topp->final();

    return 0;
}

#endif
