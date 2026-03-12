// Simple helper used by the test. Support both VPI and DPI (non-VPI) builds.

#include "verilated.h"
#include "verilated_vpi.h"

#include "TestVpi.h"
#include "vpi_user.h"

// Always implement the DPI-callable helper using VPI operations so both the
// DPI import path and the VPI systf wrapper use the same VPI semantics.
extern "C" int mon_check(void) {
    // Find the signal by hierarchical name and write with vpiNoDelay
    TestVpiHandle vh = vpi_handle_by_name((PLI_BYTE8*)"top.sig", NULL);
    CHECK_RESULT_NZ(vh);

    s_vpi_value val;
    val.format = vpiIntVal;
    // write an 8-bit distinct value so the test exercises multi-bit propagation
    val.value.integer = 0xCC;

    // Set the signal from the helper side without scheduling an eval
    vpi_put_value(vh, &val, NULL, vpiNoDelay);

    return 0;
}

// If this file is built as a VPI object, also provide a registered
// system-function wrapper so simulators that expect a systf can call it as
// $mon_check; the wrapper places the integer return into the VPI return
// value and returns.
#ifdef IS_VPI

static int mon_check_vpi() {
    TestVpiHandle href = vpi_handle(vpiSysTfCall, 0);
    CHECK_RESULT_NZ(href);
    s_vpi_value v;
    v.format = vpiIntVal;
    v.value.integer = mon_check();
    vpi_put_value(href, &v, NULL, vpiNoDelay);
    return 0;
}

static s_vpi_systf_data vpi_systf_data[] = {{vpiSysFunc, vpiIntFunc, (PLI_BYTE8*)"$mon_check",
                                             (PLI_INT32(*)(PLI_BYTE8*))mon_check_vpi, 0, 0, 0},
                                            {0}};

void vpi_compat_bootstrap(void) {
    p_vpi_systf_data systf_data_p = &(vpi_systf_data[0]);
    while (systf_data_p->type != 0) vpi_register_systf(systf_data_p++);
}

void (*vlog_startup_routines[])() = {vpi_compat_bootstrap, 0};

#endif  // IS_VPI
