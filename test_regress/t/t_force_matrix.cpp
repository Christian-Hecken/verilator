// SPDX-License-Identifier: CC0-1.0

#include <cstdio>
#include <cstdlib>
#include <memory>
#include <vector>
#include <string>
#include <utility>
#include <cstdint>

#include <verilated.h>
#include <verilated_vpi.h>
#include "verilated_syms.h"
#include "vpi_user.h"
#include "svdpi.h"
// Include the generated model header (prefix set by verilator --prefix)
#include "Vt_force_matrix.h"

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
extern "C" void sv_force_out_nc(unsigned char val);
extern "C" void sv_release_out_nc(void);
extern "C" void sv_force_out_c(unsigned char val);
extern "C" void sv_release_out_c(void);
extern "C" int sv_get_in_nc(void);
extern "C" int sv_get_out_nc(void);
extern "C" int sv_get_out_c(void);

// Helpers ---------------------------------------------------------------
static bool vpi_put_int(const char* fullname, int value, int flag) {
    vpiHandle vh = vpi_handle_by_name((PLI_BYTE8*)fullname, nullptr);
    if (!vh) return false;
    s_vpi_value v{};
    v.format = vpiIntVal;
    v.value.integer = value;
    vpi_put_value(vh, &v, nullptr, flag);
    return true;
}

static bool vpi_get_int(const char* fullname, int &out) {
    vpiHandle vh = vpi_handle_by_name((PLI_BYTE8*)fullname, nullptr);
    if (!vh) return false;
    s_vpi_value v{};
    v.format = vpiIntVal;
    vpi_get_value(vh, &v);
    out = v.value.integer;
    return true;
}

static const VerilatedScope* find_scope(VerilatedContext* ctx, const char* namep) {
    return ctx->scopeFind(namep);
}

static const VerilatedVar* find_var(const VerilatedScope* scopep, const char* namep) {
    if (!scopep) return nullptr;
    return scopep->varFind(namep);
}

static bool cxx_set_int(const VerilatedScope* scopep, const char* namep, uint64_t value) {
    const VerilatedVar* varp = find_var(scopep, namep);
    if (!varp) return false;
    switch (varp->vltype()) {
    case VLVT_UINT8: *(uint8_t*)varp->datap() = (uint8_t)value; return true;
    case VLVT_UINT16: *(uint16_t*)varp->datap() = (uint16_t)value; return true;
    case VLVT_UINT32: *(uint32_t*)varp->datap() = (uint32_t)value; return true;
    case VLVT_UINT64: *(uint64_t*)varp->datap() = (uint64_t)value; return true;
    case VLVT_WDATA: {
        WData* wdp = (WData*)varp->datap();
        if (!wdp) return false;
        wdp[0] = (WData)value;
        return true;
    }
    default: return false;
    }
}

static bool cxx_get_int(const VerilatedScope* scopep, const char* namep, uint64_t &out) {
    const VerilatedVar* varp = find_var(scopep, namep);
    if (!varp) return false;
    switch (varp->vltype()) {
    case VLVT_UINT8: out = *(uint8_t*)varp->datap(); return true;
    case VLVT_UINT16: out = *(uint16_t*)varp->datap(); return true;
    case VLVT_UINT32: out = *(uint32_t*)varp->datap(); return true;
    case VLVT_UINT64: out = *(uint64_t*)varp->datap(); return true;
    case VLVT_WDATA: {
        WData* wdp = (WData*)varp->datap();
        if (!wdp) return false;
        out = (uint64_t)wdp[0];
        return true;
    }
    default: return false;
    }
}

static bool cxx_force_var(const VerilatedScope* scopep, const char* namep, uint64_t value) {
    const VerilatedVar* varp = find_var(scopep, namep);
    if (!varp) return false;
    const VerilatedForceControlSignals* fcs = varp->forceControlSignals();
    if (!fcs) return false;
    const VerilatedVar* valp = fcs->forceValueSignalp;
    const VerilatedVar* enp = fcs->forceEnableSignalp;
    if (!valp || !enp) return false;
    switch (valp->vltype()) {
    case VLVT_UINT8: *(uint8_t*)valp->datap() = (uint8_t)value; break;
    case VLVT_UINT16: *(uint16_t*)valp->datap() = (uint16_t)value; break;
    case VLVT_UINT32: *(uint32_t*)valp->datap() = (uint32_t)value; break;
    case VLVT_UINT64: *(uint64_t*)valp->datap() = (uint64_t)value; break;
    case VLVT_WDATA: {
        WData* wdp = (WData*)valp->datap();
        if (!wdp) return false;
        wdp[0] = (WData)value;
        break;
    }
    default: return false;
    }
    switch (enp->vltype()) {
    case VLVT_UINT8: *(uint8_t*)enp->datap() = (uint8_t)~(uint8_t)0; break;
    case VLVT_UINT16: *(uint16_t*)enp->datap() = (uint16_t)~(uint16_t)0; break;
    case VLVT_UINT32: *(uint32_t*)enp->datap() = (uint32_t)~(uint32_t)0; break;
    case VLVT_UINT64: *(uint64_t*)enp->datap() = (uint64_t)~(uint64_t)0; break;
    case VLVT_WDATA: {
        WData* wdp = (WData*)enp->datap();
        if (!wdp) return false;
        wdp[0] = (WData)~(WData)0; break;
    }
    default: return false;
    }
    return true;
}

static bool cxx_release_var(const VerilatedScope* scopep, const char* namep) {
    const VerilatedVar* varp = find_var(scopep, namep);
    if (!varp) return false;
    const VerilatedForceControlSignals* fcs = varp->forceControlSignals();
    if (!fcs) return false;
    const VerilatedVar* enp = fcs->forceEnableSignalp;
    if (!enp) return false;
    switch (enp->vltype()) {
    case VLVT_UINT8: *(uint8_t*)enp->datap() = 0; break;
    case VLVT_UINT16: *(uint16_t*)enp->datap() = 0; break;
    case VLVT_UINT32: *(uint32_t*)enp->datap() = 0; break;
    case VLVT_UINT64: *(uint64_t*)enp->datap() = 0; break;
    case VLVT_WDATA: {
        WData* wdp = (WData*)enp->datap();
        if (!wdp) return false;
        wdp[0] = 0; break;
    }
    default: return false;
    }
    return true;
}

int main(int argc, char** argv) {
    Verilated::commandArgs(argc, argv);
    const std::unique_ptr<VerilatedContext> contextp{new VerilatedContext};
    contextp->debug(0);
    std::unique_ptr<Vt_force_matrix> top{new Vt_force_matrix{contextp.get(), ""}};
    const VerilatedScope* testScope = find_scope(contextp.get(), "t.test");
    // Set SV DPI scope for exported SV helper calls
    svScope old_scope = svSetScope(svGetScopeFromName("t.test"));
    // Drive clock from C++ (SV clock generator removed)
    cxx_set_int(testScope, "clk", 0);
    // Helper: single clock tick (rising then falling edge) via C++ symbol API
    auto tick = [&](){
        // drive `clk` in scope t.test using the C++ symbol API
        cxx_set_int(testScope, "clk", 1);
        top->eval();
        VerilatedVpi::callValueCbs();
        VerilatedVpi::clearEvalNeeded();
        cxx_set_int(testScope, "clk", 0);
        top->eval();
        VerilatedVpi::callValueCbs();
        VerilatedVpi::clearEvalNeeded();
    };
    std::vector<TestCase> results;
    const char* actions[] = {"Write", "Force", "Release"};
    const char* assign_types[] = {"non-continuous", "continuous"};
    const char* drivers[] = {"VPI", "C++", "SV"};
    const char* evals[] = {"no_eval", "eval"};
    const char* checkers[] = {"VPI", "C++", "SV"};
    const int baseline = 0;
    const int action_value = 1;
    auto do_eval = [&](bool yes) {
        if (yes) tick();
        else {
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
                        if (std::string(assign_type) == "continuous" && std::string(driver) == "SV" && std::string(action) == "Write") {
                            tc.possible = false;
                            tc.reason = "SV cannot directly write continuous assignments (would create multiple drivers)";
                            results.push_back(tc);
                            continue;
                        }
                        if (!cxx_set_int(testScope, "in_nc", baseline)) {
                            tc.possible = false;
                            tc.reason = "Cannot set baseline in_nc via C++";
                            results.push_back(tc);
                            continue;
                        }
                        cxx_release_var(testScope, target);
                        do_eval(true);
                        bool applied = false;
                        if (std::string(action) == "Write") {
                            if (std::string(driver) == "VPI") {
                                applied = vpi_put_int(fullName.c_str(), action_value, vpiNoDelay);
                                if (!applied) tc.reason = "vpi handle missing or put failed";
                            } else if (std::string(driver) == "C++") {
                                applied = cxx_set_int(testScope, target, action_value);
                                if (!applied) tc.reason = "C++ set failed";
                            } else {
                                if (std::string(target) == "in_nc") {
                                    sv_write_in_nc((unsigned char)action_value);
                                    applied = true;
                                } else {
                                    tc.reason = "SV write helper only supports in_nc";
                                    applied = false;
                                }
                            }
                        } else if (std::string(action) == "Force") {
                            if (std::string(driver) == "VPI") {
                                applied = vpi_put_int(fullName.c_str(), action_value, vpiForceFlag);
                                if (!applied) tc.reason = "vpi force failed";
                            } else if (std::string(driver) == "C++") {
                                applied = cxx_force_var(testScope, target, action_value);
                                if (!applied) tc.reason = "C++ force not available for this signal";
                            } else {
                                if (std::string(target) == "out_nc") {
                                    sv_force_out_nc((unsigned char)action_value);
                                    applied = true;
                                } else if (std::string(target) == "out_c") {
                                    sv_force_out_c((unsigned char)action_value);
                                    applied = true;
                                } else {
                                    applied = false;
                                }
                            }
                        } else {
                            bool setup_ok = false;
                            if (std::string(driver) == "VPI") {
                                setup_ok = vpi_put_int(fullName.c_str(), action_value, vpiForceFlag);
                            } else if (std::string(driver) == "C++") {
                                setup_ok = cxx_force_var(testScope, target, action_value);
                            } else {
                                if (std::string(target) == "out_nc") {
                                    sv_force_out_nc((unsigned char)action_value);
                                    setup_ok = true;
                                } else if (std::string(target) == "out_c") {
                                    sv_force_out_c((unsigned char)action_value);
                                    setup_ok = true;
                                }
                            }
                            if (!setup_ok) {
                                tc.possible = false;
                                tc.reason = "Could not set up force prior to release";
                                results.push_back(tc);
                                continue;
                            }
                            if (std::string(driver) == "VPI") {
                                applied = vpi_put_int(fullName.c_str(), 0, vpiReleaseFlag);
                                if (!applied) tc.reason = "vpi release failed";
                            } else if (std::string(driver) == "C++") {
                                applied = cxx_release_var(testScope, target);
                                if (!applied) tc.reason = "C++ release failed";
                            } else {
                                if (std::string(target) == "out_nc") {
                                    sv_release_out_nc();
                                    applied = true;
                                } else if (std::string(target) == "out_c") {
                                    sv_release_out_c();
                                    applied = true;
                                }
                            }
                        }
                        if (!applied && tc.possible) {
                            tc.possible = false;
                            if (tc.reason.empty()) tc.reason = "Action not applied";
                            results.push_back(tc);
                            continue;
                        }
                        bool doEval = (std::string(eval) == "eval");
                        do_eval(doEval);
                        int observed = -1;
                        bool got = false;
                        if (std::string(checker) == "VPI") {
                            got = vpi_get_int(fullName.c_str(), observed);
                        } else if (std::string(checker) == "C++") {
                            uint64_t tmp = 0;
                            got = cxx_get_int(testScope, target, tmp);
                            observed = (int)tmp;
                        } else {
                            if (std::string(target) == "in_nc") observed = sv_get_in_nc(), got = true;
                            else if (std::string(target) == "out_nc") observed = sv_get_out_nc(), got = true;
                            else if (std::string(target) == "out_c") observed = sv_get_out_c(), got = true;
                        }
                        if (!got) {
                            tc.possible = false;
                            tc.reason = "Checker could not read signal";
                            results.push_back(tc);
                            continue;
                        }
                        tc.observed = observed;
                        int expected = baseline;
                        if (std::string(action) == "Write") {
                            expected = doEval ? action_value : baseline;
                        } else if (std::string(action) == "Force") {
                            expected = doEval ? action_value : baseline;
                        } else {
                            expected = doEval ? baseline : action_value;
                        }
                        tc.passed = (tc.observed == expected);
                        results.push_back(tc);
                    }
                }
            }
        }
    }
    std::printf("t_force_matrix: Test Results\n");
    for (const auto& tc : results) {
        if (!tc.possible) {
            std::printf("[%s][%s][%s][%s][%s]: SKIP: %s\n",
                        tc.action.c_str(), tc.assign_type.c_str(), tc.driver.c_str(), tc.eval.c_str(),
                        tc.checker.c_str(), tc.reason.c_str());
        } else {
            std::printf("[%s][%s][%s][%s][%s]: %s (observed=%d)\n",
                        tc.action.c_str(), tc.assign_type.c_str(), tc.driver.c_str(), tc.eval.c_str(),
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
