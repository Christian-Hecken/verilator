// -*- mode: C++; c-file-style: "cc-mode" -*-
//*************************************************************************
//
// This program is free software; you can redistribute it and/or modify it
// under the terms of either the GNU Lesser General Public License Version 3
// or the Perl Artistic License Version 2.0.
// SPDX-FileCopyrightText: 2010-2011 Wilson Snyder
// SPDX-License-Identifier: LGPL-3.0-only OR Artistic-2.0
//
//*************************************************************************

#ifdef IS_VPI

#include "sv_vpi_user.h"

#else

#include "verilated.h"
#include "verilated_vcd_c.h"
#include "verilated_vpi.h"

#ifdef T_VPI_VAR2
#include "Vt_vpi_var2.h"
#include "Vt_vpi_var2__Dpi.h"
#elif defined(T_VPI_VAR3)
#include "Vt_vpi_var3.h"
#include "Vt_vpi_var3__Dpi.h"
#else
#include "Vt_vpi_var.h"
#include "Vt_vpi_var__Dpi.h"
#endif

#include "svdpi.h"

#endif

#ifdef VERILATOR
#include "verilated.h"
#endif

#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <iostream>

// These require the above. Comment prevents clang-format moving them
#include "TestCheck.h"
#include "TestSimulator.h"
#include "TestVpi.h"

int errors = 0;

#define TEST_MSG \
    if (0) printf

unsigned int main_time = 0;
unsigned int callback_count = 0;
unsigned int callback_count_half = 0;
unsigned int callback_count_quad = 0;
unsigned int callback_count_strs = 0;
unsigned int callback_count_strs_max = 500;

//======================================================================

// We cannot replace those with VL_STRINGIFY, not available when PLI is build
#define STRINGIFY(x) STRINGIFY2(x)
#define STRINGIFY2(x) #x

int _mon_check_mcd() {
    PLI_INT32 status;

    PLI_UINT32 mcd;
    PLI_BYTE8* filename = (PLI_BYTE8*)(STRINGIFY(TEST_OBJ_DIR) "/mcd_open.tmp");
    mcd = vpi_mcd_open(filename);
    CHECK_RESULT_NZ(mcd);

    {  // Check it got written
        FILE* fp = fopen(filename, "r");
        CHECK_RESULT_NZ(fp);
        fclose(fp);
    }

    status = vpi_mcd_printf(mcd, (PLI_BYTE8*)"hello %s", "vpi_mcd_printf");
    CHECK_RESULT(status, std::strlen("hello vpi_mcd_printf"));

    status = vpi_mcd_printf(0, (PLI_BYTE8*)"empty");
    CHECK_RESULT(status, 0);

    status = vpi_mcd_flush(mcd);
    CHECK_RESULT(status, 0);

    status = vpi_mcd_flush(0);
    CHECK_RESULT(status, 1);

    status = vpi_mcd_close(mcd);
    // Icarus says 'error' on ones we're not using, so check only used ones return 0.
    CHECK_RESULT(status & mcd, 0);

    status = vpi_flush();
    CHECK_RESULT(status, 0);

    return 0;
}

int _mon_check_callbacks_error(p_cb_data cb_data) {
    vpi_printf((PLI_BYTE8*)"%%Error: callback should not be executed\n");
    return 1;
}

int _mon_check_callbacks() {
    t_cb_data cb_data;
    cb_data.reason = cbEndOfSimulation;
    cb_data.cb_rtn = _mon_check_callbacks_error;
    cb_data.user_data = 0;
    cb_data.value = NULL;
    cb_data.time = NULL;

    TestVpiHandle vh = vpi_register_cb(&cb_data);
    CHECK_RESULT_NZ(vh);

    PLI_INT32 status = vpi_remove_cb(vh);
    vh.freed();
    CHECK_RESULT_NZ(status);

    return 0;
}

int _value_callback(p_cb_data cb_data) {
    if (verbose) vpi_printf(const_cast<char*>("     _value_callback:\n"));
    if (TestSimulator::is_verilator()) {
        // this check only makes sense in Verilator
        CHECK_RESULT(cb_data->value->value.integer + 10, main_time);
    }
    callback_count++;
    return 0;
}

int _value_callback_half(p_cb_data cb_data) {
    if (TestSimulator::is_verilator()) {
        // this check only makes sense in Verilator
        CHECK_RESULT(cb_data->value->value.integer * 2 + 10, main_time);
    }
    callback_count_half++;
    return 0;
}

int _value_callback_quad(p_cb_data cb_data) {
    for (int index = 0; index < 2; index++) {
        CHECK_RESULT_HEX(cb_data->value->value.vector[1].aval,
                         (unsigned long)((index == 2) ? 0x1c77bb9bUL : 0x12819213UL));
        CHECK_RESULT_HEX(cb_data->value->value.vector[0].aval,
                         (unsigned long)((index == 2) ? 0x3784ea09UL : 0xabd31a1cUL));
    }
    callback_count_quad++;
    return 0;
}

int _value_callback_never(p_cb_data cb_data) {
    printf("%%Error: callback should never be called\n");
    exit(-1);
    return 0;
}

int _mon_check_value_callbacks() {
    s_vpi_value v;
    v.format = vpiIntVal;

    t_cb_data cb_data;
    cb_data.reason = cbValueChange;
    cb_data.time = NULL;

    {
        TestVpiHandle vh1 = VPI_HANDLE("count");
        CHECK_RESULT_NZ(vh1);

        vpi_get_value(vh1, &v);
        cb_data.value = &v;
        cb_data.obj = vh1;
        cb_data.cb_rtn = _value_callback;

        if (verbose) vpi_printf(const_cast<char*>("     vpi_register_cb(_value_callback):\n"));
        TestVpiHandle callback_h = vpi_register_cb(&cb_data);
        CHECK_RESULT_NZ(callback_h);
    }
    {
        TestVpiHandle vh1 = VPI_HANDLE("half_count");
        CHECK_RESULT_NZ(vh1);

        cb_data.obj = vh1;
        cb_data.cb_rtn = _value_callback_half;

        TestVpiHandle callback_h = vpi_register_cb(&cb_data);
        CHECK_RESULT_NZ(callback_h);
    }
    {
        TestVpiHandle vh1 = VPI_HANDLE("quads");
        CHECK_RESULT_NZ(vh1);

        v.format = vpiVectorVal;
        cb_data.obj = vh1;
        cb_data.cb_rtn = _value_callback_quad;

        TestVpiHandle callback_h = vpi_register_cb(&cb_data);
        CHECK_RESULT_NZ(callback_h);
    }
    {
        TestVpiHandle vh1 = VPI_HANDLE("quads");
        CHECK_RESULT_NZ(vh1);
        TestVpiHandle vh2 = vpi_handle_by_index(vh1, 2);
        CHECK_RESULT_NZ(vh2);

        cb_data.obj = vh2;
        cb_data.cb_rtn = _value_callback_quad;

        TestVpiHandle callback_h = vpi_register_cb(&cb_data);
        CHECK_RESULT_NZ(callback_h);
    }
    {
        TestVpiHandle vh1 = VPI_HANDLE("some_mem");
        CHECK_RESULT_NZ(vh1);
        TestVpiHandle vh2 = vpi_handle_by_index(vh1, 3);
        CHECK_RESULT_NZ(vh2);

        cb_data.obj = vh2;
        cb_data.cb_rtn = _value_callback_never;

        TestVpiHandle callback_h = vpi_register_cb(&cb_data);
        CHECK_RESULT_NZ(callback_h);
    }
    return 0;
}

int _mon_check_too_big() {
#ifdef VERILATOR
    s_vpi_value v;
    v.format = vpiVectorVal;

    TestVpiHandle h = VPI_HANDLE("too_big");
    CHECK_RESULT_NZ(h);

    Verilated::fatalOnVpiError(false);
    vpi_get_value(h, &v);
    Verilated::fatalOnVpiError(true);
    s_vpi_error_info info;
    CHECK_RESULT_NZ(vpi_chk_error(&info));

    v.format = vpiStringVal;
    vpi_get_value(h, &v);
    CHECK_RESULT_Z(vpi_chk_error(nullptr));
    CHECK_RESULT_CSTR_STRIP(v.value.str, "some text");
#endif

    return 0;
}

int _mon_check_var() {
    TestVpiHandle vh1 = VPI_HANDLE("onebit");
    CHECK_RESULT_NZ(vh1);

    TestVpiHandle vh2 = vpi_handle_by_name((PLI_BYTE8*)TestSimulator::top(), NULL);
    CHECK_RESULT_NZ(vh2);

    // scope attributes
    const char* p;
    p = vpi_get_str(vpiName, vh2);
    CHECK_RESULT_CSTR(p, "t");
    p = vpi_get_str(vpiFullName, vh2);
    CHECK_RESULT_CSTR(p, TestSimulator::top());
    p = vpi_get_str(vpiType, vh2);
    CHECK_RESULT_CSTR(p, "vpiModule");

    TestVpiHandle vh3 = vpi_handle_by_name((PLI_BYTE8*)"onebit", vh2);
    CHECK_RESULT_NZ(vh3);

#ifdef T_VPI_VAR2
    // test scoped attributes
    TestVpiHandle vh_invisible1 = vpi_handle_by_name((PLI_BYTE8*)"invisible1", vh2);
    CHECK_RESULT_Z(vh_invisible1);

    TestVpiHandle vh_invisible2 = vpi_handle_by_name((PLI_BYTE8*)"invisible2", vh2);
    CHECK_RESULT_Z(vh_invisible2);

    TestVpiHandle vh_visibleParam1 = vpi_handle_by_name((PLI_BYTE8*)"visibleParam1", vh2);
    CHECK_RESULT_NZ(vh_visibleParam1);

    TestVpiHandle vh_invisibleParam1 = vpi_handle_by_name((PLI_BYTE8*)"invisibleParam1", vh2);
    CHECK_RESULT_Z(vh_invisibleParam1);

    TestVpiHandle vh_visibleParam2 = vpi_handle_by_name((PLI_BYTE8*)"visibleParam2", vh2);
    CHECK_RESULT_NZ(vh_visibleParam2);

#endif

    // onebit attributes
    PLI_INT32 d;
    d = vpi_get(vpiType, vh3);
    CHECK_RESULT(d, vpiReg);
    if (TestSimulator::has_get_scalar()) {
        d = vpi_get(vpiVector, vh3);
        CHECK_RESULT(d, 0);
    }

    p = vpi_get_str(vpiName, vh3);
    CHECK_RESULT_CSTR(p, "onebit");
    p = vpi_get_str(vpiFullName, vh3);
    CHECK_RESULT_CSTR(p, TestSimulator::rooted("onebit"));
    p = vpi_get_str(vpiType, vh3);
    CHECK_RESULT_CSTR(p, "vpiReg");

    // array attributes
    TestVpiHandle vh4 = VPI_HANDLE("fourthreetwoone");
    CHECK_RESULT_NZ(vh4);
    if (TestSimulator::has_get_scalar()) {
        d = vpi_get(vpiVector, vh4);
        CHECK_RESULT(d, 1);
        p = vpi_get_str(vpiType, vh4);
        CHECK_RESULT_CSTR(p, "vpiRegArray");
    }

    t_vpi_value tmpValue;
    tmpValue.format = vpiIntVal;
    {
        TestVpiHandle vh10 = vpi_handle(vpiLeftRange, vh4);
        CHECK_RESULT_NZ(vh10);
        vpi_get_value(vh10, &tmpValue);
        CHECK_RESULT(tmpValue.value.integer, 4);
        CHECK_RESULT(vpi_get(vpiType, vh10), vpiConstant);
        p = vpi_get_str(vpiType, vh10);
        CHECK_RESULT_CSTR(p, "vpiConstant");
    }
    {
        TestVpiHandle vh10 = vpi_handle(vpiRightRange, vh4);
        CHECK_RESULT_NZ(vh10);
        vpi_get_value(vh10, &tmpValue);
        CHECK_RESULT(tmpValue.value.integer, 3);
        p = vpi_get_str(vpiType, vh10);
        CHECK_RESULT_CSTR(p, "vpiConstant");
    }
    {
        TestVpiHandle vh10 = vpi_iterate(vpiReg, vh4);
        CHECK_RESULT_NZ(vh10);
        p = vpi_get_str(vpiType, vh10);
        CHECK_RESULT_CSTR(p, "vpiIterator");
        TestVpiHandle vh11 = vpi_scan(vh10);
        CHECK_RESULT_NZ(vh11);
        p = vpi_get_str(vpiType, vh11);
        CHECK_RESULT_CSTR(p, "vpiReg");
        TestVpiHandle vh12 = vpi_handle(vpiLeftRange, vh11);
        CHECK_RESULT_NZ(vh12);
        vpi_get_value(vh12, &tmpValue);
        CHECK_RESULT(tmpValue.value.integer, 2);
        p = vpi_get_str(vpiType, vh12);
        CHECK_RESULT_CSTR(p, "vpiConstant");
        TestVpiHandle vh13 = vpi_handle(vpiRightRange, vh11);
        CHECK_RESULT_NZ(vh13);
        vpi_get_value(vh13, &tmpValue);
        CHECK_RESULT(tmpValue.value.integer, 1);
        p = vpi_get_str(vpiType, vh13);
        CHECK_RESULT_CSTR(p, "vpiConstant");
    }

    TestVpiHandle vh5 = VPI_HANDLE("quads");
    CHECK_RESULT_NZ(vh5);
    {
        TestVpiHandle vh10 = vpi_handle(vpiLeftRange, vh5);
        CHECK_RESULT_NZ(vh10);
        vpi_get_value(vh10, &tmpValue);
        CHECK_RESULT(tmpValue.value.integer, 2);
        p = vpi_get_str(vpiType, vh10);
        CHECK_RESULT_CSTR(p, "vpiConstant");
    }
    {
        TestVpiHandle vh10 = vpi_handle(vpiRightRange, vh5);
        CHECK_RESULT_NZ(vh10);
        vpi_get_value(vh10, &tmpValue);
        CHECK_RESULT(tmpValue.value.integer, 3);
        p = vpi_get_str(vpiType, vh10);
        CHECK_RESULT_CSTR(p, "vpiConstant");
    }
    TestVpiHandle vh6 = vpi_handle_by_index(vh5, 2);
    CHECK_RESULT_NZ(vh6);
    {
        TestVpiHandle vh10 = vpi_handle(vpiLeftRange, vh6);
        CHECK_RESULT_NZ(vh10);
        vpi_get_value(vh10, &tmpValue);
        CHECK_RESULT(tmpValue.value.integer, 0);
        p = vpi_get_str(vpiType, vh10);
        CHECK_RESULT_CSTR(p, "vpiConstant");
    }
    {
        TestVpiHandle vh10 = vpi_handle(vpiRightRange, vh6);
        CHECK_RESULT_NZ(vh10);
        vpi_get_value(vh10, &tmpValue);
        CHECK_RESULT(tmpValue.value.integer, 61);
        p = vpi_get_str(vpiType, vh10);
        CHECK_RESULT_CSTR(p, "vpiConstant");
    }

    // C++ keyword collision
    {
        TestVpiHandle vh10 = VPI_HANDLE("nullptr");
        CHECK_RESULT_NZ(vh10);
        vpi_get_value(vh10, &tmpValue);
        CHECK_RESULT(tmpValue.value.integer, 123);
        p = vpi_get_str(vpiType, vh10);
        CHECK_RESULT_CSTR(p, "vpiParameter");
    }

    // test properties on bad handle
    {
        TestVpiHandle vh999 = VPI_HANDLE("nonexistent");
        CHECK_RESULT_Z(vh999);
        d = vpi_get(vpiType, vh999);
        CHECK_RESULT(d, vpiUndefined);
        d = vpi_get(vpiSigned, vh999);
        CHECK_RESULT(d, vpiUndefined);
        d = vpi_get(vpiSize, vh999);
        CHECK_RESULT(d, vpiUndefined);
    }

    // other integer types
    tmpValue.format = vpiIntVal;
    constexpr struct {
        const char* name;
        PLI_INT32 exp_sz;
    } int_vars[] = {
        {"integer1", 32}, {"byte1", 8}, {"short1", 16}, {"int1", 32}, {"long1", 64},
    };
    for (const auto& s : int_vars) {
        TestVpiHandle vh101 = VPI_HANDLE(s.name);
        CHECK_RESULT_NZ(vh101);
        d = vpi_get(vpiType, vh101);
        CHECK_RESULT(d, vpiReg);
        auto sz = vpi_get(vpiSize, vh101);
        CHECK_RESULT(sz, s.exp_sz);
        auto sn = vpi_get(vpiSigned, vh101);
        CHECK_RESULT(sn, 1);
        vpi_get_value(vh101, &tmpValue);
        TEST_CHECK_EQ(tmpValue.value.integer, 123);
        p = vpi_get_str(vpiType, vh101);
        CHECK_RESULT_CSTR(p, "vpiReg");
    }

    // non-integer variables
    tmpValue.format = vpiRealVal;
    {
        TestVpiHandle vh101 = VPI_HANDLE("real1");
        CHECK_RESULT_NZ(vh101);
        d = vpi_get(vpiType, vh101);
        CHECK_RESULT(d, vpiRealVar);
        auto sn = vpi_get(vpiSigned, vh101);
        CHECK_RESULT(sn, 1);
        vpi_get_value(vh101, &tmpValue);
        TEST_CHECK_REAL_EQ(tmpValue.value.real, 1.0, 0.0005);
        p = vpi_get_str(vpiType, vh101);
        CHECK_RESULT_CSTR(p, "vpiRealVar");
    }

    // string variable
    tmpValue.format = vpiStringVal;
    {
        TestVpiHandle vh101 = VPI_HANDLE("str1");
        CHECK_RESULT_NZ(vh101);
        d = vpi_get(vpiType, vh101);
        CHECK_RESULT(d, vpiStringVar);
        auto sn = vpi_get(vpiSigned, vh101);
        CHECK_RESULT(sn, 0);
        vpi_get_value(vh101, &tmpValue);
        CHECK_RESULT_CSTR(tmpValue.value.str, "hello");
        p = vpi_get_str(vpiType, vh101);
        CHECK_RESULT_CSTR(p, "vpiStringVar");
    }

    return errors;
}

int _mon_check_rev() {
    t_vpi_value value;
    TestVpiHandle vh9 = VPI_HANDLE("rev");
    CHECK_RESULT_NZ(vh9);
    value.format = vpiIntVal;
    {
        TestVpiHandle vh10 = vpi_handle(vpiLeftRange, vh9);
        CHECK_RESULT_NZ(vh10);
        vpi_get_value(vh10, &value);
        TEST_CHECK_EQ(value.value.integer, 8);
        TestVpiHandle vh11 = vpi_handle(vpiRightRange, vh9);
        CHECK_RESULT_NZ(vh11);
        vpi_get_value(vh11, &value);
        TEST_CHECK_EQ(value.value.integer, 19);

        value.format = vpiVectorVal;
        vpi_get_value(vh9, &value);
        CHECK_RESULT(value.value.vector[0].aval, 0xabc);
    }
    return errors;
}

int _mon_check_varlist() {
    const char* p;

    TestVpiHandle vh2 = VPI_HANDLE("sub");
    CHECK_RESULT_NZ(vh2);
    p = vpi_get_str(vpiName, vh2);
    CHECK_RESULT_CSTR(p, "sub");
    if (TestSimulator::is_verilator()) {
        p = vpi_get_str(vpiDefName, vh2);
        CHECK_RESULT_CSTR(p, "sub");
    }

    TestVpiHandle vh10 = vpi_iterate(vpiReg, vh2);
    CHECK_RESULT_NZ(vh10);
    CHECK_RESULT(vpi_get(vpiType, vh10), vpiIterator);

    {
        TestVpiHandle vh11 = vpi_scan(vh10);
        CHECK_RESULT_NZ(vh11);
        p = vpi_get_str(vpiFullName, vh11);
        CHECK_RESULT_CSTR(p, TestSimulator::rooted("sub.subsig1"));
    }
    {
        TestVpiHandle vh12 = vpi_scan(vh10);
        CHECK_RESULT_NZ(vh12);
        p = vpi_get_str(vpiFullName, vh12);
        CHECK_RESULT_CSTR(p, TestSimulator::rooted("sub.subsig2"));
    }
    {
        TestVpiHandle vh13 = vpi_scan(vh10);
        vh10.freed();  // IEEE 37.2.2 vpi_scan at end does a vpi_release_handle
        CHECK_RESULT(vh13, 0);
    }
    return 0;
}

void touch_signal() {
    TestVpiHandle vh1 = VPI_HANDLE("count");
    TEST_CHECK_NZ(vh1);
    s_vpi_value v;
    v.format = vpiIntVal;
    s_vpi_time t;
    t.type = vpiSimTime;
    t.high = 0;
    t.low = 0;
    v.value.integer = 0;
    vpi_put_value(vh1, &v, &t, vpiNoDelay);
}

int _mon_check_ports() {
#ifdef TEST_VERBOSE
    printf("-mon_check_ports()\n");
#endif
    // test writing to input port
    TestVpiHandle vh1 = VPI_HANDLE("a");
    TEST_CHECK_NZ(vh1);

    PLI_INT32 d;
    d = vpi_get(vpiType, vh1);
    if (TestSimulator::is_verilator()) {
        TEST_CHECK_EQ(d, vpiReg);
    } else {
        TEST_CHECK_EQ(d, vpiNet);
    }

    const char* portFullName;
    if (TestSimulator::is_verilator()) {
        portFullName = "TOP.a";
    } else {
        portFullName = "t.a";
    }

    const char* name = vpi_get_str(vpiFullName, vh1);
    TEST_CHECK_EQ(strcmp(name, portFullName), 0);
    std::string handleName1 = name;

    s_vpi_value v;
    v.format = vpiIntVal;
    vpi_get_value(vh1, &v);
    TEST_CHECK_EQ(v.value.integer, 0);

    s_vpi_time t;
    t.type = vpiSimTime;
    t.high = 0;
    t.low = 0;
    v.value.integer = 2;
    vpi_put_value(vh1, &v, &t, vpiNoDelay);
    v.value.integer = 100;
    vpi_get_value(vh1, &v);
    TEST_CHECK_EQ(v.value.integer, 2);

    // get handle of toplevel module
    TestVpiHandle vht = VPI_HANDLE("");
    TEST_CHECK_NZ(vht);

    d = vpi_get(vpiType, vht);
    TEST_CHECK_EQ(d, vpiModule);

    TestVpiHandle vhi = vpi_iterate(vpiReg, vht);
    TEST_CHECK_NZ(vhi);

    TestVpiHandle vh11;
    std::string handleName2;
    while ((vh11 = vpi_scan(vhi))) {
        const char* fn = vpi_get_str(vpiFullName, vh11);
#ifdef TEST_VERBOSE
        printf("       scanned %s\n", fn);
#endif
        if (0 == strcmp(fn, portFullName)) {
            handleName2 = fn;
            break;
        }
    }
    TEST_CHECK_NZ(vh11);  // If get zero we never found the variable
    vhi.release();
    TEST_CHECK_EQ(vpi_get(vpiType, vh11), vpiReg);

    TEST_CHECK_EQ(handleName1, handleName2);

    return errors;
}

int _mon_check_getput() {
    TestVpiHandle vh2 = VPI_HANDLE("onebit");
    CHECK_RESULT_NZ(vh2);
    const char* p = vpi_get_str(vpiFullName, vh2);
    CHECK_RESULT_CSTR(p, "t.onebit");

    s_vpi_value v;
    v.format = vpiIntVal;
    vpi_get_value(vh2, &v);
    CHECK_RESULT(v.value.integer, 0);

    s_vpi_time t;
    t.type = vpiSimTime;
    t.high = 0;
    t.low = 0;
    v.value.integer = 0;
    vpi_put_value(vh2, &v, &t, vpiNoDelay);
    vpi_get_value(vh2, &v);
    CHECK_RESULT(v.value.integer, 0);

    v.value.integer = 1;
    vpi_put_value(vh2, &v, &t, vpiNoDelay);
    vpi_get_value(vh2, &v);
    CHECK_RESULT(v.value.integer, 1);

    // real
    TestVpiHandle vh3 = VPI_HANDLE("real1");
    CHECK_RESULT_NZ(vh3);
    v.format = vpiRealVal;
    vpi_get_value(vh3, &v);
    TEST_CHECK_REAL_EQ(v.value.real, 1.0, 0.0005);

    v.value.real = 123456.789;
    vpi_put_value(vh3, &v, &t, vpiNoDelay);
    v.value.real = 0.0f;
    vpi_get_value(vh3, &v);
    TEST_CHECK_REAL_EQ(v.value.real, 123456.789, 0.0005);

    // string
    TestVpiHandle vh4 = VPI_HANDLE("str1");
    CHECK_RESULT_NZ(vh4);
    v.format = vpiStringVal;
    vpi_get_value(vh4, &v);
    CHECK_RESULT_CSTR(v.value.str, "hello");

    v.value.str = const_cast<char*>("something a lot longer than hello");
    vpi_put_value(vh4, &v, &t, vpiNoDelay);
    v.value.str = 0;
    vpi_get_value(vh4, &v);
    TEST_CHECK_CSTR(v.value.str, "something a lot longer than hello");

    return errors;
}

int _mon_check_var_long_name() {
    TestVpiHandle vh2 = VPI_HANDLE(
        "LONGSTART_a_very_long_name_which_will_get_hashed_a_very_long_name_which_will_get_hashed_"
        "a_very_long_name_which_will_get_hashed_a_very_long_name_which_will_get_hashed_LONGEND");
    CHECK_RESULT_NZ(vh2);
    const char* p = vpi_get_str(vpiFullName, vh2);
    CHECK_RESULT_CSTR(p, "t.LONGSTART_a_very_long_name_which_will_get_hashed_a_very_long_name_"
                         "which_will_get_hashed_a_very_long_name_which_will_get_hashed_a_very_"
                         "long_name_which_will_get_hashed_LONGEND");
    return 0;
}

int _mon_check_getput_iter() {
    TestVpiHandle vh2 = VPI_HANDLE("sub");
    CHECK_RESULT_NZ(vh2);
    TestVpiHandle vh10 = vpi_iterate(vpiReg, vh2);
    CHECK_RESULT_NZ(vh10);
    CHECK_RESULT(vpi_get(vpiType, vh10), vpiIterator);

    TestVpiHandle vh11;
    while (1) {
        vh11 = vpi_scan(vh10);
        CHECK_RESULT_NZ(vh11);  // If get zero we never found the variable
        const char* p = vpi_get_str(vpiFullName, vh11);
#ifdef TEST_VERBOSE
        printf("       scanned %s\n", p);
#endif
        if (0 == strcmp(p, "t.sub.subsig1")) break;
    }
    CHECK_RESULT(vpi_get(vpiType, vh11), vpiReg);

    s_vpi_time t;
    t.type = vpiSimTime;
    t.high = 0;
    t.low = 0;
    s_vpi_value v;
    v.format = vpiIntVal;
    v.value.integer = 0;
    vpi_put_value(vh11, &v, &t, vpiNoDelay);
    vpi_get_value(vh11, &v);
    CHECK_RESULT(v.value.integer, 0);

    v.value.integer = 1;
    vpi_put_value(vh11, &v, &t, vpiNoDelay);
    vpi_get_value(vh11, &v);
    CHECK_RESULT(v.value.integer, 1);
    return 0;
}

int _mon_check_quad() {
    TestVpiHandle vh2 = VPI_HANDLE("quads");
    CHECK_RESULT_NZ(vh2);

    s_vpi_value v;
    t_vpi_vecval vv[2];
    bzero(&vv, sizeof(vv));

    s_vpi_time t;
    t.type = vpiSimTime;
    t.high = 0;
    t.low = 0;

    TestVpiHandle vhidx2 = vpi_handle_by_index(vh2, 2);
    CHECK_RESULT_NZ(vhidx2);
    TestVpiHandle vhidx3 = vpi_handle_by_index(vh2, 3);
    CHECK_RESULT_NZ(vhidx3);

    // Packed words should be indexable
    TestVpiHandle vhidx3idx0 = vpi_handle_by_index(vhidx3, 0);
    CHECK_RESULT_NZ(vhidx3idx0);
    TestVpiHandle vhidx2idx2 = vpi_handle_by_index(vhidx2, 2);
    CHECK_RESULT_NZ(vhidx2idx2);
    TestVpiHandle vhidx3idx3 = vpi_handle_by_index(vhidx3, 3);
    CHECK_RESULT_NZ(vhidx3idx3);
    TestVpiHandle vhidx2idx61 = vpi_handle_by_index(vhidx2, 61);
    CHECK_RESULT_NZ(vhidx2idx61);

    v.format = vpiVectorVal;
    v.value.vector = vv;
    v.value.vector[1].aval = 0x12819213UL;
    v.value.vector[0].aval = 0xabd31a1cUL;
    vpi_put_value(vhidx2, &v, &t, vpiNoDelay);

    v.format = vpiVectorVal;
    v.value.vector = vv;
    v.value.vector[1].aval = 0x1c77bb9bUL;
    v.value.vector[0].aval = 0x3784ea09UL;
    vpi_put_value(vhidx3, &v, &t, vpiNoDelay);

    vpi_get_value(vhidx2, &v);
    CHECK_RESULT(v.value.vector[1].aval, 0x12819213UL);
    CHECK_RESULT(v.value.vector[1].bval, 0);

    vpi_get_value(vhidx3, &v);
    CHECK_RESULT(v.value.vector[1].aval, 0x1c77bb9bUL);
    CHECK_RESULT(v.value.vector[1].bval, 0);

    return 0;
}

int _mon_check_delayed() {
    TestVpiHandle vh = VPI_HANDLE("delayed");
    CHECK_RESULT_NZ(vh);

    s_vpi_time t;
    t.type = vpiSimTime;
    t.high = 0;
    t.low = 0;

    s_vpi_value v;
    v.format = vpiIntVal;
    v.value.integer = 123;
    vpi_put_value(vh, &v, &t, vpiInertialDelay);
    CHECK_RESULT_Z(vpi_chk_error(nullptr));
    vpi_get_value(vh, &v);
    CHECK_RESULT(v.value.integer, 0);

    TestVpiHandle vhMem = VPI_HANDLE("delayed_mem");
    CHECK_RESULT_NZ(vhMem);
    TestVpiHandle vhMemWord = vpi_handle_by_index(vhMem, 7);
    CHECK_RESULT_NZ(vhMemWord);
    v.value.integer = 456;
    vpi_put_value(vhMemWord, &v, &t, vpiInertialDelay);
    CHECK_RESULT_Z(vpi_chk_error(nullptr));

    // test unsupported vpiInertialDelay cases
    // - should these also throw vpi errors?
    v.format = vpiStringVal;
    v.value.str = nullptr;
    vpi_put_value(vh, &v, &t, vpiInertialDelay);
    CHECK_RESULT_NZ(vpi_chk_error(nullptr));

    v.format = vpiVectorVal;
    v.value.vector = nullptr;
    vpi_put_value(vh, &v, &t, vpiInertialDelay);
    CHECK_RESULT_NZ(vpi_chk_error(nullptr));

    // This format throws an error now
#ifdef VERILATOR
    Verilated::fatalOnVpiError(false);
#endif
    v.format = vpiObjTypeVal;
    vpi_put_value(vh, &v, &t, vpiInertialDelay);
#ifdef VERILATOR
    Verilated::fatalOnVpiError(true);
#endif

    return 0;
}

int _mon_check_string() {
    static struct {
        const char* name;
        const char* initial;
        const char* value;
    } text_test_obs[] = {
        {"text_byte", "B", "xxA"},  // x's dropped
        {"text_half", "Hf", "xxT2"},  // x's dropped
        {"text_word", "Word", "Tree"},
        {"text_long", "Long64b", "44Four44"},
        {"text", "Verilog Test module", "lorem ipsum"},
    };

    for (int i = 0; i < 5; i++) {
        TestVpiHandle vh1 = VPI_HANDLE(text_test_obs[i].name);
        CHECK_RESULT_NZ(vh1);

        s_vpi_value v;
        s_vpi_time t = {vpiSimTime, 0, 0, 0.0};
        s_vpi_error_info e;

        v.format = vpiStringVal;
        vpi_get_value(vh1, &v);
        if (vpi_chk_error(&e)) printf("%%vpi_chk_error : %s\n", e.message);

        (void)vpi_chk_error(NULL);

        CHECK_RESULT_CSTR_STRIP(v.value.str, text_test_obs[i].initial);

        v.value.str = (PLI_BYTE8*)text_test_obs[i].value;
        vpi_put_value(vh1, &v, &t, vpiNoDelay);
    }

    return 0;
}

int _mon_check_putget_str(p_cb_data cb_data) {
    static TestVpiHandle cb;
    static struct {
        TestVpiHandle scope, sig, rfr, check, verbose;
        std::string str;
        int type;  // value type in .str
        union {
            PLI_INT32 integer;
            s_vpi_vecval vector[4];
        } value;  // reference
    } data[129];

    if (cb_data) {
        if (verbose) vpi_printf(const_cast<char*>("     _mon_check_putget_str callback:\n"));

        // this is the callback
        static unsigned int seed = 1;
        s_vpi_time t;
        t.type = vpiSimTime;
        t.high = 0;
        t.low = 0;
        for (int i = 2; i <= 6; i++) {
            static s_vpi_value v;
            int words = (i + 31) >> 5;
            TEST_MSG("========== %d ==========\n", i);
            if (callback_count_strs) {
                // check persistence
                if (data[i].type) {
                    v.format = data[i].type;
                } else {
                    static PLI_INT32 vals[]
                        = {vpiBinStrVal, vpiOctStrVal, vpiHexStrVal, vpiDecStrVal};
                    v.format = vals[rand_r(&seed) % ((words > 2) ? 3 : 4)];
                    TEST_MSG("new format %d\n", v.format);
                }
                vpi_get_value(data[i].sig, &v);
                TEST_MSG("%s\n", v.value.str);
                if (data[i].type) {
                    CHECK_RESULT_CSTR(v.value.str, data[i].str.c_str());
                } else {
                    data[i].type = v.format;
                    data[i].str = std::string{v.value.str};
                }
            }

            // check for corruption
            v.format = (words == 1) ? vpiIntVal : vpiVectorVal;
            vpi_get_value(data[i].sig, &v);
            if (v.format == vpiIntVal) {
                TEST_MSG("%08x %08x\n", v.value.integer, data[i].value.integer);
                CHECK_RESULT(v.value.integer, data[i].value.integer);
            } else {
                for (int k = 0; k < words; k++) {
                    TEST_MSG("%d %08x %08x\n", k, v.value.vector[k].aval,
                             data[i].value.vector[k].aval);
                    CHECK_RESULT_HEX(v.value.vector[k].aval, data[i].value.vector[k].aval);
                }
            }

            if (callback_count_strs & 7) {
                // put same value back - checking encoding/decoding equivalent
                v.format = data[i].type;
                v.value.str = (PLI_BYTE8*)(data[i].str.c_str());  // Can't reinterpret_cast
                vpi_put_value(data[i].sig, &v, &t, vpiNoDelay);
                v.format = vpiIntVal;
                v.value.integer = 1;
                // vpi_put_value(data[i].verbose, &v, &t, vpiNoDelay);
                vpi_put_value(data[i].check, &v, &t, vpiNoDelay);
            } else {
                // stick a new random value in
                unsigned int mask = ((i & 31) ? (1 << (i & 31)) : 0) - 1;
                if (words == 1) {
                    v.value.integer = rand_r(&seed);
                    data[i].value.integer = v.value.integer &= mask;
                    v.format = vpiIntVal;
                    TEST_MSG("new value %08x\n", data[i].value.integer);
                } else {
                    TEST_MSG("new value\n");
                    for (int j = 0; j < 4; j++) {
                        data[i].value.vector[j].aval = rand_r(&seed);
                        if (j == (words - 1)) data[i].value.vector[j].aval &= mask;
                        TEST_MSG(" %08x\n", data[i].value.vector[j].aval);
                    }
                    v.value.vector = data[i].value.vector;
                    v.format = vpiVectorVal;
                }
                vpi_put_value(data[i].sig, &v, &t, vpiNoDelay);
                vpi_put_value(data[i].rfr, &v, &t, vpiNoDelay);
            }
            if ((callback_count_strs & 1) == 0) data[i].type = 0;
        }
        if (++callback_count_strs == callback_count_strs_max) {
            int success = vpi_remove_cb(cb);
            cb.freed();
            CHECK_RESULT_NZ(success);
        };
    } else {
        // setup and install
        for (int i = 1; i <= 6; i++) {
            char buf[32];
            VL_SNPRINTF(buf, sizeof(buf), TestSimulator::rooted("arr[%d].arr"), i);
            CHECK_RESULT_NZ(data[i].scope = vpi_handle_by_name((PLI_BYTE8*)buf, NULL));
            CHECK_RESULT_NZ(data[i].sig = vpi_handle_by_name((PLI_BYTE8*)"sig", data[i].scope));
            CHECK_RESULT_NZ(data[i].rfr = vpi_handle_by_name((PLI_BYTE8*)"rfr", data[i].scope));
            CHECK_RESULT_NZ(data[i].check
                            = vpi_handle_by_name((PLI_BYTE8*)"check", data[i].scope));
            CHECK_RESULT_NZ(data[i].verbose
                            = vpi_handle_by_name((PLI_BYTE8*)"verbose", data[i].scope));
        }

        for (int i = 1; i <= 6; i++) {
            char buf[32];
            VL_SNPRINTF(buf, sizeof(buf), TestSimulator::rooted("subs[%d].subsub"), i);
            CHECK_RESULT_NZ(data[i].scope = vpi_handle_by_name((PLI_BYTE8*)buf, NULL));
        }

        static t_cb_data cb_data;
        static s_vpi_value v;
        TestVpiHandle count_h = VPI_HANDLE("count");

        cb_data.reason = cbValueChange;
        cb_data.cb_rtn = _mon_check_putget_str;  // this function
        cb_data.obj = count_h;
        cb_data.value = &v;
        cb_data.time = NULL;
        v.format = vpiIntVal;

        cb = vpi_register_cb(&cb_data);
        // It is legal to free the callback handle immediately if not otherwise needed
        CHECK_RESULT_NZ(cb);
    }
    return 0;
}

int _mon_check_vlog_info() {
    s_vpi_vlog_info vlog_info;
    PLI_INT32 rtn = vpi_get_vlog_info(&vlog_info);
    CHECK_RESULT(rtn, 1);
    CHECK_RESULT(vlog_info.argc, 4);
    CHECK_RESULT_CSTR(vlog_info.argv[1], "+PLUS");
    CHECK_RESULT_CSTR(vlog_info.argv[2], "+INT=1234");
    CHECK_RESULT_CSTR(vlog_info.argv[3], "+STRSTR");
    CHECK_RESULT_Z(vlog_info.argv[4]);
    if (TestSimulator::is_verilator()) {
        CHECK_RESULT_CSTR(vlog_info.product, "Verilator");
        CHECK_RESULT(std::strlen(vlog_info.version) > 0, 1);
    }
    return 0;
}

int _mon_check_multi_index() {
    // Comprehensive tests for vpi_handle_by_multi_index and vpi_handle_by_name with array indexing

    // ========== BASIC FUNCTIONALITY TESTS ==========

    // Basic 1D unpacked array access
    TestVpiHandle vh_quads = vpi_handle_by_name((PLI_BYTE8*)"t.quads", nullptr);
    CHECK_RESULT_NZ(vh_quads);

    PLI_INT32 indices_single[1] = {2};
    TestVpiHandle vh_quads_2 = vpi_handle_by_multi_index(vh_quads, 1, indices_single);
    CHECK_RESULT_NZ(vh_quads_2);

    // 2D unpacked array access
    TestVpiHandle vh_mem_2d = vpi_handle_by_name((PLI_BYTE8*)"t.mem_2d", nullptr);
    CHECK_RESULT_NZ(vh_mem_2d);

    PLI_INT32 indices_2d[2] = {1, 3};
    TestVpiHandle vh_mem_2d_1_3 = vpi_handle_by_multi_index(vh_mem_2d, 2, indices_2d);
    CHECK_RESULT_NZ(vh_mem_2d_1_3);

    s_vpi_value v;
    v.format = vpiIntVal;
    vpi_get_value(vh_mem_2d_1_3, &v);
    CHECK_RESULT(v.value.integer, 11);  // 1*8 + 3

    // 3D unpacked array access
    TestVpiHandle vh_mem_3d = vpi_handle_by_name((PLI_BYTE8*)"t.mem_3d", nullptr);
    CHECK_RESULT_NZ(vh_mem_3d);

    PLI_INT32 indices_3d[3] = {1, 1, 1};
    TestVpiHandle vh_mem_3d_1_1_1 = vpi_handle_by_multi_index(vh_mem_3d, 3, indices_3d);
    CHECK_RESULT_NZ(vh_mem_3d_1_1_1);

    v.format = vpiIntVal;
    vpi_get_value(vh_mem_3d_1_1_1, &v);
    CHECK_RESULT(v.value.integer, 7);  // (1*4) + (1*2) + 1

    // Verify multi_index matches sequential vpi_handle_by_index
    TestVpiHandle vh_mem_2d_seq_1 = vpi_handle_by_index(vh_mem_2d, 1);
    CHECK_RESULT_NZ(vh_mem_2d_seq_1);
    TestVpiHandle vh_mem_2d_seq_1_3 = vpi_handle_by_index(vh_mem_2d_seq_1, 3);
    CHECK_RESULT_NZ(vh_mem_2d_seq_1_3);
    vpi_get_value(vh_mem_2d_seq_1_3, &v);
    CHECK_RESULT(v.value.integer, 11);

    // ========== vpi_handle_by_name WITH ARRAY INDEXING ==========

    // Single index via name
    TestVpiHandle vh_quads_2_by_name = vpi_handle_by_name((PLI_BYTE8*)"t.quads[2]", nullptr);
    CHECK_RESULT_NZ(vh_quads_2_by_name);

    // 2D array via name
    TestVpiHandle vh_mem_2d_by_name = vpi_handle_by_name((PLI_BYTE8*)"t.mem_2d[1][3]", nullptr);
    CHECK_RESULT_NZ(vh_mem_2d_by_name);
    vpi_get_value(vh_mem_2d_by_name, &v);
    CHECK_RESULT(v.value.integer, 11);

    // 3D array via name
    TestVpiHandle vh_mem_3d_by_name = vpi_handle_by_name((PLI_BYTE8*)"t.mem_3d[1][1][1]", nullptr);
    CHECK_RESULT_NZ(vh_mem_3d_by_name);
    vpi_get_value(vh_mem_3d_by_name, &v);
    CHECK_RESULT(v.value.integer, 7);

    // Spaces in indices (should work - Verilog parser handles space)
    TestVpiHandle vh_mem_2d_spaces = vpi_handle_by_name((PLI_BYTE8*)"t.mem_2d[ 0 ][ 4 ]", nullptr);
    CHECK_RESULT_NZ(vh_mem_2d_spaces);
    vpi_get_value(vh_mem_2d_spaces, &v);
    CHECK_RESULT(v.value.integer, 4);

    // ========== ERROR HANDLING: INVALID INPUTS TO vpi_handle_by_multi_index ==========

    // Null handle
    PLI_INT32 dummy_indices[1] = {0};
    TestVpiHandle vh_null_handle = vpi_handle_by_multi_index(nullptr, 1, dummy_indices);
    CHECK_RESULT_Z(vh_null_handle);

    // Null index array
    TestVpiHandle vh_null_array = vpi_handle_by_multi_index(vh_quads, 1, nullptr);
    CHECK_RESULT_Z(vh_null_array);

    // Zero num_index
    TestVpiHandle vh_zero_indices = vpi_handle_by_multi_index(vh_quads, 0, dummy_indices);
    CHECK_RESULT_Z(vh_zero_indices);

    // Negative num_index
    TestVpiHandle vh_neg_indices = vpi_handle_by_multi_index(vh_quads, -1, dummy_indices);
    CHECK_RESULT_Z(vh_neg_indices);

    // ========== ERROR HANDLING: OUT OF BOUNDS AND INVALID INDICES ==========

    // Out of bounds on 1D array (quads is [2:3])
    PLI_INT32 oob_indices[1] = {99};
    TestVpiHandle vh_quads_oob = vpi_handle_by_multi_index(vh_quads, 1, oob_indices);
    CHECK_RESULT_Z(vh_quads_oob);

    // Out of bounds on 2D array (mem_2d[0:3][0:7])
    PLI_INT32 oob_indices_2d[2] = {0, 99};
    TestVpiHandle vh_mem_2d_oob = vpi_handle_by_multi_index(vh_mem_2d, 2, oob_indices_2d);
    CHECK_RESULT_Z(vh_mem_2d_oob);

    // ========== ERROR HANDLING: TOO MANY DIMENSIONS / BIT SELECTION ==========

    // Indexing into packed dimensions of a signal
    // quads[2] returns a 62-bit packed vector, and we can select bits from it
    // So quads[2][0] actually succeeds and returns a 1-bit value (bit selection is allowed)
    TestVpiHandle vh_quads_elem = vpi_handle_by_index(vh_quads, 2);
    CHECK_RESULT_NZ(vh_quads_elem);  // quads[2] should succeed
    TestVpiHandle vh_quads_bit = vpi_handle_by_index(vh_quads_elem, 0);
    CHECK_RESULT_NZ(vh_quads_bit);  // quads[2][0] succeeds - bit selection is allowed

    // Verify multi-index works correctly and applying too many indices eventually fails
    PLI_INT32 multi_indices[3] = {0, 0, 0};
    TestVpiHandle vh_mem_3d_test = vpi_handle_by_name((PLI_BYTE8*)"t.mem_3d", nullptr);
    CHECK_RESULT_NZ(vh_mem_3d_test);
    // Apply all 3 indices - should work since mem_3d is 3D
    TestVpiHandle vh_mem_3d_result = vpi_handle_by_multi_index(vh_mem_3d_test, 3, multi_indices);
    CHECK_RESULT_NZ(vh_mem_3d_result);

    // But applying more indices to 3D array should fail if we go beyond bit selection range
    TestVpiHandle vh_3d_elem = vpi_handle_by_index(vh_mem_3d_result, 0);
    CHECK_RESULT_NZ(vh_3d_elem);  // Index [0] into 16-bit value succeeds (bit 0)

    // Try to index beyond available bits
    TestVpiHandle vh_3d_elem_oob = vpi_handle_by_index(vh_mem_3d_result, 16);
    CHECK_RESULT_Z(vh_3d_elem_oob);  // Index [16] on 16-bit value should fail (out of range)

    // ========== ERROR HANDLING: INVALID SYNTAX IN vpi_handle_by_name ==========

    // Trailing garbage after valid index
    TestVpiHandle vh_trailing_garbage
        = vpi_handle_by_name((PLI_BYTE8*)"t.mem_2d[0][0]bar", nullptr);
    CHECK_RESULT_Z(vh_trailing_garbage);

    // Non-integer index
    TestVpiHandle vh_non_int_index = vpi_handle_by_name((PLI_BYTE8*)"t.mem_2d[0][abc]", nullptr);
    CHECK_RESULT_Z(vh_non_int_index);

    // Floating point index
    TestVpiHandle vh_float_index = vpi_handle_by_name((PLI_BYTE8*)"t.mem_2d[0][3.14]", nullptr);
    CHECK_RESULT_Z(vh_float_index);

    // Range index (colon notation) instead of single index
    TestVpiHandle vh_range_index = vpi_handle_by_name((PLI_BYTE8*)"t.mem_2d[0][1:3]", nullptr);
    CHECK_RESULT_Z(vh_range_index);

    // Empty index brackets
    TestVpiHandle vh_empty_brackets = vpi_handle_by_name((PLI_BYTE8*)"t.mem_2d[0][]", nullptr);
    CHECK_RESULT_Z(vh_empty_brackets);

    // Missing closing bracket
    TestVpiHandle vh_missing_close = vpi_handle_by_name((PLI_BYTE8*)"t.mem_2d[0[0]", nullptr);
    CHECK_RESULT_Z(vh_missing_close);

    // Unclosed bracket
    TestVpiHandle vh_unclosed = vpi_handle_by_name((PLI_BYTE8*)"t.mem_2d[0][1", nullptr);
    CHECK_RESULT_Z(vh_unclosed);

    // ========== WHITESPACE ROBUSTNESS TESTS ==========

    // Whitespace inside brackets
    TestVpiHandle vh_ws_inside_single
        = vpi_handle_by_name((PLI_BYTE8*)"t.mem_2d[ 1 ][ 3 ]", nullptr);
    CHECK_RESULT_NZ(vh_ws_inside_single);
    vpi_get_value(vh_ws_inside_single, &v);
    CHECK_RESULT(v.value.integer, 11);  // 1*8 + 3

    // Leading zeros with whitespace
    TestVpiHandle vh_ws_leading_zeros
        = vpi_handle_by_name((PLI_BYTE8*)"t.mem_2d[ 001 ][ 002 ]", nullptr);
    CHECK_RESULT_NZ(vh_ws_leading_zeros);
    vpi_get_value(vh_ws_leading_zeros, &v);
    CHECK_RESULT(v.value.integer, 10);  // 1*8 + 2

    // Whitespace between bracket groups
    TestVpiHandle vh_ws_between_brackets
        = vpi_handle_by_name((PLI_BYTE8*)"t.mem_2d[2] [4]", nullptr);
    CHECK_RESULT_NZ(vh_ws_between_brackets);
    vpi_get_value(vh_ws_between_brackets, &v);
    CHECK_RESULT(v.value.integer, 20);  // 2*8 + 4

    // Multiple whitespace between bracket groups
    TestVpiHandle vh_ws_multi_between
        = vpi_handle_by_name((PLI_BYTE8*)"t.mem_2d[0]  \t  [5]", nullptr);
    CHECK_RESULT_NZ(vh_ws_multi_between);
    vpi_get_value(vh_ws_multi_between, &v);
    CHECK_RESULT(v.value.integer, 5);  // 0*8 + 5

    // Trailing whitespace after indices
    TestVpiHandle vh_ws_trailing = vpi_handle_by_name((PLI_BYTE8*)"t.mem_2d[3][7]  ", nullptr);
    CHECK_RESULT_NZ(vh_ws_trailing);
    vpi_get_value(vh_ws_trailing, &v);
    CHECK_RESULT(v.value.integer, 31);  // 3*8 + 7

    // Whitespace with tab characters
    TestVpiHandle vh_ws_tabs = vpi_handle_by_name((PLI_BYTE8*)"t.mem_2d[\t2\t][\t6\t]", nullptr);
    CHECK_RESULT_NZ(vh_ws_tabs);
    vpi_get_value(vh_ws_tabs, &v);
    CHECK_RESULT(v.value.integer, 22);  // 2*8 + 6

    // Whitespace with newline characters
    TestVpiHandle vh_ws_newline = vpi_handle_by_name((PLI_BYTE8*)"t.mem_2d[1\n][2]", nullptr);
    CHECK_RESULT_NZ(vh_ws_newline);
    vpi_get_value(vh_ws_newline, &v);
    CHECK_RESULT(v.value.integer, 10);  // 1*8 + 2

    // Only whitespace inside brackets - should fail
    TestVpiHandle vh_ws_only = vpi_handle_by_name((PLI_BYTE8*)"t.mem_2d[   ][0]", nullptr);
    CHECK_RESULT_Z(vh_ws_only);

    // ========== ERROR HANDLING: INDEXING NON-ARRAY SIGNALS ==========

    // Attempt to index into a non-array scalar (onebit has no unpacked dimensions)
    TestVpiHandle vh_onebit_indexed = vpi_handle_by_name((PLI_BYTE8*)"t.onebit[0]", nullptr);
    CHECK_RESULT_Z(vh_onebit_indexed);

    // Attempt to index into a packed-only vector (twoone has no unpacked dimensions)
    TestVpiHandle vh_twoone_indexed = vpi_handle_by_name((PLI_BYTE8*)"t.twoone[0]", nullptr);
    CHECK_RESULT_Z(vh_twoone_indexed);

    // ========== ERROR HANDLING: NON-EXISTENT SIGNALS ==========

    // Non-existent signal
    TestVpiHandle vh_nonexistent = vpi_handle_by_name((PLI_BYTE8*)"t.nonexistent[0][0]", nullptr);
    CHECK_RESULT_Z(vh_nonexistent);

    // Partial path to non-existent signal that tries to index too far
    TestVpiHandle vh_partial_nonexistent
        = vpi_handle_by_name((PLI_BYTE8*)"t.mem_2d[0][0][999]", nullptr);
    CHECK_RESULT_Z(vh_partial_nonexistent);

    // ========== EDGE CASES: BOUNDARY CONDITIONS ==========

    // Lowest valid index on 2D array
    PLI_INT32 indices_min[2] = {0, 0};
    TestVpiHandle vh_mem_2d_min = vpi_handle_by_multi_index(vh_mem_2d, 2, indices_min);
    CHECK_RESULT_NZ(vh_mem_2d_min);
    vpi_get_value(vh_mem_2d_min, &v);
    CHECK_RESULT(v.value.integer, 0);

    // Highest valid index on 2D array
    PLI_INT32 indices_max[2] = {3, 7};
    TestVpiHandle vh_mem_2d_max = vpi_handle_by_multi_index(vh_mem_2d, 2, indices_max);
    CHECK_RESULT_NZ(vh_mem_2d_max);
    vpi_get_value(vh_mem_2d_max, &v);
    CHECK_RESULT(v.value.integer, 31);  // 3*8 + 7

    // ========== RANGE SELECTION TESTS (should all fail - ranges not supported as single index)
    // ==========

    // Range in first dimension
    TestVpiHandle vh_range_first = vpi_handle_by_name((PLI_BYTE8*)"t.mem_2d[0:2][0]", nullptr);
    CHECK_RESULT_Z(vh_range_first);

    // Range in second dimension
    TestVpiHandle vh_range_second = vpi_handle_by_name((PLI_BYTE8*)"t.mem_2d[0][1:4]", nullptr);
    CHECK_RESULT_Z(vh_range_second);

    // Range in both dimensions
    TestVpiHandle vh_range_both = vpi_handle_by_name((PLI_BYTE8*)"t.mem_2d[0:2][1:4]", nullptr);
    CHECK_RESULT_Z(vh_range_both);

    // Plus notation for ranges
    TestVpiHandle vh_range_plus = vpi_handle_by_name((PLI_BYTE8*)"t.mem_2d[0+:2][0]", nullptr);
    CHECK_RESULT_Z(vh_range_plus);

    // ========== MIXED PACKED AND UNPACKED DIMENSION TESTS ==========

    // fourthreetwoone is a packed array: reg [2:1] fourthreetwoone[4:3]
    // This has unpacked dimension [4:3] and packed dimensions [2:1]
    TestVpiHandle vh_packed_array = vpi_handle_by_name((PLI_BYTE8*)"t.fourthreetwoone", nullptr);
    if (vh_packed_array) {
        // Access fourthreetwoone[4] - should succeed
        TestVpiHandle vh_packed_elem
            = vpi_handle_by_name((PLI_BYTE8*)"t.fourthreetwoone[4]", nullptr);
        CHECK_RESULT_NZ(vh_packed_elem);

        // Try to index into the packed element - should work for bit selection
        TestVpiHandle vh_packed_bit = vpi_handle_by_index(vh_packed_elem, 1);
        CHECK_RESULT_NZ(vh_packed_bit);  // Bit selection should work

        // Try to index beyond available bits - should fail
        TestVpiHandle vh_packed_oob_bit = vpi_handle_by_index(vh_packed_elem, 10);
        CHECK_RESULT_Z(vh_packed_oob_bit);  // Out of range bit selection should fail
    }

    // ========== FINAL BIT SELECTION TESTS ==========

    // mem_1d[0] returns a 32-bit value - we can select from it
    TestVpiHandle vh_mem_1d = vpi_handle_by_name((PLI_BYTE8*)"t.mem_1d[0]", nullptr);
    CHECK_RESULT_NZ(vh_mem_1d);
    TestVpiHandle vh_mem_1d_bit = vpi_handle_by_index(vh_mem_1d, 0);
    CHECK_RESULT_NZ(vh_mem_1d_bit);  // Bit selection should work

    // mem_2d[0][0] returns an 8-bit value - we can select from it
    TestVpiHandle vh_mem_2d_elem = vpi_handle_by_name((PLI_BYTE8*)"t.mem_2d[0][0]", nullptr);
    CHECK_RESULT_NZ(vh_mem_2d_elem);
    TestVpiHandle vh_mem_2d_elem_bit = vpi_handle_by_index(vh_mem_2d_elem, 0);
    CHECK_RESULT_NZ(vh_mem_2d_elem_bit);  // Bit selection should work
    TestVpiHandle vh_mem_2d_elem_bit_oob = vpi_handle_by_index(vh_mem_2d_elem, 8);
    CHECK_RESULT_Z(vh_mem_2d_elem_bit_oob);  // Out of range bit should fail

    // Highest valid index on 3D array
    PLI_INT32 indices_3d_max[3] = {1, 1, 1};
    TestVpiHandle vh_mem_3d_max = vpi_handle_by_multi_index(vh_mem_3d, 3, indices_3d_max);
    CHECK_RESULT_NZ(vh_mem_3d_max);
    vpi_get_value(vh_mem_3d_max, &v);
    CHECK_RESULT(v.value.integer, 7);

    // ========== EDGE CASES: MULTIPLE TESTS ON SAME ARRAY ==========

    // Multiple sequential accesses to different indices
    for (int idx = 0; idx < 4; idx++) {
        PLI_INT32 test_indices[1] = {idx};
        TestVpiHandle vh_mem_1d_idx = vpi_handle_by_name((PLI_BYTE8*)"t.mem_1d", nullptr);
        if (!vh_mem_1d_idx) return 1033;  // Error code for debugging
        TestVpiHandle vh_elem = vpi_handle_by_index(vh_mem_1d_idx, idx);
        if (!vh_elem) return 1034;
        vpi_get_value(vh_elem, &v);
        CHECK_RESULT(v.value.integer, idx * 256);
    }

    // Multiple accesses to same element
    for (int i = 0; i < 3; i++) {
        TestVpiHandle vh_repeated = vpi_handle_by_name((PLI_BYTE8*)"t.mem_2d[2][2]", nullptr);
        CHECK_RESULT_NZ(vh_repeated);
        vpi_get_value(vh_repeated, &v);
        CHECK_RESULT(v.value.integer, 18);  // 2*8 + 2
    }

    return 0;
}

extern "C" int mon_check() {
    // Callback from initial block in monitor
#ifdef TEST_VERBOSE
    printf("-mon_check()\n");
#endif

    if (int status = _mon_check_mcd()) return status;
    if (int status = _mon_check_callbacks()) return status;
    if (int status = _mon_check_value_callbacks()) return status;
    if (int status = _mon_check_var()) return status;
    if (int status = _mon_check_rev()) return status;
    if (int status = _mon_check_varlist()) return status;
    if (int status = _mon_check_var_long_name()) return status;
// Ports are not public_flat_rw in t_vpi_var
#if defined(T_VPI_VAR2) || defined(T_VPI_VAR3)
    if (int status = _mon_check_ports()) return status;
#endif
    if (int status = _mon_check_getput()) return status;
    if (int status = _mon_check_getput_iter()) return status;
    if (int status = _mon_check_quad()) return status;
    if (int status = _mon_check_string()) return status;
    if (int status = _mon_check_putget_str(NULL)) return status;
    if (int status = _mon_check_vlog_info()) return status;
    if (int status = _mon_check_multi_index()) return status;
    if (int status = _mon_check_delayed()) return status;
    if (int status = _mon_check_too_big()) return status;
#ifndef IS_VPI
    VerilatedVpi::selfTest();
#endif
    return 0;  // Ok
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

double sc_time_stamp() { return main_time; }
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

#if VM_TRACE
    contextp->traceEverOn(true);
    VL_PRINTF("Enabling waves...\n");
    VerilatedVcdC* tfp = new VerilatedVcdC;
    topp->trace(tfp, 99);
    tfp->open(STRINGIFY(TEST_OBJ_DIR) "/simx.vcd");
#endif

    topp->clk = 0;
    topp->a = 0;

    topp->eval();
    main_time += 10;

    while (vl_time_stamp64() < sim_time && !contextp->gotFinish()) {
        main_time += 1;
        VerilatedVpi::doInertialPuts();
        topp->eval();
        VerilatedVpi::callValueCbs();
        topp->clk = !topp->clk;
        // mon_do();
#if VM_TRACE
        if (tfp) tfp->dump(main_time);
#endif
    }
    CHECK_RESULT(callback_count, 501);
    CHECK_RESULT(callback_count_half, 250);
    CHECK_RESULT(callback_count_quad, 2);
    CHECK_RESULT(callback_count_strs, callback_count_strs_max);
    VerilatedVpi::clearEvalNeeded();
    if (VerilatedVpi::evalNeeded()) {
        vl_fatal(FILENM, __LINE__, "main", "%Error: Unexpected VPI dirty state");
    }
    touch_signal();
    if (!VerilatedVpi::evalNeeded()) {
        vl_fatal(FILENM, __LINE__, "main", "%Error: Unexpected VPI clean state");
    }
    if (!contextp->gotFinish()) {
        vl_fatal(FILENM, __LINE__, "main", "%Error: Timeout; never got a $finish");
    }
    topp->final();

#if VM_TRACE
    if (tfp) tfp->close();
#endif

    return 0;
}

#endif
