#pragma once

#include <stdint.h>
#include <stdbool.h>

#define PATTERN_STEPS 16

typedef struct {
    uint8_t note;       // MIDI note number (0 = rest/off)
    uint8_t velocity;   // MIDI velocity (0-127)
    bool active;        // Step is active
} pattern_step_t;

typedef struct {
    pattern_step_t steps[PATTERN_STEPS];
    uint32_t current_step;
    uint32_t tempo_bpm;
    bool playing;
} pattern_t;

void pattern_init(pattern_t* pattern);
void pattern_set_step(pattern_t* pattern, uint8_t step, uint8_t note, uint8_t velocity);
void pattern_toggle_step(pattern_t* pattern, uint8_t step);
void pattern_clear(pattern_t* pattern);
uint32_t pattern_step_frames(pattern_t* pattern, uint32_t sample_rate);
bool pattern_advance(pattern_t* pattern, pattern_step_t* out_step);
