# Elementish Synth - Release Notes

## v1.4.0

Review-fix release: note transitions, tuning, and string pickup position.

### Bug Fixes

- **Tuning is applied to directly played notes:** COARSE, FINE and pitch bend
  were only applied to sequencer-generated notes; `NoteOn`, `GateOn`, held-note
  retuning and the preset retrigger now use the fully tuned pitch
  (`ClampedFullTunedNote`), so COARSE/FINE/pitch bend work on every note path
- **All-notes-off / panic cleanup:** `AllNoteOff()` now clears the held-note
  state, so a subsequent preset load or retune cannot re-trigger a note after
  the host requested silence
- **STRING/MSTRING POSITION control works:** the pickup position comb was a
  no-op (the read position was invariant to the parameter); the position tap is
  now actually summed into the delay line output, so POSITION changes the
  string timbre as documented
- **Full-mode COARSE retunes a held note** (lightweight mode already did);
  turning COARSE while a note is held now re-pitches it immediately
- **Preset switch no longer goes silent with the sequencer:** if a note is
  held while the sequencer is enabled and the new preset disables it (SEQ
  OFF), the retrigger falls through to a direct note instead of feeding the
  disabled sequencer
- **Preset load / note transition stability:** voice runtime state is reset
  and the held note retriggered on preset switch (sound engine no longer stops
  mid-note; note-transition distortion reduced)
- **Sequencer state leaks:** pending notes are dropped on note-off,
  all-notes-off, panic and preset change; interval-scale quantization no
  longer wraps to the wrong octave
- **Reverb diagnostics:** `debug_last_wet_` is now tracked on host builds so
  reverb instability is visible in the render debug log

### Regression Tests

- `--preset-retrigger-test`: 56 preset transitions (voice must keep sounding
  and stay finite after a preset switch while a note is held)
- `--bow-stability-test`: repeated bow note transitions stay finite and
  audible
- `--gate-test`: drumlogue pattern-sequencer simulation with correct COARSE
  transpose mapping
- Test CLI fix: `--cutoff`/`--resonance` consume their value argument in
  lightweight builds (previously the value was mis-parsed as the output file)

### Presets Editor

- Native presets-editor builds now apply the unit's `config.mk` macros, so the
  editor loads the same lightweight build that ships on hardware (with the
  Marbles sequencer) instead of the full filter/LFO build
- drupiter-synth can now be built natively on Apple Silicon with its release
  macros (`NEON_DCO` and friends)

---

## v1.3.1

Preset-transition fix.

### Bug Fixes

- **Preset load no longer kills the playing voice:** `LoadPreset()` now resets
  the voice runtime state (envelope segments, exciter/sample-player state) and
  retriggers the currently held note with the new preset, so a preset switch
  while a note is held continues to sound instead of dying out after the stale
  envelope completes
- **Blown preset (4) is audible again:** its SPACE value was below the blow
  tube's oscillation threshold, leaving it near-silent; raised to 80
- **Quartic attack table ends at 1.0** (the table was truncated at ~0.43, so
  attack never reached full level)
- **Sequencer preset changes** now stop active subdivisions and drop pending
  notes (voice boundary), matching `LoadPreset`

### Regression Test

- `--preset-retrigger-test`: holds a note into its decay tail, loads every
  other factory preset, and verifies the voice keeps sounding with finite
  output (all 56 transitions)

---

## v1.3.0

Review-fix release.

### Bug Fixes

- **Sequencer note leak:** pending sequencer notes are dropped on note-off,
  all-notes-off, panic and preset change, so no note can ring out after the
  host requested silence
- **MIDI tuning clamp:** tuned notes are clamped to [0, 127] before the
  uint8_t conversion (out-of-range float casts are undefined behavior)
- **Attack envelope:** the quartic lookup table was non-monotonic and
  overshot 1.0; replaced with a correct, monotonic t⁴ curve
- **Pitch bend:** now bends the currently held note, not just the next one
- **Interval-scale quantization:** OCT/5TH/4TH/TRI no longer wrap to the wrong
  octave (e.g. +11 now maps to +12 instead of +0)
- **STRING/MSTRING resonator:** GEOMETRY now maps to dispersion (STRING) and
  the Elements chord table (MSTRING); POSITION now controls the string pickup
  for both models
- **BLOW exciter:** rewritten as an Elements-style granular sample-player
  source (noise wavetable scan) instead of plain filtered noise; FLOW sets
  restart density
- **SPACE metaparameter:** now drives raw exciter bleed, stereo spread, and a
  reverb tail (reverb amount and time follow the original Elements mapping)
- **FINE tuning:** re-exposed on page 6; DEJA VU moved to page 4
- **Test CLI:** lightweight parameter IDs fixed (--model no longer corrupted
  by --cutoff/--resonance); added --fine

### Page 6 Parameter Changes (Lightweight Mode)

| Parameter | Description |
|-----------|-------------|
| COARSE | Coarse tuning (±24 semitones) |
| FINE | Fine tuning (±100 cents) |
| SEQ | Sequencer preset (0-15) |
| SPREAD | Note range (0-127) |

DEJA VU moved to page 4 (slot after VOLUME).

---

## v1.2.0 (December 2025)

Generative sequencer release with Marbles-inspired note generation.

### New Features

- **Marbles-Inspired Generative Sequencer** (Lightweight mode only)
  - Triggered by drumlogue pattern sequencer or MIDI notes
  - Generates tempo-synced melodic subdivisions
  - 16 presets combining subdivision rate and scale quantization

- **16 Sequencer Presets**
  - OFF: Normal note playback
  - SLOW/MED/FAST: 1/2/4 notes per beat (chromatic)
  - X2/X4: 8/16 notes per beat (extreme subdivisions)
  - MAJ/MIN/PENT/CHROM: Scale-quantized 16th notes
  - OCT/5TH/4TH: Interval-based 8th notes
  - TRI: Triplet feel with triad quantization
  - 7TH: 16th notes quantized to 7th chord tones
  - RAND: Random scale selection per trigger

- **Déjà Vu Loop Buffer**
  - 8-step pattern memory
  - DEJA VU parameter: 0 = fully random, 127 = locked loop
  - Creates repeating melodic patterns from randomness

- **SPREAD Control**
  - Adjusts note range from unison (0) to ±24 semitones (127)
  - Works with scale quantization for musical results

### Technical Improvements

- **Skip-ahead optimization** in Process() - no per-sample iteration
- **Note queue** (8-deep circular buffer) prevents note loss at high subdivisions
- **Gate triggering** - works with drumlogue's built-in pattern sequencer
- **Bounds checking** on preset selection for safety

### Page 6 Parameter Changes (Lightweight Mode)

| Parameter | Description |
|-----------|-------------|
| COARSE | Base note for sequencer (±24 semitones from middle C) |
| SEQ | Sequencer preset (0-15) |
| SPREAD | Note range (0-127) |
| DEJA VU | Pattern looping (0-127) |

### Build Info

- Binary size: ~124KB
- All 16 sequencer presets validated
- Gate and MIDI triggering tested

---

## v1.1.0 (December 2025)

Performance optimization release with ARM NEON SIMD support.

### New Features

- **NEON SIMD Optimizations** - Significant performance improvements on ARM Cortex-A7
  - Modal resonator: ~40% speedup (4-way parallel SVF processing)
  - CosineOscillator batch compute: ~2.4x faster amplitude calculation
  - Stereo soft clamp: 2x faster output processing
  - FastTanh saturation: ~20% speedup (4 values simultaneously)

### Technical Improvements

- **Structure-of-Arrays (SoA) layout** for mode coefficients eliminates scatter-gather overhead in NEON loop
- **NaN protection and stability checks** in NEON mode processing loop
- **Optimized CosineOscillator::Next4()** batch method for modal amplitude modulation
- **Vectorized stereo output processing** using float32x2_t

### Internal Changes

- Moved NEON DSP utilities to shared `common/neon_dsp.h` library
- Added SyncSoAFromModes()/SyncSoAToModes() for efficient coefficient sync
- Coefficient sync only happens when parameters change (not per-sample)

### Build Info

- Binary size: ~124KB (minimal increase for NEON paths)
- All 8 presets validated
- All 3 resonator modes (MODAL/STRING/MSTRING) tested

---

## v1.0.0 (December 2025)

Initial release of Elementish Synth for Korg drumlogue.

### Features

- **Modal Synthesis Engine** - Port of Mutable Instruments Elements
- **Three Excitation Types:**
  - BOW: Continuous friction/bowing with adjustable timbre
  - BLOW: Granular noise/breath with flow control
  - STRIKE: Percussive impacts (5 modes: SAMPLE, GRANULAR, NOISE, PLECTRUM, PARTICLE)
- **Three Resonator Models:**
  - MODAL: Classic 8-partial modal resonator
  - STRING: Karplus-Strong string synthesis
  - MSTRING: 5 sympathetic strings for rich harmonic content
- **12 Mallet Types:** SOFT/MED/HARD/PLEC/STIK/BOW × DK(dark)/BR(bright)
- **8 Factory Presets:** Init, Bowed Str, Bell, Pluck, Blown, Marimba, String, Drone
- **Full MIDI Support:** Velocity-sensitive dynamics, pitch bend (+/- 2 semitones)
- **24 Parameters** across 6 pages
- **Stereo Width Control** via SPACE parameter

### Technical Details

- Sample Rate: 48kHz (native drumlogue)
- Processing: Stereo output
- Optimized for Cortex-A7 with NEON disabled in initial release

### Credits

- DSP Algorithms based on [Mutable Instruments Elements](https://mutable-instruments.net/modules/elements/) by Émilie Gillet
- Original License: MIT License
