#pragma once

#include <stdint.h>

struct tsf;

void tsf_kill_note(tsf* f, int channel, int preset, int key);

void voice_init();
void voice_reset();
void voice_note_on(uint8_t note, uint8_t velocity);
void voice_note_off(uint8_t note);
void voice_gate_off();
void voice_all_note_off();
void voice_pitch_bend(uint16_t value);
void voice_process_envelopes();
void voice_channel_pressure(uint8_t pressure);
