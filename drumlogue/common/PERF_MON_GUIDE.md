# Performance Monitoring Guide for Drumlogue Units

## Quick Start

The `perf_mon.h` header provides cycle counting and performance metrics for drumlogue DSP units. It has **zero overhead when disabled** and is controlled via the `-DPERF_MON` compiler flag.

### 1. Enable in Your Unit

Add to your unit's `config.mk`:
```makefile
# Enable performance monitoring during build
UDEFS = -DPERF_MON
```

Or in your unit's `unit.cc`:
```cpp
#include "common/perf_mon.h"

// Call once in unit_init()
void unit_init(const unit_runtime_desc_t* runtime) {
    PERF_MON_INIT();
    
    // Register named counters for different DSP sections
    perf_osc_ = PERF_MON_REGISTER("Oscillator");
    perf_filter_ = PERF_MON_REGISTER("Filter");
    perf_effects_ = PERF_MON_REGISTER("Effects");
}
```

### 2. Wrap DSP Sections

Surround performance-critical sections with start/end calls:

```cpp
void unit_render(unit_runtime_desc_t* runtime) {
    const float* in = runtime->input_buffer;
    float* out = runtime->output_buffer;
    
    for (uint32_t i = 0; i < runtime->frames_per_buffer; i++) {
        float sig = 0.0f;
        
        // Measure oscillator
        PERF_MON_START(perf_osc_);
        sig = osc_.Process();
        PERF_MON_END(perf_osc_);
        
        // Measure filter
        PERF_MON_START(perf_filter_);
        sig = filter_.Process(sig);
        PERF_MON_END(perf_filter_);
        
        // Measure effects
        PERF_MON_START(perf_effects_);
        out[i] = ProcessEffects(sig);
        PERF_MON_END(perf_effects_);
    }
}
```

### 3. Query Performance Data

Get statistics from performance counters:

```cpp
// Get average cycles for a section
uint32_t avg_cycles = PERF_MON_GET_AVG(perf_osc_);

// Get peak cycles (worst case)
uint32_t peak_cycles = PERF_MON_GET_PEAK(perf_osc_);

// Get minimum cycles (best case)
uint32_t min_cycles = PERF_MON_GET_MIN(perf_osc_);

// Get number of measurements
uint32_t frame_count = PERF_MON_GET_FRAMES(perf_osc_);

// Get counter name
const char* name = PERF_MON_GET_NAME(perf_osc_);

// Reset all counters
PERF_MON_RESET();
```

## Performance Macros Reference

| Macro | Purpose |
|-------|---------|
| `PERF_MON_INIT()` | Initialize performance system (call once) |
| `PERF_MON_REGISTER(name)` | Register counter, returns counter ID |
| `PERF_MON_START(id)` | Start cycle counting |
| `PERF_MON_END(id)` | Stop and accumulate cycles |
| `PERF_MON_GET_AVG(id)` | Get average cycles per measurement |
| `PERF_MON_GET_PEAK(id)` | Get maximum cycles (worst case) |
| `PERF_MON_GET_MIN(id)` | Get minimum cycles (best case) |
| `PERF_MON_GET_FRAMES(id)` | Get total measurements |
| `PERF_MON_GET_NAME(id)` | Get counter name string |
| `PERF_MON_RESET()` | Reset all counters |

## How It Works

### Cycle Counting

Performance monitoring uses the ARM DWT (Data Watchpoint & Trace) `PMCCNTR` register, which counts CPU cycles. This is available on:
- ARM Cortex-A7 (drumlogue platform)
- ARM Cortex-M4/M7 (other logue platforms)

### Zero Overhead When Disabled

When `-DPERF_MON` is **not** defined, all macros compile to empty stubs:
```cpp
#define PERF_MON_START(id) do {} while(0)  // Does nothing
#define PERF_MON_END(id)   do {} while(0)  // Does nothing
```

This means **no performance impact** on the default build.

## Interpreting Results

### Cycle Budget

Drumlogue processes 64 samples per render call at 48kHz:
- **Time per sample:** ~1000 cycles (at 48MHz ARM clock)
- **Time per render call:** ~64,000 cycles total
- **Budget:** Varies by unit type (effects are DSP-heavy, synths less so)

### Example Output

```
Oscillator: avg=1250 peak=1340 min=1200 (48000 meas)
Filter:     avg=800  peak=950  min=750  (48000 meas)
Effects:    avg=2100 peak=2400 min=1950 (48000 meas)
Total: 4150 cycles/sample
```

Interpretation:
- **Average:** 4150 cycles/sample is ~83% of budget
- **Peak:** Spike to 4690 cycles in worst case
- **Min:** Best case is 3900 cycles

### Optimization Tips

1. **Reduce peak cycles** - Look at `PEAK` to find worst-case scenarios
2. **Check variance** - Large gap between MIN and PEAK indicates branching
3. **Profile bottlenecks** - Add more counters to drill down into slow sections
4. **Compare modes** - Measure MONO vs POLY vs UNISON to find mode-specific issues

## Build Examples

### Build with Performance Monitoring

```bash
# Add to your unit's config.mk
UDEFS = -DPERF_MON

# Build
./build.sh your-unit
```

### Build without Performance Monitoring (Default)

```bash
# Default (no UDEFS or UDEFS without -DPERF_MON)
./build.sh your-unit

# No performance overhead
```

## For Test Harnesses

Desktop test harnesses can also use performance monitoring:

```cpp
// In test/your-unit/main.cc
#include "../common/perf_mon.h"

int main() {
    PERF_MON_INIT();
    perf_dsp_ = PERF_MON_REGISTER("DSP");
    
    // ... process audio ...
    
    // Print results
    printf("DSP: avg=%u peak=%u cycles/sample\n",
           PERF_MON_GET_AVG(perf_dsp_),
           PERF_MON_GET_PEAK(perf_dsp_));
}
```

Compile with: `g++ -DPERF_MON -std=c++17 main.cc ...`

## Common Patterns

### Pattern 1: Profile All Synthesis Modes

```cpp
// In drupiter-synth.cc
static uint8_t perf_mono_, perf_poly_, perf_unison_;

void DrupiterSynth::RenderMono(...) {
    PERF_MON_START(perf_mono_);
    // ... render ...
    PERF_MON_END(perf_mono_);
}

void DrupiterSynth::RenderPoly(...) {
    PERF_MON_START(perf_poly_);
    // ... render ...
    PERF_MON_END(perf_poly_);
}

void DrupiterSynth::RenderUnison(...) {
    PERF_MON_START(perf_unison_);
    // ... render ...
    PERF_MON_END(perf_unison_);
}
```

### Pattern 2: Per-Voice Profiling

```cpp
for (uint8_t v = 0; v < num_voices; v++) {
    PERF_MON_START(perf_voice_dsp_);
    ProcessVoice(v);
    PERF_MON_END(perf_voice_dsp_);
}
```

### Pattern 3: Conditional Profiling

```cpp
#ifdef PERF_MON
    uint8_t perf_critical_section_;
#endif

void Init() {
#ifdef PERF_MON
    PERF_MON_INIT();
    perf_critical_section_ = PERF_MON_REGISTER("CriticalSection");
#endif
}

void Render() {
    PERF_MON_START(perf_critical_section_);
    // ... critical code ...
    PERF_MON_END(perf_critical_section_);
}
```

## Limitations

- **ARM DWT register:** Performance monitoring relies on ARM DWT availability. Not all ARM devices support this.
- **Counter limit:** Maximum 16 named counters per unit
- **Cycle accuracy:** Nested measurements may have slight inaccuracy due to function call overhead
- **ARM only:** Desktop testing only works on ARM platforms (macOS Silicon, not Intel)

## Reference Example

See `common/perf_mon_example.cc` for a complete integration example.
