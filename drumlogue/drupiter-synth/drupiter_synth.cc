/**
 * @file drupiter_synth.cc
 * @brief Main synthesizer implementation for Drupiter
 *
 * Based on Bristol Jupiter-8 emulation architecture
 * Integrates DCO, VCF, Envelopes, and LFO
 */

#include "drupiter_synth.h"
#include "../common/hub_control.h"
#include "../common/param_format.h"
#include "dsp/jupiter_dco.h"
#include "dsp/jupiter_vcf.h"
#include "dsp/jupiter_env.h"
#include "dsp/jupiter_lfo.h"
#include "dsp/smoothed_value.h"
#include <cmath>
#include <cstring>
#include <cstdio>

#ifdef USE_NEON
#include <arm_neon.h>
#endif

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

namespace {

// Fast 2^x approximation (accurate for |x| < 8)
// Uses polynomial: 2^x ≈ 1 + 0.693x + 0.240x² + 0.056x³
// Relative error < 0.3% for |x| < 4
inline float fast_pow2(float x) {
    // Clamp to safe range
    if (x < -8.0f) return 0.00390625f;  // 2^-8
    if (x > 8.0f) return 256.0f;         // 2^8
    
    // Polynomial approximation (Horner's form)
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
    : dco1_(nullptr)
    , dco2_(nullptr)
    , vcf_(nullptr)
    , lfo_(nullptr)
    , env_vcf_(nullptr)
    , env_vca_(nullptr)
    , sample_rate_(48000.0f)
    , gate_(false)
    , current_note_(60)
    , current_velocity_(100)
    , current_freq_hz_(440.0f)
    , sync_mode_(0)
    , xmod_depth_(0.0f)
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
    , mod_hub_(kModDestinations)  // Initialize HubControl
    , effect_mode_(0)  // CHORUS by default
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
    
    // Initialize factory presets
    InitFactoryPresets();
    
    // Load init preset (this will set all parameters including smoothed values)
    LoadPreset(0);
    
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
    const float dco1_oct_mult = OctaveToMultiplier(current_preset_.params[PARAM_DCO1_OCT]);
    const float dco2_oct_mult = OctaveToMultiplier(current_preset_.params[PARAM_DCO2_OCT]);
    
    // Detune: convert cents to frequency ratio (from DCO2 TUNE param)
    // Map 0..100 (50=center) to ±200 cents (±2 semitones)
    const float detune_cents = static_cast<float>(current_preset_.params[PARAM_DCO2_TUNE]) * 4.0f;
    const float detune_ratio = cents_to_ratio(detune_cents);  // Fast approximation
    
    // Cross-modulation depth from direct XMOD param
    const float xmod_depth = xmod_depth_;  // Set via PARAM_XMOD in SetParameter
    
    // === Read MOD HUB values ===
    const float lfo_pwm_depth = mod_hub_.GetValue(MOD_LFO_TO_PWM) / 100.0f;
    const float lfo_vcf_depth = mod_hub_.GetValue(MOD_LFO_TO_VCF) / 100.0f;
    const float lfo_vco_depth = mod_hub_.GetValue(MOD_LFO_TO_VCO) / 100.0f;
    const float env_pwm_depth = mod_hub_.GetValue(MOD_ENV_TO_PWM) / 100.0f;
    // ENV→VCF is bipolar: 0-100 maps to -1.0 to +1.0 (50 = center/zero)
    const float env_vcf_depth = (static_cast<int32_t>(mod_hub_.GetValue(MOD_ENV_TO_VCF)) - 50) / 50.0f;
    const uint8_t hpf_cutoff = mod_hub_.GetValue(MOD_HPF);
    const uint8_t vcf_type = mod_hub_.GetValue(MOD_VCF_TYPE);
    const uint8_t lfo_delay = mod_hub_.GetValue(MOD_LFO_DELAY);
    const uint8_t lfo_wave = mod_hub_.GetValue(MOD_LFO_WAVE);
    const bool unison_enabled = (mod_hub_.GetValue(MOD_UNISON) > 0);
    
    // Apply LFO delay setting (0-100 maps to 0-5 seconds)
    const float lfo_delay_sec = (lfo_delay / 100.0f) * 5.0f;
    lfo_->SetDelay(lfo_delay_sec);
    
    // Apply LFO waveform (0-3: TRI/RAMP/SQR/S&H)
    lfo_->SetWaveform(static_cast<dsp::JupiterLFO::Waveform>(lfo_wave & 0x03));
    
    // Keyboard tracking from new KEYFLW parameter
    const float key_track = current_preset_.params[PARAM_VCF_KEYFLW] / 100.0f;
    
    // Velocity modulation (fixed value for now, TODO: make parameter)
    const float vel_mod = (current_velocity_ / 127.0f) * 0.5f;  // Fixed 50% velocity->VCF
    
    // ============ Main DSP loop - render to mix_buffer_ ============
    for (uint32_t i = 0; i < frames; ++i) {
        // Process LFO
        lfo_out_ = lfo_->Process();
        
        // Process envelopes
        vcf_env_out_ = env_vcf_->Process();
        vca_env_out_ = env_vca_->Process();
        
        // Get smoothed oscillator levels
        const float dco1_level = dco1_level_smooth_->Process();
        const float dco2_level = dco2_level_smooth_->Process();
        
        // Apply PWM modulation (LFO and ENV to pulse width)
        // Base pulse width from parameter
        float base_pw = current_preset_.params[PARAM_DCO1_PW] / 100.0f;
        
        // Add LFO→PWM modulation
        float pw_mod = 0.0f;
        if (lfo_pwm_depth > 0.001f) {
            pw_mod += lfo_out_ * lfo_pwm_depth * 0.4f;  // ±40% PWM range
        }
        
        // Add ENV→PWM modulation
        if (env_pwm_depth > 0.001f) {
            pw_mod += vcf_env_out_ * env_pwm_depth * 0.4f;  // ±40% PWM range
        }
        
        // Apply modulated pulse width, clamped to safe range (10%-90%)
        float modulated_pw = base_pw + pw_mod;
        if (modulated_pw < 0.1f) modulated_pw = 0.1f;
        if (modulated_pw > 0.9f) modulated_pw = 0.9f;
        dco1_->SetPulseWidth(modulated_pw);
        dco2_->SetPulseWidth(modulated_pw);  // Both DCOs share PWM
        
        // Calculate DCO frequencies with LFO modulation
        float freq1 = current_freq_hz_ * dco1_oct_mult;
        float freq2 = current_freq_hz_ * dco2_oct_mult * detune_ratio;
        
        // Apply LFO modulation to frequencies (vibrato)
        if (lfo_vco_depth > 0.001f) {
            const float lfo_mod = 1.0f + lfo_out_ * lfo_vco_depth * 0.05f;  // ±5% vibrato
            freq1 *= lfo_mod;
            freq2 *= lfo_mod;
        }
        
        dco1_->SetFrequency(freq1);
        dco2_->SetFrequency(freq2);
        
        // Cross-modulation (DCO2 -> DCO1 FM) - Jupiter-8 style
        // Uses direct XMOD parameter for control
        if (xmod_depth > 0.001f) {
            dco1_->ApplyFM(dco2_out_ * xmod_depth * 0.5f);
        } else {
            dco1_->ApplyFM(0.0f);
        }
        
        // Process DCO1 first (master for sync)
        dco1_out_ = dco1_->Process();
        
        // Hard sync: Reset DCO2 phase when DCO1 wraps
        // This is the classic Jupiter-8 sync sound
        if (sync_mode_ == 2 && dco1_->DidWrap()) {  // HARD sync
            dco2_->ResetPhase();
        }
        // Soft sync: Attenuate DCO2 when DCO1 wraps (gentler effect)
        else if (sync_mode_ == 1 && dco1_->DidWrap()) {  // SOFT sync
            // Soft sync gently pushes DCO2 toward phase reset
            // Creates a more subtle timbral variation
            float phase2 = dco2_->GetPhase();
            if (phase2 > 0.5f) {
                dco2_->ResetPhase();  // Only reset if past halfway
            }
        }

        // Process DCO2 (slave)
        dco2_out_ = dco2_->Process();
        
        // Mix oscillators with smoothed levels
        if (unison_enabled) {
            // Unison mode: Sum current voice with 3 additional detuned voices
            // This creates a thick, chorus-like sound
            // Base voice (no detune)
            mixed_ = dco1_out_ * dco1_level + dco2_out_ * dco2_level;
            
            // Voice 2: +10 cents detune
            const float detune2 = cents_to_ratio(10.0f);
            dco1_->SetFrequency(freq1 * detune2);
            dco2_->SetFrequency(freq2 * detune2);
            float voice2_dco1 = dco1_->Process();
            float voice2_dco2 = dco2_->Process();
            mixed_ += voice2_dco1 * dco1_level + voice2_dco2 * dco2_level;
            
            // Voice 3: -10 cents detune
            const float detune3 = cents_to_ratio(-10.0f);
            dco1_->SetFrequency(freq1 * detune3);
            dco2_->SetFrequency(freq2 * detune3);
            float voice3_dco1 = dco1_->Process();
            float voice3_dco2 = dco2_->Process();
            mixed_ += voice3_dco1 * dco1_level + voice3_dco2 * dco2_level;
            
            // Voice 4: +20 cents detune
            const float detune4 = cents_to_ratio(20.0f);
            dco1_->SetFrequency(freq1 * detune4);
            dco2_->SetFrequency(freq2 * detune4);
            float voice4_dco1 = dco1_->Process();
            float voice4_dco2 = dco2_->Process();
            mixed_ += voice4_dco1 * dco1_level + voice4_dco2 * dco2_level;
            
            // Average the 4 voices to prevent clipping
            mixed_ *= 0.25f;
        } else {
            // Normal single-voice mode
            mixed_ = dco1_out_ * dco1_level + dco2_out_ * dco2_level;
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
        cutoff_base *= semitones_to_ratio(note_offset * key_track * 12.0f);  // Fast approx
        
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
        
        // Apply filter mode from hub (0=LP12, 1=LP24, 2=HP12, 3=BP12)
        vcf_->SetMode(static_cast<dsp::JupiterVCF::Mode>(vcf_type & 0x03));
        
        filtered_ = vcf_->Process(mixed_);
        
        // Apply HPF (high-pass filter) from hub - Jupiter-8 style
        // HPF value: 0-100 maps to 20Hz-2kHz cutoff
        if (hpf_cutoff > 0) {
            // Simple one-pole HPF: y[n] = alpha * (y[n-1] + x[n] - x[n-1])
            // where alpha = 1 / (1 + 2*pi*fc/fs)
            const float hpf_freq = 20.0f + (hpf_cutoff / 100.0f) * 1980.0f;  // 20Hz-2kHz
            const float rc = 1.0f / (2.0f * M_PI * hpf_freq);
            const float dt = 1.0f / sample_rate_;
            const float alpha = rc / (rc + dt);
            
            // Static state variables for HPF
            static float hpf_prev_output = 0.0f;
            static float hpf_prev_input = 0.0f;
            
            filtered_ = alpha * (hpf_prev_output + filtered_ - hpf_prev_input);
            hpf_prev_output = filtered_;
            hpf_prev_input = filtered_;
        }
        
        // Apply VCA with envelope, store to intermediate buffer
        mix_buffer_[i] = filtered_ * vca_env_out_ * 0.5f;  // Scale to prevent clipping
    }
    
    // ============ Output stage with chorus/space effect ============
    
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
        // Configure effect based on EFFECT parameter
        // Effect mode: 0=Chorus, 1=Space, 2=Dry, 3=Both
        const uint8_t effect_mode = current_preset_.params[PARAM_EFFECT];
    
    if (effect_mode == 2) {
        // DRY mode: Bypass all effects, just copy mono to stereo
        for (uint32_t i = 0; i < frames; i++) {
            left_buffer_[i] = mix_buffer_[i];
            right_buffer_[i] = mix_buffer_[i];
        }
    } else if (effect_mode == 3) {
        // BOTH mode: Maximum effect (chorus + space combined)
        space_widener_->SetModDepth(8.0f);      // 8ms modulation depth
        space_widener_->SetMix(0.8f);           // 80% wet/dry mix
        space_widener_->SetDelayTime(40.0f);    // 40ms delay time
        space_widener_->ProcessMonoBatch(mix_buffer_, left_buffer_, right_buffer_, frames);
    } else if (effect_mode == 0) {
        // CHORUS mode: Moderate modulation depth, less delay time
        space_widener_->SetModDepth(3.0f);      // 3ms modulation depth
        space_widener_->SetMix(0.5f);           // 50% wet/dry mix
        space_widener_->SetDelayTime(15.0f);    // 15ms delay time
        space_widener_->ProcessMonoBatch(mix_buffer_, left_buffer_, right_buffer_, frames);
    } else {
        // SPACE mode (1): Wider stereo, more delay time
        space_widener_->SetModDepth(6.0f);      // 6ms modulation depth
        space_widener_->SetMix(0.7f);           // 70% wet/dry mix
        space_widener_->SetDelayTime(35.0f);    // 35ms delay time for wide stereo
        space_widener_->ProcessMonoBatch(mix_buffer_, left_buffer_, right_buffer_, frames);
    }
    }
    
    // Add tiny DC offset for denormal protection before interleaving
    const float denormal_offset = 1.0e-15f;
#ifdef USE_NEON
    // NEON-optimized stereo interleave (processes 4 frames at a time)
    {
        float32x4_t dc = vdupq_n_f32(denormal_offset);
        uint32_t i = 0;
        for (; i + 4 <= frames; i += 4) {
            float32x4_t l = vaddq_f32(vld1q_f32(&left_buffer_[i]), dc);
            float32x4_t r = vaddq_f32(vld1q_f32(&right_buffer_[i]), dc);
            float32x4x2_t interleaved = vzipq_f32(l, r);
            vst1q_f32(&out[i * 2], interleaved.val[0]);
            vst1q_f32(&out[i * 2 + 4], interleaved.val[1]);
        }
        // Scalar tail for remaining frames
        for (; i < frames; ++i) {
            out[i * 2] = left_buffer_[i] + denormal_offset;
            out[i * 2 + 1] = right_buffer_[i] + denormal_offset;
        }
    }
#else
    // Scalar fallback for non-NEON builds
    for (uint32_t i = 0; i < frames; ++i) {
        out[i * 2] = left_buffer_[i] + denormal_offset;
        out[i * 2 + 1] = right_buffer_[i] + denormal_offset;
    }
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
            v = clamp_u8_int32(value, 0, 3);
            break;
            
        // Page 2: DCO-2 discrete params
        case PARAM_DCO2_OCT:
            v = clamp_u8_int32(value, 0, 2);
            break;
        case PARAM_DCO2_WAVE:
            v = clamp_u8_int32(value, 0, 3);
            break;
        case PARAM_SYNC:
            v = clamp_u8_int32(value, 0, 2);
            break;
            
        // Page 6: MOD HUB selector and EFFECT mode
        case PARAM_MOD_HUB:
            v = clamp_u8_int32(value, 0, 8);  // 9 hub destinations
            mod_hub_.SetDestination(v);
            current_preset_.params[id] = v;
            return;  // Hub handles its own state
            
        case PARAM_MOD_AMT:
            // Hub amount: Store in hub and preset's hub_values array
            v = clamp_u8_int32(value, 0, 100);
            mod_hub_.SetValue(v);
            // Save to preset's hub storage for current destination
            {
                uint8_t dest = current_preset_.params[PARAM_MOD_HUB];
                if (dest < MOD_NUM_DESTINATIONS) {
                    current_preset_.hub_values[dest] = v;
                }
            }
            return;  // Hub handles its own state
            
        case PARAM_EFFECT:
            v = clamp_u8_int32(value, 0, 1);  // 0=Chorus, 1=Space
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
            switch (v & 0x03) {
                case 0: dco1_->SetWaveform(dsp::JupiterDCO::WAVEFORM_SAW); break;
                case 1: dco1_->SetWaveform(dsp::JupiterDCO::WAVEFORM_SQUARE); break;
                case 2: dco1_->SetWaveform(dsp::JupiterDCO::WAVEFORM_PULSE); break;
                case 3: dco1_->SetWaveform(dsp::JupiterDCO::WAVEFORM_TRIANGLE); break;
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
            switch (v & 0x03) {
                case 0: dco2_->SetWaveform(dsp::JupiterDCO::WAVEFORM_SAW); break;
                case 1: dco2_->SetWaveform(dsp::JupiterDCO::WAVEFORM_NOISE); break;
                case 2: dco2_->SetWaveform(dsp::JupiterDCO::WAVEFORM_PULSE); break;
                case 3: dco2_->SetWaveform(dsp::JupiterDCO::WAVEFORM_SINE); break;
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
            break;
        case PARAM_VCF_DECAY:
            env_vcf_->SetDecay(ParameterToEnvelopeTime(v));
            break;
        case PARAM_VCF_SUSTAIN:
            env_vcf_->SetSustain(v / 100.0f);
            break;
        case PARAM_VCF_RELEASE:
            env_vcf_->SetRelease(ParameterToEnvelopeTime(v));
            break;
        
        // ======== Page 5: VCA Envelope ========
        case PARAM_VCA_ATTACK:
            env_vca_->SetAttack(ParameterToEnvelopeTime(v));
            break;
        case PARAM_VCA_DECAY:
            env_vca_->SetDecay(ParameterToEnvelopeTime(v));
            break;
        case PARAM_VCA_SUSTAIN:
            env_vca_->SetSustain(v / 100.0f);
            break;
        case PARAM_VCA_RELEASE:
            env_vca_->SetRelease(ParameterToEnvelopeTime(v));
            break;
        
        // ======== Page 6: LFO, MOD HUB & Effects ========
        case PARAM_LFO_RATE:
            lfo_->SetFrequency(ParameterToExponentialFreq(v, 0.1f, 20.0f));
            break;
        case PARAM_MOD_HUB:
        case PARAM_MOD_AMT:
            // Hub updates handled in Render() by reading hub values
            break;
        case PARAM_EFFECT:
            // Effect mode (Chorus/Space) handled in Render()
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
    
    static char str_buf[16];  // Buffer for formatted strings
    
    switch (id) {
        // ======== Page 1: DCO-1 ========
        case PARAM_DCO1_OCT:
            return kOctaveNames[value < 3 ? value : 0];
        case PARAM_DCO1_WAVE:
            return kDco1WaveNames[value < 4 ? value : 0];
            
        // ======== Page 2: DCO-2 ========
        case PARAM_DCO2_OCT:
            return kOctaveNames[value < 3 ? value : 0];
        case PARAM_DCO2_WAVE:
            return kDco2WaveNames[value < 4 ? value : 0];
        case PARAM_SYNC:
            return kSyncNames[value < 3 ? value : 0];
        
        // ======== Page 6: MOD HUB ========
        case PARAM_MOD_HUB:
            if (value >= 0 && value < 9) {
                return mod_hub_.GetDestinationName(value);
            }
            return "";
            
        case PARAM_MOD_AMT: {
            // Get current destination to validate range
            uint8_t dest = mod_hub_.GetDestination();
            
            // Check if value is within current destination's range
            if (dest >= MOD_NUM_DESTINATIONS) {
                return nullptr;  // Invalid destination
            }
            
            const auto& dest_info = kModDestinations[dest];
            if (value < dest_info.min || value > dest_info.max) {
                return nullptr;  // Out of range - tells UI to stop querying
            }
            
            return mod_hub_.GetValueStringForDest(dest, value, str_buf, sizeof(str_buf));
        }
            
        case PARAM_EFFECT:
            if (value >= 0 && value < 4) {
                return effect_names[value];
            }
            return "DRY";  // Default fallback
        
        default:
            return nullptr;
    }
}

void DrupiterSynth::NoteOn(uint8_t note, uint8_t velocity) {
    current_note_ = note;
    current_velocity_ = velocity;
    current_freq_hz_ = NoteToFrequency(note);
    gate_ = true;
    
    // Trigger envelopes
    float vel_norm = velocity / 127.0f;
    env_vcf_->NoteOn(vel_norm);
    env_vca_->NoteOn(vel_norm);
    
    // Trigger LFO delay
    lfo_->Trigger();
}

void DrupiterSynth::NoteOff(uint8_t note) {
    if (note == current_note_ || note == 255) {  // 255 = all notes
        gate_ = false;
        env_vcf_->NoteOff();
        env_vca_->NoteOff();
    }
}

void DrupiterSynth::AllNoteOff() {
    gate_ = false;
    env_vcf_->NoteOff();
    env_vca_->NoteOff();
}

void DrupiterSynth::LoadPreset(uint8_t preset_id) {
    if (preset_id >= 6) {
        preset_id = 0;
    }
    
    current_preset_ = factory_presets_[preset_id];
    
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
    
    // Restore MOD HUB destination values
    for (uint8_t dest = 0; dest < MOD_NUM_DESTINATIONS; ++dest) {
        mod_hub_.SetValueForDest(dest, current_preset_.hub_values[dest]);
    }
    
    // Apply all parameters to DSP components
    for (uint8_t i = 0; i < PARAM_COUNT; ++i) {
        SetParameter(i, current_preset_.params[i]);
    }
}

void DrupiterSynth::SavePreset(uint8_t preset_id) {
    if (preset_id < 6) {
        // Save current MOD HUB values to preset
        for (uint8_t dest = 0; dest < MOD_NUM_DESTINATIONS; ++dest) {
            current_preset_.hub_values[dest] = mod_hub_.GetValue(dest);
        }
        factory_presets_[preset_id] = current_preset_;
    }
}

float DrupiterSynth::NoteToFrequency(uint8_t note) const {
    // MIDI note to frequency: f = 440 * 2^((note - 69) / 12)
    return 440.0f * powf(2.0f, (note - 69) / 12.0f);
}

float DrupiterSynth::OctaveToMultiplier(uint8_t octave_param) const {
    // Preferred mapping: 0..2 => 16' (0.5x), 8' (1.0x), 4' (2.0x)
    if (octave_param <= 2) {
        static constexpr float mult[3] = {0.5f, 1.0f, 2.0f};
        return mult[octave_param];
    }
    // Backwards-compatible mapping for any legacy 0..127 values.
    if (octave_param < 42) return 0.5f;
    if (octave_param < 85) return 1.0f;
    return 2.0f;
}

float DrupiterSynth::GenerateNoise() {
    // Simple white noise generator
    noise_seed_ = (noise_seed_ * 1103515245 + 12345) & 0x7FFFFFFF;
    return (static_cast<float>(noise_seed_) / 2147483647.0f) * 2.0f - 1.0f;
}

float DrupiterSynth::ParameterToEnvelopeTime(uint8_t value) {
    // Quadratic scaling for envelope times (better control at low values)
    // 0 = 1ms, 32 = ~319ms, 64 = ~1.28s, 127 = 5s
    float normalized = value / 100.0f;
    return 0.001f + normalized * normalized * 4.999f;
}

float DrupiterSynth::ParameterToExponentialFreq(uint8_t value, float min_freq, float max_freq) {
    // Quadratic scaling for frequency parameters
    float normalized = value / 100.0f;
    return min_freq + normalized * normalized * (max_freq - min_freq);
}

void DrupiterSynth::InitFactoryPresets() {
    // Factory presets now use hub control for modulation routing
    
    // Initialize all presets with default hub values
    for (uint8_t p = 0; p < 6; ++p) {
        for (uint8_t dest = 0; dest < MOD_NUM_DESTINATIONS; ++dest) {
            factory_presets_[p].hub_values[dest] = kModDestinations[dest].default_value;
        }
    }
    
    // Preset 0: Init - Basic sound
    std::memset(&factory_presets_[0], 0, sizeof(Preset));
    // Reinit hub values after memset
    for (uint8_t dest = 0; dest < MOD_NUM_DESTINATIONS; ++dest) {
        factory_presets_[0].hub_values[dest] = kModDestinations[dest].default_value;
    }
    // Page 1: DCO-1
    factory_presets_[0].params[PARAM_DCO1_OCT] = 1;       // 8'
    factory_presets_[0].params[PARAM_DCO1_WAVE] = 0;      // SAW
    factory_presets_[0].params[PARAM_DCO1_PW] = 50;       // 50%
    factory_presets_[0].params[PARAM_XMOD] = 0;           // No cross-mod
    // Page 2: DCO-2
    factory_presets_[0].params[PARAM_DCO2_OCT] = 1;       // 8'
    factory_presets_[0].params[PARAM_DCO2_WAVE] = 0;      // SAW
    factory_presets_[0].params[PARAM_DCO2_TUNE] = 0;     // Center (no detune)
    factory_presets_[0].params[PARAM_SYNC] = 0;           // OFF
    // Page 3: MIX & VCF
    factory_presets_[0].params[PARAM_OSC_MIX] = 0;        // DCO1 only
    factory_presets_[0].params[PARAM_VCF_CUTOFF] = 79;
    factory_presets_[0].params[PARAM_VCF_RESONANCE] = 16;
    factory_presets_[0].params[PARAM_VCF_KEYFLW] = 50;    // 50% keyboard tracking
    // Page 4: VCF Envelope
    factory_presets_[0].params[PARAM_VCF_ATTACK] = 4;
    factory_presets_[0].params[PARAM_VCF_DECAY] = 31;
    factory_presets_[0].params[PARAM_VCF_SUSTAIN] = 50;
    factory_presets_[0].params[PARAM_VCF_RELEASE] = 24;
    // Page 5: VCA Envelope
    factory_presets_[0].params[PARAM_VCA_ATTACK] = 1;
    factory_presets_[0].params[PARAM_VCA_DECAY] = 39;
    factory_presets_[0].params[PARAM_VCA_SUSTAIN] = 79;
    factory_presets_[0].params[PARAM_VCA_RELEASE] = 16;
    // Page 6: LFO, MOD HUB & Effects
    factory_presets_[0].params[PARAM_LFO_RATE] = 32;      // Moderate LFO rate
    factory_presets_[0].params[PARAM_MOD_HUB] = MOD_VCF_TYPE;  // VCF Type by default
    factory_presets_[0].hub_values[MOD_VCF_TYPE] = 1;     // LP24 filter
    factory_presets_[0].params[PARAM_EFFECT] = 0;         // Chorus
    std::strcpy(factory_presets_[0].name, "Init 1");
    
    // Preset 1: Bass - Punchy bass with filter envelope
    std::memset(&factory_presets_[1], 0, sizeof(Preset));
    // Reinit hub values after memset
    for (uint8_t dest = 0; dest < MOD_NUM_DESTINATIONS; ++dest) {
        factory_presets_[1].hub_values[dest] = kModDestinations[dest].default_value;
    }
    factory_presets_[1].params[PARAM_DCO1_OCT] = 0;       // 16'
    factory_presets_[1].params[PARAM_DCO1_WAVE] = 2;      // PULSE
    factory_presets_[1].params[PARAM_DCO1_PW] = 31;       // Narrow pulse
    factory_presets_[1].params[PARAM_XMOD] = 0;
    factory_presets_[1].params[PARAM_DCO2_OCT] = 0;       // 16'
    factory_presets_[1].params[PARAM_DCO2_WAVE] = 0;      // SAW
    factory_presets_[1].params[PARAM_DCO2_TUNE] = 0;
    factory_presets_[1].params[PARAM_SYNC] = 0;
    factory_presets_[1].params[PARAM_OSC_MIX] = 0;        // DCO1 only
    factory_presets_[1].params[PARAM_VCF_CUTOFF] = 39;
    factory_presets_[1].params[PARAM_VCF_RESONANCE] = 39;
    factory_presets_[1].params[PARAM_VCF_KEYFLW] = 75;    // High key tracking for bass
    factory_presets_[1].params[PARAM_VCF_ATTACK] = 0;
    factory_presets_[1].params[PARAM_VCF_DECAY] = 27;
    factory_presets_[1].params[PARAM_VCF_SUSTAIN] = 16;
    factory_presets_[1].params[PARAM_VCF_RELEASE] = 8;
    factory_presets_[1].params[PARAM_VCA_ATTACK] = 0;
    factory_presets_[1].params[PARAM_VCA_DECAY] = 31;
    factory_presets_[1].params[PARAM_VCA_SUSTAIN] = 63;
    factory_presets_[1].params[PARAM_VCA_RELEASE] = 12;
    factory_presets_[1].params[PARAM_LFO_RATE] = 32;
    factory_presets_[1].params[PARAM_MOD_HUB] = MOD_VCF_TYPE;
    factory_presets_[1].hub_values[MOD_VCF_TYPE] = 1;     // LP24
    factory_presets_[1].params[PARAM_EFFECT] = 0;
    std::strcpy(factory_presets_[1].name, "Bass 1");
    
    // Preset 2: Lead - Sharp lead with sync
    std::memset(&factory_presets_[2], 0, sizeof(Preset));
    // Reinit hub values after memset
    for (uint8_t dest = 0; dest < MOD_NUM_DESTINATIONS; ++dest) {
        factory_presets_[2].hub_values[dest] = kModDestinations[dest].default_value;
    }
    factory_presets_[2].params[PARAM_DCO1_OCT] = 1;       // 8'
    factory_presets_[2].params[PARAM_DCO1_WAVE] = 0;      // SAW
    factory_presets_[2].params[PARAM_DCO1_PW] = 50;
    factory_presets_[2].params[PARAM_XMOD] = 0;
    factory_presets_[2].params[PARAM_DCO2_OCT] = 2;       // 4' (one octave up)
    factory_presets_[2].params[PARAM_DCO2_WAVE] = 0;      // SAW
    factory_presets_[2].params[PARAM_DCO2_TUNE] = 0;
    factory_presets_[2].params[PARAM_SYNC] = 2;           // HARD sync for classic lead
    factory_presets_[2].params[PARAM_OSC_MIX] = 30;       // Mostly DCO1 with some DCO2
    factory_presets_[2].params[PARAM_VCF_CUTOFF] = 71;
    factory_presets_[2].params[PARAM_VCF_RESONANCE] = 55;
    factory_presets_[2].params[PARAM_VCF_KEYFLW] = 40;
    factory_presets_[2].params[PARAM_VCF_ATTACK] = 4;
    factory_presets_[2].params[PARAM_VCF_DECAY] = 24;
    factory_presets_[2].params[PARAM_VCF_SUSTAIN] = 47;
    factory_presets_[2].params[PARAM_VCF_RELEASE] = 20;
    factory_presets_[2].params[PARAM_VCA_ATTACK] = 2;
    factory_presets_[2].params[PARAM_VCA_DECAY] = 24;
    factory_presets_[2].params[PARAM_VCA_SUSTAIN] = 79;
    factory_presets_[2].params[PARAM_VCA_RELEASE] = 16;
    factory_presets_[2].params[PARAM_LFO_RATE] = 50;
    factory_presets_[2].params[PARAM_MOD_HUB] = MOD_LFO_TO_VCF;
    factory_presets_[2].hub_values[MOD_LFO_TO_VCF] = 30;
    factory_presets_[2].params[PARAM_EFFECT] = 0;
    std::strcpy(factory_presets_[2].name, "Lead 1");
    
    // Preset 3: Pad - Warm pad with detuned oscillators
    std::memset(&factory_presets_[3], 0, sizeof(Preset));
    // Reinit hub values after memset
    for (uint8_t dest = 0; dest < MOD_NUM_DESTINATIONS; ++dest) {
        factory_presets_[3].hub_values[dest] = kModDestinations[dest].default_value;
    }
    factory_presets_[3].params[PARAM_DCO1_OCT] = 1;       // 8'
    factory_presets_[3].params[PARAM_DCO1_WAVE] = 0;      // SAW
    factory_presets_[3].params[PARAM_DCO1_PW] = 50;
    factory_presets_[3].params[PARAM_XMOD] = 0;
    factory_presets_[3].params[PARAM_DCO2_OCT] = 1;       // 8'
    factory_presets_[3].params[PARAM_DCO2_WAVE] = 0;      // SAW
    factory_presets_[3].params[PARAM_DCO2_TUNE] = 3;     // Slight detune
    factory_presets_[3].params[PARAM_SYNC] = 0;
    factory_presets_[3].params[PARAM_OSC_MIX] = 50;       // Equal mix
    factory_presets_[3].params[PARAM_VCF_CUTOFF] = 63;
    factory_presets_[3].params[PARAM_VCF_RESONANCE] = 20;
    factory_presets_[3].params[PARAM_VCF_KEYFLW] = 20;
    factory_presets_[3].params[PARAM_VCF_ATTACK] = 35;
    factory_presets_[3].params[PARAM_VCF_DECAY] = 39;
    factory_presets_[3].params[PARAM_VCF_SUSTAIN] = 55;
    factory_presets_[3].params[PARAM_VCF_RELEASE] = 39;
    factory_presets_[3].params[PARAM_VCA_ATTACK] = 39;
    factory_presets_[3].params[PARAM_VCA_DECAY] = 39;
    factory_presets_[3].params[PARAM_VCA_SUSTAIN] = 79;
    factory_presets_[3].params[PARAM_VCA_RELEASE] = 55;
    factory_presets_[3].params[PARAM_LFO_RATE] = 35;
    factory_presets_[3].params[PARAM_MOD_HUB] = MOD_VCF_TYPE;
    factory_presets_[3].hub_values[MOD_VCF_TYPE] = 1;     // LP24
    factory_presets_[3].params[PARAM_EFFECT] = 0;
    std::strcpy(factory_presets_[3].name, "Pad 1");
    
    // Preset 4: Brass - Bright brass with XMOD
    std::memset(&factory_presets_[4], 0, sizeof(Preset));
    // Reinit hub values after memset
    for (uint8_t dest = 0; dest < MOD_NUM_DESTINATIONS; ++dest) {
        factory_presets_[4].hub_values[dest] = kModDestinations[dest].default_value;
    }
    factory_presets_[4].params[PARAM_DCO1_OCT] = 1;       // 8'
    factory_presets_[4].params[PARAM_DCO1_WAVE] = 0;      // SAW
    factory_presets_[4].params[PARAM_DCO1_PW] = 50;
    factory_presets_[4].params[PARAM_XMOD] = 15;          // Subtle cross-mod
    factory_presets_[4].params[PARAM_DCO2_OCT] = 1;       // 8'
    factory_presets_[4].params[PARAM_DCO2_WAVE] = 0;      // SAW
    factory_presets_[4].params[PARAM_DCO2_TUNE] = 0;
    factory_presets_[4].params[PARAM_SYNC] = 0;
    factory_presets_[4].params[PARAM_OSC_MIX] = 40;
    factory_presets_[4].params[PARAM_VCF_CUTOFF] = 59;
    factory_presets_[4].params[PARAM_VCF_RESONANCE] = 24;
    factory_presets_[4].params[PARAM_VCF_KEYFLW] = 60;
    factory_presets_[4].params[PARAM_VCF_ATTACK] = 12;
    factory_presets_[4].params[PARAM_VCF_DECAY] = 35;
    factory_presets_[4].params[PARAM_VCF_SUSTAIN] = 51;
    factory_presets_[4].params[PARAM_VCF_RELEASE] = 27;
    factory_presets_[4].params[PARAM_VCA_ATTACK] = 12;
    factory_presets_[4].params[PARAM_VCA_DECAY] = 35;
    factory_presets_[4].params[PARAM_VCA_SUSTAIN] = 71;
    factory_presets_[4].params[PARAM_VCA_RELEASE] = 24;
    factory_presets_[4].params[PARAM_LFO_RATE] = 40;
    factory_presets_[4].params[PARAM_MOD_HUB] = MOD_ENV_TO_VCF;
    factory_presets_[4].hub_values[MOD_ENV_TO_VCF] = 40;
    factory_presets_[4].params[PARAM_EFFECT] = 0;
    std::strcpy(factory_presets_[4].name, "Brass 1");
    
    // Preset 5: Strings - Lush strings with detuned oscillators
    std::memset(&factory_presets_[5], 0, sizeof(Preset));
    // Reinit hub values after memset
    for (uint8_t dest = 0; dest < MOD_NUM_DESTINATIONS; ++dest) {
        factory_presets_[5].hub_values[dest] = kModDestinations[dest].default_value;
    }
    factory_presets_[5].params[PARAM_DCO1_OCT] = 1;       // 8'
    factory_presets_[5].params[PARAM_DCO1_WAVE] = 0;      // SAW
    factory_presets_[5].params[PARAM_DCO1_PW] = 50;
    factory_presets_[5].params[PARAM_XMOD] = 0;
    factory_presets_[5].params[PARAM_DCO2_OCT] = 1;       // 8'
    factory_presets_[5].params[PARAM_DCO2_WAVE] = 0;      // SAW
    factory_presets_[5].params[PARAM_DCO2_TUNE] = 5;     // Detune for richness
    factory_presets_[5].params[PARAM_SYNC] = 0;
    factory_presets_[5].params[PARAM_OSC_MIX] = 50;       // Equal mix
    factory_presets_[5].params[PARAM_VCF_CUTOFF] = 75;
    factory_presets_[5].params[PARAM_VCF_RESONANCE] = 16;
    factory_presets_[5].params[PARAM_VCF_KEYFLW] = 25;
    factory_presets_[5].params[PARAM_VCF_ATTACK] = 47;
    factory_presets_[5].params[PARAM_VCF_DECAY] = 43;
    factory_presets_[5].params[PARAM_VCF_SUSTAIN] = 59;
    factory_presets_[5].params[PARAM_VCF_RELEASE] = 47;
    factory_presets_[5].params[PARAM_VCA_ATTACK] = 51;
    factory_presets_[5].params[PARAM_VCA_DECAY] = 43;
    factory_presets_[5].params[PARAM_VCA_SUSTAIN] = 79;
    factory_presets_[5].params[PARAM_VCA_RELEASE] = 63;
    factory_presets_[5].params[PARAM_LFO_RATE] = 38;
    factory_presets_[5].params[PARAM_MOD_HUB] = MOD_LFO_TO_VCO;
    factory_presets_[5].hub_values[MOD_LFO_TO_VCO] = 20;
    factory_presets_[5].params[PARAM_EFFECT] = 0;
    std::strcpy(factory_presets_[5].name, "String 1");
}
