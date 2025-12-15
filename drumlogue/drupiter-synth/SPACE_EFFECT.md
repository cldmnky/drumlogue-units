# Space Effect Implementation

## Overview

The drupiter-synth now includes an Elements-style "space" effect using the common `AnimatedStereoWidener` class from `drumlogue/common/stereo_widener.h`. This effect creates a subtle stereo spread using Mid/Side processing with LFO modulation, providing the same stereo imaging as the elementish-synth.

## Implementation Details

### What is the Space Effect?

The space effect is a stereo widening technique from Mutable Instruments Elements that:
- Takes a mono (center) signal
- Creates a modulated stereo field using a slow LFO
- Applies width control with LFO-based animation
- Creates subtle stereo movement and width

### How It Works

The `AnimatedStereoWidener` class from common utilities:
1. **LFO Generation** - A slow triangle wave LFO (configurable rate, default 0.5 Hz)
2. **Animated Panning** - Creates stereo spread: `L = mono × (1 + offset)`, `R = mono × (1 - offset)`
   - Where `offset = width × lfo_depth × lfo_value`
3. **NEON Optimization** - Batch processes 4 samples at a time on ARM for efficiency

### Fixed Parameters

- **LFO Rate**: 0.5 Hz (slow modulation like Elements)
- **Space Width**: 50% (0.5) - fixed, no user control
- **LFO Depth**: 100% (1.0) - full modulation of the stereo field
- **LFO Waveform**: Triangle wave (smooth modulation)

## Code Changes

### Files Modified

1. **drupiter_synth.h**
   - Added `#include "../common/stereo_widener.h"`
   - Added `common::AnimatedStereoWidener* space_widener_` member variable
   - Removed inline space effect variables

2. **drupiter_synth.cc**
   - Allocates `AnimatedStereoWidener` in `Init()`
   - Initializes with 0.5 Hz LFO, 50% width, 100% depth
   - Uses `ProcessMonoBatch()` to convert mono to animated stereo
   - Deletes in `Teardown()`

### Implementation (Using Common Class)

```cpp
// In Init():
space_widener_ = new common::AnimatedStereoWidener();
space_widener_->Init(sample_rate_);
space_widener_->SetWidth(0.5f);      // 50% stereo width
space_widener_->SetLfoRate(0.5f);    // 0.5 Hz like Elements
space_widener_->SetLfoDepth(1.0f);   // Full LFO modulation depth

// In Render():
// Sanitize mono buffer
drupiter::neon::SanitizeAndClamp(mix_buffer_, 1.0f, frames);

// Apply space effect using common class (NEON-optimized)
space_widener_->ProcessMonoBatch(mix_buffer_, left_buffer_, right_buffer_, frames);

// Interleave to output
for (uint32_t i = 0; i < frames; ++i) {
    out[i * 2] = left_buffer_[i] + 1.0e-15f;
    out[i * 2 + 1] = right_buffer_[i] + 1.0e-15f;
}
```

## Why Use common/stereo_widener.h?

The common implementation provides:

1. **Code Reuse** - Shared across all drumlogue units
2. **NEON Optimization** - Batch processing of 4 samples at a time on ARM
3. **Consistency** - Same stereo imaging across different synths
4. **Maintainability** - Bug fixes and improvements benefit all units
5. **Flexibility** - Easy to adjust parameters (width, LFO rate, depth)
6. **Well-tested** - Proven implementation used by elementish-synth

### Performance Benefits

The NEON-optimized batch processing in `AnimatedStereoWidener::ProcessMonoBatch()`:
- Processes 4 samples simultaneously using ARM NEON SIMD
- Computes triangle LFO for 4 phases in parallel
- Applies stereo panning vectorized
- Much faster than scalar per-sample processing

## Sonic Character

The space effect provides:
- **Subtle stereo width** - Not exaggerated, natural sounding
- **Animated stereo field** - Slow LFO creates gentle movement
- **Maintains mono compatibility** - Proper M/S ensures phase coherence
- **Frequency-independent** - Works consistently across all frequencies

This is exactly how Elements creates its stereo image and is now available in drupiter-synth at 50% for all sounds.

## Comparison to Elementish-Synth

The drupiter implementation now uses the **same common class** as elementish-synth could use:
- Same `AnimatedStereoWidener` from `common/stereo_widener.h`
- Same 0.5 Hz LFO rate
- Same triangle LFO waveform  
- Same 50% default width
- Same NEON optimizations

The elementish-synth has its own inline implementation in the resonator because:
1. The resonator natively outputs Mid/Side signals (center + side)
2. It can directly multiply the side signal by the space parameter
3. Historical - implemented before common utilities existed

Both approaches produce the same sonic result, but using the common class is preferred for new units.

## Future Enhancements

If user control is desired later:
1. Add `PARAM_SPACE` to parameter list (would require removing another param to stay at 24)
2. Make `space_widener_->SetWidth()` adjustable via `SetParameter()`
3. Could also expose LFO rate and depth if desired

Currently fixed at 50% to keep the parameter count at 24 and provide good default stereo imaging.

## Testing

The space effect can be heard as:
- Subtle stereo movement when listening in headphones
- Wider soundstage compared to mono
- Gentle left-right animation over ~2 second cycles (0.5 Hz)
- Maintains punch and center when summed to mono
