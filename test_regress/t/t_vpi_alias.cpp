// -*- mode: C++; c-file-style: "cc-mode" -*-
// Simple VPI alias test: inner module contains public_flat_rw signal that
// should be eliminated during inlining.  Verify VPI write reaches the
// canonical storage.

#include "verilated.h"
#include "verilated_vpi.h"

#include "TestSimulator.h"
#include "TestVpi.h"
#include "Vt_vpi_alias.h"
#include "Vt_vpi_alias__Dpi.h"

#include <cstdio>
#include <cstdlib>

int main(int argc, char** argv) {
    const std::unique_ptr<VerilatedContext> contextp{new VerilatedContext};
    contextp->debug(0);
    contextp->commandArgs(argc, argv);

    const std::unique_ptr<Vt_vpi_alias> top{new Vt_vpi_alias{contextp.get(), ""}};

    // initial values
    top->a = 0;
    top->b = 0;
    top->eval();

    // obtain handle to the internal signal 'tmp' (should alias to real storage)
    TestVpiHandle h = VPI_HANDLE("tmp");
    CHECK_RESULT_NZ(h);

    // write 1 via VPI and evaluate
    s_vpi_value v;
    v.format = vpiIntVal;
    v.value.integer = 1;
    vpi_put_value(h, &v, NULL, vpiNoDelay);
    top->eval();

    // the inner logic computes tmp = a & b; we overwrote tmp so y should reflect
    // the VPI write regardless of a,b.  After putting 1, y should be 1.
    if (top->y != 1) {
        vl_fatal(__FILE__, __LINE__, "main", "%%Error: alias test failed, y=%d\n", top->y);
    }

    return 0;
}
