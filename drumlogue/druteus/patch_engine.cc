#include "patch_engine.h"
#include "druteus_state.h"
#include "params.h"
#include "tools/proteus_instrument_map.h"
#include "dsp_primitives.h"

proteus_patch_t current_patch;
bool patch_has_secondary = false;
int voice_preset_primary = 1;
int voice_preset_secondary = 1;
float patch_tune_primary = 0.0f;
float patch_tune_secondary = 0.0f;

uint32_t cached_env_atk = 0;
uint32_t cached_env_hold = 0;
uint32_t cached_env_dec = 0;
uint32_t cached_env_sus = 99;
uint32_t cached_env_rel = 99;
bool cached_env_enabled = false;
uint32_t cached_env2_atk = 0;
uint32_t cached_env2_hold = 0;
uint32_t cached_env2_dec = 0;
uint32_t cached_env2_sus = 99;
uint32_t cached_env2_rel = 99;
bool cached_env2_enabled = false;

void s_load_patch(uint16_t patch_idx) {
  if (patch_idx >= PROTEUS_PATCH_COUNT)
    return;

  current_patch = kProteusPatchTable[patch_idx];
  patch_has_secondary = false;

  cached_env_atk     = current_patch.i1attack;
  cached_env_hold    = current_patch.i1hold;
  cached_env_dec     = current_patch.i1decay;
  cached_env_sus     = current_patch.i1sustain;
  cached_env_rel     = current_patch.i1release;
  cached_env_enabled = (current_patch.i1envelopeon != 0);
  cached_env2_atk    = current_patch.i2attack;
  cached_env2_hold   = current_patch.i2hold;
  cached_env2_dec    = current_patch.i2decay;
  cached_env2_sus    = current_patch.i2sustain;
  cached_env2_rel    = current_patch.i2release;
  cached_env2_enabled = false;

  if (soundfont == nullptr)
    return;

  int max_preset = tsf_get_presetcount(soundfont);
  if (max_preset <= 0)
    return;

  int idx0 = resolve_proteus_instrument_to_sf2_preset(
      (int)current_patch.i1instrument, max_preset);
  if (idx0 < 0)
    idx0 = 1;
  voice_preset_primary = idx0;

  tsf_channel_set_presetindex(soundfont, 0, idx0);
  tsf_channel_midi_control(soundfont, 0, 7, current_patch.i1volume);
  tsf_channel_set_pan(soundfont, 0,
      ((int)current_patch.i1pan + 7) * 9 / 127.0f);
  patch_tune_primary =
      (float)(current_patch.i1tuningcoarse * 100 + current_patch.i1tuningfine) /
      100.0f;
  tsf_channel_set_tuning(soundfont, 0,
      patch_tune_primary + Params[param_fine_tune] / 64.0f);

  {
    float pr;
    uint8_t pbr = current_patch.pitchbendrange;
    if (pbr == 0) pr = 2.0f;
    else if (pbr <= 12) pr = (float)pbr;
    else pr = 24.0f;
    tsf_channel_set_pitchrange(soundfont, 0, pr);
    if (current_patch.i2volume > 0)
      tsf_channel_set_pitchrange(soundfont, 1, pr);
  }

  int idx1 = resolve_proteus_instrument_to_sf2_preset(
      (int)current_patch.i2instrument, max_preset);
  // Fall back to preset 1 if the i2 instrument is unmapped. We always try
  // to enable the secondary layer when i2volume > 0, matching the pre-refactor
  // behavior for valid mappings and gracefully degrading for unmapped ones.
  if (idx1 < 0)
    idx1 = 1;
  if (current_patch.i2volume > 0) {
    patch_has_secondary = true;
    cached_env2_enabled = (current_patch.i2envelopeon != 0);
    voice_preset_secondary = idx1;

    tsf_channel_set_presetindex(soundfont, 1, idx1);
    tsf_channel_midi_control(soundfont, 1, 7, current_patch.i2volume);
    tsf_channel_set_pan(soundfont, 1,
        ((int)current_patch.i2pan + 7) * 9 / 127.0f);
    patch_tune_secondary =
        (float)(current_patch.i2tuningcoarse * 100 + current_patch.i2tuningfine) /
        100.0f;
    tsf_channel_set_tuning(soundfont, 1,
        patch_tune_secondary + Params[param_fine_tune] / 64.0f);
  } else {
    cached_env2_enabled = false;
    patch_tune_secondary = 0.0f;
  }
}

void s_apply_params() {
  if (soundfont == nullptr)
    return;
  tsf_set_max_voices(soundfont, Params[param_max_voices]);
  s_load_patch(Params[param_preset]);
  tsf_channel_set_pitchwheel(soundfont, 0, (int)last_pitch_bend);
  if (patch_has_secondary)
    tsf_channel_set_pitchwheel(soundfont, 1, (int)last_pitch_bend);
}

void compute_crossfade_weights(uint8_t mode, uint8_t velocity, uint8_t note,
                                uint8_t switchpoint, uint8_t balance,
                                uint8_t amount, uint8_t direction,
                                float* primary_weight,
                                float* secondary_weight) {
  float pri = 1.0f;
  float sec = 1.0f;

  const float center = clamp01((switchpoint + ((int)balance - 64)) / 127.0f);

  if (mode == 1) {
    const float u = velocity / 127.0f;
    const float width = amount > 0 ? (amount / 255.0f) : 0.0f;
    if (width <= 0.0f) {
      pri = (u < center) ? 1.0f : 0.0f;
      sec = 1.0f - pri;
    } else {
      const float lo = clamp01(center - 0.5f * width);
      const float hi = clamp01(center + 0.5f * width);
      if (u <= lo) {
        pri = 1.0f;
        sec = 0.0f;
      } else if (u >= hi) {
        pri = 0.0f;
        sec = 1.0f;
      } else {
        const float span = (hi - lo);
        const float t = (span > 0.0f) ? ((u - lo) / span) : 0.5f;
        sec = clamp01(t);
        pri = 1.0f - sec;
      }
    }
  } else if (mode == 2) {
    const uint8_t split_key = (uint8_t)(center * 127.0f + 0.5f);
    if (note < split_key) {
      pri = 1.0f;
      sec = 0.0f;
    } else {
      pri = 0.0f;
      sec = 1.0f;
    }
  }

  if (direction != 0) {
    const float tmp = pri;
    pri = sec;
    sec = tmp;
  }

  *primary_weight = pri;
  *secondary_weight = sec;
}
