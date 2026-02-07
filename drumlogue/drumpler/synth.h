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
        channel_(0) {}
  ~Synth(void) {}

  inline int8_t Init(const unit_runtime_desc_t * desc) {
    // Check compatibility of samplerate with unit, for drumlogue should be 48000
    if (desc->samplerate != 48000)
      return k_unit_err_samplerate;

    // Check compatibility of frame geometry
    if (desc->output_channels != 2)  // should be stereo output
      return k_unit_err_geometry;

#ifdef DRUMPLER_ROM_EMBEDDED
    fprintf(stderr, "[Drumpler] Initializing with embedded ROM (%u bytes)\n", g_drumpler_rom_size);
    // Initialize emulator with embedded ROM
    if (!emulator_.Init(g_drumpler_rom, g_drumpler_rom_size)) {
      fprintf(stderr, "[Drumpler] ERROR: Emulator init failed!\n");
      return k_unit_err_memory;  // Failed to initialize emulator
    }
    
    // Warmup: Let emulator boot by rendering silent frames
    // The MCU needs ~2 seconds of cycles to fully initialize
    fprintf(stderr, "[Drumpler] Warming up emulator (this may take a moment)...\n");
    fflush(stderr);
    float warmup_l[128];
    float warmup_r[128];
    const int warmup_iterations = 750;  // ~2 seconds @ 48kHz (750 * 128 frames)
    
    // Send a test note halfway through warmup to prime the audio engine
    bool found_audio = false;
    for (int i = 0; i < warmup_iterations; ++i) {
      if (i == warmup_iterations / 2) {
        fprintf(stderr, "[Drumpler] Sending test note to prime audio engine...\n");
        fflush(stderr);
        emulator_.NoteOn(0, 60, 100);   // Test note on channel 0
      }
      
      emulator_.Render(warmup_l, warmup_r, 128);
      
      // Check for audio 5 frames after test note
      if (i == (warmup_iterations / 2) + 5 && !found_audio) {
        float max_val = 0.0f;
        for (int j = 0; j < 128; ++j) {
          float abs_l = fabsf(warmup_l[j]);
          float abs_r = fabsf(warmup_r[j]);
          if (abs_l > max_val) max_val = abs_l;
          if (abs_r > max_val) max_val = abs_r;
        }
        fprintf(stderr, "[Drumpler] Audio check (5 frames after note): max=%f\n", max_val);
        fflush(stderr);
        if (max_val > 0.0001f) {
          found_audio = true;
          fprintf(stderr, "[Drumpler] SUCCESS: Audio engine is producing sound!\n");
          fflush(stderr);
        }
      }
      
      if (i == (warmup_iterations / 2) + 10) {
        emulator_.NoteOff(0, 60);       // Release after 10 frames
      }
    }
    fprintf(stderr, "[Drumpler] Warmup complete\n");
    fflush(stderr);
    
    initialized_ = true;
    fprintf(stderr, "[Drumpler] Emulator initialized successfully\n");
    ApplyAllParams();
#else
    fprintf(stderr, "[Drumpler] ERROR: DRUMPLER_ROM_EMBEDDED not defined!\n");
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
      static int warn_count = 0;
      if (warn_count++ < 5) {
        fprintf(stderr, "[Drumpler] Render: outputting silence (initialized=%d, frames=%zu)\n", initialized_, frames);
      }
      float * __restrict out_p = out;
      const float * out_e = out_p + (frames << 1);  // assuming stereo output
      for (; out_p != out_e; out_p += 2) {
        out_p[0] = 0.0f;
        out_p[1] = 0.0f;
      }
      return;
    }

    // Render emulator output (internally resampled from 64kHz to 48kHz)
    // Output format: interleaved stereo [L0, R0, L1, R1, ...]
    float * __restrict out_p = out;
    
    // Create temp buffers for deinterleaved output (emulator expects separate L/R)
    static float temp_l[128];  // Max frames per callback is typically 32-64
    static float temp_r[128];
    
    const size_t render_frames = (frames > 128) ? 128 : frames;
    
#ifdef DEBUG
    static int render_debug_count = 0;
    if (render_debug_count++ < 3) {
      fprintf(stderr, "[Drumpler] synth.h Render: calling emulator_.Render() with %zu frames\n", render_frames);
      fflush(stderr);
    }
#endif
    
    emulator_.Render(temp_l, temp_r, render_frames);
    
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
    for (size_t i = 0; i < render_frames; ++i) {
      out_p[i * 2 + 0] = temp_l[i];
      out_p[i * 2 + 1] = temp_r[i];
    }
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
        emulator_.ProgramChange(channel_, static_cast<uint8_t>(tone_));
        break;
      case kParamCutoff:
        cutoff_ = clamp_int(static_cast<int>(value), 0, 100);
        SendCc(74, PercentToMidi(cutoff_));
        break;
      case kParamResonance:
        resonance_ = clamp_int(static_cast<int>(value), 0, 100);
        SendCc(71, PercentToMidi(resonance_));
        break;
      case kParamAttack:
        attack_ = clamp_int(static_cast<int>(value), 0, 100);
        SendCc(73, PercentToMidi(attack_));
        break;
      case kParamReverb:
        reverb_ = clamp_int(static_cast<int>(value), 0, 100);
        SendCc(91, PercentToMidi(reverb_));
        break;
      case kParamChorus:
        chorus_ = clamp_int(static_cast<int>(value), 0, 100);
        SendCc(93, PercentToMidi(chorus_));
        break;
      case kParamDelay:
        delay_ = clamp_int(static_cast<int>(value), 0, 100);
        SendCc(94, PercentToMidi(delay_));
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
#ifdef DEBUG
    fprintf(stderr, "[Drumpler] synth.h NoteOn: note=%d vel=%d channel=%d\n", note, velocity, channel_);
    fflush(stderr);
#else
    fprintf(stderr, "[Drumpler] NoteOn: note=%d vel=%d\n", note, velocity);
#endif
    // Send to emulator on MIDI channel (channel_ is 0-based)
    emulator_.NoteOn(channel_, note, velocity);
  }

  inline void NoteOff(uint8_t note) {
    if (!initialized_) return;
    emulator_.NoteOff(0, note);
  }

  inline void GateOn(uint8_t velocity) {
    // For monophonic mode - send note on for middle C
    if (!initialized_) return;
    emulator_.NoteOn(0, 60, velocity);
  }

  inline void GateOff() {
    // For monophonic mode - send note off for middle C
    if (!initialized_) return;
    emulator_.NoteOff(0, 60);
  }

  inline void AllNoteOff() {
    if (!initialized_) return;
    // Send MIDI All Notes Off (CC 123)
    emulator_.ControlChange(0, 123, 0);
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
    emulator_.ProgramChange(channel_, static_cast<uint8_t>(tone_));
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
  mutable char param_str_[16];
  mutable char preset_str_[16];

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
    emulator_.ProgramChange(channel_, static_cast<uint8_t>(tone_));
    SendCc(7, PercentToMidi(level_));
    SendCc(10, static_cast<uint8_t>(pan_ + 64));
    SendCc(74, PercentToMidi(cutoff_));
    SendCc(71, PercentToMidi(resonance_));
    SendCc(73, PercentToMidi(attack_));
    SendCc(91, PercentToMidi(reverb_));
    SendCc(93, PercentToMidi(chorus_));
    SendCc(94, PercentToMidi(delay_));
  }

  /*===========================================================================*/
  /* Constants. */
  /*===========================================================================*/
};

