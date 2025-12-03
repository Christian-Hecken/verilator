// ======================================================================
// This file ONLY is placed under the Creative Commons Public Domain, for
// any use, without warranty.
// SPDX-License-Identifier: CC0-1.0
// ======================================================================

// DESCRIPTION: Test retrieving several strings using vpi_get_value
//
// This test checks that retrieving two strings using vpi_get_value yields independent values, i.e.
// the s_vpi_value.value.str does not point to the same char* across different invocations.

#include "TestVpi.h"  // For CHECK_RESULT_NZ
#include "vpi_user.h"

#include <cstring>

extern "C" int getBinStrings(void) {

    TestVpiHandle binStr1Handle
        = vpi_handle_by_name(const_cast<PLI_BYTE8*>("t.binString1"), nullptr);
    CHECK_RESULT_NZ(binStr1Handle);  // NOLINT(concurrency-mt-unsafe)
    TestVpiHandle binStr2Handle
        = vpi_handle_by_name(const_cast<PLI_BYTE8*>("t.binString2"), nullptr);
    CHECK_RESULT_NZ(binStr2Handle);  // NOLINT(concurrency-mt-unsafe)
    TestVpiHandle binStr3Handle
        = vpi_handle_by_name(const_cast<PLI_BYTE8*>("t.binString3"), nullptr);
    CHECK_RESULT_NZ(binStr3Handle);  // NOLINT(concurrency-mt-unsafe)
    TestVpiHandle decStr1Handle
        = vpi_handle_by_name(const_cast<PLI_BYTE8*>("t.decString1"), nullptr);
    CHECK_RESULT_NZ(decStr1Handle);  // NOLINT(concurrency-mt-unsafe)

    s_vpi_value receivedBinStr1ValueS{.format = vpiBinStrVal, .value = {}};
    vpi_get_value(binStr1Handle, &receivedBinStr1ValueS);
    CHECK_RESULT_Z(vpi_chk_error(nullptr));  // NOLINT(concurrency-mt-unsafe)

    s_vpi_value receivedBinStr2ValueS{.format = vpiBinStrVal, .value = {}};
    vpi_get_value(binStr2Handle, &receivedBinStr2ValueS);
    CHECK_RESULT_Z(vpi_chk_error(nullptr));  // NOLINT(concurrency-mt-unsafe)

    s_vpi_value receivedBinStr3ValueS{.format = vpiBinStrVal, .value = {}};
    vpi_get_value(binStr3Handle, &receivedBinStr3ValueS);
    CHECK_RESULT_Z(vpi_chk_error(nullptr));  // NOLINT(concurrency-mt-unsafe)

    s_vpi_value receivedDecStr1ValueS{.format = vpiDecStrVal, .value = {}};
    vpi_get_value(decStr1Handle, &receivedDecStr1ValueS);
    CHECK_RESULT_Z(vpi_chk_error(nullptr));  // NOLINT(concurrency-mt-unsafe)

    // NOLINTNEXTLINE(bugprone-suspicious-string-compare,concurrency-mt-unsafe)
    CHECK_RESULT_CSTR(receivedBinStr1ValueS.value.str, "10101010");
    // NOLINTNEXTLINE(bugprone-suspicious-string-compare,concurrency-mt-unsafe)
    CHECK_RESULT_CSTR(receivedBinStr2ValueS.value.str, "00001111");
    // NOLINTNEXTLINE(concurrency-mt-unsafe)
    CHECK_RESULT_NZ(receivedBinStr1ValueS.value.str != receivedBinStr3ValueS.value.str);
    // NOLINTNEXTLINE(bugprone-suspicious-string-compare,concurrency-mt-unsafe)
    CHECK_RESULT_CSTR(receivedDecStr1ValueS.value.str, "123");

    return 0;
}
