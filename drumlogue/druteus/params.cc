#include "params.h"
#include "druteus_state.h"
#include "sf_loader.h"
#include "patch_engine.h"
#include "tools/proteus_patches.h"

const int32_t kParamDefaults[param_num] = {
  0,      // SFONT
  0,      // PRESET
  16,     // VOICES
  0,      // TUNE: no transpose
  0,      // FINETN: concert pitch
  100,    // VOLUME
  64,     // PAN: center
  0,      // unused
  0,      // XFADE: off
  0,      // LAYERS: both
  0,      // unused
  0,      // unused
  0,      // CHORUS: off
  0,      // REVERB: off
  0,      // V.CURVE: linear
  0,      // unused
  127,    // CUTOFF: fully open
  0,      // RES: no resonance
  0,      // unused
  0,      // unused
  0,      // LFO RTE: off
  0,      // LFO AMT: off
  0,      // LFO DST: pitch
  1,      // LFO WAV: sine
};

int32_t Params[param_num];

void params_set(uint8_t index, int32_t value) {
  if (index >= param_num)
    return;

  switch (index) {
    case param_soundfont:
      if (soundfont_list.count <= 0)
        break;
      if (value >= soundfont_list.count)
        value = soundfont_list.count - 1;
      if (value < 0)
        value = 0;
      if (value == Params[index])
        break;
      if (soundfont != nullptr)
        tsf_channel_sounds_off_all(soundfont, 0);
      state = SF_LOAD_START;
      break;

    case param_preset:
      if (value < 0)  value = 0;
      if (value >= PROTEUS_PATCH_COUNT)
        value = PROTEUS_PATCH_COUNT - 1;
      if (value != Params[index]) {
        Params[index] = value;
        s_load_patch((uint16_t)value);
      }
      return;

    case param_max_voices:
      if (value < 1)  value = 1;
      if (value > 16) value = 16;
      voice_allocator.Init((uint8_t)value);
      voice_allocator.SetMode(common::VoiceMode::Polyphonic);
      voice_allocator.SetAllocationStrategy(common::VoiceAllocationStrategy::OldestNote);
      if (soundfont != nullptr)
        tsf_set_max_voices(soundfont, value);
      break;

    case param_transpose:
      if (value < -12) value = -12;
      if (value > 12)  value = 12;
      break;

    case param_volume:
      if (value < 0)   value = 0;
      if (value > 127) value = 127;
      if (soundfont != nullptr) {
        tsf_channel_midi_control(soundfont, 0, 7, value);
        if (patch_has_secondary)
          tsf_channel_midi_control(soundfont, 1, 7, value);
      }
      break;

    case param_pan:
      if (value < 0)   value = 0;
      if (value > 127) value = 127;
      if (soundfont != nullptr) {
        tsf_channel_set_pan(soundfont, 0, value / 127.0f);
        if (patch_has_secondary)
          tsf_channel_set_pan(soundfont, 1, value / 127.0f);
      }
      break;

    case param_velocity_curve:
      if (value < 0) value = 0;
      if (value > 4) value = 4;
      break;

    case param_fine_tune:
      if (value < -63) value = -63;
      if (value > 63)  value = 63;
      if (soundfont != nullptr) {
        float fine = value / 64.0f;
        tsf_channel_set_tuning(soundfont, 0, patch_tune_primary + fine);
        if (patch_has_secondary)
          tsf_channel_set_tuning(soundfont, 1, patch_tune_secondary + fine);
      }
      break;

    case param_xfade:
      if (value < 0) value = 0;
      if (value > 2) value = 2;
      break;

    case param_layers:
      if (value < 0) value = 0;
      if (value > 2) value = 2;
      break;

    case param_unused_10:
    case param_unused_11:
      break;

    case param_cutoff:
      if (value < 0)   value = 0;
      if (value > 127) value = 127;
      break;

    case param_resonance:
      if (value < 0)   value = 0;
      if (value > 127) value = 127;
      break;

    case param_lfo_rate:
      if (value < 0)   value = 0;
      if (value > 127) value = 127;
      break;

    case param_lfo_amount:
      if (value < 0)   value = 0;
      if (value > 127) value = 127;
      break;

    case param_lfo_dest:
      if (value < 0) value = 0;
      if (value > 2) value = 2;
      break;

    case param_lfo_wave:
      if (value < 0) value = 0;
      if (value > 4) value = 4;
      break;

    default:
      break;
  }

  Params[index] = value;
}

int32_t params_get(uint8_t index) {
  if (index < param_num)
    return Params[index];
  return 0;
}

const char *params_get_str(uint8_t index, int32_t value) {
  value = (int16_t)value;

  switch (index) {
    case param_soundfont:
      if (soundfont_list.count <= 0)
        return "NO SF2";
      if (value < 0)
        value = 0;
      if (value >= soundfont_list.count)
        value = soundfont_list.count - 1;
      return soundfont_list.get(value);

    case param_preset:
      if (value < 0) value = 0;
      if (value >= PROTEUS_PATCH_COUNT)
        value = PROTEUS_PATCH_COUNT - 1;
      return kProteusPatchTable[value].name;

    case param_xfade: {
      static const char *xfade_names[] = { "OFF", "VEL", "KEY" };
      if (value < 0) value = 0;
      if (value > 2) value = 2;
      return xfade_names[value];
    }

    case param_layers: {
      static const char *layer_names[] = { "BOTH", "PRI", "SEC" };
      if (value < 0) value = 0;
      if (value > 2) value = 2;
      return layer_names[value];
    }

    case param_velocity_curve: {
      static const char *curve_names[] = {
        "LINEAR", "EXP", "LOG", "COMP", "STEEP"
      };
      if (value < 0) value = 0;
      if (value > 4) value = 4;
      return curve_names[value];
    }

    case param_lfo_dest: {
      static const char *dest_names[] = { "PITCH", "VOL", "BOTH" };
      if (value < 0) value = 0;
      if (value > 2) value = 2;
      return dest_names[value];
    }

    case param_lfo_wave: {
      static const char *wave_names[] = { "TRI", "SINE", "SQR", "SAW", "RND" };
      if (value < 0) value = 0;
      if (value > 4) value = 4;
      return wave_names[value];
    }

    default:
      return nullptr;
  }
}
