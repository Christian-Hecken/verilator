// DESCRIPTION: Verilator: Verilog Test module
//
// This file ONLY is placed under the Creative Commons Public Domain.
// SPDX-FileCopyrightText: 2026 Verilator Contributors
// SPDX-License-Identifier: CC0-1.0

#include <cstdio>
#include <cstdlib>
#include <memory>
#include <vector>
#include <string>
#include <cstdint>

#include <verilated.h>
#include <verilated_vpi.h>
#include "vpi_user.h"
#include "svdpi.h"
#include "Vt_force_matrix.h"
#include "Vt_force_matrix___024root.h"

struct TestCase {
    std::string action;
    std::string assign_type;
    std::string driver;
    std::string eval;
    std::string checker;
    bool possible;
    std::string reason;
    bool passed;
    int observed;
};

// Prototypes for exported SystemVerilog helper routines
extern "C" void sv_write_in_nc(unsigned char val);
extern "C" void sv_write_out_nc(unsigned char val);
extern "C" void sv_force_out_nc(unsigned char val);
extern "C" void sv_release_out_nc(void);
extern "C" void sv_force_out_c(unsigned char val);
extern "C" void sv_release_out_c(void);
extern "C" int sv_get_in_nc(void);
extern "C" int sv_get_out_nc(void);
extern "C" int sv_get_out_c(void);

// VPI helper functions
static bool vpi_put_int(const char* fullname, int value, int flag) {
    vpiHandle vh = vpi_handle_by_name((PLI_BYTE8*)fullname, nullptr);
    if (!vh) return false;
    s_vpi_value v{};
    v.format = vpiIntVal;
    v.value.integer = value;
    vpi_put_value(vh, &v, nullptr, flag);
    return true;
}

static bool vpi_get_int(const char* fullname, int& out) {
    vpiHandle vh = vpi_handle_by_name((PLI_BYTE8*)fullname, nullptr);
    if (!vh) return false;
    s_vpi_value v{};
    v.format = vpiIntVal;
    vpi_get_value(vh, &v);
    out = v.value.integer;
    return true;
}

int main(int argc, char** argv) {
    Verilated::commandArgs(argc, argv);
    const std::unique_ptr<VerilatedContext> contextp{new VerilatedContext};
    contextp->debug(0);
    std::unique_ptr<Vt_force_matrix> top{new Vt_force_matrix{contextp.get(), ""}};

    // Set SV DPI scope for exported SV helper calls
    svScope old_scope = svSetScope(svGetScopeFromName("t.test"));

    // Helper: single clock tick (rising then falling edge) via rootp direct access
    auto tick = [&]() {
        top->rootp->t__DOT__clk = 1;
        top->eval();
        VerilatedVpi::doInertialPuts();
        VerilatedVpi::callValueCbs();
        VerilatedVpi::clearEvalNeeded();
        top->rootp->t__DOT__clk = 0;
        top->eval();
        VerilatedVpi::doInertialPuts();
        VerilatedVpi::callValueCbs();
        VerilatedVpi::clearEvalNeeded();
    };

    // Initialize clock
    top->rootp->t__DOT__clk = 0;
    top->eval();

    std::vector<TestCase> results;

    const char* actions[] = {"Write", "Force", "Release"};
    const char* assign_types[] = {"non-continuous", "continuous"};
    const char* drivers[] = {"VPI", "C++", "SV"};
    const char* evals[] = {"no_eval", "eval"};
    const char* checkers[] = {"VPI", "C++", "SV"};
    const int baseline = 0;
    const int action_value = 1;

    auto do_eval = [&](bool yes) {
        if (yes) {
            tick();
        } else {
            VerilatedVpi::doInertialPuts();
            VerilatedVpi::callValueCbs();
            VerilatedVpi::clearEvalNeeded();
        }
    };

    for (auto action : actions) {
        for (auto assign_type : assign_types) {
            const char* target = (std::string(assign_type) == "continuous") ? "out_c" : "out_nc";
            const std::string fullName = std::string("t.test.") + target;

            for (auto driver : drivers) {
                for (auto eval : evals) {
                    for (auto checker : checkers) {
                        TestCase tc;
                        tc.action = action;
                        tc.assign_type = assign_type;
                        tc.driver = driver;
                        tc.eval = eval;
                        tc.checker = checker;
                        tc.possible = true;
                        tc.reason.clear();
                        tc.passed = false;
                        tc.observed = -1;

                        // Skip impossible combinations
                        if (std::string(assign_type) == "continuous" && std::string(driver) == "SV"
                            && std::string(action) == "Write") {
                            tc.possible = false;
                            tc.reason = "SV cannot directly write continuous assignments (multiple drivers)";
                            results.push_back(tc);
                            continue;
                        }

                        // Reset: set in_nc to baseline and release any force on target
                        top->rootp->t__DOT__test__DOT__in_nc = baseline;
                        if (std::string(target) == "out_nc") {
                            top->rootp->t__DOT__test__DOT__out_nc__VforceEn = 0;
                        } else {
                            top->rootp->t__DOT__test__DOT__out_c__VforceEn = 0;
                        }
                        do_eval(true);

                        // Perform action
                        bool applied = false;

                        if (std::string(action) == "Write") {
                            if (std::string(driver) == "VPI") {
                                applied = vpi_put_int(fullName.c_str(), action_value, vpiNoDelay);
                                if (!applied) tc.reason = "vpi handle missing or put failed";
                            } else if (std::string(driver) == "C++") {
                                if (std::string(target) == "out_nc") {
                                    top->rootp->t__DOT__test__DOT__out_nc = action_value;
                                } else {
                                    top->rootp->t__DOT__test__DOT__out_c = action_value;
                                }
                                applied = true;
                            } else {  // SV
                                if (std::string(target) == "out_nc") {
                                    sv_write_out_nc((unsigned char)action_value);
                                    applied = true;
                                } else {
                                    // Can't write continuous through SV (already caught above)
                                    applied = false;
                                }
                            }
                        } else if (std::string(action) == "Force") {
                            if (std::string(driver) == "VPI") {
                                applied = vpi_put_int(fullName.c_str(), action_value, vpiForceFlag);
                                if (!applied) tc.reason = "vpi force failed";
                            } else if (std::string(driver) == "C++") {
                                // Set force value and enable for the target signal
                                if (std::string(target) == "out_nc") {
                                    top->rootp->t__DOT__test__DOT__out_nc__VforceVal = action_value;
                                    top->rootp->t__DOT__test__DOT__out_nc__VforceEn = 1;
                                } else {
                                    top->rootp->t__DOT__test__DOT__out_c__VforceVal = action_value;
                                    top->rootp->t__DOT__test__DOT__out_c__VforceEn = 1;
                                }
                                applied = true;
                            } else {  // SV
                                if (std::string(target) == "out_nc") {
                                    sv_force_out_nc((unsigned char)action_value);
                                } else {
                                    sv_force_out_c((unsigned char)action_value);
                                }
                                applied = true;
                            }
                        } else {  // Release
                            // First set up a force to release
                            bool setup_ok = false;
                            if (std::string(driver) == "VPI") {
                                setup_ok = vpi_put_int(fullName.c_str(), action_value, vpiForceFlag);
                            } else if (std::string(driver) == "C++") {
                                if (std::string(target) == "out_nc") {
                                    top->rootp->t__DOT__test__DOT__out_nc__VforceVal = action_value;
                                    top->rootp->t__DOT__test__DOT__out_nc__VforceEn = 1;
                                } else {
                                    top->rootp->t__DOT__test__DOT__out_c__VforceVal = action_value;
                                    top->rootp->t__DOT__test__DOT__out_c__VforceEn = 1;
                                }
                                setup_ok = true;
                            } else {  // SV
                                if (std::string(target) == "out_nc") {
                                    sv_force_out_nc((unsigned char)action_value);
                                } else {
                                    sv_force_out_c((unsigned char)action_value);
                                }
                                setup_ok = true;
                            }
                            if (!setup_ok) {
                                tc.possible = false;
                                tc.reason = "Could not set up force prior to release";
                                results.push_back(tc);
                                continue;
                            }

                            // Now perform the release
                            if (std::string(driver) == "VPI") {
                                applied = vpi_put_int(fullName.c_str(), 0, vpiReleaseFlag);
                                if (!applied) tc.reason = "vpi release failed";
                            } else if (std::string(driver) == "C++") {
                                if (std::string(target) == "out_nc") {
                                    top->rootp->t__DOT__test__DOT__out_nc__VforceEn = 0;
                                } else {
                                    top->rootp->t__DOT__test__DOT__out_c__VforceEn = 0;
                                }
                                applied = true;
                            } else {  // SV
                                if (std::string(target) == "out_nc") {
                                    sv_release_out_nc();
                                } else {
                                    sv_release_out_c();
                                }
                                applied = true;
                            }
                        }

                        if (!applied && tc.possible) {
                            tc.possible = false;
                            if (tc.reason.empty()) tc.reason = "Action not applied";
                            results.push_back(tc);
                            continue;
                        }

                        // Perform optional eval
                        bool doEval = (std::string(eval) == "eval");
                        do_eval(doEval);

                        // Read the value using the specified checker method
                        int observed = -1;
                        bool got = false;

                        if (std::string(checker) == "VPI") {
                            got = vpi_get_int(fullName.c_str(), observed);
                        } else if (std::string(checker) == "C++") {
                            if (std::string(target) == "out_nc") {
                                observed = top->rootp->t__DOT__test__DOT__out_nc;
                            } else {
                                observed = top->rootp->t__DOT__test__DOT__out_c;
                            }
                            got = true;
                        } else {  // SV
                            if (std::string(target) == "in_nc") {
                                observed = sv_get_in_nc();
                                got = true;
                            } else if (std::string(target) == "out_nc") {
                                observed = sv_get_out_nc();
                                got = true;
                            } else if (std::string(target) == "out_c") {
                                observed = sv_get_out_c();
                                got = true;
                            }
                        }

                        if (!got) {
                            tc.possible = false;
                            tc.reason = "Checker could not read signal";
                            results.push_back(tc);
                            continue;
                        }

                        tc.observed = observed;

                        // Determine expected value based on action and eval
                        int expected = baseline;
                        if (std::string(action) == "Write") {
                            // Direct write takes effect immediately in memory.
                            // With eval: always_ff fires and overwrites out_nc = in_nc = 0 (baseline)
                            // Without eval: the write is visible in memory (action_value)
                            expected = doEval ? baseline : action_value;
                        } else if (std::string(action) == "Force") {
                            // Force takes effect immediately and persists through eval.
                            // With eval: force remains active -> action_value
                            // Without eval: force active -> action_value
                            expected = action_value;
                        } else {  // Release
                            // Release disables the force effect, restoring underlying logic.
                            // With eval: always_ff drives out_nc = in_nc = 0 (baseline)
                            // Without eval: previous forced value remains briefly (action_value)
                            expected = doEval ? baseline : action_value;
                        }

                        tc.passed = (tc.observed == expected);
                        results.push_back(tc);
                    }
                }
            }
        }
    }

    // Print results
    std::printf("t_force_matrix: Test Results\n");
    for (const auto& tc : results) {
        if (!tc.possible) {
            std::printf("[%s][%s][%s][%s][%s]: SKIP: %s\n", tc.action.c_str(),
                        tc.assign_type.c_str(), tc.driver.c_str(), tc.eval.c_str(),
                        tc.checker.c_str(), tc.reason.c_str());
        } else {
            std::printf("[%s][%s][%s][%s][%s]: %s (observed=%d)\n", tc.action.c_str(),
                        tc.assign_type.c_str(), tc.driver.c_str(), tc.eval.c_str(),
                        tc.checker.c_str(), tc.passed ? "PASS" : "FAIL", tc.observed);
        }
    }

    // Restore previous DPI scope and finalize
    svSetScope(old_scope);
    top->final();

    // Signal to the test harness that we completed normally
    printf("*-* All Finished *-*\n");
    return 0;
}
