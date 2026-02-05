/**
 * @file unison_renderer.h
 * @brief Unison mode renderer for Drupiter synth
 */

#pragma once

#include "drupiter_synth.h"
#include <cstdint>

namespace dsp {

/**
 * @brief Handles unison mode rendering for DrupiterSynth
 */
class UnisonRenderer {
public:
    /**
     * @brief Render unison mode
     * @param synth Reference to the main synth
     * @param modulated_pw Current pulse width modulation
     * @param dco1_oct_mult DCO1 octave multiplier
     * @param dco2_oct_mult DCO2 octave multiplier
     * @param detune_ratio Detune ratio
     * @param lfo_vco_depth LFO to VCO depth
     * @param lfo_out Current LFO output
     * @param pitch_mod_ratio Pitch modulation ratio
     * @param smoothed_pitch_bend Pitch bend modulation
     * @param dco1_level DCO1 level
     * @param dco2_level DCO2 level
     * @param dco1_wave_param DCO1 waveform parameter value
     * @param semitones_to_ratio Function to convert semitones to ratio
     * @return Mixed output signal
     */
    static float RenderUnison(
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
    );
};

} // namespace dsp