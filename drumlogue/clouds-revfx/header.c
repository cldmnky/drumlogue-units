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
    .version = 0x00010000U,                                // v1.0.0 (major<<16 | minor<<8 | patch)
    .name = "cloudsrevfx",                                // displayed name, 7-bit ASCII, max 13 chars
    .num_presets = 1,                                      // expose a single INIT preset for now
    .num_params = 8,                                       // number of active parameters (rest left blank)
    .params = {
        // Format: min, max, center, default, type, fractional, frac. type, <reserved>, name

        // Page 1
        // Blend: percent with 0.5 precision
        {0, (100 << 1), 0, (50 << 1), k_unit_param_type_percent, 1, 0, 0, {"BLEND"}},
        // Size: unipolar room size
        {0, 127, 0, 64, k_unit_param_type_none, 0, 0, 0, {"SIZE"}},
        // Texture: bipolar density/texture
        {-64, 64, 0, 0, k_unit_param_type_none, 0, 0, 0, {"TEXTURE"}},
        // Position: grain position
        {0, 127, 0, 64, k_unit_param_type_none, 0, 0, 0, {"POSITION"}},

        // Page 2
        // Density: bipolar spread
        {-100, 100, 0, 0, k_unit_param_type_percent, 0, 0, 0, {"DENSITY"}},
        // Pitch: cents offset
        {(-2400), 2400, 0, 0, k_unit_param_type_cents, 0, 0, 0, {"PITCH"}},
        // Feedback: percent
        {0, (100 << 1), 0, (50 << 1), k_unit_param_type_percent, 1, 0, 0, {"FEEDBACK"}},
        // Reverb send: percent
        {0, (100 << 1), 0, (20 << 1), k_unit_param_type_percent, 1, 0, 0, {"REVERB"}},

        // Remaining parameter slots left blank to control paging
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
        {0, 0, 0, 0, k_unit_param_type_none, 0, 0, 0, {""}},

        {0, 0, 0, 0, k_unit_param_type_none, 0, 0, 0, {""}},
        {0, 0, 0, 0, k_unit_param_type_none, 0, 0, 0, {""}},
        {0, 0, 0, 0, k_unit_param_type_none, 0, 0, 0, {""}},
        {0, 0, 0, 0, k_unit_param_type_none, 0, 0, 0, {""}}}};
