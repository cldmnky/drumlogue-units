// Map common NEON helpers into the drupiter namespace.
// Include this in dsp files that need NEON buffer operations.

#pragma once

#define NEON_DSP_NS drupiter
#include "../../common/neon_dsp.h"
#undef NEON_DSP_NS
