/**
 * @file pepege_synth.h
 * @brief 2-Voice Polyphonic Pepege wavetable synthesizer
 *
 * Coordinates oscillators, envelopes, filter, and LFO across 2 voices.
 * Uses PPG Wave style oscillators for authentic 8-bit character.
 * 
 * Voice allocation: Round-robin with oldest-note stealing when all voices busy.
 * 
 * v1.1.2: Rename to Pepege-Synth and preserve MOD HUB
 */

#pragma once

#include <cstdint>
#include <cstdio>
#include <cmath>

#ifndef TEST
#include "unit.h"
#endif
#include "envelope.h"
#include "filter.h"
#include "lfo.h"
#include "smoothed_value.h"
#include "resources/ppg_waves.h"
#include "../common/stereo_widener.h"
#include "../common/ppg_osc.h"
#include "../common/hub_control.h"  // NEW: Advanced hub control
#include "../common/param_format.h"

// NEON SIMD optimizations (Cortex-A7)
#ifdef USE_NEON
#include <arm_neon.h>
#endif

// Common DSP utilities (includes NEON helpers when USE_NEON defined)
#define NEON_DSP_NS pepege
#include "../common/neon_dsp.h"

// Number of polyphonic voices (2 for CPU headroom)
static constexpr uint32_t kNumVoices = 2;

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
    
    // Page 6: MOD HUB & Output
    P_MOD_SELECT,    // Selects which mod destination to edit
    P_MOD_VALUE,     // Value for selected mod destination
    P_OSC_MIX,       // Bipolar: -64=A, 0=50/50, +63=B
    P_SPACE,         // Bipolar: -64=mono, 0=normal, +63=wide
    
    P_NUM_PARAMS
};

// MOD HUB destinations (expanded with HubControl)
enum ModDestination {
    MOD_LFO_RATE = 0,      // LFO Rate (0.05-20Hz)
    MOD_LFO_SHAPE,         // LFO Shape selection
    MOD_LFO_TO_MORPH,      // LFO modulates morph (bipolar)
    MOD_LFO_TO_FILTER,     // LFO modulates filter cutoff (bipolar)
    MOD_VEL_TO_FILTER,     // Velocity to filter cutoff
    MOD_KEY_TRACK,         // Filter key tracking
    MOD_OSC_B_DETUNE,      // Osc B detune (cents, bipolar)
    MOD_PB_RANGE,          // Pitch bend range
    MOD_NUM_DESTINATIONS
};

// LFO shape names (for enum display)
static const char* const kLfoShapeNames[] = {
    "Sine", "Tri", "Saw+", "Saw-", "Square", "S&H"
};

// Pitch bend range names
static const char* const kPbRangeNames[] = {
    "+/-2", "+/-7", "+/-12", "+/-24"
};

// Hub control destinations for MOD HUB (using HubControl system)
static constexpr common::HubControl<MOD_NUM_DESTINATIONS>::Destination kModDestinations[] = {
    {"LFO SPD", "%",   0, 100, 20,  false, nullptr},        // LFO rate (0-100%)
    {"LFO SHP", "",    0, 5,   0,   false, kLfoShapeNames}, // LFO shape (0-5)
    {"LFO>MRP", "%",   0, 100, 50,  true,  nullptr},        // LFO to morph (bipolar ±)
    {"LFO>FLT", "%",   0, 100, 50,  true,  nullptr},        // LFO to filter (bipolar ±)
    {"VEL>FLT", "%",   0, 100, 0,   false, nullptr},        // Velocity to filter
    {"KEY TRK", "%",   0, 100, 0,   false, nullptr},        // Key tracking
    {"B TUNE", "c",    0, 100, 50,  true,  nullptr},        // Osc B detune (bipolar ±50 cents)
    {"PB RNG",  "",    0, 3,   0,   false, kPbRangeNames}   // Pitch bend range (0-3)
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

class PepegeSynth {
public:
    PepegeSynth() {}
    ~PepegeSynth() {}
    
#ifndef TEST
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
        pepege::neon::ClearBuffer(mix_buffer_, kMaxFrames);
        
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
        
        // Initialize MOD HUB with HubControl
        mod_hub_ptr_ = new common::HubControl<MOD_NUM_DESTINATIONS>(kModDestinations);
        
        // Initialize hub_values storage with defaults (0-100 range)
        hub_values_[MOD_LFO_RATE] = 20;      // LFO speed (20%)
        hub_values_[MOD_LFO_SHAPE] = 0;      // Sine shape
        hub_values_[MOD_LFO_TO_MORPH] = 50;  // No modulation (50 = center)
        hub_values_[MOD_LFO_TO_FILTER] = 50; // No modulation (50 = center)
        hub_values_[MOD_VEL_TO_FILTER] = 0;  // No velocity to filter
        hub_values_[MOD_KEY_TRACK] = 0;      // No key tracking
        hub_values_[MOD_OSC_B_DETUNE] = 50;  // No detune (50 = center)
        hub_values_[MOD_PB_RANGE] = 0;       // ±2 semitones
        
        // Set destination to first slot initially
        mod_hub_ptr_->SetDestination(0);
        
        // Set default values for each destination in HubControl
        for (int i = 0; i < MOD_NUM_DESTINATIONS; i++) {
            mod_hub_ptr_->SetValueForDest(i, hub_values_[i]);
        }
        
        preset_idx_ = 0;
        
        return k_unit_err_none;
    }
#else
    // Test-friendly Init method
    void Init(float sample_rate = 48000.0f) {
        sample_rate_ = sample_rate;
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
        pepege::neon::ClearBuffer(mix_buffer_, kMaxFrames);
        
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
        
        // Initialize MOD HUB with HubControl
        mod_hub_ptr_ = new common::HubControl<MOD_NUM_DESTINATIONS>(kModDestinations);
        
        // Initialize hub_values storage with defaults (0-100 range)
        hub_values_[MOD_LFO_RATE] = 20;      // LFO speed (20%)
        hub_values_[MOD_LFO_SHAPE] = 0;      // Sine shape
        hub_values_[MOD_LFO_TO_MORPH] = 50;  // No modulation (50 = center)
        hub_values_[MOD_LFO_TO_FILTER] = 50; // No modulation (50 = center)
        hub_values_[MOD_VEL_TO_FILTER] = 0;  // No velocity to filter
        hub_values_[MOD_KEY_TRACK] = 0;      // No key tracking
        hub_values_[MOD_OSC_B_DETUNE] = 50;  // No detune (50 = center)
        hub_values_[MOD_PB_RANGE] = 0;       // ±2 semitones
        
        // Set destination to first slot initially
        mod_hub_ptr_->SetDestination(0);
        
        // Set default values for each destination in HubControl
        for (int i = 0; i < MOD_NUM_DESTINATIONS; i++) {
            mod_hub_ptr_->SetValueForDest(i, hub_values_[i]);
        }
        
        preset_idx_ = 0;
    }
#endif
    
    void Teardown() {
        if (mod_hub_ptr_ != nullptr) {
            delete mod_hub_ptr_;
            mod_hub_ptr_ = nullptr;
        }
    }
    
    void Reset() {
        for (uint32_t v = 0; v < kNumVoices; v++) {
            voices_[v].Reset();
        }
        lfo_.Reset();
        voice_counter_ = 0;
    }
    
    void Resume() {}
    void Suspend() {}
    
#ifdef TEST
    // Test-friendly Process method that matches WAV processing expectations
    void Process(const float* input, float* output, size_t frames, size_t channels) {
        (void)input;  // Synth doesn't use input
        Render(output, static_cast<uint32_t>(frames));
        // If stereo output is expected but Render produces mono, duplicate to stereo
        if (channels == 2) {
            for (size_t i = frames; i > 0; --i) {
                output[i * 2 - 1] = output[i * 2 - 2] = output[i - 1];
            }
        }
    }
#endif
    
    void Render(float* out, uint32_t frames) {
        // Limit frames to our buffer size
        if (frames > kMaxFrames) frames = kMaxFrames;
        
        // Clear mix buffer
        pepege::neon::ClearBuffer(mix_buffer_, frames);
        
        // === Read MOD HUB values using HubControl ===
        // LFO rate: 0-100 maps to 0-127 for SetRate
        const int32_t lfo_rate = mod_hub_ptr_->GetValue(MOD_LFO_RATE) * 127 / 100;
        // LFO shape: 0-5 directly (enum value)
        const int32_t lfo_shape_value = mod_hub_ptr_->GetValue(MOD_LFO_SHAPE);
        const float lfo_shape_morph = lfo_shape_value * 1.0f;  // 0-5 range
        // Bipolar modulation depths: 0-100, 50 = center (no mod) -> -1 to +1
        const float lfo_to_morph = mod_hub_ptr_->GetValueNormalizedBipolar(MOD_LFO_TO_MORPH);
        const float lfo_to_filter = mod_hub_ptr_->GetValueNormalizedBipolar(MOD_LFO_TO_FILTER);
        // Unipolar values: 0-100 -> 0-1
        const float vel_to_filter = mod_hub_ptr_->GetValueNormalizedUnipolar(MOD_VEL_TO_FILTER);
        const float key_track = mod_hub_ptr_->GetValueNormalizedUnipolar(MOD_KEY_TRACK);
        // Osc B detune: bipolar ±50 cents -> -32 to +32 cents
        const float osc_b_detune = mod_hub_ptr_->GetValueScaledBipolar(MOD_OSC_B_DETUNE, 32.0f);
        // PB range: 0-3 directly (enum value)
        int pb_range_idx = mod_hub_ptr_->GetValue(MOD_PB_RANGE);
        if (pb_range_idx < 0 || pb_range_idx > 3) pb_range_idx = 0;
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
            // Get smoothed parameter values
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
                
                // Calculate frequency with pitch bend
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
                int filter_type = params_[P_FILTER_TYPE];
                if (filter_type < 0 || filter_type > 3) filter_type = 0;
                voice.filter.SetType(static_cast<FilterType>(filter_type));
                float filtered = voice.filter.Process(osc_out);
                
                // Apply envelope and velocity, add to mix
                sample_sum += filtered * amp_env_val * voice.velocity;
            }
            
            // Store mixed sample (scale by 1/voices for headroom)
            mix_buffer_[i] = sample_sum * (1.0f / kNumVoices);
        }
        
        // === NEON-optimized output stage ===
        
        // Sanitize (remove NaN/Inf) and soft clamp
        pepege::neon::SanitizeAndClamp(mix_buffer_, 1.0f, frames);
        
        // Get current smoothed stereo width value
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
        pepege::neon::InterleaveStereo(left_buf, right_buf, out, frames);
    }
    
    void SetParameter(uint8_t id, int32_t value) {
        if (id == P_MOD_SELECT) {
            // Clamp to valid range
            if (value < 0) value = 0;
            if (value >= MOD_NUM_DESTINATIONS) value = MOD_NUM_DESTINATIONS - 1;
            
            // Set MOD HUB destination
            mod_hub_ptr_->SetDestination(value);
            
            // Restore the previously stored value for this destination
            // This preserves user edits when switching between destinations
            mod_hub_ptr_->SetValueForDest(value, hub_values_[value]);
            
            // Store in params for GetParameter
            params_[id] = value;
        } else if (id == P_MOD_VALUE) {
            // Clamp to 0-100 range
            if (value < 0) value = 0;
            if (value > 100) value = 100;
            
            // Set value for currently selected MOD HUB destination
            mod_hub_ptr_->SetValue(value);
            
            // Store the ORIGINAL UI value (0-100) for later restoration
            uint8_t dest = mod_hub_ptr_->GetDestination();
            if (dest < MOD_NUM_DESTINATIONS) {
                hub_values_[dest] = value;
            }
        } else if (id < P_NUM_PARAMS) {
            if (params_[id] != value) {
                params_[id] = value;
                params_dirty_ |= (1u << id);
            }
        }
    }
    
    int32_t GetParameter(uint8_t id) const {
        if (id == P_MOD_SELECT) {
            return mod_hub_ptr_->GetDestination();
        } else if (id == P_MOD_VALUE) {
            // Return ORIGINAL 0-100 slider value for UI sync
            uint8_t dest = mod_hub_ptr_->GetDestination();
            if (dest < MOD_NUM_DESTINATIONS) {
                return hub_values_[dest];
            }
            return 0;
        } else if (id < P_NUM_PARAMS) {
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
                return mod_hub_ptr_->GetCurrentDestinationName();
            case P_MOD_VALUE: {
                // Use HubControl's built-in string formatting
                return mod_hub_ptr_->GetCurrentValueString(mod_value_str_, sizeof(mod_value_str_));
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
        if (idx >= kNumPresets) return;
        preset_idx_ = idx;

        const Preset& p = kPresets[idx];

        // Copy params
        for (int i = 0; i < P_NUM_PARAMS; i++) {
            params_[i] = p.params[i];
        }

        // Copy MOD HUB values (convert from old 0-127 format to HubControl 0-100 format)
        for (int i = 0; i < MOD_NUM_DESTINATIONS; i++) {
            int32_t old_value = p.mod[i];
            int32_t new_value;
            
            // Convert based on destination type
            switch (i) {
                case MOD_LFO_RATE:
                    // 0-127 -> 0-100
                    new_value = old_value * 100 / 127;
                    break;
                case MOD_LFO_SHAPE:
                    // 0-127 maps to 0-5 -> 0-5 directly
                    new_value = old_value * 6 / 128;
                    if (new_value > 5) new_value = 5;
                    break;
                case MOD_LFO_TO_MORPH:
                case MOD_LFO_TO_FILTER:
                case MOD_OSC_B_DETUNE:
                    // Bipolar: 0-127 with 64=center -> 0-100 with 50=center
                    new_value = (old_value * 100 + 63) / 127;  // Round properly
                    break;
                case MOD_VEL_TO_FILTER:
                case MOD_KEY_TRACK:
                    // Unipolar: 0-127 -> 0-100
                    new_value = old_value * 100 / 127;
                    break;
                case MOD_PB_RANGE:
                    // 0-127 maps to 0-3 -> 0-3 directly
                    new_value = old_value * 4 / 128;
                    if (new_value > 3) new_value = 3;
                    break;
                default:
                    new_value = old_value * 100 / 127;  // Default conversion
                    break;
            }
            
            // Store in both hub_values and HubControl
            hub_values_[i] = new_value;
            mod_hub_ptr_->SetValueForDest(i, new_value);
        }

        // Ensure MOD SELECT points to first slot to avoid confusion
        params_[P_MOD_SELECT] = 0;
        mod_hub_ptr_->SetDestination(0);

        // Force reload of wavetables and params
        current_bank_a_ = -1;
        current_bank_b_ = -1;
        params_dirty_ = 0xFFFFFFFF;
    }
    
    uint8_t GetPresetIndex() const {
        return preset_idx_;
    }
    
    static const char* GetPresetName(uint8_t idx) {
        static const char* names[] = {
            "Glass Keys",
            "Dust Pad",
            "Sync Bass",
            "Noise Sweep",
            "Pluck",
            "PWM Lead"
        };
        if (idx >= kNumPresets) return nullptr;
        return names[idx];
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
    
    // Buffer for MOD VALUE display string
    mutable char mod_value_str_[8];
    
    // Global state
    float pitch_bend_;
    float pressure_;
    uint32_t tempo_;
    uint32_t voice_counter_;  // For round-robin allocation
    uint32_t params_dirty_;   // Bitmask of changed parameters

    // Factory presets
    struct Preset {
        int8_t params[P_NUM_PARAMS];
        int8_t mod[MOD_NUM_DESTINATIONS];
    };
    
    static constexpr uint8_t kNumPresets = 6;
    
    // Presets are voiced for the PPG-style banks
    static constexpr Preset kPresets[kNumPresets] = {
        // 0: Glass Keys
        {
            {
                /*A BANK*/4, /*A MORPH*/96, /*A OCT*/0, /*A TUNE*/0,
                /*B BANK*/9, /*B MORPH*/64, /*B OCT*/0, /*MODE*/0,
                /*CUTOFF*/96, /*RESO*/30, /*FLT ENV*/20, /*FLT TYP*/1,
                /*ATTACK*/5, /*DECAY*/60, /*SUST*/100, /*REL*/50,
                /*F.ATK*/10, /*F.DCY*/60, /*F.SUS*/50, /*F.REL*/60,
                /*MOD SEL*/0, /*MOD VAL*/64, /*OSC MIX*/16, /*SPACE*/10
            },
            {
                /*LFO RT*/45, /*LFO SHP*/0, /*LFO>MOR*/80, /*LFO>FLT*/64,
                /*VEL>F*/40, /*KEYTRK*/80, /*DETUNE*/64, /*PB RNG*/32
            }
        },
        // 1: Dust Pad
        {
            {
                3, 100, -1, 0,
                12, 90, 0, 0,
                70, 25, 50, 0,
                40, 90, 100, 80,
                30, 80, 80, 70,
                0, 64, 0, 40
            },
            {
                30, 0, 90, 70,
                20, 90, 64, 32
            }
        },
        // 2: Sync Bass
        {
            {
                7, 80, -1, -5,
                6, 70, -1, 2,
                60, 70, 40, 1,
                2, 50, 70, 25,
                5, 70, 40, 40,
                0, 64, -20, -20
            },
            {
                60, 100, 40, 30,
                30, 70, 80, 96
            }
        },
        // 3: Noise Sweep FX
        {
            {
                14, 64, 0, 0,
                6, 32, 0, 1,
                90, 20, 50, 2,
                0, 70, 80, 40,
                0, 90, 20, 50,
                0, 64, -10, 50
            },
            {
                20, 120, 90, 90,
                10, 0, 64, 64
            }
        },
        // 4: Pluck
        {
            {
                5, 40, 0, 0,
                12, 50, 0, 0,
                90, 20, 50, 1,
                2, 40, 80, 25,
                2, 60, 30, 30,
                0, 64, -10, 15
            },
            {
                80, 0, 50, 30,
                60, 70, 70, 32
            }
        },
        // 5: PWM Lead
        {
            {
                8, 90, 1, 6,
                8, 60, 0, 0,
                100, 20, 30, 0,
                5, 70, 100, 60,
                20, 60, 50, 50,
                0, 64, 10, 30
            },
            {
                55, 64, 80, 40,
                50, 80, 90, 64
            }
        }
    };
    
    // Parameters
    int32_t params_[P_NUM_PARAMS];
    
    // MOD HUB control (initialized in Init)
    common::HubControl<MOD_NUM_DESTINATIONS>* mod_hub_ptr_;
    
    // MOD HUB values storage (original 0-100 slider values)
    int32_t hub_values_[MOD_NUM_DESTINATIONS];
    
    uint8_t preset_idx_;
};
