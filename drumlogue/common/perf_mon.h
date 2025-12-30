/**
 * @file perf_mon.h
 * @brief Performance monitoring utilities for drumlogue DSP units
 * 
 * Provides cycle counting and performance metrics that can be compiled in/out
 * via -DPERF_MON flag. When disabled, all macros compile to nothing.
 * 
 * Usage:
 *   Compile with: -DPERF_MON to enable performance monitoring
 *   Compile with: (default) to disable (zero overhead)
 * 
 * Example:
 *   PERF_MON_INIT();              // Initialize (call once in unit_init)
 *   PERF_MON_START(my_section);   // Start measurement
 *   // ... DSP code ...
 *   PERF_MON_END(my_section);     // End measurement
 *   
 *   // Get stats
 *   uint32_t cycles = PERF_MON_GET_CYCLES(my_section);
 *   float utilization = PERF_MON_GET_UTILIZATION(my_section);
 * 
 * @note Performance counters use ARM DWT (Data Watchpoint & Trace) PMCCNTR register
 *       which is available on ARM Cortex-M and Cortex-A processors.
 * @note When PERF_MON is disabled, all functions compile to empty inline stubs.
 */

#pragma once

#include <cstdint>
#include <cstdio>
#ifdef TEST
#include <chrono>
#endif

namespace dsp {

#ifdef PERF_MON

/**
 * @brief Performance counter metadata
 */
struct PerfCounter {
    const char* name;
    uint32_t start_cycles;
    uint32_t total_cycles;
    uint32_t frame_count;
    uint32_t peak_cycles;
    uint32_t min_cycles;
};

/**
 * @brief Performance counter statistics (exported)
 */
struct PerfStats {
    const char* name;
    uint32_t average_cycles;
    uint32_t peak_cycles;
    uint32_t min_cycles;
    uint32_t frame_count;
};

/**
 * @brief Global performance monitoring system
 */
class PerfMon {
 public:
    static constexpr size_t kMaxCounters = 16;
    
    /**
     * @brief Initialize performance monitoring
     * Call once in unit_init()
     */
    static void Init() {
        counter_count_ = 0;
        for (size_t i = 0; i < kMaxCounters; i++) {
            counters_[i].name = nullptr;
            counters_[i].total_cycles = 0;
            counters_[i].frame_count = 0;
            counters_[i].peak_cycles = 0;
            counters_[i].min_cycles = 0xFFFFFFFF;
        }
    }
    
    /**
     * @brief Register a named performance counter
     * @param name Counter name (e.g., "OSC", "FILTER", "EFFECTS")
     * @return Counter index for use in Start/End
     */
    static uint8_t RegisterCounter(const char* name) {
        if (counter_count_ >= kMaxCounters) {
            return 0xFF;  // Counter full
        }
        counters_[counter_count_].name = name;
        return counter_count_++;
    }
    
    /**
     * @brief Start cycle counting for a section
     * @param counter_id ID from RegisterCounter()
     */
    static inline void Start(uint8_t counter_id) {
        if (counter_id >= counter_count_) return;
        counters_[counter_id].start_cycles = GetCycleCount();
    }
    
    /**
     * @brief End cycle counting and accumulate
     * @param counter_id ID from RegisterCounter()
     */
    static inline void End(uint8_t counter_id) {
        if (counter_id >= counter_count_) return;
        
        uint32_t end_cycles = GetCycleCount();
        uint32_t elapsed = end_cycles - counters_[counter_id].start_cycles;
        
        counters_[counter_id].total_cycles += elapsed;
        counters_[counter_id].frame_count++;
        
        if (elapsed > counters_[counter_id].peak_cycles) {
            counters_[counter_id].peak_cycles = elapsed;
        }
        if (elapsed < counters_[counter_id].min_cycles) {
            counters_[counter_id].min_cycles = elapsed;
        }
    }
    
    /**
     * @brief Get average cycles for a counter
     * @param counter_id ID from RegisterCounter()
     * @return Average cycles per measurement
     */
    static uint32_t GetAverageCycles(uint8_t counter_id) {
        if (counter_id >= counter_count_ || counters_[counter_id].frame_count == 0) {
            return 0;
        }
        return counters_[counter_id].total_cycles / counters_[counter_id].frame_count;
    }
    
    /**
     * @brief Get peak cycles for a counter
     * @param counter_id ID from RegisterCounter()
     * @return Maximum cycles in any single measurement
     */
    static uint32_t GetPeakCycles(uint8_t counter_id) {
        if (counter_id >= counter_count_) {
            return 0;
        }
        return counters_[counter_id].peak_cycles;
    }
    
    /**
     * @brief Get minimum cycles for a counter
     * @param counter_id ID from RegisterCounter()
     * @return Minimum cycles in any single measurement
     */
    static uint32_t GetMinCycles(uint8_t counter_id) {
        if (counter_id >= counter_count_) {
            return 0;
        }
        return counters_[counter_id].min_cycles;
    }
    
    /**
     * @brief Get total frame count for a counter
     * @param counter_id ID from RegisterCounter()
     */
    static uint32_t GetFrameCount(uint8_t counter_id) {
        if (counter_id >= counter_count_) {
            return 0;
        }
        return counters_[counter_id].frame_count;
    }
    
    /**
     * @brief Get counter name
     * @param counter_id ID from RegisterCounter()
     */
    static const char* GetCounterName(uint8_t counter_id) {
        if (counter_id >= counter_count_) {
            return "";
        }
        return counters_[counter_id].name;
    }
    
    /**
     * @brief Get number of active counters
     */
    static uint8_t GetCounterCount() {
        return counter_count_;
    }
    
    /**
     * @brief Reset all counters
     */
    static void Reset() {
        for (size_t i = 0; i < counter_count_; i++) {
            counters_[i].total_cycles = 0;
            counters_[i].frame_count = 0;
            counters_[i].peak_cycles = 0;
            counters_[i].min_cycles = 0xFFFFFFFF;
        }
    }
    
    /**
     * @brief Export statistics for a single counter
     * @param counter_id ID from RegisterCounter()
     * @return PerfStats structure with exported data
     */
    static PerfStats GetStats(uint8_t counter_id) {
        PerfStats stats = {};
        if (counter_id >= counter_count_) {
            return stats;
        }
        
        const PerfCounter& c = counters_[counter_id];
        stats.name = c.name;
        stats.average_cycles = (c.frame_count > 0) ? (c.total_cycles / c.frame_count) : 0;
        stats.peak_cycles = c.peak_cycles;
        stats.min_cycles = (c.frame_count > 0) ? c.min_cycles : 0;
        stats.frame_count = c.frame_count;
        return stats;
    }
    
    /**
     * @brief Export all counter statistics
     * @param out_stats Array to receive PerfStats (must be >= GetCounterCount() size)
     * @param max_count Maximum entries in out_stats array
     * @return Number of counters exported
     */
    static uint8_t ExportAllStats(PerfStats* out_stats, uint8_t max_count) {
        if (!out_stats || max_count == 0) {
            return 0;
        }
        
        uint8_t count = (counter_count_ < max_count) ? counter_count_ : max_count;
        for (uint8_t i = 0; i < count; i++) {
            out_stats[i] = GetStats(i);
        }
        return count;
    }
    
    /**
     * @brief Format counter stats as a string
     * @param counter_id Counter to format
     * @param buffer Output buffer
     * @param buf_size Buffer size
     * @return Number of bytes written to buffer
     * 
     * Example output: "Oscillator: avg=1250 peak=1340 min=1200 (48000 meas)"
     */
    static int FormatStats(uint8_t counter_id, char* buffer, size_t buf_size) {
        if (!buffer || buf_size == 0 || counter_id >= counter_count_) {
            return 0;
        }
        
        PerfStats stats = GetStats(counter_id);
        return snprintf(buffer, buf_size, "%s: avg=%u peak=%u min=%u (%u meas)",
                        stats.name, stats.average_cycles, stats.peak_cycles,
                        stats.min_cycles, stats.frame_count);
    }
    
    /**
     * @brief Print all performance statistics to stdout
     * (Useful for debugging via printf)
     */
    static void PrintAllStats() {
        printf("\n=== Performance Monitoring Statistics ===\n");
        for (uint8_t i = 0; i < counter_count_; i++) {
            PerfStats stats = GetStats(i);
            printf("  %s: avg=%u peak=%u min=%u (%u measurements)\n",
                   stats.name, stats.average_cycles, stats.peak_cycles,
                   stats.min_cycles, stats.frame_count);
        }
        printf("=========================================\n\n");
    }
    
    /**
     * @brief Calculate total average cycles across all counters
     * @return Sum of average cycles for all active counters
     */
    static uint32_t GetTotalAverageCycles() {
        uint32_t total = 0;
        for (uint8_t i = 0; i < counter_count_; i++) {
            total += GetStats(i).average_cycles;
        }
        return total;
    }
    
    /**
     * @brief Calculate total peak cycles across all counters
     * @return Sum of peak cycles for all active counters
     */
    static uint32_t GetTotalPeakCycles() {
        uint32_t total = 0;
        for (uint8_t i = 0; i < counter_count_; i++) {
            total += GetStats(i).peak_cycles;
        }
        return total;
    }
 
 private:
    static PerfCounter counters_[kMaxCounters];
    static uint8_t counter_count_;
    
    /**
     * @brief Read cycle counter (DWT PMCCNTR on ARM, high-res timer on desktop)
     * Available on ARM Cortex-A and Cortex-M with DWT support
     * On desktop, uses high-resolution clock for timing simulation
     */
    static inline uint32_t GetCycleCount() {
#ifdef TEST
        // For testing on x86_64, use high-resolution clock converted to "cycles"
        // Simulate ~600MHz ARM clock (600 cycles per microsecond)
        using namespace std::chrono;
        static auto start_time = high_resolution_clock::now();
        auto now = high_resolution_clock::now();
        auto elapsed_us = duration_cast<microseconds>(now - start_time).count();
        return static_cast<uint32_t>(elapsed_us * 600);  // 600 cycles per microsecond
#else
        // ARM DWT PMCCNTR register address
        // 0xE0001004 on most Cortex-M/A processors
        volatile uint32_t* pmccntr = (volatile uint32_t*)0xE0001004;
        return *pmccntr;
#endif
    }
};

// Helper to access static members via function
// NOTE: Out-of-class definitions removed - using function-local statics instead

// Macro API for convenient use
#define PERF_MON_INIT() \
    ::dsp::PerfMon::Init()

#define PERF_MON_REGISTER(name) \
    ::dsp::PerfMon::RegisterCounter(name)

#define PERF_MON_START(id) \
    ::dsp::PerfMon::Start(id)

#define PERF_MON_END(id) \
    ::dsp::PerfMon::End(id)

#define PERF_MON_GET_AVG(id) \
    ::dsp::PerfMon::GetAverageCycles(id)

#define PERF_MON_GET_PEAK(id) \
    ::dsp::PerfMon::GetPeakCycles(id)

#define PERF_MON_GET_MIN(id) \
    ::dsp::PerfMon::GetMinCycles(id)

#define PERF_MON_GET_FRAMES(id) \
    ::dsp::PerfMon::GetFrameCount(id)

#define PERF_MON_GET_NAME(id) \
    ::dsp::PerfMon::GetCounterName(id)

#define PERF_MON_RESET() \
    ::dsp::PerfMon::Reset()

// Data export macros
#define PERF_MON_GET_STATS(id) \
    ::dsp::PerfMon::GetStats(id)

#define PERF_MON_EXPORT_ALL(out_stats, max_count) \
    ::dsp::PerfMon::ExportAllStats(out_stats, max_count)

#define PERF_MON_FORMAT_STATS(id, buf, size) \
    ::dsp::PerfMon::FormatStats(id, buf, size)

#define PERF_MON_PRINT_ALL() \
    ::dsp::PerfMon::PrintAllStats()

#define PERF_MON_TOTAL_AVG() \
    ::dsp::PerfMon::GetTotalAverageCycles()

#define PERF_MON_TOTAL_PEAK() \
    ::dsp::PerfMon::GetTotalPeakCycles()

#else  // PERF_MON not defined - compile to nothing

// Empty stubs when performance monitoring is disabled
#define PERF_MON_INIT() do {} while(0)
#define PERF_MON_REGISTER(name) 0
#define PERF_MON_START(id) do {} while(0)
#define PERF_MON_END(id) do {} while(0)
#define PERF_MON_GET_AVG(id) 0
#define PERF_MON_GET_PEAK(id) 0
#define PERF_MON_GET_MIN(id) 0
#define PERF_MON_GET_FRAMES(id) 0
#define PERF_MON_GET_NAME(id) ""
#define PERF_MON_RESET() do {} while(0)
#define PERF_MON_GET_STATS(id) (PerfStats{})
#define PERF_MON_EXPORT_ALL(out_stats, max_count) 0
#define PERF_MON_FORMAT_STATS(id, buf, size) 0
#define PERF_MON_PRINT_ALL() do {} while(0)
#define PERF_MON_TOTAL_AVG() 0
#define PERF_MON_TOTAL_PEAK() 0

#endif  // PERF_MON

}  // namespace dsp
