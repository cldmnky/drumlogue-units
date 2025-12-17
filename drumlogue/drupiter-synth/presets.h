/**
 * @file presets.h
 * @brief Factory presets for Drupiter Jupiter-8 synth
 *
 * Defines 6 factory presets with classic Jupiter-8 inspired sounds:
 * - Init: Basic starting point
 * - Bass: Punchy bass
 * - Lead: Sharp sync lead
 * - Pad: Warm detuned pad
 * - Brass: Bright brass with cross-modulation
 * - Strings: Lush detuned strings
 */

#pragma once

#include "../common/preset_manager.h"
#include "drupiter_synth.h"
#include <cstring>

namespace presets {

// Preset structure compatible with PresetManager (24 params + hub values)
struct DrupiterPreset {
    char name[14];
    uint8_t params[DrupiterSynth::PARAM_COUNT];
    uint8_t hub_values[MOD_NUM_DESTINATIONS];
};

// Factory presets array
static const DrupiterPreset kFactoryPresets[6] = {
    // Preset 0: Init - Basic sound
    {
        "Init 1",
        {
            // Page 1: DCO-1
            1,    // PARAM_DCO1_OCT: 8'
            0,    // PARAM_DCO1_WAVE: SAW
            50,   // PARAM_DCO1_PW: 50%
            0,    // PARAM_XMOD: No cross-mod
            // Page 2: DCO-2
            1,    // PARAM_DCO2_OCT: 8'
            0,    // PARAM_DCO2_WAVE: SAW
            50,   // PARAM_DCO2_TUNE: Center (no detune)
            0,    // PARAM_SYNC: OFF
            // Page 3: MIX & VCF
            50,   // PARAM_OSC_MIX: Equal mix
            79,   // PARAM_VCF_CUTOFF
            16,   // PARAM_VCF_RESONANCE
            50,   // PARAM_VCF_KEYFLW: 50% keyboard tracking
            // Page 4: VCF Envelope
            4,    // PARAM_VCF_ATTACK
            31,   // PARAM_VCF_DECAY
            50,   // PARAM_VCF_SUSTAIN
            24,   // PARAM_VCF_RELEASE
            // Page 5: VCA Envelope
            1,    // PARAM_VCA_ATTACK
            39,   // PARAM_VCA_DECAY
            79,   // PARAM_VCA_SUSTAIN
            16,   // PARAM_VCA_RELEASE
            // Page 6: LFO, MOD HUB & Effects
            32,   // PARAM_LFO_RATE: Moderate
            static_cast<uint8_t>(MOD_VCF_TYPE),  // PARAM_MOD_HUB: VCF Type
            0,    // PARAM_MOD_AMT: Will use hub_values
            0     // PARAM_EFFECT: Chorus
        },
        {0, 0, 0, 0, 0, 0, 1, 0, 0, 0}  // hub_values: MOD_VCF_TYPE=1 (LP24)
    },
    
    // Preset 1: Bass - Punchy bass with filter envelope
    {
        "Bass 1",
        {
            0, 2, 31, 0,        // DCO1: 16', PULSE, narrow PW, no XMOD
            0, 0, 50, 0,        // DCO2: 16', SAW, center tune, no SYNC
            50, 39, 39, 75,     // MIX equal, cutoff low, reso high, key track high
            0, 27, 16, 8,       // VCF env: fast attack, short decay
            0, 31, 63, 12,      // VCA env: punchy
            32, static_cast<uint8_t>(MOD_VCF_TYPE), 0, 0
        },
        {0, 0, 0, 0, 0, 0, 1, 0, 0, 0}  // MOD_VCF_TYPE=1 (LP24)
    },
    
    // Preset 2: Lead - Sharp lead with sync
    {
        "Lead 1",
        {
            1, 0, 50, 0,        // DCO1: 8', SAW
            2, 0, 50, 2,        // DCO2: 4' (octave up), SAW, HARD sync
            30, 71, 55, 40,     // Mostly DCO1, bright cutoff, high reso
            4, 24, 47, 20,      // VCF env: moderate
            2, 24, 79, 16,      // VCA env: slight attack
            50, static_cast<uint8_t>(MOD_LFO_TO_VCF), 0, 0
        },
        {0, 30, 0, 0, 0, 0, 1, 0, 0, 0}  // MOD_LFO_TO_VCF=30
    },
    
    // Preset 3: Pad - Warm pad with detuned oscillators
    {
        "Pad 1",
        {
            1, 0, 50, 0,        // DCO1: 8', SAW
            1, 0, 53, 0,        // DCO2: 8', SAW, slight detune
            50, 63, 20, 20,     // Equal mix, medium cutoff, low reso
            35, 39, 55, 39,     // VCF env: slow attack
            39, 39, 79, 55,     // VCA env: slow attack, long release
            35, static_cast<uint8_t>(MOD_VCF_TYPE), 0, 0
        },
        {0, 0, 0, 0, 0, 0, 1, 0, 0, 0}  // MOD_VCF_TYPE=1 (LP24)
    },
    
    // Preset 4: Brass - Bright brass with XMOD
    {
        "Brass 1",
        {
            1, 0, 50, 15,       // DCO1: 8', SAW, subtle XMOD
            1, 0, 50, 0,        // DCO2: 8', SAW
            40, 59, 24, 60,     // DCO1 dominant, bright, moderate reso
            12, 35, 51, 27,     // VCF env: medium attack
            12, 35, 71, 24,     // VCA env: medium attack
            40, static_cast<uint8_t>(MOD_ENV_TO_VCF), 0, 0
        },
        {0, 0, 0, 0, 40, 0, 1, 0, 0, 0}  // MOD_ENV_TO_VCF=40
    },
    
    // Preset 5: Strings - Lush strings with detuned oscillators
    {
        "String 1",
        {
            1, 0, 50, 0,        // DCO1: 8', SAW
            1, 0, 55, 0,        // DCO2: 8', SAW, detune for richness
            50, 75, 16, 25,     // Equal mix, bright, low reso, low key track
            47, 43, 59, 47,     // VCF env: slow attack, long release
            51, 43, 79, 63,     // VCA env: slow attack, very long release
            38, static_cast<uint8_t>(MOD_LFO_TO_VCO), 0, 0
        },
        {0, 0, 20, 0, 0, 0, 1, 0, 0, 0}  // MOD_LFO_TO_VCO=20 (vibrato)
    }
};

}  // namespace presets
