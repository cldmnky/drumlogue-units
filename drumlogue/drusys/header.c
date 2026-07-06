#include "unit.h"

const __unit_header unit_header_t unit_header = {
    .header_size = sizeof(unit_header_t),
    .target      = UNIT_TARGET_PLATFORM | k_unit_module_synth,
    .api         = UNIT_API_VERSION,
    .dev_id      = 0x434C444DU,
    .unit_id     = 0x00000006U,
    .version     = 0x000100U,
    .name        = "DRUSYS",
    .num_presets = 0,
    .num_params  = 4,
    .params = {
        /* 0 KERNEL  */ {0, 0, 0, 0, k_unit_param_type_none, 0, 0, 0, {"KERNEL"}},
        /* 1 CPU     */ {0, 0, 0, 0, k_unit_param_type_none, 0, 0, 0, {"CPU"}},
        /* 2 MEM     */ {0, 0, 0, 0, k_unit_param_type_none, 0, 0, 0, {"MEM"}},
        /* 3 STATUS  */ {0, 0, 0, 0, k_unit_param_type_none, 0, 0, 0, {"STATUS"}},
    },
};
