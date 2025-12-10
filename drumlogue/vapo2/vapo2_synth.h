/**
 * @file vapo2_synth.h
 * @brief 4-Voice Polyphonic Vapo2 wavetable synthesizer
 *
 * Coordinates oscillators, envelopes, filter, and LFO across 4 voices.
 * Uses PPG Wave style oscillators for authentic 8-bit character.
 * 
 * Voice allocation: Round-robin with oldest-note stealing when all voices busy.
 * 
 * v1.1.0: MOD HUB system for expanded modulation routing
 */

#pragma once

#include <cstdint>
#include <cmath>

#include "unit.h"
#include "wavetable_osc.h"
#include "envelope.h"
#include "filter.h"
#include "lfo.h"
#include "smoothed_value.h"
#include "resources/ppg_waves.h"
#include "../common/stereo_widener.h"

// NEON SIMD optimizations (Cortex-A7)
#ifdef USE_NEON
#include <arm_neon.h>
#endif

// Common DSP utilities (includes NEON helpers when USE_NEON defined)
#define NEON_DSP_NS vapo2
#include "../common/neon_dsp.h"

// Number of polyphonic voices (2 for CPU headroom)
static constexpr uint32_t kNumVoices = 2;

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
    
    // Page 6: MOD HUB & Output
    P_MOD_SELECT,    // Selects which mod destination to edit
    P_MOD_VALUE,     // Value for selected mod destination
    P_OSC_MIX,       // Bipolar: -64=A, 0=50/50, +63=B
    P_SPACE,         // Bipolar: -64=mono, 0=normal, +63=wide
    
    P_NUM_PARAMS
};

// MOD HUB destinations (all use -64..+63 bipolar range for SDK compatibility)
enum ModDestination {
    MOD_LFO_RATE = 0,      // -64..+63 maps to 0.05-20Hz (centered ~2Hz)
    MOD_LFO_SHAPE,         // -64..+63: use lower 3 bits only (0-5 = shapes)
    MOD_LFO_TO_MORPH,      // -64..+63 bipolar depth
    MOD_LFO_TO_FILTER,     // -64..+63 bipolar depth
    MOD_VEL_TO_FILTER,     // -64..+63 (-64=none, +63=full)
    MOD_KEY_TRACK,         // -64..+63 (-64=none, +63=full)
    MOD_OSC_B_DETUNE,      // -64..+63 cents
    MOD_PB_RANGE,          // -64..+63: use lower 2 bits (0-3 = ranges)
    MOD_NUM_DESTINATIONS
};

// Filter type names
static const char* s_filter_names[] = {
    "LP12",
    "LP24",
    "HP12",
    "BP12"
};

// PPG Bank names for display (shortened for display)
static const char* ppg_bank_names[] = {
    "UPPER",    // 0:  waves 0-15
    "RESNT1",   // 1:  waves 16-31
    "RESNT2",   // 2:  waves 32-47
    "MELLOW",   // 3:  waves 48-63
    "BRIGHT",   // 4:  waves 64-79
    "HARSH",    // 5:  waves 80-95
    "CLIPPR",   // 6:  waves 96-111
    "SYNC",     // 7:  waves 112-127
    "PWM",      // 8:  waves 128-143
    "VOCAL1",   // 9:  waves 144-159
    "VOCAL2",   // 10: waves 160-175
    "ORGAN",    // 11: waves 176-191
    "BELL",     // 12: waves 192-207
    "ALIEN",    // 13: waves 208-223
    "NOISE",    // 14: waves 224-239
    "SPECAL"    // 15: waves 240-255
};

// PPG oscillator mode names
static const char* ppg_mode_names[] = {
    "HiFi",   // INTERP_2D - bilinear interpolation (smoothest)
    "LoFi",   // INTERP_1D - sample interpolation only (stepped waves)
    "Raw"     // NO_INTERP - no interpolation (authentic PPG crunch)
};

// MOD HUB destination names
static const char* mod_dest_names[] = {
    "LFO SPD",   // 0: LFO Rate
    "LFO SHP",   // 1: LFO Shape
    "LFO>MRP",   // 2: LFO to Morph
    "LFO>FLT",   // 3: LFO to Filter
    "VEL>FLT",   // 4: Velocity to Filter
    "KEY TRK",   // 5: Filter Key Track
    "B TUNE",    // 6: Osc B Detune
    "PB RNG"     // 7: Pitch Bend Range
};

// LFO shape names
static const char* lfo_shape_names[] = {
    "Sine",
    "Tri",
    "Saw+",
    "Saw-",
    "Square",
    "S&H"
};

// Pitch bend range names
static const char* pb_range_names[] = {
    "+/-2",
    "+/-7",
    "+/-12",
    "+/-24"
};

// Pitch bend semitone values
static const float pb_semitones[] = {2.0f, 7.0f, 12.0f, 24.0f};

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
        
        // Initialize parameter smoothing
        cutoff_smooth_.Init(1.0f, 0.005f);   // Slower for filter
        osc_mix_smooth_.Init(0.5f, 0.01f);   // Medium for crossfade
        space_smooth_.Init(0.0f, 0.01f);     // Medium for stereo
        
        // Initialize state
        pitch_bend_ = 0.0f;
        pressure_ = 0.0f;
        voice_counter_ = 0;
        params_dirty_ = 0xFFFFFFFF;  // Mark all params dirty initially
        
        // Clear intermediate buffers
        vapo2::neon::ClearBuffer(mix_buffer_, kMaxFrames);
        
        // Default parameters
        for (int i = 0; i < P_NUM_PARAMS; i++) {
            params_[i] = 0;
        }
        
        // Set some sensible defaults (matching header.c)
        params_[P_OSC_MODE] = 0;  // HiFi PPG mode by default
        params_[P_OSC_MIX] = 0;   // Bipolar 0 = 50% mix
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
        params_[P_SPACE] = 0;    // Bipolar 0 = normal stereo spread
        
        // Initialize MOD HUB values (all 0-127 range, 64 = center for bipolar)
        mod_values_[MOD_LFO_RATE] = 40;      // LFO speed (0-127)
        mod_values_[MOD_LFO_SHAPE] = 0;      // Sine (0-5 mapped from 0-127)
        mod_values_[MOD_LFO_TO_MORPH] = 64;  // No modulation (64 = center)
        mod_values_[MOD_LFO_TO_FILTER] = 64; // No modulation (64 = center)
        mod_values_[MOD_VEL_TO_FILTER] = 0;  // No velocity to filter (0-127)
        mod_values_[MOD_KEY_TRACK] = 0;      // No key tracking (0-127)
        mod_values_[MOD_OSC_B_DETUNE] = 64;  // No detune (64 = center)
        mod_values_[MOD_PB_RANGE] = 0;       // ±2 semitones (0-3 mapped from 0-127)
        
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
        
        // === Read MOD HUB values (all 0-127 range) ===
        // LFO rate: 0-127 directly for SetRate
        const int32_t lfo_rate = mod_values_[MOD_LFO_RATE];
        // LFO shape: 0-127 maps to 0.0-5.0 for smooth morphing between shapes
        const float lfo_shape_morph = mod_values_[MOD_LFO_SHAPE] * 5.0f / 127.0f;
        // Bipolar modulation depths: 0-127, 64 = center (no mod)
        const float lfo_to_morph = (mod_values_[MOD_LFO_TO_MORPH] - 64) / 64.0f;  // -1 to +1
        const float lfo_to_filter = (mod_values_[MOD_LFO_TO_FILTER] - 64) / 64.0f;  // -1 to +1
        const float vel_to_filter = mod_values_[MOD_VEL_TO_FILTER] / 127.0f;  // 0-1
        const float key_track = mod_values_[MOD_KEY_TRACK] / 127.0f;  // 0-1
        const float osc_b_detune = (mod_values_[MOD_OSC_B_DETUNE] - 64) * 0.5f;  // -32 to +32 cents
        // PB range: 0-127 maps to 0-3 (index into pb_semitones)
        const int pb_range_idx = (mod_values_[MOD_PB_RANGE] * 4 / 128) & 3;
        const float pb_range = pb_semitones[pb_range_idx];
        
        // === Update smoothed parameters ===
        cutoff_smooth_.SetTarget(params_[P_FILTER_CUTOFF] / 127.0f);
        osc_mix_smooth_.SetTarget((params_[P_OSC_MIX] + 64) / 127.0f);  // Bipolar to 0-1
        space_smooth_.SetTarget((params_[P_SPACE] + 64) / 127.0f * 1.5f);  // Bipolar to 0-1.5
        
        // Calculate derived parameters (global)
        const float osc_a_morph = params_[P_OSC_A_MORPH] / 127.0f;
        const float osc_b_morph = params_[P_OSC_B_MORPH] / 127.0f;
        
        const float resonance = params_[P_FILTER_RESO] / 127.0f;
        const float filter_env_amt = params_[P_FILTER_ENV] / 64.0f;
        
        const float osc_a_octave = static_cast<float>(params_[P_OSC_A_OCT]);
        const float osc_a_tune = params_[P_OSC_A_TUNE] / 100.0f;
        const float osc_b_octave = static_cast<float>(params_[P_OSC_B_OCT]);
        
        // Get current banks and check for reload (dirty flag check)
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
        
        // === Update parameters only when dirty ===
        
        // PPG oscillator mode
        if (params_dirty_ & (1u << P_OSC_MODE)) {
            const auto ppg_mode = static_cast<common::PpgMode>(params_[P_OSC_MODE]);
            for (uint32_t v = 0; v < kNumVoices; v++) {
                voices_[v].osc_a.SetMode(ppg_mode);
                voices_[v].osc_b.SetMode(ppg_mode);
            }
        }
        
        // Envelope parameters (check dirty flags for any envelope param)
        const uint32_t amp_env_mask = (1u << P_AMP_ATTACK) | (1u << P_AMP_DECAY) | 
                                       (1u << P_AMP_SUSTAIN) | (1u << P_AMP_RELEASE);
        if (params_dirty_ & amp_env_mask) {
            for (uint32_t v = 0; v < kNumVoices; v++) {
                voices_[v].amp_env.SetAttack(params_[P_AMP_ATTACK]);
                voices_[v].amp_env.SetDecay(params_[P_AMP_DECAY]);
                voices_[v].amp_env.SetSustain(params_[P_AMP_SUSTAIN] / 127.0f);
                voices_[v].amp_env.SetRelease(params_[P_AMP_RELEASE]);
            }
        }
        
        const uint32_t filt_env_mask = (1u << P_FILT_ATTACK) | (1u << P_FILT_DECAY) | 
                                        (1u << P_FILT_SUSTAIN) | (1u << P_FILT_RELEASE);
        if (params_dirty_ & filt_env_mask) {
            for (uint32_t v = 0; v < kNumVoices; v++) {
                voices_[v].filter_env.SetAttack(params_[P_FILT_ATTACK]);
                voices_[v].filter_env.SetDecay(params_[P_FILT_DECAY]);
                voices_[v].filter_env.SetSustain(params_[P_FILT_SUSTAIN] / 127.0f);
                voices_[v].filter_env.SetRelease(params_[P_FILT_RELEASE]);
            }
        }
        
        // LFO rate and shape morph (from MOD HUB)
        lfo_.SetRate(lfo_rate);
        lfo_.SetShapeMorph(lfo_shape_morph);  // Smooth morph between shapes
        
        // Clear dirty flags
        params_dirty_ = 0;
        
        // Process each sample
        for (uint32_t i = 0; i < frames; i++) {
            // Get smoothed values for this sample
            const float cutoff_base = cutoff_smooth_.Process();
            const float osc_mix = osc_mix_smooth_.Process();
            
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
                
                // Calculate frequency for this voice with configurable pitch bend range
                const float base_note = static_cast<float>(voice.note) + pitch_bend_ * pb_range;
                const float freq_a = 440.0f * powf(2.0f, (base_note - 69.0f + osc_a_octave * 12.0f + osc_a_tune) / 12.0f);
                const float freq_b = 440.0f * powf(2.0f, (base_note - 69.0f + osc_b_octave * 12.0f + osc_b_detune) / 12.0f);
                
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
                
                // Calculate filter cutoff with envelope, LFO, velocity, and key tracking
                float cutoff = cutoff_base;
                cutoff += filt_env_val * filter_env_amt;
                cutoff += lfo_val * lfo_to_filter * 0.5f;  // LFO to filter
                cutoff += voice.velocity * vel_to_filter * 0.5f;  // Velocity to filter
                cutoff += (voice.note - 60) / 60.0f * key_track;  // Key tracking (C4 = center)
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
        
        // Get smoothed stereo width
        const float space = space_smooth_.GetValue();
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
        if (id == P_MOD_VALUE) {
            // Route MOD_VALUE to the currently selected mod destination
            int sel = params_[P_MOD_SELECT];
            if (sel >= 0 && sel < MOD_NUM_DESTINATIONS) {
                mod_values_[sel] = value;
            }
        } else if (id < P_NUM_PARAMS) {
            if (params_[id] != value) {
                params_[id] = value;
                params_dirty_ |= (1u << id);
            }
        }
    }
    
    int32_t GetParameter(uint8_t id) const {
        if (id == P_MOD_VALUE) {
            // Return the value for the currently selected mod destination
            int sel = params_[P_MOD_SELECT];
            if (sel >= 0 && sel < MOD_NUM_DESTINATIONS) {
                return mod_values_[sel];
            }
            return 0;
        }
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
            case P_MOD_SELECT:
                if (value >= 0 && value < MOD_NUM_DESTINATIONS) {
                    return mod_dest_names[value];
                }
                break;
            case P_MOD_VALUE: {
                // Return string based on currently selected mod destination
                // Value is 0-127 range (common for all destinations)
                int sel = params_[P_MOD_SELECT];
                switch (sel) {
                    case MOD_LFO_SHAPE: {
                        // Map 0-127 to 0-5 for shape index
                        int shape = value * 6 / 128;
                        if (shape > 5) shape = 5;
                        return lfo_shape_names[shape];
                    }
                    case MOD_PB_RANGE: {
                        // Map 0-127 to 0-3 for range index
                        int range = value * 4 / 128;
                        if (range > 3) range = 3;
                        return pb_range_names[range];
                    }
                    // Other destinations show numeric value (return nullptr)
                }
                break;
            }
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
    
    // Parameter smoothing
    SmoothedValue cutoff_smooth_;
    SmoothedValue osc_mix_smooth_;
    SmoothedValue space_smooth_;
    
    // NEON-aligned intermediate buffer for mixing
    float mix_buffer_[kMaxFrames] __attribute__((aligned(16)));
    
    // Global state
    float pitch_bend_;
    float pressure_;
    uint32_t tempo_;
    uint32_t voice_counter_;  // For round-robin allocation
    uint32_t params_dirty_;   // Bitmask of changed parameters
    
    // Parameters
    int32_t params_[P_NUM_PARAMS];
    
    // MOD HUB values (stored separately from main params)
    int32_t mod_values_[MOD_NUM_DESTINATIONS];
    
    uint8_t preset_idx_;
};
