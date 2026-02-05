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
#include "polyphonic_renderer.h"
#include "mono_renderer.h"
#include "unison_renderer.h"
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

// Map DCO1 UI parameter value (0-4) to waveform enum
// DCO1 waveforms: SAW(0), SQR(1), PUL(2), TRI(3), SAW_PWM(4)
inline dsp::JupiterDCO::Waveform map_dco1_waveform(uint8_t value) {
    switch (value) {
        case 0: return dsp::JupiterDCO::WAVEFORM_SAW;
        case 1: return dsp::JupiterDCO::WAVEFORM_SQUARE;
        case 2: return dsp::JupiterDCO::WAVEFORM_PULSE;
        case 3: return dsp::JupiterDCO::WAVEFORM_TRIANGLE;
        case 4: return dsp::JupiterDCO::WAVEFORM_SAW_PWM;
        default: return dsp::JupiterDCO::WAVEFORM_SAW;
    }
}

// Map DCO2 UI parameter value (0-4) to waveform enum
// DCO2 waveforms: SAW(0), NSE(1), PUL(2), SIN(3), SAW_PWM(4)
// Note: DCO2 has different mapping than DCO1 - NOISE at index 1, SINE at index 3
inline dsp::JupiterDCO::Waveform map_dco2_waveform(uint8_t value) {
    switch (value) {
        case 0: return dsp::JupiterDCO::WAVEFORM_SAW;
        case 1: return dsp::JupiterDCO::WAVEFORM_NOISE;  // DCO2 has NOISE instead of SQUARE
        case 2: return dsp::JupiterDCO::WAVEFORM_PULSE;
        case 3: return dsp::JupiterDCO::WAVEFORM_SINE;   // DCO2 has SINE instead of TRIANGLE
        case 4: return dsp::JupiterDCO::WAVEFORM_SAW_PWM;
        default: return dsp::JupiterDCO::WAVEFORM_SAW;
    }
}

}  // namespace

DrupiterSynth::DrupiterSynth()
    : allocator_()
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
    , noise_seed_(0x12345678)
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
    
    // Initialize components
    dco1_.Init(sample_rate_);
    dco2_.Init(sample_rate_);
    vcf_.Init(sample_rate_);
    lfo_.Init(sample_rate_);
    env_vcf_.Init(sample_rate_);
    env_vca_.Init(sample_rate_);
    
    // Initialize parameter smoothing (start at 0, will be set by preset load)
    cutoff_smooth_.Init(0.0f, 0.005f);       // Filter cutoff - slow for smooth sweeps
    dco1_level_smooth_.Init(0.0f, 0.01f);    // DCO1 level - faster smoothing
    dco2_level_smooth_.Init(0.0f, 0.01f);    // DCO2 level - faster smoothing
    
    // Initialize knob catch mechanism (start at 0, will be set by preset load)
    catch_cutoff_.Init(0);
    catch_mix_.Init(0);
    catch_reso_.Init(0);
    catch_keyflw_.Init(0);
    catch_dco1_pw_.Init(0);
    catch_dco2_tune_.Init(50);    // Center position for bipolar detune
    catch_xmod_.Init(0);
    catch_lfo_rate_.Init(0);
    catch_mod_amt_.Init(0);
    
    // Initialize MIDI modulation smoothing (per-buffer processing)
    pitch_bend_smooth_.Init(0.0f, 0.005f);   // Pitch bend - slow for smooth vibrato
    pressure_smooth_.Init(0.0f, 0.01f);      // Channel pressure - medium smoothing for swell
    pitch_bend_semitones_ = 0.0f;
    channel_pressure_normalized_ = 0.0f;
    
    // Initialize chorus effect (delay-based stereo spread)
    space_widener_.Init(sample_rate_);
    space_widener_.SetDelayTime(15.0f);      // 15ms base delay
    space_widener_.SetModDepth(3.0f);        // ±3ms modulation
    space_widener_.SetLfoRate(0.5f);         // 0.5 Hz LFO
    space_widener_.SetMix(0.5f);             // 50% wet/dry mix
    
    // Initialize voice allocator (Hoover v2.0)
    allocator_.Init(sample_rate_);
    allocator_.SetMode(current_mode_);
    
    // Initialize MOD HUB UI cache (prevents flickering on mod hub page)
    last_mod_hub_dest_ = 255;
    last_mod_amt_value_ = 255;
    cached_mod_str_[0] = '\0';
    
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
    // No cleanup needed - all objects are static members
}

void DrupiterSynth::Reset() {
    gate_ = false;
    env_vcf_.Reset();
    env_vca_.Reset();
    lfo_.Reset();
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
        frames = kMaxFrames;
    }
    
    // Check buffer guard on entry (detect any previous overflow)
    if (buffer_guard_ != 0xDEADBEEFDEADBEEF) {
        buffer_guard_ = 0xDEADBEEFDEADBEEF;
    }
    
    // ============ Per-Buffer Setup ============
    #ifdef PERF_MON
    PERF_MON_START(perf_render_total_);
    #endif
    
    // Prepare rendering context (extracts ~150 lines of setup)
    RenderSetup setup = PrepareRenderSetup(frames);
    
    // ============ Main DSP loop - render to mix_buffer_ ============
    const float base_pw = current_preset_.params[PARAM_DCO1_PW] / 100.0f;
    
    for (uint32_t i = 0; i < frames; ++i) {
        // Process per-frame modulation (extracts ~70 lines)
        FrameModulation mod = ProcessFrameModulation(setup, base_pw);
        
        // === MODE-SPECIFIC OSCILLATOR PROCESSING ===
        #ifdef PERF_MON
        PERF_MON_START(perf_dco_);
        #endif
        
        // Delegate mode-specific rendering to specialized renderers
        if (current_mode_ == dsp::SYNTH_MODE_POLYPHONIC) {
            mixed_ = dsp::PolyphonicRenderer::RenderVoices(*this, mod.modulated_pw, setup.dco1_oct_mult, setup.dco2_oct_mult,
                                           setup.detune_ratio, setup.xmod_depth, setup.lfo_vco_depth, lfo_out_, mod.pitch_mod_ratio,
                                           setup.env_pitch_depth, mod.dco1_level, mod.dco2_level, mod.cutoff_base_nominal,
                                           setup.resonance, setup.vcf_mode, setup.hpf_alpha, setup.key_track, setup.smoothed_pressure,
                                           setup.env_vcf_depth, setup.lfo_vcf_depth, current_preset_.params[PARAM_DCO1_WAVE],
                                           current_preset_.params[PARAM_DCO2_WAVE], current_preset_.params[PARAM_VCF_CUTOFF],
                                           fast_pow2, semitones_to_ratio);
        } else if (current_mode_ == dsp::SYNTH_MODE_UNISON) {
            mixed_ = dsp::UnisonRenderer::RenderUnison(*this, mod.modulated_pw, setup.dco1_oct_mult, setup.dco2_oct_mult,
                                       setup.detune_ratio, setup.lfo_vco_depth, lfo_out_, mod.pitch_mod_ratio,
                                       setup.smoothed_pitch_bend, mod.dco1_level, mod.dco2_level,
                                       current_preset_.params[PARAM_DCO1_WAVE], semitones_to_ratio);
        } else {
            // MONO mode
            mixed_ = dsp::MonoRenderer::RenderMono(*this, mod.modulated_pw, setup.dco1_oct_mult, setup.dco2_oct_mult,
                                   setup.detune_ratio, setup.xmod_depth, setup.lfo_vco_depth, lfo_out_, mod.pitch_mod_ratio,
                                   setup.smoothed_pitch_bend, mod.dco1_level, mod.dco2_level,
                                   semitones_to_ratio);
        }
        
        #ifdef PERF_MON
        PERF_MON_END(perf_dco_);
        PERF_MON_START(perf_vcf_);
        #endif
        
        // Process filter and VCA (extracts ~90 lines)
        mix_buffer_[i] = ProcessFilterVcaFrame(setup, mixed_, mod.cutoff_base_nominal, lfo_out_);
        
        #ifdef PERF_MON
        PERF_MON_END(perf_vcf_);
        #endif
    }
    
    // ============ Output stage with effects ============
    #ifdef PERF_MON
    PERF_MON_START(perf_effects_);
    #endif
    
    // Finalize output (extracts ~50 lines)
    FinalizeOutput(frames, out);
    
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
                mod_hub_.SetValueForDest(v, current_preset_.hub_values[v]);
            }
            
            current_preset_.params[id] = v;
            return;  // Hub handles its own state
            
        case PARAM_MOD_AMT: {
            // Hub amount: Store in hub and preset's hub_values array
            v = clamp_u8_int32(value, 0, 100);
            int32_t caught_v = catch_mod_amt_.Update(v);
            const int32_t actual_value = mod_hub_.SetValueAndGetClamped(caught_v);
            
            // Store the ORIGINAL UI value (0-100), not the clamped value
            // This is critical for proper restoration when switching destinations
            {
                uint8_t dest = current_preset_.params[PARAM_MOD_HUB];
                if (dest < MOD_NUM_DESTINATIONS) {
                    // Store original 0-100 value for restoration later
                    current_preset_.hub_values[dest] = v;  // Store original, not clamped
                    
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
        }
            
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
                case 0: dco1_.SetWaveform(dsp::JupiterDCO::WAVEFORM_SAW); break;
                case 1: dco1_.SetWaveform(dsp::JupiterDCO::WAVEFORM_SQUARE); break;
                case 2: dco1_.SetWaveform(dsp::JupiterDCO::WAVEFORM_PULSE); break;
                case 3: dco1_.SetWaveform(dsp::JupiterDCO::WAVEFORM_TRIANGLE); break;
                case 4: dco1_.SetWaveform(dsp::JupiterDCO::WAVEFORM_SAW_PWM); break;  // Hoover v2.0!
            }
            break;
        case PARAM_DCO1_PW:
            {
                int32_t caught_value = catch_dco1_pw_.Update(v);
                dco1_.SetPulseWidth(caught_value / 100.0f);
            }
            break;
        case PARAM_XMOD:
            {
                int32_t caught_value = catch_xmod_.Update(v);
                xmod_depth_ = caught_value / 100.0f;
            }
            break;
            
        // ======== Page 2: DCO-2 ========
        case PARAM_DCO2_OCT:
            // Octave handled in Render()
            break;
        case PARAM_DCO2_WAVE:
            switch (v) {
                case 0: dco2_.SetWaveform(dsp::JupiterDCO::WAVEFORM_SAW); break;
                case 1: dco2_.SetWaveform(dsp::JupiterDCO::WAVEFORM_NOISE); break;
                case 2: dco2_.SetWaveform(dsp::JupiterDCO::WAVEFORM_PULSE); break;
                case 3: dco2_.SetWaveform(dsp::JupiterDCO::WAVEFORM_SINE); break;
                case 4: dco2_.SetWaveform(dsp::JupiterDCO::WAVEFORM_SAW_PWM); break;  // Hoover v2.0!
            }
            break;
        case PARAM_DCO2_TUNE:
            {
                int32_t caught_value = catch_dco2_tune_.Update(v);
                current_preset_.params[id] = caught_value;  // Update with caught value
            }
            break;
        case PARAM_SYNC:
            sync_mode_ = v;
            break;
            
        // ======== Page 3: MIX & VCF ========
        case PARAM_OSC_MIX:
            {
                int32_t caught_value = catch_mix_.Update(v);
                current_preset_.params[id] = caught_value;  // Update with caught value
            }
            break;
        case PARAM_VCF_CUTOFF:
            {
                int32_t caught_value = catch_cutoff_.Update(v);
                cutoff_smooth_.SetTarget(caught_value / 100.0f);
            }
            break;
        case PARAM_VCF_RESONANCE:
            {
                int32_t caught_value = catch_reso_.Update(v);
                vcf_.SetResonance(caught_value / 100.0f);
            }
            break;
        case PARAM_VCF_KEYFLW:
            {
                int32_t caught_value = catch_keyflw_.Update(v);
                current_preset_.params[id] = caught_value;  // Update with caught value
            }
            break;
        
        // ======== Page 4: VCF Envelope ========
        case PARAM_VCF_ATTACK:
            env_vcf_.SetAttack(ParameterToEnvelopeTime(v));
            // Task 2.2.1: Also set per-voice pitch envelopes
            for (uint8_t i = 0; i < DRUPITER_MAX_VOICES; i++) {
                allocator_.GetVoiceMutable(i).env_pitch.SetAttack(ParameterToEnvelopeTime(v));
            }
            break;
        case PARAM_VCF_DECAY:
            env_vcf_.SetDecay(ParameterToEnvelopeTime(v));
            for (uint8_t i = 0; i < DRUPITER_MAX_VOICES; i++) {
                allocator_.GetVoiceMutable(i).env_pitch.SetDecay(ParameterToEnvelopeTime(v));
            }
            break;
        case PARAM_VCF_SUSTAIN:
            env_vcf_.SetSustain(v / 100.0f);
            for (uint8_t i = 0; i < DRUPITER_MAX_VOICES; i++) {
                allocator_.GetVoiceMutable(i).env_pitch.SetSustain(v / 100.0f);
            }
            break;
        case PARAM_VCF_RELEASE:
            env_vcf_.SetRelease(ParameterToEnvelopeTime(v));
            for (uint8_t i = 0; i < DRUPITER_MAX_VOICES; i++) {
                allocator_.GetVoiceMutable(i).env_pitch.SetRelease(ParameterToEnvelopeTime(v));
            }
            break;
        
        // ======== Page 5: VCA Envelope ========
        case PARAM_VCA_ATTACK:
#ifdef DEBUG
            fprintf(stderr, "[Param] VCA_ATTACK value=%d . time=%.6fs\n", v, ParameterToEnvelopeTime(v));
            fflush(stderr);
#endif
            env_vca_.SetAttack(ParameterToEnvelopeTime(v));
            // Update all polyphonic voice envelopes
            for (uint8_t i = 0; i < DRUPITER_MAX_VOICES; i++) {
                allocator_.GetVoiceMutable(i).env_amp.SetAttack(ParameterToEnvelopeTime(v));
            }
            break;
        case PARAM_VCA_DECAY:
#ifdef DEBUG
            fprintf(stderr, "[Param] VCA_DECAY value=%d . time=%.6fs\n", v, ParameterToEnvelopeTime(v));
            fflush(stderr);
#endif
            env_vca_.SetDecay(ParameterToEnvelopeTime(v));
            // Update all polyphonic voice envelopes
            for (uint8_t i = 0; i < DRUPITER_MAX_VOICES; i++) {
                allocator_.GetVoiceMutable(i).env_amp.SetDecay(ParameterToEnvelopeTime(v));
            }
            break;
        case PARAM_VCA_SUSTAIN:
            env_vca_.SetSustain(v / 100.0f);
            // Update all polyphonic voice envelopes
            for (uint8_t i = 0; i < DRUPITER_MAX_VOICES; i++) {
                allocator_.GetVoiceMutable(i).env_amp.SetSustain(v / 100.0f);
            }
            break;
        case PARAM_VCA_RELEASE:
#ifdef DEBUG
            fprintf(stderr, "[Param] VCA_RELEASE value=%d . time=%.6fs\n", v, ParameterToEnvelopeTime(v));
            fflush(stderr);
#endif
            env_vca_.SetRelease(ParameterToEnvelopeTime(v));
            // Update all polyphonic voice envelopes
            for (uint8_t i = 0; i < DRUPITER_MAX_VOICES; i++) {
                allocator_.GetVoiceMutable(i).env_amp.SetRelease(ParameterToEnvelopeTime(v));
            }
            break;
        
        // ======== Page 6: LFO, MOD HUB & Effects ========
        case PARAM_LFO_RATE:
            {
                int32_t caught_value = catch_lfo_rate_.Update(v);
                lfo_.SetFrequency(ParameterToExponentialFreq(caught_value, 0.1f, 20.0f));
            }
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
        return mod_hub_.GetOriginalValue(dest);
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
            // OPTIMIZATION: Cache the formatted string to prevent flickering
            // Only reformat if destination or value actually changed
            uint8_t current_dest = mod_hub_.GetDestination();
            uint8_t current_value = static_cast<uint8_t>(value & 0xFF);
            
            // Check if cache is valid (destination or value changed)
            if (current_dest == last_mod_hub_dest_ && 
                current_value == last_mod_amt_value_) {
                // Cache hit - return cached string (prevents UI flickering)
                return cached_mod_str_;
            }
            
            // Cache miss - reformat string
            last_mod_hub_dest_ = current_dest;
            last_mod_amt_value_ = current_value;
            
            // Use common buffer for formatting, then copy to cache
            const char* str = mod_hub_.GetCurrentValueString(mod_value_str_, sizeof(mod_value_str_));
            if (str != nullptr) {
                strncpy(cached_mod_str_, str, sizeof(cached_mod_str_) - 1);
                cached_mod_str_[sizeof(cached_mod_str_) - 1] = '\0';
            } else {
                cached_mod_str_[0] = '\0';
            }
            
            return cached_mod_str_;
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
    current_note_ = note;
    current_freq_hz_ = common::MidiHelper::NoteToFreq(note);
    current_velocity_ = velocity;
    
    // Trigger envelopes
    env_vca_.NoteOn();
    env_vcf_.NoteOn();
    
    // Phase 3: LFO key trigger (JP-8 feature)
    // Reset LFO phase on each note-on for synchronized modulation
    lfo_.Trigger();
}

void DrupiterSynth::NoteOff(uint8_t note) {
#ifdef DEBUG
    fprintf(stderr, "[Synth] NoteOff: note=%d\n", note);
    fflush(stderr);
#endif
    
    // Route through voice allocator (Hoover v2.0)
    // VoiceAllocator handles retrigger logic internally for voice-level state
    allocator_.NoteOff(note);
    
    // Update synth-level state for mono/unison modes
    // In these modes, voice[0] is always the active voice
    if (current_mode_ != dsp::SYNTH_MODE_POLYPHONIC) {
        bool has_held_notes = allocator_.HasHeldNotes();
        
        if (has_held_notes) {
            // Get the current note from voice allocator (it handles last-note priority)
            const auto& voice = allocator_.GetVoice(0);
            if (voice.active && voice.midi_note != current_note_) {
                // Voice allocator retriggered to a different note
                current_note_ = voice.midi_note;
                current_freq_hz_ = common::MidiHelper::NoteToFreq(voice.midi_note);
                
#ifdef DEBUG
                fprintf(stderr, "[Synth] Synced to retriggered note %d (freq=%.2f Hz)\n",
                        voice.midi_note, current_freq_hz_);
                fflush(stderr);
#endif
            }
        } else {
            // No notes held - release main envelopes
            env_vca_.NoteOff();
            env_vcf_.NoteOff();
            current_note_ = 0;
            
#ifdef DEBUG
            fprintf(stderr, "[Synth] No held notes, releasing main envelopes\n");
            fflush(stderr);
#endif
        }
    }
}

void DrupiterSynth::AllNoteOff() {
    // Route through voice allocator (Hoover v2.0)
    allocator_.AllNotesOff();
}

void DrupiterSynth::PitchBend(uint16_t bend) {
    // Convert MIDI pitch bend (0-16383, center=8192) to semitones (±2 range)
    using common::MidiHelper;
    pitch_bend_semitones_ = MidiHelper::PitchBendToSemitones(bend, 2.0f);
    
    // Set smoothing target for per-buffer processing
    // 0.005f coefficient: ~200ms smoothing, musical vibrato response
    pitch_bend_smooth_.SetTarget(pitch_bend_semitones_);
}

void DrupiterSynth::ChannelPressure(uint8_t pressure) {
    // Convert MIDI channel pressure (0-127) to normalized float (0.0-1.0)
    using common::MidiHelper;
    channel_pressure_normalized_ = MidiHelper::PressureToFloat(pressure);
    
    // Set smoothing target for per-buffer processing
    // 0.01f coefficient: ~100ms smoothing, smooth swell response
    pressure_smooth_.SetTarget(channel_pressure_normalized_);
}

void DrupiterSynth::Aftertouch(uint8_t note, uint8_t aftertouch) {
    // Simplified monophonic implementation:
    // Only apply aftertouch for the currently playing note
    (void)note;  // For polyphonic support, would track per-voice
    ChannelPressure(aftertouch);
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
    {
        cutoff_smooth_.SetImmediate(current_preset_.params[PARAM_VCF_CUTOFF] / 100.0f);
    }
    
    // Initialize catchable values with preset values (assume knob = preset initially)
    // This prevents catching until the knob is actually moved away from preset value
    catch_cutoff_.Init(current_preset_.params[PARAM_VCF_CUTOFF]);
    catch_mix_.Init(current_preset_.params[PARAM_OSC_MIX]);
    catch_reso_.Init(current_preset_.params[PARAM_VCF_RESONANCE]);
    catch_keyflw_.Init(current_preset_.params[PARAM_VCF_KEYFLW]);
    catch_dco1_pw_.Init(current_preset_.params[PARAM_DCO1_PW]);
    catch_dco2_tune_.Init(current_preset_.params[PARAM_DCO2_TUNE]);
    catch_xmod_.Init(current_preset_.params[PARAM_XMOD]);
    catch_lfo_rate_.Init(current_preset_.params[PARAM_LFO_RATE]);
    // MOD_AMT: Initialize with the current destination's hub value
    {
        uint8_t dest = current_preset_.params[PARAM_MOD_HUB];
        if (dest < MOD_NUM_DESTINATIONS) {
            catch_mod_amt_.Init(current_preset_.hub_values[dest]);
        }
    }
    
    // DCO levels from OSC_MIX parameter
    float osc_mix = current_preset_.params[PARAM_OSC_MIX] / 100.0f;
    {
        dco1_level_smooth_.SetImmediate(1.0f - osc_mix);
    }
    {
        dco2_level_smooth_.SetImmediate(osc_mix);
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
    // space_widener_ is now static object return;
    
    switch (effect_mode) {
        case 0:  // CHORUS mode: Moderate modulation depth, less delay time
            space_widener_.SetModDepth(3.0f);      // 3ms modulation depth
            space_widener_.SetMix(0.5f);           // 50% wet/dry mix
            space_widener_.SetDelayTime(15.0f);    // 15ms delay time
            break;
        case 1:  // SPACE mode: Wider stereo, more delay time
            space_widener_.SetModDepth(6.0f);      // 6ms modulation depth
            space_widener_.SetMix(0.7f);           // 70% wet/dry mix
            space_widener_.SetDelayTime(35.0f);    // 35ms delay time for wide stereo
            break;
        case 2:  // DRY mode: No effect processing (handled in Render)
            break;
        case 3:  // BOTH mode: Maximum effect (chorus + space combined)
            space_widener_.SetModDepth(8.0f);      // 8ms modulation depth
            space_widener_.SetMix(0.8f);           // 80% wet/dry mix
            space_widener_.SetDelayTime(40.0f);    // 40ms delay time
            break;
    }
}

void DrupiterSynth::UpdateLfoSettings() {
    // Update LFO settings when hub parameters change
    // Called from SetParameter when MOD_HUB or MOD_AMT changes
    // lfo_ is now static object return;
    
    // Apply LFO delay setting (0-100 maps to 0-5 seconds)
    const uint8_t lfo_delay = mod_hub_.GetValue(MOD_LFO_DELAY);
    const float lfo_delay_sec = (lfo_delay / 100.0f) * 5.0f;
    lfo_.SetDelay(lfo_delay_sec);
    
    // Apply LFO waveform (0-3: TRI/RAMP/SQR/S&H)
    const uint8_t lfo_wave = mod_hub_.GetValue(MOD_LFO_WAVE);
    lfo_.SetWaveform(static_cast<dsp::JupiterLFO::Waveform>(lfo_wave & 0x03));
}

// ============================================================================
// Render Helper Method Implementations
// ============================================================================

DrupiterSynth::RenderSetup DrupiterSynth::PrepareRenderSetup(uint32_t frames) {
    RenderSetup setup;
    
    // Read oscillator parameters
    const float osc_mix = current_preset_.params[PARAM_OSC_MIX] / 100.0f;
    const float dco1_level_target = 1.0f - osc_mix;
    const float dco2_level_target = osc_mix;
    
    // Update smoothed parameter targets
    cutoff_smooth_.SetTarget(current_preset_.params[PARAM_VCF_CUTOFF] / 100.0f);
    dco1_level_smooth_.SetTarget(dco1_level_target);
    dco2_level_smooth_.SetTarget(dco2_level_target);
    
    // VCF resonance
    setup.resonance = current_preset_.params[PARAM_VCF_RESONANCE] / 100.0f;
    
    // Process MIDI modulation smoothing
    setup.smoothed_pitch_bend = pitch_bend_smooth_.Process();
    setup.smoothed_pressure = pressure_smooth_.Process();
    
    // Pre-calculate DCO constants
    setup.dco1_oct_mult = Dco1OctaveToMultiplier(current_preset_.params[PARAM_DCO1_OCT]);
    setup.dco2_oct_mult = Dco2OctaveToMultiplier(current_preset_.params[PARAM_DCO2_OCT]);
    
    // Detune
    const int32_t detune_param = current_preset_.params[PARAM_DCO2_TUNE];
    const float detune_cents = (static_cast<float>(detune_param) - 50.0f);
    setup.detune_ratio = cents_to_ratio(detune_cents);
    
    // Cross-modulation depth
    setup.xmod_depth = xmod_depth_;
    
    // Read MOD HUB values
    setup.lfo_pwm_depth = mod_hub_.GetValueNormalizedUnipolar(MOD_LFO_TO_PWM);
    setup.lfo_vcf_depth = mod_hub_.GetValueNormalizedUnipolar(MOD_LFO_TO_VCF);
    setup.lfo_vco_depth = mod_hub_.GetValueNormalizedUnipolar(MOD_LFO_TO_VCO);
    setup.env_pwm_depth = mod_hub_.GetValueNormalizedUnipolar(MOD_ENV_TO_PWM);
    setup.env_vcf_depth = mod_hub_.GetValueNormalizedBipolar(MOD_ENV_TO_VCF);
    setup.env_pitch_depth = mod_hub_.GetValueScaledBipolar(MOD_ENV_TO_PITCH, 12.0f);
    setup.lfo_env_amt = mod_hub_.GetValueNormalizedUnipolar(MOD_LFO_ENV_AMT);
    setup.vca_level = mod_hub_.GetValueNormalizedUnipolar(MOD_VCA_LEVEL);
    setup.vca_lfo_depth = mod_hub_.GetValueNormalizedUnipolar(MOD_VCA_LFO);
    setup.vca_kybd = mod_hub_.GetValueNormalizedUnipolar(MOD_VCA_KYBD);
    
    // Update synthesis mode and allocator settings
    const uint8_t synth_mode_value = mod_hub_.GetValue(MOD_SYNTH_MODE);
    const dsp::SynthMode synth_mode = static_cast<dsp::SynthMode>(synth_mode_value < 3 ? synth_mode_value : 0);
    if (synth_mode != current_mode_) {
        current_mode_ = synth_mode;
        allocator_.SetMode(current_mode_);
    }
    
    // Unison detune control
    const float unison_detune_cents = static_cast<float>(mod_hub_.GetValue(MOD_UNISON_DETUNE));
    allocator_.SetUnisonDetune(unison_detune_cents);
    
    // Portamento time control
    const uint8_t porta_param = mod_hub_.GetValue(MOD_PORTAMENTO_TIME);
    float porta_time_ms = 0.0f;
    if (porta_param > 0) {
        const float porta_normalized = porta_param / 100.0f;
        porta_time_ms = 10.0f * powf(50.0f, porta_normalized);
    }
    allocator_.SetPortamentoTime(porta_time_ms);
    
    // VCF filter type
    const uint8_t vcf_type = mod_hub_.GetValue(MOD_VCF_TYPE);
    setup.vcf_mode = (vcf_type < 50) ? dsp::JupiterVCF::MODE_LP12 : dsp::JupiterVCF::MODE_LP24;
    vcf_.SetMode(setup.vcf_mode);
    
    // Pre-calculate HPF coefficient
    const uint8_t hpf_cutoff = mod_hub_.GetValue(MOD_HPF);
    setup.hpf_alpha = 0.0f;
    if (hpf_cutoff > 0) {
        const float hpf_freq = 20.0f + (hpf_cutoff / 100.0f) * 1980.0f;
        const float rc = 1.0f / (2.0f * M_PI * hpf_freq);
        const float dt = 1.0f / sample_rate_;
        setup.hpf_alpha = rc / (rc + dt);
    }
    
    // Keyboard tracking
    setup.key_track = (current_preset_.params[PARAM_VCF_KEYFLW] / 100.0f) * 1.2f;
    
    // Velocity modulation for MONO/UNISON modes
    setup.vel_mod = (current_velocity_ / 127.0f) * 0.5f;
    
    // Process portamento/glide for MONO/UNISON modes
    if (current_mode_ == dsp::SYNTH_MODE_MONOPHONIC || 
        current_mode_ == dsp::SYNTH_MODE_UNISON) {
        dsp::Voice& voice0 = allocator_.GetVoiceMutable(0);
        if (voice0.is_gliding) {
            float log_pitch = logf(voice0.pitch_hz);
            log_pitch += voice0.glide_increment * frames;
            
            float log_target = logf(voice0.glide_target_hz);
            if ((voice0.glide_increment > 0.0f && log_pitch >= log_target) ||
                (voice0.glide_increment < 0.0f && log_pitch <= log_target)) {
                voice0.pitch_hz = voice0.glide_target_hz;
                voice0.is_gliding = false;
            } else {
                voice0.pitch_hz = expf(log_pitch);
            }
            
            current_freq_hz_ = voice0.pitch_hz;
        }
    }
    
    return setup;
}

DrupiterSynth::FrameModulation DrupiterSynth::ProcessFrameModulation(
    const RenderSetup& setup, 
    float base_pw
) {
    FrameModulation mod;
    
    // Process envelopes FIRST
    vcf_env_out_ = env_vcf_.Process();
    vca_env_out_ = env_vca_.Process();
    
    // Process LFO with optional envelope rate modulation
    if (setup.lfo_env_amt > kMinModulation) {
        const float rate_mult = 1.0f + (vcf_env_out_ * setup.lfo_env_amt);
        lfo_out_ = lfo_.Process() * rate_mult;
    } else {
        lfo_out_ = lfo_.Process();
    }
    
    // Get smoothed oscillator levels
    mod.dco1_level = dco1_level_smooth_.Process();
    mod.dco2_level = dco2_level_smooth_.Process();
    
    // Apply PWM modulation
    float pw_mod = 0.0f;
    if (setup.lfo_pwm_depth > kMinModulation) {
        pw_mod += lfo_out_ * setup.lfo_pwm_depth * 0.4f;
    }
    if (setup.env_pwm_depth > kMinModulation) {
        pw_mod += vcf_env_out_ * setup.env_pwm_depth * 0.4f;
    }
    mod.modulated_pw = clampf(base_pw + pw_mod, 0.1f, 0.9f);
    
    // Process cutoff smoothing and mapping
    const float cutoff_norm = cutoff_smooth_.Process();
    const float linear_blend = 0.15f;
    const float exp_portion = fast_pow2(cutoff_norm * 7.0f);
    const float lin_portion = 1.0f + cutoff_norm * 127.0f;
    const float cutoff_mult = linear_blend * lin_portion + (1.0f - linear_blend) * exp_portion;
    mod.cutoff_base_nominal = 100.0f * cutoff_mult;
    
    // Pre-calculate pitch envelope modulation ratio
    mod.pitch_mod_ratio = 1.0f;
    if (fabsf(setup.env_pitch_depth) > kMinModulation) {
        mod.pitch_mod_ratio = fasterpow2f(vcf_env_out_ * setup.env_pitch_depth / 12.0f);
    }
    
    return mod;
}

float DrupiterSynth::ProcessFilterVcaFrame(
    const RenderSetup& setup,
    float mixed,
    float cutoff_base_nominal,
    float lfo_out
) {
    // Soft clamp mixed oscillators
    if (mixed > 1.2f) mixed = 1.2f + 0.5f * (mixed - 1.2f);
    else if (mixed < -1.2f) mixed = -1.2f + 0.5f * (mixed + 1.2f);
    
    // Apply HPF (MONO and UNISON modes only)
    float hpf_out = mixed;
    if (setup.hpf_alpha > 0.0f && current_mode_ != dsp::SYNTH_MODE_POLYPHONIC) {
        hpf_out = setup.hpf_alpha * (hpf_prev_output_ + mixed - hpf_prev_input_);
        hpf_prev_output_ = hpf_out;
        hpf_prev_input_ = mixed;
        
        if (hpf_out > 1.5f) hpf_out = 1.5f + 0.3f * (hpf_out - 1.5f);
        else if (hpf_out < -1.5f) hpf_out = -1.5f + 0.3f * (hpf_out + 1.5f);
    }
    
    // Apply keyboard tracking for MONO/UNISON modes
    float cutoff_base = cutoff_base_nominal;
    const float note_offset = (static_cast<int32_t>(current_note_) - 60) / 12.0f;
    const float tracking_exponent = note_offset * setup.key_track;
    const float clamped_exponent = (tracking_exponent > 4.0f) ? 4.0f :
                      (tracking_exponent < -4.0f) ? -4.0f : tracking_exponent;
    cutoff_base *= semitones_to_ratio(clamped_exponent * 12.0f);

    // Combine all modulation sources
    float total_mod = vcf_env_out_ * 2.0f
                    + setup.env_vcf_depth * vcf_env_out_
                    + lfo_out * setup.lfo_vcf_depth * 1.0f
                    + setup.vel_mod * 2.0f
                    + setup.smoothed_pressure * 1.0f;

    if (total_mod > 3.0f) total_mod = 3.0f;
    else if (total_mod < -3.0f) total_mod = -3.0f;
    
    float cutoff_modulated = cutoff_base * fast_pow2(total_mod);
    
    // Update filter if cutoff changed significantly
    const float cutoff_diff = cutoff_modulated - last_cutoff_hz_;
    if (cutoff_diff > 1.0f || cutoff_diff < -1.0f) {
        vcf_.SetCutoffModulated(cutoff_modulated);
        last_cutoff_hz_ = cutoff_modulated;
    }
    
    // Apply VCF
    float filtered;
    if (current_mode_ == dsp::SYNTH_MODE_POLYPHONIC) {
        filtered = hpf_out;
    } else if (current_preset_.params[PARAM_VCF_CUTOFF] >= 100) {
        filtered = hpf_out;
    } else {
        filtered = vcf_.Process(hpf_out);
        if (filtered > 1.8f) filtered = 1.8f + 0.2f * (filtered - 1.8f);
        else if (filtered < -1.8f) filtered = -1.8f + 0.2f * (filtered + 1.8f);
    }
    
    // Apply VCA
    float vca_gain = (current_mode_ == dsp::SYNTH_MODE_POLYPHONIC) ? 1.0f : vca_env_out_;
    
    if (current_mode_ != dsp::SYNTH_MODE_POLYPHONIC) {
        const float velocity_vca = 0.2f + (current_velocity_ / 127.0f) * 0.8f;
        vca_gain *= velocity_vca;
    }
    
    if (current_mode_ == dsp::SYNTH_MODE_UNISON) {
        dsp::Voice& lead_voice = allocator_.GetVoiceMutable(0);
        vca_gain = lead_voice.env_amp.Process();
        
        if (!lead_voice.env_amp.IsActive()) {
            vca_gain = 0.0f;
            allocator_.MarkVoiceInactive(0);
        }
    } else if (current_mode_ == dsp::SYNTH_MODE_POLYPHONIC) {
        vca_gain = 1.0f;
    }
    
    // VCA LFO (tremolo)
    if (setup.vca_lfo_depth > kMinModulation) {
        const int vca_lfo_switch = static_cast<int>(setup.vca_lfo_depth * 3.999f);
        static constexpr float kVcaLfoDepths[4] = {0.0f, 0.33f, 0.67f, 1.0f};
        const float quantized_depth = kVcaLfoDepths[vca_lfo_switch];
        const float tremolo = 1.0f + (lfo_out * quantized_depth * 0.5f);
        vca_gain *= tremolo;
    }
    
    // VCA Keyboard tracking
    if (setup.vca_kybd > kMinModulation) {
        const float note_offset = (static_cast<int32_t>(current_note_) - 60) / 12.0f;
        const float kb_gain = 1.0f + (note_offset * setup.vca_kybd * 0.5f);
        vca_gain *= kb_gain;
    }
    
    // VCA Level control
    vca_gain *= setup.vca_level;
    
    return filtered * vca_gain * 0.6f;
}

void DrupiterSynth::FinalizeOutput(uint32_t frames, float* out) {
    // Sanitize buffer
    drupiter::neon::SanitizeAndClamp(mix_buffer_, 1.0f, frames);
    
    // Effect mode processing
    const uint8_t effect_mode = current_preset_.params[PARAM_EFFECT];
    
    if (effect_mode == 2) {
        // DRY mode: Bypass effects
        for (uint32_t i = 0; i < frames; i++) {
            left_buffer_[i] = mix_buffer_[i];
            right_buffer_[i] = mix_buffer_[i];
        }
    } else {
        // Process with pre-configured parameters
        space_widener_.ProcessMonoBatch(mix_buffer_, left_buffer_, right_buffer_, frames);
    }
    
    // Add DC offset for denormal protection
    const float denormal_offset = 1.0e-15f;
    drupiter::neon::ApplyGain(left_buffer_, 1.0f, frames);
    drupiter::neon::ApplyGain(right_buffer_, 1.0f, frames);
    
    // Interleave stereo
    drupiter::neon::InterleaveStereo(left_buffer_, right_buffer_, out, frames);
    
    // Add DC offset to output
    for (uint32_t i = 0; i < frames * 2; ++i) {
        out[i] += denormal_offset;
    }
}
