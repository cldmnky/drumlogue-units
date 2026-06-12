#include "lfo_engine.h"
#include "druteus_state.h"
#include "params.h"
#include "dsp_primitives.h"
#include <math.h>

float lfo_phase               = 0.0f;
float lfo2_phase              = 0.0f;
float lfo1_delay_completed    = 0.0f;
float lfo2_delay_completed    = 0.0f;
float aux_env_cached_         = 0.0f;

// Per-LFO sample-and-hold state — review #8.
// Previously the static state inside lfo_wave_shape was shared across
// all three LFOs (user LFO, patch LFO1, patch LFO2), so the wrap
// detector fired on whichever LFO happened to call the function last
// and S&H was effectively broken whenever more than one consumer used
// shape 4.
static struct {
  float s_h;
  float s_h_prev_phase;
  bool  initialized;
} s_sh_state[3] = { {0,0,false}, {0,0,false}, {0,0,false} };

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
      // Per-LFO slot 0 = user LFO, 1 = patch LFO1, 2 = patch LFO2.
      // Caller picks the slot via lfo_wave_shape_slot().
      return 0.0f;  // routed through lfo_wave_shape_slot
    }
    default: return sinf(phase * 2.0f * (float)M_PI);
  }
}

float lfo_wave_shape_slot(float phase, uint8_t shape, uint8_t slot) {
  if (shape == 4) {
    if (slot >= 3) slot = 0;
    auto& st = s_sh_state[slot];
    if (phase < st.s_h_prev_phase || !st.initialized) {
      st.s_h = ((float)((sample_count * 2654435761u) & 0x7FFFFFFF)
                / 2147483648.0f) * 2.0f - 1.0f;
      st.initialized = true;
    }
    st.s_h_prev_phase = phase;
    return st.s_h;
  }
  return lfo_wave_shape(phase, shape);
}

void lfo_reset_sh_state() {
  for (int i = 0; i < 3; i++) {
    s_sh_state[i].s_h = 0.0f;
    s_sh_state[i].s_h_prev_phase = 0.0f;
    s_sh_state[i].initialized = false;
  }
}

void lfo_init() {
  lfo_phase            = 0.0f;
  lfo2_phase           = 0.0f;
  lfo1_delay_completed = 0.0f;
  lfo2_delay_completed = 0.0f;
  lfo_reset_sh_state();
}

void lfo_process_lfo_pitch_offset(uint32_t frames) {
  // ── LFO1 delay ramp ──────────────────────────────────────
  if (current_patch.lfo1amount != 0 && current_patch.lfo1delay > 0) {
    float delay_samples = lfo_delay_to_samples(current_patch.lfo1delay);
    lfo1_delay_completed += (float)frames;
    if (lfo1_delay_completed > delay_samples)
      lfo1_delay_completed = (float)delay_samples;
  }

  // ── LFO1 phase advancement ───────────────────────────────
  if (current_patch.lfo1amount != 0) {
    float rate = 0.05f + current_patch.lfo1frequency * (24.95f / 127.0f);
    if (current_patch.lfo1variation > 0) {
      float var = (float)current_patch.lfo1variation / 127.0f;
      float rnd = ((float)((sample_count / 64) * 2654435761u & 0x7FFFFFFF)
                   / 2147483648.0f) - 1.0f;
      rate *= 1.0f + rnd * var * 0.5f;
      if (rate < 0.01f) rate = 0.01f;
    }
    lfo_phase += rate * (float)frames / 48000.0f;
    if (lfo_phase >= 1.0f) lfo_phase -= 1.0f;
  }

  // ── LFO2 delay ramp ──────────────────────────────────────
  if (current_patch.lfo2amount != 0 && current_patch.lfo2delay > 0) {
    float delay_samples = lfo_delay_to_samples(current_patch.lfo2delay);
    lfo2_delay_completed += (float)frames;
    if (lfo2_delay_completed > delay_samples)
      lfo2_delay_completed = (float)delay_samples;
  }

  // ── LFO2 phase advancement ───────────────────────────────
  if (current_patch.lfo2amount != 0) {
    float rate = 0.05f + current_patch.lfo2frequency * (24.95f / 127.0f);
    if (current_patch.lfo2variation > 0) {
      float var = (float)current_patch.lfo2variation / 127.0f;
      float rnd = ((float)(((sample_count / 64) + 1) * 2654435761u & 0x7FFFFFFF)
                   / 2147483648.0f) - 1.0f;
      rate *= 1.0f + rnd * var * 0.5f;
      if (rate < 0.01f) rate = 0.01f;
    }
    lfo2_phase += rate * (float)frames / 48000.0f;
    if (lfo2_phase >= 1.0f) lfo2_phase -= 1.0f;
  }

  // Pitch modulation is no longer computed here — it is handled
  // by the realtime modulation matrix (lfo_get_realtime_pitch_offset).
}

float lfo_process_user_mod(uint32_t frames, uint8_t dest) {
  float rate   = 0.05f + Params[param_lfo_rate] * (24.95f / 127.0f);
  float amt    = Params[param_lfo_amount] / 127.0f;
  uint8_t wave = Params[param_lfo_wave];
  lfo_phase += rate * (float)frames / 48000.0f;
  if (lfo_phase >= 1.0f) lfo_phase -= 1.0f;
  // User LFO uses slot 0 for S&H state.
  float lfo_val = lfo_wave_shape_slot(lfo_phase, wave, 0);
  if (dest == 0 || dest == 2)
    return lfo_val * amt * 0.5f;
  return 0.0f;
}

static float s_compute_voice_aux_env_level(int vi) {
  if (!voice_env[vi].active) return 0.0f;

  // eff_i3amount / eff_i3attack / eff_i3decay / eff_i3release are
  // already the per-voice effective values (keyvel-modulated, with
  // the base patch value baked in at note-on).  Adding
  // current_patch.i3* on top would double-count — review #7.
  int modded_amount = (int)voice_env[vi].eff_i3amount;
  if (modded_amount < -128) modded_amount = -128;
  if (modded_amount > 127)  modded_amount = 127;

  uint32_t eff_attack  = (uint32_t)voice_env[vi].eff_i3attack;
  uint32_t eff_decay   = (uint32_t)voice_env[vi].eff_i3decay;
  uint32_t eff_release = (uint32_t)voice_env[vi].eff_i3release;

  float aux_delay_s = lfo_delay_to_samples(current_patch.i3delay);
  float aux_atk_s   = env_time_to_samples_attack(eff_attack);
  float aux_hold_s  = env_time_to_samples_hold(current_patch.i3hold);
  float aux_dec_s   = env_time_to_samples_decay(eff_decay);
  float aux_sus_l   = (float)current_patch.i3sustain / 99.0f;
  float aux_rel_s   = env_time_to_samples_release(eff_release);
  if (aux_rel_s < 240.0f) aux_rel_s = 240.0f;

  return aux_env_level_at_sample(sample_count,
      aux_delay_s, aux_atk_s, aux_hold_s, aux_dec_s, aux_sus_l, aux_rel_s,
      voice_env[vi].aux_env_on_sample,
      voice_env[vi].aux_env_released ? voice_env[vi].aux_env_off_sample : 0,
      voice_env[vi].aux_env_release_start);
}

float lfo_compute_global_aux_env_level() {
  float max_level = 0.0f;
  for (int vi = 0; vi < 16; vi++) {
    float level = s_compute_voice_aux_env_level(vi);
    if (level > max_level) max_level = level;
  }
  return max_level;
}

void lfo_update_aux_env_cache() {
  aux_env_cached_ = lfo_compute_global_aux_env_level();
}

float lfo_get_realtime_pitch_offset(uint8_t channel) {
  const proteus_patch_t& p = current_patch;
  float pitch_offset = 0.0f;

  // Aux env level was already computed once this block by the caller
  // (cached in aux_env_cached_); fall back to a fresh compute if not.
  float lfo1_val = (p.lfo1amount != 0) ? lfo_wave_shape_slot(lfo_phase, p.lfo1shape, 1) : 0.0f;
  float lfo2_val = (p.lfo2amount != 0) ? lfo_wave_shape_slot(lfo2_phase, p.lfo2shape, 2) : 0.0f;
  float aux_level = aux_env_cached_;

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

    float delay_scale = 1.0f;
    switch (src) {
      case 7:
        if (p.lfo1delay > 0) {
          float ds = lfo_delay_to_samples(p.lfo1delay);
          delay_scale = lfo1_delay_completed >= ds ? 1.0f : lfo1_delay_completed / ds;
        }
        mod_val = lfo1_val;
        amt_scale = (float)p.lfo1amount * (1.0f / 127.0f) * delay_scale;
        break;
      case 8:
        if (p.lfo2delay > 0) {
          float ds = lfo_delay_to_samples(p.lfo2delay);
          delay_scale = lfo2_delay_completed >= ds ? 1.0f : lfo2_delay_completed / ds;
        }
        mod_val = lfo2_val;
        amt_scale = (float)p.lfo2amount * (1.0f / 127.0f) * delay_scale;
        break;
      case 9:
        mod_val = aux_level;
        amt_scale = (float)p.i3amount * (1.0f / 127.0f);
        break;
      default:
        continue;
    }

    float scaled = mod_val * amt_scale;

    bool applies = false;
    if (dest == 1) {
      applies = true;
    } else if (channel == 0 && dest == 2) {
      applies = true;
    } else if (channel == 1 && dest == 3) {
      applies = true;
    }

    if (applies) {
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

    float delay_scale = 1.0f;
    switch (src) {
      case 7:
        if (p.lfo1delay > 0) {
          float ds = lfo_delay_to_samples(p.lfo1delay);
          delay_scale = lfo1_delay_completed >= ds ? 1.0f : lfo1_delay_completed / ds;
        }
        mod_val = (current_patch.lfo1amount != 0) ? lfo_wave_shape_slot(lfo_phase, p.lfo1shape, 1) : 0.0f;
        amt_scale = (float)p.lfo1amount * (1.0f / 127.0f) * delay_scale;
        break;
      case 8:
        if (p.lfo2delay > 0) {
          float ds = lfo_delay_to_samples(p.lfo2delay);
          delay_scale = lfo2_delay_completed >= ds ? 1.0f : lfo2_delay_completed / ds;
        }
        mod_val = (current_patch.lfo2amount != 0) ? lfo_wave_shape_slot(lfo2_phase, p.lfo2shape, 2) : 0.0f;
        amt_scale = (float)p.lfo2amount * (1.0f / 127.0f) * delay_scale;
        break;
      case 9:
        if (current_patch.i3amount != 0) {
          mod_val = aux_env_cached_;
          amt_scale = (float)p.i3amount * (1.0f / 127.0f);
        }
        break;
      default:
        continue;
    }

    float scaled = mod_val * amt_scale;

    switch (dest) {
      case 4: case 5: case 6:
        {
          float gain = (amt_scale >= 0.0f)
              ? 1.0f + 0.5f * scaled
              : 1.0f + scaled;
          if (gain > 1.5f) gain = 1.5f;
          if (gain < 0.0f) gain = 0.0f;
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
