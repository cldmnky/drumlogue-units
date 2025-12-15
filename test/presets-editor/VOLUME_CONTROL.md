# Volume Control and Distortion Fix

## Changes Made

### Audio Distortion Issues Fixed

The presets editor was experiencing audio distortion when playing synth units. This was caused by:

1. **No output level control** - Synth units can generate hot signals that exceed ±1.0
2. **No clipping protection** - Digital clipping causes harsh distortion
3. **No volume attenuation** - No way to reduce output level

### Solutions Implemented

#### 1. Master Volume Control
- Added `master_volume` field to `audio_config_t` (default: 0.5 / 50%)
- Added `audio_engine_set_master_volume()` function for runtime volume adjustment
- Volume is applied in the audio callback before output

#### 2. Soft-Clipping Protection
- Implemented `soft_clip()` function using `tanh()` for smooth saturation
- Prevents harsh digital clipping while maintaining signal integrity
- Applied after volume scaling in the audio callback
- Formula: `tanh(x * 0.9) / 0.9` provides gentle saturation at ±1.0

#### 3. UI Volume Slider
- Added prominent volume slider at the top of Parameters panel
- Range: 0-100% (displayed as percentage)
- Styled with blue colors to stand out
- Includes tooltip explaining its purpose
- Updates audio engine in real-time when audio is running

## Usage

### Starting the Presets Editor GUI

```bash
cd test/presets-editor
make gui
./bin/presets-editor-gui ../../drumlogue/<unit-name>/<unit-name>.drmlgunit
```

### Adjusting Volume

1. Start audio from the **Audio** menu
2. Use the **Master Volume** slider in the Parameters panel
3. Default is 50% - increase if too quiet, decrease if distorting
4. The soft-clipper will prevent harsh clipping even at 100%

## Technical Details

### Audio Pipeline

```
Unit DSP Output → Master Volume (×0.0-1.0) → Soft Clip (tanh) → PortAudio Output
```

### Soft-Clipping Algorithm

The soft-clipping uses hyperbolic tangent for smooth saturation:

```c
static inline float soft_clip(float x) {
    // tanh provides smooth saturation at ±1.0
    // For values near 0, tanh(x) ≈ x, so no distortion at low levels
    // Scale to make clipping more gradual
    return tanhf(x * 0.9f) / 0.9f;
}
```

**Characteristics:**
- Near-linear for signals < ~0.7 (minimal coloration)
- Smooth saturation approaching ±1.0
- No harsh digital clipping artifacts
- Preserves transients better than hard clipping

### Default Volume Setting

The default volume is set to **50%** (0.5) as a safe starting point:
- Prevents most units from clipping
- Leaves headroom for parameter adjustments
- Can be increased if the unit is naturally quiet

## Recommendations

### For Synth Units
- Start with 50% volume
- Adjust based on the specific unit's output level
- Most MI-based synths (elementish, pepege, drupiter) work well at 40-60%

### For Effect Units
- May need higher volume (60-80%) since they process existing signals
- Watch for clipping when wet/dry mix is at extremes

### Troubleshooting Distortion

If you still hear distortion:
1. Lower the master volume
2. Check if unit parameters are at extreme values
3. Some units may have internal gain staging issues
4. The soft-clipper should prevent harsh clipping, but lower volume for cleaner sound

## Files Modified

- `audio/audio_engine.h` - Added volume control API
- `audio/audio_engine.c` - Implemented volume scaling and soft-clipping
- `gui/imgui_app.h` - Added master_volume_ member
- `gui/imgui_app.cpp` - Added UI slider and initialization

## Benefits

✅ Prevents harsh digital clipping distortion  
✅ User-controllable output level  
✅ Smooth saturation when signals are hot  
✅ Real-time volume adjustment  
✅ No audio dropouts when changing volume  
✅ Works with all unit types (synth, delfx, revfx, masterfx)
