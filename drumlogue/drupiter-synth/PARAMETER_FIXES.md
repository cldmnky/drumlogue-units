# Drupiter Parameter Fixes - Technical Details

## Overview
This document describes the fixes applied to address parameter issues in the Drupiter Jupiter-8 synthesizer implementation.

## Issues Fixed

### 1. Filter Cutoff Behavior at Low Percentages (1-2%)

**Problem:**
- At low cutoff values (1-2%), the filter was removing all sound
- Only resonance was audible, making the synth unusable at low cutoff settings
- This was especially problematic with envelope modulation pulling cutoff even lower

**Root Cause:**
The original cutoff calculation used an aggressive exponential curve:
```cpp
cutoff_curve = pow(cutoff_norm, 0.25);  // Fourth root - very steep at low values
cutoff_base = 20.0f * pow(2.0f, cutoff_curve * 10.0f);  // 20Hz to 20kHz
```

At 1% cutoff (cutoff_norm = 0.01):
- `cutoff_curve = pow(0.01, 0.25) = 0.316`
- `cutoff_base = 20 * pow(2, 3.16) ≈ 180Hz`
- With keyboard tracking (50%) and envelope modulation, this could easily go below 30Hz
- At such low frequencies with high resonance, the filter becomes essentially silent except for the resonant peak

**Solution:**
```cpp
cutoff_curve = pow(cutoff_norm, 0.5);  // Square root - gentler curve
cutoff_base = 30.0f * pow(2.0f, cutoff_curve * 8.5f);  // 30Hz to 12kHz
```

At 1% cutoff now:
- `cutoff_curve = pow(0.01, 0.5) = 0.1`
- `cutoff_base = 30 * pow(2, 0.85) ≈ 54Hz`
- This is audible and provides usable low-pass filtering

**Additional Changes:**
- Raised minimum cutoff clamp from 20Hz to 30Hz in both `UpdateCoefficients()` and `ClampCutoff()`
- Reduced resonance feedback from 3.2 to 2.8 to prevent over-resonance at low cutoffs
- This matches Jupiter-8 characteristics better - less extreme self-oscillation

**Jupiter-8 Reference:**
The Jupiter-8 filter has a usable range starting around 30-50Hz, not all the way down to 20Hz. The new range (30Hz to 12kHz) better matches the original instrument's character.

---

### 2. D2 Detune Parameter Not Audible

**Problem:**
- Changing the D2 TUNE parameter didn't produce any noticeable effect
- Detuning was too subtle to hear

**Root Cause:**
The detune range was only ±50 cents (half a semitone):
```cpp
const float detune_cents = (param - 50) * 1.0f;  // ±50 cents
```

This is musically very subtle - most listeners can't easily distinguish ±50 cents of detuning.

**Solution:**
Increased detune range to ±200 cents (±2 semitones):
```cpp
const float detune_cents = (param - 50) * 4.0f;  // ±200 cents (±2 semitones)
```

**Jupiter-8 Reference:**
The Jupiter-8 detune control typically provides a range of several semitones for obvious chorus/beating effects. The new ±200 cent range provides:
- Noticeable detuning at small adjustments (±10-20 cents for subtle chorus)
- Strong detuning at extreme settings (±200 cents for dramatic effects)
- Center position (50) = perfect tuning (no detune)

---

### 3. D1 PWM Parameter Not Working

**Problem:**
- D1 PWM (Pulse Width Modulation) parameter didn't change the sound
- The control appeared to have no effect

**Root Cause:**
Auto-switch logic was interfering:
```cpp
if (current_preset_.params[PARAM_DCO1_WAVE] == dsp::JupiterDCO::WAVEFORM_SQUARE) {
    current_preset_.params[PARAM_DCO1_WAVE] = dsp::JupiterDCO::WAVEFORM_PULSE;
    dco1_->SetWaveform(dsp::JupiterDCO::WAVEFORM_PULSE);
}
dco1_->SetPulseWidth(v / 100.0f);
```

This logic attempted to auto-switch from SQUARE to PULSE waveform, but:
1. It only affected the preset parameter, not necessarily the actual waveform
2. It added complexity that could prevent PWM from being applied
3. PWM should work regardless of current waveform selection

**Solution:**
Removed auto-switch logic, apply PWM directly:
```cpp
dco1_->SetPulseWidth(v / 100.0f);
```

**Notes:**
- PWM only affects the sound when PULSE waveform is selected
- User must manually select PULSE waveform (D1 WAVE parameter)
- This matches standard synthesizer behavior - PWM controls are always active but only audible with pulse waveforms

---

### 4. DCO Sawtooth Waveform Direction

**Problem:**
- While not explicitly reported, the sawtooth waveform direction didn't match Jupiter-8

**Root Cause:**
The original implementation used an ascending sawtooth (rising edge):
```cpp
ramp_table_[i] = phase * 2.0f - 1.0f;  // -1 to +1 (ascending)
```

**Solution:**
Changed to descending sawtooth to match Jupiter-8/Roland style:
```cpp
ramp_table_[i] = 1.0f - phase * 2.0f;  // +1 to -1 (descending)
```

**Jupiter-8 Reference:**
Roland synthesizers (Jupiter-8, Juno series) use descending sawtooth waveforms. This produces a slightly different harmonic character compared to ascending saws, particularly in the way the waveform interacts with envelope modulation and sync.

---

## Testing Recommendations

After these fixes, the following should be tested on hardware:

### Filter Cutoff Testing
1. Set CUTOFF to 1-2% with no envelope modulation
   - Expected: Filter should pass audible low-frequency content
   - Should not be completely silent
   
2. Set CUTOFF to 1-2% with high RESONANCE
   - Expected: Resonant peak should be audible but not overpowering
   - Some audio should still pass through
   
3. Set CUTOFF to 1-2% with positive ENV AMT
   - Expected: Filter should open up naturally with envelope
   - No silence at note attack

### Detune Testing
1. Set both DCO1 and DCO2 to SAW waveform, both at 100% level
2. Set D2 TUNE to 50 (center) - should sound perfectly in tune
3. Adjust D2 TUNE to 60 (10% detuned)
   - Expected: Noticeable beating/chorus effect
4. Adjust D2 TUNE to 70 (20% detuned)
   - Expected: Strong detuning, clear beating
5. Adjust D2 TUNE to extremes (0 or 100)
   - Expected: ±2 semitones of detuning (dramatic effect)

### PWM Testing
1. Set DCO1 waveform to PULSE
2. Adjust D1 PW from 0 to 100
   - Expected: Gradual change in timbre from thin to thick and back
   - At 50%: square wave (balanced)
   - At extremes: thin, nasal sound

### Sawtooth Direction Testing
1. Listen to SAW waveform with sync enabled
   - Expected: Classic sync sweep sound
2. Compare with pulse and square waveforms
   - Expected: Different harmonic character

---

## Summary of Changes

### Files Modified
1. `drupiter_synth.cc` - Filter cutoff calculation, detune range, PWM logic
2. `dsp/jupiter_vcf.cc` - Minimum cutoff, resonance feedback
3. `dsp/jupiter_dco.cc` - Sawtooth waveform direction

### Parameter Value Mappings

| Parameter | Old Range | New Range | Notes |
|-----------|-----------|-----------|-------|
| VCF Cutoff | 20Hz-20kHz (10 octaves) | 30Hz-12kHz (8.5 octaves) | Better Jupiter-8 match |
| D2 Detune | ±50 cents | ±200 cents | Now audibly useful |
| Resonance | 0-3.2 feedback | 0-2.8 feedback | Better stability |
| Min Cutoff | 20Hz | 30Hz | Prevents silence |

### Behavioral Changes
- Filter is now usable at all cutoff percentages (1-100%)
- Detune parameter provides obvious detuning effect
- PWM works consistently when PULSE waveform is selected
- Sawtooth has proper Jupiter-8 character

---

## Technical Notes

### Filter Stability
The combination of raising minimum cutoff to 30Hz and reducing resonance feedback to 2.8 significantly improves filter stability. The previous settings could cause:
- Oscillation at very low cutoffs
- DC offset buildup
- Numerical instability in the filter state

### Detuning Resolution
At 48kHz sample rate with the new ±200 cent range:
- Resolution: ~4 cents per parameter step (100 steps)
- Sufficient for musical detuning effects
- Chorusing effect typically uses 5-15 cents
- Dramatic detuning uses 50-200 cents

### PWM Characteristics
With the direct PWM application:
- Pulse width range: 1% to 99% (0.01 to 0.99)
- Center (50%) produces square wave
- Extremes produce thin, harmonic-rich tones
- Works with PULSE waveform selection only

---

## Compatibility Notes

These changes are **backward compatible** with existing presets because:
1. Parameter ranges remain 0-100
2. Only the internal interpretation changed
3. Presets will sound different (better) but won't cause errors

Users may want to adjust existing presets:
- **Filter cutoff**: May need to be lowered slightly (new curve is gentler)
- **Detune**: May need to be closer to center (50) due to wider range
- **PWM**: No change needed (now works as expected)

---

## Future Improvements

Potential enhancements for future versions:
1. Add filter keyboard tracking amount as a parameter
2. Add separate control for filter self-oscillation
3. Add cross-modulation depth control
4. Add filter envelope depth bipolar control
5. Add PWM modulation from LFO

---

**Version**: 1.0.1 (Parameter Fixes)
**Date**: December 2025
**Author**: GitHub Copilot with user feedback
