/**
 * @file mono_renderer.cc
 * @brief Mono mode renderer implementation for Drupiter synth
 */

#include "mono_renderer.h"
#include "dsp/jupiter_dco.h"
#include "../common/dsp_utils.h"

// Threshold constants for modulation and distance checks
static constexpr float kMinModulation = 0.001f;  // Minimum significant modulation depth

namespace dsp {

float MonoRenderer::RenderMono(
    DrupiterSynth& synth,
    float modulated_pw,
    float dco1_oct_mult,
    float dco2_oct_mult,
    float detune_ratio,
    float xmod_depth,
    float lfo_vco_depth,
    float lfo_out,
    float pitch_mod_ratio,
    float smoothed_pitch_bend,
    float dco1_level,
    float dco2_level,
    float (*semitones_to_ratio)(float)
) {
    // MONO MODE: Use main synth DCOs (monophonic, single voice)
    synth.GetDCO1().SetPulseWidth(modulated_pw);
    synth.GetDCO2().SetPulseWidth(modulated_pw);  // Both DCOs share PWM

    // Calculate DCO frequencies with LFO modulation
    float freq1 = synth.GetCurrentFreqHz() * dco1_oct_mult;
    float freq2 = synth.GetCurrentFreqHz() * dco2_oct_mult * detune_ratio;

    // Apply LFO modulation to frequencies (vibrato)
    if (lfo_vco_depth > kMinModulation) {
        const float lfo_mod = 1.0f + lfo_out * lfo_vco_depth * 0.05f;  // ±5% vibrato
        freq1 *= lfo_mod;
        freq2 *= lfo_mod;
    }

    // Apply pitch bend modulation (smooth per-buffer from MIDI wheel)
    if (smoothed_pitch_bend != 0.0f) {
        const float pitch_bend_ratio = semitones_to_ratio(smoothed_pitch_bend);
        freq1 *= pitch_bend_ratio;
        freq2 *= pitch_bend_ratio;
    }

    // Apply pitch envelope modulation (pre-calculated)
    freq1 *= pitch_mod_ratio;
    freq2 *= pitch_mod_ratio;

    synth.GetDCO1().SetFrequency(freq1);
    synth.GetDCO2().SetFrequency(freq2);

    // Only process DCO2 if it's audible (level > 0) or needed for XMOD
    const bool dco2_needed = (dco2_level > kMinModulation) || (xmod_depth > kMinModulation);

    float dco2_out = 0.0f;
    if (dco2_needed) {
        // Process DCO2 first to get fresh output for FM
        dco2_out = synth.GetDCO2().Process();

        // Cross-modulation (DCO2 . DCO1 FM) - Jupiter-8 style
        // Only apply FM if XMOD depth is significant
        if (xmod_depth > kMinModulation) {
            // Scale: 100% XMOD = ±1 semitone = ±1/12 octave
            synth.GetDCO1().ApplyFM(dco2_out * xmod_depth * 0.083f);
        } else {
            synth.GetDCO1().ApplyFM(0.0f);
        }
    } else {
        // DCO2 not needed - silence it and ensure no FM
        synth.GetDCO1().ApplyFM(0.0f);
    }

    // Process DCO1 (optionally modulated by DCO2)
    float dco1_out = synth.GetDCO1().Process();

    // Note: Sync disabled when XMOD is active
    // Jupiter-8 doesn't support sync+xmod simultaneously due to processing order
    // (DCO2 must be processed first for FM, breaking sync master/slave relationship)

    // Mix oscillators with smoothed levels
    return dco1_out * dco1_level + dco2_out * dco2_level;
}

} // namespace dsp