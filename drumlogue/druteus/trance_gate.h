#pragma once

#include <stdint.h>

void trance_gate_init();
void trance_gate_set_tempo(uint32_t tempo_fixed);
void trance_gate_process_stereo(float* left, float* right, uint32_t frames);
