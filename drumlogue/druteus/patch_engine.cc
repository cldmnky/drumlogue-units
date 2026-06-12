#include "patch_engine.h"
#include "druteus_state.h"
#include "params.h"
#include "tools/proteus_instrument_map.h"
#include "dsp_primitives.h"

namespace {

constexpr float kMidiMax             = 127.0f;
constexpr float kFineTuneScale       = 64.0f;
constexpr float kPanRange            = 14.0f;
constexpr float kPanOffset           =  7.0f;
constexpr float kDefaultPitchBend    =  2.0f;
constexpr float kMaxPitchBend        = 24.0f;
constexpr float kCrossfadeWidthScale = 255.0f;
constexpr int   kBalanceCenter       = 64;

}  // namespace

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

float cached_xfade_center = 0.5f;
float cached_xfade_width = 0.0f;
float cached_xfade_lo = 0.0f;
float cached_xfade_hi = 1.0f;
float cached_xfade_span = 0.0f;
uint8_t cached_xfade_split_key = 64;

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
  // Fall back to 0 — a single-preset SF2 has only preset 0 valid;
  // preset 1 is out of range and would yield silence (review #18).
  if (idx0 < 0)
    idx0 = 0;
  if (idx0 >= max_preset)
    idx0 = 0;
  voice_preset_primary = idx0;

  tsf_channel_set_presetindex(soundfont, 0, idx0);
  tsf_channel_midi_control(soundfont, 0, 7, current_patch.i1volume);
  tsf_channel_set_pan(soundfont, 0,
      ((float)current_patch.i1pan + kPanOffset) / kPanRange);
  patch_tune_primary =
      (float)(current_patch.i1tuningcoarse * 100 + current_patch.i1tuningfine) /
      100.0f;
  tsf_channel_set_tuning(soundfont, 0,
      patch_tune_primary + Params[param_fine_tune] / kFineTuneScale);

  {
    float pr;
    uint8_t pbr = current_patch.pitchbendrange;
    if (pbr == 0) pr = kDefaultPitchBend;
    else if (pbr <= 12) pr = (float)pbr;
    else pr = kMaxPitchBend;
    tsf_channel_set_pitchrange(soundfont, 0, pr);
    if (current_patch.i2volume > 0)
      tsf_channel_set_pitchrange(soundfont, 1, pr);
  }

  int idx1 = resolve_proteus_instrument_to_sf2_preset(
      (int)current_patch.i2instrument, max_preset);
  if (idx1 < 0)
    idx1 = 0;
  if (idx1 >= max_preset)
    idx1 = 0;
  if (current_patch.i2volume > 0) {
    patch_has_secondary = true;
    cached_env2_enabled = (current_patch.i2envelopeon != 0);
    voice_preset_secondary = idx1;

    tsf_channel_set_presetindex(soundfont, 1, idx1);
    tsf_channel_midi_control(soundfont, 1, 7, current_patch.i2volume);
    tsf_channel_set_pan(soundfont, 1,
        ((float)current_patch.i2pan + kPanOffset) / kPanRange);
    patch_tune_secondary =
        (float)(current_patch.i2tuningcoarse * 100 + current_patch.i2tuningfine) /
        100.0f;
    tsf_channel_set_tuning(soundfont, 1,
        patch_tune_secondary + Params[param_fine_tune] / kFineTuneScale);
  } else {
    cached_env2_enabled = false;
    patch_tune_secondary = 0.0f;
  }

  cached_xfade_center = clamp01(
      (current_patch.switchpoint + ((int)current_patch.crossfadebalance - kBalanceCenter)) / kMidiMax);
  cached_xfade_width = current_patch.crossfadeamount > 0
      ? (current_patch.crossfadeamount / kCrossfadeWidthScale) : 0.0f;
  cached_xfade_lo = clamp01(cached_xfade_center - 0.5f * cached_xfade_width);
  cached_xfade_hi = clamp01(cached_xfade_center + 0.5f * cached_xfade_width);
  cached_xfade_span = cached_xfade_hi - cached_xfade_lo;
  cached_xfade_split_key = (uint8_t)(cached_xfade_center * kMidiMax + 0.5f);
}

void s_apply_params() {
  if (soundfont == nullptr)
    return;
  tsf_set_max_voices(soundfont, Params[param_max_voices]);
  s_load_patch(Params[param_preset]);

  // Apply the latest user-set TSF channel state (volume/pan/fine-tune)
  // so it survives a soundfont reload — these are kept in pending_*
  // atomics on the audio thread side but the canonical value is Params[].
  int vol   = Params[param_volume];
  int pan   = Params[param_pan];
  int fine  = Params[param_fine_tune];

  tsf_channel_midi_control(soundfont, 0, 7, vol);
  if (patch_has_secondary)
    tsf_channel_midi_control(soundfont, 1, 7, vol);

  tsf_channel_set_pan(soundfont, 0, pan / 127.0f);
  if (patch_has_secondary)
    tsf_channel_set_pan(soundfont, 1, pan / 127.0f);

  tsf_channel_set_tuning(soundfont, 0,
      patch_tune_primary + fine / 64.0f);
  if (patch_has_secondary)
    tsf_channel_set_tuning(soundfont, 1,
        patch_tune_secondary + fine / 64.0f);

  tsf_channel_set_pitchwheel(soundfont, 0, (int)last_pitch_bend);
  if (patch_has_secondary)
    tsf_channel_set_pitchwheel(soundfont, 1, (int)last_pitch_bend);
}

void compute_crossfade_weights(uint8_t mode, uint8_t velocity, uint8_t note,
                                uint8_t switchpoint, uint8_t balance,
                                uint8_t amount, uint8_t direction,
                                float* primary_weight,
                                float* secondary_weight) {
  (void)switchpoint;
  (void)balance;
  (void)amount;

  float pri = 1.0f;
  float sec = 1.0f;

  if (mode == 1) {
    const float u = velocity / kMidiMax;

    if (cached_xfade_width <= 0.0f) {
      pri = (u < cached_xfade_center) ? 1.0f : 0.0f;
      sec = 1.0f - pri;
    } else {
      if (u <= cached_xfade_lo) {
        pri = 1.0f;
        sec = 0.0f;
      } else if (u >= cached_xfade_hi) {
        pri = 0.0f;
        sec = 1.0f;
      } else {
        const float t = (cached_xfade_span > 0.0f)
            ? ((u - cached_xfade_lo) / cached_xfade_span) : 0.5f;
        sec = clamp01(t);
        pri = 1.0f - sec;
      }
    }
  } else if (mode == 2) {
    const float u = note / kMidiMax;

    if (cached_xfade_width <= 0.0f) {
      pri = (u < cached_xfade_center) ? 1.0f : 0.0f;
      sec = 1.0f - pri;
    } else {
      if (u <= cached_xfade_lo) {
        pri = 1.0f;
        sec = 0.0f;
      } else if (u >= cached_xfade_hi) {
        pri = 0.0f;
        sec = 1.0f;
      } else {
        const float t = (cached_xfade_span > 0.0f)
            ? ((u - cached_xfade_lo) / cached_xfade_span) : 0.5f;
        sec = clamp01(t);
        pri = 1.0f - sec;
      }
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
