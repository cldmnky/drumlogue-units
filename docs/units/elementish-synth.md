---
layout: unit
title: Elementish Synth
tagline: Modal synthesis voice with bow, blow, and strike exciters and versatile resonator models
unit_type: Synthesizer
version: v1.2.0
filename: elementish_synth.drmlgunit
download_url: https://github.com/cldmnky/drumlogue-units/releases
permalink: /units/elementish-synth/
---

## Overview

A powerful modal synthesis voice for the Korg drumlogue, inspired by **Mutable Instruments Elements**. This unit brings expressive physical modeling synthesis to your drumlogue, combining three excitation sources (bow, blow, strike) with versatile resonator models for rich, evolving timbres.

## Features

- **Modal synthesis engine** based on Mutable Instruments Elements
- **Three excitation types**: Bow (friction), Blow (breath), Strike (percussion)
- **Three resonator models**: Modal (8-partial), String (Karplus-Strong), Multi-String (5 sympathetic strings)
- **NEON SIMD optimizations** for improved ARM Cortex-A7 performance (~40% faster modal resonator)
- **12 mallet types** for percussive variety
- **5 strike modes** including sample, granular, noise, plectrum, and particle
- **Marbles-inspired generative sequencer** (Lightweight mode) with 16 presets
- **8 built-in presets** covering a range of sounds from mallets to strings to drones
- **Full velocity response** with expressive dynamics
- **Pitch bend support** (±2 semitones)
- **Tempo-synced subdivisions** from 1 to 16 notes per beat
- **Déjà vu loop buffer** for repeating melodic patterns

---

## Installation

1. Download the `elementish_synth.drmlgunit` file from [GitHub Releases](https://github.com/cldmnky/drumlogue-units/releases)
2. Connect your drumlogue to your computer via USB
3. Copy the `.drmlgunit` file to the `Units/Synth/` folder on the drumlogue
4. Safely eject the drumlogue and restart it
5. The synth will appear as "Elementish" in the Synth slot selection

---

## Parameters

The unit has 24 parameters organized across 6 pages:

### Page 1 - Exciter Mix

| Parameter | Range | Description |
|-----------|-------|-------------|
| **BOW** | 0-127 | Bow exciter level. Creates continuous friction/bowing sounds |
| **BLOW** | 0-127 | Blow exciter level. Granular noise for breath-like sounds |
| **STRIKE** | 0-127 | Strike exciter level. Percussive impacts |
| **MALLET** | 0-11 | Mallet type selection (see Mallet Types table) |

### Page 2 - Exciter Timbre

| Parameter | Range | Description |
|-----------|-------|-------------|
| **BOW TIM** | -64 to +63 | Bow friction/texture. Negative = smooth, positive = rough |
| **FLOW** | -64 to +63 | Air turbulence for blow exciter |
| **STK MOD** | 0-4 | Strike mode (see Strike Modes table) |
| **DENSITY** | -64 to +63 | Granular density for GRANULAR/PARTICLE strike modes |

### Page 3 - Resonator

| Parameter | Range | Description |
|-----------|-------|-------------|
| **GEOMETRY** | -64 to +63 | Structure shape. Morphs from string → bar → membrane → plate → bell |
| **BRIGHT** | -64 to +63 | High-frequency damping. Negative = wood/muted, positive = glass/bright |
| **DAMPING** | -64 to +63 | Energy dissipation. Negative = quick decay, positive = sustained |
| **POSITION** | -64 to +63 | Excitation point on surface. Affects harmonic content like PWM |

### Page 4 - Model & Space

| Parameter | Range | Description |
|-----------|-------|-------------|
| **MODEL** | 0-2 | Resonator model: MODAL, STRING, or MSTRING |
| **SPACE** | 0-127 | Stereo width. 0 = mono, 127 = wide stereo |
| **VOLUME** | 0-127 | Output level |
| *(unused)* | - | Reserved for future use |

### Page 5 - Envelope

| Parameter | Range | Description |
|-----------|-------|-------------|
| **ATTACK** | 0-127 | Envelope attack time |
| **DECAY** | 0-127 | Envelope decay time |
| **RELEASE** | 0-127 | Envelope release time |
| **CONTOUR** | 0-3 | Envelope mode: ADR, AD, AR, or LOOP |

### Page 6 - Tuning / Sequencer

> **Note**: Page 6 differs between Full and Lightweight modes. Lightweight mode replaces LFO with a generative sequencer.

#### Full Mode (Page 6 - LFO)

| Parameter | Range | Description |
|-----------|-------|--------------|
| **LFO RT** | 0-127 | LFO rate/speed |
| **LFO DEP** | 0-127 | LFO modulation depth |
| **LFO PRE** | 0-7 | LFO shape + destination preset |
| **COARSE** | -64 to +63 | Pitch coarse tune. ±24 semitones range |

#### Lightweight Mode (Page 6 - Sequencer)

| Parameter | Range | Description |
|-----------|-------|--------------|
| **COARSE** | -64 to +63 | Base pitch for sequencer. Sets root note |
| **SEQ** | 0-15 | Sequencer preset (see Sequencer Presets table) |
| **SPREAD** | 0-127 | Note range. 0 = narrow, 127 = ±24 semitones |
| **DEJA VU** | 0-127 | Pattern looping. 0 = random, 127 = locked loop |

---

## Mallet Types

| Value | Name | Description |
|-------|------|-------------|
| 0 | SOFT DK | Soft mallet, dark timbre |
| 1 | SOFT BR | Soft mallet, bright timbre |
| 2 | MED DK | Medium mallet, dark timbre |
| 3 | MED BR | Medium mallet, bright timbre |
| 4 | HARD DK | Hard mallet, dark timbre |
| 5 | HARD BR | Hard mallet, bright timbre |
| 6 | PLEC DK | Plectrum, dark timbre |
| 7 | PLEC BR | Plectrum, bright timbre |
| 8 | STIK DK | Stick, dark timbre |
| 9 | STIK BR | Stick, bright timbre |
| 10 | BOW DK | Bow attack, dark timbre |
| 11 | BOW BR | Bow attack, bright timbre |

---

## Strike Modes

| Value | Mode | Description |
|-------|------|-------------|
| 0 | SAMPLE | Uses sampled mallet/strike sounds |
| 1 | GRANULAR | Granular synthesis strike |
| 2 | NOISE | Filtered noise burst |
| 3 | PLECTRUM | Plucked string excitation |
| 4 | PARTICLE | Particle/dust-like texture |

---

## Resonator Models

| Value | Model | Description |
|-------|-------|-------------|
| 0 | MODAL | Classic Elements 8-partial modal resonator. Rich harmonic content |
| 1 | STRING | Karplus-Strong string synthesis. Clean plucked string sound |
| 2 | MSTRING | 5 sympathetic strings. Rich, shimmering harmonic resonance |

---

## Envelope Modes

| Value | Mode | Description |
|-------|------|-------------|
| 0 | ADR | Attack-Decay-Release (standard envelope) |
| 1 | AD | Attack-Decay only (no sustain) |
| 2 | AR | Attack-Release only (gate-based) |
| 3 | LOOP | Looping envelope for drones |

---

## Sequencer Presets (Lightweight Mode)

The sequencer generates tempo-synced note subdivisions when triggered by the drumlogue pattern sequencer or MIDI notes.

| Value | Name | Subdivision | Scale | Description |
|-------|------|-------------|-------|-------------|
| 0 | OFF | - | - | Sequencer disabled, normal note playback |
| 1 | SLOW | 1/beat | Chromatic | 1 note per beat, chromatic random |
| 2 | MED | 2/beat | Chromatic | 2 notes per beat (8th notes) |
| 3 | FAST | 4/beat | Chromatic | 4 notes per beat (16th notes) |
| 4 | X2 | 8/beat | Chromatic | 8 notes per beat (32nd notes) |
| 5 | X4 | 16/beat | Chromatic | 16 notes per beat (64th notes) |
| 6 | MAJ | 4/beat | Major | 16th notes quantized to major scale |
| 7 | MIN | 4/beat | Minor | 16th notes quantized to minor scale |
| 8 | PENT | 4/beat | Pentatonic | 16th notes quantized to pentatonic |
| 9 | CHROM | 4/beat | Chromatic | 16th notes, full chromatic |
| 10 | OCT | 2/beat | Octaves | 8th notes, octave jumps only |
| 11 | 5TH | 2/beat | Fifths | 8th notes, perfect fifth intervals |
| 12 | 4TH | 2/beat | Fourths | 8th notes, perfect fourth intervals |
| 13 | TRI | 3/beat | Triads | Triplet feel, triad tones |
| 14 | 7TH | 4/beat | 7th Chord | 16th notes, 7th chord tones |
| 15 | RAND | 4/beat | Random | 16th notes, random scale each trigger |

---

## Presets

The unit includes 8 carefully crafted presets:

| # | Name | Description |
|---|------|-------------|
| 0 | **INIT** | Clean starting point with basic strike |
| 1 | **BOWED STR** | Bowed string with sustain. Use BOW level to control |
| 2 | **BELL** | Metallic bell percussion with long decay |
| 3 | **WOBBLE** | Wobble bass sound |
| 4 | **BLOWN TUBE** | Wind/breath instrument. Use BLOW level to control |
| 5 | **SHIMMER** | Ambient shimmer texture |
| 6 | **PLUCK STR** | Plucked string using STRING resonator model |
| 7 | **DRONE** | Evolving drone pad with looping envelope |

---

## Usage Tips

### Creating Mallet/Percussion Sounds

1. Start with the **INIT** preset or set **STRIKE** to 100
2. Choose a **MALLET** type - soft mallets for marimba-like sounds, hard for bells
3. Adjust **GEOMETRY** to morph between string (negative) and bell (positive) characters
4. Use **BRIGHT** to control the high-frequency content
5. Set short **ATTACK** and moderate **DECAY** for percussive hits

### Creating Bowed Sounds

1. Start with the **BOWED STR** preset or set **BOW** to 80-100
2. Set **STRIKE** and **BLOW** to 0
3. Use **BOW TIM** to control the friction texture
4. Set **GEOMETRY** negative for string-like resonance
5. Use longer **ATTACK** for smooth note starts
6. Set **CONTOUR** to AR for gate-following sustain

### Creating Blown/Wind Sounds

1. Start with the **BLOWN TUBE** preset or set **BLOW** to 80-100
2. Set **STRIKE** and **BOW** to 0
3. Use **FLOW** to control air turbulence
4. Experiment with **GEOMETRY** for different tube characters
5. **POSITION** affects where the "air stream" hits the resonator

### Using the String Models

1. Select **MODEL** = STRING for clean Karplus-Strong plucks
2. Select **MODEL** = MSTRING for rich sympathetic string resonance
3. STRING works great with STRIKE exciter for plucked sounds
4. MSTRING adds shimmer and is excellent for pads

### Creating Ambient Pads

1. Start with the **DRONE** or **SHIMMER** preset
2. Mix BOW and BLOW for continuous excitation
3. Set **CONTOUR** to LOOP for self-sustaining sounds
4. Use high **DAMPING** (positive) for long sustain
5. Increase **SPACE** for wide stereo spread
6. Try MSTRING model for rich harmonics

### Tuning Tips

- Use **COARSE** to transpose by semitones (±24)
- Use **FINE** for detuning effects or pitch correction (±100 cents)
- The synth responds to drumlogue's pitch bend (±2 semitones default)

### Using the Generative Sequencer (Lightweight Mode)

The sequencer is inspired by Mutable Instruments Marbles and creates tempo-synced generative melodies:

1. **Enable**: Set **SEQ** to any preset other than OFF
2. **Set base note**: Use **COARSE** to set the root note (0 = middle C)
3. **Create pattern**: Create a pattern in drumlogue's sequencer - each step triggers the generative sequencer
4. **Adjust spread**: **SPREAD** controls the note range (0 = unison, 127 = ±24 semitones)
5. **Lock patterns**: **DEJA VU** controls repetition (0 = fully random, 127 = 8-step loop)

#### Sequencer Tips

- **Simple melodies**: Use PENT or MAJ with low SPREAD (20-40)
- **Arpeggios**: Use TRI or 7TH presets with medium SPREAD
- **Chaos**: Use CHROM or RAND with high SPREAD (100+)
- **Repeating riffs**: Set DEJA VU to 127 for locked 8-step patterns
- **Evolving patterns**: Set DEJA VU to 64-90 for mostly-repeating with variations
- **Bass lines**: Use OCT or 5TH presets with COARSE set to -24

---

## Technical Specifications

| Specification | Value |
|---------------|-------|
| **Sample Rate** | 48kHz (native drumlogue rate, adapted from original 32kHz) |
| **Processing** | Stereo output |
| **Polyphony** | Monophonic (single voice) |
| **CPU Usage** | Efficient - MODAL ~0.5%, STRING/MSTRING ~0.3% |
| **Binary Size** | ~124KB |

---

## Version History

### v1.2.0 - Generative Sequencer Release

- Marbles-inspired generative sequencer (Lightweight mode)
- 16 sequencer presets with different subdivisions and scales
- Déjà vu loop buffer for repeating patterns
- Works with both MIDI notes and drumlogue pattern sequencer
- Optimized note queue to prevent note loss at high subdivisions

### v1.1.0 - Performance Optimization Release

- NEON SIMD optimizations (~40% faster modal resonator)
- CosineOscillator batch compute (~2.4x faster)
- Stereo soft clamp optimization (2x faster)
- Structure-of-Arrays layout for mode coefficients

### v1.0.0 - Initial Release

- Full Elements DSP integration at 48kHz
- Three resonator models: MODAL, STRING, MSTRING
- Level-balanced output across all models
- 8 factory presets
- Lightweight mode for optimal drumlogue performance

---

## Credits

- **DSP Algorithms**: Based on [Mutable Instruments Elements](https://mutable-instruments.net/modules/elements/) by Émilie Gillet
- **Sequencer**: Inspired by [Mutable Instruments Marbles](https://mutable-instruments.net/modules/marbles/) déjà vu concept
- **Original License**: MIT License
- **drumlogue Port**: CLDM
