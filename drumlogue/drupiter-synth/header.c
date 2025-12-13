/**
 * @file header.c
 * @brief drumlogue SDK unit header for Drupiter Jupiter-8 synth
 *
 * Based on Bristol Jupiter-8 emulation
 * Original Bristol code: Copyright (c) Nick Copeland
 * Drumlogue port: 2025
 *
 * Parameter layout inspired by Roland Jupiter-8:
 * - Page 1: DCO-1 (Oscillator 1)
 * - Page 2: DCO-2 (Oscillator 2)
 * - Page 3: VCF (Voltage Controlled Filter)
 * - Page 4: VCF Envelope
 * - Page 5: VCA Envelope
 * - Page 6: LFO & Modulation
 */

#include "unit.h"  // Common definitions for all units

// ---- Unit header definition  --------------------------------------------------------------------

const __unit_header unit_header_t unit_header = {
    .header_size = sizeof(unit_header_t),                  // leave as is, size of this header
    .target = UNIT_TARGET_PLATFORM | k_unit_module_synth,  // target platform and module for this unit
    .api = UNIT_API_VERSION,                               // logue sdk API version against which unit was built
    .dev_id = 0x434C444DU,                                 // developer id ("CLDM")
    .unit_id = 0x00000004U,                                // unit id unique within dev_id scope
    .version = 0x010000U,                                  // v1.0.0 (major<<16 | minor<<8 | patch)
    .name = "Drupiter",                                    // displayed name, 7-bit ASCII, max 13 chars
    .num_presets = 6,                                      // 6 presets
    .num_params = 24,                                      // number of parameters (6 pages x 4)
    .params = {
        // Format: min, max, center, default, type, fractional, frac. type, <reserved>, name

        // ==================== Page 1: DCO-1 ====================
        // DCO-1 Octave: 16', 8', 4'
        {0, 2, 1, 1, k_unit_param_type_strings, 0, 0, 0, {"D1 OCT"}},
        // DCO-1 Waveform (JP-8 VCO1): Saw, Square, Pulse, Triangle
        {0, 3, 0, 1, k_unit_param_type_strings, 0, 0, 0, {"D1 WAVE"}},
        // DCO-1 Pulse Width: 0-100% (only affects PULSE waveform)
        {0, 100, 0, 50, k_unit_param_type_percent, 0, 0, 0, {"D1 PW"}},
        // DCO-1 Level: 0-100%
        {0, 100, 0, 79, k_unit_param_type_percent, 0, 0, 0, {"D1 LEVEL"}},

        // ==================== Page 2: DCO-2 ====================
        // DCO-2 Octave: 16', 8', 4'
        {0, 2, 1, 1, k_unit_param_type_strings, 0, 0, 0, {"D2 OCT"}},
        // DCO-2 Waveform (JP-8 VCO2): Saw, Noise, Pulse, Sine
        {0, 3, 0, 0, k_unit_param_type_strings, 0, 0, 0, {"D2 WAVE"}},
        // DCO-2 Detune: 0-100 (50 = center)
        {0, 100, 50, 50, k_unit_param_type_percent, 0, 0, 0, {"D2 TUNE"}},
        // DCO-2 Level: 0-100%
        {0, 100, 0, 79, k_unit_param_type_percent, 0, 0, 0, {"D2 LEVEL"}},

        // ==================== Page 3: VCF ====================
        // VCF Cutoff: Filter cutoff frequency
        {0, 100, 0, 79, k_unit_param_type_percent, 0, 0, 0, {"CUTOFF"}},
        // VCF Resonance: Filter resonance/Q
        {0, 100, 0, 16, k_unit_param_type_percent, 0, 0, 0, {"RESO"}},
        // VCF Envelope Amount: 0-100 (50 = center / bipolar)
        {0, 100, 50, 63, k_unit_param_type_percent, 0, 0, 0, {"ENV AMT"}},
        // VCF Type: LP12/LP24/HP12/BP12
        {0, 3, 0, 1, k_unit_param_type_strings, 0, 0, 0, {"VCF TYP"}},

        // ==================== Page 4: VCF Envelope ====================
        // Filter Envelope Attack: 0-127 (0 = instant, 127 = slow)
        {0, 100, 0, 4, k_unit_param_type_percent, 0, 0, 0, {"F.ATK"}},
        // Filter Envelope Decay: 0-127
        {0, 100, 0, 31, k_unit_param_type_percent, 0, 0, 0, {"F.DCY"}},
        // Filter Envelope Sustain: 0-127
        {0, 100, 0, 50, k_unit_param_type_percent, 0, 0, 0, {"F.SUS"}},
        // Filter Envelope Release: 0-127
        {0, 100, 0, 24, k_unit_param_type_percent, 0, 0, 0, {"F.REL"}},

        // ==================== Page 5: VCA Envelope ====================
        // Amp Envelope Attack: 0-127
        {0, 100, 0, 1, k_unit_param_type_percent, 0, 0, 0, {"A.ATK"}},
        // Amp Envelope Decay: 0-127
        {0, 100, 0, 39, k_unit_param_type_percent, 0, 0, 0, {"A.DCY"}},
        // Amp Envelope Sustain: 0-127
        {0, 100, 0, 79, k_unit_param_type_percent, 0, 0, 0, {"A.SUS"}},
        // Amp Envelope Release: 0-127
        {0, 100, 0, 16, k_unit_param_type_percent, 0, 0, 0, {"A.REL"}},

        // ==================== Page 6: LFO & Modulation ====================
        // LFO Rate: 0-127 (0.1Hz to ~50Hz)
        {0, 100, 0, 32, k_unit_param_type_percent, 0, 0, 0, {"LFO RATE"}},
        // LFO Waveform: Triangle, Ramp, Square, S&H
        {0, 3, 0, 0, k_unit_param_type_strings, 0, 0, 0, {"LFO WAV"}},
        // LFO to VCO Depth: 0-127 (vibrato amount)
        {0, 100, 0, 0, k_unit_param_type_percent, 0, 0, 0, {"LFO>VCO"}},
        // LFO to VCF Depth: 0-127 (filter modulation)
        {0, 100, 0, 0, k_unit_param_type_percent, 0, 0, 0, {"LFO>VCF"}},
    }
};
