#include "trance_gate.h"
#include "dsp_primitives.h"
#include "params.h"
#include <cstring>
#include <cmath>
#include <cstdint>

static constexpr float kSampleRate = 48000.0f;
static constexpr float kCrossfadeSamples = 48.0f;
static constexpr int kStepsPerBar = 16;

// 32 rhythmic patterns as 16-bit masks.  Bit 0 = step 0 (first 16th of beat 1).
// 1 = gate open, 0 = gate closed.
static const uint16_t kGatePatterns[32] = {
  0xFFFFu,  //  1: All 16th notes
  0x5555u,  //  2: Straight 8th (on beat)
  0xAAAAu,  //  3: Off-beat 16th
  0x1111u,  //  4: Quarter notes (downbeat of each beat)
  0x0101u,  //  5: Half notes (beat 1 + beat 3)
  0x0001u,  //  6: Downbeat only
  0xCCCCu,  //  7: First two 16ths of each beat
  0x3333u,  //  8: Last two 16ths of each beat
  0xEEEEu,  //  9: First three 16ths of each beat
  0x7777u,  // 10: Last three 16ths of each beat
  0xDDDDu,  // 11: Skip 3rd 16th of each beat
  0xBBBBu,  // 12: Skip 2nd 16th of each beat
  0x6666u,  // 13: Middle two 16ths of each beat (2+3)
  0x9999u,  // 14: Edge two 16ths of each beat (1+4)
  0xFF00u,  // 15: First half of bar full
  0x00FFu,  // 16: Second half of bar full
  0xF0F0u,  // 17: Beat 1+3 full
  0x0F0Fu,  // 18: Beat 2+4 full
  0x111Fu,  // 19: Quarters with last beat full (buildup)
  0xF111u,  // 20: Full first beat with quarters rest (breakdown)
  0x888Fu,  // 21: Off-beat 8th with last beat full
  0xB6DBu,  // 22: Age of Love — galloping 3-note trance gate (on-on-off repeating)
  0x8421u,  // 23: Descending staircase
  0x8001u,  // 24: Bar start + end only
  0xC003u,  // 25: Outer two edges of bar
  0x3C3Cu,  // 26: Inner mid-beat accents
  0x0FF0u,  // 27: Middle half of bar
  0xF00Fu,  // 28: Outer quarters of bar
  0xAA55u,  // 29: Contrast alternating (first half off-beat, second half on-beat)
  0x55AAu,  // 30: Reverse contrast alternating
  0x9249u,  // 31: 3-step polyrhythm (on-off-off repeating)
  0x2492u   // 32: 3-step polyrhythm reverse
};

// State
static float s_bpm = 120.0f;
static float s_step_phase = 0.0f;
static bool s_init = false;

void trance_gate_init() {
  s_bpm = 120.0f;
  s_step_phase = 0.0f;
  s_init = true;
}

void trance_gate_set_tempo(uint32_t tempo_fixed) {
  float bpm = (float)(tempo_fixed >> 16);
  bpm += (float)(tempo_fixed & 0xFFFFu) / 65536.0f;
  if (bpm < 20.0f) bpm = 20.0f;
  if (bpm > 300.0f) bpm = 300.0f;
  s_bpm = bpm;
}

void trance_gate_process_stereo(float* left, float* right, uint32_t frames) {
  int pattern = Params[param_trance_gate];
  if (pattern <= 0)
    return;
  if (pattern > 32)
    pattern = 32;

  uint16_t mask = kGatePatterns[pattern - 1];
  float bpm = s_bpm;
  if (bpm < 20.0f) bpm = 120.0f;

  // Step duration in samples: 16th note = 60 / bpm / 4 seconds
  float step_samples = (60.0f / bpm * kSampleRate) / 4.0f;
  if (step_samples < 1.0f) step_samples = 1.0f;

  float advance = 1.0f / step_samples;
  float xfade_start = 1.0f - kCrossfadeSamples / step_samples;
  float xfade_scale = (xfade_start < 1.0f) ? 1.0f / (1.0f - xfade_start) : 1.0f;

  for (uint32_t i = 0; i < frames; i++) {
    // Advance phase by one sample within the 16-step cycle
    s_step_phase += advance;
    if (s_step_phase >= (float)kStepsPerBar)
      s_step_phase -= (float)kStepsPerBar;

    int step = (int)s_step_phase;
    float frac = s_step_phase - (float)step;

    // Current and next step gate values
    float cur_gate = (mask & (1u << step)) ? 1.0f : 0.0f;
    int next_step = (step + 1 == kStepsPerBar) ? 0 : step + 1;
    float next_gate = (mask & (1u << next_step)) ? 1.0f : 0.0f;

    float gain;
    if (frac >= xfade_start) {
      float t = (frac - xfade_start) * xfade_scale;
      if (t > 1.0f) t = 1.0f;
      gain = cur_gate * (1.0f - t) + next_gate * t;
    } else {
      gain = cur_gate;
    }

    left[i]  *= gain;
    right[i] *= gain;
  }
}
