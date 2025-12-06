/**
 *  @file header.c
 *  @brief drumlogue SDK unit header for Elements modal synthesis synth
 *
 *  Based on Mutable Instruments Elements
 *  Original code: Copyright 2014 Emilie Gillet (MIT License)
 *  Drumlogue port: 2024
 */

#include "unit.h"  // Common definitions for all units

// ---- Unit header definition  --------------------------------------------------------------------

const __unit_header unit_header_t unit_header = {
    .header_size = sizeof(unit_header_t),                  // leave as is, size of this header
    .target = UNIT_TARGET_PLATFORM | k_unit_module_synth,  // target platform and module for this unit
    .api = UNIT_API_VERSION,                               // logue sdk API version against which unit was built
    .dev_id = 0x434C444DU,                                 // developer id ("CLDM")
    .unit_id = 0x00000002U,                                // unit id unique within dev_id scope
    .version = 0x00010000U,                                // v1.0.0
    .name = "Elements",                                    // displayed name, 7-bit ASCII, max 13 chars
    .num_presets = 8,                                      // 8 presets: INIT + 7 crafted presets
    .num_params = 24,                                      // number of parameters (6 pages x 4)
    .params = {
        // Format: min, max, center, default, type, fractional, frac. type, <reserved>, name

        // ==================== Page 1: Exciter (BOW/BLOW/STRIKE mix) ====================
        // BOW: Bowing exciter level
        {0, 127, 0, 0, k_unit_param_type_none, 0, 0, 0, {"BOW"}},
        // BLOW: Blowing exciter level  
        {0, 127, 0, 0, k_unit_param_type_none, 0, 0, 0, {"BLOW"}},
        // STRIKE: Strike exciter level
        {0, 127, 0, 100, k_unit_param_type_none, 0, 0, 0, {"STRIKE"}},
        // MALLET: Strike sample/mallet type + timbre selection (12 variants)
        {0, 11, 0, 0, k_unit_param_type_strings, 0, 0, 0, {"MALLET"}},

        // ==================== Page 2: Exciter Timbre ====================
        // BOW TIMBRE: Bow friction/texture
        {-64, 63, 0, 0, k_unit_param_type_none, 0, 0, 0, {"BOW TIM"}},
        // BLOW TIMBRE: Blow turbulence/noise color
        {-64, 63, 0, 0, k_unit_param_type_none, 0, 0, 0, {"BLW TIM"}},
        // STK MODE: Strike mode (SAMPLE/GRANULAR/NOISE/PLECTRUM/PARTICLES)
        {0, 4, 0, 0, k_unit_param_type_strings, 0, 0, 0, {"STK MOD"}},
        // GRAN DENS: Granular density (active in GRANULAR mode)
        {-64, 63, 0, 0, k_unit_param_type_none, 0, 0, 0, {"DENSITY"}},

        // ==================== Page 3: Resonator ====================
        // GEOMETRY: Structure (string->bar->membrane->bell)
        {-64, 63, 0, 0, k_unit_param_type_none, 0, 0, 0, {"GEOMETRY"}},
        // BRIGHTNESS: High-freq damping (dark wood to bright metal)
        {-64, 63, 0, 0, k_unit_param_type_none, 0, 0, 0, {"BRIGHT"}},
        // DAMPING: Overall decay time
        {-64, 63, 0, 0, k_unit_param_type_none, 0, 0, 0, {"DAMPING"}},
        // POSITION: Excitation point on resonator
        {-64, 63, 0, 0, k_unit_param_type_none, 0, 0, 0, {"POSITION"}},

        // ==================== Page 4: Filter & Model ====================
        // CUTOFF: Filter cutoff frequency
        {0, 127, 0, 127, k_unit_param_type_none, 0, 0, 0, {"CUTOFF"}},
        // RESONANCE: Filter resonance/Q
        {0, 127, 0, 0, k_unit_param_type_none, 0, 0, 0, {"RESO"}},
        // FLT ENV: Filter envelope modulation amount
        {0, 127, 0, 64, k_unit_param_type_none, 0, 0, 0, {"FLT ENV"}},
        // MODEL: Resonator model (MODAL/STRING/MSTRING)
        {0, 2, 0, 0, k_unit_param_type_strings, 0, 0, 0, {"MODEL"}},

        // ==================== Page 5: Envelope (ADR) ====================
        // ATTACK: Envelope attack time
        {0, 127, 0, 5, k_unit_param_type_none, 0, 0, 0, {"ATTACK"}},
        // DECAY: Envelope decay time
        {0, 127, 0, 40, k_unit_param_type_none, 0, 0, 0, {"DECAY"}},
        // RELEASE: Envelope release time
        {0, 127, 0, 40, k_unit_param_type_none, 0, 0, 0, {"RELEASE"}},
        // ENV MODE: Envelope shape (ADR/AD/AR/LOOP)
        {0, 3, 0, 0, k_unit_param_type_strings, 0, 0, 0, {"ENV MOD"}},

        // ==================== Page 6: LFO ====================
        // LFO RATE: LFO speed
        {0, 127, 0, 40, k_unit_param_type_none, 0, 0, 0, {"LFO RT"}},
        // LFO DEPTH: Modulation amount
        {0, 127, 0, 0, k_unit_param_type_none, 0, 0, 0, {"LFO DEP"}},
        // LFO PRESET: Shape+Destination combo
        {0, 7, 0, 0, k_unit_param_type_strings, 0, 0, 0, {"LFO PRE"}},
        // COARSE: Pitch coarse tune (-24 to +24 semi)
        {-64, 63, 0, 0, k_unit_param_type_none, 0, 0, 0, {"COARSE"}}}};


