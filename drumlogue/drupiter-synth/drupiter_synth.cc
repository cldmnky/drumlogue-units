/**
 * @file drupiter_synth.cc
 * @brief Main synthesizer implementation for Drupiter
 *
 * Based on Bristol Jupiter-8 emulation architecture
 * Integrates DCO, VCF, Envelopes, and LFO
 */

#include "drupiter_synth.h"
#include "dsp/jupiter_dco.h"
#include "dsp/jupiter_vcf.h"
#include "dsp/jupiter_env.h"
#include "dsp/jupiter_lfo.h"
#include "dsp/smoothed_value.h"
#include <cmath>
#include <cstring>
#include <cstdio>

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
    , noise_seed_(0x12345678)
    , cutoff_smooth_(nullptr)
    , dco1_level_smooth_(nullptr)
    , dco2_level_smooth_(nullptr)
    , last_cutoff_hz_(1000.0f)
{
    // Initialize preset to defaults
    std::memset(&current_preset_, 0, sizeof(Preset));
    std::strcpy(current_preset_.name, "Init");
    
    // Initialize MOD HUB with sensible defaults
    std::memset(mod_values_, 0, sizeof(mod_values_));
    mod_values_[MOD_LFO_RATE] = 32;      // ~2Hz LFO
    mod_values_[MOD_LFO_WAVE] = 0;       // Triangle
    mod_values_[MOD_LFO_TO_VCO] = 0;     // No vibrato
    mod_values_[MOD_LFO_TO_VCF] = 0;     // No filter mod
    mod_values_[MOD_VEL_TO_VCF] = 50;    // 50% velocity to filter
    mod_values_[MOD_KEY_TRACK] = 50;     // 50% key tracking
    mod_values_[MOD_PB_RANGE] = 0;       // ±2 semitones
    mod_values_[MOD_VCF_ENV_AMT] = 50;   // 50% VCF envelope amount
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
    // Limit frames to our buffer size
    if (frames > kMaxFrames) frames = kMaxFrames;
    
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
    const float detune_cents = (static_cast<int32_t>(current_preset_.params[PARAM_DCO2_TUNE]) - 50) * 4.0f;
    const float detune_ratio = cents_to_ratio(detune_cents);  // Fast approximation
    
    // Cross-modulation depth from direct XMOD param
    const float xmod_depth = xmod_depth_;  // Set via PARAM_XMOD in SetParameter
    
    // === Read MOD HUB values ===
    const float lfo_vco_depth = mod_values_[MOD_LFO_TO_VCO] / 100.0f;
    const float lfo_vcf_depth = mod_values_[MOD_LFO_TO_VCF] / 100.0f;
    const float vel_to_vcf = mod_values_[MOD_VEL_TO_VCF] / 100.0f;
    const float key_track = mod_values_[MOD_KEY_TRACK] / 100.0f;
    const float vcf_env_amt_mod = mod_values_[MOD_VCF_ENV_AMT] / 100.0f;  // Additional env amount from HUB
    const float env_amt = (current_preset_.params[PARAM_VCF_SUSTAIN] > 50 ? 0.5f : -0.5f) * vcf_env_amt_mod;  // Simplified - combine with main env
    
    // Velocity modulation factor (based on current note velocity)
    const float vel_mod = (current_velocity_ / 127.0f) * vel_to_vcf;
    
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
        mixed_ = dco1_out_ * dco1_level + dco2_out_ * dco2_level;
        
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
        
        // Combine envelope, LFO, and velocity modulation (in octaves).
        // Keep ranges conservative so cutoff doesn't peg at max and become ineffective.
        float total_mod = vcf_env_out_ * env_amt * 2.0f 
                        + lfo_out_ * lfo_vcf_depth * 1.0f
                        + vel_mod * 2.0f;  // Velocity adds up to +2 octaves

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
        
        filtered_ = vcf_->Process(mixed_);
        
        // Apply VCA with envelope, store to intermediate buffer
        mix_buffer_[i] = filtered_ * vca_env_out_ * 0.5f;  // Scale to prevent clipping
    }
    
    // ============ Output stage with chorus effect ============
    
    // Sanitize buffer (remove NaN/Inf) and apply soft clamp
    drupiter::neon::SanitizeAndClamp(mix_buffer_, 1.0f, frames);
    
    // Configure chorus effect from parameters
    // CHORUS_DEPTH: 0=off (dry), 50=moderate, 100=deep chorus
    const float chorus_depth = current_preset_.params[PARAM_CHORUS_DEPTH] / 100.0f;
    space_widener_->SetModDepth(chorus_depth * 6.0f);  // 0-6ms modulation depth
    space_widener_->SetMix(chorus_depth);              // Wet/dry mix follows depth
    
    // SPACE: Controls stereo spread via delay time (10-50ms range)
    // 0=narrow (10ms), 50=normal (30ms), 100=wide (50ms)
    const float space = current_preset_.params[PARAM_SPACE] / 100.0f;
    space_widener_->SetDelayTime(10.0f + space * 40.0f);  // 10-50ms delay time
    
    // Apply chorus stereo effect using modulated delays
    // Converts mono to wide stereo with LFO-modulated delay times
    space_widener_->ProcessMonoBatch(mix_buffer_, left_buffer_, right_buffer_, frames);
    
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
    // ========== MOD HUB ==========
    if (id == PARAM_MOD_VALUE) {
        // Route MOD_VALUE to the currently selected mod destination
        uint8_t sel = current_preset_.params[PARAM_MOD_SELECT];
        if (sel < MOD_NUM_DESTINATIONS) {
            // Special handling for LFO_WAVE which has 0-3 range
            uint8_t v;
            if (sel == MOD_LFO_WAVE) {
                v = (value < 0) ? 0 : ((value > 3) ? 3 : static_cast<uint8_t>(value));
            } else {
                v = (value < 0) ? 0 : ((value > 100) ? 100 : static_cast<uint8_t>(value));
            }
            mod_values_[sel] = v;
            
            // Update DSP components immediately for certain destinations
            switch (sel) {
                case MOD_LFO_RATE:
                    lfo_->SetFrequency(ParameterToExponentialFreq(v, 0.1f, 20.0f));
                    break;
                case MOD_LFO_WAVE:
                    lfo_->SetWaveform(static_cast<dsp::JupiterLFO::Waveform>(v & 0x03));
                    break;
                default:
                    // Other mod values are read in Render()
                    break;
            }
        }
        return;
    }
    
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
            
        // Page 3: VCF type
        case PARAM_VCF_TYPE:
            v = clamp_u8_int32(value, 0, 3);
            break;
            
        // Page 6: MOD selector
        case PARAM_MOD_SELECT:
            v = clamp_u8_int32(value, 0, MOD_NUM_DESTINATIONS - 1);
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
        case PARAM_VCF_TYPE:
            vcf_->SetMode(static_cast<dsp::JupiterVCF::Mode>(v & 0x03));
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
        
        // ======== Page 6: MOD HUB & Output ========
        case PARAM_MOD_SELECT:
            // When selection changes, update the visible MOD_VALUE param
            if (v < MOD_NUM_DESTINATIONS) {
                current_preset_.params[PARAM_MOD_VALUE] = mod_values_[v];
            }
            break;
        case PARAM_CHORUS_DEPTH:
        case PARAM_SPACE:
            // Applied in Render() output stage
            break;
    }
}

int32_t DrupiterSynth::GetParameter(uint8_t id) const {
    // Special handling for MOD HUB value
    if (id == PARAM_MOD_VALUE) {
        uint8_t sel = current_preset_.params[PARAM_MOD_SELECT];
        if (sel < MOD_NUM_DESTINATIONS) {
            return mod_values_[sel];
        }
        return 50;  // Default center value
    }
    
    if (id >= PARAM_COUNT) {
        return 0;
    }
    return current_preset_.params[id];
}

const char* DrupiterSynth::GetParameterStr(uint8_t id, int32_t value) {
    static const char* filter_types[] = {"LP12", "LP24", "HP12", "BP12"};
    static char str_buf[16];  // Buffer for formatted strings (larger to avoid truncation warnings)
    
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
        case PARAM_DCO2_TUNE: {
            // Show detune as centered value: 50=0, <50=negative, >50=positive
            // Clamp to expected range to avoid truncation
            int detune = (value > 100 ? 100 : (value < 0 ? 0 : value)) - 50;
            if (detune == 0) {
                return "0";
            } else if (detune > 0) {
                snprintf(str_buf, sizeof(str_buf), "+%d", detune);
            } else {
                snprintf(str_buf, sizeof(str_buf), "%d", detune);
            }
            return str_buf;
        }
        case PARAM_SYNC:
            return kSyncNames[value < 3 ? value : 0];
        
        // ======== Page 3: VCF ========
        case PARAM_VCF_TYPE:
            return filter_types[value & 0x03];
        
        // ======== Page 6: MOD HUB ========
        case PARAM_MOD_SELECT:
            if (value >= 0 && value < MOD_NUM_DESTINATIONS) {
                return kModDestNames[value];
            }
            return kModDestNames[0];
        
        // MOD HUB value - context-aware display
        case PARAM_MOD_VALUE: {
            uint8_t sel = current_preset_.params[PARAM_MOD_SELECT];
            switch (sel) {
                case MOD_LFO_WAVE:
                    // LFO waveform names
                    if (value < 4) {
                        return kLfoWaveNames[value];
                    }
                    return kLfoWaveNames[0];
                case MOD_PB_RANGE:
                    // Pitch bend range names
                    if (value < 4) {
                        return kPbRangeNames[value];
                    }
                    return kPbRangeNames[0];
                default:
                    // Most MOD destinations are 0-100%
                    snprintf(str_buf, sizeof(str_buf), "%d%%", value);
                    return str_buf;
            }
        }
        
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
    
    // Apply all parameters to DSP components
    for (uint8_t i = 0; i < PARAM_COUNT; ++i) {
        SetParameter(i, current_preset_.params[i]);
    }
}

void DrupiterSynth::SavePreset(uint8_t preset_id) {
    if (preset_id < 6) {
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
    // Factory presets now use direct params for DCO1/DCO2 (no more HUB routing)
    // MOD HUB values are still stored separately
    
    // Helper to set MOD HUB values for a preset
    auto setModValues = [this](uint8_t lfo_rate, uint8_t lfo_wave, uint8_t lfo_vco, 
                                uint8_t lfo_vcf, uint8_t vel_vcf, uint8_t key_track,
                                uint8_t pb_range, uint8_t vcf_env_amt) {
        mod_values_[MOD_LFO_RATE] = lfo_rate;
        mod_values_[MOD_LFO_WAVE] = lfo_wave;
        mod_values_[MOD_LFO_TO_VCO] = lfo_vco;
        mod_values_[MOD_LFO_TO_VCF] = lfo_vcf;
        mod_values_[MOD_VEL_TO_VCF] = vel_vcf;
        mod_values_[MOD_KEY_TRACK] = key_track;
        mod_values_[MOD_PB_RANGE] = pb_range;
        mod_values_[MOD_VCF_ENV_AMT] = vcf_env_amt;
    };
    
    // Preset 0: Init - Basic sound
    std::memset(&factory_presets_[0], 0, sizeof(Preset));
    // Page 1: DCO-1
    factory_presets_[0].params[PARAM_DCO1_OCT] = 1;       // 8'
    factory_presets_[0].params[PARAM_DCO1_WAVE] = 0;      // SAW
    factory_presets_[0].params[PARAM_DCO1_PW] = 50;       // 50%
    factory_presets_[0].params[PARAM_XMOD] = 0;           // No cross-mod
    // Page 2: DCO-2
    factory_presets_[0].params[PARAM_DCO2_OCT] = 1;       // 8'
    factory_presets_[0].params[PARAM_DCO2_WAVE] = 0;      // SAW
    factory_presets_[0].params[PARAM_DCO2_TUNE] = 50;     // Center (no detune)
    factory_presets_[0].params[PARAM_SYNC] = 0;           // OFF
    // Page 3: MIX & VCF
    factory_presets_[0].params[PARAM_OSC_MIX] = 0;        // DCO1 only
    factory_presets_[0].params[PARAM_VCF_CUTOFF] = 79;
    factory_presets_[0].params[PARAM_VCF_RESONANCE] = 16;
    factory_presets_[0].params[PARAM_VCF_TYPE] = 1;       // LP24
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
    // Page 6: MOD HUB & Output
    factory_presets_[0].params[PARAM_MOD_SELECT] = 0;
    factory_presets_[0].params[PARAM_MOD_VALUE] = 32;
    factory_presets_[0].params[PARAM_CHORUS_DEPTH] = 0;
    factory_presets_[0].params[PARAM_SPACE] = 50;
    std::strcpy(factory_presets_[0].name, "Init");
    
    // Preset 1: Bass - Punchy bass with filter envelope
    std::memset(&factory_presets_[1], 0, sizeof(Preset));
    factory_presets_[1].params[PARAM_DCO1_OCT] = 0;       // 16'
    factory_presets_[1].params[PARAM_DCO1_WAVE] = 2;      // PULSE
    factory_presets_[1].params[PARAM_DCO1_PW] = 31;       // Narrow pulse
    factory_presets_[1].params[PARAM_XMOD] = 0;
    factory_presets_[1].params[PARAM_DCO2_OCT] = 0;       // 16'
    factory_presets_[1].params[PARAM_DCO2_WAVE] = 0;      // SAW
    factory_presets_[1].params[PARAM_DCO2_TUNE] = 50;
    factory_presets_[1].params[PARAM_SYNC] = 0;
    factory_presets_[1].params[PARAM_OSC_MIX] = 0;        // DCO1 only
    factory_presets_[1].params[PARAM_VCF_CUTOFF] = 39;
    factory_presets_[1].params[PARAM_VCF_RESONANCE] = 39;
    factory_presets_[1].params[PARAM_VCF_TYPE] = 1;
    factory_presets_[1].params[PARAM_VCF_ATTACK] = 0;
    factory_presets_[1].params[PARAM_VCF_DECAY] = 27;
    factory_presets_[1].params[PARAM_VCF_SUSTAIN] = 16;
    factory_presets_[1].params[PARAM_VCF_RELEASE] = 8;
    factory_presets_[1].params[PARAM_VCA_ATTACK] = 0;
    factory_presets_[1].params[PARAM_VCA_DECAY] = 31;
    factory_presets_[1].params[PARAM_VCA_SUSTAIN] = 63;
    factory_presets_[1].params[PARAM_VCA_RELEASE] = 12;
    factory_presets_[1].params[PARAM_MOD_SELECT] = 0;
    factory_presets_[1].params[PARAM_MOD_VALUE] = 32;
    factory_presets_[1].params[PARAM_CHORUS_DEPTH] = 0;
    factory_presets_[1].params[PARAM_SPACE] = 30;
    std::strcpy(factory_presets_[1].name, "Bass");
    
    // Preset 2: Lead - Sharp lead with sync
    std::memset(&factory_presets_[2], 0, sizeof(Preset));
    factory_presets_[2].params[PARAM_DCO1_OCT] = 1;       // 8'
    factory_presets_[2].params[PARAM_DCO1_WAVE] = 0;      // SAW
    factory_presets_[2].params[PARAM_DCO1_PW] = 50;
    factory_presets_[2].params[PARAM_XMOD] = 0;
    factory_presets_[2].params[PARAM_DCO2_OCT] = 2;       // 4' (one octave up)
    factory_presets_[2].params[PARAM_DCO2_WAVE] = 0;      // SAW
    factory_presets_[2].params[PARAM_DCO2_TUNE] = 50;
    factory_presets_[2].params[PARAM_SYNC] = 2;           // HARD sync for classic lead
    factory_presets_[2].params[PARAM_OSC_MIX] = 30;       // Mostly DCO1 with some DCO2
    factory_presets_[2].params[PARAM_VCF_CUTOFF] = 71;
    factory_presets_[2].params[PARAM_VCF_RESONANCE] = 55;
    factory_presets_[2].params[PARAM_VCF_TYPE] = 1;
    factory_presets_[2].params[PARAM_VCF_ATTACK] = 4;
    factory_presets_[2].params[PARAM_VCF_DECAY] = 24;
    factory_presets_[2].params[PARAM_VCF_SUSTAIN] = 47;
    factory_presets_[2].params[PARAM_VCF_RELEASE] = 20;
    factory_presets_[2].params[PARAM_VCA_ATTACK] = 2;
    factory_presets_[2].params[PARAM_VCA_DECAY] = 24;
    factory_presets_[2].params[PARAM_VCA_SUSTAIN] = 79;
    factory_presets_[2].params[PARAM_VCA_RELEASE] = 16;
    factory_presets_[2].params[PARAM_MOD_SELECT] = 0;
    factory_presets_[2].params[PARAM_MOD_VALUE] = 50;
    factory_presets_[2].params[PARAM_CHORUS_DEPTH] = 20;
    factory_presets_[2].params[PARAM_SPACE] = 60;
    std::strcpy(factory_presets_[2].name, "Lead");
    
    // Preset 3: Pad - Warm pad with detuned oscillators
    std::memset(&factory_presets_[3], 0, sizeof(Preset));
    factory_presets_[3].params[PARAM_DCO1_OCT] = 1;       // 8'
    factory_presets_[3].params[PARAM_DCO1_WAVE] = 0;      // SAW
    factory_presets_[3].params[PARAM_DCO1_PW] = 50;
    factory_presets_[3].params[PARAM_XMOD] = 0;
    factory_presets_[3].params[PARAM_DCO2_OCT] = 1;       // 8'
    factory_presets_[3].params[PARAM_DCO2_WAVE] = 0;      // SAW
    factory_presets_[3].params[PARAM_DCO2_TUNE] = 53;     // Slight detune
    factory_presets_[3].params[PARAM_SYNC] = 0;
    factory_presets_[3].params[PARAM_OSC_MIX] = 50;       // Equal mix
    factory_presets_[3].params[PARAM_VCF_CUTOFF] = 63;
    factory_presets_[3].params[PARAM_VCF_RESONANCE] = 20;
    factory_presets_[3].params[PARAM_VCF_TYPE] = 1;
    factory_presets_[3].params[PARAM_VCF_ATTACK] = 35;
    factory_presets_[3].params[PARAM_VCF_DECAY] = 39;
    factory_presets_[3].params[PARAM_VCF_SUSTAIN] = 55;
    factory_presets_[3].params[PARAM_VCF_RELEASE] = 39;
    factory_presets_[3].params[PARAM_VCA_ATTACK] = 39;
    factory_presets_[3].params[PARAM_VCA_DECAY] = 39;
    factory_presets_[3].params[PARAM_VCA_SUSTAIN] = 79;
    factory_presets_[3].params[PARAM_VCA_RELEASE] = 55;
    factory_presets_[3].params[PARAM_MOD_SELECT] = 0;
    factory_presets_[3].params[PARAM_MOD_VALUE] = 35;
    factory_presets_[3].params[PARAM_CHORUS_DEPTH] = 50;
    factory_presets_[3].params[PARAM_SPACE] = 80;
    std::strcpy(factory_presets_[3].name, "Pad");
    
    // Preset 4: Brass - Bright brass with XMOD
    std::memset(&factory_presets_[4], 0, sizeof(Preset));
    factory_presets_[4].params[PARAM_DCO1_OCT] = 1;       // 8'
    factory_presets_[4].params[PARAM_DCO1_WAVE] = 0;      // SAW
    factory_presets_[4].params[PARAM_DCO1_PW] = 50;
    factory_presets_[4].params[PARAM_XMOD] = 15;          // Subtle cross-mod
    factory_presets_[4].params[PARAM_DCO2_OCT] = 1;       // 8'
    factory_presets_[4].params[PARAM_DCO2_WAVE] = 0;      // SAW
    factory_presets_[4].params[PARAM_DCO2_TUNE] = 50;
    factory_presets_[4].params[PARAM_SYNC] = 0;
    factory_presets_[4].params[PARAM_OSC_MIX] = 40;
    factory_presets_[4].params[PARAM_VCF_CUTOFF] = 59;
    factory_presets_[4].params[PARAM_VCF_RESONANCE] = 24;
    factory_presets_[4].params[PARAM_VCF_TYPE] = 1;
    factory_presets_[4].params[PARAM_VCF_ATTACK] = 12;
    factory_presets_[4].params[PARAM_VCF_DECAY] = 35;
    factory_presets_[4].params[PARAM_VCF_SUSTAIN] = 51;
    factory_presets_[4].params[PARAM_VCF_RELEASE] = 27;
    factory_presets_[4].params[PARAM_VCA_ATTACK] = 12;
    factory_presets_[4].params[PARAM_VCA_DECAY] = 35;
    factory_presets_[4].params[PARAM_VCA_SUSTAIN] = 71;
    factory_presets_[4].params[PARAM_VCA_RELEASE] = 24;
    factory_presets_[4].params[PARAM_MOD_SELECT] = 0;
    factory_presets_[4].params[PARAM_MOD_VALUE] = 40;
    factory_presets_[4].params[PARAM_CHORUS_DEPTH] = 15;
    factory_presets_[4].params[PARAM_SPACE] = 55;
    std::strcpy(factory_presets_[4].name, "Brass");
    
    // Preset 5: Strings - Lush strings with detuned oscillators
    std::memset(&factory_presets_[5], 0, sizeof(Preset));
    factory_presets_[5].params[PARAM_DCO1_OCT] = 1;       // 8'
    factory_presets_[5].params[PARAM_DCO1_WAVE] = 0;      // SAW
    factory_presets_[5].params[PARAM_DCO1_PW] = 50;
    factory_presets_[5].params[PARAM_XMOD] = 0;
    factory_presets_[5].params[PARAM_DCO2_OCT] = 1;       // 8'
    factory_presets_[5].params[PARAM_DCO2_WAVE] = 0;      // SAW
    factory_presets_[5].params[PARAM_DCO2_TUNE] = 55;     // Detune for richness
    factory_presets_[5].params[PARAM_SYNC] = 0;
    factory_presets_[5].params[PARAM_OSC_MIX] = 50;       // Equal mix
    factory_presets_[5].params[PARAM_VCF_CUTOFF] = 75;
    factory_presets_[5].params[PARAM_VCF_RESONANCE] = 16;
    factory_presets_[5].params[PARAM_VCF_TYPE] = 1;
    factory_presets_[5].params[PARAM_VCF_ATTACK] = 47;
    factory_presets_[5].params[PARAM_VCF_DECAY] = 43;
    factory_presets_[5].params[PARAM_VCF_SUSTAIN] = 59;
    factory_presets_[5].params[PARAM_VCF_RELEASE] = 47;
    factory_presets_[5].params[PARAM_VCA_ATTACK] = 51;
    factory_presets_[5].params[PARAM_VCA_DECAY] = 43;
    factory_presets_[5].params[PARAM_VCA_SUSTAIN] = 79;
    factory_presets_[5].params[PARAM_VCA_RELEASE] = 63;
    factory_presets_[5].params[PARAM_MOD_SELECT] = 0;
    factory_presets_[5].params[PARAM_MOD_VALUE] = 38;
    factory_presets_[5].params[PARAM_CHORUS_DEPTH] = 60;
    factory_presets_[5].params[PARAM_SPACE] = 85;
    std::strcpy(factory_presets_[5].name, "Strings");
    
    // Initialize default MOD HUB values
    setModValues(32, 0, 0, 0, 50, 50, 0, 50);
}
