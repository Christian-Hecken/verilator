// -*- mode: C++; c-file-style: "cc-mode" -*-
//*************************************************************************
// SPDX-License-Identifier: CC0-1.0
// This file ONLY is placed under the Creative Commons Public Domain, for
// any use, without warranty, 2024 by Wilson Snyder.
//*************************************************************************

#ifdef IS_VPI
#include "vpi_user.h"
#include "sv_vpi_user.h"
#else
#include "Vt_alias_public__Dpi.h"
#include "verilated.h"
#include "verilated_vpi.h"
#endif

#include <cstdio>
#include <cstdlib>

// These require the above. Comment prevents clang-format moving them
#include "TestCheck.h"
#include "TestSimulator.h"
#include "TestVpi.h"

//======================================================================

int vpi_check_aliases() {
    // Check basic aliases
    TestVpiHandle alias0_h = VPI_HANDLE("alias0");
    CHECK_RESULT_NZ(alias0_h);
    
    TestVpiHandle driver0_h = VPI_HANDLE("driver0");
    CHECK_RESULT_NZ(driver0_h);
    
    TestVpiHandle alias0_n_h = VPI_HANDLE("alias0_n");
    CHECK_RESULT_NZ(alias0_n_h);
    
    s_vpi_value v;
    v.format = vpiIntVal;
    
    vpi_get_value(alias0_h, &v);
    int alias0_val = v.value.integer;
    
    vpi_get_value(driver0_h, &v);
    int driver0_val = v.value.integer;
    
    vpi_get_value(alias0_n_h, &v);
    int alias0_n_val = v.value.integer;
    
    CHECK_RESULT(alias0_val, driver0_val);
    CHECK_RESULT(alias0_n_val, !alias0_val);
    
    // Check port aliases
    TestVpiHandle port_in_a_h = VPI_HANDLE("port_in_a");
    CHECK_RESULT_NZ(port_in_a_h);
    
    TestVpiHandle port_in_b_h = VPI_HANDLE("port_in_b");
    CHECK_RESULT_NZ(port_in_b_h);
    
    TestVpiHandle port_driver_a_h = VPI_HANDLE("port_driver_a");
    CHECK_RESULT_NZ(port_driver_a_h);
    
    TestVpiHandle port_driver_b_h = VPI_HANDLE("port_driver_b");
    CHECK_RESULT_NZ(port_driver_b_h);
    
    TestVpiHandle port_out_x_h = VPI_HANDLE("port_out_x");
    CHECK_RESULT_NZ(port_out_x_h);
    
    TestVpiHandle port_out_y_h = VPI_HANDLE("port_out_y");
    CHECK_RESULT_NZ(port_out_y_h);
    
    // Check that port_in_a matches port_driver_a (alias)
    vpi_get_value(port_in_a_h, &v);
    int port_in_a_val = v.value.integer;
    
    vpi_get_value(port_driver_a_h, &v);
    int port_driver_a_val = v.value.integer;
    
    CHECK_RESULT(port_in_a_val, port_driver_a_val);
    
    // Check that port_in_b matches port_driver_b (alias)
    vpi_get_value(port_in_b_h, &v);
    int port_in_b_val = v.value.integer;
    
    vpi_get_value(port_driver_b_h, &v);
    int port_driver_b_val = v.value.integer;
    
    CHECK_RESULT(port_in_b_val, port_driver_b_val);
    
    // Check outputs match expected logic (0 & 0 = 0, 0 | 0 = 0)
    vpi_get_value(port_out_x_h, &v);
    CHECK_RESULT(v.value.integer, 0);
    
    vpi_get_value(port_out_y_h, &v);
    CHECK_RESULT(v.value.integer, 0);
    
    return 0;
}

int vpi_modify_driver() {
    // Modify basic driver
    TestVpiHandle driver0_h = VPI_HANDLE("driver0");
    CHECK_RESULT_NZ(driver0_h);
    
    s_vpi_value v;
    v.format = vpiIntVal;
    v.value.integer = 1;
    vpi_put_value(driver0_h, &v, NULL, vpiNoDelay);
    
    // Modify port drivers
    TestVpiHandle port_driver_a_h = VPI_HANDLE("port_driver_a");
    CHECK_RESULT_NZ(port_driver_a_h);
    
    TestVpiHandle port_driver_b_h = VPI_HANDLE("port_driver_b");
    CHECK_RESULT_NZ(port_driver_b_h);
    
    // Set both port inputs to 1
    v.value.integer = 1;
    vpi_put_value(port_driver_a_h, &v, NULL, vpiNoDelay);
    vpi_put_value(port_driver_b_h, &v, NULL, vpiNoDelay);
    
    return 0;
}

int vpi_check_modified() {
    s_vpi_value v;
    v.format = vpiIntVal;
    
    // Check basic alias
    TestVpiHandle alias0_h = VPI_HANDLE("alias0");
    CHECK_RESULT_NZ(alias0_h);
    
    TestVpiHandle alias0_n_h = VPI_HANDLE("alias0_n");
    CHECK_RESULT_NZ(alias0_n_h);
    
    vpi_get_value(alias0_h, &v);
    CHECK_RESULT(v.value.integer, 1);
    
    vpi_get_value(alias0_n_h, &v);
    CHECK_RESULT(v.value.integer, 0);
    
    // Check port aliases
    TestVpiHandle port_in_a_h = VPI_HANDLE("port_in_a");
    CHECK_RESULT_NZ(port_in_a_h);
    
    TestVpiHandle port_in_b_h = VPI_HANDLE("port_in_b");
    CHECK_RESULT_NZ(port_in_b_h);
    
    TestVpiHandle port_out_x_h = VPI_HANDLE("port_out_x");
    CHECK_RESULT_NZ(port_out_x_h);
    
    TestVpiHandle port_out_y_h = VPI_HANDLE("port_out_y");
    CHECK_RESULT_NZ(port_out_y_h);
    
    // Check that inputs are now 1 (via aliases)
    vpi_get_value(port_in_a_h, &v);
    CHECK_RESULT(v.value.integer, 1);
    
    vpi_get_value(port_in_b_h, &v);
    CHECK_RESULT(v.value.integer, 1);
    
    // Check outputs match expected logic (1 & 1 = 1, 1 | 1 = 1)
    vpi_get_value(port_out_x_h, &v);
    CHECK_RESULT(v.value.integer, 1);
    
    vpi_get_value(port_out_y_h, &v);
    CHECK_RESULT(v.value.integer, 1);
    
    return 0;
}