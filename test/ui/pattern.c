#include "pattern.h"
#include <string.h>

void pattern_init(pattern_t* pattern) {
    memset(pattern, 0, sizeof(pattern_t));
    pattern->tempo_bpm = 120;
    pattern->playing = false;
    
    // Default pattern: C4 scale
    const uint8_t default_notes[] = {60, 62, 64, 65, 67, 69, 71, 72, 
                                     72, 71, 69, 67, 65, 64, 62, 60};
    for (int i = 0; i < PATTERN_STEPS; i++) {
        pattern->steps[i].note = default_notes[i];
        pattern->steps[i].velocity = 100;
        pattern->steps[i].active = true;
    }
}

void pattern_set_step(pattern_t* pattern, uint8_t step, uint8_t note, uint8_t velocity) {
    if (step >= PATTERN_STEPS) return;
    pattern->steps[step].note = note;
    pattern->steps[step].velocity = velocity;
    pattern->steps[step].active = (note > 0);
}

void pattern_toggle_step(pattern_t* pattern, uint8_t step) {
    if (step >= PATTERN_STEPS) return;
    pattern->steps[step].active = !pattern->steps[step].active;
}

void pattern_clear(pattern_t* pattern) {
    for (int i = 0; i < PATTERN_STEPS; i++) {
        pattern->steps[i].active = false;
    }
}

uint32_t pattern_step_frames(pattern_t* pattern, uint32_t sample_rate) {
    // Calculate frames per step based on tempo
    // 16th notes at given BPM
    float seconds_per_step = 60.0f / pattern->tempo_bpm / 4.0f;  // 16th note
    return (uint32_t)(seconds_per_step * sample_rate);
}

bool pattern_advance(pattern_t* pattern, pattern_step_t* out_step) {
    if (!pattern->playing) {
        return false;
    }
    
    if (out_step) {
        *out_step = pattern->steps[pattern->current_step];
    }
    
    pattern->current_step = (pattern->current_step + 1) % PATTERN_STEPS;
    return true;
}
