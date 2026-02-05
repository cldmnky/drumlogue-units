/**
 * @file unison_renderer.cc
 * @brief Unison mode renderer implementation for Drupiter synth
 */

#include "unison_renderer.h"
#include "dsp/jupiter_dco.h"
#include "../common/dsp_utils.h"

// Threshold constants for modulation and distance checks
static constexpr float kMinModulation = 0.001f;  // Minimum significant modulation depth

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

namespace dsp {

float UnisonRenderer::RenderUnison(
    DrupiterSynth& synth,
    float modulated_pw,
    float dco1_oct_mult,
    float dco2_oct_mult,
    float detune_ratio,
    float lfo_vco_depth,
    float lfo_out,
    float pitch_mod_ratio,
    float smoothed_pitch_bend,
    float dco1_level,
    float dco2_level,
    uint8_t dco1_wave_param,
    float (*semitones_to_ratio)(float)
) {
    // UNISON MODE: Use UnisonOscillator for multi-voice detuned stack + DCO2
    dsp::UnisonOscillator& unison_osc = synth.GetAllocator().GetUnisonOscillator();

    // Set waveform and pulse width (same as DCO1 with proper mapping)
    unison_osc.SetWaveform(map_dco1_waveform(dco1_wave_param));
    unison_osc.SetPulseWidth(modulated_pw);

    // Calculate frequency with LFO modulation
    float unison_freq = synth.GetCurrentFreqHz() * dco1_oct_mult;
    if (lfo_vco_depth > kMinModulation) {
        const float lfo_mod = 1.0f + lfo_out * lfo_vco_depth * 0.05f;
        unison_freq *= lfo_mod;
    }

    // Apply pitch bend modulation (smooth per-buffer from MIDI wheel)
    if (smoothed_pitch_bend != 0.0f) {
        const float pitch_bend_ratio = semitones_to_ratio(smoothed_pitch_bend);
        unison_freq *= pitch_bend_ratio;
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
    synth.GetDCO2().SetPulseWidth(modulated_pw);
    float freq2 = synth.GetCurrentFreqHz() * dco2_oct_mult * detune_ratio;
    if (lfo_vco_depth > kMinModulation) {
        const float lfo_mod = 1.0f + lfo_out * lfo_vco_depth * 0.05f;
        freq2 *= lfo_mod;
    }

    // Apply pitch envelope modulation to DCO2 (pre-calculated)
    freq2 *= pitch_mod_ratio;

    synth.GetDCO2().SetFrequency(freq2);
    float dco2_out = synth.GetDCO2().Process();

    // Mix unison stack with DCO2
    return unison_mono * dco1_level + dco2_out * dco2_level;
}

} // namespace dsp