#include "verilated.h"
#include "verilated_vpi.h"

#include "TestVpi.h"
#include "vpi_user.h"

extern "C" int mon_check(void) {
    TestVpiHandle vh = vpi_handle_by_name((PLI_BYTE8*)"top.r", NULL);
    CHECK_RESULT_NZ(vh);

    s_vpi_value val;
    val.format = vpiIntVal;
    val.value.integer = 1;

    vpi_put_value(vh, &val, NULL, vpiForceFlag);
    vpi_put_value(vh, &val, NULL, vpiReleaseFlag);
    vpi_get_value(vh, &val);

    CHECK_RESULT(val.value.integer, 1);
    return 0;
}

// called after an SV force/release sequence; just read the value
extern "C" int sv_check(void) {
    TestVpiHandle vh = vpi_handle_by_name((PLI_BYTE8*)"top.r", NULL);
    CHECK_RESULT_NZ(vh);

    s_vpi_value val;
    val.format = vpiIntVal;
    vpi_get_value(vh, &val);
    CHECK_RESULT(val.value.integer, 1);
    return 0;
}
