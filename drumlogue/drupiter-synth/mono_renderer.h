/**
 * @file mono_renderer.h
 * @brief Mono mode renderer for Drupiter synth
 */

#pragma once

#include "drupiter_synth.h"
#include <cstdint>

namespace dsp {

/**
 * @brief Handles mono mode rendering for DrupiterSynth
 */
class MonoRenderer {
public:
    /**
     * @brief Render mono mode
     * @param synth Reference to the main synth
     * @param modulated_pw Current pulse width modulation
     * @param dco1_oct_mult DCO1 octave multiplier
     * @param dco2_oct_mult DCO2 octave multiplier
     * @param detune_ratio Detune ratio
     * @param xmod_depth Cross-modulation depth
     * @param lfo_vco_depth LFO to VCO depth
     * @param lfo_out Current LFO output
     * @param pitch_mod_ratio Pitch modulation ratio
     * @param smoothed_pitch_bend Pitch bend modulation
     * @param dco1_level DCO1 level
     * @param dco2_level DCO2 level
     * @param semitones_to_ratio Function to convert semitones to ratio
     * @return Mixed output signal
     */
    static float RenderMono(
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
    );
};

} // namespace dsp