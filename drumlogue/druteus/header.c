/**
 * @file header.c
 * @brief drumlogue SDK unit header for DRUTEUS — Proteus/1 SF2 synth
 */

#include "unit.h"

const __unit_header unit_header_t unit_header = {
    .header_size = sizeof(unit_header_t),
    .target      = UNIT_TARGET_PLATFORM | k_unit_module_synth,
    .api         = UNIT_API_VERSION,
    .dev_id      = 0x434C444DU,   /* "CLDM" */
    .unit_id     = 0x00000005U,
    .version     = 0x000100U,     /* 0.1.0 */
    .name        = "DRUTEUS",
    .num_presets = 0,
    .num_params  = 24,
    .params = {
        /* Page 1 — sound source */
        /* 0 SFONT   */ {0, 63,  0, 0,  k_unit_param_type_strings, 0, 0, 0, {"SFONT"}},
        /* 1 PATCH   */ {0, 767, 0, 0,  k_unit_param_type_strings, 0, 0, 0, {"PATCH"}},
        /* 2 VOICES  */ {1, 16,  1, 16, k_unit_param_type_none,    0, 0, 0, {"VOICES"}},
        /* 3 TUNE    */ {-12,12, 0, 0,  k_unit_param_type_none,    0, 0, 0, {"TUNE"}},

        /* Page 2 — pitch & mix */
        /* 4 FINETN  */ {-63,63, 0, 0,  k_unit_param_type_none, 0, 0, 0, {"FINETN"}},
        /* 5 VOLUME  */ {0, 127, 0,100, k_unit_param_type_none, 0, 0, 0, {"VOLUME"}},
        /* 6 PAN     */ {0, 127,64, 64, k_unit_param_type_none, 0, 0, 0, {"PAN"}},
        /* 7         */ {0, 0,   0, 0,  k_unit_param_type_none, 0, 0, 0, {""}},

        /* Page 3 — layer control */
        /* 8 XFADE   */ {0, 2,   0, 0,  k_unit_param_type_strings, 0, 0, 0, {"XFADE"}},
        /* 9 LAYERS  */ {0, 2,   0, 0,  k_unit_param_type_strings, 0, 0, 0, {"LAYERS"}},
        /* 10        */ {0, 0,   0, 0,  k_unit_param_type_none,    0, 0, 0, {""}},
        /* 11        */ {0, 0,   0, 0,  k_unit_param_type_none,    0, 0, 0, {""}},

        /* Page 4 — effects & feel */
        /* 12 CHORUS */ {0, 15,  0, 0,  k_unit_param_type_none,    0, 0, 0, {"CHORUS"}},
        /* 13 REVERB */ {0, 127, 0, 0,  k_unit_param_type_percent, 0, 0, 0, {"REVERB"}},
        /* 14 V.CURVE*/ {0, 4,   0, 0,  k_unit_param_type_strings, 0, 0, 0, {"V.CURVE"}},
        /* 15        */ {0, 0,   0, 0,  k_unit_param_type_none,    0, 0, 0, {""}},

        /* Page 5 — play mode (all blank now that SOLO is removed) */
        /* 16        */ {0, 0,   0, 0,  k_unit_param_type_none, 0, 0, 0, {""}},
        /* 17        */ {0, 0,   0, 0,  k_unit_param_type_none, 0, 0, 0, {""}},
        /* 18        */ {0, 0,   0, 0,  k_unit_param_type_none, 0, 0, 0, {""}},
        /* 19        */ {0, 0,   0, 0,  k_unit_param_type_none, 0, 0, 0, {""}},

        /* Page 6 — LFO */
        /* 20 LFO RTE*/ {0, 127, 0, 0,  k_unit_param_type_none,    0, 0, 0, {"LFO RTE"}},
        /* 21 LFO AMT*/ {0, 127, 0, 0,  k_unit_param_type_none,    0, 0, 0, {"LFO AMT"}},
        /* 22 LFO DST*/ {0, 2,   0, 0,  k_unit_param_type_strings, 0, 0, 0, {"LFO DST"}},
        /* 23 LFO WAV*/ {0, 4,   0, 1,  k_unit_param_type_strings, 0, 0, 0, {"LFO WAV"}},
    },
};
