#include "lfo_engine.h"
#include "druteus_state.h"
#include "params.h"
#include "dsp_primitives.h"
#include <math.h>
#include <stdlib.h>

float lfo_phase            = 0.0f;
float lfo2_phase           = 0.0f;
float lfo_delay_completed  = 0.0f;

float lfo_wave_shape(float phase, uint8_t shape) {
  switch (shape) {
    case 0: {
      float v = phase > 0.5f ? 1.0f - phase : phase;
      return v * 4.0f - 1.0f;
    }
    case 1: return sinf(phase * 2.0f * (float)M_PI);
    case 2: return phase < 0.5f ? 1.0f : -1.0f;
    case 3: return phase * 2.0f - 1.0f;
    case 4: {
      static float s_h = 0.0f;
      static float s_h_prev_phase = 0.0f;
      if (phase < s_h_prev_phase)
        s_h = (float)rand() / (float)RAND_MAX * 2.0f - 1.0f;
      s_h_prev_phase = phase;
      return s_h;
    }
    default: return sinf(phase * 2.0f * (float)M_PI);
  }
}

void lfo_init() {
  lfo_phase       = 0.0f;
  lfo2_phase      = 0.0f;
  lfo_delay_completed = 0.0f;
}

float lfo_process_lfo_pitch_offset(uint32_t frames) {
  float lfo_pitch_offset = 0.0f;

  float lfo_delay_scale = 1.0f;
  uint32_t delay_param  = (current_patch.lfo1amount != 0 || current_patch.lfo2amount != 0)
                         ? current_patch.lfo1delay : 0;
  if (delay_param > 0) {
    float delay_samples = lfo_delay_to_samples(delay_param);
    lfo_delay_completed += (float)frames;
    if (lfo_delay_completed < delay_samples)
      lfo_delay_scale = lfo_delay_completed / delay_samples;
  }

  if (current_patch.lfo1amount != 0) {
    float rate = 0.05f + current_patch.lfo1frequency * (24.95f / 127.0f);
    float amt  = (float)current_patch.lfo1amount / 127.0f * lfo_delay_scale;
    uint8_t w  = current_patch.lfo1shape;
    if (current_patch.lfo1variation > 0) {
      float var = (float)current_patch.lfo1variation / 127.0f;
      float rnd = ((float)((sample_count / 64) * 2654435761u & 0x7FFFFFFF)
                   / 2147483648.0f) - 1.0f;
      rate *= 1.0f + rnd * var * 0.5f;
      if (rate < 0.01f) rate = 0.01f;
    }
    lfo_phase += rate * (float)frames / 48000.0f;
    if (lfo_phase >= 1.0f) lfo_phase -= 1.0f;
    lfo_pitch_offset += lfo_wave_shape(lfo_phase, w) * amt * 0.5f;
  }

  if (current_patch.lfo2amount != 0) {
    float rate = 0.05f + current_patch.lfo2frequency * (24.95f / 127.0f);
    float amt  = (float)current_patch.lfo2amount / 127.0f * lfo_delay_scale;
    uint8_t w  = current_patch.lfo2shape;
    if (current_patch.lfo2variation > 0) {
      float var = (float)current_patch.lfo2variation / 127.0f;
      float rnd = ((float)(((sample_count / 64) + 1) * 2654435761u & 0x7FFFFFFF)
                   / 2147483648.0f) - 1.0f;
      rate *= 1.0f + rnd * var * 0.5f;
      if (rate < 0.01f) rate = 0.01f;
    }
    lfo2_phase += rate * (float)frames / 48000.0f;
    if (lfo2_phase >= 1.0f) lfo2_phase -= 1.0f;
    lfo_pitch_offset += lfo_wave_shape(lfo2_phase, w) * amt * 0.5f;
  }

  return lfo_pitch_offset;
}

float lfo_process_user_mod(uint32_t frames, uint8_t dest) {
  float rate   = 0.05f + Params[param_lfo_rate] * (24.95f / 127.0f);
  float amt    = Params[param_lfo_amount] / 127.0f;
  uint8_t wave = Params[param_lfo_wave];
  lfo_phase += rate * (float)frames / 48000.0f;
  if (lfo_phase >= 1.0f) lfo_phase -= 1.0f;
  float lfo_val = lfo_wave_shape(lfo_phase, wave);
  if (dest == 0 || dest == 2)
    return lfo_val * amt * 0.5f;
  return 0.0f;
}

static float s_compute_voice_aux_env_level(int vi) {
  if (!voice_env[vi].active) return 0.0f;

  // Use per-voice effective aux params (keyvel-modulated)
  int modded_amount = (int)current_patch.i3amount + (int)voice_env[vi].eff_i3amount;
  if (modded_amount < -128) modded_amount = -128;
  if (modded_amount > 127)  modded_amount = 127;

  int modded_attack = (int)current_patch.i3attack + (int)voice_env[vi].eff_i3attack;
  if (modded_attack < 0)  modded_attack = 0;
  if (modded_attack > 99) modded_attack = 99;

  int modded_decay = (int)current_patch.i3decay + (int)voice_env[vi].eff_i3decay;
  if (modded_decay < 0)  modded_decay = 0;
  if (modded_decay > 99) modded_decay = 99;

  int modded_release = (int)current_patch.i3release + (int)voice_env[vi].eff_i3release;
  if (modded_release < 0)  modded_release = 0;
  if (modded_release > 99) modded_release = 99;

  float aux_delay_s = lfo_delay_to_samples(current_patch.i3delay);
  float aux_atk_s   = env_time_to_samples_attack((uint32_t)modded_attack);
  float aux_hold_s  = env_time_to_samples_hold(current_patch.i3hold);
  float aux_dec_s   = env_time_to_samples_decay((uint32_t)modded_decay);
  float aux_sus_l   = (float)current_patch.i3sustain / 99.0f;
  float aux_rel_s   = env_time_to_samples_release((uint32_t)modded_release);
  if (aux_rel_s < 240.0f) aux_rel_s = 240.0f;

  return aux_env_level_at_sample(sample_count,
      aux_delay_s, aux_atk_s, aux_hold_s, aux_dec_s, aux_sus_l, aux_rel_s,
      voice_env[vi].aux_env_on_sample,
      voice_env[vi].aux_env_off_sample,
      voice_env[vi].aux_env_release_start);
}

static float s_compute_global_aux_env_level() {
  float max_level = 0.0f;
  for (int vi = 0; vi < 16; vi++) {
    float level = s_compute_voice_aux_env_level(vi);
    if (level > max_level) max_level = level;
  }
  return max_level;
}

float lfo_get_realtime_pitch_offset(uint8_t channel) {
  const proteus_patch_t& p = current_patch;
  float pitch_offset = 0.0f;

  // Current LFO values (phases already advanced by lfo_process_lfo_pitch_offset)
  float lfo1_val = (p.lfo1amount != 0) ? lfo_wave_shape(lfo_phase, p.lfo1shape) : 0.0f;
  float lfo2_val = (p.lfo2amount != 0) ? lfo_wave_shape(lfo2_phase, p.lfo2shape) : 0.0f;
  float aux_level = s_compute_global_aux_env_level();

  uint8_t srcs[8] = {
    p.realtimesource1, p.realtimesource2,
    p.realtimesource3, p.realtimesource4,
    p.realtimesource5, p.realtimesource6,
    p.realtimesource7, p.realtimesource8
  };
  uint8_t dests[8] = {
    p.realtimedest1, p.realtimedest2,
    p.realtimedest3, p.realtimedest4,
    p.realtimedest5, p.realtimedest6,
    p.realtimedest7, p.realtimedest8
  };

  for (int i = 0; i < 8; i++) {
    uint8_t src = srcs[i];
    uint8_t dest = dests[i];
    if (src == 0 || dest == 0) continue;

    float mod_val = 0.0f;
    float amt_scale = 0.0f;

    switch (src) {
      case 7:
        mod_val = lfo1_val;
        amt_scale = (float)p.lfo1amount * (1.0f / 127.0f);
        break;
      case 8:
        mod_val = lfo2_val;
        amt_scale = (float)p.lfo2amount * (1.0f / 127.0f);
        break;
      case 9:
        mod_val = aux_level;
        amt_scale = (float)p.i3amount * (1.0f / 127.0f);
        break;
      default:
        continue;
    }

    float scaled = mod_val * amt_scale;

    // dest 1 = Pitch (global, affects both)
    // dest 2 = Primary Pitch
    // dest 3 = Secondary Pitch
    bool applies = false;
    if (dest == 1) {
      applies = true;  // global pitch
    } else if (channel == 0 && dest == 2) {
      applies = true;  // primary pitch
    } else if (channel == 1 && dest == 3) {
      applies = true;  // secondary pitch
    }

    if (applies) {
      // 0.5f factor: full modulation (-128..127) = ±0.5 semitones
      pitch_offset += scaled * 0.5f;
    }
  }

  return pitch_offset;
}

void lfo_apply_patch_mod(float* out, uint32_t frames) {
  const proteus_patch_t& p = current_patch;
  float vol_mod = 1.0f;

  uint8_t srcs[8] = {
    p.realtimesource1, p.realtimesource2,
    p.realtimesource3, p.realtimesource4,
    p.realtimesource5, p.realtimesource6,
    p.realtimesource7, p.realtimesource8
  };
  uint8_t dests[8] = {
    p.realtimedest1, p.realtimedest2,
    p.realtimedest3, p.realtimedest4,
    p.realtimedest5, p.realtimedest6,
    p.realtimedest7, p.realtimedest8
  };

  for (int i = 0; i < 8; i++) {
    uint8_t src = srcs[i];
    uint8_t dest = dests[i];
    if (src == 0 || dest == 0) continue;

    float mod_val = 0.0f;
    float amt_scale = 0.0f;

    switch (src) {
      case 7:
        mod_val = (current_patch.lfo1amount != 0) ? lfo_wave_shape(lfo_phase, p.lfo1shape) : 0.0f;
        amt_scale = (float)p.lfo1amount * (1.0f / 127.0f);
        break;
      case 8:
        mod_val = (current_patch.lfo2amount != 0) ? lfo_wave_shape(lfo2_phase, p.lfo2shape) : 0.0f;
        amt_scale = (float)p.lfo2amount * (1.0f / 127.0f);
        break;
      case 9:
        if (current_patch.i3amount != 0) {
          mod_val = s_compute_global_aux_env_level();
          amt_scale = (float)p.i3amount * (1.0f / 127.0f);
        }
        break;
      default:
        continue;
    }

    float scaled = mod_val * amt_scale;

    switch (dest) {
      case 4: case 5: case 6:
        // Asymmetric modulation: positive amount caps at +0.5x boost
        // to prevent clipping; negative keeps full depth for silence.
        {
          float gain = (amt_scale >= 0.0f)
              ? 1.0f + 0.5f * scaled
              : 1.0f + scaled;
          vol_mod *= gain;
        }
        break;
      default:
        break;
    }
  }

  if (vol_mod < 0.0f) vol_mod = 0.0f;
  if (vol_mod != 1.0f) {
    for (uint32_t i = 0; i < frames * 2; i++)
      out[i] *= vol_mod;
  }
}
