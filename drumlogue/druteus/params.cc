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
      // Store the new index FIRST so the audio thread can't observe
      // `reload_requested` set while still seeing the old index
      // (review #3 — order of stores matters here).
      Params[index] = value;
      reload_requested.store(true, std::memory_order_release);
      break;

    case param_preset:
      if (value < 0)  value = 0;
      if (value >= PROTEUS_PATCH_COUNT)
        value = PROTEUS_PATCH_COUNT - 1;
      if (value != Params[index]) {
        Params[index] = value;
        patch_dirty.store(true, std::memory_order_release);
      }
      return;

    case param_max_voices:
      if (value < 1)  value = 1;
      if (value > 16) value = 16;
      // Defer tsf_set_max_voices to the audio thread — it reallocates
      // f->voices and would race with tsf_render_float (review #1).
      pending_max_voices.store(value, std::memory_order_relaxed);
      voices_dirty.store(true, std::memory_order_release);
      break;

    case param_transpose:
      if (value < -12) value = -12;
      if (value > 12)  value = 12;
      break;

    case param_volume:
      if (value < 0)   value = 0;
      if (value > 127) value = 127;
      // Defer: tsf_channel_midi_control walks live voices and would
      // race with tsf_render_float (review #10).
      pending_volume.store(value, std::memory_order_relaxed);
      break;

    case param_pan:
      if (value < 0)   value = 0;
      if (value > 127) value = 127;
      pending_pan.store(value, std::memory_order_relaxed);
      break;

    case param_velocity_curve:
      if (value < 0) value = 0;
      if (value > 4) value = 4;
      break;

    case param_fine_tune:
      if (value < -63) value = -63;
      if (value > 63)  value = 63;
      pending_fine_tune.store(value, std::memory_order_relaxed);
      break;

    case param_xfade:
      if (value < 0) value = 0;
      if (value > 2) value = 2;
      break;

    case param_layers:
      if (value < 0) value = 0;
      if (value > 2) value = 2;
      break;

    case param_trance_gate:
      if (value < 0) value = 0;
      if (value > 32) value = 32;
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

    case param_trance_gate: {
      if (value <= 0) return "OFF";
      if (value > 32) value = 32;
      static const char* gate_names[33] = {
        "OFF","1","2","3","4","5","6","7","8","9","10",
        "11","12","13","14","15","16","17","18","19","20",
        "21","22","23","24","25","26","27","28","29","30",
        "31","32"
      };
      return gate_names[value % 33];
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

    case param_unused_18:
      return unit_get_perf_display();

    default:
      return nullptr;
  }
}
