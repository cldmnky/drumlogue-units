/**
 * @file polyphonic_renderer.h
 * @brief Polyphonic mode renderer for Drupiter synth
 */

#pragma once

#include "drupiter_synth.h"
#include <cstdint>

namespace dsp {

/**
 * @brief Handles polyphonic mode rendering for DrupiterSynth
 */
class PolyphonicRenderer {
public:
    /**
     * @brief Render polyphonic voices
     * @param synth Reference to the main synth
     * @param frames Number of frames to render
     * @param modulated_pw Current pulse width modulation
     * @param dco1_oct_mult DCO1 octave multiplier
     * @param dco2_oct_mult DCO2 octave multiplier
     * @param detune_ratio Detune ratio
     * @param xmod_depth Cross-modulation depth
     * @param lfo_vco_depth LFO to VCO depth
     * @param lfo_out Current LFO output
     * @param pitch_mod_ratio Pitch modulation ratio
     * @param env_pitch_depth Pitch envelope depth
     * @param dco1_level DCO1 level
     * @param dco2_level DCO2 level
     * @param cutoff_base_nominal Base cutoff frequency
     * @param resonance Filter resonance
     * @param vcf_mode Filter mode
     * @param hpf_alpha HPF coefficient
     * @param key_track Keyboard tracking amount
     * @param smoothed_pressure Channel pressure
     * @param env_vcf_depth Envelope to VCF depth
     * @param lfo_vcf_depth LFO to VCF depth
     * @param fast_pow2 Fast power of 2 function
     * @return Mixed output signal
     */
    static float RenderVoices(
        DrupiterSynth& synth,
        uint32_t frames,
        float modulated_pw,
        float dco1_oct_mult,
        float dco2_oct_mult,
        float detune_ratio,
        float xmod_depth,
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
    );
};

} // namespace dsp