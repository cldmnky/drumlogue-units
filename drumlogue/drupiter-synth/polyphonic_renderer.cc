/**
 * @file polyphonic_renderer.cc
 * @brief Polyphonic mode renderer implementation for Drupiter synth
 */

#include "polyphonic_renderer.h"
#include "dsp/jupiter_dco.h"
#include "dsp/jupiter_vcf.h"
#include "../common/dsp_utils.h"
#include "../common/neon_dsp.h"
#include <cmath>
#include <cstdio>

#ifdef USE_NEON
#include <arm_neon.h>
#endif

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

// Threshold constants for modulation and distance checks
static constexpr float kMinModulation = 0.001f;  // Minimum significant modulation depth

// Fast pow2 approximation using bit manipulation (from jupiter_dco.cc)
DRUMLOGUE_ALWAYS_INLINE float fasterpow2f(float p) {
    float clipp = (p < -126.0f) ? -126.0f : p;
    union { uint32_t i; float f; } v = {
        static_cast<uint32_t>((1 << 23) * (clipp + 126.94269504f))
    };
    return v.f;
}

// Cached state for performance optimizations
static uint8_t cached_dco1_wave = 255;  // Cache to avoid redundant SetWaveform calls
static uint8_t cached_dco2_wave = 255;
static float cached_inv_voice_count = 1.0f;  // Pre-calculated reciprocal of voice count
static uint8_t cached_active_voice_count = 0;

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

namespace dsp {

float PolyphonicRenderer::RenderVoices(
    DrupiterSynth& synth,
    uint32_t frames,
    float modulated_pw,
    float dco1_oct_mult,
    float dco2_oct_mult,
    float detune_ratio,
    float /*xmod_depth*/,  // Unused in polyphonic mode (saves CPU)
    float lfo_vco_depth,
    float lfo_out,
    float pitch_mod_ratio,
    float env_pitch_depth,
    float dco1_level,
    float dco2_level,
    float cutoff_base_nominal,
    float resonance,
    JupiterVCF::Mode vcf_mode,
    float hpf_alpha,
    float key_track,
    float smoothed_pressure,
    float env_vcf_depth,
    float lfo_vcf_depth,
    uint8_t dco1_wave_param,
    uint8_t dco2_wave_param,
    uint8_t vcf_cutoff_param,
    float (*fast_pow2)(float),
    float (*semitones_to_ratio)(float)
) {
    // POLYPHONIC MODE: Render and mix multiple independent voices
    float mixed = 0.0f;
    uint8_t active_voice_count = 0;
    
    // OPTIMIZATION: Check HPF condition once before voice loop instead of per-voice
    const bool apply_hpf = hpf_alpha > 0.0f;

    // Render each active voice
    for (uint8_t v = 0; v < DRUPITER_MAX_VOICES; v++) {
        const dsp::Voice& voice = synth.GetAllocator().GetVoice(v);

        // Skip inactive voices (no note or envelope finished)
        if (!voice.active && !voice.env_amp.IsActive()) {
            continue;
        }

        active_voice_count++;

        // Get non-const access to voice for processing
        dsp::Voice& voice_mut = const_cast<dsp::Voice&>(voice);

        // Task 2.2.4: Process portamento/glide
        // CRITICAL: Apply glide increment for EACH FRAME in the buffer, not just once
        // OPTIMIZATION: Use multiplication instead of expensive log/exp operations
        if (voice_mut.is_gliding) {
            const float glide_ratio = 1.0f + voice_mut.glide_increment * frames;
            voice_mut.pitch_hz *= glide_ratio;

            // Check if we've reached target (compare in frequency domain directly)
            if ((voice_mut.glide_increment > 0.0f && voice_mut.pitch_hz >= voice_mut.glide_target_hz) ||
                (voice_mut.glide_increment < 0.0f && voice_mut.pitch_hz <= voice_mut.glide_target_hz)) {
                voice_mut.pitch_hz = voice_mut.glide_target_hz;
                voice_mut.is_gliding = false;
            }
        }

        // Set voice-specific parameters using proper waveform mapping
        // DCO1 and DCO2 have different waveform sets at the same UI indices
        // OPTIMIZATION: Only call SetWaveform if waveform parameter changed
        if (dco1_wave_param != cached_dco1_wave) {
            voice_mut.dco1.SetWaveform(map_dco1_waveform(dco1_wave_param));
            cached_dco1_wave = dco1_wave_param;
        }
        if (dco2_wave_param != cached_dco2_wave) {
            voice_mut.dco2.SetWaveform(map_dco2_waveform(dco2_wave_param));
            cached_dco2_wave = dco2_wave_param;
        }
        voice_mut.dco1.SetPulseWidth(modulated_pw);
        voice_mut.dco2.SetPulseWidth(modulated_pw);

        // Calculate frequencies for this voice
        float voice_freq1 = voice.pitch_hz * dco1_oct_mult;
        float voice_freq2 = voice.pitch_hz * dco2_oct_mult * detune_ratio;

        // Apply LFO vibrato with NEON vectorization when available
        if (lfo_vco_depth > kMinModulation) {
            const float lfo_mod = 1.0f + lfo_out * lfo_vco_depth * 0.05f;
#ifdef USE_NEON
            // OPTIMIZATION: Vectorize - multiply both frequencies by lfo_mod simultaneously
            float32x2_t freq_pair = vld1_f32(&voice_freq1);
            float32x2_t lfo_vec = vdup_n_f32(lfo_mod);
            freq_pair = vmul_f32(freq_pair, lfo_vec);
            vst1_f32(&voice_freq1, freq_pair);
#else
            voice_freq1 *= lfo_mod;
            voice_freq2 *= lfo_mod;
#endif
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

        // Process voice oscillators
        float voice_dco1 = voice_mut.dco1.Process();
        float voice_dco2 = 0.0f;

        if (dco2_level > kMinModulation) {
            voice_dco2 = voice_mut.dco2.Process();
        }

        // Mix this voice's oscillators
        float voice_mix = voice_dco1 * dco1_level + voice_dco2 * dco2_level;

        // Process voice envelope (each voice has its own envelope)
        float voice_env = voice_mut.env_amp.Process();

        // ============================================================
        // PHASE 1: Per-voice VCF processing (polyphonic mode)
        // ============================================================
        const float voice_vcf_env = voice_mut.env_filter.Process();

        // Apply per-voice HPF (Phase 2: one-pole high-pass filter)
        // OPTIMIZATION: HPF check moved outside loop - apply_hpf already determined once per frame
        float voice_hpf_out = voice_mix;
        if (apply_hpf) {
            voice_hpf_out = hpf_alpha * (voice_mut.hpf_prev_output + voice_mix - voice_mut.hpf_prev_input);
            voice_mut.hpf_prev_output = voice_hpf_out;
            voice_mut.hpf_prev_input = voice_mix;
        }

        // Apply per-voice keyboard tracking
        float voice_cutoff_base = cutoff_base_nominal;
        const float voice_note_offset = (static_cast<int32_t>(voice.midi_note) - 60) / 12.0f;
        const float voice_tracking_exponent = voice_note_offset * key_track;
        const float voice_clamped_exponent = (voice_tracking_exponent > 4.0f) ? 4.0f :
                                             (voice_tracking_exponent < -4.0f) ? -4.0f : voice_tracking_exponent;
        voice_cutoff_base *= semitones_to_ratio(voice_clamped_exponent * 12.0f);

        // Per-voice velocity modulation for VCF (0-50% scaled)
        const float voice_vel_mod = (voice.velocity / 127.0f) * 0.5f;

        // Combine envelope, LFO, velocity, and pressure modulation (shared sources)
        float voice_total_mod = voice_vcf_env * 2.0f              // Base envelope modulation
                              + env_vcf_depth * voice_vcf_env     // Hub envelope.VCF modulation
                              + lfo_out * lfo_vcf_depth * 1.0f    // LFO modulation
                              + voice_vel_mod * 2.0f              // Velocity adds up to +2 octaves
                              + smoothed_pressure * 1.0f;         // Channel pressure adds up to +1 octave

        // Clamp modulation depth to avoid extreme cutoff values (branchless, from common/dsp_utils.h)
        voice_total_mod = clampf(voice_total_mod, -3.0f, 3.0f);

        // Apply modulation to cutoff base
        const float voice_cutoff_modulated = voice_cutoff_base * fast_pow2(voice_total_mod);

        // Set per-voice filter parameters and process
        voice_mut.vcf.SetCutoffModulated(voice_cutoff_modulated);
        voice_mut.vcf.SetResonance(resonance);
        voice_mut.vcf.SetMode(vcf_mode);

        float voice_filtered = voice_hpf_out;
        if (vcf_cutoff_param < 100) {
            voice_filtered = voice_mut.vcf.Process(voice_hpf_out);
        }

        // Apply voice envelope (VCA)
        float voice_output = voice_filtered * voice_env;

        // Apply velocity scaling to VCA amplitude (soft hits quieter, loud hits louder)
        // Map velocity 0-127 to VCA multiplier 0.2-1.0 (soft hits still audible)
        const float voice_vca_gain = 0.2f + (voice.velocity / 127.0f) * 0.8f;  // 0.2 to 1.0
        voice_output *= voice_vca_gain;

        // Add filtered, velocity-scaled voice to mix
        mixed += voice_output;

        // If envelope is fully released, mark voice inactive so it can retrigger
        if (!voice_mut.env_amp.IsActive()) {
            synth.GetAllocator().MarkVoiceInactive(v);
        }
    }

    // Scale by voice count to prevent clipping
    // OPTIMIZATION: Pre-calculate and cache reciprocal to avoid sqrt on every frame
    if (active_voice_count > 0) {
        if (active_voice_count != cached_active_voice_count) {
            // Only recalculate if voice count changed
            cached_inv_voice_count = 1.0f / sqrtf(static_cast<float>(active_voice_count));
            cached_active_voice_count = active_voice_count;
        }
        mixed *= cached_inv_voice_count;  // Multiply instead of divide
    }

    return mixed;
}

} // namespace dsp