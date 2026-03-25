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

#include "Vt_gate_opt_unified.h"

#include <cstdio>
#include <cstdlib>

int main(int argc, char** argv) {
    const std::unique_ptr<VerilatedContext> contextp{new VerilatedContext};
    contextp->commandArgs(argc, argv);

    const std::unique_ptr<Vt_gate_opt_unified> top{new Vt_gate_opt_unified{contextp.get(), "TOP"}};

    // Test VPI access to the public signal in Inner module
    // Obtain handle to the internal signal 'inner_inst.tmp' (public_flat_rw)
    vpiHandle h = vpi_handle_by_name(const_cast<PLI_BYTE8*>("TOP.t.inner_inst.tmp"), nullptr);
    if (!h) {
        vl_fatal(__FILE__, __LINE__, "main",
                 "%%Error: vpi_handle_by_name failed for TOP.t.inner_inst.tmp");
    }

    s_vpi_value v;
    v.format = vpiIntVal;

    // Test with input = 1
    top->in = 1;
    top->eval();

    // Read tmp via VPI - should match input (since tmp = in)
    vpi_get_value(h, &v);
    if (v.value.integer != 1) {
        vl_fatal(__FILE__, __LINE__, "main", "%%Error: VPI alias read test failed (expected 1)");
    }

    // Verify all outputs match input
    if (top->out_alias != 1) {
        vl_fatal(__FILE__, __LINE__, "main", "%%Error: out_alias mismatch (expected 1)");
    }
    if (top->out_dead != 1) {
        vl_fatal(__FILE__, __LINE__, "main", "%%Error: out_dead mismatch (expected 1)");
    }
    if (top->out_vpi != 1) {
        vl_fatal(__FILE__, __LINE__, "main", "%%Error: out_vpi mismatch (expected 1)");
    }

    // Test with input = 0
    top->in = 0;
    top->eval();

    vpi_get_value(h, &v);
    if (v.value.integer != 0) {
        vl_fatal(__FILE__, __LINE__, "main", "%%Error: VPI alias read test failed (expected 0)");
    }

    // Verify all outputs match input
    if (top->out_alias != 0) {
        vl_fatal(__FILE__, __LINE__, "main", "%%Error: out_alias mismatch (expected 0)");
    }
    if (top->out_dead != 0) {
        vl_fatal(__FILE__, __LINE__, "main", "%%Error: out_dead mismatch (expected 0)");
    }
    if (top->out_vpi != 0) {
        vl_fatal(__FILE__, __LINE__, "main", "%%Error: out_vpi mismatch (expected 0)");
    }

    vpi_release_handle(h);

    printf("*-* All Finished *-*\n");

    return 0;
}
