// ======================================================================
// This file ONLY is placed under the Creative Commons Public Domain, for
// any use, without warranty.
// SPDX-License-Identifier: CC0-1.0
// ======================================================================
#ifdef IS_VPI

#include "verilated.h"  // For VL_PRINTF

#include "TestSimulator.h"  // For is_icarus()
#include "TestVpi.h"  // For CHECK_RESULT_NZ
#include "vpi_user.h"

#else

#include "verilated.h"

#include "TestSimulator.h"  // For is_icarus()
#include "TestVpi.h"  // For CHECK_RESULT_NZ
#include "Vt_vpi_forceable_bad.h"  // Need to include either this or VM_PREFIX_INCLUDE for VM_PREFIX to work
#include "vpi_user.h"  // For vpi_put_value
//#include VM_PREFIX_INCLUDE

#endif

extern "C" int force_value(void) {
    if (!TestSimulator::is_verilator()) {
#ifdef VERILATOR
        printf("TestSimulator indicating not verilator, but VERILATOR macro is defined\n");
        return 1;
#endif
    }

    PLI_BYTE8 test_signal_name[] = "t.non_forceable_signal";
    vpiHandle signal = vpi_handle_by_name(test_signal_name, nullptr);
    CHECK_RESULT_NZ(signal);  // NOLINT(concurrency-mt-unsafe)

    s_vpi_value value_s;
    value_s.format = vpiIntVal;
    value_s.value.integer = 0;
    vpi_put_value(signal, &value_s, nullptr, vpiForceFlag);
    // NOLINTNEXTLINE(concurrency-mt-unsafe);
    CHECK_RESULT_Z(vpi_chk_error(nullptr))

    return 0;
}

#ifdef IS_VPI

static int force_value_vpi() {
    TestVpiHandle href = vpi_handle(vpiSysTfCall, 0);
    s_vpi_value vpi_value;

    vpi_value.format = vpiIntVal;
    vpi_value.value.integer = force_value();
    vpi_put_value(href, &vpi_value, NULL, vpiNoDelay);

    return 0;
}

std::array vpi_systf_data = {s_vpi_systf_data{vpiSysFunc, vpiIntFunc, (PLI_BYTE8*)"$force_value",
                                              (PLI_INT32(*)(PLI_BYTE8*))force_value_vpi, 0, 0, 0}};

// cver entry
extern "C" void vpi_compat_bootstrap(void) {
    for (s_vpi_systf_data& systf : vpi_systf_data) vpi_register_systf(&systf);
}

// icarus entry
void (*vlog_startup_routines[])() = {vpi_compat_bootstrap, 0};

#else
int main(int argc, char** argv) {
    const std::unique_ptr<VerilatedContext> contextp{new VerilatedContext};
    constexpr uint64_t sim_time = 5;
    contextp->debug(0);
    contextp->commandArgs(argc, argv);

    const std::unique_ptr<VM_PREFIX> topp{new VM_PREFIX{contextp.get(),
                                                        // Note null name - we're flattening it out
                                                        ""}};

    while (contextp->time() < sim_time && !contextp->gotFinish()) {
        contextp->timeInc(1);
        topp->eval();
    }

    if (!contextp->gotFinish()) {
        // NOLINTNEXTLINE(concurrency-mt-unsafe)
        vl_fatal(FILENM, __LINE__, "main", "%Error: Timeout; never got a $finish");
    }
    topp->final();

    return 0;
}
#endif
