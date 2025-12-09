# Marbles Integration TODO

Integration of Mutable Instruments Marbles-inspired sequencing into elementish-synth.

## Overview

Add a generative sequencer inspired by Marbles' "déjà vu" concept. Uses 3 parameters on Page 6 (Lightweight mode), replacing FINE tuning.

**Option B: Clock-Synced Subdivisions** (Implemented)
- Pattern step **triggers** the sequencer
- Sequencer generates notes at **subdivisions** of the beat
- Pattern controls rhythm, sequencer adds melodic variation + note bursts

### How It Works

```
Pattern Step (NoteOn) → Trigger(note, velocity)
                       ↓
         Sequencer generates N subdivision notes
         (N depends on preset: SLOW=1, MED=2, FAST=4, X2=8, X4=16)
                       ↓
         Each subdivision → GenerateNote() → synth.NoteOn()
                       ↓
Pattern Step (NoteOff) → Release() stops remaining subdivisions
```

### Example

- Pattern: 4 quarter notes at 120 BPM
- SEQ = FAST (4 subdivisions per beat)
- SPREAD = 64, DEJA VU = 80

Result: Each quarter note becomes 4 sixteenth notes with random pitch variation (but looping due to high DEJA VU).

### New Page 6 Layout

| Slot | Parameter | Range | Type | Description |
|------|-----------|-------|------|-------------|
| 20 | COARSE | -64 to +63 | bipolar | Pitch transpose (keep existing) |
| 21 | SEQ | 0-15 | strings | Sequencer preset (rate + range + scale) |
| 22 | SPREAD | 0-127 | continuous | Note range/spread amount |
| 23 | DEJA VU | 0-127 | continuous | Sequence looping (0=random, 127=locked) |

### SEQ Presets

| Preset | Subdivisions | Scale | Description |
|--------|-------------|-------|-------------|
| OFF | - | - | Sequencer disabled, normal play |
| SLOW | 1 | chromatic | 1 note per beat (pitch randomized) |
| MED | 2 | chromatic | 2 notes per beat |
| FAST | 4 | chromatic | 4 notes per beat (16th notes) |
| X2 | 8 | chromatic | 8 notes per beat (32nd notes) |
| X4 | 16 | chromatic | 16 notes per beat |
| MAJ | 4 | Major | 4 notes, major scale |
| MIN | 4 | Minor | 4 notes, minor scale |
| PENT | 4 | Pentatonic | 4 notes, pentatonic |
| CHROM | 4 | Chromatic | 4 notes, all semitones |
| OCT | 2 | Octaves | 2 notes, octave jumps |
| 5TH | 2 | Fifths | 2 notes, perfect fifths |
| 4TH | 2 | Fourths | 2 notes, perfect fourths |
| TRI | 3 | Triad | 3 notes (triplet feel), triads |
| 7TH | 4 | 7th chord | 4 notes, 7th chord tones |
| RAND | 4 | Random | 4 notes, random scale each step |

---

## Implementation Steps

### Phase 1: Header & Parameter Setup

- [x] **1.1** Update `header.c` Page 6 (Lightweight section)
  - Remove FINE parameter (slot 21)
  - Add SEQ parameter (slot 21): `{0, 15, 0, 0, k_unit_param_type_strings, ...}`
  - Add SPREAD parameter (slot 22): `{0, 127, 0, 64, k_unit_param_type_none, ...}`
  - Add DEJA VU parameter (slot 23): `{0, 127, 0, 0, k_unit_param_type_none, ...}`

- [x] **1.2** Update `elements_synth_v2.h` parameter string handler
  - Add `getParameterStrValue()` case for SEQ (id 21) returning preset names
  - Remove FINE tuning logic if present

### Phase 2: Create Sequencer Module

- [x] **2.1** Create `dsp/marbles_sequencer.h`
  - Header-only implementation (no separate .cc needed)
  - MarblesSequencer class with Init, Process, GetNextNote
  - 16 presets (SEQ_OFF through SEQ_RAND)
  - Scale quantization tables
  - Déjà vu loop buffer (8 steps)

- [x] **2.2** ~~Create `dsp/marbles_sequencer.cc`~~ (Not needed - header-only)

- [x] **2.3** Add scale tables
  - Major, Minor, Pentatonic, Chromatic
  - Octaves, Fifths, Fourths
  - Triads, Sevenths

### Phase 3: Integration

- [x] **3.1** Update `elements_synth_v2.h` class
  - Add `#include "dsp/marbles_sequencer.h"`
  - Add `MarblesSequencer sequencer_;` member (conditional on ELEMENTS_LIGHTWEIGHT)
  - Initialize in `Init()`
  - Update `SetTempo()` to forward to sequencer

- [x] **3.2** Update `Render()` method
  - Process sequencer each frame
  - Get generated notes and trigger NoteOn

- [x] **3.3** Update `applyParameter()` for new params
  - case 21: SEQ preset
  - case 22: SPREAD
  - case 23: DEJA VU

- [x] **3.4** Update `NoteOn()` to set sequencer base note

### Phase 4: Update Config & Build

- [x] **4.1** ~~Update `config.mk`~~ (Not needed - header-only implementation)

- [x] **4.2** Update presets in `elements_synth_v2.h`
  - Updated `setPresetParams()` signature for new params
  - Default: SEQ=0 (OFF), SPREAD=64, DEJA_VU=0

### Phase 5: Testing

- [x] **5.1** Build and verify no compilation errors
  ```bash
  ./build.sh elementish-synth
  ```
  Build successful: 127644 bytes text, 720 data, 92920 bss

- [x] **5.2** Create test harness updates
  - Added `RunSequencerTest()` and `RunSequencerTestSuite()` functions
  - Test tempo sync calculations
  - Test scale quantization (major, minor, pentatonic, octaves, fifths)
  - Test déjà vu looping behavior (random, 50%, locked)
  - Test spread variations (narrow, medium, wide)
  - Test subdivision rates (SLOW, MED, FAST, X2)
  - Test musical patterns (arp, bass, lead)
  
  Run tests:
  ```bash
  cd test/elementish-synth
  make clean && make LIGHTWEIGHT=1
  ./elements_synth_test --seq-test seq_test  # Full suite
  ./elements_synth_test --seq 8 --spread 80 --dejavu 100 --bpm 140 --bars 4 -o custom.wav
  ```

- [ ] **5.3** Hardware testing
  - Verify sequencer syncs to drumlogue tempo
  - Verify DEJA VU creates repeating patterns
  - Verify SPREAD controls note range
  - Verify SEQ presets change behavior
  - Test interaction with MIDI input

---

## Files to Modify

| File | Changes |
|------|---------|
| `header.c` | Replace FINE with SEQ, add SPREAD, DEJA VU |
| `elements_synth_v2.h` | Add sequencer integration, parameter handling |
| `config.mk` | Add new source file |

## Files to Create

| File | Purpose |
|------|---------|
| `dsp/marbles_sequencer.h` | Sequencer class declaration |
| `dsp/marbles_sequencer.cc` | Sequencer implementation |

---

## Technical Notes

### Tempo Conversion

Drumlogue tempo format: `tempo = BPM << 16` (16.16 fixed point)

```cpp
float bpm = (float)tempo / 65536.0f;
float beats_per_second = bpm / 60.0f;
float samples_per_beat = sample_rate_ / beats_per_second;
phase_increment_ = 1.0f / samples_per_beat;
```

### Déjà Vu Algorithm

```cpp
float GetNextValue() {
    float random_value = RandomFloat();  // 0.0 to 1.0
    
    if (RandomFloat() < deja_vu_) {
        // Replay from loop
        return loop_buffer_[loop_index_];
    } else {
        // New random, store in loop
        loop_buffer_[loop_index_] = random_value;
        return random_value;
    }
    
    loop_index_ = (loop_index_ + 1) % loop_length_;
}
```

### Scale Quantization

```cpp
// Convert random 0-1 to note offset within spread range
float voltage = (random_value - 0.5f) * 2.0f * spread_;  // -spread to +spread
int semitones = (int)(voltage * 12.0f);  // Convert to semitones
int quantized = QuantizeToScale(semitones, scale);
return base_note_ + quantized + transpose_;
```

---

## Future Enhancements (Not in Scope)

- [ ] MIDI clock sync (external clock input)
- [ ] Gate length control
- [ ] Velocity randomization
- [ ] Arpeggiator mode (cycle through held notes)
- [ ] Euclidean rhythm patterns
- [ ] Per-step probability
