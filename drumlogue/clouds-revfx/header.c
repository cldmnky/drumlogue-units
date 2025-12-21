/**
 *  @file header.c
 *  @brief drumlogue SDK unit header for Clouds-inspired reverb FX
 */

#include "unit.h"  // Common definitions for all units

// ---- Unit header definition  --------------------------------------------------------------------

const __unit_header unit_header_t unit_header = {
    .header_size = sizeof(unit_header_t),                  // leave as is, size of this header
    .target = UNIT_TARGET_PLATFORM | k_unit_module_revfx,  // target platform and module for this unit
    .api = UNIT_API_VERSION,                               // logue sdk API version against which unit was built
    .dev_id = 0x434C444DU,                                 // developer id ("CLDM")
    .unit_id = 0x00000001U,                                // unit id unique within dev_id scope
    .version = 0x010200U,                                  // v1.2.0
    .name = "Clds Reverb",                                 // displayed name, 7-bit ASCII, max 13 chars
    .num_presets = 0,                                      // Presets not supported on drumlogue for reverb/delay effects (hardware limitation)
    .num_params = 24,                                      // number of active parameters (16 + 6 LFO + 2 blank)
    .params = {
        // Format: min, max, center, default, type, fractional, frac. type, <reserved>, name

        // Page 1 - Main reverb controls
        // DRY/WET: 0-100% with 0.5 precision (value 0-200)
        {0, 200, 0, 100, k_unit_param_type_percent, 1, 0, 0, {"DRY/WET"}},
        // TIME: reverb time 0-127
        {0, 127, 0, 80, k_unit_param_type_none, 0, 0, 0, {"TIME"}},
        // DIFFUSION: reverb internal diffusion 0-127
        {0, 127, 0, 80, k_unit_param_type_none, 0, 0, 0, {"DIFFUSION"}},
        // LP: lowpass damping 0-127
        {0, 127, 0, 90, k_unit_param_type_none, 0, 0, 0, {"LP DAMP"}},

        // Page 2 - Additional reverb + texture
        // INPUT GAIN: input level 0-127
        {0, 127, 0, 50, k_unit_param_type_none, 0, 0, 0, {"IN GAIN"}},
        // TEXTURE: diffuser amount (post-reverb smearing) 0-127
        {0, 127, 0, 0, k_unit_param_type_none, 0, 0, 0, {"TEXTURE"}},
        // GRAIN AMT: granular mix amount 0-127
        {0, 127, 0, 0, k_unit_param_type_none, 0, 0, 0, {"GRAIN AMT"}},
        // GRAIN SIZE: grain size 0-127
        {0, 127, 0, 64, k_unit_param_type_none, 0, 0, 0, {"GRN SIZE"}},

        // Page 3 - Granular controls
        // GRAIN DENSITY: grain spawn rate 0-127
        {0, 127, 0, 64, k_unit_param_type_none, 0, 0, 0, {"GRN DENS"}},
        // GRAIN PITCH: pitch shift -24 to +24 semitones (0-127, center=64)
        {0, 127, 64, 64, k_unit_param_type_none, 0, 0, 0, {"GRN PITCH"}},
        // GRAIN POS: buffer position 0-127
        {0, 127, 0, 64, k_unit_param_type_none, 0, 0, 0, {"GRN POS"}},
        // FREEZE: freeze buffer 0-1
        {0, 1, 0, 0, k_unit_param_type_none, 0, 0, 0, {"FREEZE"}},

        // Page 4 - Pitch shifter
        // SHIFT AMT: pitch shifter mix amount 0-127
        {0, 127, 0, 0, k_unit_param_type_none, 0, 0, 0, {"SHIFT AMT"}},
        // SHIFT PITCH: pitch shift -24 to +24 semitones (0-127, center=64)
        {0, 127, 64, 64, k_unit_param_type_none, 0, 0, 0, {"SHFT PTCH"}},
        // SHIFT SIZE: window size 0-127
        {0, 127, 0, 64, k_unit_param_type_none, 0, 0, 0, {"SHFT SIZE"}},
        // Reserved blank slot
        {0, 0, 0, 0, k_unit_param_type_none, 0, 0, 0, {""}},

        // Page 5 - LFO1
        // LFO1 ASSIGN: target parameter (0=OFF, 1-15 = params)
        {0, 15, 0, 0, k_unit_param_type_strings, 0, 0, 0, {"LFO1 ASGN"}},
        // LFO1 SPEED: rate 0-127 (0 = ~0.05Hz, 127 = ~10Hz)
        {0, 127, 0, 64, k_unit_param_type_none, 0, 0, 0, {"LFO1 SPD"}},
        // LFO1 DEPTH: modulation depth 0-127
        {0, 127, 0, 64, k_unit_param_type_none, 0, 0, 0, {"LFO1 DPTH"}},
        // LFO1 WAVEFORM: sine/saw/random
        {0, 2, 0, 0, k_unit_param_type_strings, 0, 0, 0, {"LFO1 WAVE"}},

        // Page 6 - LFO2
        // LFO2 ASSIGN: target parameter (0=OFF, 1-15 = params)
        {0, 15, 0, 0, k_unit_param_type_strings, 0, 0, 0, {"LFO2 ASGN"}},
        // LFO2 SPEED: rate 0-127 (0 = ~0.05Hz, 127 = ~10Hz)
        {0, 127, 0, 64, k_unit_param_type_none, 0, 0, 0, {"LFO2 SPD"}},
        // LFO2 DEPTH: modulation depth 0-127
        {0, 127, 0, 64, k_unit_param_type_none, 0, 0, 0, {"LFO2 DPTH"}},
        // LFO2 WAVEFORM: sine/saw/random
        {0, 2, 0, 0, k_unit_param_type_strings, 0, 0, 0, {"LFO2 WAVE"}}}};
