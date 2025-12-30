/**
 * @file drupiter_synth.cc
 * @brief Main synthesizer implementation for Drupiter
 *
 * Based on Bristol Jupiter-8 emulation architecture
 * Integrates DCO, VCF, Envelopes, and LFO
 */

#include "drupiter_synth.h"
#include "presets.h"
#include "../common/hub_control.h"
#include "../common/param_format.h"
#include "../common/midi_helper.h"
#include "../common/preset_manager.h"
#include "../common/dsp_utils.h"
#include "dsp/jupiter_dco.h"
#include "dsp/jupiter_vcf.h"
#include "dsp/jupiter_env.h"
#include "dsp/jupiter_lfo.h"
#include "../common/smoothed_value.h"
#include <cmath>
#include <cstring>
#include <cstdio>
#include <iostream>

// Enable USE_NEON for ARM NEON optimizations (but not in test builds)
#ifndef TEST
#ifdef __ARM_NEON
#define USE_NEON 1
#endif
#endif

// Disable NEON DSP when testing on host
#ifndef TEST
#define NEON_DSP_NS drupiter
#include "../common/neon_dsp.h"
#endif

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

// Fast pow2 approximation using bit manipulation (from jupiter_dco.cc)
DRUMLOGUE_ALWAYS_INLINE float fasterpow2f(float p) {
    float clipp = (p < -126.0f) ? -126.0f : p;
    union { uint32_t i; float f; } v = { 
        static_cast<uint32_t>((1 << 23) * (clipp + 126.94269504f))
    };
    return v.f;
}

namespace {

// Threshold constants for modulation and distance checks
static constexpr float kMinModulation = 0.001f;  // Minimum significant modulation depth
static constexpr float kMinDistance = 1e-6f;     // Minimum significant log distance for glide

// Fast 2^x approximation (accurate for |x| < 8)
// Uses polynomial: 2^x ≈ 1 + 0.693x + 0.240x² + 0.056x³
// Relative error < 0.3% for |x| < 4, piecewise approximation for |x| >= 4
inline float fast_pow2(float x) {
    // Clamp to safe range
    if (x < -8.0f) return 0.00390625f;  // 2^-8
    if (x > 8.0f) return 256.0f;         // 2^8
    
    // For |x| >= 4, use piecewise approximation: 2^x = 2^(4 + (x-4)) = 16 * 2^(x-4)
    // This keeps the polynomial input in the accurate range
    if (x >= 4.0f) {
        return 16.0f * fast_pow2(x - 4.0f);
    } else if (x <= -4.0f) {
        return fast_pow2(x + 4.0f) * 0.0625f;  // 2^x = 2^((x+4) - 4) = 2^(x+4) / 16
    }
    
    // Polynomial approximation (Horner's form) for |x| < 4
    const float c1 = 0.693147181f;  // ln(2)
    const float c2 = 0.240226507f;  // ln(2)²/2!
    const float c3 = 0.055504109f;  // ln(2)³/3!
    const float c4 = 0.009618129f;  // ln(2)⁴/4!
    
    return 1.0f + x * (c1 + x * (c2 + x * (c3 + x * c4)));
}

// Fast cents to ratio: 2^(cents/1200)
// For detune in cents, typically ±200
inline float cents_to_ratio(float cents) {
    return fast_pow2(cents * (1.0f / 1200.0f));
}

// Fast semitones to ratio: 2^(semitones/12)
inline float semitones_to_ratio(float semitones) {
    return fast_pow2(semitones * (1.0f / 12.0f));
}

inline uint8_t clamp_u8_int32(int32_t value, int32_t min_value, int32_t max_value) {
    if (value < min_value) return static_cast<uint8_t>(min_value);
    if (value > max_value) return static_cast<uint8_t>(max_value);
    return static_cast<uint8_t>(value);
}

inline uint8_t scale_127_to_100(uint8_t v) {
    // Rounded integer mapping.
    return static_cast<uint8_t>((static_cast<uint32_t>(v) * 100u + 63u) / 127u);
}

inline uint8_t scale_127_centered_to_100(uint8_t v, uint8_t center_127, uint8_t center_100) {
    // Maps values around center with proportional slope. Intended for bipolar-ish params
    // like detune/env amount that previously used a 0..127 with 64=center convention.
    const int32_t delta = static_cast<int32_t>(v) - static_cast<int32_t>(center_127);
    const int32_t mapped = static_cast<int32_t>(center_100) + (delta * 50) / 64;  // 0.78125x
    return clamp_u8_int32(mapped, 0, 100);
}

}  // namespace

DrupiterSynth::DrupiterSynth()
    : allocator_()
    , dco1_(nullptr)
    , dco2_(nullptr)
    , vcf_(nullptr)
    , lfo_(nullptr)
    , env_vcf_(nullptr)
    , env_vca_(nullptr)
    , sample_rate_(48000.0f)
    , current_mode_(dsp::SYNTH_MODE_MONOPHONIC)
    , gate_(false)
    , current_note_(0)
    , current_velocity_(0)
    , current_freq_hz_(440.0f)
    , current_preset_()
    , current_preset_idx_(0)
    , sync_mode_(0)
    , xmod_depth_(0.0f)
    , mod_hub_(kModDestinations)  // Initialize HubControl
    , mod_value_str_()
    , effect_mode_(0)  // CHORUS by default
    , dco1_out_(0.0f)
    , dco2_out_(0.0f)
    , noise_out_(0.0f)
    , mixed_(0.0f)
    , filtered_(0.0f)
    , vcf_env_out_(0.0f)
    , vca_env_out_(0.0f)
    , lfo_out_(0.0f)
    , buffer_guard_(0xDEADBEEFDEADBEEF)
    , space_widener_(nullptr)
    , noise_seed_(0x12345678)
    , cutoff_smooth_(nullptr)
    , dco1_level_smooth_(nullptr)
    , dco2_level_smooth_(nullptr)
    , last_cutoff_hz_(1000.0f)
    , hpf_prev_output_(0.0f)
    , hpf_prev_input_(0.0f)
{
    // Initialize preset to defaults
    std::memset(&current_preset_, 0, sizeof(Preset));
    std::strcpy(current_preset_.name, "Init");
    
    // HubControl initialized with defaults from kModDestinations
    mod_value_str_[0] = '\0';
}

DrupiterSynth::~DrupiterSynth() {
    Teardown();
}

int8_t DrupiterSynth::Init(const unit_runtime_desc_t* desc) {
    if (!desc) {
        return -1;
    }
    
    sample_rate_ = desc->samplerate;
    
    // Allocate DSP components
    dco1_ = new dsp::JupiterDCO();
    dco2_ = new dsp::JupiterDCO();
    vcf_ = new dsp::JupiterVCF();
    lfo_ = new dsp::JupiterLFO();
    env_vcf_ = new dsp::JupiterEnvelope();
    env_vca_ = new dsp::JupiterEnvelope();
    
    // Allocate parameter smoothing
    cutoff_smooth_ = new dsp::SmoothedValue();
    dco1_level_smooth_ = new dsp::SmoothedValue();
    dco2_level_smooth_ = new dsp::SmoothedValue();
    
    // Allocate chorus effect
    space_widener_ = new common::ChorusStereoWidener();
    
    if (!dco1_ || !dco2_ || !vcf_ || !lfo_ || !env_vcf_ || !env_vca_ ||
        !cutoff_smooth_ || !dco1_level_smooth_ || !dco2_level_smooth_ || !space_widener_) {
        Teardown();
        return -1;
    }
    
    // Initialize components
    dco1_->Init(sample_rate_);
    dco2_->Init(sample_rate_);
    vcf_->Init(sample_rate_);
    lfo_->Init(sample_rate_);
    env_vcf_->Init(sample_rate_);
    env_vca_->Init(sample_rate_);
    
    // Initialize parameter smoothing (start at 0, will be set by preset load)
    cutoff_smooth_->Init(0.0f, 0.005f);       // Filter cutoff - slow for smooth sweeps
    dco1_level_smooth_->Init(0.0f, 0.01f);    // DCO1 level - faster smoothing
    dco2_level_smooth_->Init(0.0f, 0.01f);    // DCO2 level - faster smoothing
    
    // Initialize chorus effect (delay-based stereo spread)
    space_widener_->Init(sample_rate_);
    space_widener_->SetDelayTime(15.0f);      // 15ms base delay
    space_widener_->SetModDepth(3.0f);        // ±3ms modulation
    space_widener_->SetLfoRate(0.5f);         // 0.5 Hz LFO
    space_widener_->SetMix(0.5f);             // 50% wet/dry mix
    
    // Initialize voice allocator (Hoover v2.0)
    allocator_.Init(sample_rate_);
    allocator_.SetMode(current_mode_);
    
    // Load init preset (this will set all parameters including smoothed values)
    LoadPreset(0);
    
    // Initialize performance monitoring (when -DPERF_MON enabled)
    #ifdef PERF_MON
    PERF_MON_INIT();
    perf_voice_alloc_ = PERF_MON_REGISTER("VoiceAlloc");
    perf_dco_ = PERF_MON_REGISTER("DCO");
    perf_vcf_ = PERF_MON_REGISTER("VCF");
    perf_effects_ = PERF_MON_REGISTER("Effects");
    perf_render_total_ = PERF_MON_REGISTER("RenderTotal");
    #endif
    
    // Initialize audio buffers to zero
    std::memset(mix_buffer_, 0, sizeof(mix_buffer_));
    std::memset(left_buffer_, 0, sizeof(left_buffer_));
    std::memset(right_buffer_, 0, sizeof(right_buffer_));
    
    // Initialize buffer guard
    buffer_guard_ = 0xDEADBEEFDEADBEEF;
    
    return 0;
}

void DrupiterSynth::Teardown() {
    delete dco1_;
    delete dco2_;
    delete vcf_;
    delete lfo_;
    delete env_vcf_;
    delete env_vca_;
    delete cutoff_smooth_;
    delete dco1_level_smooth_;
    delete dco2_level_smooth_;
    delete space_widener_;
    
    dco1_ = nullptr;
    dco2_ = nullptr;
    vcf_ = nullptr;
    lfo_ = nullptr;
    env_vcf_ = nullptr;
    env_vca_ = nullptr;
    cutoff_smooth_ = nullptr;
    dco1_level_smooth_ = nullptr;
    dco2_level_smooth_ = nullptr;
    space_widener_ = nullptr;
}

void DrupiterSynth::Reset() {
    gate_ = false;
    if (env_vcf_) env_vcf_->Reset();
    if (env_vca_) env_vca_->Reset();
    if (lfo_) lfo_->Reset();
}

void DrupiterSynth::Resume() {
    // Nothing special needed
}

void DrupiterSynth::Suspend() {
    AllNoteOff();
}

void DrupiterSynth::Render(float* out, uint32_t frames) {
    
    // Safety: ensure output buffer and internal buffers can handle the request
    if (!out || frames == 0) return;
    
    // CRITICAL: Limit frames to our buffer size to prevent overflow
    if (frames > kMaxFrames) {
        // Buffer overflow protection - this should NEVER happen
        // If it does, log the violation and clamp
        frames = kMaxFrames;
    }
    
    // Check buffer guard on entry (detect any previous overflow)
    if (buffer_guard_ != 0xDEADBEEFDEADBEEF) {
        // Buffer was corrupted - reset guard and space_widener_
        buffer_guard_ = 0xDEADBEEFDEADBEEF;
        // space_widener_ is likely corrupted, but we can't safely fix it mid-render
    }
    
    // === Read direct DCO parameters (Pages 1 & 2) ===
    // OSC MIX: 0=DCO1 only, 50=equal, 100=DCO2 only
    const float osc_mix = current_preset_.params[PARAM_OSC_MIX] / 100.0f;
    const float dco1_level_target = 1.0f - osc_mix;  // Inverted: 0=full, 100=none
    const float dco2_level_target = osc_mix;         // Direct: 0=none, 100=full
    
    // Update smoothed parameter targets (once per buffer for efficiency)
    cutoff_smooth_->SetTarget(current_preset_.params[PARAM_VCF_CUTOFF] / 100.0f);
    dco1_level_smooth_->SetTarget(dco1_level_target);
    dco2_level_smooth_->SetTarget(dco2_level_target);
    
    // Pre-calculate DCO constants from direct params
    // DCO1: 0=16', 1=8', 2=4'
    // DCO2: 0=32', 1=16', 2=8', 3=4' (extended range)
    const float dco1_oct_mult = Dco1OctaveToMultiplier(current_preset_.params[PARAM_DCO1_OCT]);
    const float dco2_oct_mult = Dco2OctaveToMultiplier(current_preset_.params[PARAM_DCO2_OCT]);
    
    // Detune: convert 0-100 parameter to ±50 cents (50=center/no detune)
    // Jupiter-8 has +50 cents unipolar, but we implement ±50 for more flexibility
    const int32_t detune_param = current_preset_.params[PARAM_DCO2_TUNE];
    const float detune_cents = (static_cast<float>(detune_param) - 50.0f);  // Maps 0-100 to -50 to +50
    const float detune_ratio = cents_to_ratio(detune_cents);  // Fast approximation
    
    // Cross-modulation depth from direct XMOD param
    const float xmod_depth = xmod_depth_;  // Set via PARAM_XMOD in SetParameter
    
    // === Read MOD HUB values ===
    // Unipolar destinations (0 to 1.0)
    const float lfo_pwm_depth = mod_hub_.GetValueNormalizedUnipolar(MOD_LFO_TO_PWM);
    const float lfo_vcf_depth = mod_hub_.GetValueNormalizedUnipolar(MOD_LFO_TO_VCF);
    const float lfo_vco_depth = mod_hub_.GetValueNormalizedUnipolar(MOD_LFO_TO_VCO);
    const float env_pwm_depth = mod_hub_.GetValueNormalizedUnipolar(MOD_ENV_TO_PWM);
    
    // Bipolar destinations (-1.0 to +1.0)
    const float env_vcf_depth = mod_hub_.GetValueNormalizedBipolar(MOD_ENV_TO_VCF);
    const uint8_t hpf_cutoff = mod_hub_.GetValue(MOD_HPF);
    const uint8_t vcf_type = mod_hub_.GetValue(MOD_VCF_TYPE);
    const uint8_t lfo_delay = mod_hub_.GetValue(MOD_LFO_DELAY);
    const uint8_t lfo_wave = mod_hub_.GetValue(MOD_LFO_WAVE);
    
    // New modulation features
    const float lfo_env_amt = mod_hub_.GetValueNormalizedUnipolar(MOD_LFO_ENV_AMT);
    const float vca_level = mod_hub_.GetValueNormalizedUnipolar(MOD_VCA_LEVEL);
    const float vca_lfo_depth = mod_hub_.GetValueNormalizedUnipolar(MOD_VCA_LFO);
    const float vca_kybd = mod_hub_.GetValueNormalizedUnipolar(MOD_VCA_KYBD);
    
    // Phase 2: Pitch envelope modulation (ENV→PIT), bipolar scaled by 12 semitones
    const float env_pitch_depth = mod_hub_.GetValueScaledBipolar(MOD_ENV_TO_PITCH, 12.0f);
    
    // Synthesis mode selection (Hoover v2.0) - read from MOD HUB
    const uint8_t synth_mode_value = mod_hub_.GetValue(MOD_SYNTH_MODE);
    const dsp::SynthMode synth_mode = static_cast<dsp::SynthMode>(synth_mode_value < 3 ? synth_mode_value : 0);
    if (synth_mode != current_mode_) {
        current_mode_ = synth_mode;
        allocator_.SetMode(current_mode_);
    }
    
    // Unison detune control (Hoover v2.0) - 0-50 cents from MOD HUB
    const float unison_detune_cents = static_cast<float>(mod_hub_.GetValue(MOD_UNISON_DETUNE));
    allocator_.SetUnisonDetune(unison_detune_cents);
    
    // Portamento time control (Phase 2.2.4) - 0-100 maps to 0-500ms exponentially
    // Special case: 0 = portamento disabled (0ms), 1-100 = 10-500ms exponential
    const uint8_t porta_param = mod_hub_.GetValue(MOD_PORTAMENTO_TIME);
    float porta_time_ms = 0.0f;
    if (porta_param > 0) {
        const float porta_normalized = porta_param / 100.0f;
        porta_time_ms = 10.0f * powf(50.0f, porta_normalized);  // 10ms to 500ms exponential
    }
    allocator_.SetPortamentoTime(porta_time_ms);
    
    // NOTE: LFO delay and waveform are now set in UpdateLfoSettings()
    // when MOD_HUB or MOD_AMT parameters change (not every buffer)
    // This is a critical performance optimization.
    (void)lfo_delay;  // Used in UpdateLfoSettings()
    (void)lfo_wave;   // Used in UpdateLfoSettings()
    
    // Apply VCF filter type (12dB or 24dB slope) - Jupiter-8 switchable
    // 0-49 = 12dB/oct (2-pole), 50-100 = 24dB/oct (4-pole)
    // NOTE: Set mode ONCE per buffer, not per-sample (critical optimization)
    const dsp::JupiterVCF::Mode vcf_mode = (vcf_type < 50) 
        ? dsp::JupiterVCF::MODE_LP12 
        : dsp::JupiterVCF::MODE_LP24;
    vcf_->SetMode(vcf_mode);
    
    // Pre-calculate HPF coefficient ONCE per buffer (not per-sample)
    // HPF cutoff (non-resonant high-pass before low-pass)
    // Jupiter-8 has fixed HPF, we make it adjustable
    float hpf_alpha = 0.0f;  // 0 = bypass HPF
    if (hpf_cutoff > 0) {
        const float hpf_freq = 20.0f + (hpf_cutoff / 100.0f) * 1980.0f;  // 20Hz-2kHz
        const float rc = 1.0f / (2.0f * M_PI * hpf_freq);
        const float dt = 1.0f / sample_rate_;
        hpf_alpha = rc / (rc + dt);
    }
    
    // Keyboard tracking from KEYFLW parameter
    // Jupiter-8 spec: 0-120% range, with slider downward = reduced tracking
    // Map 0-100 parameter to 0-1.2 multiplier (100 = 1.2 = 120%)
    const float key_track = (current_preset_.params[PARAM_VCF_KEYFLW] / 100.0f) * 1.2f;
    
    // Velocity modulation (fixed value for now, TODO: make parameter)
    const float vel_mod = (current_velocity_ / 127.0f) * 0.5f;  // Fixed 50% velocity->VCF
    
    // Task 2.2.4: Process portamento/glide for MONO/UNISON modes
    // For polyphonic mode, glide is processed per-voice in the render loop
    // Use the correct glide increment calculated in voice allocator
    if (current_mode_ == dsp::SYNTH_MODE_MONOPHONIC || 
        current_mode_ == dsp::SYNTH_MODE_UNISON) {
        dsp::Voice& voice0 = allocator_.GetVoiceMutable(0);
        if (voice0.is_gliding) {
            // Use the pre-calculated glide increment from voice allocator
            // This ensures constant speed glide (not slowing down near target)
            float log_pitch = logf(voice0.pitch_hz);
            log_pitch += voice0.glide_increment;
            
            // Check if we've reached target
            float log_target = logf(voice0.glide_target_hz);
            if ((voice0.glide_increment > 0.0f && log_pitch >= log_target) ||
                (voice0.glide_increment < 0.0f && log_pitch <= log_target)) {
                voice0.pitch_hz = voice0.glide_target_hz;
                voice0.is_gliding = false;
            } else {
                voice0.pitch_hz = expf(log_pitch);
            }
            
            // Update current_freq_hz_ for MONO/UNISON rendering
            current_freq_hz_ = voice0.pitch_hz;
        }
    }
    
    // ============ Main DSP loop - render to mix_buffer_ ============
    #ifdef PERF_MON
    PERF_MON_START(perf_render_total_);
    #endif
    
    for (uint32_t i = 0; i < frames; ++i) {
        
        // Process envelopes FIRST (needed for LFO rate modulation)
        vcf_env_out_ = env_vcf_->Process();
        vca_env_out_ = env_vca_->Process();
        
        // Process LFO with optional envelope rate modulation
        // ENV→LFO modulates LFO frequency: 0% = no change, 100% = double rate at peak
        if (lfo_env_amt > kMinModulation) {
            // Temporarily boost LFO rate based on envelope
            // Store current rate, modulate, process, restore
            // This avoids permanently changing the LFO rate parameter
            const float rate_mult = 1.0f + (vcf_env_out_ * lfo_env_amt);  // 1.0-2.0x range
            // Note: LFO doesn't have SetRateMultiplier, so apply via temporary frequency scaling
            // For now, LFO output will be modulated in amplitude instead
            lfo_out_ = lfo_->Process() * rate_mult;  // Amplitude modulation approximation
        } else {
            lfo_out_ = lfo_->Process();
        }
        
        // Get smoothed oscillator levels
        const float dco1_level = dco1_level_smooth_->Process();
        const float dco2_level = dco2_level_smooth_->Process();
        
        // Apply PWM modulation (LFO and ENV to pulse width)
        // Base pulse width from parameter
        float base_pw = current_preset_.params[PARAM_DCO1_PW] / 100.0f;
        
        // Add LFO→PWM modulation
        float pw_mod = 0.0f;
        if (lfo_pwm_depth > kMinModulation) {
            pw_mod += lfo_out_ * lfo_pwm_depth * 0.4f;  // ±40% PWM range
        }
        
        // Add ENV→PWM modulation
        if (env_pwm_depth > kMinModulation) {
            pw_mod += vcf_env_out_ * env_pwm_depth * 0.4f;  // ±40% PWM range
        }
        
        // Apply modulated pulse width, clamped to safe range (10%-90%)
        float modulated_pw = clampf(base_pw + pw_mod, 0.1f, 0.9f);
        
#ifdef DEBUG
        static uint32_t debug_frame_counter = 0;
        bool should_debug = (debug_frame_counter++ % 4800 == 0); // Every 100ms at 48kHz
#endif
        
        // === MODE-SPECIFIC OSCILLATOR PROCESSING ===
        #ifdef PERF_MON
        PERF_MON_START(perf_voice_alloc_);
        #endif
        
        #ifdef PERF_MON
        PERF_MON_END(perf_voice_alloc_);
        PERF_MON_START(perf_dco_);
        #endif
        
        // Pre-calculate pitch envelope modulation ratio (used in all modes)
        float pitch_mod_ratio = 1.0f;
        if (fabsf(env_pitch_depth) > kMinModulation) {
            pitch_mod_ratio = powf(2.0f, vcf_env_out_ * env_pitch_depth / 12.0f);
        }
        
        if (current_mode_ == dsp::SYNTH_MODE_POLYPHONIC) {
            // POLYPHONIC MODE: Render and mix multiple independent voices
            mixed_ = 0.0f;
            uint8_t active_voice_count = 0;
            
            // Render each active voice
            for (uint8_t v = 0; v < DRUPITER_MAX_VOICES; v++) {
                const dsp::Voice& voice = allocator_.GetVoice(v);
                
                // Skip inactive voices (no note or envelope finished)
                if (!voice.active && !voice.env_amp.IsActive()) {
                    continue;
                }
                
                active_voice_count++;
                
#ifdef DEBUG
                if (should_debug) {
                    fprintf(stderr, "[POLY] Voice %d: active=%d note=%d freq=%.2f Hz env_active=%d\n",
                            v, voice.active, voice.midi_note, voice.pitch_hz, voice.env_amp.IsActive());
                    fflush(stderr);
                }
#endif
                
                // Get non-const access to voice for processing
                dsp::Voice& voice_mut = const_cast<dsp::Voice&>(voice);
                
                // Task 2.2.4: Process portamento/glide
                // Use the pre-calculated glide increment from voice allocator
                // This ensures constant speed glide (not slowing down near target)
                if (voice_mut.is_gliding) {
                    // Use the pre-calculated glide increment from voice allocator
                    float log_pitch = logf(voice_mut.pitch_hz);
                    log_pitch += voice_mut.glide_increment;
                    
                    // Check if we've reached target
                    float log_target = logf(voice_mut.glide_target_hz);
                    if ((voice_mut.glide_increment > 0.0f && log_pitch >= log_target) ||
                        (voice_mut.glide_increment < 0.0f && log_pitch <= log_target)) {
                        voice_mut.pitch_hz = voice_mut.glide_target_hz;
                        voice_mut.is_gliding = false;
                    } else {
                        voice_mut.pitch_hz = expf(log_pitch);
                    }
                }
                
                // Set voice-specific parameters
                voice_mut.dco1.SetWaveform(static_cast<dsp::JupiterDCO::Waveform>(
                    current_preset_.params[PARAM_DCO1_WAVE]));
                voice_mut.dco2.SetWaveform(static_cast<dsp::JupiterDCO::Waveform>(
                    current_preset_.params[PARAM_DCO2_WAVE]));
                voice_mut.dco1.SetPulseWidth(modulated_pw);
                voice_mut.dco2.SetPulseWidth(modulated_pw);
                
                // Calculate frequencies for this voice
                float voice_freq1 = voice.pitch_hz * dco1_oct_mult;
                float voice_freq2 = voice.pitch_hz * dco2_oct_mult * detune_ratio;
                
                // Apply LFO vibrato
                if (lfo_vco_depth > kMinModulation) {
                    const float lfo_mod = 1.0f + lfo_out_ * lfo_vco_depth * 0.05f;
                    voice_freq1 *= lfo_mod;
                    voice_freq2 *= lfo_mod;
                }
                
                // Apply pitch envelope modulation (Task 2.2.1: Per-voice pitch envelope)
                if (pitch_mod_ratio != 1.0f) {
                    // Use per-voice pitch envelope for independent pitch modulation
                    const float voice_env_pitch = voice_mut.env_pitch.Process();
                    // Use faster pow2 approximation instead of expensive powf
                    const float voice_pitch_ratio = fasterpow2f(voice_env_pitch * env_pitch_depth / 12.0f);

                    // NEON-optimized: multiply both frequencies by ratio simultaneously
#ifdef USE_NEON
                    float32x2_t freq_pair = vld1_f32(&voice_freq1);  // Load freq1, freq2
                    float32x2_t ratio_vec = vdup_n_f32(voice_pitch_ratio);  // Duplicate ratio
                    freq_pair = vmul_f32(freq_pair, ratio_vec);  // Multiply both
                    vst1_f32(&voice_freq1, freq_pair);  // Store back
#else
                    voice_freq1 *= voice_pitch_ratio;
                    voice_freq2 *= voice_pitch_ratio;
#endif
                }
                
                voice_mut.dco1.SetFrequency(voice_freq1);
                voice_mut.dco2.SetFrequency(voice_freq2);
                
#ifdef DEBUG
                if (should_debug) {
                    fprintf(stderr, "[POLY] Voice %d: freq1=%.2f freq2=%.2f waveform=%d\n",
                            v, voice_freq1, voice_freq2, current_preset_.params[PARAM_DCO1_WAVE]);
                    fflush(stderr);
                }
#endif
                
                // Process voice oscillators
                float voice_dco1 = voice_mut.dco1.Process();
                float voice_dco2 = 0.0f;
                
                if (dco2_level > kMinModulation) {
                    voice_dco2 = voice_mut.dco2.Process();
                }
                
                // Mix this voice's oscillators
                float voice_mix = voice_dco1 * dco1_level + voice_dco2 * dco2_level;
                
#ifdef DEBUG
                if (should_debug) {
                    fprintf(stderr, "[POLY] Voice %d: dco1=%.3f dco2=%.3f mix=%.3f levels=(%.3f,%.3f)\n",
                            v, voice_dco1, voice_dco2, voice_mix, dco1_level, dco2_level);
                    fflush(stderr);
                }
#endif
                
                // Process voice envelope (each voice has its own envelope)
                float voice_env = voice_mut.env_amp.Process();
                
#ifdef DEBUG
                if (should_debug) {
                    fprintf(stderr, "[POLY] Voice %d: voice_env=%.3f\n", v, voice_env);
                    fflush(stderr);
                }
#endif
                
                // Apply envelope and add to mix
                mixed_ += voice_mix * voice_env;
            }
            
            // Scale by voice count to prevent clipping
            if (active_voice_count > 0) {
                mixed_ /= sqrtf(static_cast<float>(active_voice_count));
            }
            
        } else if (current_mode_ == dsp::SYNTH_MODE_UNISON) {
            // UNISON MODE: Use UnisonOscillator for multi-voice detuned stack + DCO2
            dsp::UnisonOscillator& unison_osc = allocator_.GetUnisonOscillator();
            
            // Set waveform and pulse width (same as DCO1)
            unison_osc.SetWaveform(static_cast<dsp::JupiterDCO::Waveform>(
                current_preset_.params[PARAM_DCO1_WAVE]));
            unison_osc.SetPulseWidth(modulated_pw);
            
            // Calculate frequency with LFO modulation
            float unison_freq = current_freq_hz_ * dco1_oct_mult;
            if (lfo_vco_depth > kMinModulation) {
                const float lfo_mod = 1.0f + lfo_out_ * lfo_vco_depth * 0.05f;
                unison_freq *= lfo_mod;
            }
            
            // Apply pitch envelope modulation (pre-calculated)
            unison_freq *= pitch_mod_ratio;
            
            unison_osc.SetFrequency(unison_freq);
            
            // Process stereo unison output
            float unison_left, unison_right;
            unison_osc.Process(&unison_left, &unison_right);
            
            // Average L+R for unison mono signal
            float unison_mono = (unison_left + unison_right) * 0.5f;
            
            // Also process DCO2 (like in MONO mode)
            dco2_->SetPulseWidth(modulated_pw);
            float freq2 = current_freq_hz_ * dco2_oct_mult * detune_ratio;
            if (lfo_vco_depth > kMinModulation) {
                const float lfo_mod = 1.0f + lfo_out_ * lfo_vco_depth * 0.05f;
                freq2 *= lfo_mod;
            }
            
            // Apply pitch envelope modulation to DCO2 (pre-calculated)
            freq2 *= pitch_mod_ratio;
            
            dco2_->SetFrequency(freq2);
            dco2_out_ = dco2_->Process();
            
            // Mix unison stack with DCO2
            mixed_ = unison_mono * dco1_level + dco2_out_ * dco2_level;
        } else {
            // MONO MODE: Use main synth DCOs (monophonic, single voice)
            dco1_->SetPulseWidth(modulated_pw);
            dco2_->SetPulseWidth(modulated_pw);  // Both DCOs share PWM
            
            // Calculate DCO frequencies with LFO modulation
            float freq1 = current_freq_hz_ * dco1_oct_mult;
            float freq2 = current_freq_hz_ * dco2_oct_mult * detune_ratio;
        
            // Apply LFO modulation to frequencies (vibrato)
            if (lfo_vco_depth > kMinModulation) {
                const float lfo_mod = 1.0f + lfo_out_ * lfo_vco_depth * 0.05f;  // ±5% vibrato
                freq1 *= lfo_mod;
                freq2 *= lfo_mod;
            }
            
            // Apply pitch envelope modulation (pre-calculated)
            freq1 *= pitch_mod_ratio;
            freq2 *= pitch_mod_ratio;
            
            dco1_->SetFrequency(freq1);
            dco2_->SetFrequency(freq2);
            
            // Only process DCO2 if it's audible (level > 0) or needed for XMOD
            const bool dco2_needed = (dco2_level > kMinModulation) || (xmod_depth > kMinModulation);
            
            if (dco2_needed) {
                // Process DCO2 first to get fresh output for FM
                dco2_out_ = dco2_->Process();
                
                // Cross-modulation (DCO2 -> DCO1 FM) - Jupiter-8 style
                // Only apply FM if XMOD depth is significant
                if (xmod_depth > kMinModulation) {
                    // Scale: 100% XMOD = ±1 semitone = ±1/12 octave
                    dco1_->ApplyFM(dco2_out_ * xmod_depth * 0.083f);
                } else {
                    dco1_->ApplyFM(0.0f);
                }
            } else {
                // DCO2 not needed - silence it and ensure no FM
                dco2_out_ = 0.0f;
                dco1_->ApplyFM(0.0f);
            }
            
            // Process DCO1 (optionally modulated by DCO2)
            dco1_out_ = dco1_->Process();
            
            // Note: Sync disabled when XMOD is active
            // Jupiter-8 doesn't support sync+xmod simultaneously due to processing order
            // (DCO2 must be processed first for FM, breaking sync master/slave relationship)
            
            // Mix oscillators with smoothed levels
            mixed_ = dco1_out_ * dco1_level + dco2_out_ * dco2_level;
        }
        // === END MODE-SPECIFIC PROCESSING ===
        
#ifdef DEBUG
        if (should_debug && current_mode_ == dsp::SYNTH_MODE_POLYPHONIC) {
            fprintf(stderr, "[POLY] After mixing: mixed_=%.3f\n", mixed_);
            fflush(stderr);
        }
#endif
        
        // Soft clamp mixed oscillators to prevent clipping (both at 100% = potential ±2.0)
        if (mixed_ > 1.2f) mixed_ = 1.2f + 0.5f * (mixed_ - 1.2f);
        else if (mixed_ < -1.2f) mixed_ = -1.2f + 0.5f * (mixed_ + 1.2f);
        
        // Apply HPF (high-pass filter) FIRST - Jupiter-8 signal flow
        // HPF comes before LPF in Jupiter-8 architecture
        // HPF coefficient (hpf_alpha) pre-calculated before loop for efficiency
        float hpf_out = mixed_;
        if (hpf_alpha > 0.0f) {
            // Simple one-pole HPF: y[n] = alpha * (y[n-1] + x[n] - x[n-1])
            hpf_out = hpf_alpha * (hpf_prev_output_ + mixed_ - hpf_prev_input_);
            hpf_prev_output_ = hpf_out;
            hpf_prev_input_ = mixed_;
            
            // Soft clamp HPF output to prevent filter overload
            if (hpf_out > 1.5f) hpf_out = 1.5f + 0.3f * (hpf_out - 1.5f);
            else if (hpf_out < -1.5f) hpf_out = -1.5f + 0.3f * (hpf_out + 1.5f);
        }
        
        // Apply filter with smoothed cutoff, envelope, and LFO modulation
        const float cutoff_norm = cutoff_smooth_->Process();
        
        // Jupiter-8 style cutoff mapping:
        // - At 0%: ~100Hz (closed, but still audible/usable)
        // - At 50%: ~1kHz (typical starting point)
        // - At 100%: ~12kHz (fully open)
        // Use a combination of linear and exponential mapping for musical control
        // 
        // Map 0-1 to cover ~7 octaves (100Hz to 12.8kHz)
        // Linear portion at low end prevents the filter from getting "stuck" closed
        const float linear_blend = 0.15f;  // 15% linear, 85% exponential
        const float exp_portion = fast_pow2(cutoff_norm * 7.0f);  // 2^7 = 128x range
        const float lin_portion = 1.0f + cutoff_norm * 127.0f;     // 1 to 128 linear
        const float cutoff_mult = linear_blend * lin_portion + (1.0f - linear_blend) * exp_portion;
        float cutoff_base = 100.0f * cutoff_mult;  // 100Hz * multiplier

        // Keyboard tracking (from MOD HUB)
        const float note_offset = (static_cast<int32_t>(current_note_) - 60) / 12.0f;
        // Clamp tracking exponent to ±4 octaves to prevent numerical issues
        const float tracking_exponent = note_offset * key_track;
        const float clamped_exponent = (tracking_exponent > 4.0f) ? 4.0f : 
                                      (tracking_exponent < -4.0f) ? -4.0f : tracking_exponent;
        cutoff_base *= semitones_to_ratio(clamped_exponent * 12.0f);  // Fast approx
        
        // Combine envelope, LFO, velocity, and hub modulation
        float total_mod = vcf_env_out_ * 2.0f              // Base envelope modulation
                        + env_vcf_depth * vcf_env_out_     // Hub envelope->VCF modulation
                        + lfo_out_ * lfo_vcf_depth * 1.0f  // LFO modulation
                        + vel_mod * 2.0f;                   // Velocity adds up to +2 octaves

        // Clamp modulation depth to avoid extreme cutoff values.
        if (total_mod > 3.0f) total_mod = 3.0f;
        else if (total_mod < -3.0f) total_mod = -3.0f;
        
        // Use fast 2^x approximation for all modulation values
        // fast_pow2 is accurate enough for audio-rate modulation
        float cutoff_modulated = cutoff_base * fast_pow2(total_mod);
        
        // Only update filter coefficients if cutoff changed significantly (optimization)
        const float cutoff_diff = cutoff_modulated - last_cutoff_hz_;
        if (cutoff_diff > 1.0f || cutoff_diff < -1.0f) {
            vcf_->SetCutoffModulated(cutoff_modulated);
            last_cutoff_hz_ = cutoff_modulated;
        }
        
        // Filter mode already set before loop (vcf_mode)
        
        // Bypass VCF when cutoff is at maximum (100) to avoid filter ringing artifacts
        if (current_preset_.params[PARAM_VCF_CUTOFF] >= 100) {
            filtered_ = hpf_out;  // Bypass LPF, but keep HPF processing
        } else {
            #ifdef PERF_MON
            PERF_MON_END(perf_dco_);
            PERF_MON_START(perf_vcf_);
            #endif
            
            filtered_ = vcf_->Process(hpf_out);  // Process HPF output through LPF
            
            #ifdef PERF_MON
            PERF_MON_END(perf_vcf_);
            PERF_MON_START(perf_effects_);
            #endif
            
            // Soft clip filter output to prevent resonance spikes
            if (filtered_ > 1.8f) filtered_ = 1.8f + 0.2f * (filtered_ - 1.8f);
            else if (filtered_ < -1.8f) filtered_ = -1.8f + 0.2f * (filtered_ + 1.8f);
        }
        
        // Apply VCA with envelope, LFO tremolo, keyboard tracking, and level control
        // Base VCA envelope
        // In POLY mode, voices already rendered with their individual envelopes - no additional processing needed
        // In UNISON mode, use voice 0's envelope and let it complete its release
        // In MONO mode, use main env_vca_
        float vca_gain = vca_env_out_;
        if (current_mode_ == dsp::SYNTH_MODE_UNISON) {
            // In UNISON mode: All unison voices share the same envelope state
            // CRITICAL: Always process envelope (even during release) to allow proper note-off
            // Don't check IsAnyVoiceActive() - let envelope complete its full ADSR cycle
            dsp::Voice& lead_voice = allocator_.GetVoiceMutable(0);
            vca_gain = lead_voice.env_amp.Process();
            
            // Only check envelope state (not voice active flag) to detect true silence
            if (!lead_voice.env_amp.IsActive()) {
                vca_gain = 0.0f;  // Envelope fully released = silence
            }
        } else if (current_mode_ == dsp::SYNTH_MODE_POLYPHONIC) {
            // In POLY mode, voices have already been rendered with their individual envelopes
            // Each voice applied its own envelope at line 473, so no additional envelope processing here
            // Just use base VCA gain of 1.0 for global modulation (tremolo, keyboard tracking)
            vca_gain = 1.0f;
        }
        
        // VCA LFO (tremolo): 0% = no tremolo, 100% = full amplitude modulation
        if (vca_lfo_depth > kMinModulation) {
            // Bipolar LFO (-1 to +1) -> unipolar tremolo (0.5 to 1.5)
            const float tremolo = 1.0f + (lfo_out_ * vca_lfo_depth * 0.5f);
            vca_gain *= tremolo;
        }
        
        // VCA Keyboard tracking: higher notes louder
        // 0% = no tracking, 100% = +6dB per octave above C4
        if (vca_kybd > kMinModulation) {
            const float note_offset = (static_cast<int32_t>(current_note_) - 60) / 12.0f;  // Octaves from C4
            const float kb_gain = 1.0f + (note_offset * vca_kybd * 0.5f);  // +50% per octave at 100%
            vca_gain *= kb_gain;
        }
        
        // VCA Level control (master output scaling)
        vca_gain *= vca_level;
        
        // Apply VCA with extra headroom to prevent clipping (0.6f base scaling)
        mix_buffer_[i] = filtered_ * vca_gain * 0.6f;
        
#ifdef DEBUG
        static int debug_output_counter = 0;
        
        // Periodic output (once per second)
        if (++debug_output_counter >= 48000 && i == 0) {  // Log once per second, first frame only
            debug_output_counter = 0;
            fprintf(stderr, "[Synth Output] DCO1=%.6f, mixed=%.6f, filtered=%.6f, env=%.6f, final=%.6f\n",
                    dco1_out_, mixed_, filtered_, vca_env_out_, mix_buffer_[i]);
            fflush(stderr);
        }
#endif
    }
    
    #ifdef PERF_MON
    PERF_MON_END(perf_effects_);
    #endif
    
    // ============ Output stage with chorus/space effect ============
    
    #ifdef PERF_MON
    PERF_MON_START(perf_effects_);
    #endif
    
    // Sanitize buffer (remove NaN/Inf) and apply soft clamp
    drupiter::neon::SanitizeAndClamp(mix_buffer_, 1.0f, frames);
    
    // Check if effect processor is initialized
    if (!space_widener_) {
        // Fallback: bypass effect, copy mono to stereo
        for (uint32_t i = 0; i < frames; i++) {
            left_buffer_[i] = mix_buffer_[i];
            right_buffer_[i] = mix_buffer_[i];
        }
    } else {
        // Effect mode: 0=Chorus, 1=Space, 2=Dry, 3=Both
        // Effect parameters are now configured in UpdateEffectParameters() on change,
        // not every buffer (critical optimization)
        const uint8_t effect_mode = current_preset_.params[PARAM_EFFECT];
    
        if (effect_mode == 2) {
            // DRY mode: Bypass all effects, just copy mono to stereo
            for (uint32_t i = 0; i < frames; i++) {
                left_buffer_[i] = mix_buffer_[i];
                right_buffer_[i] = mix_buffer_[i];
            }
        } else {
            // CHORUS/SPACE/BOTH: Process with pre-configured parameters
            space_widener_->ProcessMonoBatch(mix_buffer_, left_buffer_, right_buffer_, frames);
        }
    }
    
    // Add tiny DC offset for denormal protection before interleaving
    const float denormal_offset = 1.0e-15f;
    drupiter::neon::ApplyGain(left_buffer_, 1.0f, frames);  // Could add offset here if needed
    drupiter::neon::ApplyGain(right_buffer_, 1.0f, frames);  // Could add offset here if needed
    
    // Use neon_dsp InterleaveStereo function for optimized stereo interleaving
    drupiter::neon::InterleaveStereo(left_buffer_, right_buffer_, out, frames);
    
    // Add DC offset to output (simpler than modifying buffers)
    for (uint32_t i = 0; i < frames * 2; ++i) {
        out[i] += denormal_offset;
    }
    
    #ifdef PERF_MON
    PERF_MON_END(perf_effects_);
    PERF_MON_END(perf_render_total_);
    #endif
}

void DrupiterSynth::SetParameter(uint8_t id, int32_t value) {
    if (id >= PARAM_COUNT) {
        return;
    }

    // Clamp to expected ranges based on parameter type
    uint8_t v = 0;
    switch (id) {
        // Page 1: DCO-1 discrete params
        case PARAM_DCO1_OCT:
            v = clamp_u8_int32(value, 0, 2);
            break;
        case PARAM_DCO1_WAVE:
            v = clamp_u8_int32(value, 0, 4);  // 0-4: SAW/SQR/PUL/TRI/SAW_PWM
            break;
            
        // Page 2: DCO-2 discrete params
        case PARAM_DCO2_OCT:
            v = clamp_u8_int32(value, 0, 2);
            break;
        case PARAM_DCO2_WAVE:
            v = clamp_u8_int32(value, 0, 4);  // 0-4: SAW/NSE/PUL/SIN/SAW_PWM
            break;
        case PARAM_SYNC:
            v = clamp_u8_int32(value, 0, 2);
            break;
            
        // Page 6: MOD HUB selector and EFFECT mode
        case PARAM_MOD_HUB:
            v = clamp_u8_int32(value, 0, MOD_NUM_DESTINATIONS - 1);  // 14 hub destinations (0-17)
            mod_hub_.SetDestination(v);
            
            // Restore the previously stored value for this destination
            // This allows switching between MOD HUB options and remembering each value
            if (v < MOD_NUM_DESTINATIONS) {
                mod_hub_.SetValue(current_preset_.hub_values[v]);
            }
            
            current_preset_.params[id] = v;
            return;  // Hub handles its own state
            
        case PARAM_MOD_AMT:
            // Hub amount: Store in hub and preset's hub_values array
            v = clamp_u8_int32(value, 0, 100);
            mod_hub_.SetValue(v);  // Hub will clamp to destination's actual range
            
            // Store the ORIGINAL UI value (0-100), not the clamped value
            // This is critical for proper restoration when switching destinations
            {
                uint8_t dest = current_preset_.params[PARAM_MOD_HUB];
                if (dest < MOD_NUM_DESTINATIONS) {
                    // Store original 0-100 value for restoration later
                    current_preset_.hub_values[dest] = v;  // Store original, not clamped
                    
                    // Get the actual clamped value from the hub for DSP use
                    int32_t actual_value = mod_hub_.GetValue(dest);
                    
                    // Apply specific destinations to DSP components immediately
                    switch (dest) {
                        case MOD_SYNTH_MODE:  // S MODE (range 0-2: MONO/POLY/UNISON)
                            if (actual_value <= 2) {
                                // Only allow mode changes when no notes are playing
                                // to prevent audio glitches and envelope state issues
                                if (!allocator_.IsAnyVoiceActive()) {
                                    current_mode_ = static_cast<dsp::SynthMode>(actual_value);
                                    allocator_.SetMode(current_mode_);
                                }
                                // If voices are active, mode change will apply after all notes off
                            }
                            break;
                        case MOD_UNISON_DETUNE:  // UNI DET (range 0-50 cents)
                            // Update unison detune with actual clamped value
                            allocator_.SetUnisonDetune(actual_value);
                            break;
                        default:
                            break;  // Other destinations handled in Render()
                    }
                }
            }
            return;  // Hub handles its own state
            
        case PARAM_EFFECT:
            v = clamp_u8_int32(value, 0, 3);  // 0=CHORUS, 1=SPACE, 2=DRY, 3=BOTH
            break;
            
        default:
            // All other params are 0-100
            v = clamp_u8_int32(value, 0, 100);
            break;
    }
    current_preset_.params[id] = v;
    
    // Update DSP components based on parameter changes
    switch (id) {
        // ======== Page 1: DCO-1 ========
        case PARAM_DCO1_OCT:
            // Octave handled in Render()
            break;
        case PARAM_DCO1_WAVE:
            switch (v) {
                case 0: dco1_->SetWaveform(dsp::JupiterDCO::WAVEFORM_SAW); break;
                case 1: dco1_->SetWaveform(dsp::JupiterDCO::WAVEFORM_SQUARE); break;
                case 2: dco1_->SetWaveform(dsp::JupiterDCO::WAVEFORM_PULSE); break;
                case 3: dco1_->SetWaveform(dsp::JupiterDCO::WAVEFORM_TRIANGLE); break;
                case 4: dco1_->SetWaveform(dsp::JupiterDCO::WAVEFORM_SAW_PWM); break;  // Hoover v2.0!
            }
            break;
        case PARAM_DCO1_PW:
            dco1_->SetPulseWidth(v / 100.0f);
            break;
        case PARAM_XMOD:
            xmod_depth_ = v / 100.0f;
            break;
            
        // ======== Page 2: DCO-2 ========
        case PARAM_DCO2_OCT:
            // Octave handled in Render()
            break;
        case PARAM_DCO2_WAVE:
            switch (v) {
                case 0: dco2_->SetWaveform(dsp::JupiterDCO::WAVEFORM_SAW); break;
                case 1: dco2_->SetWaveform(dsp::JupiterDCO::WAVEFORM_NOISE); break;
                case 2: dco2_->SetWaveform(dsp::JupiterDCO::WAVEFORM_PULSE); break;
                case 3: dco2_->SetWaveform(dsp::JupiterDCO::WAVEFORM_SINE); break;
                case 4: dco2_->SetWaveform(dsp::JupiterDCO::WAVEFORM_SAW_PWM); break;  // Hoover v2.0!
            }
            break;
        case PARAM_DCO2_TUNE:
            // Detune handled in Render()
            break;
        case PARAM_SYNC:
            sync_mode_ = v;
            break;
            
        // ======== Page 3: MIX & VCF ========
        case PARAM_OSC_MIX:
            // Mix handled in Render() with smoothing
            break;
        case PARAM_VCF_CUTOFF:
            if (cutoff_smooth_) {
                cutoff_smooth_->SetTarget(v / 100.0f);
            }
            break;
        case PARAM_VCF_RESONANCE:
            vcf_->SetResonance(v / 100.0f);
            break;
        case PARAM_VCF_KEYFLW:
            // Keyboard tracking handled in Render()
            break;
        
        // ======== Page 4: VCF Envelope ========
        case PARAM_VCF_ATTACK:
            env_vcf_->SetAttack(ParameterToEnvelopeTime(v));
            // Task 2.2.1: Also set per-voice pitch envelopes
            for (uint8_t i = 0; i < DRUPITER_MAX_VOICES; i++) {
                allocator_.GetVoiceMutable(i).env_pitch.SetAttack(ParameterToEnvelopeTime(v));
            }
            break;
        case PARAM_VCF_DECAY:
            env_vcf_->SetDecay(ParameterToEnvelopeTime(v));
            for (uint8_t i = 0; i < DRUPITER_MAX_VOICES; i++) {
                allocator_.GetVoiceMutable(i).env_pitch.SetDecay(ParameterToEnvelopeTime(v));
            }
            break;
        case PARAM_VCF_SUSTAIN:
            env_vcf_->SetSustain(v / 100.0f);
            for (uint8_t i = 0; i < DRUPITER_MAX_VOICES; i++) {
                allocator_.GetVoiceMutable(i).env_pitch.SetSustain(v / 100.0f);
            }
            break;
        case PARAM_VCF_RELEASE:
            env_vcf_->SetRelease(ParameterToEnvelopeTime(v));
            for (uint8_t i = 0; i < DRUPITER_MAX_VOICES; i++) {
                allocator_.GetVoiceMutable(i).env_pitch.SetRelease(ParameterToEnvelopeTime(v));
            }
            break;
        
        // ======== Page 5: VCA Envelope ========
        case PARAM_VCA_ATTACK:
#ifdef DEBUG
            fprintf(stderr, "[Param] VCA_ATTACK value=%d -> time=%.6fs\n", v, ParameterToEnvelopeTime(v));
            fflush(stderr);
#endif
            env_vca_->SetAttack(ParameterToEnvelopeTime(v));
            // Update all polyphonic voice envelopes
            for (uint8_t i = 0; i < DRUPITER_MAX_VOICES; i++) {
                allocator_.GetVoiceMutable(i).env_amp.SetAttack(ParameterToEnvelopeTime(v));
            }
            break;
        case PARAM_VCA_DECAY:
#ifdef DEBUG
            fprintf(stderr, "[Param] VCA_DECAY value=%d -> time=%.6fs\n", v, ParameterToEnvelopeTime(v));
            fflush(stderr);
#endif
            env_vca_->SetDecay(ParameterToEnvelopeTime(v));
            // Update all polyphonic voice envelopes
            for (uint8_t i = 0; i < DRUPITER_MAX_VOICES; i++) {
                allocator_.GetVoiceMutable(i).env_amp.SetDecay(ParameterToEnvelopeTime(v));
            }
            break;
        case PARAM_VCA_SUSTAIN:
            env_vca_->SetSustain(v / 100.0f);
            // Update all polyphonic voice envelopes
            for (uint8_t i = 0; i < DRUPITER_MAX_VOICES; i++) {
                allocator_.GetVoiceMutable(i).env_amp.SetSustain(v / 100.0f);
            }
            break;
        case PARAM_VCA_RELEASE:
#ifdef DEBUG
            fprintf(stderr, "[Param] VCA_RELEASE value=%d -> time=%.6fs\n", v, ParameterToEnvelopeTime(v));
            fflush(stderr);
#endif
            env_vca_->SetRelease(ParameterToEnvelopeTime(v));
            // Update all polyphonic voice envelopes
            for (uint8_t i = 0; i < DRUPITER_MAX_VOICES; i++) {
                allocator_.GetVoiceMutable(i).env_amp.SetRelease(ParameterToEnvelopeTime(v));
            }
            break;
        
        // ======== Page 6: LFO, MOD HUB & Effects ========
        case PARAM_LFO_RATE:
            lfo_->SetFrequency(ParameterToExponentialFreq(v, 0.1f, 20.0f));
            break;
        case PARAM_MOD_HUB:
        case PARAM_MOD_AMT:
            // Hub updates handled in Render() by reading hub values
            // Also update LFO settings if the hub affects LFO
            UpdateLfoSettings();
            break;
        case PARAM_EFFECT:
            // Effect mode changed - update effect routing
            UpdateEffectParameters(v);
            break;
    }
}

int32_t DrupiterSynth::GetParameter(uint8_t id) const {
    // Special handling for MOD HUB amount - return current destination's value
    if (id == PARAM_MOD_AMT) {
        uint8_t dest = current_preset_.params[PARAM_MOD_HUB];
        return mod_hub_.GetValue(dest);
    }
    
    if (id >= PARAM_COUNT) {
        return 0;
    }
    return current_preset_.params[id];
}

const char* DrupiterSynth::GetParameterStr(uint8_t id, int32_t value) {
    static const char* effect_names[] = {"CHORUS", "SPACE", "DRY", "BOTH"};
    
    // Separate static buffers for each parameter type to avoid race conditions
    static char tune_buf[16];      // For PARAM_DCO2_TUNE
    static char modamt_buf[16];    // For PARAM_MOD_AMT (fallback only - hub now returns stable pointers)
    
    switch (id) {
        // ======== Page 1: DCO-1 ========
        case PARAM_DCO1_OCT:
            return kOctave1Names[value < 3 ? value : 0];
        case PARAM_DCO1_WAVE:
            return kDco1WaveNames[value < 5 ? value : 0];  // 5 waveforms: SAW/SQR/PUL/TRI/SAW_PWM
            
        // ======== Page 2: DCO-2 ========
        case PARAM_DCO2_OCT:
            return kOctave2Names[value < 4 ? value : 0];
        case PARAM_DCO2_WAVE:
            return kDco2WaveNames[value < 5 ? value : 0];  // 5 waveforms: SAW/NSE/PUL/SIN/SAW_PWM
        case PARAM_DCO2_TUNE: {
            // Map 0-100 to -50 to +50 cents
            int32_t cents = value - 50;
            snprintf(tune_buf, sizeof(tune_buf), "%+d ct", static_cast<int>(cents));
            return tune_buf;
        }
        case PARAM_SYNC:
            return kSyncNames[value < 3 ? value : 0];
        
        // ======== Page 6: MOD HUB ========
        case PARAM_MOD_HUB:
            if (value >= 0 && value < MOD_NUM_DESTINATIONS) {
                return mod_hub_.GetDestinationName(value);  // Returns stable pointer
            }
            return "";
            
        case PARAM_MOD_AMT: {
            // Get current destination from mod_hub
            uint8_t dest = mod_hub_.GetDestination();
            
            if (dest >= MOD_NUM_DESTINATIONS) {
                return "";
            }
            
            // Get the CLAMPED value from hub (destination-specific range)
            // The raw slider value (0-100) must be converted to the destination's range
            int32_t clamped_value = mod_hub_.GetValue(dest);
            return mod_hub_.GetValueStringForDest(dest, clamped_value, modamt_buf, sizeof(modamt_buf));
        }
            
        case PARAM_EFFECT:
            return effect_names[value < 4 ? value : 0];
        
        default:
            return nullptr;
    }
}

void DrupiterSynth::SetHubValue(uint8_t destination, uint8_t value) {
    if (destination < MOD_NUM_DESTINATIONS) {
        mod_hub_.SetValueForDest(destination, value);
        // Also update the preset storage for consistency
        current_preset_.hub_values[destination] = value;
    }
}

void DrupiterSynth::NoteOn(uint8_t note, uint8_t velocity) {
#ifdef DEBUG
    static uint8_t last_note = 255;
    static int note_on_counter = 0;
    
    fprintf(stderr, "[Synth] NoteOn: note=%d velocity=%d mode=%d\n",
            note, velocity, (int)current_mode_);
    fflush(stderr);
    
    if (note == last_note) {
        note_on_counter++;
        if (note_on_counter % 100 == 0) {
            fprintf(stderr, "[Synth] NoteOn RETRIGGER! Same note %d called %d times\n", note, note_on_counter);
            fflush(stderr);
        }
    } else {
        if (note_on_counter > 1) {
            fprintf(stderr, "[Synth] Previous note %d was retriggered %d times\n", last_note, note_on_counter);
            fflush(stderr);
        }
        last_note = note;
        note_on_counter = 1;
    }
#endif

    // Route through voice allocator (Hoover v2.0)
    allocator_.NoteOn(note, velocity);
    
    // Update main synth DSP state (for all modes - mono uses dco1_/dco2_, unison uses UnisonOscillator)
    current_freq_hz_ = common::MidiHelper::NoteToFreq(note);
    current_velocity_ = velocity;
    
    // Trigger envelopes
    env_vca_->NoteOn();
    env_vcf_->NoteOn();
}

void DrupiterSynth::NoteOff(uint8_t note) {
#ifdef DEBUG
    fprintf(stderr, "[Synth] NoteOff: note=%d\n", note);
    fflush(stderr);
#endif
    
    // Route through voice allocator (Hoover v2.0)
    allocator_.NoteOff(note);
    
    // Trigger envelope release on main synth DSP
    env_vca_->NoteOff();
    env_vcf_->NoteOff();
}

void DrupiterSynth::AllNoteOff() {
    // Route through voice allocator (Hoover v2.0)
    allocator_.AllNotesOff();
}

void DrupiterSynth::LoadPreset(uint8_t preset_id) {
    if (preset_id >= 12) {
        preset_id = 0;
    }
    
    current_preset_idx_ = preset_id;
    
    // Load from factory presets (defined in presets.h)
    const auto& factory = presets::kFactoryPresets[preset_id];
    std::memcpy(current_preset_.params, factory.params, sizeof(current_preset_.params));
    std::memcpy(current_preset_.hub_values, factory.hub_values, sizeof(current_preset_.hub_values));
    std::strcpy(current_preset_.name, factory.name);
    
    // Set smoothed parameters immediately (no smoothing during preset load)
    if (cutoff_smooth_) {
        cutoff_smooth_->SetImmediate(current_preset_.params[PARAM_VCF_CUTOFF] / 100.0f);
    }
    
    // DCO levels from OSC_MIX parameter
    float osc_mix = current_preset_.params[PARAM_OSC_MIX] / 100.0f;
    if (dco1_level_smooth_) {
        dco1_level_smooth_->SetImmediate(1.0f - osc_mix);
    }
    if (dco2_level_smooth_) {
        dco2_level_smooth_->SetImmediate(osc_mix);
    }
    
    // Load XMOD and SYNC directly
    xmod_depth_ = current_preset_.params[PARAM_XMOD] / 100.0f;
    sync_mode_ = current_preset_.params[PARAM_SYNC];
    
    // Restore MOD HUB values from preset (source of truth for hub state)
    // Must be done BEFORE applying parameters
    mod_hub_.SetDestination(current_preset_.params[PARAM_MOD_HUB]);
    for (uint8_t dest = 0; dest < MOD_NUM_DESTINATIONS; ++dest) {
        mod_hub_.SetValueForDest(dest, current_preset_.hub_values[dest]);
    }
    
    // Apply all parameters to DSP components
    for (uint8_t i = 0; i < PARAM_COUNT; ++i) {
        SetParameter(i, current_preset_.params[i]);
    }
    
    // Initialize effect and LFO settings from preset (critical for preset load)
    UpdateEffectParameters(current_preset_.params[PARAM_EFFECT]);
    UpdateLfoSettings();
}

void DrupiterSynth::SavePreset(uint8_t preset_id) {
    // Note: drumlogue SDK doesn't support user preset storage
    // This is a placeholder for potential future implementation
    (void)preset_id;
}

float DrupiterSynth::Dco1OctaveToMultiplier(uint8_t octave_param) const {
    // DCO1 octave mapping: 0=16' (0.5x), 1=8' (1.0x), 2=4' (2.0x)
    switch (octave_param) {
        case 0: return 0.5f;   // 16'
        case 1: return 1.0f;   // 8'
        case 2: return 2.0f;   // 4'
        default: return 1.0f;  // Safe default
    }
}

float DrupiterSynth::Dco2OctaveToMultiplier(uint8_t octave_param) const {
    // DCO2 octave mapping (extended range): 0=32' (0.25x), 1=16' (0.5x), 2=8' (1.0x), 3=4' (2.0x)
    switch (octave_param) {
        case 0: return 0.25f;  // 32' (sub-octave)
        case 1: return 0.5f;   // 16'
        case 2: return 1.0f;   // 8'
        case 3: return 2.0f;   // 4'
        default: return 1.0f;  // Safe default
    }
}

float DrupiterSynth::GenerateNoise() {
    // Simple white noise generator
    noise_seed_ = (noise_seed_ * 1103515245 + 12345) & 0x7FFFFFFF;
    return (static_cast<float>(noise_seed_) / 2147483647.0f) * 2.0f - 1.0f;
}

float DrupiterSynth::ParameterToEnvelopeTime(uint8_t value) {
    // Quadratic scaling for envelope times (better control at low values)
    // 0 = instant (0.0001s min for stability), 32 = ~320ms, 64 = ~1.28s, 100 = 5s
    if (value == 0) {
        return 0.0001f;  // Nearly instant: 0.1ms = ~5 samples at 48kHz
    }
    float normalized = value / 100.0f;
    return 0.001f + normalized * normalized * 4.999f;
}

float DrupiterSynth::ParameterToExponentialFreq(uint8_t value, float min_freq, float max_freq) {
    // Quadratic scaling for frequency parameters
    float normalized = value / 100.0f;
    return min_freq + normalized * normalized * (max_freq - min_freq);
}

const char* DrupiterSynth::GetPresetName(uint8_t preset_id) const {
    if (preset_id >= 12) {
        return "Invalid";
    }
    return presets::kFactoryPresets[preset_id].name;
}

void DrupiterSynth::UpdateEffectParameters(uint8_t effect_mode) {
    // Configure effect parameters ONCE when mode changes (not every buffer)
    // This is a critical performance optimization
    if (!space_widener_) return;
    
    switch (effect_mode) {
        case 0:  // CHORUS mode: Moderate modulation depth, less delay time
            space_widener_->SetModDepth(3.0f);      // 3ms modulation depth
            space_widener_->SetMix(0.5f);           // 50% wet/dry mix
            space_widener_->SetDelayTime(15.0f);    // 15ms delay time
            break;
        case 1:  // SPACE mode: Wider stereo, more delay time
            space_widener_->SetModDepth(6.0f);      // 6ms modulation depth
            space_widener_->SetMix(0.7f);           // 70% wet/dry mix
            space_widener_->SetDelayTime(35.0f);    // 35ms delay time for wide stereo
            break;
        case 2:  // DRY mode: No effect processing (handled in Render)
            break;
        case 3:  // BOTH mode: Maximum effect (chorus + space combined)
            space_widener_->SetModDepth(8.0f);      // 8ms modulation depth
            space_widener_->SetMix(0.8f);           // 80% wet/dry mix
            space_widener_->SetDelayTime(40.0f);    // 40ms delay time
            break;
    }
}

void DrupiterSynth::UpdateLfoSettings() {
    // Update LFO settings when hub parameters change
    // Called from SetParameter when MOD_HUB or MOD_AMT changes
    if (!lfo_) return;
    
    // Apply LFO delay setting (0-100 maps to 0-5 seconds)
    const uint8_t lfo_delay = mod_hub_.GetValue(MOD_LFO_DELAY);
    const float lfo_delay_sec = (lfo_delay / 100.0f) * 5.0f;
    lfo_->SetDelay(lfo_delay_sec);
    
    // Apply LFO waveform (0-3: TRI/RAMP/SQR/S&H)
    const uint8_t lfo_wave = mod_hub_.GetValue(MOD_LFO_WAVE);
    lfo_->SetWaveform(static_cast<dsp::JupiterLFO::Waveform>(lfo_wave & 0x03));
}
