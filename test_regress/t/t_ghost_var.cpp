// -*- mode: C++; c-file-style: "cc-mode" -*-
//
// DESCRIPTION: Verilator: Ghost variable test C++ driver
//
// Tests that ghost variables are accessible via rootp and produce
// correct values even after gate reduction inlines them.
//
// This file ONLY is placed under the Creative Commons Public Domain.
// SPDX-FileCopyrightText: 2026 Verilator Contributors
// SPDX-License-Identifier: CC0-1.0

#include "verilated.h"

#include "Vt_ghost_var.h"
#include "Vt_ghost_var___024root.h"

#include <cstdio>
#include <cstdlib>
#include <memory>

int main(int argc, char** argv) {
    const std::unique_ptr<VerilatedContext> contextp{new VerilatedContext};
    contextp->commandArgs(argc, argv);

    const std::unique_ptr<Vt_ghost_var> topp{new Vt_ghost_var{contextp.get(), "top"}};

    topp->clk = 0;
    topp->eval();
    contextp->timeInc(10);

    for (int cyc = 0; cyc < 30 && !contextp->gotFinish(); cyc++) {
        // Toggle clock
        topp->clk = 1;
        topp->eval();
        contextp->timeInc(5);

        // Verify ghost variable 'c' is accessible and correct via rootp
        const uint8_t a_val = topp->rootp->t__DOT__a;
        const uint8_t expected_c = static_cast<uint8_t>(~a_val);
        const uint8_t actual_c = topp->rootp->t__DOT__c;
        if (cyc > 1 && actual_c != expected_c) {
            printf("%%Error: cyc=%d c=%u expected=%u a=%u\n", cyc, actual_c, expected_c, a_val);
            return 1;
        }

        topp->clk = 0;
        topp->eval();
        contextp->timeInc(5);
    }

    topp->final();

    if (!contextp->gotFinish()) {
        printf("%%Error: Timeout; never got $finish\n");
        return 1;
    }

    printf("Test passed\n");
    return 0;
}
