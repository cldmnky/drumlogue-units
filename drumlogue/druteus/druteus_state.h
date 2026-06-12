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

// Loader state machine state.  Touched by the audio thread only;
// `reload_requested` (separate atomic, see below) is the cross-thread
// trigger from the control thread.
extern std::atomic<uint32_t> state;

// Cross-thread reload trigger: control thread sets this to true (release);
// audio thread sees it, advances `state` to SF_LOAD_START, then clears it.
// Decoupled from `state` to avoid lost-update races on the read-modify-write.
extern std::atomic<bool> reload_requested;

extern std::atomic<bool> suspended;
extern std::atomic<bool> patch_dirty;

// Deferred TSF state.  Param writes from the control thread set the
// corresponding "pending" value (release) and raise `voices_dirty`;
// the audio thread applies them at the top of `unit_render`.
extern std::atomic<bool> voices_dirty;
extern std::atomic<int>  pending_max_voices;
extern std::atomic<int>  pending_volume;
extern std::atomic<int>  pending_pan;
extern std::atomic<int>  pending_fine_tune;

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
extern float cached_xfade_switch_point;
extern float cached_xfade_width;
extern float cached_xfade_lo;
extern float cached_xfade_hi;
extern float cached_xfade_span;

struct VoiceEnv {
  bool     active;
  bool     released;             /* explicit "note-off received" flag
                                    (replaces the note_off_sample==0
                                    sentinel — see review #16) */
  uint8_t  note;                 /* the adjusted (transposed) MIDI key
                                    the TSF voice was started on */
  uint8_t  raw_note;             /* original MIDI note from caller */
  uint64_t note_on_sample;
  uint64_t note_off_sample;
  float    release_start_level;
  uint64_t note2_on_sample;
  uint64_t note2_off_sample;
  bool     released2;
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
  bool     aux_env_released;
  float    aux_env_release_start;
  /* —– key/velocity modulation (computed at note-on) —– */
  float keyvel_volume_mod;         /* 1.0 = no change */
  float keyvel_pan_mod;            /* 0.0 = center (shift in ±pan-steps) */
  float keyvel_tone_mod;           /* 0.0 = no shift (filter cutoff fraction) */
  float xfade_pri_weight;          /* base primary/secondary balance at note-on */
  float xfade_sec_weight;
  uint8_t keyvel_sample_start_pri; /* per-note sample-start offset */
  uint8_t keyvel_sample_start_sec; /* per-note sample-start offset */
  /* —– effective aux envelope params (keyvel-modulated, for per-voice use) —– */
  int8_t  eff_i3amount;            /* effective i3amount after keyvel mod */
  uint8_t eff_i3attack;            /* effective i3attack after keyvel mod */
  uint8_t eff_i3decay;             /* effective i3decay after keyvel mod */
  uint8_t eff_i3release;           /* effective i3release after keyvel mod */
};
extern VoiceEnv voice_env[16];
extern int active_notes;

extern float lfo_phase;
extern float lfo2_phase;
extern float lfo1_delay_completed;
extern float lfo2_delay_completed;
