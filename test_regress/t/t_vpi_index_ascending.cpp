#include "verilated.h"  // For VL_PRINTF

#include "TestVpi.h"  // For CHECK_RESULT_NZ
#include "vpi_user.h"

const std::vector<std::string> signalNames = {"t.ascPackedC",  //
                                              "t.ascPackedS",  //
                                              "t.ascPackedI",  //
                                              "t.ascPackedQ",  //
                                              "t.ascPackedW",  //
                                              "t.descPackedC",  //
                                              "t.descPackedS",  //
                                              "t.descPackedI",  //
                                              "t.descPackedQ",  //
                                              "t.descPackedW",  //
                                              "t.ascUnpackedC",  //
                                              "t.ascUnpackedS",  //
                                              "t.ascUnpackedI",  //
                                              "t.ascUnpackedQ",  //
                                              "t.ascUnpackedW",  //
                                              "t.descUnpackedC",  //
                                              "t.descUnpackedS",  //
                                              "t.descUnpackedI",  //
                                              "t.descUnpackedQ",  //
                                              "t.descUnpackedW"};

constexpr int idx = -1;

extern "C" int printValues() {
    for (const auto& signalName : signalNames) {
        TestVpiHandle baseSignalHandle
            = vpi_handle_by_name(const_cast<PLI_BYTE8*>(signalName.c_str()), nullptr);
        TestVpiHandle indexedHandle = vpi_handle_by_index(baseSignalHandle, idx);
        CHECK_RESULT_NZ(indexedHandle);

        int size = vpi_get(vpiSize, indexedHandle);
        s_vpi_value value;
        value.format = vpiIntVal;
        vpi_get_value(indexedHandle, &value);
        VL_PRINTF("[VPI] %s : %x, size %d\n",
                  (signalName + "[" + std::to_string(idx) + "]").c_str(), value.value.integer,
                  size);
    }

    return 0;
}

extern "C" int printSingleElements() {
    for (const auto& signalName : signalNames) {
        TestVpiHandle baseSignalHandle
            = vpi_handle_by_name(const_cast<PLI_BYTE8*>(signalName.c_str()), nullptr);
        TestVpiHandle firstDimIndexedHandle = vpi_handle_by_index(baseSignalHandle, -2);
        CHECK_RESULT_NZ(firstDimIndexedHandle);
        TestVpiHandle indexedHandle = vpi_handle_by_index(firstDimIndexedHandle, 1);
        CHECK_RESULT_NZ(indexedHandle);

        int size = vpi_get(vpiSize, indexedHandle);
        s_vpi_value value;
        value.format = vpiIntVal;
        vpi_get_value(indexedHandle, &value);
        VL_PRINTF("[VPI] %s : %x, size %d\n", (signalName + "[-2][1]").c_str(),
                  value.value.integer, size);
    }
    return 0;
}

#ifdef IS_VPI
static int printValuesVpi(PLI_BYTE8*) {
    TestVpiHandle href = vpi_handle(vpiSysTfCall, 0);
    s_vpi_value vpi_value;

    vpi_value.format = vpiIntVal;
    vpi_value.value.integer = printValues();
    vpi_put_value(href, &vpi_value, NULL, vpiNoDelay);

    return 0;
}

static int printSingleElementsVpi(PLI_BYTE8*) {
    TestVpiHandle href = vpi_handle(vpiSysTfCall, 0);
    s_vpi_value vpi_value;

    vpi_value.format = vpiIntVal;
    vpi_value.value.integer = printSingleElements();
    vpi_put_value(href, &vpi_value, NULL, vpiNoDelay);

    return 0;
}

std::array<s_vpi_systf_data, 2> vpi_systf_data
    = {s_vpi_systf_data{vpiSysFunc, vpiIntFunc, (PLI_BYTE8*)"$printValues",
                        (PLI_INT32(*)(PLI_BYTE8*))printValuesVpi, 0, 0, 0},
       s_vpi_systf_data{vpiSysFunc, vpiIntFunc, (PLI_BYTE8*)"$printSingleElements",
                        (PLI_INT32(*)(PLI_BYTE8*))printSingleElementsVpi, 0, 0, 0}};

extern "C" void vpi_compat_bootstrap(void) {
    for (s_vpi_systf_data& systf : vpi_systf_data) vpi_register_systf(&systf);
}

void (*vlog_startup_routines[])() = {vpi_compat_bootstrap, 0};
#endif
