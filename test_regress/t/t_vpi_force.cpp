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

#include <algorithm>
#include <memory>  // For std::unique_ptr

namespace {

constexpr int maxAllowedErrorLevel = vpiWarning;
const std::string scopeName = "t.test";

// TODO: Rename
using signalValueTypes = union {
    const char* str;  // TODO: Maybe turn into string in case the array can't be kept constexpr
    PLI_INT32 integer;
    double real;
    const struct t_vpi_vecval* vector;
};

using TestSignal = const struct {
    const char* signalName;
    PLI_INT32 valueType;
    signalValueTypes releaseValue;
    signalValueTypes forceValue;
    std::pair<signalValueTypes, bool>
        partialForceValue;  // TODO: Explain reason for bool (DIY optional)
};

constexpr std::array<TestSignal, 11> TestSignals = {
    TestSignal{"onebit", vpiIntVal, {.integer = 1}, {.integer = 0}, {{}, false}},
    TestSignal{"intval",
               vpiIntVal,
               {.integer = -1431655766},
               {.integer = 0x55555555},
               {{.integer = -1431677611}, true}},  // TODO: Explain values

    TestSignal{
        "quad",
        vpiVectorVal,
        // NOTE: This is a 62 bit signal, so the first two bits of the MSBs (*second* vecval,
        // since the LSBs come first) are set to 0, hence the 0x2 and 0x1, respectively.
        {.vector = (t_vpi_vecval[]){{0xAAAAAAAAUL, 0}, {0x2AAAAAAAUL, 0}}},
        {.vector = (t_vpi_vecval[]){{0x55555555UL, 0}, {0x15555555UL, 0}}},
        {{.vector = (t_vpi_vecval[]){{0xD5555555UL, 0}, {0x2AAAAAAAUL, 0}}}, true}},

    TestSignal{"real1",
               vpiRealVal,
               {.real = 1.0},
               {.real = 123456.789},
               {{}, false}},  // TODO: Explain why no partial force

    TestSignal{"textHalf", vpiStringVal, {.str = "Hf"}, {.str = "T2"}, {{.str = "H2"}, true}},
    TestSignal{"textLong",
               vpiStringVal,
               {.str = "Long64b"},
               {.str = "44Four44"},
               {{.str = "Lonur44"}, true}},
    TestSignal{"text",
               vpiStringVal,
               {.str = "Verilog Test module"},
               {.str = "lorem ipsum"},
               {{.str = "Verilog Tesem ipsum"}, true}},

    TestSignal{"binString",
               vpiBinStrVal,
               {.str = "10101010"},
               {.str = "01010101"},
               {{.str = "10100101"}, true}},
    TestSignal{"octString",
               vpiOctStrVal,
               {.str = "25252"},
               {.str = "52525"},
               {{.str = "25325"}, true}},  // TODO: Explain
    TestSignal{"decString",
               vpiDecStrVal,
               {.str = "12297829382473034410"},
               {.str = "6148914691236517205"},
               {{.str = "12297829381041378645"}, true}},
    TestSignal{"hexString",
               vpiHexStrVal,
               {.str = "aaaaaaaaaaaaaaaa"},
               {.str = "5555555555555555"},
               {{.str = "aaaaaaaa55555555"}, true}},
};

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
    return {errorOccured ? errorInfo.message : std::string{}, errorOccured};
}

#ifdef VERILATOR  // m_varsp is Verilator-specific and does not make sense for other simulators
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

bool insertSignalIntoScope(const std::pair<std::string, std::string>& scopeAndSignalNames,
                           std::unique_ptr<const VerilatedVar> signal) {
    const std::string& scopeName = scopeAndSignalNames.first;
    const std::string& signalName = scopeAndSignalNames.second;

    const VerilatedScope* const scopep = Verilated::threadContextp()->scopeFind(scopeName.c_str());
    if (!scopep) return false;
    VerilatedVarNameMap* const varsp = scopep->varsp();

    // NOTE: The lifetime of the name inserted into varsp must be the same as the scopep, i.e. the
    // same as threadContextp. Otherwise, the key in the m_varsp map will be a stale pointer.
    // Hence, names of signals being inserted are stored in the static set, and it is assumed that
    // the set's lifetime is the same as the threadContextp.
    static std::set<std::string> insertedSignalNames;
    const std::pair<std::set<std::string>::const_iterator, bool> insertedSignalName
        = insertedSignalNames.insert(signalName);

    varsp->insert(
        std::pair<const char*, VerilatedVar>{insertedSignalName.first->c_str(), *signal});
    return true;
}

int tryVpiGetWithMissingSignal(const TestVpiHandle& signalToGet,  // NOLINT(misc-misplaced-const)
                               PLI_INT32 signalFormat,
                               const std::pair<std::string, std::string>& scopeAndSignalNames,
                               const std::string& expectedErrorMessage) {
    const std::string& scopeName = scopeAndSignalNames.first;
    const std::string& signalNameToRemove = scopeAndSignalNames.second;
    std::unique_ptr<const VerilatedVar> removedSignal
        = removeSignalFromScope(scopeName, signalNameToRemove);
    CHECK_RESULT_NZ(removedSignal);  // NOLINT(concurrency-mt-unsafe)

    s_vpi_value value_s{.format = signalFormat, .value = {}};

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
        = insertSignalIntoScope({scopeName, signalNameToRemove}, std::move(removedSignal));
    CHECK_RESULT_NZ(insertSuccess);  // NOLINT(concurrency-mt-unsafe)
    return 0;
}

int tryVpiPutWithMissingSignal(const s_vpi_value value_s,
                               const TestVpiHandle& signalToPut,  // NOLINT(misc-misplaced-const)
                               const int flag, const std::string& scopeName,
                               const std::string& signalNameToRemove,
                               const std::vector<std::string>& expectedErrorMessageSubstrings) {
    std::unique_ptr<const VerilatedVar> removedSignal
        = removeSignalFromScope(scopeName, signalNameToRemove);
    CHECK_RESULT_NZ(removedSignal);  // NOLINT(concurrency-mt-unsafe)

    // Prevent program from terminating, so error message can be collected
    Verilated::fatalOnVpiError(false);
    vpi_put_value(signalToPut, const_cast<p_vpi_value>(&value_s), nullptr, flag);
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
        = insertSignalIntoScope({scopeName, signalNameToRemove}, std::move(removedSignal));
    CHECK_RESULT_NZ(insertSuccess);  // NOLINT(concurrency-mt-unsafe)
    return 0;
}
#endif

bool vpiValuesEqual(const std::size_t bitCount, const s_vpi_value& first,
                    const s_vpi_value& second) {
    if (first.format != second.format) return false;
    switch (first.format) {
    case vpiIntVal: return first.value.integer == second.value.integer; break;
    case vpiVectorVal: {
        const t_vpi_vecval* const first_vecval = first.value.vector;
        const t_vpi_vecval* const second_vecval = second.value.vector;
        const std::size_t vectorElements = (bitCount + 31) / 32;  // Ceil
        for (std::size_t i{0}; i < vectorElements; ++i) {
            if (first_vecval[i].aval != second_vecval[i].aval) return false;
        }
        return true;
    }
    case vpiRealVal:
        return std::abs(first.value.real - second.value.real)
               < std::numeric_limits<double>::epsilon();
        break;
    case vpiStringVal:
    case vpiBinStrVal:
    case vpiOctStrVal:
    case vpiDecStrVal:
    case vpiHexStrVal: {
        // TODO: Understand what this does:
        // #define CHECK_RESULT_CSTR_STRIP(got, exp) CHECK_RESULT_CSTR(got + strspn(got, " "), exp)
        const std::string fixed_received = first.value.str + std::strspn(first.value.str, " ");
        return std::string{fixed_received} == std::string{second.value.str};
        break;
        // return std::strcmp(first.value.str, second.value.str) == 0; break; // TODO: REMOVE
    }
    default:
        VL_PRINTF("Unsupported value format %i passed to vpiValuesEqual\n", first.format);
        return false;
    }
}

std::unique_ptr<s_vpi_value> vpiValueWithFormat(PLI_INT32 signalFormat, signalValueTypes value) {
    std::unique_ptr<s_vpi_value> value_sp = std::make_unique<s_vpi_value>();
    value_sp->format = signalFormat;

    switch (signalFormat) {
    case vpiIntVal: value_sp->value = {.integer = value.integer}; break;
    case vpiVectorVal: value_sp->value = {.vector = const_cast<p_vpi_vecval>(value.vector)}; break;
    case vpiRealVal: value_sp->value = {.real = value.real}; break;
    case vpiStringVal:
    case vpiBinStrVal:
    case vpiOctStrVal:
    case vpiDecStrVal:
    case vpiHexStrVal: value_sp->value = {.str = const_cast<PLI_BYTE8*>(value.str)}; break;
    default:
        VL_PRINTF("Unsupported value format %i passed to vpiValueWithFormat\n", signalFormat);
        return nullptr;
    }

    return value_sp;
}

int checkValue(const std::string& scopeName, const std::string& testSignalName,
               PLI_INT32 signalFormat, signalValueTypes expectedValue) {
    const std::string testSignalFullName
        = std::string{scopeName} + "." + std::string{testSignalName};
    TestVpiHandle const signalHandle  //NOLINT(misc-misplaced-const)
        = vpi_handle_by_name(const_cast<PLI_BYTE8*>(testSignalFullName.c_str()), nullptr);
    CHECK_RESULT_NZ(signalHandle);  // NOLINT(concurrency-mt-unsafe)

#ifdef VERILATOR
    // NOLINTNEXTLINE(concurrency-mt-unsafe)
    CHECK_RESULT_Z(tryVpiGetWithMissingSignal(
        signalHandle, signalFormat, {scopeName, testSignalName + "__VforceEn"},
        "vl_vpi_get_value: Signal '" + testSignalFullName
            + "' is marked forceable, but force control signals could not be retrieved. Error "
              "message: getForceControlSignals: vpi force or release requested for '"
            + testSignalFullName + "', but vpiHandle '(nil)' of enable signal '"
            + testSignalFullName
            + "__VforceEn' could not be cast to VerilatedVpioVar*. Ensure signal is marked as "
              "forceable"));

    // NOLINTNEXTLINE(concurrency-mt-unsafe)
    CHECK_RESULT_Z(tryVpiGetWithMissingSignal(
        signalHandle, signalFormat, {scopeName, testSignalName + "__VforceVal"},
        "vl_vpi_get_value: Signal '" + testSignalFullName
            + "' is marked forceable, but force control signals could not be retrieved. Error "
              "message: getForceControlSignals: vpi force or release requested for '"
            + testSignalFullName + "', but vpiHandle '(nil)' of value signal '"
            + testSignalFullName
            + "__VforceVal' could not be cast to VerilatedVpioVar*. Ensure signal is marked "
              "as "
              "forceable"));
#endif

    std::unique_ptr<s_vpi_value> receivedValueSp = vpiValueWithFormat(signalFormat, {});
    CHECK_RESULT_NZ(receivedValueSp);  // NOLINT(concurrency-mt-unsafe)
    vpi_get_value(signalHandle, receivedValueSp.get());

    // NOLINTNEXTLINE(concurrency-mt-unsafe)
    CHECK_RESULT_Z(vpiCheckErrorLevel(maxAllowedErrorLevel))

    std::unique_ptr<s_vpi_value> expectedValueSp = vpiValueWithFormat(signalFormat, expectedValue);
    CHECK_RESULT_NZ(expectedValueSp);  // NOLINT(concurrency-mt-unsafe)
    // NOLINTNEXTLINE(concurrency-mt-unsafe)
    CHECK_RESULT_NZ(
        vpiValuesEqual(vpi_get(vpiSize, signalHandle), *receivedValueSp, *expectedValueSp));

    return 0;
}

int forceSignal(const std::string& scopeName, const std::string& testSignalName,
                PLI_INT32 signalFormat, signalValueTypes forceValue) {
    const std::string testSignalFullName
        = std::string{scopeName} + "." + std::string{testSignalName};
    TestVpiHandle const signalHandle  //NOLINT(misc-misplaced-const)
        = vpi_handle_by_name(const_cast<PLI_BYTE8*>(testSignalFullName.c_str()), nullptr);
    CHECK_RESULT_NZ(signalHandle);  // NOLINT(concurrency-mt-unsafe)

    std::unique_ptr<s_vpi_value> value_sp = vpiValueWithFormat(signalFormat, forceValue);
    CHECK_RESULT_NZ(value_sp);  // NOLINT(concurrency-mt-unsafe)

#ifdef VERILATOR
    // NOLINTNEXTLINE(concurrency-mt-unsafe)
    CHECK_RESULT_Z(tryVpiPutWithMissingSignal(
        *value_sp, signalHandle, vpiForceFlag, scopeName, testSignalName + "__VforceEn",
        {"vpi_put_value: Signal '" + testSignalFullName + "' with vpiHandle ",
         // Exact handle address does not matter
         " is marked forceable, but force control signals could not be retrieved. Error "
         "message: getForceControlSignals: vpi force or release requested for '"
             + testSignalFullName + "', but vpiHandle '(nil)' of enable signal '"
             + testSignalFullName
             + "__VforceEn' could not be cast to VerilatedVpioVar*. Ensure signal is marked "
               "as "
               "forceable"}));

    // NOLINTNEXTLINE(concurrency-mt-unsafe)
    CHECK_RESULT_Z(tryVpiPutWithMissingSignal(
        *value_sp, signalHandle, vpiForceFlag, scopeName, testSignalName + "__VforceVal",
        {"vpi_put_value: Signal '" + testSignalFullName + "' with vpiHandle ",
         // Exact handle address does not matter
         " is marked forceable, but force control signals could not be retrieved. Error "
         "message: getForceControlSignals: vpi force or release requested for '"
             + testSignalFullName + "', but vpiHandle '(nil)' of value signal '"
             + testSignalFullName
             + "__VforceVal' could not be cast to VerilatedVpioVar*. Ensure signal is marked "
               "as "
               "forceable"}));
#endif

    vpi_put_value(signalHandle, value_sp.get(), nullptr, vpiForceFlag);

    // NOLINTNEXTLINE(concurrency-mt-unsafe)
    CHECK_RESULT_Z(vpiCheckErrorLevel(maxAllowedErrorLevel))

    return 0;
}

int releaseSignal(const std::string& scopeName, const std::string& testSignalName,
                  PLI_INT32 signalFormat,
                  signalValueTypes forceValue) {  // TODO: const correctness
    const std::string testSignalFullName
        = std::string{scopeName} + "." + std::string{testSignalName};
    TestVpiHandle const signalHandle  //NOLINT(misc-misplaced-const)
        = vpi_handle_by_name(const_cast<PLI_BYTE8*>(testSignalFullName.c_str()), nullptr);
    CHECK_RESULT_NZ(signalHandle);  // NOLINT(concurrency-mt-unsafe)

    // initialize value_sp to forceValue to ensure it is updated to releaseValue
    std::unique_ptr<s_vpi_value> value_sp = vpiValueWithFormat(signalFormat, forceValue);
    CHECK_RESULT_NZ(value_sp);  //NOLINT(concurrency-mt-unsafe)

#ifdef VERILATOR
    // NOLINTNEXTLINE(concurrency-mt-unsafe)
    CHECK_RESULT_Z(tryVpiPutWithMissingSignal(
        *value_sp, signalHandle, vpiReleaseFlag, scopeName, testSignalName + "__VforceEn",
        {"vpi_put_value: Signal '" + testSignalFullName + "' with vpiHandle ",
         // Exact handle address does not matter
         " is marked forceable, but force control signals could not be retrieved. Error "
         "message: getForceControlSignals: vpi force or release requested for '"
             + testSignalFullName + "', but vpiHandle '(nil)' of enable signal '"
             + testSignalFullName
             + "__VforceEn' could not be cast to VerilatedVpioVar*. Ensure signal is marked "
               "as "
               "forceable"}));

    // NOLINTNEXTLINE(concurrency-mt-unsafe)
    CHECK_RESULT_Z(tryVpiPutWithMissingSignal(
        *value_sp, signalHandle, vpiReleaseFlag, scopeName, testSignalName + "__VforceVal",
        {"vpi_put_value: Signal '" + testSignalFullName + "' with vpiHandle ",
         // Exact handle address does not matter
         " is marked forceable, but force control signals could not be retrieved. Error "
         "message: getForceControlSignals: vpi force or release requested for '"
             + testSignalFullName + "', but vpiHandle '(nil)' of value signal '"
             + testSignalFullName
             + "__VforceVal' could not be cast to VerilatedVpioVar*. Ensure signal is marked "
               "as "
               "forceable"}));
#endif

    vpi_put_value(signalHandle, value_sp.get(), nullptr, vpiReleaseFlag);

    // NOLINTNEXTLINE(concurrency-mt-unsafe)
    CHECK_RESULT_Z(vpiCheckErrorLevel(maxAllowedErrorLevel))
    // TODO: Correct value for value_s is not implemented yet in vpi_put_value with vpiReleaseFlag,
    // so for now it will always return the force value.

    // NOLINTNEXTLINE(concurrency-mt-unsafe, performance-avoid-endl)
    // CHECK_RESULT(value_s.value.integer, forceValue) // TODO

    return 0;
}

}  // namespace

extern "C" int checkValuesForced(void) {
    // TODO: The any_of functions just return 0 or 1, but we really want to return the line number!
    return std::any_of(TestSignals.begin(), TestSignals.end(), [](const TestSignal& signal) {
        CHECK_RESULT_Z(  // NOLINT(concurrency-mt-unsafe)
            checkValue(scopeName, signal.signalName, signal.valueType, signal.forceValue));
        return 0;
    });
}

extern "C" int checkValuesPartiallyForced(void) {
    return std::any_of(TestSignals.begin(), TestSignals.end(), [](const TestSignal& signal) {
        if (signal.partialForceValue.second)
            CHECK_RESULT_Z(  // NOLINT(concurrency-mt-unsafe)
                checkValue(scopeName, signal.signalName, signal.valueType,
                           signal.partialForceValue.first));
        return 0;
    });
}

extern "C" int checkValuesReleased(void) {
    return std::any_of(TestSignals.begin(), TestSignals.end(), [](const TestSignal& signal) {
        CHECK_RESULT_Z(  // NOLINT(concurrency-mt-unsafe)
            checkValue(scopeName, signal.signalName, signal.valueType, signal.releaseValue));
        return 0;
    });
}

#ifdef VERILATOR
// These functions only make sense with Verilator, because other simulators either support the
// functionality (e.g. forcing unpacked signals) or fail at elaboration time (e.g. trying to force
// a string). The checks for error messages are specific to verilated_vpi.cpp.

extern "C" int tryCheckingForceableString(void) {
    const std::string forceableStringName = std::string{scopeName} + ".str1";
    TestVpiHandle const stringSignalHandle  //NOLINT(misc-misplaced-const)
        = vpi_handle_by_name(const_cast<PLI_BYTE8*>(forceableStringName.c_str()), nullptr);

    s_vpi_value value_s{.format = vpiStringVal, .value = {}};

    // Prevent program from terminating, so error message can be collected
    Verilated::fatalOnVpiError(false);
    vpi_get_value(stringSignalHandle, &value_s);
    // Re-enable so tests that should pass properly terminate the simulation on failure
    Verilated::fatalOnVpiError(true);

    std::pair<const std::string, const bool> receivedError = vpiGetErrorMessage();
    const bool errorOccurred = receivedError.second;
    const std::string receivedErrorMessage = receivedError.first;
    CHECK_RESULT_NZ(errorOccurred);  // NOLINT(concurrency-mt-unsafe)

    const std::string expectedErrorMessage
        = "attempting to retrieve value of forceable signal " + forceableStringName
          + " with data type VLVT_STRING, but strings cannot be forced.";
    // NOLINTNEXTLINE(concurrency-mt-unsafe,performance-avoid-endl)
    CHECK_RESULT(receivedErrorMessage, expectedErrorMessage);
    return 0;
}

extern "C" int tryForcingUnpackedSignal(void) {
    const std::string forceableUnpackedSignalName = std::string{scopeName} + ".unpacked";
    TestVpiHandle const signalHandle  //NOLINT(misc-misplaced-const)
        = vpi_handle_by_name(const_cast<PLI_BYTE8*>(forceableUnpackedSignalName.c_str()), nullptr);

    s_vpi_value value_s{.format = vpiIntVal, .value = {}};

    // Prevent program from terminating, so error message can be collected
    Verilated::fatalOnVpiError(false);
    vpi_put_value(signalHandle, &value_s, nullptr, 0);
    // Re-enable so tests that should pass properly terminate the simulation on failure
    Verilated::fatalOnVpiError(true);

    std::pair<const std::string, const bool> receivedError = vpiGetErrorMessage();
    const bool errorOccurred = receivedError.second;
    const std::string receivedErrorMessage = receivedError.first;
    CHECK_RESULT_NZ(errorOccurred);  // NOLINT(concurrency-mt-unsafe)

    const std::string expectedErrorMessage
        = "vpi_put_value: Signal " + forceableUnpackedSignalName
          + " is marked as forceable, but forcing is not supported for unpacked arrays (#4735).";
    // NOLINTNEXTLINE(concurrency-mt-unsafe,performance-avoid-endl)
    CHECK_RESULT(receivedErrorMessage, expectedErrorMessage);
    return 0;
}

extern "C" int tryCheckingUnpackedSignal(void) {
    const std::string forceableUnpackedSignalName = std::string{scopeName} + ".unpacked";
    TestVpiHandle const signalHandle  //NOLINT(misc-misplaced-const)
        = vpi_handle_by_name(const_cast<PLI_BYTE8*>(forceableUnpackedSignalName.c_str()), nullptr);

    s_vpi_value value_s{.format = vpiIntVal, .value = {}};

    // Prevent program from terminating, so error message can be collected
    Verilated::fatalOnVpiError(false);
    vpi_get_value(signalHandle, &value_s);
    // Re-enable so tests that should pass properly terminate the simulation on failure
    Verilated::fatalOnVpiError(true);

    std::pair<const std::string, const bool> receivedError = vpiGetErrorMessage();
    const bool errorOccurred = receivedError.second;
    const std::string receivedErrorMessage = receivedError.first;
    CHECK_RESULT_NZ(errorOccurred);  // NOLINT(concurrency-mt-unsafe)

    const std::string expectedErrorMessage
        = "vl_vpi_get_value: Signal " + forceableUnpackedSignalName
          + " is marked as forceable, but forcing is not supported for unpacked arrays (#4735).";
    // NOLINTNEXTLINE(concurrency-mt-unsafe,performance-avoid-endl)
    CHECK_RESULT(receivedErrorMessage, expectedErrorMessage);
    return 0;
}
#endif

extern "C" int forceValues(void) {
    if (!TestSimulator::is_verilator()) {
#ifdef VERILATOR
        printf("TestSimulator indicating not verilator, but VERILATOR macro is defined\n");
        return 1;
#endif
    }

    return std::any_of(TestSignals.begin(), TestSignals.end(), [](const TestSignal& signal) {
        CHECK_RESULT_Z(  // NOLINT(concurrency-mt-unsafe)
            forceSignal(scopeName, signal.signalName, signal.valueType, signal.forceValue));
        return 0;
    });
}

extern "C" int releaseValues(void) {
    return std::any_of(TestSignals.begin(), TestSignals.end(), [](const TestSignal& signal) {
        CHECK_RESULT_Z(  // NOLINT(concurrency-mt-unsafe)
            releaseSignal(scopeName, signal.signalName, signal.valueType, signal.forceValue));
        return 0;
    });
}

#ifdef IS_VPI

static int checkValuesForcedVpi() {
    TestVpiHandle href = vpi_handle(vpiSysTfCall, 0);
    s_vpi_value vpi_value;

    vpi_value.format = vpiIntVal;
    vpi_value.value.integer = checkValuesForced();
    vpi_put_value(href, &vpi_value, NULL, vpiNoDelay);

    return 0;
}

static int checkValuesPartiallyForcedVpi() {
    TestVpiHandle href = vpi_handle(vpiSysTfCall, 0);
    s_vpi_value vpi_value;

    vpi_value.format = vpiIntVal;
    vpi_value.value.integer = checkValuesPartiallyForced();
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
       s_vpi_systf_data{vpiSysFunc, vpiIntFunc, (PLI_BYTE8*)"$checkValuesPartiallyForced",
                        (PLI_INT32(*)(PLI_BYTE8*))checkValuesPartiallyForcedVpi, 0, 0, 0},
       s_vpi_systf_data{vpiSysFunc, vpiIntFunc, (PLI_BYTE8*)"$checkValuesReleased",
                        (PLI_INT32(*)(PLI_BYTE8*))checkValuesReleasedVpi, 0, 0, 0}};

// cver entry
extern "C" void vpi_compat_bootstrap(void) {
    for (s_vpi_systf_data& systf : vpi_systf_data) vpi_register_systf(&systf);
}

// icarus entry
void (*vlog_startup_routines[])() = {vpi_compat_bootstrap, 0};
#endif
