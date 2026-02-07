/**
 *  @file header.c
 *  @brief drumlogue SDK unit header for drumpler
 *
 *  Copyright (c) 2020-2022 KORG Inc. All rights reserved.
 *
 */

#include "unit.h"  // Note: Include common definitions for all units

// ---- Unit header definition  --------------------------------------------------------------------

const __unit_header unit_header_t unit_header = {
    .header_size = sizeof(unit_header_t),                  // leave as is, size of this header
    .target = UNIT_TARGET_PLATFORM | k_unit_module_synth,  // target platform and module for this unit
    .api = UNIT_API_VERSION,                               // logue sdk API version against which unit was built
    .dev_id = 0x434C444DU,                                 // developer identifier ("CLDM")
    .unit_id = 0x00000005U,                                // unit id unique within dev_id scope
    .version = 0x00010000U,                                // v1.0.0 (major<<16 | minor<<8 | patch)
    .name = "drumpler",                                   // displayed name, 7-bit ASCII, max 13 chars
    .num_presets = 128,                                    // ROM presets (0-127)
    .num_params = 12,                                      // number of parameters for this unit, max 24
    .params = {
        // Page 1: PART / POLY / LEVEL / PAN
        {1, 16, 1, 1, k_unit_param_type_strings, 0, 0, 0, {"PART"}},
        {1, 32, 0, 16, k_unit_param_type_strings, 0, 0, 0, {"POLY"}},
        {0, 100, 0, 100, k_unit_param_type_percent, 0, 0, 0, {"LEVEL"}},
        {-63, 63, 0, 0, k_unit_param_type_strings, 0, 0, 0, {"PAN"}},

        // Page 2: TONE / CUTOFF / RESO / ATTACK
        {0, 127, 0, 0, k_unit_param_type_strings, 0, 0, 0, {"TONE"}},
        {0, 100, 0, 100, k_unit_param_type_percent, 0, 0, 0, {"CUTOFF"}},
        {0, 100, 0, 0, k_unit_param_type_percent, 0, 0, 0, {"RESO"}},
        {0, 100, 0, 0, k_unit_param_type_percent, 0, 0, 0, {"ATTACK"}},

        // Page 3: REVERB / CHORUS / DELAY / (blank)
        {0, 100, 0, 0, k_unit_param_type_percent, 0, 0, 0, {"REVERB"}},
        {0, 100, 0, 0, k_unit_param_type_percent, 0, 0, 0, {"CHORUS"}},
        {0, 100, 0, 0, k_unit_param_type_percent, 0, 0, 0, {"DELAY"}},
        {0, 0, 0, 0, k_unit_param_type_none, 0, 0, 0, {""}},

        // Page 4-6: empty
        {0, 0, 0, 0, k_unit_param_type_none, 0, 0, 0, {""}},
        {0, 0, 0, 0, k_unit_param_type_none, 0, 0, 0, {""}},
        {0, 0, 0, 0, k_unit_param_type_none, 0, 0, 0, {""}},
        {0, 0, 0, 0, k_unit_param_type_none, 0, 0, 0, {""}},

        {0, 0, 0, 0, k_unit_param_type_none, 0, 0, 0, {""}},
        {0, 0, 0, 0, k_unit_param_type_none, 0, 0, 0, {""}},
        {0, 0, 0, 0, k_unit_param_type_none, 0, 0, 0, {""}},
        {0, 0, 0, 0, k_unit_param_type_none, 0, 0, 0, {""}},

        {0, 0, 0, 0, k_unit_param_type_none, 0, 0, 0, {""}},
        {0, 0, 0, 0, k_unit_param_type_none, 0, 0, 0, {""}},
        {0, 0, 0, 0, k_unit_param_type_none, 0, 0, 0, {""}},
        {0, 0, 0, 0, k_unit_param_type_none, 0, 0, 0, {""}}}};
