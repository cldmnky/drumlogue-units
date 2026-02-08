#pragma once
/*
 *  File: synth.h
 *
 *  Drumpler Synth Class - JV-880 Emulator Integration
 *  Wraps the Nuked-SC55/JV-880 emulator for drumlogue
 *
 *  Original emulator: Copyright (C) 2021, 2024 nukeykt
 *  Non-commercial use only (MAME-style BSD)
 *
 *  2021-2024 (c) Korg
 *
 */

#include <atomic>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstdio>

#include "unit.h"  // Note: Include common definitions for all units
#include "emulator/jv880_wrapper.h"  // JV-880 emulator wrapper
#include "../common/perf_mon.h"

#ifdef USE_NEON
#define NEON_DSP_NS drumpler
#include "../common/neon_dsp.h"
#undef NEON_DSP_NS
#endif

#ifndef fast_inline
#define fast_inline inline
#endif

enum ParamId {
  kParamPart = 0,
  kParamPoly,
  kParamLevel,
  kParamPan,
  kParamTone,
  kParamCutoff,
  kParamResonance,
  kParamAttack,
  kParamReverb,
  kParamChorus,
  kParamDelay,
  kParamBlank,
};

static inline int clamp_int(int value, int min_value, int max_value) {
  if (value < min_value) return min_value;
  if (value > max_value) return max_value;
  return value;
}

#ifdef DRUMPLER_ROM_EMBEDDED
extern const unsigned char g_drumpler_rom[];
extern const unsigned int g_drumpler_rom_size;
#endif

class Synth {
 public:
  /*===========================================================================*/
  /* Public Data Structures/Types. */
  /*===========================================================================*/

  /*===========================================================================*/
  /* Lifecycle Methods. */
  /*===========================================================================*/

  Synth(void)
      : emulator_(),
        initialized_(false),
        part_(1),
        poly_(16),
        level_(100),
        pan_(0),
        tone_(0),
        cutoff_(100),
        resonance_(0),
        attack_(0),
        reverb_(0),
        chorus_(0),
        delay_(0),
        preset_index_(0),
        channel_(0),
        last_note_(60),
        last_velocity_(100),
        silence_frames_(0),
        is_idle_(true),
        warmup_remaining_(0) {}
  ~Synth(void) {}

  inline int8_t Init(const unit_runtime_desc_t * desc) {
    // Check compatibility of samplerate with unit, for drumlogue should be 48000
    if (desc->samplerate != 48000)
      return k_unit_err_samplerate;

    // Check compatibility of frame geometry
    if (desc->output_channels != 2)  // should be stereo output
      return k_unit_err_geometry;

#ifdef DRUMPLER_ROM_EMBEDDED
#ifdef DEBUG
    fprintf(stderr, "[Drumpler] Initializing with embedded ROM (%u bytes)\n", g_drumpler_rom_size);
    fflush(stderr);
#endif
    // Initialize emulator with embedded ROM
    if (!emulator_.Init(g_drumpler_rom, g_drumpler_rom_size)) {
#ifdef DEBUG
      fprintf(stderr, "[Drumpler] ERROR: Emulator init failed!\n");
#endif
      return k_unit_err_memory;  // Failed to initialize emulator
    }
    
    // Deferred warmup: MCU firmware boots progressively during first Render() calls.
    // This avoids blocking unit_init() for 10+ seconds which causes drumlogue to hang.
    // ApplyAllParams() is called automatically when warmup completes in Render().
    warmup_remaining_ = 64000;  // ~1.3s of 48kHz frames for MCU firmware boot
    initialized_ = true;
  #ifdef PERF_MON
    PERF_MON_INIT();
    perf_render_total_ = PERF_MON_REGISTER("RenderTotal");
    perf_emulator_ = PERF_MON_REGISTER("Emulator");
    perf_interleave_ = PERF_MON_REGISTER("Interleave");
  #endif
#else
#ifdef DEBUG
    fprintf(stderr, "[Drumpler] ERROR: DRUMPLER_ROM_EMBEDDED not defined!\n");
#endif
    // Without embedded ROM, unit is non-functional
    initialized_ = false;
#endif

    return k_unit_err_none;
  }

  inline void Teardown() {
    // Note: cleanup and release resources if any
    initialized_ = false;
  }

  inline void Reset() {
    // Reset emulator (GS Reset)
    if (initialized_) {
      emulator_.Reset();
    }
  }

  inline void Resume() {
    // Note: Synth will resume and exit suspend state. Usually means the synth
    // was selected and the render callback will be called again
  }

  inline void Suspend() {
    // Note: Synth will enter suspend state. Usually means another synth was
    // selected and thus the render callback will not be called
  }

  /*===========================================================================*/
  /* Other Public Methods. */
  /*===========================================================================*/

  fast_inline void Render(float * out, size_t frames) {
    if (!initialized_ || frames == 0) {
      // Output silence if not initialized
#ifdef DEBUG
      static int warn_count = 0;
      if (warn_count++ < 5) {
        fprintf(stderr, "[Drumpler] Render: outputting silence (initialized=%d, frames=%zu)\n", initialized_, frames);
      }
#endif
    #ifdef USE_NEON
      drumpler::neon::ClearBuffer(out, static_cast<uint32_t>(frames * 2));
    #else
      float * __restrict out_p = out;
      const float * out_e = out_p + (frames << 1);  // assuming stereo output
      for (; out_p != out_e; out_p += 2) {
        out_p[0] = 0.0f;
        out_p[1] = 0.0f;
      }
    #endif
      return;
    }

    // Deferred warmup: boot MCU firmware progressively during first render calls
    if (warmup_remaining_ > 0) {
      static float warmup_l[128], warmup_r[128];
      const uint32_t chunk = (frames < 128) ? static_cast<uint32_t>(frames) : 128;
      emulator_.Render(warmup_l, warmup_r, chunk);
      warmup_remaining_ -= static_cast<int32_t>(frames);
      if (warmup_remaining_ <= 0) {
        // MCU firmware booted — apply initial parameters
        ApplyAllParams();
      }
    #ifdef USE_NEON
      drumpler::neon::ClearBuffer(out, static_cast<uint32_t>(frames * 2));
    #else
      float * __restrict clr_p = out;
      const float * clr_e = clr_p + (frames << 1);
      for (; clr_p != clr_e; clr_p += 2) {
        clr_p[0] = 0.0f;
        clr_p[1] = 0.0f;
      }
    #endif
      return;
    }

    // OPTIMIZATION: Skip emulation when idle (no notes for 2 seconds)
    // This dramatically reduces CPU load when no audio is playing
    if (is_idle_) {
    #ifdef USE_NEON
      drumpler::neon::ClearBuffer(out, static_cast<uint32_t>(frames * 2));
    #else
      float * __restrict out_p = out;
      const float * out_e = out_p + (frames << 1);
      for (; out_p != out_e; out_p += 2) {
        out_p[0] = 0.0f;
        out_p[1] = 0.0f;
      }
    #endif
      return;
    }

  #ifdef PERF_MON
    PERF_MON_START(perf_render_total_);
  #endif

    // Render emulator output (internally resampled from 64kHz to 48kHz)
    // Output format: interleaved stereo [L0, R0, L1, R1, ...]
    float * __restrict out_p = out;

    static constexpr size_t kRenderBlockFrames = 128;
    static float temp_l[kRenderBlockFrames];
    static float temp_r[kRenderBlockFrames];
    
#ifdef DEBUG
    static int render_debug_count = 0;
#endif

    size_t remaining = frames;
    size_t out_index = 0;

    while (remaining > 0) {
      const size_t render_frames = (remaining > kRenderBlockFrames)
                                    ? kRenderBlockFrames
                                    : remaining;

#ifdef DEBUG
      if (render_debug_count++ < 3) {
        fprintf(stderr, "[Drumpler] synth.h Render: calling emulator_.Render() with %zu frames\n", render_frames);
        fflush(stderr);
      }
#endif

    #ifdef PERF_MON
      PERF_MON_START(perf_emulator_);
    #endif
      emulator_.Render(temp_l, temp_r, render_frames);
    #ifdef PERF_MON
      PERF_MON_END(perf_emulator_);
    #endif
    
#ifdef DEBUG
    if (render_debug_count <= 3) {
      // Check if output has non-zero samples
      float max_val = 0.0f;
      for (size_t i = 0; i < render_frames; ++i) {
        float abs_l = fabsf(temp_l[i]);
        float abs_r = fabsf(temp_r[i]);
        if (abs_l > max_val) max_val = abs_l;
        if (abs_r > max_val) max_val = abs_r;
      }
      fprintf(stderr, "[Drumpler] synth.h Render: emulator output max=%f\n", max_val);
      fflush(stderr);
    }
#endif
    
      // Interleave L/R into output buffer
    #ifdef PERF_MON
      PERF_MON_START(perf_interleave_);
    #endif
    #ifdef USE_NEON
      drumpler::neon::InterleaveStereo(temp_l, temp_r,
                   out_p + out_index * 2,
                   static_cast<uint32_t>(render_frames));
    #else
      for (size_t i = 0; i < render_frames; ++i) {
        const size_t dst = (out_index + i) * 2;
        out_p[dst + 0] = temp_l[i];
        out_p[dst + 1] = temp_r[i];
      }
    #endif
    #ifdef PERF_MON
      PERF_MON_END(perf_interleave_);
    #endif

      out_index += render_frames;
      remaining -= render_frames;
    }

    // Check for silence to enable idle mode
    // Use NEON MaxAbsBuffer with early-exit threshold — when audio is playing,
    // the first chunk typically exceeds kSilenceLevel so this is O(1).
    float max_abs = drumpler::neon::MaxAbsBuffer(out, static_cast<uint32_t>(frames * 2), kSilenceLevel);
    
    if (max_abs < kSilenceLevel) {
      silence_frames_ += frames;
      if (silence_frames_ >= kSilenceThreshold) {
        is_idle_ = true;
      }
    } else {
      silence_frames_ = 0;
    }

  #ifdef PERF_MON
    PERF_MON_END(perf_render_total_);
  #endif
  }

  inline void setParameter(uint8_t index, int32_t value) {
    if (!initialized_) return;

    switch (index) {
      case kParamPart:
        part_ = clamp_int(static_cast<int>(value), 1, 16);
        channel_ = static_cast<uint8_t>(part_ - 1);
        ApplyAllParams();
        break;
      case kParamPoly:
        poly_ = clamp_int(static_cast<int>(value), 1, 32);
        break;
      case kParamLevel:
        level_ = clamp_int(static_cast<int>(value), 0, 100);
        SendCc(7, PercentToMidi(level_));
        break;
      case kParamPan:
        pan_ = clamp_int(static_cast<int>(value), -63, 63);
        SendCc(10, static_cast<uint8_t>(pan_ + 64));
        break;
      case kParamTone:
        tone_ = clamp_int(static_cast<int>(value), 0, 127);
        preset_index_ = static_cast<uint8_t>(tone_);
        // JV-880 requires direct nvram patch write, not standard MIDI Program Change
        emulator_.SetCurrentProgram(static_cast<uint8_t>(tone_));
        break;
      case kParamCutoff:
        cutoff_ = clamp_int(static_cast<int>(value), 0, 100);
        // Send cutoff to all 4 tones via SysEx (TVF Cutoff Frequency at tone offset 0x4A)
        {
          uint8_t midi_val = PercentToMidi(cutoff_);
          for (uint8_t t = 0; t < 4; ++t) {
            emulator_.SendSysexPatchToneParam(t, 0x4A, midi_val);
          }
        }
        break;
      case kParamResonance:
        resonance_ = clamp_int(static_cast<int>(value), 0, 100);
        // Send resonance to all 4 tones via SysEx (TVF Resonance at tone offset 0x4B)
        {
          uint8_t midi_val = PercentToMidi(resonance_);
          for (uint8_t t = 0; t < 4; ++t) {
            emulator_.SendSysexPatchToneParam(t, 0x4B, midi_val);
          }
        }
        break;
      case kParamAttack:
        attack_ = clamp_int(static_cast<int>(value), 0, 100);
        // Send attack time to all 4 tones via SysEx (TVA Env Time 1 at tone offset 0x69)
        {
          uint8_t midi_val = PercentToMidi(attack_);
          for (uint8_t t = 0; t < 4; ++t) {
            emulator_.SendSysexPatchToneParam(t, 0x69, midi_val);
          }
        }
        break;
      case kParamReverb:
        reverb_ = clamp_int(static_cast<int>(value), 0, 100);
        // Reverb Level via SysEx Patch Common param (offset 0x0E)
        emulator_.SendSysexPatchCommonParam(0x0E, PercentToMidi(reverb_));
        break;
      case kParamChorus:
        chorus_ = clamp_int(static_cast<int>(value), 0, 100);
        // Chorus Level via SysEx Patch Common param (offset 0x12)
        emulator_.SendSysexPatchCommonParam(0x12, PercentToMidi(chorus_));
        break;
      case kParamDelay:
        delay_ = clamp_int(static_cast<int>(value), 0, 100);
        // Delay Feedback via SysEx Patch Common param (offset 0x10)
        emulator_.SendSysexPatchCommonParam(0x10, PercentToMidi(delay_));
        break;
      default:
        break;
    }
  }

  inline int32_t getParameterValue(uint8_t index) const {
    switch (index) {
      case kParamPart:
        return part_;
      case kParamPoly:
        return poly_;
      case kParamLevel:
        return level_;
      case kParamPan:
        return pan_;
      case kParamTone:
        return tone_;
      case kParamCutoff:
        return cutoff_;
      case kParamResonance:
        return resonance_;
      case kParamAttack:
        return attack_;
      case kParamReverb:
        return reverb_;
      case kParamChorus:
        return chorus_;
      case kParamDelay:
        return delay_;
      default:
        break;
    }
    return 0;
  }

  inline const char * getParameterStrValue(uint8_t index, int32_t value) const {
    (void)value;
    switch (index) {
      // Note: String memory must be accessible even after function returned.
      //       It can be assumed that caller will have copied or used the string
      //       before the next call to getParameterStrValue
      case kParamPart:
        std::snprintf(param_str_, sizeof(param_str_), "%d", part_);
        return param_str_;
      case kParamPoly:
        std::snprintf(param_str_, sizeof(param_str_), "%d", poly_);
        return param_str_;
      case kParamPan:
        if (pan_ == 0) {
          std::snprintf(param_str_, sizeof(param_str_), "C");
        } else if (pan_ < 0) {
          std::snprintf(param_str_, sizeof(param_str_), "L%d", -pan_);
        } else {
          std::snprintf(param_str_, sizeof(param_str_), "R%d", pan_);
        }
        return param_str_;
      case kParamTone:
        if (!emulator_.GetPatchName(static_cast<uint8_t>(tone_), param_str_, sizeof(param_str_))) {
          std::snprintf(param_str_, sizeof(param_str_), "P%03d", tone_);
        }
        return param_str_;
      default:
        break;
    }
    return nullptr;
  }

  inline const uint8_t * getParameterBmpValue(uint8_t index,
                                              int32_t value) const {
    (void)value;
    switch (index) {
      // Note: Bitmap memory must be accessible even after function returned.
      //       It can be assumed that caller will have copied or used the bitmap
      //       before the next call to getParameterBmpValue
      // Note: Not yet implemented upstream
      default:
        break;
    }
    return nullptr;
  }

  inline void NoteOn(uint8_t note, uint8_t velocity) {
    if (!initialized_) {
      fprintf(stderr, "[Drumpler] NoteOn called but NOT INITIALIZED!\n");
      return;
    }
    
    // Wake up from idle when note triggered
    is_idle_ = false;
    silence_frames_ = 0;
    
#ifdef DEBUG
    fprintf(stderr, "[Drumpler] synth.h NoteOn: note=%d vel=%d channel=%d\n", note, velocity, channel_);
    fflush(stderr);
#endif
    // Send to emulator on MIDI channel (channel_ is 0-based)
    last_note_ = note;
    last_velocity_ = velocity;
    emulator_.NoteOn(channel_, note, velocity);
  }

  inline void NoteOff(uint8_t note) {
    if (!initialized_) return;
    emulator_.NoteOff(channel_, note);
  }

  inline void GateOn(uint8_t velocity) {
    // For monophonic mode - send note on for middle C
    if (!initialized_) return;
    
    // Wake up from idle
    is_idle_ = false;
    silence_frames_ = 0;
    
    last_velocity_ = velocity;
    emulator_.NoteOn(channel_, last_note_, velocity);
  }

  inline void GateOff() {
    // For monophonic mode - send note off for middle C
    if (!initialized_) return;
    emulator_.NoteOff(channel_, last_note_);
  }

  inline void AllNoteOff() {
    if (!initialized_) return;
    // Send MIDI All Notes Off (CC 123)
    emulator_.ControlChange(channel_, 123, 0);
  }

  inline void PitchBend(uint16_t bend) {
    if (!initialized_) return;
    // TODO: Send MIDI pitch bend to emulator
    (void)bend;
  }

  inline void ChannelPressure(uint8_t pressure) {
    if (!initialized_) return;
    // TODO: Send MIDI channel pressure
    (void)pressure;
  }

  inline void Aftertouch(uint8_t note, uint8_t aftertouch) {
    if (!initialized_) return;
    // TODO: Send MIDI polyphonic aftertouch
    (void)note;
    (void)aftertouch;
  }

  inline void LoadPreset(uint8_t idx) {
    if (!initialized_) return;
    tone_ = clamp_int(static_cast<int>(idx), 0, 127);
    preset_index_ = static_cast<uint8_t>(tone_);
#ifdef DEBUG
    fprintf(stderr, "[Drumpler] LoadPreset: idx=%d → tone=%d ch=%d\n", idx, tone_, channel_);
    fflush(stderr);
#endif
    // JV-880 requires direct nvram patch write, not standard MIDI Program Change
    emulator_.SetCurrentProgram(static_cast<uint8_t>(tone_));
  }

  inline uint8_t getPresetIndex() const { return preset_index_; }

  /*===========================================================================*/
  /* Static Members. */
  /*===========================================================================*/

  inline const char * getPresetName(uint8_t idx) {
    if (initialized_) {
      if (emulator_.GetPatchName(idx, preset_str_, sizeof(preset_str_))) {
        return preset_str_;
      }
    }
    // Fallback if not initialized or GetPatchName fails
    static int warn_once = 0;
    if (!warn_once++) {
      fprintf(stderr, "[Drumpler] getPresetName fallback: initialized=%d\n", initialized_);
    }
    std::snprintf(preset_str_, sizeof(preset_str_), "P%03d", idx);
    return preset_str_;
  }

 private:
  /*===========================================================================*/
  /* Private Member Variables. */
  /*===========================================================================*/

  std::atomic_uint_fast32_t flags_;
  drumpler::JV880Emulator emulator_;
  bool initialized_;

  int part_;
  int poly_;
  int level_;
  int pan_;
  int tone_;
  int cutoff_;
  int resonance_;
  int attack_;
  int reverb_;
  int chorus_;
  int delay_;
  uint8_t preset_index_;
  uint8_t channel_;
  uint8_t last_note_;
  uint8_t last_velocity_;
  mutable char param_str_[16];
  mutable char preset_str_[16];
  
  // Deferred warmup state
  int32_t warmup_remaining_;     // >0: MCU firmware still booting in Render()
  
  // Idle detection to reduce CPU load when silent
  uint32_t silence_frames_;      // Count consecutive silent frames
  bool is_idle_;                 // True when emulator can be skipped
  static constexpr uint32_t kSilenceThreshold = 96000; // 2 seconds @ 48kHz
  static constexpr float kSilenceLevel = 0.0001f;      // -80dB threshold

#ifdef PERF_MON
  uint8_t perf_render_total_ = 0;
  uint8_t perf_emulator_ = 0;
  uint8_t perf_interleave_ = 0;
#endif

  /*===========================================================================*/
  /* Private Methods. */
  /*===========================================================================*/

  inline void SendCc(uint8_t cc, uint8_t value) {
    emulator_.ControlChange(channel_, cc, value);
  }

  inline uint8_t PercentToMidi(int percent) const {
    return static_cast<uint8_t>((percent * 127) / 100);
  }

  inline void ApplyAllParams() {
#ifdef DEBUG
    fprintf(stderr, "[Drumpler] ApplyAllParams: ch=%d tone=%d level=%d\n", channel_, tone_, level_);
    fflush(stderr);
#endif
    // Load patch via direct nvram write (matching JUCE setCurrentProgram)
    emulator_.SetCurrentProgram(static_cast<uint8_t>(tone_));
    // Ensure global reverb and chorus are enabled (System params, matches JUCE SettingsTab)
    emulator_.SendSysexSystemParam(0x04, 1);  // Reverb Switch ON
    emulator_.SendSysexSystemParam(0x05, 1);  // Chorus Switch ON
    // Standard MIDI CCs for volume and pan (JV-880 supports these natively)
    SendCc(7, PercentToMidi(level_));
    SendCc(10, static_cast<uint8_t>(pan_ + 64));
    // SysEx for effects (Patch Common parameters)
    emulator_.SendSysexPatchCommonParam(0x0E, PercentToMidi(reverb_));   // Reverb Level
    emulator_.SendSysexPatchCommonParam(0x12, PercentToMidi(chorus_));   // Chorus Level
    emulator_.SendSysexPatchCommonParam(0x10, PercentToMidi(delay_));    // Delay Feedback
    // SysEx for tone parameters (all 4 tones)
    for (uint8_t t = 0; t < 4; ++t) {
      emulator_.SendSysexPatchToneParam(t, 0x4A, PercentToMidi(cutoff_));     // TVF Cutoff Frequency
      emulator_.SendSysexPatchToneParam(t, 0x4B, PercentToMidi(resonance_));  // TVF Resonance
      emulator_.SendSysexPatchToneParam(t, 0x69, PercentToMidi(attack_));     // TVA Env Time 1
    }
  }

  /*===========================================================================*/
  /* Constants. */
  /*===========================================================================*/
};

#ifdef PERF_MON
namespace {
__attribute__((used)) auto kPerfMonGetCount = &dsp::PerfMon::GetCounterCount;
__attribute__((used)) auto kPerfMonGetName = &dsp::PerfMon::GetCounterName;
__attribute__((used)) auto kPerfMonGetAvg = &dsp::PerfMon::GetAverageCycles;
__attribute__((used)) auto kPerfMonGetPeak = &dsp::PerfMon::GetPeakCycles;
__attribute__((used)) auto kPerfMonGetMin = &dsp::PerfMon::GetMinCycles;
__attribute__((used)) auto kPerfMonGetFrames = &dsp::PerfMon::GetFrameCount;
}  // namespace
#endif

