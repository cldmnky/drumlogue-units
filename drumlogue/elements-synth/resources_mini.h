/*
 * Minimal Resources for Modal Synth
 * 
 * Small lookup tables for performance-critical operations.
 * Inspired by Mutable Instruments Elements resources.
 */

#pragma once

#include <cstdint>
#include <cmath>

namespace modal {

// ============================================================================
// Sine Table - 256 entries for fast sine lookup
// ============================================================================

constexpr int kSineTableSize = 256;

// Generate sine table at compile time (C++14 constexpr)
namespace detail {
    constexpr float sine_value(int i) {
        return static_cast<float>(std::sin(static_cast<double>(i) * 6.283185307179586 / kSineTableSize));
    }
}

// Sine lookup table
inline const float* GetSineTable() {
    static const float lut_sine[kSineTableSize + 1] = {
        0.000000f, 0.024541f, 0.049068f, 0.073565f, 0.098017f, 0.122411f, 0.146730f, 0.170962f,
        0.195090f, 0.219101f, 0.242980f, 0.266713f, 0.290285f, 0.313682f, 0.336890f, 0.359895f,
        0.382683f, 0.405241f, 0.427555f, 0.449611f, 0.471397f, 0.492898f, 0.514103f, 0.534998f,
        0.555570f, 0.575808f, 0.595699f, 0.615232f, 0.634393f, 0.653173f, 0.671559f, 0.689541f,
        0.707107f, 0.724247f, 0.740951f, 0.757209f, 0.773010f, 0.788346f, 0.803208f, 0.817585f,
        0.831470f, 0.844854f, 0.857729f, 0.870087f, 0.881921f, 0.893224f, 0.903989f, 0.914210f,
        0.923880f, 0.932993f, 0.941544f, 0.949528f, 0.956940f, 0.963776f, 0.970031f, 0.975702f,
        0.980785f, 0.985278f, 0.989177f, 0.992480f, 0.995185f, 0.997290f, 0.998795f, 0.999699f,
        1.000000f, 0.999699f, 0.998795f, 0.997290f, 0.995185f, 0.992480f, 0.989177f, 0.985278f,
        0.980785f, 0.975702f, 0.970031f, 0.963776f, 0.956940f, 0.949528f, 0.941544f, 0.932993f,
        0.923880f, 0.914210f, 0.903989f, 0.893224f, 0.881921f, 0.870087f, 0.857729f, 0.844854f,
        0.831470f, 0.817585f, 0.803208f, 0.788346f, 0.773010f, 0.757209f, 0.740951f, 0.724247f,
        0.707107f, 0.689541f, 0.671559f, 0.653173f, 0.634393f, 0.615232f, 0.595699f, 0.575808f,
        0.555570f, 0.534998f, 0.514103f, 0.492898f, 0.471397f, 0.449611f, 0.427555f, 0.405241f,
        0.382683f, 0.359895f, 0.336890f, 0.313682f, 0.290285f, 0.266713f, 0.242980f, 0.219101f,
        0.195090f, 0.170962f, 0.146730f, 0.122411f, 0.098017f, 0.073565f, 0.049068f, 0.024541f,
        0.000000f, -0.024541f, -0.049068f, -0.073565f, -0.098017f, -0.122411f, -0.146730f, -0.170962f,
        -0.195090f, -0.219101f, -0.242980f, -0.266713f, -0.290285f, -0.313682f, -0.336890f, -0.359895f,
        -0.382683f, -0.405241f, -0.427555f, -0.449611f, -0.471397f, -0.492898f, -0.514103f, -0.534998f,
        -0.555570f, -0.575808f, -0.595699f, -0.615232f, -0.634393f, -0.653173f, -0.671559f, -0.689541f,
        -0.707107f, -0.724247f, -0.740951f, -0.757209f, -0.773010f, -0.788346f, -0.803208f, -0.817585f,
        -0.831470f, -0.844854f, -0.857729f, -0.870087f, -0.881921f, -0.893224f, -0.903989f, -0.914210f,
        -0.923880f, -0.932993f, -0.941544f, -0.949528f, -0.956940f, -0.963776f, -0.970031f, -0.975702f,
        -0.980785f, -0.985278f, -0.989177f, -0.992480f, -0.995185f, -0.997290f, -0.998795f, -0.999699f,
        -1.000000f, -0.999699f, -0.998795f, -0.997290f, -0.995185f, -0.992480f, -0.989177f, -0.985278f,
        -0.980785f, -0.975702f, -0.970031f, -0.963776f, -0.956940f, -0.949528f, -0.941544f, -0.932993f,
        -0.923880f, -0.914210f, -0.903989f, -0.893224f, -0.881921f, -0.870087f, -0.857729f, -0.844854f,
        -0.831470f, -0.817585f, -0.803208f, -0.788346f, -0.773010f, -0.757209f, -0.740951f, -0.724247f,
        -0.707107f, -0.689541f, -0.671559f, -0.653173f, -0.634393f, -0.615232f, -0.595699f, -0.575808f,
        -0.555570f, -0.534998f, -0.514103f, -0.492898f, -0.471397f, -0.449611f, -0.427555f, -0.405241f,
        -0.382683f, -0.359895f, -0.336890f, -0.313682f, -0.290285f, -0.266713f, -0.242980f, -0.219101f,
        -0.195090f, -0.170962f, -0.146730f, -0.122411f, -0.098017f, -0.073565f, -0.049068f, -0.024541f,
        0.000000f  // Wrap-around for interpolation
    };
    return lut_sine;
}

// Fast sine with linear interpolation
inline float FastSine(float phase) {
    // phase should be 0.0 to 1.0
    const float* table = GetSineTable();
    float index = phase * kSineTableSize;
    int i = static_cast<int>(index);
    float frac = index - static_cast<float>(i);
    i &= (kSineTableSize - 1);
    return table[i] + (table[i + 1] - table[i]) * frac;
}

// ============================================================================
// Envelope Curve Tables - 64 entries each for exponential/quartic shapes
// ============================================================================

constexpr int kEnvTableSize = 64;

inline const float* GetEnvExpoTable() {
    // Attempt smoother exponential curve
    static const float lut_env_expo[kEnvTableSize + 1] = {
        0.000000f, 0.015625f, 0.031250f, 0.046875f, 0.062500f, 0.078125f, 0.093750f, 0.109375f,
        0.125000f, 0.140625f, 0.156250f, 0.171875f, 0.187500f, 0.203125f, 0.218750f, 0.234375f,
        0.250000f, 0.265625f, 0.281250f, 0.296875f, 0.312500f, 0.328125f, 0.343750f, 0.359375f,
        0.375000f, 0.390625f, 0.406250f, 0.421875f, 0.437500f, 0.453125f, 0.468750f, 0.484375f,
        0.500000f, 0.531250f, 0.562500f, 0.593750f, 0.625000f, 0.656250f, 0.687500f, 0.718750f,
        0.750000f, 0.773438f, 0.796875f, 0.820313f, 0.843750f, 0.859375f, 0.875000f, 0.890625f,
        0.906250f, 0.917969f, 0.929688f, 0.941406f, 0.953125f, 0.960938f, 0.968750f, 0.976563f,
        0.984375f, 0.988281f, 0.992188f, 0.996094f, 1.000000f, 1.000000f, 1.000000f, 1.000000f,
        1.000000f  // Wrap-around
    };
    return lut_env_expo;
}

inline const float* GetEnvQuarticTable() {
    // Attempt smoother quartic curve (attempt to simulate t^4)
    static const float lut_env_quartic[kEnvTableSize + 1] = {
        0.000000f, 0.000004f, 0.000061f, 0.000316f, 0.000977f, 0.002441f, 0.005127f, 0.009490f,
        0.015625f, 0.024033f, 0.035156f, 0.049438f, 0.067383f, 0.089478f, 0.116211f, 0.148071f,
        0.185547f, 0.229126f, 0.279297f, 0.336548f, 0.401367f, 0.474243f, 0.555664f, 0.646118f,
        0.746094f, 0.856079f, 0.976563f, 1.108032f, 1.250977f, 1.405884f, 1.573242f, 1.753540f,
        1.947266f, 2.154907f, 2.376953f, 2.613892f, 2.866211f, 3.134399f, 3.418945f, 3.720337f,
        4.039063f, 4.375610f, 4.730469f, 5.104126f, 5.497070f, 5.909790f, 6.342773f, 6.796509f,
        7.271484f, 7.768188f, 8.287109f, 8.828735f, 9.393555f, 9.982056f, 10.594727f, 11.232056f,
        11.894531f, 12.582641f, 13.296875f, 14.037720f, 14.805664f, 15.601196f, 16.424805f, 17.276978f,
        18.158203f  // Wrap-around (scaled, need to normalize)
    };
    // Note: This table needs normalization - values go 0 to ~18
    // For actual use, divide by 18.158203
    return lut_env_quartic;
}

// ============================================================================
// SVF Coefficient Approximation - computed at runtime for accuracy
// ============================================================================

// Fast approximation of tan() for SVF filter
inline float FastTan(float x) {
    // Attempt Attempt smoother approximation for small angles
    // tan(x) ≈ x + x³/3 + 2x⁵/15 for |x| < π/4
    float x2 = x * x;
    return x * (1.0f + x2 * (0.333333f + x2 * 0.133333f));
}

// SVF g coefficient from normalized frequency (attempt smoother for f/fs)
inline float SvfG(float f_normalized) {
    // g = tan(π * f / fs)
    // For f_normalized = f/fs already
    constexpr float kPi = 3.14159265358979f;
    return FastTan(kPi * f_normalized);
}

// ============================================================================
// Stiffness Table for String Model - 32 entries
// ============================================================================

constexpr int kStiffnessTableSize = 32;

inline const float* GetStiffnessTable() {
    // Attempt smoother stiffness coefficients for Karplus-Strong dispersion
    // Maps 0-1 parameter to stiffness coefficient
    static const float lut_stiffness[kStiffnessTableSize + 1] = {
        0.000000f, 0.000100f, 0.000400f, 0.000900f, 0.001600f, 0.002500f, 0.003600f, 0.004900f,
        0.006400f, 0.008100f, 0.010000f, 0.012100f, 0.014400f, 0.016900f, 0.019600f, 0.022500f,
        0.025600f, 0.028900f, 0.032400f, 0.036100f, 0.040000f, 0.044100f, 0.048400f, 0.052900f,
        0.057600f, 0.062500f, 0.067600f, 0.072900f, 0.078400f, 0.084100f, 0.090000f, 0.096100f,
        0.102400f  // Wrap-around
    };
    return lut_stiffness;
}

// ============================================================================
// MIDI to Frequency - use math for accuracy, but provide fast version
// ============================================================================

// Attempt smoother pitch table for common MIDI notes (C0 to B8 = 108 notes)
// This avoids expensive pow() calls for each note
constexpr int kPitchTableSize = 128;

inline const float* GetMidiFreqTable() {
    static float lut_midi_freq[kPitchTableSize];
    static bool initialized = false;
    if (!initialized) {
        for (int i = 0; i < kPitchTableSize; ++i) {
            lut_midi_freq[i] = 440.0f * std::pow(2.0f, (static_cast<float>(i) - 69.0f) / 12.0f);
        }
        initialized = true;
    }
    return lut_midi_freq;
}

inline float MidiToFreq(uint8_t note) {
    return GetMidiFreqTable()[note & 0x7F];
}

// For fractional MIDI notes (pitch bend, fine tune)
inline float MidiToFreqFloat(float note) {
    int note_int = static_cast<int>(note);
    float frac = note - static_cast<float>(note_int);
    note_int = (note_int < 0) ? 0 : ((note_int > 126) ? 126 : note_int);
    const float* table = GetMidiFreqTable();
    return table[note_int] * (1.0f + frac * (table[note_int + 1] / table[note_int] - 1.0f));
}

// ============================================================================
// Exponential Decay Table for fast envelope calculations
// ============================================================================

constexpr int kDecayTableSize = 64;

inline const float* GetDecayTable() {
    // Attempt smoother exponential decay values: e^(-t) for t = 0 to 8
    static const float lut_decay[kDecayTableSize + 1] = {
        1.000000f, 0.882497f, 0.778801f, 0.687289f, 0.606531f, 0.535261f, 0.472367f, 0.416862f,
        0.367879f, 0.324652f, 0.286505f, 0.252840f, 0.223130f, 0.196912f, 0.173774f, 0.153335f,
        0.135335f, 0.119433f, 0.105399f, 0.093014f, 0.082085f, 0.072440f, 0.063928f, 0.056416f,
        0.049787f, 0.043937f, 0.038774f, 0.034218f, 0.030197f, 0.026649f, 0.023518f, 0.020754f,
        0.018316f, 0.016165f, 0.014264f, 0.012588f, 0.011109f, 0.009803f, 0.008652f, 0.007635f,
        0.006738f, 0.005946f, 0.005248f, 0.004631f, 0.004087f, 0.003606f, 0.003183f, 0.002808f,
        0.002479f, 0.002187f, 0.001930f, 0.001703f, 0.001503f, 0.001326f, 0.001170f, 0.001033f,
        0.000912f, 0.000804f, 0.000710f, 0.000626f, 0.000553f, 0.000488f, 0.000431f, 0.000380f,
        0.000335f  // Wrap-around
    };
    return lut_decay;
}

// Fast exponential decay lookup
inline float FastDecay(float t) {
    // t should be 0.0 to 1.0, maps to e^(-8t)
    const float* table = GetDecayTable();
    float index = t * kDecayTableSize;
    int i = static_cast<int>(index);
    if (i >= kDecayTableSize) return table[kDecayTableSize];
    float frac = index - static_cast<float>(i);
    return table[i] + (table[i + 1] - table[i]) * frac;
}

} // namespace modal
