/**
 * @file presets.h
 * @brief Factory preset definitions for Drupiter Synth
 *
 * Preset structure:
 * - 24 parameters (PARAM_COUNT)
 * - 9 hub values (MOD_NUM_DESTINATIONS)
 * - 13-character name (drumlogue limit)
 */

#pragma once

#include "drupiter_synth.h"
#include <cstring>

/**
 * @brief Factory preset initialization
 * 
 * Creates 6 factory presets covering common synthesis categories:
 * - Init: Basic starting point
 * - Bass: Punchy bass with filter envelope
 * - Lead: Sharp lead with sync
 * - Pad: Warm pad with detuned oscillators
 * - Brass: Bright brass with cross-modulation
 * - Strings: Lush strings with detune
 */
inline void InitDrupiterFactoryPresets(DrupiterSynth::Preset presets[6]) {
    // Initialize all presets with default hub values
    for (uint8_t p = 0; p < 6; ++p) {
        for (uint8_t dest = 0; dest < DrupiterSynth::MOD_NUM_DESTINATIONS; ++dest) {
            presets[p].hub_values[dest] = DrupiterSynth::kModDestinations[dest].default_value;
        }
    }
    
    // ========================================================================
    // Preset 0: Init - Basic sound
    // ========================================================================
    std::memset(&presets[0], 0, sizeof(DrupiterSynth::Preset));
    // Reinit hub values after memset
    for (uint8_t dest = 0; dest < DrupiterSynth::MOD_NUM_DESTINATIONS; ++dest) {
        presets[0].hub_values[dest] = DrupiterSynth::kModDestinations[dest].default_value;
    }
    // Page 1: DCO-1
    presets[0].params[DrupiterSynth::PARAM_DCO1_OCT] = 1;       // 8'
    presets[0].params[DrupiterSynth::PARAM_DCO1_WAVE] = 0;      // SAW
    presets[0].params[DrupiterSynth::PARAM_DCO1_PW] = 50;       // 50%
    presets[0].params[DrupiterSynth::PARAM_XMOD] = 0;           // No cross-mod
    // Page 2: DCO-2
    presets[0].params[DrupiterSynth::PARAM_DCO2_OCT] = 1;       // 8'
    presets[0].params[DrupiterSynth::PARAM_DCO2_WAVE] = 0;      // SAW
    presets[0].params[DrupiterSynth::PARAM_DCO2_TUNE] = 50;     // Center (no detune)
    presets[0].params[DrupiterSynth::PARAM_SYNC] = 0;           // OFF
    // Page 3: MIX & VCF
    presets[0].params[DrupiterSynth::PARAM_OSC_MIX] = 0;        // DCO1 only
    presets[0].params[DrupiterSynth::PARAM_VCF_CUTOFF] = 79;
    presets[0].params[DrupiterSynth::PARAM_VCF_RESONANCE] = 16;
    presets[0].params[DrupiterSynth::PARAM_VCF_KEYFLW] = 50;    // 50% keyboard tracking
    // Page 4: VCF Envelope
    presets[0].params[DrupiterSynth::PARAM_VCF_ATTACK] = 4;
    presets[0].params[DrupiterSynth::PARAM_VCF_DECAY] = 31;
    presets[0].params[DrupiterSynth::PARAM_VCF_SUSTAIN] = 50;
    presets[0].params[DrupiterSynth::PARAM_VCF_RELEASE] = 24;
    // Page 5: VCA Envelope
    presets[0].params[DrupiterSynth::PARAM_VCA_ATTACK] = 1;
    presets[0].params[DrupiterSynth::PARAM_VCA_DECAY] = 39;
    presets[0].params[DrupiterSynth::PARAM_VCA_SUSTAIN] = 79;
    presets[0].params[DrupiterSynth::PARAM_VCA_RELEASE] = 16;
    // Page 6: LFO, MOD HUB & Effects
    presets[0].params[DrupiterSynth::PARAM_LFO_RATE] = 32;      // Moderate LFO rate
    presets[0].params[DrupiterSynth::PARAM_MOD_HUB] = DrupiterSynth::MOD_VCF_TYPE;
    presets[0].hub_values[DrupiterSynth::MOD_VCF_TYPE] = 1;     // LP24 filter
    presets[0].params[DrupiterSynth::PARAM_EFFECT] = 0;         // Chorus
    std::strcpy(presets[0].name, "Init 1");
    
    // ========================================================================
    // Preset 1: Bass - Punchy bass with filter envelope
    // ========================================================================
    std::memset(&presets[1], 0, sizeof(DrupiterSynth::Preset));
    for (uint8_t dest = 0; dest < DrupiterSynth::MOD_NUM_DESTINATIONS; ++dest) {
        presets[1].hub_values[dest] = DrupiterSynth::kModDestinations[dest].default_value;
    }
    presets[1].params[DrupiterSynth::PARAM_DCO1_OCT] = 0;       // 16'
    presets[1].params[DrupiterSynth::PARAM_DCO1_WAVE] = 2;      // PULSE
    presets[1].params[DrupiterSynth::PARAM_DCO1_PW] = 31;       // Narrow pulse
    presets[1].params[DrupiterSynth::PARAM_XMOD] = 0;
    presets[1].params[DrupiterSynth::PARAM_DCO2_OCT] = 0;       // 16'
    presets[1].params[DrupiterSynth::PARAM_DCO2_WAVE] = 0;      // SAW
    presets[1].params[DrupiterSynth::PARAM_DCO2_TUNE] = 50;
    presets[1].params[DrupiterSynth::PARAM_SYNC] = 0;
    presets[1].params[DrupiterSynth::PARAM_OSC_MIX] = 0;        // DCO1 only
    presets[1].params[DrupiterSynth::PARAM_VCF_CUTOFF] = 39;
    presets[1].params[DrupiterSynth::PARAM_VCF_RESONANCE] = 39;
    presets[1].params[DrupiterSynth::PARAM_VCF_KEYFLW] = 75;    // High key tracking for bass
    presets[1].params[DrupiterSynth::PARAM_VCF_ATTACK] = 0;
    presets[1].params[DrupiterSynth::PARAM_VCF_DECAY] = 27;
    presets[1].params[DrupiterSynth::PARAM_VCF_SUSTAIN] = 16;
    presets[1].params[DrupiterSynth::PARAM_VCF_RELEASE] = 8;
    presets[1].params[DrupiterSynth::PARAM_VCA_ATTACK] = 0;
    presets[1].params[DrupiterSynth::PARAM_VCA_DECAY] = 31;
    presets[1].params[DrupiterSynth::PARAM_VCA_SUSTAIN] = 63;
    presets[1].params[DrupiterSynth::PARAM_VCA_RELEASE] = 12;
    presets[1].params[DrupiterSynth::PARAM_LFO_RATE] = 32;
    presets[1].params[DrupiterSynth::PARAM_MOD_HUB] = DrupiterSynth::MOD_VCF_TYPE;
    presets[1].hub_values[DrupiterSynth::MOD_VCF_TYPE] = 1;     // LP24
    presets[1].params[DrupiterSynth::PARAM_EFFECT] = 0;
    std::strcpy(presets[1].name, "Bass 1");
    
    // ========================================================================
    // Preset 2: Lead - Sharp lead with sync
    // ========================================================================
    std::memset(&presets[2], 0, sizeof(DrupiterSynth::Preset));
    for (uint8_t dest = 0; dest < DrupiterSynth::MOD_NUM_DESTINATIONS; ++dest) {
        presets[2].hub_values[dest] = DrupiterSynth::kModDestinations[dest].default_value;
    }
    presets[2].params[DrupiterSynth::PARAM_DCO1_OCT] = 1;       // 8'
    presets[2].params[DrupiterSynth::PARAM_DCO1_WAVE] = 0;      // SAW
    presets[2].params[DrupiterSynth::PARAM_DCO1_PW] = 50;
    presets[2].params[DrupiterSynth::PARAM_XMOD] = 0;
    presets[2].params[DrupiterSynth::PARAM_DCO2_OCT] = 2;       // 4' (one octave up)
    presets[2].params[DrupiterSynth::PARAM_DCO2_WAVE] = 0;      // SAW
    presets[2].params[DrupiterSynth::PARAM_DCO2_TUNE] = 50;
    presets[2].params[DrupiterSynth::PARAM_SYNC] = 2;           // HARD sync for classic lead
    presets[2].params[DrupiterSynth::PARAM_OSC_MIX] = 30;       // Mostly DCO1 with some DCO2
    presets[2].params[DrupiterSynth::PARAM_VCF_CUTOFF] = 71;
    presets[2].params[DrupiterSynth::PARAM_VCF_RESONANCE] = 55;
    presets[2].params[DrupiterSynth::PARAM_VCF_KEYFLW] = 40;
    presets[2].params[DrupiterSynth::PARAM_VCF_ATTACK] = 4;
    presets[2].params[DrupiterSynth::PARAM_VCF_DECAY] = 24;
    presets[2].params[DrupiterSynth::PARAM_VCF_SUSTAIN] = 47;
    presets[2].params[DrupiterSynth::PARAM_VCF_RELEASE] = 20;
    presets[2].params[DrupiterSynth::PARAM_VCA_ATTACK] = 2;
    presets[2].params[DrupiterSynth::PARAM_VCA_DECAY] = 24;
    presets[2].params[DrupiterSynth::PARAM_VCA_SUSTAIN] = 79;
    presets[2].params[DrupiterSynth::PARAM_VCA_RELEASE] = 16;
    presets[2].params[DrupiterSynth::PARAM_LFO_RATE] = 50;
    presets[2].params[DrupiterSynth::PARAM_MOD_HUB] = DrupiterSynth::MOD_LFO_TO_VCF;
    presets[2].hub_values[DrupiterSynth::MOD_LFO_TO_VCF] = 30;
    presets[2].params[DrupiterSynth::PARAM_EFFECT] = 0;
    std::strcpy(presets[2].name, "Lead 1");
    
    // ========================================================================
    // Preset 3: Pad - Warm pad with detuned oscillators
    // ========================================================================
    std::memset(&presets[3], 0, sizeof(DrupiterSynth::Preset));
    for (uint8_t dest = 0; dest < DrupiterSynth::MOD_NUM_DESTINATIONS; ++dest) {
        presets[3].hub_values[dest] = DrupiterSynth::kModDestinations[dest].default_value;
    }
    presets[3].params[DrupiterSynth::PARAM_DCO1_OCT] = 1;       // 8'
    presets[3].params[DrupiterSynth::PARAM_DCO1_WAVE] = 0;      // SAW
    presets[3].params[DrupiterSynth::PARAM_DCO1_PW] = 50;
    presets[3].params[DrupiterSynth::PARAM_XMOD] = 0;
    presets[3].params[DrupiterSynth::PARAM_DCO2_OCT] = 1;       // 8'
    presets[3].params[DrupiterSynth::PARAM_DCO2_WAVE] = 0;      // SAW
    presets[3].params[DrupiterSynth::PARAM_DCO2_TUNE] = 53;     // Slight detune
    presets[3].params[DrupiterSynth::PARAM_SYNC] = 0;
    presets[3].params[DrupiterSynth::PARAM_OSC_MIX] = 50;       // Equal mix
    presets[3].params[DrupiterSynth::PARAM_VCF_CUTOFF] = 63;
    presets[3].params[DrupiterSynth::PARAM_VCF_RESONANCE] = 20;
    presets[3].params[DrupiterSynth::PARAM_VCF_KEYFLW] = 20;
    presets[3].params[DrupiterSynth::PARAM_VCF_ATTACK] = 35;
    presets[3].params[DrupiterSynth::PARAM_VCF_DECAY] = 39;
    presets[3].params[DrupiterSynth::PARAM_VCF_SUSTAIN] = 55;
    presets[3].params[DrupiterSynth::PARAM_VCF_RELEASE] = 39;
    presets[3].params[DrupiterSynth::PARAM_VCA_ATTACK] = 39;
    presets[3].params[DrupiterSynth::PARAM_VCA_DECAY] = 39;
    presets[3].params[DrupiterSynth::PARAM_VCA_SUSTAIN] = 79;
    presets[3].params[DrupiterSynth::PARAM_VCA_RELEASE] = 55;
    presets[3].params[DrupiterSynth::PARAM_LFO_RATE] = 35;
    presets[3].params[DrupiterSynth::PARAM_MOD_HUB] = DrupiterSynth::MOD_VCF_TYPE;
    presets[3].hub_values[DrupiterSynth::MOD_VCF_TYPE] = 1;     // LP24
    presets[3].params[DrupiterSynth::PARAM_EFFECT] = 0;
    std::strcpy(presets[3].name, "Pad 1");
    
    // ========================================================================
    // Preset 4: Brass - Bright brass with cross-modulation
    // ========================================================================
    std::memset(&presets[4], 0, sizeof(DrupiterSynth::Preset));
    for (uint8_t dest = 0; dest < DrupiterSynth::MOD_NUM_DESTINATIONS; ++dest) {
        presets[4].hub_values[dest] = DrupiterSynth::kModDestinations[dest].default_value;
    }
    presets[4].params[DrupiterSynth::PARAM_DCO1_OCT] = 1;       // 8'
    presets[4].params[DrupiterSynth::PARAM_DCO1_WAVE] = 0;      // SAW
    presets[4].params[DrupiterSynth::PARAM_DCO1_PW] = 50;
    presets[4].params[DrupiterSynth::PARAM_XMOD] = 15;          // Subtle cross-mod
    presets[4].params[DrupiterSynth::PARAM_DCO2_OCT] = 1;       // 8'
    presets[4].params[DrupiterSynth::PARAM_DCO2_WAVE] = 0;      // SAW
    presets[4].params[DrupiterSynth::PARAM_DCO2_TUNE] = 50;
    presets[4].params[DrupiterSynth::PARAM_SYNC] = 0;
    presets[4].params[DrupiterSynth::PARAM_OSC_MIX] = 40;
    presets[4].params[DrupiterSynth::PARAM_VCF_CUTOFF] = 59;
    presets[4].params[DrupiterSynth::PARAM_VCF_RESONANCE] = 24;
    presets[4].params[DrupiterSynth::PARAM_VCF_KEYFLW] = 60;
    presets[4].params[DrupiterSynth::PARAM_VCF_ATTACK] = 12;
    presets[4].params[DrupiterSynth::PARAM_VCF_DECAY] = 35;
    presets[4].params[DrupiterSynth::PARAM_VCF_SUSTAIN] = 51;
    presets[4].params[DrupiterSynth::PARAM_VCF_RELEASE] = 27;
    presets[4].params[DrupiterSynth::PARAM_VCA_ATTACK] = 12;
    presets[4].params[DrupiterSynth::PARAM_VCA_DECAY] = 35;
    presets[4].params[DrupiterSynth::PARAM_VCA_SUSTAIN] = 71;
    presets[4].params[DrupiterSynth::PARAM_VCA_RELEASE] = 24;
    presets[4].params[DrupiterSynth::PARAM_LFO_RATE] = 40;
    presets[4].params[DrupiterSynth::PARAM_MOD_HUB] = DrupiterSynth::MOD_ENV_TO_VCF;
    presets[4].hub_values[DrupiterSynth::MOD_ENV_TO_VCF] = 40;
    presets[4].params[DrupiterSynth::PARAM_EFFECT] = 0;
    std::strcpy(presets[4].name, "Brass 1");
    
    // ========================================================================
    // Preset 5: Strings - Lush strings with detune
    // ========================================================================
    std::memset(&presets[5], 0, sizeof(DrupiterSynth::Preset));
    for (uint8_t dest = 0; dest < DrupiterSynth::MOD_NUM_DESTINATIONS; ++dest) {
        presets[5].hub_values[dest] = DrupiterSynth::kModDestinations[dest].default_value;
    }
    presets[5].params[DrupiterSynth::PARAM_DCO1_OCT] = 1;       // 8'
    presets[5].params[DrupiterSynth::PARAM_DCO1_WAVE] = 0;      // SAW
    presets[5].params[DrupiterSynth::PARAM_DCO1_PW] = 50;
    presets[5].params[DrupiterSynth::PARAM_XMOD] = 0;
    presets[5].params[DrupiterSynth::PARAM_DCO2_OCT] = 1;       // 8'
    presets[5].params[DrupiterSynth::PARAM_DCO2_WAVE] = 0;      // SAW
    presets[5].params[DrupiterSynth::PARAM_DCO2_TUNE] = 55;     // Detune for richness
    presets[5].params[DrupiterSynth::PARAM_SYNC] = 0;
    presets[5].params[DrupiterSynth::PARAM_OSC_MIX] = 50;       // Equal mix
    presets[5].params[DrupiterSynth::PARAM_VCF_CUTOFF] = 75;
    presets[5].params[DrupiterSynth::PARAM_VCF_RESONANCE] = 16;
    presets[5].params[DrupiterSynth::PARAM_VCF_KEYFLW] = 25;
    presets[5].params[DrupiterSynth::PARAM_VCF_ATTACK] = 47;
    presets[5].params[DrupiterSynth::PARAM_VCF_DECAY] = 43;
    presets[5].params[DrupiterSynth::PARAM_VCF_SUSTAIN] = 59;
    presets[5].params[DrupiterSynth::PARAM_VCF_RELEASE] = 47;
    presets[5].params[DrupiterSynth::PARAM_VCA_ATTACK] = 51;
    presets[5].params[DrupiterSynth::PARAM_VCA_DECAY] = 43;
    presets[5].params[DrupiterSynth::PARAM_VCA_SUSTAIN] = 79;
    presets[5].params[DrupiterSynth::PARAM_VCA_RELEASE] = 63;
    presets[5].params[DrupiterSynth::PARAM_LFO_RATE] = 38;
    presets[5].params[DrupiterSynth::PARAM_MOD_HUB] = DrupiterSynth::MOD_LFO_TO_VCO;
    presets[5].hub_values[DrupiterSynth::MOD_LFO_TO_VCO] = 20;
    presets[5].params[DrupiterSynth::PARAM_EFFECT] = 0;
    std::strcpy(presets[5].name, "String 1");
}

