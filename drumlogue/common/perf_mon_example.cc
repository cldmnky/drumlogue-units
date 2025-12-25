/**
 * @file perf_mon_example.cc
 * @brief Example: How to integrate performance monitoring into a drumlogue unit
 * 
 * This demonstrates how to use perf_mon.h to measure DSP performance.
 * 
 * Build with:  -DPERF_MON  to enable performance monitoring
 * Build with:  (default)   to disable (zero overhead)
 * 
 * When enabled, the unit will track cycle counts for different DSP sections.
 */

// ============================================================================
// STEP 1: Include perf_mon.h in your unit header
// ============================================================================

// In your_synth.h or unit.cc:
// #include "common/perf_mon.h"

// ============================================================================
// STEP 2: Register performance counters in unit_init()
// ============================================================================

namespace dsp {

class ExampleSynthWithPerfMon {
 private:
  // Performance monitoring IDs (use #ifdef to avoid warnings when PERF_MON is off)
#ifdef PERF_MON
  uint8_t perf_osc_;
  uint8_t perf_filter_;
  uint8_t perf_env_;
  uint8_t perf_modulation_;
  uint8_t perf_effects_;
#endif
  
 public:
  void Init() {
    // Initialize performance monitoring system
    PERF_MON_INIT();
    
    // Register named performance counters
    // The counter ID is what you pass to START/END macros
#ifdef PERF_MON
    perf_osc_ = PERF_MON_REGISTER("Oscillator");
    perf_filter_ = PERF_MON_REGISTER("Filter");
    perf_env_ = PERF_MON_REGISTER("Envelope");
    perf_modulation_ = PERF_MON_REGISTER("Modulation");
    perf_effects_ = PERF_MON_REGISTER("Effects");
#endif
    
    // ... rest of unit initialization
  }
  
  // ========================================================================
  // STEP 3: Wrap DSP sections with PERF_MON_START/END
  // ========================================================================
  
  void Render(float* out, uint32_t frames) {
    for (uint32_t i = 0; i < frames; i++) {
      float sig = 0.0f;
      
      // Measure oscillator performance
      PERF_MON_START(perf_osc_);
      sig = ProcessOscillator();
      PERF_MON_END(perf_osc_);
      
      // Measure modulation (LFO, envelopes, etc.)
      PERF_MON_START(perf_modulation_);
      UpdateModulation();
      PERF_MON_END(perf_modulation_);
      
      // Measure filter performance
      PERF_MON_START(perf_filter_);
      sig = filter_.Process(sig);
      PERF_MON_END(perf_filter_);
      
      // Measure envelope performance
      PERF_MON_START(perf_env_);
      float env = env_amp_.Process();
      PERF_MON_END(perf_env_);
      sig *= env;
      
      // Measure effects performance
      PERF_MON_START(perf_effects_);
      out[i] = ProcessEffects(sig);
      PERF_MON_END(perf_effects_);
    }
  }
  
  // ========================================================================
  // STEP 4 (Optional): Retrieve and display performance statistics
  // ========================================================================
  
  /**
   * @brief Print performance statistics to debug output
   * Called from unit_set_tempo() or other non-audio callback
   */
  void PrintPerformanceStats() {
#ifdef PERF_MON
    const uint8_t count = 5;  // Number of counters we registered
    
    for (uint8_t i = 0; i < count; i++) {
      uint32_t avg = PERF_MON_GET_AVG(i);
      uint32_t peak = PERF_MON_GET_PEAK(i);
      uint32_t min_cycles = PERF_MON_GET_MIN(i);
      uint32_t frames = PERF_MON_GET_FRAMES(i);
      const char* name = PERF_MON_GET_NAME(i);
      
      // Example output format:
      // "Oscillator: avg=1250 peak=1340 min=1200 (5120 measurements)"
      printf("%s: avg=%u peak=%u min=%u (%u meas)\n",
             name, avg, peak, min_cycles, frames);
    }
    
    // Calculate total cycles per frame
    uint32_t total_avg = PERF_MON_GET_AVG(0) + PERF_MON_GET_AVG(1) +
                         PERF_MON_GET_AVG(2) + PERF_MON_GET_AVG(3) +
                         PERF_MON_GET_AVG(4);
    printf("Total: %u cycles/sample\n", total_avg);
#endif
  }
  
  /**
   * @brief Reset all performance counters
   * Call this to start fresh measurements
   */
  void ResetPerformanceCounters() {
    PERF_MON_RESET();
  }
  
 private:
  // Example stub functions
  float ProcessOscillator() { return 0.0f; }
  void UpdateModulation() {}
  float ProcessEffects(float sig) { return sig; }
};

}  // namespace dsp

// ============================================================================
// USAGE NOTES
// ============================================================================
//
// 1. When PERF_MON is disabled (default):
//    - All PERF_MON_* macros compile to empty do-while loops or 0
//    - Zero performance overhead
//    - No code size impact
//
// 2. When PERF_MON is enabled (-DPERF_MON):
//    - Cycle counting active (uses ARM DWT PMCCNTR register)
//    - Memory: ~16 * 32 bytes for up to 16 named counters
//    - Each counter tracks: min, max, total, and frame count
//
// 3. Performance interpretation:
//    - Cycles are ARM CPU cycles (ARMv7-A on drumlogue)
//    - Sample rate is 48kHz, so 1 sample = ~1000 cycles at 48MHz
//    - Total available: ~1000 cycles/sample for entire DSP
//    - Track average, peak, and minimum to identify bottlenecks
//
// 4. To enable in your unit:
//    - Add to config.mk: UDEFS = -DPERF_MON
//    - Or compile specific .cc files with -DPERF_MON
//
// ============================================================================
