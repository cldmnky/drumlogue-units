/**
 * @file header.c
 * @brief drumlogue SDK unit header for Vapo2 wavetable synth
 *
 * Inspired by VAST Dynamics Vaporizer2
 * Using wavetable techniques from Mutable Instruments Plaits
 */

#include "unit.h"

// ---- Unit header definition -----------------------------------------------

const __unit_header unit_header_t unit_header = {
    .header_size = sizeof(unit_header_t),
    .target = UNIT_TARGET_PLATFORM | k_unit_module_synth,
    .api = UNIT_API_VERSION,
    .dev_id = 0x434C444DU,    // "CLDM" - developer ID
    .unit_id = 0x00000003U,   // Unique unit ID (3 = vapo2)
    .version = 0x00010101U,   // v1.1.1 - Presets + LFO morph
    .name = "Vapo2",          // Display name (max 13 chars)
    .num_presets = 6,         // Built-in factory presets
    .num_params = 24,         // 6 pages × 4 parameters
    .params = {
        // Format: min, max, center, default, type, fractional, frac_mode, reserved, name

        // ==================== Page 1: Oscillator A ====================
        // OSC A BANK: Wavetable bank selection (16 banks)
        {0, 15, 0, 0, k_unit_param_type_strings, 0, 0, 0, {"A BANK"}},
        // OSC A MORPH: Position within wavetable (morphs between waves)
        {0, 127, 0, 0, k_unit_param_type_none, 0, 0, 0, {"A MORPH"}},
        // OSC A OCTAVE: Octave offset
        {-3, 3, 0, 0, k_unit_param_type_none, 0, 0, 0, {"A OCT"}},
        // OSC A TUNE: Fine tuning in cents
        {-64, 63, 0, 0, k_unit_param_type_none, 0, 0, 0, {"A TUNE"}},

        // ==================== Page 2: Oscillator B ====================
        // OSC B BANK: Wavetable bank selection (16 banks)
        {0, 15, 0, 0, k_unit_param_type_strings, 0, 0, 0, {"B BANK"}},
        // OSC B MORPH: Position within wavetable
        {0, 127, 0, 0, k_unit_param_type_none, 0, 0, 0, {"B MORPH"}},
        // OSC B OCTAVE: Octave offset relative to A
        {-3, 3, 0, 0, k_unit_param_type_none, 0, 0, 0, {"B OCT"}},
        // OSC MODE: PPG interpolation mode (0=HiFi, 1=LoFi, 2=Raw) - default HiFi
        {0, 2, 0, 0, k_unit_param_type_strings, 0, 0, 0, {"OSC MOD"}},

        // ==================== Page 3: Filter ====================
        // CUTOFF: Filter cutoff frequency
        {0, 127, 0, 127, k_unit_param_type_none, 0, 0, 0, {"CUTOFF"}},
        // RESONANCE: Filter resonance
        {0, 127, 0, 0, k_unit_param_type_none, 0, 0, 0, {"RESO"}},
        // FILTER ENV: Envelope modulation amount (bipolar)
        {-64, 63, 0, 32, k_unit_param_type_none, 0, 0, 0, {"FLT ENV"}},
        // FILTER TYPE: LP12/LP24/HP12/BP12
        {0, 3, 0, 0, k_unit_param_type_strings, 0, 0, 0, {"FLT TYP"}},

        // ==================== Page 4: Amplitude Envelope ====================
        // ATTACK: Amp envelope attack time
        {0, 127, 0, 5, k_unit_param_type_none, 0, 0, 0, {"ATTACK"}},
        // DECAY: Amp envelope decay time
        {0, 127, 0, 40, k_unit_param_type_none, 0, 0, 0, {"DECAY"}},
        // SUSTAIN: Amp envelope sustain level
        {0, 127, 0, 80, k_unit_param_type_none, 0, 0, 0, {"SUSTAIN"}},
        // RELEASE: Amp envelope release time
        {0, 127, 0, 30, k_unit_param_type_none, 0, 0, 0, {"RELEASE"}},

        // ==================== Page 5: Filter Envelope ====================
        // F.ATTACK: Filter envelope attack time
        {0, 127, 0, 10, k_unit_param_type_none, 0, 0, 0, {"F.ATK"}},
        // F.DECAY: Filter envelope decay time
        {0, 127, 0, 50, k_unit_param_type_none, 0, 0, 0, {"F.DCY"}},
        // F.SUSTAIN: Filter envelope sustain level
        {0, 127, 0, 40, k_unit_param_type_none, 0, 0, 0, {"F.SUS"}},
        // F.RELEASE: Filter envelope release time
        {0, 127, 0, 40, k_unit_param_type_none, 0, 0, 0, {"F.REL"}},

        // ==================== Page 6: MOD HUB & Output ====================
        // MOD SELECT: Choose modulation destination (0-7)
        {0, 7, 0, 0, k_unit_param_type_strings, 0, 0, 0, {"MOD SEL"}},
        // MOD VALUE: Value for selected destination (0-127, interpreted per destination)
        {0, 127, 0, 64, k_unit_param_type_strings, 0, 0, 0, {"MOD VAL"}},
        // OSC MIX: Balance between Osc A and B (bipolar: -64=A, 0=50/50, +63=B)
        {-64, 63, 0, 0, k_unit_param_type_none, 0, 0, 0, {"OSC MIX"}},
        // SPACE: Stereo width (bipolar: -64=mono, 0=normal, +63=wide)
        {-64, 63, 0, 0, k_unit_param_type_none, 0, 0, 0, {"SPACE"}},
    }
};
