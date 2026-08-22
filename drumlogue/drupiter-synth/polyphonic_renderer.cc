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
#include <cstring>

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

// Cached propagation state for performance optimizations.
//
// The cache is keyed by owning DrupiterSynth instance: multiple instances
// legitimately exist in tests (and sequentially on preset reloads), and
// previously these file-scope values leaked between them, skipping required
// SetWaveform/SetResonance propagation on freshly constructed synths.
struct PolyCache {
    const void* owner = nullptr;
    uint8_t dco1_wave = 255;   // Cache to avoid redundant SetWaveform calls
    uint8_t dco2_wave = 255;
    float inv_voice_count = 1.0f;   // Pre-calculated reciprocal of voice count
    uint8_t active_voice_count = 0;
    float resonance = -1.0f;        // Cache to avoid redundant SetResonance calls
    int vcf_mode = -1;              // Cache to avoid redundant SetMode calls
};
static PolyCache s_poly_cache;

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
    uint8_t vcf_cutoff_param
) {
    // POLYPHONIC MODE: Render and mix multiple independent voices
    float mixed = 0.0f;
    uint8_t active_voice_count = 0;
    
    // OPTIMIZATION: Check HPF condition once before voice loop instead of per-voice
    const bool apply_hpf = hpf_alpha > 0.0f;

    // Reset caches when a different synth instance owns this renderer state
    if (s_poly_cache.owner != static_cast<const void*>(&synth)) {
        s_poly_cache = PolyCache();
        s_poly_cache.owner = &synth;
    }

    // OPTIMIZATION: Propagate waveforms to ALL voices in one pre-loop pass
    // when the parameter changes. Previously done inside the voice loop with
    // the cache write there, so only the first active voice received the new
    // waveform until a second parameter change came along.
    if (dco1_wave_param != s_poly_cache.dco1_wave) {
        const JupiterDCO::Waveform wf = map_dco1_waveform(dco1_wave_param);
        for (uint8_t vo = 0; vo < DRUPITER_MAX_VOICES; vo++) {
            synth.GetAllocator().GetVoiceMutable(vo).dco1.SetWaveform(wf);
        }
        s_poly_cache.dco1_wave = dco1_wave_param;
    }
    if (dco2_wave_param != s_poly_cache.dco2_wave) {
        const JupiterDCO::Waveform wf = map_dco2_waveform(dco2_wave_param);
        for (uint8_t vo = 0; vo < DRUPITER_MAX_VOICES; vo++) {
            synth.GetAllocator().GetVoiceMutable(vo).dco2.SetWaveform(wf);
        }
        s_poly_cache.dco2_wave = dco2_wave_param;
    }

    // OPTIMIZATION: resonance and mode only change when parameters change.
    // Update ALL voices in a single pre-loop pass when they do, instead of
    // calling the setters per voice per sample. (The cache must not be
    // updated inside the voice loop, or only the first active voice would
    // receive the new setting.)
    if (resonance != s_poly_cache.resonance || static_cast<int>(vcf_mode) != s_poly_cache.vcf_mode) {
        s_poly_cache.resonance = resonance;
        s_poly_cache.vcf_mode = static_cast<int>(vcf_mode);
        for (uint8_t v = 0; v < DRUPITER_MAX_VOICES; v++) {
            dsp::Voice& voice_mut = synth.GetAllocator().GetVoiceMutable(v);
            voice_mut.vcf.SetResonance(resonance);
            voice_mut.vcf.SetMode(vcf_mode);
        }
    }

    // Per-voice note/velocity-derived constants. These only change when a
    // voice is retriggered (or key tracking moves), so compute them lazily
    // once instead of every sample.
    uint8_t kt_note[DRUPITER_MAX_VOICES];
    uint32_t kt_bits[DRUPITER_MAX_VOICES];
    float kt_ratio[DRUPITER_MAX_VOICES];
    uint32_t vel_bits[DRUPITER_MAX_VOICES];
    float vel_mod2[DRUPITER_MAX_VOICES];  // velocity * 0.5 * 2 (VCF octaves)
    float vel_vca[DRUPITER_MAX_VOICES];   // 0.2..1.0 VCA multiplier
    uint32_t key_track_bits;
    std::memcpy(&key_track_bits, &key_track, sizeof(key_track_bits));
    for (uint8_t vo = 0; vo < DRUPITER_MAX_VOICES; ++vo) {
        kt_note[vo] = 0xFF;
        kt_bits[vo] = 0xFFFFFFFFu;
        kt_ratio[vo] = 1.0f;
        vel_bits[vo] = 0xFFFFFFFFu;
        vel_mod2[vo] = 0.0f;
        vel_vca[vo] = 1.0f;
    }

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
            const float glide_ratio = 1.0f + voice_mut.glide_increment;
            voice_mut.pitch_hz *= glide_ratio;

            // Check if we've reached target (compare in frequency domain directly)
            if ((voice_mut.glide_increment > 0.0f && voice_mut.pitch_hz >= voice_mut.glide_target_hz) ||
                (voice_mut.glide_increment < 0.0f && voice_mut.pitch_hz <= voice_mut.glide_target_hz)) {
                voice_mut.pitch_hz = voice_mut.glide_target_hz;
                voice_mut.is_gliding = false;
            }
        }

        // NOTE: waveforms are propagated in the pre-loop pass above; here we
        // only update the continuously-modulated pulse width.
        voice_mut.dco1.SetPulseWidth(modulated_pw);
        voice_mut.dco2.SetPulseWidth(modulated_pw);

        // Calculate frequencies for this voice
        float voice_freq1 = voice.pitch_hz * dco1_oct_mult;
        float voice_freq2 = voice.pitch_hz * dco2_oct_mult * detune_ratio;

        // Apply LFO vibrato
        // (The previous NEON variant used vld1_f32 across two separate local
        // variables, which is undefined behaviour and slower than two scalar
        // multiplies on Cortex-A7.)
        if (lfo_vco_depth > kMinModulation) {
            const float lfo_mod = 1.0f + lfo_out * lfo_vco_depth * 0.05f;
            voice_freq1 *= lfo_mod;
            voice_freq2 *= lfo_mod;
        }

        // Apply pitch envelope modulation (Task 2.2.1: Per-voice pitch envelope)
        if (pitch_mod_ratio != 1.0f) {
            // Use per-voice pitch envelope for independent pitch modulation
            const float voice_env_pitch = voice_mut.env_pitch.Process();
            // Use faster pow2 approximation instead of expensive powf
            const float voice_pitch_ratio = fasterpow2f(voice_env_pitch * env_pitch_depth / 12.0f);

            voice_freq1 *= voice_pitch_ratio;
            voice_freq2 *= voice_pitch_ratio;
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

        // Apply per-voice keyboard tracking (hoisted: recomputed only when the
        // voice's note or the key-tracking amount changes)
        if (kt_note[v] != voice.midi_note || kt_bits[v] != key_track_bits) {
            const float voice_note_offset =
                (static_cast<int32_t>(voice.midi_note) - 60) / 12.0f;
            const float voice_tracking_exponent =
                clampf(voice_note_offset * key_track, -4.0f, 4.0f);
            kt_ratio[v] = semitones_to_ratio(voice_tracking_exponent * 12.0f);
            kt_note[v] = voice.midi_note;
            kt_bits[v] = key_track_bits;
        }
        float voice_cutoff_base = cutoff_base_nominal * kt_ratio[v];

        // Per-voice velocity modulation (hoisted, keyed on the velocity bits).
        // Stored pre-multiplied by 2 so the combine below keeps its original
        // `voice_vel_mod * 2.0f` semantics without a per-sample multiply.
        uint32_t vbits;
        std::memcpy(&vbits, &voice.velocity, sizeof(vbits));
        if (vel_bits[v] != vbits) {
            vel_mod2[v] = voice.velocity;  // velocity * 0.5f * 2.0f combined
            vel_vca[v] = 0.2f + voice.velocity * 0.8f;
            vel_bits[v] = vbits;
        }
        const float voice_vel_mod = vel_mod2[v];

        // Combine envelope, LFO, velocity, and pressure modulation (shared sources)
        float voice_total_mod = voice_vcf_env * 2.0f              // Base envelope modulation
                              + env_vcf_depth * voice_vcf_env     // Hub envelope.VCF modulation
                              + lfo_out * lfo_vcf_depth * 1.0f    // LFO modulation
                              + voice_vel_mod                     // Velocity adds up to +2 octaves (hoisted, pre-scaled)
                              + smoothed_pressure * 1.0f;         // Channel pressure adds up to +1 octave

        // Clamp modulation depth to avoid extreme cutoff values (branchless, from common/dsp_utils.h)
        voice_total_mod = clampf(voice_total_mod, -3.0f, 3.0f);

        // Apply modulation to cutoff base
        const float voice_cutoff_modulated = voice_cutoff_base * fast_pow2(voice_total_mod);

        // Set per-voice filter cutoff and process
        voice_mut.vcf.SetCutoffModulated(voice_cutoff_modulated);

        float voice_filtered = voice_hpf_out;
        if (vcf_cutoff_param < 100) {
            voice_filtered = voice_mut.vcf.Process(voice_hpf_out);
        }

        // Apply voice envelope (VCA)
        float voice_output = voice_filtered * voice_env;

        // Apply velocity scaling to VCA amplitude (soft hits quieter, loud hits louder)
        // BUGFIX: voice.velocity is already normalized 0.0-1.0 (VelocityToFloat);
        // dividing by 127 again clamped the gain to ~0.2 with no audible range.
        // Map normalized velocity to VCA multiplier 0.2-1.0 (soft hits still audible)
        voice_output *= vel_vca[v];

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
        if (active_voice_count != s_poly_cache.active_voice_count) {
            // Only recalculate if voice count changed
            s_poly_cache.inv_voice_count =
                1.0f / sqrtf(static_cast<float>(active_voice_count));
            s_poly_cache.active_voice_count = active_voice_count;
        }
        mixed *= s_poly_cache.inv_voice_count;  // Multiply instead of divide
    }

    return mixed;
}

} // namespace dsp