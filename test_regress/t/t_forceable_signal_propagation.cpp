#include "verilated.h"
#include "verilated_vpi.h"

#include "TestVpi.h"
#include "vpi_user.h"

extern "C" int mon_check(void) {
    TestVpiHandle vh = vpi_handle_by_name((PLI_BYTE8*)"top.sig", NULL);
    CHECK_RESULT_NZ(vh);

    s_vpi_value val;
    val.format = vpiIntVal;
    val.value.integer = 0xCC;

    vpi_put_value(vh, &val, NULL, vpiNoDelay);

    return 0;
}

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
