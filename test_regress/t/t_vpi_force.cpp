// ======================================================================
// This file ONLY is placed under the Creative Commons Public Domain, for
// any use, without warranty.
// SPDX-License-Identifier: CC0-1.0
// ======================================================================

// DESCRIPTION: vpi force and release test
//
// This test checks that forcing a signal using vpi_put_value with vpiForceFlag
// sets it to the correct value, and then releasing it with vpiReleaseFlag
// returns it to the initial state. This is a basic test that just checks the
// correct behavior for a clocked register being forced with a VpiIntVal.

#include "verilated.h"  // For VL_PRINTF
#include "verilated_sym_props.h"  // For VerilatedVar
#include "verilated_syms.h"  // For VerilatedVarNameMap

#include "TestSimulator.h"  // For is_verilator()
#include "TestVpi.h"  // For CHECK_RESULT_NZ
#include "vpi_user.h"

#include <memory>  // For std::unique_ptr

constexpr PLI_INT32 forceValue = 0;
constexpr PLI_INT32 releaseValue = 1;
constexpr int maxAllowedErrorLevel = vpiWarning;
const std::string testSignalName = "t.test.clockedReg";

namespace {

bool vpiCheckErrorLevel(const int maxAllowedErrorLevel) {
    t_vpi_error_info errorInfo{};
    const bool errorOccured = vpi_chk_error(&errorInfo);
    if (VL_UNLIKELY(errorOccured)) {
        VL_PRINTF("%s", errorInfo.message);
        return errorInfo.level > maxAllowedErrorLevel;
    }
    return false;
}

std::pair<const std::string, const bool> vpiGetErrorMessage() {
    t_vpi_error_info errorInfo{};
    const bool errorOccured = vpi_chk_error(&errorInfo);
    return {errorInfo.message, errorOccured};
}

std::unique_ptr<const VerilatedVar> removeSignalFromScope(const std::string& scopeName,
                                                          const std::string& signalName) {
    const VerilatedScope* const scopep = Verilated::threadContextp()->scopeFind(scopeName.c_str());
    if (!scopep) return nullptr;
    VerilatedVarNameMap* const varsp = scopep->varsp();
    const VerilatedVarNameMap::const_iterator foundSignalIt = varsp->find(signalName.c_str());
    if (foundSignalIt == varsp->end()) return nullptr;
    VerilatedVar foundSignal = foundSignalIt->second;
    varsp->erase(foundSignalIt);
    return std::make_unique<const VerilatedVar>(foundSignal);
}

// WARNING: signalName's lifetime must be the same as the scopep, i.e. the same as the
// threadContextp! Otherwise, the key in the m_varsp map will be a stale pointer!
// => Only literals are used in this test
bool insertSignalIntoScope(const std::string& scopeName, const char* signalName,
                           std::unique_ptr<const VerilatedVar> signal) {
    const VerilatedScope* const scopep = Verilated::threadContextp()->scopeFind(scopeName.c_str());
    if (!scopep) return false;
    VerilatedVarNameMap* const varsp = scopep->varsp();
    varsp->insert(std::pair<const char*, VerilatedVar>{signalName, *signal});
    return true;
}

int tryVpiGetWithMissingSignal(vpiHandle const signalToGet,  // NOLINT(misc-misplaced-const)
                               const char* const scopeName, const char* const signalNameToRemove,
                               const std::string& expectedErrorMessage) {
    std::unique_ptr<const VerilatedVar> removedSignal
        = removeSignalFromScope(scopeName, signalNameToRemove);
    CHECK_RESULT_NZ(removedSignal);  // NOLINT(concurrency-mt-unsafe)

    s_vpi_value value_s{.format = vpiIntVal, .value = {.integer = 0}};

    // Prevent program from terminating, so error message can be collected
    Verilated::fatalOnVpiError(false);
    vpi_get_value(signalToGet, &value_s);
    // Re-enable so tests that should pass properly terminate the simulation on failure
    Verilated::fatalOnVpiError(true);

    std::pair<const std::string, const bool> receivedError = vpiGetErrorMessage();
    const bool errorOccurred = receivedError.second;
    const std::string receivedErrorMessage = receivedError.first;
    CHECK_RESULT_NZ(errorOccurred);  // NOLINT(concurrency-mt-unsafe)

    // NOLINTNEXTLINE(concurrency-mt-unsafe,performance-avoid-endl)
    CHECK_RESULT(receivedErrorMessage, expectedErrorMessage);
    bool insertSuccess
        = insertSignalIntoScope(scopeName, signalNameToRemove, std::move(removedSignal));
    CHECK_RESULT_NZ(insertSuccess);  // NOLINT(concurrency-mt-unsafe)
    return 0;
}

int tryVpiPutWithMissingSignal(const int valueToPut,
                               vpiHandle const signalToPut,  // NOLINT(misc-misplaced-const)
                               const int flag, const char* const scopeName,
                               const char* const signalNameToRemove,
                               const std::vector<std::string>& expectedErrorMessageSubstrings) {
    std::unique_ptr<const VerilatedVar> removedSignal
        = removeSignalFromScope(scopeName, signalNameToRemove);
    CHECK_RESULT_NZ(removedSignal);  // NOLINT(concurrency-mt-unsafe)

    s_vpi_value value_s{.format = vpiIntVal, .value = {.integer = valueToPut}};

    // Prevent program from terminating, so error message can be collected
    Verilated::fatalOnVpiError(false);
    vpi_put_value(signalToPut, &value_s, nullptr, flag);
    // Re-enable so tests that should pass properly terminate the simulation on failure
    Verilated::fatalOnVpiError(true);

    std::pair<const std::string, const bool> receivedError = vpiGetErrorMessage();
    const bool errorOccurred = receivedError.second;
    const std::string receivedErrorMessage = receivedError.first;
    CHECK_RESULT_NZ(errorOccurred);  // NOLINT(concurrency-mt-unsafe)

    const bool allExpectedErrorSubstringsFound
        = std::all_of(expectedErrorMessageSubstrings.begin(), expectedErrorMessageSubstrings.end(),
                      [receivedErrorMessage](const std::string& expectedSubstring) {
                          return receivedErrorMessage.find(expectedSubstring) != std::string::npos;
                      });
    CHECK_RESULT_NZ(allExpectedErrorSubstringsFound);  // NOLINT(concurrency-mt-unsafe)
    bool insertSuccess
        = insertSignalIntoScope(scopeName, signalNameToRemove, std::move(removedSignal));
    CHECK_RESULT_NZ(insertSuccess);  // NOLINT(concurrency-mt-unsafe)
    return 0;
}

int checkValues(int expectedValue) {
    vpiHandle const signalp  //NOLINT(misc-misplaced-const)
        = vpi_handle_by_name(const_cast<PLI_BYTE8*>(testSignalName.c_str()), nullptr);
    CHECK_RESULT_NZ(signalp);  // NOLINT(concurrency-mt-unsafe)

    // NOLINTNEXTLINE(concurrency-mt-unsafe);
    CHECK_RESULT_Z(tryVpiGetWithMissingSignal(
        signalp, "t.test", "clockedReg__VforceEn",
        "vl_vpi_get_value: Signal 't.test.clockedReg' is marked forceable, but force control "
        "signals could not be retrieved. Error message: getForceControlSignals: vpi force or "
        "release requested for 't.test.clockedReg', but vpiHandle '(nil)' of enable signal "
        "'t.test.clockedReg__VforceEn' could not be cast to VerilatedVpioVar*. Ensure signal "
        "is "
        "marked as forceable"));

    // NOLINTNEXTLINE(concurrency-mt-unsafe);
    CHECK_RESULT_Z(tryVpiGetWithMissingSignal(
        signalp, "t.test", "clockedReg__VforceVal",
        "vl_vpi_get_value: Signal 't.test.clockedReg' is marked forceable, but force control "
        "signals could not be retrieved. Error message: getForceControlSignals: vpi force or "
        "release requested for 't.test.clockedReg', but vpiHandle '(nil)' of value signal "
        "'t.test.clockedReg__VforceVal' could not be cast to VerilatedVpioVar*. Ensure signal "
        "is "
        "marked as forceable"));

    s_vpi_value value_s{.format = vpiIntVal, .value = {.integer = 0}};
    vpi_get_value(signalp, &value_s);
    const int signalValue = value_s.value.integer;

    // NOLINTNEXTLINE(concurrency-mt-unsafe);
    CHECK_RESULT_Z(vpiCheckErrorLevel(maxAllowedErrorLevel))
    // NOLINTNEXTLINE(concurrency-mt-unsafe, performance-avoid-endl);
    CHECK_RESULT(signalValue, expectedValue)

    return 0;
}

}  // namespace

extern "C" int baselineValue(void) { return releaseValue; }

extern "C" int checkValuesForced(void) { return checkValues(forceValue); }

extern "C" int checkValuesReleased(void) { return checkValues(releaseValue); }

extern "C" int forceValues(void) {
    if (!TestSimulator::is_verilator()) {
#ifdef VERILATOR
        printf("TestSimulator indicating not verilator, but VERILATOR macro is defined\n");
        return 1;
#endif
    }

    vpiHandle const signalp  //NOLINT(misc-misplaced-const)
        = vpi_handle_by_name(const_cast<PLI_BYTE8*>(testSignalName.c_str()), nullptr);
    CHECK_RESULT_NZ(signalp);  // NOLINT(concurrency-mt-unsafe)

    // NOLINTNEXTLINE(concurrency-mt-unsafe);
    CHECK_RESULT_Z(tryVpiPutWithMissingSignal(
        forceValue, signalp, vpiForceFlag, "t.test", "clockedReg__VforceEn",
        {"vpi_put_value: Signal 't.test.clockedReg' with vpiHandle ",
         // Exact handle address does not matter
         " is marked forceable, but force control signals could not be retrieved. Error "
         "message: getForceControlSignals: vpi force or release requested for "
         "'t.test.clockedReg', but vpiHandle '(nil)' of enable signal "
         "'t.test.clockedReg__VforceEn' could not be cast to VerilatedVpioVar*. Ensure "
         "signal is marked as forceable"}));

    // NOLINTNEXTLINE(concurrency-mt-unsafe);
    CHECK_RESULT_Z(tryVpiPutWithMissingSignal(
        forceValue, signalp, vpiForceFlag, "t.test", "clockedReg__VforceVal",
        {"vpi_put_value: Signal 't.test.clockedReg' with vpiHandle ",
         // Exact handle address does not matter
         " is marked forceable, but force control signals could not be retrieved. Error "
         "message: getForceControlSignals: vpi force or release requested for "
         "'t.test.clockedReg', but vpiHandle '(nil)' of value signal "
         "'t.test.clockedReg__VforceVal' could not be cast to VerilatedVpioVar*. Ensure "
         "signal is marked as forceable"}));

    s_vpi_value value_s{.format = vpiIntVal, .value = {.integer = forceValue}};
    vpi_put_value(signalp, &value_s, nullptr, vpiForceFlag);

    // NOLINTNEXTLINE(concurrency-mt-unsafe);
    CHECK_RESULT_Z(vpiCheckErrorLevel(maxAllowedErrorLevel))

    return 0;
}

extern "C" int releaseValues(void) {
    vpiHandle const signalp  //NOLINT(misc-misplaced-const)
        = vpi_handle_by_name(const_cast<PLI_BYTE8*>(testSignalName.c_str()), nullptr);
    CHECK_RESULT_NZ(signalp);  // NOLINT(concurrency-mt-unsafe)

    // NOLINTNEXTLINE(concurrency-mt-unsafe);
    CHECK_RESULT_Z(tryVpiPutWithMissingSignal(
        releaseValue, signalp, vpiReleaseFlag, "t.test", "clockedReg__VforceEn",
        {"vpi_put_value: Signal 't.test.clockedReg' with vpiHandle ",
         // Exact handle address does not matter
         " is marked forceable, but force control signals could not be retrieved. Error "
         "message: getForceControlSignals: vpi force or release requested for "
         "'t.test.clockedReg', but vpiHandle '(nil)' of enable signal "
         "'t.test.clockedReg__VforceEn' could not be cast to VerilatedVpioVar*. Ensure "
         "signal is marked as forceable"}));

    // NOLINTNEXTLINE(concurrency-mt-unsafe);
    CHECK_RESULT_Z(tryVpiPutWithMissingSignal(
        releaseValue, signalp, vpiReleaseFlag, "t.test", "clockedReg__VforceVal",
        {"vpi_put_value: Signal 't.test.clockedReg' with vpiHandle ",
         // Exact handle address does not matter
         " is marked forceable, but force control signals could not be retrieved. Error "
         "message: getForceControlSignals: vpi force or release requested for "
         "'t.test.clockedReg', but vpiHandle '(nil)' of value signal "
         "'t.test.clockedReg__VforceVal' could not be cast to VerilatedVpioVar*. Ensure "
         "signal is marked as forceable"}));

    s_vpi_value value_s{.format = vpiIntVal, .value = {.integer = releaseValue}};
    vpi_put_value(signalp, &value_s, nullptr, vpiReleaseFlag);

    // NOLINTNEXTLINE(concurrency-mt-unsafe);
    CHECK_RESULT_Z(vpiCheckErrorLevel(maxAllowedErrorLevel))
    // TODO: Correct value for value_s is not implemented yet in vpi_put_value with vpiReleaseFlag,
    // so for now it will always return the force value.

    // NOLINTNEXTLINE(concurrency-mt-unsafe, performance-avoid-endl);
    CHECK_RESULT(value_s.value.integer, forceValue)

    return 0;
}

#ifdef IS_VPI

static int baselineValueVpi() {
    TestVpiHandle href = vpi_handle(vpiSysTfCall, 0);
    s_vpi_value vpi_value;

    vpi_value.format = vpiIntVal;
    vpi_value.value.integer = baselineValue();
    vpi_put_value(href, &vpi_value, NULL, vpiNoDelay);

    return 0;
}

static int checkValuesForcedVpi() {
    TestVpiHandle href = vpi_handle(vpiSysTfCall, 0);
    s_vpi_value vpi_value;

    vpi_value.format = vpiIntVal;
    vpi_value.value.integer = checkValuesForced();
    vpi_put_value(href, &vpi_value, NULL, vpiNoDelay);

    return 0;
}

static int checkValuesReleasedVpi() {
    TestVpiHandle href = vpi_handle(vpiSysTfCall, 0);
    s_vpi_value vpi_value;

    vpi_value.format = vpiIntVal;
    vpi_value.value.integer = checkValuesReleased();
    vpi_put_value(href, &vpi_value, NULL, vpiNoDelay);

    return 0;
}

static int forceValuesVpi() {
    TestVpiHandle href = vpi_handle(vpiSysTfCall, 0);
    s_vpi_value vpi_value;

    vpi_value.format = vpiIntVal;
    vpi_value.value.integer = forceValues();
    vpi_put_value(href, &vpi_value, NULL, vpiNoDelay);

    return 0;
}

static int releaseValuesVpi() {
    TestVpiHandle href = vpi_handle(vpiSysTfCall, 0);
    s_vpi_value vpiValue;

    vpiValue.format = vpiIntVal;
    vpiValue.value.integer = releaseValues();
    vpi_put_value(href, &vpiValue, NULL, vpiNoDelay);

    return 0;
}

std::array<s_vpi_systf_data, 5> vpi_systf_data
    = {s_vpi_systf_data{vpiSysFunc, vpiIntFunc, (PLI_BYTE8*)"$forceValues",
                        (PLI_INT32(*)(PLI_BYTE8*))forceValuesVpi, 0, 0, 0},
       s_vpi_systf_data{vpiSysFunc, vpiIntFunc, (PLI_BYTE8*)"$releaseValues",
                        (PLI_INT32(*)(PLI_BYTE8*))releaseValuesVpi, 0, 0, 0},
       s_vpi_systf_data{vpiSysFunc, vpiIntFunc, (PLI_BYTE8*)"$checkValuesForced",
                        (PLI_INT32(*)(PLI_BYTE8*))checkValuesForcedVpi, 0, 0, 0},
       s_vpi_systf_data{vpiSysFunc, vpiIntFunc, (PLI_BYTE8*)"$checkValuesReleased",
                        (PLI_INT32(*)(PLI_BYTE8*))checkValuesReleasedVpi, 0, 0, 0},
       s_vpi_systf_data{vpiSysFunc, vpiIntFunc, (PLI_BYTE8*)"$baselineValue",
                        (PLI_INT32(*)(PLI_BYTE8*))baselineValueVpi, 0, 0, 0}};

// cver entry
extern "C" void vpi_compat_bootstrap(void) {
    for (s_vpi_systf_data& systf : vpi_systf_data) vpi_register_systf(&systf);
}

// icarus entry
void (*vlog_startup_routines[])() = {vpi_compat_bootstrap, 0};
#endif
