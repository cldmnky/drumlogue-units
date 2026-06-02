#include "dsp_primitives.h"

static const env_point_t kReleasePts[] = {
  {0, 0.0f}, {5, 0.125f}, {10, 0.25f}, {15, 0.4f}, {20, 0.6f},
  {30, 1.2f}, {40, 2.2f}, {50, 4.0f}, {60, 9.0f}, {75, 15.0f},
  {80, 20.0f}, {99, 60.0f},
};

static const env_point_t kDecayPts[] = {
  {0, 0.0f}, {5, 0.125f}, {10, 0.25f}, {20, 0.4f}, {30, 0.75f},
  {40, 1.5f}, {50, 3.0f}, {60, 5.0f}, {70, 9.0f}, {75, 12.0f},
  {80, 18.0f}, {99, 40.0f},
};

static const env_point_t kHoldPts[] = {
  {0, 0.0f}, {5, 0.125f}, {10, 0.25f}, {20, 0.4f}, {30, 0.8f},
  {40, 1.3f}, {50, 1.75f}, {60, 2.3f}, {70, 3.2f}, {75, 3.5f},
  {80, 4.2f}, {99, 6.5f},
};

static const env_point_t kDelayPts[] = {
  {0, 0.0f}, {5, 0.125f}, {10, 0.25f}, {20, 0.6f}, {32, 1.0f},
  {40, 1.5f}, {64, 2.5f}, {75, 3.5f}, {80, 4.2f}, {96, 6.2f},
  {100, 7.0f}, {127, 13.0f},
};

float env_lookup_seconds(uint32_t value, const env_point_t *table, int n) {
  if (value <= table[0].knob)    return table[0].seconds;
  if (value >= table[n - 1].knob) return table[n - 1].seconds;
  for (int i = 1; i < n; i++) {
    if (value <= table[i].knob) {
      float frac = (float)(value - table[i - 1].knob) /
                   (float)(table[i].knob - table[i - 1].knob);
      return table[i - 1].seconds +
             frac * (table[i].seconds - table[i - 1].seconds);
    }
  }
  return table[n - 1].seconds;
}

float env_time_to_samples_attack(uint32_t v)  { return env_lookup_seconds(v, kReleasePts, 12) * 48000.0f; }
float env_time_to_samples_hold(uint32_t v)     { return env_lookup_seconds(v, kHoldPts,    12) * 48000.0f; }
float env_time_to_samples_decay(uint32_t v)     { return env_lookup_seconds(v, kDecayPts,   12) * 48000.0f; }
float env_time_to_samples_release(uint32_t v)   { return env_lookup_seconds(v, kReleasePts, 12) * 48000.0f; }
float lfo_delay_to_samples(uint32_t v) { return env_lookup_seconds(v, kDelayPts, 12) * 48000.0f; }

float env_level_at_sample(uint64_t sample,
    float atk_samples, float hold_samples, float dec_samples, float sus_level,
    uint64_t note_on) {
  uint64_t elapsed = sample - note_on;
  if (elapsed < atk_samples)
    return (float)elapsed / atk_samples;
  if (elapsed < atk_samples + hold_samples)
    return 1.0f;
  if (elapsed < atk_samples + hold_samples + dec_samples) {
    float dec_t = (float)(elapsed - atk_samples - hold_samples) / dec_samples;
    return 1.0f - dec_t * (1.0f - sus_level);
  }
  return sus_level;
}

float aux_env_level_at_sample(uint64_t sample,
    float delay_samples, float atk_samples, float hold_samples,
    float dec_samples, float sus_level, float rel_samples,
    uint64_t note_on, uint64_t note_off, float release_start_level) {
  if (note_off == 0 || sample < note_off) {
    uint64_t elapsed = sample - note_on;
    if (elapsed < delay_samples)
      return 0.0f;
    uint64_t after_delay = elapsed - (uint64_t)delay_samples;
    if (after_delay < (uint64_t)atk_samples)
      return (float)after_delay / atk_samples;
    if (after_delay < (uint64_t)atk_samples + (uint64_t)hold_samples)
      return 1.0f;
    if (after_delay < (uint64_t)atk_samples + (uint64_t)hold_samples + (uint64_t)dec_samples) {
      float dec_t = (float)(after_delay - (uint64_t)atk_samples - (uint64_t)hold_samples) / dec_samples;
      return 1.0f - dec_t * (1.0f - sus_level);
    }
    return sus_level;
  } else {
    uint64_t rel_elapsed = sample - note_off;
    if (rel_elapsed < (uint64_t)rel_samples)
      return release_start_level * (1.0f - (float)rel_elapsed / rel_samples);
    return 0.0f;
  }
}
