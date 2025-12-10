/**
 *  @file header.c
 *  @brief drumlogue SDK unit header for Elements modal synthesis synth
 *
 *  Based on Mutable Instruments Elements
 *  Original code: Copyright 2014 Emilie Gillet (MIT License)
 *  Drumlogue port: 2024
 *
 *  ELEMENTS_LIGHTWEIGHT: When defined, removes Filter (Page 4) and LFO (Page 6)
 *  for improved performance on the drumlogue hardware.
 *
 *  Parameter layout follows the original Elements philosophy:
 *  - Exciter section: BOW, BLOW, STRIKE generators with their timbres
 *  - Resonator section: GEOMETRY, BRIGHTNESS, DAMPING, POSITION
 *  - The interplay between exciter spectrum and resonator modes creates the sound
 */

#include "unit.h"  // Common definitions for all units

// ---- Unit header definition  --------------------------------------------------------------------

const __unit_header unit_header_t unit_header = {
    .header_size = sizeof(unit_header_t),                  // leave as is, size of this header
    .target = UNIT_TARGET_PLATFORM | k_unit_module_synth,  // target platform and module for this unit
    .api = UNIT_API_VERSION,                               // logue sdk API version against which unit was built
    .dev_id = 0x434C444DU,                                 // developer id ("CLDM")
    .unit_id = 0x00000002U,                                // unit id unique within dev_id scope
    .version = 0x010200U,                                // v1.2.0 (major<<16 | minor<<8 | patch)
    .name = "Elementish",                                  // displayed name, 7-bit ASCII, max 13 chars
    .num_presets = 8,                                      // 8 presets: INIT + 7 crafted presets
    .num_params = 24,                                      // number of parameters (6 pages x 4)
    .params = {
        // Format: min, max, center, default, type, fractional, frac. type, <reserved>, name

        // ==================== Page 1: Exciter Mix ====================
        // BOW: Bowing exciter level (also increases "pressure" = brighter/noisier)
        {0, 127, 0, 0, k_unit_param_type_none, 0, 0, 0, {"BOW"}},
        // BLOW: Granular blowing noise level
        {0, 127, 0, 0, k_unit_param_type_none, 0, 0, 0, {"BLOW"}},
        // STRIKE: Percussive noise level
        {0, 127, 0, 100, k_unit_param_type_none, 0, 0, 0, {"STRIKE"}},
        // MALLET: Strike sample/mallet type (samples→mallets→plectrums→particles)
        {0, 11, 0, 0, k_unit_param_type_strings, 0, 0, 0, {"MALLET"}},

        // ==================== Page 2: Exciter Timbre ====================
        // BOW TIMBRE: Bow friction/smoothness (granularity of bow material)
        {-64, 63, 0, 0, k_unit_param_type_none, 0, 0, 0, {"BOW TIM"}},
        // FLOW: Air turbulence/texture for blow (like scanning wavetable of noise)
        {-64, 63, 0, 0, k_unit_param_type_none, 0, 0, 0, {"FLOW"}},
        // STK MODE: Strike synthesis mode
        {0, 4, 0, 0, k_unit_param_type_strings, 0, 0, 0, {"STK MOD"}},
        // DENSITY: Granular density (for GRANULAR/PARTICLE modes)
        {-64, 63, 0, 0, k_unit_param_type_none, 0, 0, 0, {"DENSITY"}},

        // ==================== Page 3: Resonator ====================
        // GEOMETRY: Structure shape (string→bar→membrane→plate→bell)
        {-64, 63, 0, 0, k_unit_param_type_none, 0, 0, 0, {"GEOMETRY"}},
        // BRIGHTNESS: High-freq mode damping (wood/nylon→glass/steel)
        {-64, 63, 0, 0, k_unit_param_type_none, 0, 0, 0, {"BRIGHT"}},
        // DAMPING: Energy dissipation rate (muting effect)
        {-64, 63, 0, 0, k_unit_param_type_none, 0, 0, 0, {"DAMPING"}},
        // POSITION: Excitation point on surface (affects harmonics like PWM)
        {-64, 63, 0, 0, k_unit_param_type_none, 0, 0, 0, {"POSITION"}},

#ifndef ELEMENTS_LIGHTWEIGHT
        // ==================== Page 4: Filter & Model ====================
        // CUTOFF: Filter cutoff frequency
        {0, 127, 0, 127, k_unit_param_type_none, 0, 0, 0, {"CUTOFF"}},
        // RESONANCE: Filter resonance/Q
        {0, 127, 0, 0, k_unit_param_type_none, 0, 0, 0, {"RESO"}},
        // FLT ENV: Filter envelope modulation amount
        {0, 127, 0, 64, k_unit_param_type_none, 0, 0, 0, {"FLT ENV"}},
        // MODEL: Resonator model (MODAL/STRING/MSTRING)
        {0, 2, 0, 0, k_unit_param_type_strings, 0, 0, 0, {"MODEL"}},
#else
        // ==================== Page 4: Model & Space (Lightweight) ====================
        // MODEL: Resonator model (MODAL/STRING/MSTRING)
        {0, 2, 0, 0, k_unit_param_type_strings, 0, 0, 0, {"MODEL"}},
        // SPACE: Stereo width (Elements signature parameter)
        {0, 127, 0, 70, k_unit_param_type_none, 0, 0, 0, {"SPACE"}},
        // VOLUME: Unit output level
        {0, 127, 0, 100, k_unit_param_type_none, 0, 0, 0, {"VOLUME"}},
        // Blank placeholder
        {0, 0, 0, 0, k_unit_param_type_none, 0, 0, 0, {""}},
#endif

        // ==================== Page 5: Envelope ====================
        // ATTACK: Envelope attack time
        {0, 127, 0, 5, k_unit_param_type_none, 0, 0, 0, {"ATTACK"}},
        // DECAY: Envelope decay time
        {0, 127, 0, 40, k_unit_param_type_none, 0, 0, 0, {"DECAY"}},
        // RELEASE: Envelope release time
        {0, 127, 0, 40, k_unit_param_type_none, 0, 0, 0, {"RELEASE"}},
        // CONTOUR: Envelope shape (ADR/AD/AR/LOOP - like Elements' morphing contour)
        {0, 3, 0, 0, k_unit_param_type_strings, 0, 0, 0, {"CONTOUR"}},

#ifndef ELEMENTS_LIGHTWEIGHT
        // ==================== Page 6: LFO ====================
        // LFO RATE: LFO speed
        {0, 127, 0, 40, k_unit_param_type_none, 0, 0, 0, {"LFO RT"}},
        // LFO DEPTH: Modulation amount
        {0, 127, 0, 0, k_unit_param_type_none, 0, 0, 0, {"LFO DEP"}},
        // LFO PRESET: Shape+Destination combo
        {0, 7, 0, 0, k_unit_param_type_strings, 0, 0, 0, {"LFO PRE"}},
        // COARSE: Pitch coarse tune (-24 to +24 semi)
        {-64, 63, 0, 0, k_unit_param_type_none, 0, 0, 0, {"COARSE"}}}};
#else
        // ==================== Page 6: Sequencer (Lightweight) ====================
        // COARSE: Pitch coarse tune (-24 to +24 semitones)
        {-64, 63, 0, 0, k_unit_param_type_none, 0, 0, 0, {"COARSE"}},
        // SEQ: Sequencer preset (rate + scale combination)
        {0, 15, 0, 0, k_unit_param_type_strings, 0, 0, 0, {"SEQ"}},
        // SPREAD: Note range/spread amount (0=narrow, 127=wide)
        {0, 127, 0, 64, k_unit_param_type_none, 0, 0, 0, {"SPREAD"}},
        // DEJA VU: Sequence looping amount (0=random, 127=locked loop)
        {0, 127, 0, 0, k_unit_param_type_none, 0, 0, 0, {"DEJA VU"}}}};
#endif


