// ======================================================================
// This file ONLY is placed under the Creative Commons Public Domain, for
// any use, without warranty.
// SPDX-License-Identifier: CC0-1.0
// ======================================================================

#include "TestSimulator.h"  // For CHECK_RESULT
#include "TestVpi.h"  // For TestVpiHandle
#include "vpi_user.h"

extern "C" int checkForcedSignal() {
    TestVpiHandle signalHandle{vpi_handle_by_name(const_cast<PLI_BYTE8*>("t.data"), nullptr)};
    CHECK_RESULT_NZ(signalHandle);

    s_vpi_value result{.format = vpiIntVal, .value = {.integer = 1}};
    vpi_get_value(signalHandle, &result);
    CHECK_RESULT(result.value.integer, 0);
    return 0;
}
