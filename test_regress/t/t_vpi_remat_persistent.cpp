// -*- mode: C++; c-file-style: "cc-mode" -*-
//
// DESCRIPTION: Verilator: VPI rematerialized public signal persistent handle test
//
// This file ONLY is placed under the Creative Commons Public Domain, for
// any use, without warranty, 2024 by Wilson Snyder.
// SPDX-License-Identifier: CC0-1.0

#include "verilated.h"
#include "verilated_vpi.h"

static vpiHandle s_remat_h = nullptr;

extern "C" int cache_remat_handle() {
    if (s_remat_h) {
        VL_PRINTF("%%Error: remat handle was cached more than once\n");
        return __LINE__;
    }

    s_remat_h = vpi_handle_by_name(const_cast<PLI_BYTE8*>("t.remat"), nullptr);

    if (!s_remat_h) {
        VL_PRINTF("%%Error: failed to find t.remat\n");
        return __LINE__;
    }

    return 0;
}

extern "C" int check_remat_value(int expected) {
    if (!s_remat_h) {
        VL_PRINTF("%%Error: remat handle was not cached\n");
        return __LINE__;
    }

    s_vpi_value actual_v;
    actual_v.format = vpiIntVal;
    vpi_get_value(s_remat_h, &actual_v);

    if (actual_v.value.integer != expected) {
        VL_PRINTF("%%Error: remat value mismatch: expected %d, got %d\n",
                  expected, actual_v.value.integer);
        return __LINE__;
    }

    return 0;
}