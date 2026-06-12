#pragma once

#include <stdint.h>

void dsp_init();
void dsp_reset();
void dsp_process_filter(float *out, uint32_t frames);
void dsp_process_effects(float *out, uint32_t frames);

// Initialise the per-block smoother followers to the current params
// (avoids a zip on the first render).  Called from unit_init after
// Params[] has been seeded.
void dsp_init_smoothers(float cutoff, float res);
