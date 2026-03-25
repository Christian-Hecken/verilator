// -*- mode: C++; c-file-style: "cc-mode" -*-
//*************************************************************************
//
// This program is free software; you can redistribute it and/or modify it
// under the terms of either the GNU Lesser General Public License Version 3
// or the Perl Artistic License Version 2.0.
// SPDX-FileCopyrightText: 2026 Wilson Snyder
// SPDX-License-Identifier: LGPL-3.0-only OR Artistic-2.0
//
//*************************************************************************

#include "verilated.h"
#include "verilated_vpi.h"

#include "Vt_vpi_alias.h"

#include <cstdio>
#include <cstdlib>

int main(int argc, char** argv) {
    const std::unique_ptr<VerilatedContext> contextp{new VerilatedContext};
    contextp->commandArgs(argc, argv);

    const std::unique_ptr<Vt_vpi_alias> top{new Vt_vpi_alias{contextp.get(), "TOP"}};

    // Drive input a = 1
    top->a = 1;
    top->eval();

    // Obtain handle to the internal signal 'in.tmp' (public_flat_rw)
    vpiHandle h = vpi_handle_by_name(const_cast<PLI_BYTE8*>("TOP.top.in.tmp"), nullptr);
    if (!h) {
        vl_fatal(__FILE__, __LINE__, "main",
                 "%%Error: vpi_handle_by_name failed for TOP.top.in.tmp");
    }

    // Read tmp via VPI - should match input a (since tmp = a)
    s_vpi_value v;
    v.format = vpiIntVal;
    vpi_get_value(h, &v);

    if (v.value.integer != 1) {
        vl_fatal(__FILE__, __LINE__, "main", "%%Error: alias read test failed (expected 1)");
    }

    // Change input and verify VPI read follows
    top->a = 0;
    top->eval();

    vpi_get_value(h, &v);
    if (v.value.integer != 0) {
        vl_fatal(__FILE__, __LINE__, "main", "%%Error: alias read test failed (expected 0)");
    }

    // Also verify that the output y matches (y = tmp = a)
    if (top->y != 0) { vl_fatal(__FILE__, __LINE__, "main", "%%Error: output y mismatch"); }

    vpi_release_handle(h);

    return 0;
}
