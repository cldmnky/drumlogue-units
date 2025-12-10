/**
 * @file pepege_synth_mono.h
 * @brief Main synth wrapper for Pepege wavetable synthesizer
 *
 * Coordinates oscillators, envelopes, filter, and LFO.
 * Uses PPG Wave style oscillators for authentic 8-bit character.
 */

#pragma once

#include <cstdint>
#include <cmath>

#include "unit.h"
#include "wavetable_osc.h"
#include "envelope.h"
#include "filter.h"
#include "lfo.h"
#include "resources/ppg_waves.h"
#include "../common/stereo_widener.h"

// NEON SIMD optimizations (Cortex-A7)
#ifdef USE_NEON
#include <arm_neon.h>
#endif

// Common DSP utilities (includes NEON helpers when USE_NEON defined)
#define NEON_DSP_NS pepege
#include "../common/neon_dsp.h"

// Parameter indices (matching header.c order)
enum PepegeParams {
    // Page 1: Oscillator A
    P_OSC_A_BANK = 0,
    P_OSC_A_MORPH,
    P_OSC_A_OCT,
    P_OSC_A_TUNE,
    
    // Page 2: Oscillator B
    P_OSC_B_BANK,
    P_OSC_B_MORPH,
    P_OSC_B_OCT,
    P_OSC_MODE,      // PPG interpolation mode (0=HiFi, 1=LoFi, 2=Raw)
    
    // Page 3: Filter
    P_FILTER_CUTOFF,
    P_FILTER_RESO,
    P_FILTER_ENV,
    P_FILTER_TYPE,
    
    // Page 4: Amp Envelope
    P_AMP_ATTACK,
    P_AMP_DECAY,
    P_AMP_SUSTAIN,
    P_AMP_RELEASE,
    
    // Page 5: Filter Envelope
    P_FILT_ATTACK,
    P_FILT_DECAY,
    P_FILT_SUSTAIN,
    P_FILT_RELEASE,
    
    // Page 6: Modulation & Output
    P_LFO_RATE,
    P_LFO_TO_MORPH,
    P_OSC_MIX,
    P_SPACE,         // Stereo width (0=mono, 64=normal, 127=wide)
    
    P_NUM_PARAMS
};

// Filter type names
static const char* s_filter_names[] = {
    "LP12",
    "LP24",
    "HP12",
    "BP12"
};

// PPG Bank names for display
static const char* ppg_bank_names[] = {
    "UPPER_WT",   // 0:  waves 0-15
    "RESONANT1",  // 1:  waves 16-31
    "RESONANT2",  // 2:  waves 32-47
    "MELLOW",     // 3:  waves 48-63
    "BRIGHT",     // 4:  waves 64-79
    "HARSH",      // 5:  waves 80-95
    "CLIPPER",    // 6:  waves 96-111
    "SYNC",       // 7:  waves 112-127
    "PWM",        // 8:  waves 128-143
    "VOCAL1",     // 9:  waves 144-159
    "VOCAL2",     // 10: waves 160-175
    "ORGAN",      // 11: waves 176-191
    "BELL",       // 12: waves 192-207
    "ALIEN",      // 13: waves 208-223
    "NOISE",      // 14: waves 224-239
    "SPECIAL"     // 15: waves 240-255
};

// PPG oscillator mode names
static const char* ppg_mode_names[] = {
    "HiFi",   // INTERP_2D - bilinear interpolation (smoothest)
    "LoFi",   // INTERP_1D - sample interpolation only (stepped waves)
    "Raw"     // NO_INTERP - no interpolation (authentic PPG crunch)
};

// Maximum frames we'll ever process at once (drumlogue uses 64 typically)
static constexpr uint32_t kMaxFrames = 64;

class PepegeSynth {
public:
    PepegeSynth() {}
    ~PepegeSynth() {}
    
    int8_t Init(const unit_runtime_desc_t* desc) {
        sample_rate_ = static_cast<float>(desc->samplerate);
        inv_sample_rate_ = 1.0f / sample_rate_;
        
        // Initialize full PPG oscillators
        osc_a_.Init(sample_rate_);
        osc_b_.Init(sample_rate_);
        
        // Initialize bank tracking (force reload on first render)
        current_bank_a_ = -1;
        current_bank_b_ = -1;
        
        // Initialize other components
        amp_env_.Init(sample_rate_);
        filter_env_.Init(sample_rate_);
        filter_.Init(sample_rate_);
        lfo_.Init(sample_rate_);
        
        // Initialize state
        note_ = 60;
        velocity_ = 0.0f;
        gate_ = false;
        pitch_bend_ = 0.0f;
        pressure_ = 0.0f;
        
        // Clear intermediate buffer
        pepege::neon::ClearBuffer(mono_buffer_, kMaxFrames);
        
        // Default parameters
        for (int i = 0; i < P_NUM_PARAMS; i++) {
            params_[i] = 0;
        }
        
        // Set some sensible defaults (matching header.c)
        params_[P_OSC_MODE] = 2;  // Raw PPG mode by default
        params_[P_OSC_MIX] = 64;  // 50% mix
        params_[P_FILTER_CUTOFF] = 127;
        params_[P_AMP_ATTACK] = 5;
        params_[P_AMP_DECAY] = 40;
        params_[P_AMP_SUSTAIN] = 80;
        params_[P_AMP_RELEASE] = 30;
        params_[P_FILT_ATTACK] = 10;
        params_[P_FILT_DECAY] = 50;
        params_[P_FILT_SUSTAIN] = 40;
        params_[P_FILT_RELEASE] = 40;
        params_[P_FILTER_ENV] = 32;  // Center point for bipolar
        params_[P_LFO_RATE] = 40;
        params_[P_SPACE] = 64;  // Default to normal stereo spread
        
        preset_idx_ = 0;
        
        return k_unit_err_none;
    }
    
    void Teardown() {}
    
    void Reset() {
        osc_a_.Reset();
        osc_b_.Reset();
        amp_env_.Reset();
        filter_env_.Reset();
        filter_.Reset();
        lfo_.Reset();
    }
    
    void Resume() {}
    void Suspend() {}
    
    void Render(float* out, uint32_t frames) {
        // Limit frames to our buffer size
        if (frames > kMaxFrames) frames = kMaxFrames;
        
        // Calculate derived parameters
        const float osc_a_morph = params_[P_OSC_A_MORPH] / 127.0f;
        const float osc_b_morph = params_[P_OSC_B_MORPH] / 127.0f;
        const float osc_mix = params_[P_OSC_MIX] / 127.0f;
        
        const float cutoff_base = params_[P_FILTER_CUTOFF] / 127.0f;
        const float resonance = params_[P_FILTER_RESO] / 127.0f;
        // P_FILTER_ENV is bipolar (-64 to 63), normalize to -1..+1
        const float filter_env_amt = params_[P_FILTER_ENV] / 64.0f;
        
        // P_LFO_TO_MORPH is bipolar (-64 to 63), normalize to -1..+1
        const float lfo_to_morph = params_[P_LFO_TO_MORPH] / 64.0f;
        
        // Calculate base frequencies for both oscillators
        // P_OSC_A_OCT is -3 to +3
        const float osc_a_octave = static_cast<float>(params_[P_OSC_A_OCT]);
        // P_OSC_A_TUNE is -64 to +63 cents
        const float osc_a_tune = params_[P_OSC_A_TUNE] / 100.0f;  // Cents to semitones
        // P_OSC_B_OCT is -3 to +3
        const float osc_b_octave = static_cast<float>(params_[P_OSC_B_OCT]);
        
        // MIDI note to frequency with pitch bend
        const float base_note = static_cast<float>(note_) + pitch_bend_ * 2.0f;  // ±2 semitone bend range
        
        const float freq_a_base = 440.0f * powf(2.0f, (base_note - 69.0f + osc_a_octave * 12.0f + osc_a_tune) / 12.0f);
        const float freq_b_base = 440.0f * powf(2.0f, (base_note - 69.0f + osc_b_octave * 12.0f) / 12.0f);
        
        // Get current banks
        const int bank_a = params_[P_OSC_A_BANK];
        const int bank_b = params_[P_OSC_B_BANK];
        
        // Check if we need to reload wavetables (bank changed)
        if (bank_a != current_bank_a_) {
            LoadBankWavetable(osc_a_, bank_a);
            current_bank_a_ = bank_a;
        }
        if (bank_b != current_bank_b_) {
            LoadBankWavetable(osc_b_, bank_b);
            current_bank_b_ = bank_b;
        }
        
        // Set PPG oscillator mode (0=HiFi/INTERP_2D, 1=LoFi/INTERP_1D, 2=Raw/NO_INTERP)
        const auto ppg_mode = static_cast<common::PpgMode>(params_[P_OSC_MODE]);
        osc_a_.SetMode(ppg_mode);
        osc_b_.SetMode(ppg_mode);
        
        // Update envelope parameters
        amp_env_.SetAttack(params_[P_AMP_ATTACK]);
        amp_env_.SetDecay(params_[P_AMP_DECAY]);
        amp_env_.SetSustain(params_[P_AMP_SUSTAIN] / 127.0f);
        amp_env_.SetRelease(params_[P_AMP_RELEASE]);
        
        filter_env_.SetAttack(params_[P_FILT_ATTACK]);
        filter_env_.SetDecay(params_[P_FILT_DECAY]);
        filter_env_.SetSustain(params_[P_FILT_SUSTAIN] / 127.0f);
        filter_env_.SetRelease(params_[P_FILT_RELEASE]);
        
        // Update LFO rate
        lfo_.SetRate(params_[P_LFO_RATE]);
        
        // Process each sample into mono buffer
        for (uint32_t i = 0; i < frames; i++) {
            // Get LFO value (-1 to +1)
            const float lfo_val = lfo_.Process();
            
            // Apply LFO to morph with clamping
            float morph_a = osc_a_morph + lfo_val * lfo_to_morph * 0.5f;
            float morph_b = osc_b_morph + lfo_val * lfo_to_morph * 0.5f;
            morph_a = fminf(1.0f, fmaxf(0.0f, morph_a));
            morph_b = fminf(1.0f, fmaxf(0.0f, morph_b));
            
            // Set oscillator frequencies and wave positions
            osc_a_.SetFrequency(freq_a_base);
            osc_a_.SetWavePosition(morph_a);
            
            osc_b_.SetFrequency(freq_b_base);
            osc_b_.SetWavePosition(morph_b);
            
            // Render oscillators using full PPG processing
            float osc_out_a = osc_a_.Process();
            float osc_out_b = osc_b_.Process();
            
            // Mix oscillators
            float osc_out = osc_out_a * (1.0f - osc_mix) + osc_out_b * osc_mix;
            
            // Get envelope values
            const float amp_env_val = amp_env_.Process(gate_);
            const float filt_env_val = filter_env_.Process(gate_);
            
            // Calculate filter cutoff with envelope
            float cutoff = cutoff_base + filt_env_val * filter_env_amt;
            cutoff = fminf(1.0f, fmaxf(0.0f, cutoff));
            
            // Apply filter
            filter_.SetCutoff(cutoff);
            filter_.SetResonance(resonance);
            filter_.SetType(static_cast<FilterType>(params_[P_FILTER_TYPE]));
            float filtered = filter_.Process(osc_out);
            
            // Store in mono buffer (apply amp envelope here)
            mono_buffer_[i] = filtered * amp_env_val;
        }
        
        // === NEON-optimized output stage ===
        
        // Apply velocity gain using NEON
        pepege::neon::ApplyGain(mono_buffer_, velocity_, frames);
        
        // Sanitize (remove NaN/Inf) and soft clamp to prevent harsh clipping
        pepege::neon::SanitizeAndClamp(mono_buffer_, 1.0f, frames);
        
        // Apply stereo widening
        // SPACE: 0=mono, 64=normal stereo, 127=wide
        // Map 0-127 to width 0.0-1.5 (64 -> 0.75 = slight spread)
        const float space = params_[P_SPACE] / 127.0f * 1.5f;
        stereo_widener_.SetWidth(space);
        
        // Create stereo output from mono with width
        // Using separate left/right buffers then interleaving
        float left_buf[kMaxFrames] __attribute__((aligned(16)));
        float right_buf[kMaxFrames] __attribute__((aligned(16)));
        
        // For each sample, create stereo spread using oscillator detune as "side" signal
        // Since we're mono, we create pseudo-stereo by using slight phase/amplitude differences
        // The widener applies M/S processing: wider settings push more to sides
#ifdef USE_NEON
        // NEON batch processing
        const float32x4_t width_vec = vdupq_n_f32(space);
        const float32x4_t half = vdupq_n_f32(0.5f);
        const float32x4_t one = vdupq_n_f32(1.0f);
        
        size_t i = 0;
        for (; i + 4 <= frames; i += 4) {
            float32x4_t mono = vld1q_f32(&mono_buffer_[i]);
            // Create pseudo-side signal from mono using subtle variation
            // side = mono * (width * 0.3) creates stereo field
            float32x4_t side_amt = vmulq_f32(width_vec, vdupq_n_f32(0.3f));
            float32x4_t left = vmulq_f32(mono, vaddq_f32(one, side_amt));
            float32x4_t right = vmulq_f32(mono, vsubq_f32(one, side_amt));
            // Normalize to avoid clipping: divide by (1 + side_amt)
            float32x4_t norm = vrecpeq_f32(vaddq_f32(one, vmulq_f32(side_amt, half)));
            norm = vmulq_f32(vrecpsq_f32(vaddq_f32(one, vmulq_f32(side_amt, half)), norm), norm);
            left = vmulq_f32(left, norm);
            right = vmulq_f32(right, norm);
            vst1q_f32(&left_buf[i], left);
            vst1q_f32(&right_buf[i], right);
        }
        // Scalar tail
        for (; i < frames; ++i) {
            float side_amt = space * 0.3f;
            float norm = 1.0f / (1.0f + side_amt * 0.5f);
            left_buf[i] = mono_buffer_[i] * (1.0f + side_amt) * norm;
            right_buf[i] = mono_buffer_[i] * (1.0f - side_amt) * norm;
        }
#else
        // Scalar fallback
        for (uint32_t i = 0; i < frames; ++i) {
            float side_amt = space * 0.3f;
            float norm = 1.0f / (1.0f + side_amt * 0.5f);
            left_buf[i] = mono_buffer_[i] * (1.0f + side_amt) * norm;
            right_buf[i] = mono_buffer_[i] * (1.0f - side_amt) * norm;
        }
#endif
        
        // Interleave to stereo output using NEON
        pepege::neon::InterleaveStereo(left_buf, right_buf, out, frames);
    }
    
    void SetParameter(uint8_t id, int32_t value) {
        if (id < P_NUM_PARAMS) {
            params_[id] = value;
        }
    }
    
    int32_t GetParameter(uint8_t id) const {
        if (id < P_NUM_PARAMS) {
            return params_[id];
        }
        return 0;
    }
    
    const char* GetParameterStr(uint8_t id, int32_t value) const {
        switch (id) {
            case P_OSC_A_BANK:
            case P_OSC_B_BANK:
                if (value >= 0 && value < PPG_NUM_BANKS) {
                    return ppg_bank_names[value];
                }
                break;
            case P_OSC_MODE:
                if (value >= 0 && value < 3) {
                    return ppg_mode_names[value];
                }
                break;
            case P_FILTER_TYPE:
                if (value >= 0 && value < 4) {
                    return s_filter_names[value];
                }
                break;
        }
        return nullptr;
    }
    
    void SetTempo(uint32_t tempo) {
        tempo_ = tempo;
        // Could use for tempo-synced LFO
    }
    
    void NoteOn(uint8_t note, uint8_t velocity) {
        note_ = note;
        velocity_ = velocity / 127.0f;
        gate_ = true;
        amp_env_.Gate(true);
        filter_env_.Gate(true);
    }
    
    void NoteOff(uint8_t note) {
        if (note == note_) {
            gate_ = false;
            amp_env_.Gate(false);
            filter_env_.Gate(false);
        }
    }
    
    void GateOn(uint8_t velocity) {
        velocity_ = velocity / 127.0f;
        gate_ = true;
        amp_env_.Gate(true);
        filter_env_.Gate(true);
    }
    
    void GateOff() {
        gate_ = false;
        amp_env_.Gate(false);
        filter_env_.Gate(false);
    }
    
    void AllNoteOff() {
        gate_ = false;
        amp_env_.Gate(false);
        filter_env_.Gate(false);
        amp_env_.Reset();
        filter_env_.Reset();
    }
    
    void PitchBend(uint16_t bend) {
        // Convert 14-bit value (0-16383) to -1..+1
        pitch_bend_ = (static_cast<float>(bend) - 8192.0f) / 8192.0f;
    }
    
    void ChannelPressure(uint8_t pressure) {
        pressure_ = pressure / 127.0f;
    }
    
    void Aftertouch(uint8_t note, uint8_t value) {
        (void)note;
        (void)value;
        // Could modulate per-note
    }
    
    void LoadPreset(uint8_t idx) {
        preset_idx_ = idx;
        // TODO: Load preset data
    }
    
    uint8_t GetPresetIndex() const {
        return preset_idx_;
    }
    
    const uint8_t* GetPresetData(uint8_t idx) const {
        (void)idx;
        // TODO: Return preset data
        return nullptr;
    }
    
private:
    /**
     * @brief Load a bank's wavetable into a PPG oscillator
     *
     * Creates the wavetable definition array and loads it into the oscillator.
     * Each bank uses 8 evenly-spaced waves from the 16-wave PPG bank.
     */
    void LoadBankWavetable(common::PpgOsc<PPG_WAVES_PER_BANK>& osc, int bank) {
        if (bank < 0 || bank >= PPG_NUM_BANKS) return;
        
        // Build wavetable definition: [wave_idx, position, ..., 0xFF]
        // Format: pairs of (wave_index, wavetable_position), terminated by 0xFF
        // We map 8 PPG waves to positions 0-7 in our 8-position wavetable
        uint8_t wavetable_def[PPG_WAVES_PER_BANK * 2 + 1];
        
        for (int i = 0; i < PPG_WAVES_PER_BANK; i++) {
            // Wave index in the full 256-wave array (bank*16 + i*2 for spacing)
            wavetable_def[i * 2] = static_cast<uint8_t>(bank * 16 + i * 2);
            // Position in wavetable (0-7)
            wavetable_def[i * 2 + 1] = static_cast<uint8_t>(i);
        }
        wavetable_def[PPG_WAVES_PER_BANK * 2] = 0xFF;  // Terminator
        
        osc.LoadWavetable(ppg_wave_data, wavetable_def);
    }
    
    float sample_rate_;
    float inv_sample_rate_;
    
    // Full PPG Wave oscillators with mode selection
    common::PpgOsc<PPG_WAVES_PER_BANK> osc_a_;
    common::PpgOsc<PPG_WAVES_PER_BANK> osc_b_;
    
    // Track current bank to reload wavetable on change
    int current_bank_a_;
    int current_bank_b_;
    
    // Modulation
    ADSREnvelope amp_env_;
    ADSREnvelope filter_env_;
    SVFilter filter_;
    LFO lfo_;
    common::StereoWidener stereo_widener_;
    
    // NEON-aligned intermediate buffer for batch processing
    float mono_buffer_[kMaxFrames] __attribute__((aligned(16)));
    
    // State
    uint8_t note_;
    float velocity_;
    bool gate_;
    float pitch_bend_;
    float pressure_;
    uint32_t tempo_;
    
    // Parameters
    int32_t params_[P_NUM_PARAMS];
    uint8_t preset_idx_;
};

