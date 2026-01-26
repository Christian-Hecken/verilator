#include "verilated.h"
#include "verilated_vpi.h"

#include "TestVpi.h"
#include "vpi_user.h"

#include <cstdint>
#include <memory>

extern "C" {
#include <libgen.h>
}

int set_and_get_signal(const char* name) {
    TestVpiHandle vh = vpi_handle_by_name((PLI_BYTE8*)name, nullptr);
    CHECK_RESULT_NZ(vh);

    s_vpi_value v;
    v.format = vpiIntVal;
    v.value.integer = 1;
    vpi_put_value(vh, &v, nullptr, vpiNoDelay);
    vpi_get_value(vh, &v);
    CHECK_RESULT(v.value.integer, 1);
    return 0;
}

extern "C" int set_onebit_dpi_import(p_cb_data = nullptr) {
    return set_and_get_signal("t.onebit_dpi_import_nodelay");
}

extern "C" PLI_INT32 set_onebit_vpi_sysfun(PLI_BYTE8*) {
    return set_and_get_signal("t.onebit_vpi_sysfun_nodelay");
}

PLI_INT32 set_onebit_vpi_delayed_callback_vpiNoDelay(t_cb_data*) {
    return set_and_get_signal("t.onebit_vpi_delayed_callback_vpiNoDelay");
}

PLI_INT32 set_onebit_vpi_immediate_callback_inertialdelay(t_cb_data*) {
    TestVpiHandle vh
        = vpi_handle_by_name((PLI_BYTE8*)"t.onebit_vpi_immediate_callback_inertialdelay", nullptr);
    CHECK_RESULT_NZ(vh);

    s_vpi_time t{.type = vpiSimTime, .high = 0, .low = 1, .real{}};
    s_vpi_value v;
    v.format = vpiIntVal;
    v.value.integer = 1;
    vpi_put_value(vh, &v, &t, vpiInertialDelay);
    return 0;
}

static void register_cbs(void) {
    s_vpi_time t{.type = vpiSimTime, .high = 0, .low = 1, .real{}};

    s_cb_data cbd;
    cbd.reason = cbAfterDelay;
    cbd.cb_rtn = set_onebit_vpi_delayed_callback_vpiNoDelay;
    cbd.time = &t;
    vpi_register_cb(&cbd);

    cbd.reason = cbStartOfSimulation;
    t.low = 0;
    cbd.time = &t;
    cbd.cb_rtn = set_onebit_vpi_immediate_callback_inertialdelay;
    vpi_register_cb(&cbd);

    return;
}

void register_systfs() {
    static s_vpi_systf_data set_onebit_vpi_sysfun_data{.type = vpiSysFunc,
                                                       .sysfunctype = vpiIntFunc,
                                                       .tfname
                                                       = (PLI_BYTE8*)"$set_onebit_vpi_sysfun",
                                                       .calltf = set_onebit_vpi_sysfun,
                                                       .compiletf = nullptr,
                                                       .sizetf = nullptr,
                                                       .user_data = nullptr};
    vpi_register_systf(&set_onebit_vpi_sysfun_data);
}

void (*vlog_startup_routines[])(void) = {register_systfs, register_cbs, 0};

// Copied from TestVpiMain.cpp; unchanged other than calling register_cbs
#ifndef IS_VPI

#include "Vt_vpi_nodelay_put_get.h"
static bool settle_value_callbacks() {
    bool cbs_called;
    bool again;

    // Call Value Change callbacks
    // These can modify signal values so we loop
    // until there are no more changes
    cbs_called = again = VerilatedVpi::callValueCbs();
    while (again) { again = VerilatedVpi::callValueCbs(); }

    return cbs_called;
}

int main(int argc, char** argv) {
    const std::unique_ptr<VerilatedContext> contextp{new VerilatedContext};
    bool traceOn = false;
    register_cbs();

    for (int i = 1; i < argc; ++i) {
        const std::string arg = std::string(argv[i]);
        if (arg == "--trace") {
            traceOn = true;
        } else if (arg == "--help") {
            fprintf(stderr,
                    "usage: %s [--trace]\n"
                    "\n"
                    "Cocotb + Verilator sim\n"
                    "\n"
                    "options:\n"
                    "  --trace      Enables tracing (VCD or FST)\n",
                    basename(argv[0]));
            return 0;
        }
    }

    (void)traceOn;  // Prevent unused if VM_TRACE not defined
    contextp->commandArgs(argc, argv);
#ifdef VERILATOR_SIM_DEBUG
    contextp->debug(99);
#endif
    const std::unique_ptr<VM_PREFIX> top{new VM_PREFIX{contextp.get(),
                                                       // Note null name - we're flattening it out
                                                       ""}};
    contextp->fatalOnVpiError(false);  // otherwise it will fail on systemtf

#ifdef VERILATOR_SIM_DEBUG
    contextp->internalsDump();
#endif

    VerilatedVpi::callCbs(cbStartOfSimulation);

#if VM_TRACE
#if VM_TRACE_FST
    std::unique_ptr<VerilatedFstC> tfp(new VerilatedFstC);
    const char* traceFile = "dump.fst";
#else
    std::unique_ptr<VerilatedVcdC> tfp(new VerilatedVcdC);
    const char* traceFile = "dump.vcd";
#endif

    if (traceOn) {
        contextp->traceEverOn(true);
        top->trace(tfp.get(), 99);
        tfp->open(traceFile);
    }
#endif

    while (!contextp->gotFinish()) {
        do {
            // We must evaluate whole design until we process all 'events' for
            // this time step
            do {
                top->eval_step();
                VerilatedVpi::clearEvalNeeded();
                VerilatedVpi::doInertialPuts();
                settle_value_callbacks();
            } while (VerilatedVpi::evalNeeded());

            // Run ReadWrite callback as we are done processing this eval step
            VerilatedVpi::callCbs(cbReadWriteSynch);
            VerilatedVpi::doInertialPuts();
            settle_value_callbacks();
        } while (VerilatedVpi::evalNeeded() || VerilatedVpi::hasCbs(cbReadWriteSynch));

        top->eval_end_step();

        // Call ReadOnly callbacks
        VerilatedVpi::callCbs(cbReadOnlySynch);

#if VM_TRACE
        if (traceOn) tfp->dump(contextp->time());
#endif
        // cocotb controls the clock inputs using cbAfterDelay so
        // skip ahead to the next registered callback
        const uint64_t NO_TOP_EVENTS_PENDING = static_cast<uint64_t>(~0ULL);
        const uint64_t next_time_cocotb = VerilatedVpi::cbNextDeadline();
        const uint64_t next_time_timing
            = top->eventsPending() ? top->nextTimeSlot() : NO_TOP_EVENTS_PENDING;
        const uint64_t next_time = std::min(next_time_cocotb, next_time_timing);

        // If there are no more cbAfterDelay callbacks,
        // the next deadline is max value, so end the simulation now
        if (next_time == NO_TOP_EVENTS_PENDING) {
            break;
        } else {
            contextp->time(next_time);
        }

        // Call registered NextSimTime
        // It should be called in simulation cycle before everything else
        // but not on first cycle
        VerilatedVpi::callCbs(cbNextSimTime);
        settle_value_callbacks();

        // Call registered timed callbacks (e.g. clock timer)
        // These are called at the beginning of the time step
        // before the iterative regions (IEEE 1800-2012 4.4.1)
        VerilatedVpi::callTimedCbs();
        settle_value_callbacks();
    }

    VerilatedVpi::callCbs(cbEndOfSimulation);

    top->final();

#if VM_TRACE
    if (traceOn) tfp->close();
#endif

// VM_COVERAGE is a define which is set if Verilator is
// instructed to collect coverage (when compiling the simulation)
#if VM_COVERAGE
    VerilatedCov::write("coverage.dat");
#endif

    return 0;
};
#endif
