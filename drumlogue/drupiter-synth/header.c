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
        // DCO-1 Range: Octave (0=16', 1=8', 2=4')
        {0, 2, 0, 1, k_unit_param_type_strings, 0, 0, 0, {"D1 OCT"}},
        // DCO-1 Waveform: SAW/SQR/PULSE/TRI
        {0, 3, 0, 0, k_unit_param_type_strings, 0, 0, 0, {"D1 WAVE"}},
        // DCO-1 Pulse Width: 0-100%
        {0, 100, 50, 50, k_unit_param_type_percent, 0, 0, 0, {"D1 PW"}},
        // DCO-1 Cross Modulation: DCO2->DCO1 FM depth (Jupiter-8 feature)
        {0, 100, 0, 0, k_unit_param_type_percent, 0, 0, 0, {"XMOD"}},

        // ==================== Page 2: DCO-2 ====================
        // DCO-2 Range: Octave (0=16', 1=8', 2=4')
        {0, 2, 0, 1, k_unit_param_type_strings, 0, 0, 0, {"D2 OCT"}},
        // DCO-2 Waveform: SAW/NOISE/PULSE/SINE
        {0, 3, 0, 0, k_unit_param_type_strings, 0, 0, 0, {"D2 WAVE"}},
        // DCO-2 Detune: -50 to +50 cents (50=center)
        {0, 100, 50, 50, k_unit_param_type_strings, 0, 0, 0, {"D2 TUNE"}},
        // DCO-2 Sync: OFF/SOFT/HARD (Jupiter-8 sync to DCO1)
        {0, 2, 0, 0, k_unit_param_type_strings, 0, 0, 0, {"SYNC"}},

        // ==================== Page 3: MIX & VCF ====================
        // Oscillator Mix: 0=D1 only, 50=equal, 100=D2 only
        {0, 100, 50, 79, k_unit_param_type_percent, 0, 0, 0, {"MIX"}},
        // VCF Cutoff: Filter cutoff frequency
        {0, 100, 0, 79, k_unit_param_type_percent, 0, 0, 0, {"CUTOFF"}},
        // VCF Resonance: Filter resonance/Q
        {0, 100, 0, 16, k_unit_param_type_percent, 0, 0, 0, {"RESO"}},
        // VCF Keyboard Tracking: 0-100% (50%=standard)
        {0, 100, 50, 50, k_unit_param_type_percent, 0, 0, 0, {"KEYFLW"}},

        // ==================== Page 4: VCF Envelope ====================
        // Filter Envelope Attack
        {0, 100, 0, 4, k_unit_param_type_percent, 0, 0, 0, {"F.ATK"}},
        // Filter Envelope Decay
        {0, 100, 0, 31, k_unit_param_type_percent, 0, 0, 0, {"F.DCY"}},
        // Filter Envelope Sustain
        {0, 100, 0, 50, k_unit_param_type_percent, 0, 0, 0, {"F.SUS"}},
        // Filter Envelope Release
        {0, 100, 0, 24, k_unit_param_type_percent, 0, 0, 0, {"F.REL"}},

        // ==================== Page 5: VCA Envelope ====================
        // Amp Envelope Attack
        {0, 100, 0, 1, k_unit_param_type_percent, 0, 0, 0, {"A.ATK"}},
        // Amp Envelope Decay
        {0, 100, 0, 39, k_unit_param_type_percent, 0, 0, 0, {"A.DCY"}},
        // Amp Envelope Sustain
        {0, 100, 0, 79, k_unit_param_type_percent, 0, 0, 0, {"A.SUS"}},
        // Amp Envelope Release
        {0, 100, 0, 16, k_unit_param_type_percent, 0, 0, 0, {"A.REL"}},

        // ==================== Page 6: MODULATION ====================
        // LFO RATE: Direct LFO speed control (0.1Hz - 50Hz)
        {0, 100, 0, 32, k_unit_param_type_percent, 0, 0, 0, {"LFO RT"}},
        // MOD HUB: Modulation destination selector (9 modes)
        {0, 8, 0, 0, k_unit_param_type_strings, 0, 0, 0, {"MOD HUB"}},
        // MOD AMT: Value for selected destination (context-sensitive string display)
        {0, 100, 50, 50, k_unit_param_type_strings, 0, 0, 0, {"MOD AMT"}},
        // EFFECT: Output effect selector (CHORUS/SPACE/DRY/BOTH)
        {0, 3, 0, 0, k_unit_param_type_strings, 0, 0, 0, {"EFFECT"}},
    }
};
