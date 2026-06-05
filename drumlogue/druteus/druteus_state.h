#pragma once

#include <stdint.h>
#include <atomic>
#include "tsf.h"
#include "voice_allocator.h"
#include "../common/stereo_widener.h"
#include "rings/dsp/fx/reverb.h"
#include "filter.h"
#include "logue_fs.h"
#include "tools/proteus_patches.h"

extern const char *sf2_prefix;
extern const char *sf2_suffix;
extern fs_dir soundfont_list;

extern tsf *soundfont;
extern char * __attribute__((aligned(32))) soundfont_buf;

extern volatile uint32_t state;
extern volatile bool suspended;
extern std::atomic<bool> patch_dirty;

extern common::ChorusStereoWidener chorus_dsp;
extern rings::Reverb reverb_dsp;
extern uint16_t reverb_buffer[32768];

extern SVFilter filter_l;
extern SVFilter filter_r;
extern float fx_buf_l[256];
extern float fx_buf_r[256];
extern uint64_t sample_count;
extern uint16_t last_pitch_bend;

extern common::VoiceAllocatorCore voice_allocator;

extern proteus_patch_t current_patch;
extern bool patch_has_secondary;
extern int voice_preset_primary;
extern int voice_preset_secondary;
extern float patch_tune_primary;
extern float patch_tune_secondary;

extern uint32_t cached_env_atk;
extern uint32_t cached_env_hold;
extern uint32_t cached_env_dec;
extern uint32_t cached_env_sus;
extern uint32_t cached_env_rel;
extern bool cached_env_enabled;
extern uint32_t cached_env2_atk;
extern uint32_t cached_env2_hold;
extern uint32_t cached_env2_dec;
extern uint32_t cached_env2_sus;
extern uint32_t cached_env2_rel;
extern bool cached_env2_enabled;

extern float cached_xfade_center;
extern float cached_xfade_width;
extern float cached_xfade_lo;
extern float cached_xfade_hi;
extern float cached_xfade_span;
extern uint8_t cached_xfade_split_key;

struct VoiceEnv {
  bool     active;
  uint8_t  note;
  uint64_t note_on_sample;
  uint64_t note_off_sample;
  float    release_start_level;
  uint64_t note2_on_sample;
  uint64_t note2_off_sample;
  float    release2_start_level;
  /* —– per-layer note-on delay —– */
  bool     note1_pending;
  uint8_t  note1_pending_note;
  float    note1_pending_vel;
  uint64_t note1_pending_sample;
  bool     note2_pending;
  uint8_t  note2_pending_note;
  float    note2_pending_vel;
  uint64_t note2_pending_sample;
  /* —– auxiliary envelope —– */
  uint64_t aux_env_on_sample;
  uint64_t aux_env_off_sample;
  float    aux_env_release_start;
};
extern VoiceEnv voice_env[16];
extern int active_notes;

extern float lfo_phase;
extern float lfo2_phase;
extern float lfo_delay_completed;
