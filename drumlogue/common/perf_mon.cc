/**
 * @file perf_mon.cc
 * @brief Performance monitoring static member definitions
 *
 * This file provides out-of-class definitions for PerfMon static members
 * to work with the header-only perf_mon.h library.
 */

#include "perf_mon.h"

namespace dsp {

// Static member definitions
PerfCounter PerfMon::counters_[PerfMon::kMaxCounters];
uint8_t PerfMon::counter_count_ = 0;

}  // namespace dsp
