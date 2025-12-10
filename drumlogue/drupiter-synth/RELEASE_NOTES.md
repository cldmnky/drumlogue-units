# Drupiter Release Notes

## Version 1.0.0 (December 10, 2025)

**Initial Release** - Jupiter-8 Inspired Monophonic Synthesizer for Korg Drumlogue

### Overview

Drupiter v1.0.0 is the first public release of a Jupiter-8 inspired monophonic synthesizer unit for the Korg drumlogue. This release represents a complete port of select Bristol Jupiter-8 DSP algorithms optimized for the drumlogue's ARM Cortex-M7 platform.

---

## 🎹 Features

### Oscillators
- **Dual DCO architecture** with independent control
- **4 waveforms per oscillator**: Ramp, Square, Pulse (variable width), Triangle
- **Octave switching**: 16', 8', 4' for each DCO
- **Detune control**: ±10 cents for DCO2
- **Hard sync**: DCO1 synchronizes DCO2 for classic sync timbres
- **Cross-modulation**: DCO2 → DCO1 FM (depth scales with DCO2 level)

### Filter
- **Multi-mode Chamberlin state-variable filter**
  - LP12: 12dB/oct low-pass (warm, classic)
  - LP24: 24dB/oct low-pass (steep, modern)
  - HP12: 12dB/oct high-pass (bright, thin)
  - BP12: 12dB/oct band-pass (vocal, nasal)
- **Self-oscillating resonance** at maximum setting
- **Exponential Q curve** for natural resonance control
- **Gain compensation** maintains consistent volume across resonance range
- **Keyboard tracking** (50% default) for natural pitch-following behavior

### Envelopes
- **Dual ADSR envelopes** (VCF and VCA)
- **Linear segments** optimized for CPU efficiency
- **Time range**: 1ms to 5 seconds per stage
- **Velocity sensitivity** for dynamic expression
- **Retriggerable** (doesn't reset on new note)

### LFO
- **4 waveforms**: Triangle, Ramp, Square, Sample & Hold
- **Frequency range**: 0.1Hz to 50Hz
- **Delay envelope** for gradual LFO fade-in
- **Dual routing**: LFO → VCO (vibrato) and LFO → VCF (filter sweeps)
- **Retriggerable** on note-on

### Modulation
- **LFO → VCO**: ±5% frequency modulation (vibrato)
- **LFO → VCF**: ±2 octaves cutoff modulation
- **ENV → VCF**: ±4 octaves cutoff modulation (bipolar)
- **ENV → VCA**: Full amplitude control
- **Combined modulation**: ENV and LFO sum for complex filter movement

### Presets
- **6 factory presets** covering common synthesis applications:
  1. **Init** - Clean starting point
  2. **Bass** - Deep, punchy bass with high resonance
  3. **Lead** - Bright, cutting lead with vibrato
  4. **Pad** - Lush, evolving pad with detuned DCOs
  5. **Brass** - Bold brass-like stab sound
  6. **Strings** - String ensemble with slow attack

---

## 🔧 Technical Details

### Performance
- **CPU Usage**: ~13% estimated (580 cycles/sample @ 48kHz)
- **Binary Size**: 19,740 bytes
- **Memory**: Static allocation only (no heap fragmentation)
- **Latency**: Zero added latency
- **Headroom**: 87% CPU remaining for other units/effects

### Architecture
- **DSP**: C++14, ARM Cortex-M7 optimized
- **Oscillators**: 2048-sample wavetables with linear interpolation
- **Filter**: Chamberlin state-variable algorithm
- **Envelopes**: Linear ADSR with per-sample processing
- **Sample Rate**: 48kHz (fixed)
- **Precision**: 32-bit float processing

### Parameters
- **Total**: 24 parameters
- **Layout**: 6 pages × 4 parameters
- **Navigation**: Use drumlogue page buttons
- **Range**: 0-127 (MIDI-style) for most parameters
- **String parameters**: Waveform selection, filter type, LFO waveform

---

## 📝 Implementation Notes

### Based on Bristol Jupiter-8
Drupiter is inspired by the Bristol Jupiter-8 emulation, specifically:
- **junodco.c**: DCO wavetable synthesis (adapted from Juno-style DCO)
- **filter.c**: Chamberlin state-variable filter
- **envelope.c**: ADSR envelope generators
- **lfo.c**: LFO oscillator with multiple waveforms

### Differences from Bristol
- **Monophonic** (drumlogue SDK limitation)
- **Linear envelopes** (vs. Bristol's exponential for CPU efficiency)
- **50% keyboard tracking** (vs. Bristol's variable parameter)
- **Implicit cross-modulation** (depth tied to DCO2 level)
- **Always-on hard sync** (vs. Bristol's switchable sync)

### Optimizations
- **Wavetable pre-computation**: Static tables initialized once
- **Linear interpolation**: Efficient sample-rate conversion
- **Coefficient caching**: Filter coefficients only update on parameter change
- **Stack allocation**: No dynamic memory in audio thread
- **No NEON SIMD**: Deferred (13% CPU usage doesn't require it)

---

## 🐛 Known Issues & Limitations

### By Design
- **Monophonic only**: Drumlogue SDK limitation (one voice per unit)
- **No noise generator parameter**: Noise generator present but not exposed in UI (can be enabled in future version)
- **Linear envelopes**: Not exponential like analog (optimization trade-off)
- **Fixed keyboard tracking**: 50% hardcoded (could be parameter in future)

### Potential Issues
- **Filter self-oscillation**: At RESO > 110, filter produces pure tone (this is a feature, but may surprise users)
- **High CPU with multiple units**: While Drupiter uses ~13%, combining with other CPU-heavy units may cause dropouts
- **No arpeggiator integration**: Drumlogue's arpeggiator works but doesn't interact with LFO/envelopes

### Hardware Testing
**⚠️ Note**: This release has been extensively developed and tested via build verification, but has not yet been validated on actual drumlogue hardware. Users may encounter unexpected behavior. Please report issues via GitHub.

---

## 🎯 Design Decisions

### Why Monophonic?
Drumlogue's user unit SDK is designed for monophonic operation. While the hardware supports polyphony, it's managed at a higher level outside the user unit scope.

### Why Linear Envelopes?
Exponential envelopes (like analog synthesizers) are computationally expensive. Linear envelopes provide 90% of the sonic character at 50% of the CPU cost, leaving more headroom for other effects.

### Why 50% Keyboard Tracking?
Jupiter-8 typically runs keyboard tracking around 50% for filter following. This creates a natural sound where higher notes are brighter. Making this a parameter would consume one of the limited 24 slots.

### Why Implicit Cross-Modulation?
Tying cross-modulation depth to DCO2 level saves a parameter and creates intuitive behavior: more DCO2 = more FM timbres. This matches the Jupiter-8's architecture where oscillator balance affects modulation.

### Why No NEON Optimization?
At 13% CPU usage, there's no need for SIMD optimization yet. Keeping the code simple and portable makes debugging easier and leaves optimization as a future option if needed.

---

## 📦 File Manifest

```
drumlogue/drupiter-synth/
├── drupiter_synth.drmlgunit    # Binary unit file (install this)
├── README.md                    # User documentation
├── RELEASE_NOTES.md            # This file
├── PROGRESS.md                  # Development log
├── PORT.md                      # Technical port plan
├── header.c                     # Unit metadata
├── unit.cc                      # SDK callbacks
├── config.mk                    # Build configuration
├── Makefile                     # Build system
├── drupiter_synth.h            # Main synth header
├── drupiter_synth.cc           # Main synth implementation
└── dsp/                        # DSP components
    ├── jupiter_dco.h/.cc       # Oscillator
    ├── jupiter_vcf.h/.cc       # Filter
    ├── jupiter_env.h/.cc       # Envelope
    └── jupiter_lfo.h/.cc       # LFO
```

---

## 🚀 Installation Instructions

1. **Download**: Get `drupiter_synth.drmlgunit` from the release
2. **Connect**: USB cable from drumlogue to computer
3. **Mount**: Drumlogue appears as USB drive
4. **Copy**: Place `.drmlgunit` in `Units/` folder
5. **Eject**: Safely eject drumlogue
6. **Power Cycle**: Turn drumlogue off and on
7. **Select**: Navigate to "Drupiter" in synth selection

---

## 🎓 Quick Start Guide

### Your First Sound

1. **Load Init preset**: Use preset #1
2. **Play a note**: Should hear a clean square wave
3. **Adjust filter**: 
   - Page 3: Lower CUTOFF to ~50
   - Page 3: Increase RESO to ~60
4. **Add envelope**:
   - Page 3: Increase ENV AMT to ~90
   - Page 4: Set F.ATK=0, F.DCY=40, F.SUS=30, F.REL=30
5. **Result**: Classic filter sweep bass sound

### Exploring Sync

1. **Load Lead preset**: Use preset #3
2. **Detune DCO2**:
   - Page 2: Set D2 TUNE to 70-90 (high detune)
3. **Play**: Note the aggressive, metallic timbre
4. **Sweep detune**: Move D2 TUNE while holding a note
5. **Result**: Classic hard sync sweep

### Creating Movement

1. **Load Pad preset**: Use preset #4
2. **Add LFO to filter**:
   - Page 6: Set LFO RATE to ~30
   - Page 6: Set LFO>VCF to ~50
3. **Choose waveform**:
   - Page 6: Try different LFO WAV settings
4. **Result**: Evolving, animated pad sound

---

## 🔮 Future Roadmap

### Potential v1.1 Features
- **Noise level parameter** (expose existing noise generator)
- **PWM via LFO** (pulse width modulation)
- **Filter envelope velocity** (velocity → env depth)
- **Sync on/off parameter** (switchable hard sync)
- **Additional presets** (user-contributed)

### Potential v2.0 Features
- **Dual-layer mode** (if SDK allows multi-channel MIDI)
- **Additional filter types** (Moog ladder, Oberheim SVF)
- **Unison mode** (multiple detuned voices for fat mono)
- **Built-in chorus** (simple delay-based chorus)
- **LFO sync to tempo** (if drumlogue provides tempo info)

*Note: Future features depend on SDK capabilities and CPU budget*

---

## 📜 License

**MIT License**

Copyright (c) 2025 CLDM

Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files (the "Software"), to deal in the Software without restriction, including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

---

## 🙏 Credits

### Development
- **Port Developer**: CLDM
- **Original Bristol**: Nick Copeland
- **Platform**: Korg logue SDK
- **Reference**: Bristol Jupiter-8 emulation

### Thanks
- Bristol synthesizer project for the reference implementation
- Korg for the logue SDK and drumlogue platform
- Roland Corporation for the original Jupiter-8 inspiration (not affiliated)

---

## 📞 Support & Feedback

**Bug Reports**: https://github.com/cldmnky/drumlogue-units/issues  
**Discussions**: https://github.com/cldmnky/drumlogue-units/discussions  
**Repository**: https://github.com/cldmnky/drumlogue-units

---

## ⚖️ Trademarks

"Jupiter-8", "Roland", and "Roland Jupiter-8" are registered trademarks of Roland Corporation. "Korg" and "drumlogue" are registered trademarks of Korg Inc. This project is an independent software implementation inspired by the Jupiter-8 synthesizer architecture and is not affiliated with, endorsed by, or sponsored by Roland Corporation or Korg Inc.

---

**Released**: December 10, 2025  
**Version**: 1.0.0  
**Status**: Initial Public Release  

**Happy Synthesizing!** 🎹✨
