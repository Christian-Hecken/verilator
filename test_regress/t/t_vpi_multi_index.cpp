// -*- mode: C++; c-file-style: "cc-mode" -*-
//*************************************************************************
//
// This file ONLY is placed under the Creative Commons Public Domain.
// SPDX-FileCopyrightText: 2025 Wilson Snyder
// SPDX-License-Identifier: CC0-1.0
//
//*************************************************************************
//
// Minimal test for vpi_handle_by_name with unpacked array slice syntax.
// Tests whether vpi_handle_by_name("mem[1:2][3]", ...) works, where
// [1:2] is a slice on the first unpacked dimension and [3] is a scalar
// index on the second. Cross-check against other simulators.

#ifdef IS_VPI

#include "vpi_user.h"

#include <cstdlib>

#else

#include "verilated.h"
#include "verilated_vpi.h"

#include "Vt_vpi_multi_index.h"
#include "Vt_vpi_multi_index__Dpi.h"
#include "svdpi.h"

#endif

#include <cstdio>
#include <cstring>

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

//======================================================================
// Test vpi_handle_by_name with unpacked array slice in the name string.
//
// Given: logic [31:0] mem2d [1:2][3:4]
// Question: Does vpi_handle_by_name("t.mem2d[1:2][3]") return a handle?
// If so, what kind of handle is it? (array slice? single element?)
//
// This is the VPI analog of the SystemVerilog expression:
//   my_mem[1:2][3]  -- slice on first dim, scalar index on second

static int test_slice_by_name() {
    // Test 1: Can we get a handle to a slice "mem2d[1:2][3]"?
    // This asks: do simulators support unpacked array slice notation
    // in vpi_handle_by_name?
    {
        TestVpiHandle h = vpi_handle_by_name((PLI_BYTE8*)"t.mem2d[1:2][3]", NULL);
        if (!h) {
            printf("%%Warning: vpi_handle_by_name(\"t.mem2d[1:2][3]\") returned NULL\n");
            printf("  This simulator does not support slice syntax in vpi_handle_by_name\n");
            // Not necessarily an error - we want to know if this is supported
            // Return success so we can observe the behavior
        } else {
            printf("vpi_handle_by_name(\"t.mem2d[1:2][3]\") returned a handle\n");
            int vpitype = vpi_get(vpiType, h);
            int vpisize = vpi_get(vpiSize, h);
            printf("  type=%d size=%d\n", vpitype, vpisize);

            // Try to read the value
            s_vpi_value val;
            val.format = vpiIntVal;
            vpi_get_value(h, &val);
            printf("  value=0x%08x\n", val.value.integer);
        }
    }

    // Test 2: Simpler slice "mem2d[1:2]" - slice on first dim only
    {
        TestVpiHandle h = vpi_handle_by_name((PLI_BYTE8*)"t.mem2d[1:2]", NULL);
        if (!h) {
            printf("%%Warning: vpi_handle_by_name(\"t.mem2d[1:2]\") returned NULL\n");
            printf("  This simulator does not support slice syntax in vpi_handle_by_name\n");
        } else {
            printf("vpi_handle_by_name(\"t.mem2d[1:2]\") returned a handle\n");
            int vpitype = vpi_get(vpiType, h);
            int vpisize = vpi_get(vpiSize, h);
            printf("  type=%d size=%d\n", vpitype, vpisize);
        }
    }

    // Test 3: For reference, single element "mem2d[1][3]" should work
    {
        TestVpiHandle h = vpi_handle_by_name((PLI_BYTE8*)"t.mem2d[1][3]", NULL);
        if (!h) {
            printf("%%Error: vpi_handle_by_name(\"t.mem2d[1][3]\") returned NULL\n");
            return __LINE__;
        }
        int val = vpi_get_int(h);
        printf("vpi_handle_by_name(\"t.mem2d[1][3]\") = 0x%08x (expected 0xAAAA1111)\n", val);
        if (val != (int)0xAAAA1111) {
            printf("%%Error: mem2d[1][3] = 0x%08x, expected 0xAAAA1111\n", val);
            return __LINE__;
        }
    }

    return 0;
}

//======================================================================
extern "C" int mon_check() {
    if (int status = test_slice_by_name()) return status;
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
        vl_fatal(FILENM, __LINE__, "main", "%Error: Timeout; never got a $finish");
    }
    topp->final();

    return 0;
}

#endif
