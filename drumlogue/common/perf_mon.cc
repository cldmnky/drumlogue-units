/**
 * @file perf_mon.cc
 * @brief Performance monitoring static member definitions
 *
 * This file provides out-of-class definitions for PerfMon static members
 * to work with the header-only perf_mon.h library.
 */

#include "perf_mon.h"

#ifdef PERF_MON

namespace dsp {

// Static member definitions
PerfCounter PerfMon::counters_[PerfMon::kMaxCounters];
uint8_t PerfMon::counter_count_ = 0;

}  // namespace dsp

// C-linkage export wrappers so the QEMU ARM unit host can read the counters
// via dlsym() (the PerfMon static methods are inline and never emitted as
// dynamic symbols once optimized/inlined).
extern "C" {

__attribute__((visibility("default")))
uint8_t perf_mon_get_counter_count(void) {
    return ::dsp::PerfMon::GetCounterCount();
}

__attribute__((visibility("default")))
const char* perf_mon_get_counter_name(uint8_t id) {
    return ::dsp::PerfMon::GetCounterName(id);
}

__attribute__((visibility("default")))
uint32_t perf_mon_get_average_cycles(uint8_t id) {
    return ::dsp::PerfMon::GetAverageCycles(id);
}

__attribute__((visibility("default")))
uint32_t perf_mon_get_peak_cycles(uint8_t id) {
    return ::dsp::PerfMon::GetPeakCycles(id);
}

__attribute__((visibility("default")))
uint32_t perf_mon_get_min_cycles(uint8_t id) {
    return ::dsp::PerfMon::GetMinCycles(id);
}

__attribute__((visibility("default")))
uint32_t perf_mon_get_frame_count(uint8_t id) {
    return ::dsp::PerfMon::GetFrameCount(id);
}

}  // extern "C"

#endif  // PERF_MON
