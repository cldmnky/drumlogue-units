# CPU Profiling for Drumlogue Units

## Overview

The QEMU ARM test framework includes built-in CPU profiling to measure DSP performance and real-time capabilities. This helps identify performance bottlenecks and ensure units can run in real-time on the drumlogue hardware (ARM Cortex-A7 @ 900MHz).

## Quick Start

```bash
# Profile a unit
./test-unit.sh <unit-name> --profile

# Examples
./test-unit.sh pepege-synth --profile
./test-unit.sh clouds-revfx --profile
./test-unit.sh elementish-synth --profile
```

## What Gets Measured

### Timing Metrics
- **Total render time**: Cumulative time spent in `unit_render()` calls
- **Average per buffer**: Mean processing time for each audio buffer (256 frames @ 48kHz)
- **Min/Max times**: Best and worst case processing times
- **Render calls**: Number of times `unit_render()` was invoked

### Performance Metrics
- **CPU usage**: Percentage of real-time capacity used
  - Formula: `(total_render_time / total_audio_time) * 100`
  - <50% = Excellent, 50-80% = Good, 80-100% = Heavy, >100% = Cannot run in real-time
  
- **Real-time factor**: How many times faster than real-time
  - Formula: `total_audio_time / total_render_time`
  - >1.0x = Can run in real-time
  - <1.0x = Too slow, will drop audio
  
- **Buffer overhead**: Percentage of each buffer duration spent processing
  - Formula: `(avg_render_time / buffer_duration) * 100`
  - Indicates how much headroom remains per buffer

### Headroom Analysis
- **Available headroom**: Remaining CPU capacity before hitting real-time limit
  - Formula: `(1 - (1 / realtime_factor)) * 100`
  - >50% = Plenty of headroom
  - 20-50% = Moderate headroom
  - <20% = Tight, close to limit

## Example Output

```
═══════════════════════════════════════════════════════════════
                     CPU PROFILING REPORT                      
═══════════════════════════════════════════════════════════════

Audio Configuration:
  Sample rate:        48000 Hz
  Buffer size:        256 frames (5.33 ms)
  Total audio time:   1.000 seconds
  Render calls:       188

Timing Statistics:
  Total render time:  0.064862 seconds
  Average per buffer: 0.000345 seconds (0.345 ms)
  Minimum:            0.000167 seconds (0.167 ms)
  Maximum:            0.000886 seconds (0.886 ms)

Performance Metrics:
  CPU usage:          6.49% ✅ Excellent
  Real-time factor:   15.42x ✅
  Buffer overhead:    6.47% of buffer time

Headroom Analysis:
  Available headroom: 93.51%
  Assessment:         ✅ Plenty of CPU headroom

═══════════════════════════════════════════════════════════════
```

## Benchmark Results

### Tested Units (QEMU ARM @ Cortex-A7)

| Unit | CPU Usage | Real-time Factor | Headroom | Assessment |
|------|-----------|------------------|----------|------------|
| pepege-synth | 1.75% | 57.19x | 98.25% | ✅ Excellent |
| clouds-revfx | 1.88% | 53.26x | 98.12% | ✅ Excellent |
| elementish-synth | 6.49% | 15.42x | 93.51% | ✅ Excellent |

**Note:** QEMU emulation may not perfectly reflect actual hardware performance, but provides good relative comparisons between units and algorithms.

## Interpreting Results

### CPU Usage Thresholds
- **<10%**: Very light DSP, lots of headroom for additional processing
- **10-30%**: Moderate DSP, good headroom
- **30-50%**: Heavy DSP, acceptable headroom
- **50-80%**: Very heavy DSP, limited headroom
- **>80%**: Critical, may struggle with polyphony or additional effects
- **>100%**: Cannot run in real-time, will drop audio

### Real-Time Factor Guidelines
- **>50x**: Ultra-light processing
- **10-50x**: Light to moderate processing
- **2-10x**: Moderate to heavy processing
- **1-2x**: Very heavy, minimal headroom
- **<1x**: Cannot maintain real-time (will glitch/drop audio)

## Optimization Tips

If profiling reveals high CPU usage:

1. **Use lookup tables** instead of expensive math functions (sin, exp, log)
2. **Reduce buffer processing** - minimize loops and branching
3. **Optimize filter cascades** - use efficient filter structures
4. **Use fixed-point math** where appropriate (ARM has fast integer ops)
5. **Profile hotspots** - focus optimization on slowest functions
6. **Consider NEON SIMD** for vectorizable operations
7. **Reduce state updates** - cache parameter calculations

## Hardware Comparison

### Drumlogue Specifications
- **CPU**: i.MX 6ULZ (ARM Cortex-A7 @ 900MHz)
- **Sample Rate**: 48kHz
- **Buffer Size**: Typically 256 frames (5.33ms @ 48kHz)
- **Target CPU Usage**: <80% for safe operation with polyphony

### QEMU Emulation Accuracy
- QEMU provides cycle-approximate ARM emulation
- Performance is representative but not identical to hardware
- Use profiling to compare units, not as absolute hardware prediction
- Always validate on actual hardware for production

## Advanced Usage

### Custom Buffer Sizes
```bash
# Test with different buffer sizes
./test-unit.sh <unit> --profile --buffer-size 128   # 2.67ms buffers
./test-unit.sh <unit> --profile --buffer-size 512   # 10.67ms buffers
```

### Direct unit_host_arm Usage
```bash
qemu-arm-static -cpu cortex-a7 -L /usr/arm-linux-gnueabihf \
  ./unit_host_arm \
  <unit>.drmlgunit \
  input.wav \
  output.wav \
  --profile \
  --buffer-size 256 \
  --sample-rate 48000
```

## Implementation Details

### Timing Method
- Uses `clock_gettime(CLOCK_MONOTONIC)` for nanosecond precision
- Measures time around each `unit_render()` call
- Calculates min/max/average statistics
- Compares against real-time audio duration

### Overhead
- Profiling overhead is minimal (<1% additional CPU)
- Time measurement calls are excluded from profiling stats
- Stats are accumulated in real-time, not post-processed

## Continuous Integration

Profiling can be integrated into CI/CD pipelines:

```yaml
- name: Profile DSP performance
  run: |
    cd test/qemu-arm
    ./test-unit.sh pepege-synth --profile > profile.txt
    # Parse and assert CPU usage < threshold
    grep "CPU usage" profile.txt | awk '{if ($3 > 50) exit 1}'
```

## Troubleshooting

### "No profiling data collected"
- Profiling is only enabled with `--profile` flag
- Check that unit is processing audio (synths need MIDI trigger)

### Unexpectedly high CPU usage
- Check DSP algorithm complexity
- Verify no debug code is active (printf, assertions)
- Compare with similar units (pepege-synth is baseline)
- Test on actual hardware to confirm

### Inconsistent measurements
- First buffer may be slower (initialization, cache warming)
- Max time may show outliers from QEMU scheduling
- Average time is most representative metric
