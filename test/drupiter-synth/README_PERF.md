# Drupiter Synth Performance Monitoring

This document explains how to use the built-in performance monitoring system to measure CPU usage across different synthesis modes.

## Overview

The Drupiter synth includes a comprehensive performance monitoring system that tracks CPU cycles for different DSP components. This helps optimize performance and ensure real-time audio processing constraints are met.

## Enabling Performance Monitoring

Performance monitoring is disabled by default to avoid overhead in production builds. Enable it during development:

### For Unit Build (Hardware)
```bash
./build.sh drupiter-synth PERF_MON=1
```

### For Desktop Testing
```bash
cd test/drupiter-synth
make clean
make PERF_MON=1
```

## Running Performance Tests

### Automated Performance Test
```bash
cd test/drupiter-synth
make perf-test
```

This runs a comprehensive test that measures CPU usage in all synthesis modes:
- Monophonic (1 voice)
- Polyphonic (2 voices)
- Polyphonic (4 voices)
- Unison (held note, 5-voice stack)

### Manual Performance Testing

You can also add performance monitoring to your own test code:

```cpp
#include "../common/perf_mon.h"

// Initialize in your test setup
PERF_MON_INIT();
uint8_t counter_id = PERF_MON_REGISTER("MyComponent");

// In your processing loop
PERF_MON_START(counter_id);
// ... your DSP code ...
PERF_MON_END(counter_id);

// Print results
PERF_MON_PRINT_ALL();
```

## Performance Counters

The system tracks these DSP components:

- **VoiceAlloc**: Voice management, note triggering, envelope updates
- **DCO**: Oscillator processing (wavetable lookup, FM, drift, PolyBLEP) — *nested*
- **VCF**: Filter processing (LPF with resonance) — *nested*
- **Effects**: Chorus, modulation, additional processing — *nested*
- **RenderTotal**: Complete audio buffer processing — **headline metric**

> The nested counters overlap with RenderTotal and must NOT be summed.
> The summary table reports RenderTotal only.

## Understanding Results

### CPU Utilization Calculation

Utilization is measured against the FULL CALLBACK budget, mirroring the
hardware (DRUSYS report: NXP i.MX6 ULZ, Cortex-A7 @ 900 MHz):

- **Sample Rate**: 48 kHz
- **Buffer Size**: 64 frames (drumlogue hardware buffer)
- **Callback budget**: 64 / 48000 s = 1.333 ms ≈ 1,200,000 cycles @ 900 MHz

```
Utilization % = (cycles_per_buffer / 1_200_000) × 100
```

## Understanding Results

### CPU Utilization Calculation

The test calculates CPU utilization based on:
- **Sample Rate**: 48kHz
- **CPU Frequency**: 600 MHz (typical ARM Cortex-A7)
- **Cycles per Sample**: CPU cycles available per audio sample

```
Utilization % = (cycles_used / cycles_per_sample) × 100
```

### Performance Ratings

- **< 50%**: Excellent - plenty of headroom for modulation/effects
- **50-70%**: Good - reasonable headroom, stable performance
- **70-80%**: Fair - near limit, monitor carefully
- **> 80%**: Poor - may cause audio dropouts (xruns)

## Measured Results (2026-08, desktop harness, indicative)

Whole-callback cost per 64-frame buffer; desktop wall-clock simulation,
normalized to the 900 MHz hardware budget:

| Mode              | Avg cycles | Avg % of budget | Peak % |
|-------------------|-----------:|----------------:|-------:|
| Mono (1 voice)    |      ~6350 |            ~0.5% |   ~1.4% |
| Poly (2 voices)   |      ~8265 |            ~0.7% |   ~1.0% |
| Poly (4 voices)   |      ~9173 |            ~0.8% |   ~1.1% |
| Unison (held)     |      ~4885 |            ~0.4% |   ~1.4% |

QEMU ARM profile (`make -f Makefile.podman perf-test UNIT=drupiter-synth`,
256-frame host buffer, mono preset + one held note):

- Average callback: **0.768 ms** (14.4% of the 900 MHz budget), peak 0.37×
- Real-time factor: **6.95×**

These are relative indicators only; validate on hardware before release.

## Optimization Impact

Historical notes (verify independently before quoting):

- Q31 wavetable interpolation: micro-benchmark claims of 30-40% were never
  validated end-to-end; float interpolation may be comparable on Cortex-A7.

## Technical Details

### Cycle Counting

Desktop/QEMU builds use a high-resolution wall-clock simulation of the
900 MHz cycle counter (microsecond resolution). Hardware profiling requires
an ARM PMU-based counter (the Cortex-M DWT address used previously is not
valid userspace PMU access on the i.MX6 ULZ).

### Buffer Processing

Tests process audio in **64-frame** buffers to match real hardware conditions.

### Test Sequence

Each mode test:
1. **Mode flush**: one quiet block applies a queued S MODE change before any
   note-on (deferred changes would otherwise never apply mid-run)
2. **Warm-up**: 1 second with notes held
3. **Measurement**: 2 seconds with notes held (sustained voices exercise the mode)
4. **Cleanup**: Note-off + tail renders

### Voice Configuration

- **Monophonic**: Single voice, last-note priority
- **Polyphonic**: Multiple independent voices with round-robin allocation
- **Unison**: Single note with detuned voice copies

## Troubleshooting

### PERF_MON Not Enabled Error

If you see "PERF_MON not enabled", rebuild with:
```bash
make clean && make PERF_MON=1 perf-test
```

### Inconsistent Results

Performance can vary due to:
- System load (close other applications)
- CPU frequency scaling
- Memory access patterns

Run tests multiple times and average results.

### Hardware vs Desktop Differences

Desktop tests use x86_64 cycle counters, while hardware uses ARM DWT. Results are comparable but not identical due to architecture differences.

### Known Limitations

Desktop/QEMU numbers use simulated cycles and are indicative only. For
release decisions, profile on hardware via the PERF_MON build and compare
against the 1.333 ms callback budget.

## Advanced Usage

### Custom Performance Counters

Add your own counters for detailed profiling:

```cpp
uint8_t my_counter = PERF_MON_REGISTER("MyAlgorithm");
PERF_MON_START(my_counter);
// ... algorithm code ...
PERF_MON_END(my_counter);
```

### Exporting Data

Export performance data for analysis:

```cpp
dsp::PerfStats stats[16];
uint8_t count = PERF_MON_EXPORT_ALL(stats, 16);
for (uint8_t i = 0; i < count; i++) {
    // Process stats[i]
}
```

### Real-time Monitoring

For real-time monitoring in the unit (hardware only):

```cpp
// In unit_render() - collect stats periodically
static uint32_t frame_count = 0;
if (++frame_count % 48000 == 0) {  // Every second at 48kHz
    PERF_MON_PRINT_ALL();  // Prints to debug console
}
```