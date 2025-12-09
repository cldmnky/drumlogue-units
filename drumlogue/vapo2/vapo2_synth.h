/**
 * @file vapo2_synth.h
 * @brief 4-Voice Polyphonic Vapo2 wavetable synthesizer
 *
 * Coordinates oscillators, envelopes, filter, and LFO across 4 voices.
 * Uses PPG Wave style oscillators for authentic 8-bit character.
 * 
 * Voice allocation: Round-robin with oldest-note stealing when all voices busy.
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
#define NEON_DSP_NS vapo2
#include "../common/neon_dsp.h"

// Number of polyphonic voices
static constexpr uint32_t kNumVoices = 4;

// Parameter indices (matching header.c order)
enum Vapo2Params {
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

/**
 * @brief Single synthesizer voice
 * 
 * Contains all per-voice state: oscillators, envelopes, filter.
 * Parameters are shared globally but modulation is per-voice.
 */
struct Voice {
    // Oscillators
    common::PpgOsc<PPG_WAVES_PER_BANK> osc_a;
    common::PpgOsc<PPG_WAVES_PER_BANK> osc_b;
    
    // Per-voice envelopes
    ADSREnvelope amp_env;
    ADSREnvelope filter_env;
    
    // Per-voice filter (for key tracking and independent state)
    SVFilter filter;
    
    // Voice state
    uint8_t note;
    float velocity;
    bool gate;
    uint32_t age;  // For voice stealing (higher = older)
    
    void Init(float sample_rate) {
        osc_a.Init(sample_rate);
        osc_b.Init(sample_rate);
        amp_env.Init(sample_rate);
        filter_env.Init(sample_rate);
        filter.Init(sample_rate);
        
        note = 0;
        velocity = 0.0f;
        gate = false;
        age = 0;
    }
    
    void Reset() {
        osc_a.Reset();
        osc_b.Reset();
        amp_env.Reset();
        filter_env.Reset();
        filter.Reset();
        gate = false;
        age = 0;
    }
    
    bool IsActive() const {
        return gate || amp_env.IsActive();
    }
};

class Vapo2Synth {
public:
    Vapo2Synth() {}
    ~Vapo2Synth() {}
    
    int8_t Init(const unit_runtime_desc_t* desc) {
        sample_rate_ = static_cast<float>(desc->samplerate);
        inv_sample_rate_ = 1.0f / sample_rate_;
        
        // Initialize all voices
        for (uint32_t v = 0; v < kNumVoices; v++) {
            voices_[v].Init(sample_rate_);
        }
        
        // Initialize bank tracking (force reload on first render)
        current_bank_a_ = -1;
        current_bank_b_ = -1;
        
        // Initialize global LFO
        lfo_.Init(sample_rate_);
        
        // Initialize state
        pitch_bend_ = 0.0f;
        pressure_ = 0.0f;
        voice_counter_ = 0;
        
        // Clear intermediate buffers
        vapo2::neon::ClearBuffer(mix_buffer_, kMaxFrames);
        
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
        for (uint32_t v = 0; v < kNumVoices; v++) {
            voices_[v].Reset();
        }
        lfo_.Reset();
        voice_counter_ = 0;
    }
    
    void Resume() {}
    void Suspend() {}
    
    void Render(float* out, uint32_t frames) {
        // Limit frames to our buffer size
        if (frames > kMaxFrames) frames = kMaxFrames;
        
        // Clear mix buffer
        vapo2::neon::ClearBuffer(mix_buffer_, frames);
        
        // Calculate derived parameters (global)
        const float osc_a_morph = params_[P_OSC_A_MORPH] / 127.0f;
        const float osc_b_morph = params_[P_OSC_B_MORPH] / 127.0f;
        const float osc_mix = params_[P_OSC_MIX] / 127.0f;
        
        const float cutoff_base = params_[P_FILTER_CUTOFF] / 127.0f;
        const float resonance = params_[P_FILTER_RESO] / 127.0f;
        const float filter_env_amt = params_[P_FILTER_ENV] / 64.0f;
        const float lfo_to_morph = params_[P_LFO_TO_MORPH] / 64.0f;
        
        const float osc_a_octave = static_cast<float>(params_[P_OSC_A_OCT]);
        const float osc_a_tune = params_[P_OSC_A_TUNE] / 100.0f;
        const float osc_b_octave = static_cast<float>(params_[P_OSC_B_OCT]);
        
        // Get current banks and check for reload
        const int bank_a = params_[P_OSC_A_BANK];
        const int bank_b = params_[P_OSC_B_BANK];
        
        if (bank_a != current_bank_a_) {
            // Reload wavetable for all voices
            for (uint32_t v = 0; v < kNumVoices; v++) {
                LoadBankWavetable(voices_[v].osc_a, bank_a);
            }
            current_bank_a_ = bank_a;
        }
        if (bank_b != current_bank_b_) {
            for (uint32_t v = 0; v < kNumVoices; v++) {
                LoadBankWavetable(voices_[v].osc_b, bank_b);
            }
            current_bank_b_ = bank_b;
        }
        
        // Set PPG oscillator mode for all voices
        const auto ppg_mode = static_cast<common::PpgMode>(params_[P_OSC_MODE]);
        for (uint32_t v = 0; v < kNumVoices; v++) {
            voices_[v].osc_a.SetMode(ppg_mode);
            voices_[v].osc_b.SetMode(ppg_mode);
        }
        
        // Update envelope parameters for all voices
        for (uint32_t v = 0; v < kNumVoices; v++) {
            voices_[v].amp_env.SetAttack(params_[P_AMP_ATTACK]);
            voices_[v].amp_env.SetDecay(params_[P_AMP_DECAY]);
            voices_[v].amp_env.SetSustain(params_[P_AMP_SUSTAIN] / 127.0f);
            voices_[v].amp_env.SetRelease(params_[P_AMP_RELEASE]);
            
            voices_[v].filter_env.SetAttack(params_[P_FILT_ATTACK]);
            voices_[v].filter_env.SetDecay(params_[P_FILT_DECAY]);
            voices_[v].filter_env.SetSustain(params_[P_FILT_SUSTAIN] / 127.0f);
            voices_[v].filter_env.SetRelease(params_[P_FILT_RELEASE]);
        }
        
        // Update LFO rate
        lfo_.SetRate(params_[P_LFO_RATE]);
        
        // Process each sample
        for (uint32_t i = 0; i < frames; i++) {
            // Get global LFO value (-1 to +1)
            const float lfo_val = lfo_.Process();
            
            // Apply LFO to morph with clamping
            float morph_a = osc_a_morph + lfo_val * lfo_to_morph * 0.5f;
            float morph_b = osc_b_morph + lfo_val * lfo_to_morph * 0.5f;
            morph_a = fminf(1.0f, fmaxf(0.0f, morph_a));
            morph_b = fminf(1.0f, fmaxf(0.0f, morph_b));
            
            float sample_sum = 0.0f;
            
            // Process all active voices
            for (uint32_t v = 0; v < kNumVoices; v++) {
                Voice& voice = voices_[v];
                
                // Skip completely inactive voices
                if (!voice.IsActive()) continue;
                
                // Increment age for voice stealing
                voice.age++;
                
                // Calculate frequency for this voice
                const float base_note = static_cast<float>(voice.note) + pitch_bend_ * 2.0f;
                const float freq_a = 440.0f * powf(2.0f, (base_note - 69.0f + osc_a_octave * 12.0f + osc_a_tune) / 12.0f);
                const float freq_b = 440.0f * powf(2.0f, (base_note - 69.0f + osc_b_octave * 12.0f) / 12.0f);
                
                // Set oscillator frequencies and wave positions
                voice.osc_a.SetFrequency(freq_a);
                voice.osc_a.SetWavePosition(morph_a);
                voice.osc_b.SetFrequency(freq_b);
                voice.osc_b.SetWavePosition(morph_b);
                
                // Render oscillators
                float osc_out_a = voice.osc_a.Process();
                float osc_out_b = voice.osc_b.Process();
                
                // Mix oscillators
                float osc_out = osc_out_a * (1.0f - osc_mix) + osc_out_b * osc_mix;
                
                // Get envelope values
                const float amp_env_val = voice.amp_env.Process(voice.gate);
                const float filt_env_val = voice.filter_env.Process(voice.gate);
                
                // Calculate filter cutoff with envelope
                float cutoff = cutoff_base + filt_env_val * filter_env_amt;
                cutoff = fminf(1.0f, fmaxf(0.0f, cutoff));
                
                // Apply filter
                voice.filter.SetCutoff(cutoff);
                voice.filter.SetResonance(resonance);
                voice.filter.SetType(static_cast<FilterType>(params_[P_FILTER_TYPE]));
                float filtered = voice.filter.Process(osc_out);
                
                // Apply envelope and velocity, add to mix
                sample_sum += filtered * amp_env_val * voice.velocity;
            }
            
            // Store mixed sample (scale by 1/voices for headroom)
            mix_buffer_[i] = sample_sum * 0.5f;  // -6dB headroom for 4 voices
        }
        
        // === NEON-optimized output stage ===
        
        // Sanitize (remove NaN/Inf) and soft clamp
        vapo2::neon::SanitizeAndClamp(mix_buffer_, 1.0f, frames);
        
        // Apply stereo widening
        const float space = params_[P_SPACE] / 127.0f * 1.5f;
        stereo_widener_.SetWidth(space);
        
        // Create stereo output from mono with width
        float left_buf[kMaxFrames] __attribute__((aligned(16)));
        float right_buf[kMaxFrames] __attribute__((aligned(16)));
        
#ifdef USE_NEON
        // NEON batch processing
        const float32x4_t width_vec = vdupq_n_f32(space);
        const float32x4_t half = vdupq_n_f32(0.5f);
        const float32x4_t one = vdupq_n_f32(1.0f);
        
        size_t idx = 0;
        for (; idx + 4 <= frames; idx += 4) {
            float32x4_t mono = vld1q_f32(&mix_buffer_[idx]);
            float32x4_t side_amt = vmulq_f32(width_vec, vdupq_n_f32(0.3f));
            float32x4_t left = vmulq_f32(mono, vaddq_f32(one, side_amt));
            float32x4_t right = vmulq_f32(mono, vsubq_f32(one, side_amt));
            float32x4_t norm = vrecpeq_f32(vaddq_f32(one, vmulq_f32(side_amt, half)));
            norm = vmulq_f32(vrecpsq_f32(vaddq_f32(one, vmulq_f32(side_amt, half)), norm), norm);
            left = vmulq_f32(left, norm);
            right = vmulq_f32(right, norm);
            vst1q_f32(&left_buf[idx], left);
            vst1q_f32(&right_buf[idx], right);
        }
        for (; idx < frames; ++idx) {
            float side_amt = space * 0.3f;
            float norm = 1.0f / (1.0f + side_amt * 0.5f);
            left_buf[idx] = mix_buffer_[idx] * (1.0f + side_amt) * norm;
            right_buf[idx] = mix_buffer_[idx] * (1.0f - side_amt) * norm;
        }
#else
        for (uint32_t idx = 0; idx < frames; ++idx) {
            float side_amt = space * 0.3f;
            float norm = 1.0f / (1.0f + side_amt * 0.5f);
            left_buf[idx] = mix_buffer_[idx] * (1.0f + side_amt) * norm;
            right_buf[idx] = mix_buffer_[idx] * (1.0f - side_amt) * norm;
        }
#endif
        
        // Interleave to stereo output
        vapo2::neon::InterleaveStereo(left_buf, right_buf, out, frames);
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
    }
    
    /**
     * @brief Allocate a voice for a new note
     * 
     * Strategy:
     * 1. Find a free (inactive) voice
     * 2. If none free, steal the oldest voice (highest age)
     */
    int AllocateVoice(uint8_t note) {
        int free_voice = -1;
        int oldest_voice = 0;
        uint32_t oldest_age = 0;
        
        for (uint32_t v = 0; v < kNumVoices; v++) {
            // Check if this voice is playing the same note (retrigger)
            if (voices_[v].note == note && voices_[v].gate) {
                return v;
            }
            
            // Track free voices
            if (!voices_[v].IsActive() && free_voice < 0) {
                free_voice = v;
            }
            
            // Track oldest voice for stealing
            if (voices_[v].age > oldest_age) {
                oldest_age = voices_[v].age;
                oldest_voice = v;
            }
        }
        
        // Prefer free voice, otherwise steal oldest
        return (free_voice >= 0) ? free_voice : oldest_voice;
    }
    
    void NoteOn(uint8_t note, uint8_t velocity) {
        int v = AllocateVoice(note);
        
        Voice& voice = voices_[v];
        voice.note = note;
        voice.velocity = velocity / 127.0f;
        voice.gate = true;
        voice.age = 0;  // Reset age on new note
        voice.amp_env.Gate(true);
        voice.filter_env.Gate(true);
        
        voice_counter_++;
    }
    
    void NoteOff(uint8_t note) {
        // Release all voices playing this note
        for (uint32_t v = 0; v < kNumVoices; v++) {
            if (voices_[v].note == note && voices_[v].gate) {
                voices_[v].gate = false;
                voices_[v].amp_env.Gate(false);
                voices_[v].filter_env.Gate(false);
            }
        }
    }
    
    void GateOn(uint8_t velocity) {
        // For gate trigger mode, use last note or default
        NoteOn(60, velocity);  // Middle C as default
    }
    
    void GateOff() {
        // Release all gates
        for (uint32_t v = 0; v < kNumVoices; v++) {
            if (voices_[v].gate) {
                voices_[v].gate = false;
                voices_[v].amp_env.Gate(false);
                voices_[v].filter_env.Gate(false);
            }
        }
    }
    
    void AllNoteOff() {
        for (uint32_t v = 0; v < kNumVoices; v++) {
            voices_[v].gate = false;
            voices_[v].amp_env.Gate(false);
            voices_[v].filter_env.Gate(false);
            voices_[v].amp_env.Reset();
            voices_[v].filter_env.Reset();
        }
    }
    
    void PitchBend(uint16_t bend) {
        pitch_bend_ = (static_cast<float>(bend) - 8192.0f) / 8192.0f;
    }
    
    void ChannelPressure(uint8_t pressure) {
        pressure_ = pressure / 127.0f;
    }
    
    void Aftertouch(uint8_t note, uint8_t value) {
        (void)note;
        (void)value;
    }
    
    void LoadPreset(uint8_t idx) {
        preset_idx_ = idx;
    }
    
    uint8_t GetPresetIndex() const {
        return preset_idx_;
    }
    
    const uint8_t* GetPresetData(uint8_t idx) const {
        (void)idx;
        return nullptr;
    }
    
private:
    /**
     * @brief Load a bank's wavetable into a PPG oscillator
     */
    void LoadBankWavetable(common::PpgOsc<PPG_WAVES_PER_BANK>& osc, int bank) {
        if (bank < 0 || bank >= PPG_NUM_BANKS) return;
        
        uint8_t wavetable_def[PPG_WAVES_PER_BANK * 2 + 1];
        
        for (int i = 0; i < PPG_WAVES_PER_BANK; i++) {
            wavetable_def[i * 2] = static_cast<uint8_t>(bank * 16 + i * 2);
            wavetable_def[i * 2 + 1] = static_cast<uint8_t>(i);
        }
        wavetable_def[PPG_WAVES_PER_BANK * 2] = 0xFF;
        
        osc.LoadWavetable(ppg_wave_data, wavetable_def);
    }
    
    float sample_rate_;
    float inv_sample_rate_;
    
    // Voice pool
    Voice voices_[kNumVoices];
    
    // Track current bank to reload wavetable on change
    int current_bank_a_;
    int current_bank_b_;
    
    // Global modulation
    LFO lfo_;
    common::StereoWidener stereo_widener_;
    
    // NEON-aligned intermediate buffer for mixing
    float mix_buffer_[kMaxFrames] __attribute__((aligned(16)));
    
    // Global state
    float pitch_bend_;
    float pressure_;
    uint32_t tempo_;
    uint32_t voice_counter_;  // For round-robin allocation
    
    // Parameters
    int32_t params_[P_NUM_PARAMS];
    uint8_t preset_idx_;
};
