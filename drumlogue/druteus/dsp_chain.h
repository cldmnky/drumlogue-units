#pragma once

#include <stdint.h>

void dsp_init();
void dsp_reset();
void dsp_process_filter(float *out, uint32_t frames);
void dsp_process_effects(float *out, uint32_t frames);
