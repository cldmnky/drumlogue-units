# Drupiter Parameter Fixes - Testing Guide

This guide provides step-by-step testing procedures for the parameter fixes implemented in version 1.0.1.

## Prerequisites

- Korg drumlogue with latest firmware
- USB connection for loading `.drmlgunit` file
- Audio monitoring (headphones or speakers)
- MIDI controller or drumlogue's built-in sequencer

## Test Procedures

### Test 1: Filter Cutoff at Low Percentages

**Purpose**: Verify that the filter doesn't go silent at low cutoff values

**Procedure**:
1. Load the Drupiter unit
2. Select the "Init" preset or create a new patch
3. Set parameters:
   - DCO1 WAVE: SAW
   - DCO1 LEVEL: 100%
   - DCO2 LEVEL: 0%
   - VCF TYPE: LP24
   - CUTOFF: Start at 50%
   - RESO: 50%
   - ENV AMT: 50% (no modulation)
   - F.ATK: 0%
   - F.DCY: 0%
   - F.SUS: 100%
   - F.REL: 0%

4. Play a note (middle C / note 60)
5. Slowly decrease CUTOFF from 50% down to 1%

**Expected Results**:
- ✓ Sound remains audible throughout the entire range
- ✓ At 1-2%, you should hear clear low-pass filtering (muffled sound)
- ✓ Resonance should be present but not overwhelming
- ✓ NO complete silence except for very brief moments
- ✓ Filter should smoothly darken the sound as cutoff decreases

**Old Behavior (v1.0.0)**:
- ✗ Sound disappears at ~1-5% cutoff
- ✗ Only resonant peak is audible
- ✗ Filter becomes unusable at low settings

---

### Test 2: Filter Cutoff with High Resonance

**Purpose**: Verify filter stability at low cutoff with high resonance

**Procedure**:
1. Continue from Test 1
2. Set RESO to 100%
3. Set CUTOFF to 1%
4. Play a sustained note

**Expected Results**:
- ✓ Resonant peak is audible
- ✓ Some fundamental tone is still present
- ✓ No instability or clicking
- ✓ Filter doesn't oscillate out of control
- ✓ Sound remains musical

**Old Behavior (v1.0.0)**:
- ✗ Only resonance audible, all signal removed
- ✗ Filter could become unstable

---

### Test 3: Filter Cutoff with Envelope Modulation

**Purpose**: Verify filter opens properly with envelope modulation

**Procedure**:
1. Set parameters:
   - CUTOFF: 1%
   - RESO: 30%
   - ENV AMT: 75% (positive modulation)
   - F.ATK: 5%
   - F.DCY: 40%
   - F.SUS: 30%
   - F.REL: 20%

2. Play notes and listen to the filter sweep

**Expected Results**:
- ✓ Filter opens from low to high with each note
- ✓ Clear "wah" envelope sweep
- ✓ Attack phase is audible (not silent)
- ✓ Decay and sustain are smooth
- ✓ No clicking or artifacts

**Old Behavior (v1.0.0)**:
- ✗ Initial attack might be silent
- ✗ Envelope sweep less effective

---

### Test 4: D2 Detune - Subtle Detuning

**Purpose**: Verify detune produces audible chorus/beating

**Procedure**:
1. Set parameters:
   - DCO1 WAVE: SAW
   - DCO1 LEVEL: 100%
   - DCO1 OCTAVE: 8'
   - DCO2 WAVE: SAW
   - DCO2 LEVEL: 100%
   - DCO2 OCTAVE: 8'
   - D2 TUNE: 50% (centered - no detune)
   - VCF TYPE: LP24
   - CUTOFF: 100%
   - RESO: 0%

2. Play a sustained note
3. Note the static sound (perfectly in tune)
4. Adjust D2 TUNE to 55% (5% detuned)
5. Listen for beating/chorus effect

**Expected Results**:
- ✓ At 50%: Perfect unison, static tone
- ✓ At 55%: Noticeable slow beating/warble (chorus effect)
- ✓ Beating rate: approximately 1-3 beats per second
- ✓ Sound becomes thicker and more animated

**Old Behavior (v1.0.0)**:
- ✗ No audible difference between 50% and 55%
- ✗ Detune too subtle to hear

---

### Test 5: D2 Detune - Strong Detuning

**Purpose**: Verify detune provides wide range for dramatic effects

**Procedure**:
1. Continue from Test 4
2. Adjust D2 TUNE to 70% (20% detuned)
3. Then to 100% (maximum detune)

**Expected Results**:
- ✓ At 70%: Strong detuning, fast beating (5-10 beats/second)
- ✓ At 100%: Dramatic detuning, approximately 2 semitones higher
- ✓ Creates interval effect (not just chorus)
- ✓ Very obvious timbral change

**Old Behavior (v1.0.0)**:
- ✗ Even at 100%, detune barely audible

---

### Test 6: D2 Detune - Center Position

**Purpose**: Verify 50% is perfectly centered (no detune)

**Procedure**:
1. Set D2 TUNE to exactly 50%
2. Play a sustained note
3. Listen for any beating or movement

**Expected Results**:
- ✓ Completely static tone
- ✓ No beating or warble
- ✓ Perfect unison between DCO1 and DCO2

---

### Test 7: D1 PWM Basic Functionality

**Purpose**: Verify PWM parameter affects PULSE waveform

**Procedure**:
1. Set parameters:
   - DCO1 WAVE: PULSE (important!)
   - DCO1 LEVEL: 100%
   - DCO2 LEVEL: 0%
   - D1 PW: 50% (square wave)
   - VCF TYPE: LP24
   - CUTOFF: 100%
   - RESO: 0%

2. Play a sustained note
3. Note the sound (should be square wave - balanced tone)
4. Adjust D1 PW from 50% down to 10%
5. Then from 50% up to 90%

**Expected Results**:
- ✓ At 50%: Square wave sound (balanced, hollow)
- ✓ At 10%: Thin, nasal tone (short pulse)
- ✓ At 90%: Thin, nasal tone (short negative pulse)
- ✓ Continuous, smooth timbral change throughout range
- ✓ Obvious difference between extreme settings

**Old Behavior (v1.0.0)**:
- ✗ D1 PW parameter had no effect
- ✗ Sound didn't change when adjusting PW

---

### Test 8: PWM with Non-Pulse Waveforms

**Purpose**: Verify PWM only affects PULSE waveform

**Procedure**:
1. Set DCO1 WAVE to SAW
2. Adjust D1 PW from 0% to 100%
3. Set DCO1 WAVE to SQR
4. Adjust D1 PW from 0% to 100%
5. Set DCO1 WAVE to TRI
6. Adjust D1 PW from 0% to 100%

**Expected Results**:
- ✓ SAW waveform: D1 PW has no effect (expected)
- ✓ SQR waveform: D1 PW has no effect (expected)
- ✓ TRI waveform: D1 PW has no effect (expected)
- ✓ Only PULSE waveform responds to D1 PW

**Note**: This is correct behavior. PWM only affects pulse waveforms.

---

### Test 9: Sawtooth Waveform Character

**Purpose**: Verify sawtooth has proper Jupiter-8 character

**Procedure**:
1. Set parameters:
   - DCO1 WAVE: SAW
   - DCO1 LEVEL: 100%
   - DCO2 LEVEL: 0%
   - VCF TYPE: LP24
   - CUTOFF: 80%
   - RESO: 20%

2. Play notes across the keyboard
3. Listen to the harmonic content

**Expected Results**:
- ✓ Bright, buzzy sawtooth sound
- ✓ Rich harmonic content
- ✓ Characteristic Roland/Jupiter timbre
- ✓ Responds well to filter sweeps

**Changes in v1.0.1**:
- Sawtooth direction changed from ascending to descending
- May sound subtly different from v1.0.0
- Should match Jupiter-8 reference recordings

---

### Test 10: Combined Filter + Detune

**Purpose**: Verify all fixes work together

**Procedure**:
1. Load the "Pad" preset
2. Play a chord (e.g., C-E-G)
3. Slowly sweep CUTOFF from 100% to 1%
4. Adjust D2 TUNE from 50% to 60%
5. Verify both parameters work correctly

**Expected Results**:
- ✓ Filter sweep is smooth and audible throughout
- ✓ Sound remains present even at 1% cutoff
- ✓ Detune creates obvious chorus/warmth
- ✓ Combined effect is musical and usable

---

## Comparison Table: v1.0.0 vs v1.0.1

| Parameter | v1.0.0 Behavior | v1.0.1 Behavior | Status |
|-----------|----------------|-----------------|--------|
| CUTOFF at 1-2% | Silent, only resonance | Audible low-pass filtering | ✅ Fixed |
| CUTOFF range | 20Hz-20kHz (10 octaves) | 30Hz-12kHz (8.5 octaves) | ✅ Improved |
| D2 TUNE at 55% | No audible change | Obvious chorus/beating | ✅ Fixed |
| D2 TUNE range | ±50 cents | ±200 cents | ✅ Improved |
| D1 PW | No effect | Changes pulse width | ✅ Fixed |
| Sawtooth direction | Ascending | Descending (Jupiter-8) | ✅ Corrected |
| Resonance max | 3.2 (unstable) | 2.8 (stable) | ✅ Improved |

---

## Preset Recommendations

After testing, you may want to adjust existing presets:

### Presets to Update

1. **Bass preset**:
   - Reduce CUTOFF slightly (new curve is gentler)
   - Suggested: 40-50% instead of 50%

2. **Pad preset**:
   - Adjust D2 TUNE closer to center
   - Suggested: 52-54% instead of previous setting
   - Wider detune range means less adjustment needed

3. **Lead preset**:
   - If using PULSE waveform, verify D1 PW is set
   - PWM now works correctly

---

## Known Issues / Expected Behavior

### Not Bugs

1. **PWM only works with PULSE waveform**
   - This is correct behavior
   - User must select PULSE waveform (D1 WAVE = PULSE)
   - Then D1 PW will affect the sound

2. **Detune range is wide**
   - ±200 cents may seem extreme
   - This matches Jupiter-8 character
   - Use values close to 50% for subtle effects
   - Use extremes for dramatic intervals

3. **Filter range is narrower**
   - New range: 30Hz-12kHz (was 20Hz-20kHz)
   - This matches Jupiter-8 specifications
   - Top end at 12kHz is intentional (not a bug)

---

## Troubleshooting

### "Filter still too resonant at low cutoff"
- This is partially by design (Jupiter-8 character)
- Try reducing RESO to 30-50%
- Adjust ENV AMT to be less extreme

### "Detune seems too strong"
- New range is ±200 cents (wider than before)
- Use values between 45-55% for subtle effects
- 50% = no detune (perfect tuning)

### "PWM not working"
- Ensure DCO1 WAVE is set to PULSE (not SQUARE or SAW)
- PWM only affects PULSE waveform

### "Sound different from v1.0.0"
- Sawtooth waveform direction changed
- Filter curve is different
- These changes improve Jupiter-8 accuracy

---

## Reporting Issues

If you encounter problems:

1. Verify you're testing with v1.0.1 or later
2. Follow the exact test procedures above
3. Note which test failed and how
4. Report on GitHub Issues with:
   - Parameter settings used
   - Expected vs actual behavior
   - Audio recording if possible

---

**Version**: 1.0.1  
**Last Updated**: December 13, 2025  
**Testing Duration**: ~15-20 minutes for all tests
