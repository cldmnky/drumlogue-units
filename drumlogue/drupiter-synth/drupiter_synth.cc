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

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

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
    memset(&current_preset_, 0, sizeof(Preset));
    strcpy(current_preset_.name, "Init");
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
    
    if (!dco1_ || !dco2_ || !vcf_ || !lfo_ || !env_vcf_ || !env_vca_ ||
        !cutoff_smooth_ || !dco1_level_smooth_ || !dco2_level_smooth_) {
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
    
    dco1_ = nullptr;
    dco2_ = nullptr;
    vcf_ = nullptr;
    lfo_ = nullptr;
    env_vcf_ = nullptr;
    env_vca_ = nullptr;
    cutoff_smooth_ = nullptr;
    dco1_level_smooth_ = nullptr;
    dco2_level_smooth_ = nullptr;
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
    
    // Update smoothed parameter targets (once per buffer for efficiency)
    cutoff_smooth_->SetTarget(current_preset_.params[PARAM_VCF_CUTOFF] / 127.0f);
    dco1_level_smooth_->SetTarget(current_preset_.params[PARAM_DCO1_LEVEL] / 127.0f);
    dco2_level_smooth_->SetTarget(current_preset_.params[PARAM_DCO2_LEVEL] / 127.0f);
    
    // Pre-calculate constants outside the loop
    const float dco1_oct_mult = OctaveToMultiplier(current_preset_.params[PARAM_DCO1_OCTAVE]);
    const float dco2_oct_mult = OctaveToMultiplier(current_preset_.params[PARAM_DCO2_OCTAVE]);
    
    // Detune: convert cents to frequency ratio (approximate for small deviations)
    // Exact: 2^(cents/1200), approx: 1 + cents/1731 for small cents
    const float detune_cents = (current_preset_.params[PARAM_DCO2_DETUNE] - 64) * 0.15625f;  // ±10 cents
    const float detune_ratio = 1.0f + detune_cents / 1731.0f;  // Linear approx for small detune
    
    const float lfo_vco_depth = current_preset_.params[PARAM_LFO_VCO_DEPTH] / 127.0f;
    const float env_amt = (current_preset_.params[PARAM_VCF_ENV_AMT] - 64) / 64.0f;  // -1 to +1
    const float lfo_vcf_depth = current_preset_.params[PARAM_LFO_VCF_DEPTH] / 127.0f;
    
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
        
        // Apply cross-modulation (DCO2 -> DCO1 FM)
        // Always active when DCO2 has level (Jupiter-8 style)
        if (dco2_level > 0.01f) {
            dco1_->ApplyFM(dco2_out_ * dco2_level * 0.3f);  // Scaled FM depth
        } else {
            dco1_->ApplyFM(0.0f);
        }
        
        // Process DCO1 first (master for sync)
        dco1_out_ = dco1_->Process();
        
        // DCO sync: DCO1 resets DCO2 phase on wrap
        if (dco1_->DidWrap()) {
            dco2_->ResetPhase();
        }
        
        // Process DCO2 (slave)
        dco2_out_ = dco2_->Process();
        
        // Mix oscillators with smoothed levels
        mixed_ = dco1_out_ * dco1_level + dco2_out_ * dco2_level;
        
        // Apply filter with smoothed cutoff, envelope, and LFO modulation
        const float cutoff_norm = cutoff_smooth_->Process();
        const float cutoff_base = cutoff_norm * 10000.0f + 20.0f;
        
        // Combine envelope and LFO modulation using linear approximation for small mods
        // For large mods, use exponential (±2 octaves)
        float total_mod = vcf_env_out_ * env_amt * 4.0f + lfo_out_ * lfo_vcf_depth * 2.0f;
        
        // Approximate 2^x for small x: 2^x ≈ 1 + 0.693x (for |x| < 0.5)
        // For larger range, clamp and use full calculation
        float cutoff_modulated;
        if (total_mod > -0.5f && total_mod < 0.5f) {
            cutoff_modulated = cutoff_base * (1.0f + 0.693f * total_mod);
        } else {
            cutoff_modulated = cutoff_base * powf(2.0f, total_mod);
        }
        
        // Only update filter coefficients if cutoff changed significantly (optimization)
        const float cutoff_diff = cutoff_modulated - last_cutoff_hz_;
        if (cutoff_diff > 1.0f || cutoff_diff < -1.0f) {
            vcf_->SetCutoff(cutoff_modulated);
            last_cutoff_hz_ = cutoff_modulated;
        }
        
        filtered_ = vcf_->Process(mixed_);
        
        // Apply VCA with envelope, store to intermediate buffer
        mix_buffer_[i] = filtered_ * vca_env_out_ * 0.5f;  // Scale to prevent clipping
    }
    
    // ============ NEON-optimized output stage ============
    
    // Sanitize buffer (remove NaN/Inf) and apply soft clamp
    drupiter::neon::SanitizeAndClamp(mix_buffer_, 1.0f, frames);
    
#ifdef USE_NEON
    // NEON-optimized mono to stereo duplication with interleaving
    // Process 4 samples at a time
    const float32x4_t dc_offset = vdupq_n_f32(1.0e-15f);  // Denormal protection
    uint32_t i = 0;
    
    for (; i + 4 <= frames; i += 4) {
        // Load 4 mono samples
        float32x4_t mono = vld1q_f32(&mix_buffer_[i]);
        
        // Add tiny DC offset for denormal protection
        mono = vaddq_f32(mono, dc_offset);
        
        // Duplicate to left and right (mono -> stereo)
        // Interleave: [m0,m1,m2,m3] -> [m0,m0,m1,m1,m2,m2,m3,m3]
        float32x4x2_t stereo = vzipq_f32(mono, mono);
        
        // Store interleaved stereo output
        vst1q_f32(&out[i * 2], stereo.val[0]);      // m0,m0,m1,m1
        vst1q_f32(&out[i * 2 + 4], stereo.val[1]);  // m2,m2,m3,m3
    }
    
    // Scalar fallback for remaining samples
    for (; i < frames; ++i) {
        float sample = mix_buffer_[i] + 1.0e-15f;
        out[i * 2] = sample;
        out[i * 2 + 1] = sample;
    }
#else
    // Non-NEON fallback
    for (uint32_t i = 0; i < frames; ++i) {
        float sample = mix_buffer_[i] + 1.0e-15f;  // Denormal protection
        out[i * 2] = sample;
        out[i * 2 + 1] = sample;
    }
#endif
}

void DrupiterSynth::SetParameter(uint8_t id, int32_t value) {
    if (id >= PARAM_COUNT) {
        return;
    }
    
    current_preset_.params[id] = static_cast<uint8_t>(value);
    
    // Update DSP components based on parameter changes
    switch (id) {
        // DCO1 parameters
        case PARAM_DCO1_OCTAVE:
            // Octave changes affect frequency, will be applied in next Render()
            break;
        case PARAM_DCO1_WAVE:
            dco1_->SetWaveform(static_cast<dsp::JupiterDCO::Waveform>(value & 0x03));
            break;
        case PARAM_DCO1_PW:
            dco1_->SetPulseWidth(value / 127.0f);
            break;
        case PARAM_DCO1_LEVEL:
            // Level is smoothed in Render(), but set target immediately
            if (dco1_level_smooth_) {
                dco1_level_smooth_->SetTarget(value / 127.0f);
            }
            break;
        
        // DCO2 parameters
        case PARAM_DCO2_OCTAVE:
            // Octave changes affect frequency, will be applied in next Render()
            break;
        case PARAM_DCO2_WAVE:
            dco2_->SetWaveform(static_cast<dsp::JupiterDCO::Waveform>(value & 0x03));
            break;
        case PARAM_DCO2_DETUNE:
            // Detune is applied in Render()
            break;
        case PARAM_DCO2_LEVEL:
            // Level is smoothed in Render(), but set target immediately
            if (dco2_level_smooth_) {
                dco2_level_smooth_->SetTarget(value / 127.0f);
            }
            break;
        
        // VCF parameters
        case PARAM_VCF_CUTOFF:
            // Cutoff is smoothed in Render()
            if (cutoff_smooth_) {
                cutoff_smooth_->SetTarget(value / 127.0f);
            }
            break;
        case PARAM_VCF_RESONANCE:
            vcf_->SetResonance(value / 127.0f);
            break;
        case PARAM_VCF_ENV_AMT:
            // Envelope amount is applied in Render()
            break;
        case PARAM_VCF_TYPE:
            vcf_->SetMode(static_cast<dsp::JupiterVCF::Mode>(value & 0x03));
            break;
        
        // VCF Envelope
        case PARAM_VCF_ATTACK:
            env_vcf_->SetAttack(ParameterToEnvelopeTime(value));
            break;
        case PARAM_VCF_DECAY:
            env_vcf_->SetDecay(ParameterToEnvelopeTime(value));
            break;
        case PARAM_VCF_SUSTAIN:
            env_vcf_->SetSustain(value / 127.0f);
            break;
        case PARAM_VCF_RELEASE:
            env_vcf_->SetRelease(ParameterToEnvelopeTime(value));
            break;
        
        // VCA Envelope
        case PARAM_VCA_ATTACK:
            env_vca_->SetAttack(ParameterToEnvelopeTime(value));
            break;
        case PARAM_VCA_DECAY:
            env_vca_->SetDecay(ParameterToEnvelopeTime(value));
            break;
        case PARAM_VCA_SUSTAIN:
            env_vca_->SetSustain(value / 127.0f);
            break;
        case PARAM_VCA_RELEASE:
            env_vca_->SetRelease(ParameterToEnvelopeTime(value));
            break;
        
        // LFO
        case PARAM_LFO_RATE:
            // Quadratic scaling: 0.1Hz to 20Hz (better control at low rates)
            lfo_->SetFrequency(ParameterToExponentialFreq(value, 0.1f, 20.0f));
            break;
        case PARAM_LFO_WAVE:
            lfo_->SetWaveform(static_cast<dsp::JupiterLFO::Waveform>(value & 0x03));
            break;
        case PARAM_LFO_VCO_DEPTH:
            // Stored in preset, applied in Render()
            break;
        case PARAM_LFO_VCF_DEPTH:
            // Stored in preset, applied in Render()
            break;
    }
}

int32_t DrupiterSynth::GetParameter(uint8_t id) const {
    if (id >= PARAM_COUNT) {
        return 0;
    }
    return current_preset_.params[id];
}

const char* DrupiterSynth::GetParameterStr(uint8_t id, int32_t value) {
    static const char* waveforms[] = {"RAMP", "SQR", "PULSE", "TRI"};
    static const char* filter_types[] = {"LP12", "LP24", "HP12", "BP12"};
    static const char* lfo_waves[] = {"TRI", "RAMP", "SQR", "S&H"};
    
    switch (id) {
        case PARAM_DCO1_WAVE:
        case PARAM_DCO2_WAVE:
            return waveforms[value & 0x03];
        case PARAM_VCF_TYPE:
            return filter_types[value & 0x03];
        case PARAM_LFO_WAVE:
            return lfo_waves[value & 0x03];
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
    
    // Apply keyboard tracking to filter
    vcf_->ApplyKeyboardTracking(note);
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
        cutoff_smooth_->SetImmediate(current_preset_.params[PARAM_VCF_CUTOFF] / 127.0f);
    }
    if (dco1_level_smooth_) {
        dco1_level_smooth_->SetImmediate(current_preset_.params[PARAM_DCO1_LEVEL] / 127.0f);
    }
    if (dco2_level_smooth_) {
        dco2_level_smooth_->SetImmediate(current_preset_.params[PARAM_DCO2_LEVEL] / 127.0f);
    }
    
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
    // Map 0-127 to octave multipliers: 16' (0.5x), 8' (1.0x), 4' (2.0x)
    if (octave_param < 42) {
        return 0.5f;  // 16'
    } else if (octave_param < 85) {
        return 1.0f;  // 8'
    } else {
        return 2.0f;  // 4'
    }
}

float DrupiterSynth::GenerateNoise() {
    // Simple white noise generator
    noise_seed_ = (noise_seed_ * 1103515245 + 12345) & 0x7FFFFFFF;
    return (static_cast<float>(noise_seed_) / 0x7FFFFFFF) * 2.0f - 1.0f;
}

float DrupiterSynth::ParameterToEnvelopeTime(uint8_t value) {
    // Quadratic scaling for envelope times (better control at low values)
    // 0 = 1ms, 32 = ~319ms, 64 = ~1.28s, 127 = 5s
    float normalized = value / 127.0f;
    return 0.001f + normalized * normalized * 4.999f;
}

float DrupiterSynth::ParameterToExponentialFreq(uint8_t value, float min_freq, float max_freq) {
    // Quadratic scaling for frequency parameters
    float normalized = value / 127.0f;
    return min_freq + normalized * normalized * (max_freq - min_freq);
}

void DrupiterSynth::InitFactoryPresets() {
    // Preset 0: Init - Basic sound
    memset(&factory_presets_[0], 0, sizeof(Preset));
    factory_presets_[0].params[PARAM_DCO1_OCTAVE] = 64;      // 8'
    factory_presets_[0].params[PARAM_DCO1_WAVE] = 1;         // Square
    factory_presets_[0].params[PARAM_DCO1_PW] = 64;          // 50% pulse width
    factory_presets_[0].params[PARAM_DCO1_LEVEL] = 100;
    factory_presets_[0].params[PARAM_DCO2_OCTAVE] = 64;      // 8'
    factory_presets_[0].params[PARAM_DCO2_WAVE] = 0;         // Ramp
    factory_presets_[0].params[PARAM_DCO2_DETUNE] = 64;      // No detune
    factory_presets_[0].params[PARAM_DCO2_LEVEL] = 0;        // DCO2 off
    factory_presets_[0].params[PARAM_VCF_CUTOFF] = 100;
    factory_presets_[0].params[PARAM_VCF_RESONANCE] = 20;
    factory_presets_[0].params[PARAM_VCF_ENV_AMT] = 64;      // No envelope mod
    factory_presets_[0].params[PARAM_VCF_TYPE] = 1;          // LP24
    factory_presets_[0].params[PARAM_VCF_ATTACK] = 5;
    factory_presets_[0].params[PARAM_VCF_DECAY] = 40;
    factory_presets_[0].params[PARAM_VCF_SUSTAIN] = 64;
    factory_presets_[0].params[PARAM_VCF_RELEASE] = 30;
    factory_presets_[0].params[PARAM_VCA_ATTACK] = 1;
    factory_presets_[0].params[PARAM_VCA_DECAY] = 50;
    factory_presets_[0].params[PARAM_VCA_SUSTAIN] = 100;
    factory_presets_[0].params[PARAM_VCA_RELEASE] = 20;
    factory_presets_[0].params[PARAM_LFO_RATE] = 40;
    factory_presets_[0].params[PARAM_LFO_WAVE] = 0;          // Triangle
    factory_presets_[0].params[PARAM_LFO_VCO_DEPTH] = 0;
    factory_presets_[0].params[PARAM_LFO_VCF_DEPTH] = 0;
    strcpy(factory_presets_[0].name, "Init");
    
    // Preset 1: Bass - Punchy bass with filter envelope
    memset(&factory_presets_[1], 0, sizeof(Preset));
    factory_presets_[1].params[PARAM_DCO1_OCTAVE] = 42;      // 16' (lower)
    factory_presets_[1].params[PARAM_DCO1_WAVE] = 2;         // Pulse
    factory_presets_[1].params[PARAM_DCO1_PW] = 40;          // Narrow pulse
    factory_presets_[1].params[PARAM_DCO1_LEVEL] = 127;
    factory_presets_[1].params[PARAM_DCO2_OCTAVE] = 42;      // 16'
    factory_presets_[1].params[PARAM_DCO2_WAVE] = 0;         // Ramp
    factory_presets_[1].params[PARAM_DCO2_DETUNE] = 64;
    factory_presets_[1].params[PARAM_DCO2_LEVEL] = 0;
    factory_presets_[1].params[PARAM_VCF_CUTOFF] = 50;
    factory_presets_[1].params[PARAM_VCF_RESONANCE] = 50;
    factory_presets_[1].params[PARAM_VCF_ENV_AMT] = 100;     // Strong envelope mod
    factory_presets_[1].params[PARAM_VCF_TYPE] = 1;          // LP24
    factory_presets_[1].params[PARAM_VCF_ATTACK] = 0;
    factory_presets_[1].params[PARAM_VCF_DECAY] = 35;
    factory_presets_[1].params[PARAM_VCF_SUSTAIN] = 20;
    factory_presets_[1].params[PARAM_VCF_RELEASE] = 10;
    factory_presets_[1].params[PARAM_VCA_ATTACK] = 0;
    factory_presets_[1].params[PARAM_VCA_DECAY] = 40;
    factory_presets_[1].params[PARAM_VCA_SUSTAIN] = 80;
    factory_presets_[1].params[PARAM_VCA_RELEASE] = 15;
    factory_presets_[1].params[PARAM_LFO_RATE] = 30;
    factory_presets_[1].params[PARAM_LFO_WAVE] = 0;
    factory_presets_[1].params[PARAM_LFO_VCO_DEPTH] = 0;
    factory_presets_[1].params[PARAM_LFO_VCF_DEPTH] = 0;
    strcpy(factory_presets_[1].name, "Bass");
    
    // Preset 2: Lead - Sharp lead with vibrato
    memset(&factory_presets_[2], 0, sizeof(Preset));
    factory_presets_[2].params[PARAM_DCO1_OCTAVE] = 64;      // 8'
    factory_presets_[2].params[PARAM_DCO1_WAVE] = 0;         // Ramp
    factory_presets_[2].params[PARAM_DCO1_PW] = 64;
    factory_presets_[2].params[PARAM_DCO1_LEVEL] = 127;
    factory_presets_[2].params[PARAM_DCO2_OCTAVE] = 64;
    factory_presets_[2].params[PARAM_DCO2_WAVE] = 0;
    factory_presets_[2].params[PARAM_DCO2_DETUNE] = 64;
    factory_presets_[2].params[PARAM_DCO2_LEVEL] = 0;
    factory_presets_[2].params[PARAM_VCF_CUTOFF] = 90;
    factory_presets_[2].params[PARAM_VCF_RESONANCE] = 70;
    factory_presets_[2].params[PARAM_VCF_ENV_AMT] = 80;
    factory_presets_[2].params[PARAM_VCF_TYPE] = 1;          // LP24
    factory_presets_[2].params[PARAM_VCF_ATTACK] = 5;
    factory_presets_[2].params[PARAM_VCF_DECAY] = 30;
    factory_presets_[2].params[PARAM_VCF_SUSTAIN] = 60;
    factory_presets_[2].params[PARAM_VCF_RELEASE] = 25;
    factory_presets_[2].params[PARAM_VCA_ATTACK] = 2;
    factory_presets_[2].params[PARAM_VCA_DECAY] = 30;
    factory_presets_[2].params[PARAM_VCA_SUSTAIN] = 100;
    factory_presets_[2].params[PARAM_VCA_RELEASE] = 20;
    factory_presets_[2].params[PARAM_LFO_RATE] = 50;
    factory_presets_[2].params[PARAM_LFO_WAVE] = 0;          // Triangle
    factory_presets_[2].params[PARAM_LFO_VCO_DEPTH] = 30;    // Vibrato
    factory_presets_[2].params[PARAM_LFO_VCF_DEPTH] = 0;
    strcpy(factory_presets_[2].name, "Lead");
    
    // Preset 3: Pad - Warm pad with detuned oscillators
    memset(&factory_presets_[3], 0, sizeof(Preset));
    factory_presets_[3].params[PARAM_DCO1_OCTAVE] = 64;      // 8'
    factory_presets_[3].params[PARAM_DCO1_WAVE] = 0;         // Ramp
    factory_presets_[3].params[PARAM_DCO1_PW] = 64;
    factory_presets_[3].params[PARAM_DCO1_LEVEL] = 100;
    factory_presets_[3].params[PARAM_DCO2_OCTAVE] = 64;      // 8'
    factory_presets_[3].params[PARAM_DCO2_WAVE] = 0;         // Ramp
    factory_presets_[3].params[PARAM_DCO2_DETUNE] = 68;      // Slight detune
    factory_presets_[3].params[PARAM_DCO2_LEVEL] = 90;
    factory_presets_[3].params[PARAM_VCF_CUTOFF] = 80;
    factory_presets_[3].params[PARAM_VCF_RESONANCE] = 25;
    factory_presets_[3].params[PARAM_VCF_ENV_AMT] = 70;
    factory_presets_[3].params[PARAM_VCF_TYPE] = 1;          // LP24
    factory_presets_[3].params[PARAM_VCF_ATTACK] = 45;
    factory_presets_[3].params[PARAM_VCF_DECAY] = 50;
    factory_presets_[3].params[PARAM_VCF_SUSTAIN] = 70;
    factory_presets_[3].params[PARAM_VCF_RELEASE] = 50;
    factory_presets_[3].params[PARAM_VCA_ATTACK] = 50;
    factory_presets_[3].params[PARAM_VCA_DECAY] = 50;
    factory_presets_[3].params[PARAM_VCA_SUSTAIN] = 100;
    factory_presets_[3].params[PARAM_VCA_RELEASE] = 70;
    factory_presets_[3].params[PARAM_LFO_RATE] = 35;
    factory_presets_[3].params[PARAM_LFO_WAVE] = 0;
    factory_presets_[3].params[PARAM_LFO_VCO_DEPTH] = 10;    // Subtle vibrato
    factory_presets_[3].params[PARAM_LFO_VCF_DEPTH] = 15;    // Filter movement
    strcpy(factory_presets_[3].name, "Pad");
    
    // Preset 4: Brass - Bright brass with slow attack
    memset(&factory_presets_[4], 0, sizeof(Preset));
    factory_presets_[4].params[PARAM_DCO1_OCTAVE] = 64;      // 8'
    factory_presets_[4].params[PARAM_DCO1_WAVE] = 0;         // Ramp
    factory_presets_[4].params[PARAM_DCO1_PW] = 64;
    factory_presets_[4].params[PARAM_DCO1_LEVEL] = 127;
    factory_presets_[4].params[PARAM_DCO2_OCTAVE] = 64;
    factory_presets_[4].params[PARAM_DCO2_WAVE] = 0;
    factory_presets_[4].params[PARAM_DCO2_DETUNE] = 64;
    factory_presets_[4].params[PARAM_DCO2_LEVEL] = 0;
    factory_presets_[4].params[PARAM_VCF_CUTOFF] = 75;
    factory_presets_[4].params[PARAM_VCF_RESONANCE] = 30;
    factory_presets_[4].params[PARAM_VCF_ENV_AMT] = 90;
    factory_presets_[4].params[PARAM_VCF_TYPE] = 1;          // LP24
    factory_presets_[4].params[PARAM_VCF_ATTACK] = 15;
    factory_presets_[4].params[PARAM_VCF_DECAY] = 45;
    factory_presets_[4].params[PARAM_VCF_SUSTAIN] = 65;
    factory_presets_[4].params[PARAM_VCF_RELEASE] = 35;
    factory_presets_[4].params[PARAM_VCA_ATTACK] = 15;
    factory_presets_[4].params[PARAM_VCA_DECAY] = 45;
    factory_presets_[4].params[PARAM_VCA_SUSTAIN] = 90;
    factory_presets_[4].params[PARAM_VCA_RELEASE] = 30;
    factory_presets_[4].params[PARAM_LFO_RATE] = 40;
    factory_presets_[4].params[PARAM_LFO_WAVE] = 0;
    factory_presets_[4].params[PARAM_LFO_VCO_DEPTH] = 0;
    factory_presets_[4].params[PARAM_LFO_VCF_DEPTH] = 0;
    strcpy(factory_presets_[4].name, "Brass");
    
    // Preset 5: Strings - Lush strings with detuned oscillators
    memset(&factory_presets_[5], 0, sizeof(Preset));
    factory_presets_[5].params[PARAM_DCO1_OCTAVE] = 64;      // 8'
    factory_presets_[5].params[PARAM_DCO1_WAVE] = 0;         // Ramp
    factory_presets_[5].params[PARAM_DCO1_PW] = 64;
    factory_presets_[5].params[PARAM_DCO1_LEVEL] = 110;
    factory_presets_[5].params[PARAM_DCO2_OCTAVE] = 64;      // 8'
    factory_presets_[5].params[PARAM_DCO2_WAVE] = 0;         // Ramp
    factory_presets_[5].params[PARAM_DCO2_DETUNE] = 70;      // More detune
    factory_presets_[5].params[PARAM_DCO2_LEVEL] = 100;
    factory_presets_[5].params[PARAM_VCF_CUTOFF] = 95;
    factory_presets_[5].params[PARAM_VCF_RESONANCE] = 20;
    factory_presets_[5].params[PARAM_VCF_ENV_AMT] = 60;
    factory_presets_[5].params[PARAM_VCF_TYPE] = 1;          // LP24
    factory_presets_[5].params[PARAM_VCF_ATTACK] = 60;
    factory_presets_[5].params[PARAM_VCF_DECAY] = 55;
    factory_presets_[5].params[PARAM_VCF_SUSTAIN] = 75;
    factory_presets_[5].params[PARAM_VCF_RELEASE] = 60;
    factory_presets_[5].params[PARAM_VCA_ATTACK] = 65;
    factory_presets_[5].params[PARAM_VCA_DECAY] = 55;
    factory_presets_[5].params[PARAM_VCA_SUSTAIN] = 100;
    factory_presets_[5].params[PARAM_VCA_RELEASE] = 80;
    factory_presets_[5].params[PARAM_LFO_RATE] = 38;
    factory_presets_[5].params[PARAM_LFO_WAVE] = 0;
    factory_presets_[5].params[PARAM_LFO_VCO_DEPTH] = 8;     // Very subtle vibrato
    factory_presets_[5].params[PARAM_LFO_VCF_DEPTH] = 12;    // Gentle filter movement
    strcpy(factory_presets_[5].name, "Strings");
}
