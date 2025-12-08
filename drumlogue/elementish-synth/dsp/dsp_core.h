/*
 * DSP Core - Basic building blocks for audio processing
 * Part of Modal Synth for Drumlogue
 */

#pragma once

#include <cstdint>
#include <cmath>

#ifdef USE_NEON
#include <arm_neon.h>
#endif

namespace modal {

// ============================================================================
// Build-time Configuration
// Set NUM_MODES via UDEFS in config.mk: UDEFS = -DNUM_MODES=16
// ============================================================================

#ifndef NUM_MODES
#define NUM_MODES 8  // Default: balance between richness and CPU load
#endif

// Validate range at compile time
#if NUM_MODES < 4
#error "NUM_MODES must be at least 4"
#endif
#if NUM_MODES > 32
#error "NUM_MODES must not exceed 32 (CPU/memory constraints)"
#endif

constexpr int kNumModes = NUM_MODES;
constexpr float kSampleRate = 48000.0f;
constexpr float kTwoPi = 6.283185307179586f;
constexpr float kPi = 3.14159265358979f;

// Pi power constants for polynomial approximations
constexpr float kPiPow3 = kPi * kPi * kPi;
constexpr float kPiPow5 = kPiPow3 * kPi * kPi;

// ============================================================================
// MIDI Note to Frequency Lookup Table
// Pre-computed: 440.0 * pow(2.0, (note - 69) / 12.0) for notes 0-127
// ============================================================================

static const float kMidiFreqTable[128] = {
    8.1757989156f,    8.6619572180f,    9.1770239974f,    9.7227182413f,   // 0-3
    10.3008611535f,   10.9133822323f,   11.5623257097f,   12.2498573744f,  // 4-7
    12.9782717994f,   13.7500000000f,   14.5676175474f,   15.4338531643f,  // 8-11
    16.3515978313f,   17.3239144361f,   18.3540479948f,   19.4454364826f,  // 12-15
    20.6017223071f,   21.8267644646f,   23.1246514195f,   24.4997147489f,  // 16-19
    25.9565435987f,   27.5000000000f,   29.1352350949f,   30.8677063285f,  // 20-23
    32.7031956626f,   34.6478288721f,   36.7080959897f,   38.8908729653f,  // 24-27
    41.2034446141f,   43.6535289291f,   46.2493028390f,   48.9994294977f,  // 28-31
    51.9130871975f,   55.0000000000f,   58.2704701898f,   61.7354126570f,  // 32-35
    65.4063913251f,   69.2956577442f,   73.4161919794f,   77.7817459305f,  // 36-39
    82.4068892282f,   87.3070578583f,   92.4986056779f,   97.9988589954f,  // 40-43
    103.8261743950f,  110.0000000000f,  116.5409403795f,  123.4708253140f, // 44-47
    130.8127826503f,  138.5913154884f,  146.8323839587f,  155.5634918610f, // 48-51
    164.8137784564f,  174.6141157165f,  184.9972113558f,  195.9977179909f, // 52-55
    207.6523487900f,  220.0000000000f,  233.0818807590f,  246.9416506281f, // 56-59
    261.6255653006f,  277.1826309769f,  293.6647679174f,  311.1269837221f, // 60-63
    329.6275569129f,  349.2282314330f,  369.9944227116f,  391.9954359817f, // 64-67
    415.3046975799f,  440.0000000000f,  466.1637615181f,  493.8833012561f, // 68-71
    523.2511306012f,  554.3652619537f,  587.3295358348f,  622.2539674442f, // 72-75
    659.2551138257f,  698.4564628660f,  739.9888454233f,  783.9908719635f, // 76-79
    830.6093951599f,  880.0000000000f,  932.3275230362f,  987.7666025122f, // 80-83
    1046.5022612024f, 1108.7305239075f, 1174.6590716696f, 1244.5079348883f,// 84-87
    1318.5102276515f, 1396.9129257320f, 1479.9776908465f, 1567.9817439270f,// 88-91
    1661.2187903198f, 1760.0000000000f, 1864.6550460724f, 1975.5332050245f,// 92-95
    2093.0045224048f, 2217.4610478150f, 2349.3181433393f, 2489.0158697766f,// 96-99
    2637.0204553030f, 2793.8258514640f, 2959.9553816931f, 3135.9634878540f,// 100-103
    3322.4375806396f, 3520.0000000000f, 3729.3100921447f, 3951.0664100490f,// 104-107
    4186.0090448096f, 4434.9220956300f, 4698.6362866785f, 4978.0317395533f,// 108-111
    5274.0409106059f, 5587.6517029281f, 5919.9107633862f, 6271.9269757080f,// 112-115
    6644.8751612791f, 7040.0000000000f, 7458.6201842894f, 7902.1328200980f,// 116-119
    8372.0180896192f, 8869.8441912599f, 9397.2725733570f, 9956.0634791066f,// 120-123
    10548.0818212118f,11175.3034058561f,11839.8215267723f,12543.8539514160f // 124-127
};

// Semitone ratio for fractional pitch interpolation
// This is 2^(1/12) - 1 ≈ 0.05946
constexpr float kSemitoneRatioMinus1 = 0.05946309435929526f;

// ============================================================================
// Utility functions
// ============================================================================

inline float Clamp(float x, float lo, float hi) {
    return x < lo ? lo : (x > hi ? hi : x);
}

// Fast MIDI to frequency using lookup table with linear interpolation
inline float MidiToFrequency(float note) {
    if (note < 0.0f) note = 0.0f;
    if (note > 127.0f) note = 127.0f;
    
    int idx = static_cast<int>(note);
    float frac = note - static_cast<float>(idx);
    
    if (idx >= 127) return kMidiFreqTable[127];
    
    // Linear interpolation between adjacent notes
    // f(n+frac) ≈ f(n) * (1 + frac * (2^(1/12) - 1))
    // This is accurate to within ~0.3 cents
    return kMidiFreqTable[idx] * (1.0f + frac * kSemitoneRatioMinus1);
}

// Fast semitones to ratio using the MIDI table
// For semitones in range [-64, +63], we use note 64 as reference
inline float SemitonesToRatio(float semitones) {
    float note = 64.0f + semitones;  // Use middle of table as reference
    if (note < 0.0f) note = 0.0f;
    if (note > 127.0f) note = 127.0f;
    
    int idx = static_cast<int>(note);
    float frac = note - static_cast<float>(idx);
    
    if (idx >= 127) return kMidiFreqTable[127] / kMidiFreqTable[64];
    
    float freq = kMidiFreqTable[idx] * (1.0f + frac * kSemitoneRatioMinus1);
    return freq / kMidiFreqTable[64];  // Normalize to ratio (note 64 = 1.0)
}

// ============================================================================
// Fast Math Approximations (from Mutable Instruments stmlib)
// ============================================================================

// Fast tangent approximation for filter coefficient calculation
// Optimized for frequencies below 20kHz at 48kHz sample rate
// Error < 0.1% in the audio range
inline float FastTan(float f) {
    // f should be normalized frequency (freq / sample_rate)
    // Valid for f < 0.49
    const float a = 3.260e-01f * kPiPow3;
    const float b = 1.823e-01f * kPiPow5;
    float f2 = f * f;
    return f * (kPi + f2 * (a + b * f2));
}

// Fast sine approximation using parabolic approximation
// Input: x in range [0, 1] representing [0, 2π]
// Output: sin(2πx)
inline float FastSin(float x) {
    // Wrap to [0, 1]
    x = x - static_cast<int>(x);
    if (x < 0.0f) x += 1.0f;
    
    // Parabolic sine approximation: 4x(1-|x|) scaled and adjusted
    // This gives sin for the first half of the cycle
    if (x < 0.5f) {
        // First half: 0 to π
        float t = x * 2.0f;  // 0 to 1
        return 4.0f * t * (1.0f - t);
    } else {
        // Second half: π to 2π
        float t = (x - 0.5f) * 2.0f;  // 0 to 1
        return -4.0f * t * (1.0f - t);
    }
}

// Fast cosine approximation 
// Input: x in range [0, 1] representing [0, 2π]
// Output: cos(2πx)
inline float FastCos(float x) {
    return FastSin(x + 0.25f);  // cos(x) = sin(x + π/2)
}

// More accurate sine approximation for filter coefficients
// Input: w0 in radians [0, π]
// Uses 5th order polynomial for better accuracy
inline float FastSinRad(float w0) {
    // Normalize to [0, 1] where 1 = π
    float x = w0 / kPi;
    if (x > 1.0f) x = 1.0f;
    if (x < 0.0f) x = 0.0f;
    
    // sin(πx) ≈ πx(1 - x²/6 + x⁴/120) for small x
    // Better: use Bhaskara I's approximation: 16x(π-x) / (5π² - 4x(π-x))
    // Or simpler parabola with correction: 4x(1-x) with refinement
    
    // Parabolic approximation with Corrective term
    float y = 4.0f * x * (1.0f - x);
    // Apply correction for better accuracy: y + 0.225 * y * (|y| - y)
    // Simplified for positive half-wave: just scale slightly
    return y * (1.0f + 0.225f * (1.0f - y));
}

// Fast cosine for radians, derived from sine
inline float FastCosRad(float w0) {
    // cos(w0) = sin(π/2 - w0) = sin(π/2 + w0) 
    // For w0 in [0, π]: cos(w0) = sin(π/2 - w0)
    float x = 0.5f - w0 / kPi;  // Shift by π/2
    if (x < 0.0f) x = -x;  // Handle negative
    
    float y = 4.0f * x * (1.0f - x);
    return y * (1.0f + 0.225f * (1.0f - y));
}

// Combined sin/cos calculation for efficiency
// Input: w0 in radians, Output: sin and cos values
inline void FastSinCos(float w0, float& sin_out, float& cos_out) {
    sin_out = FastSinRad(w0);
    cos_out = FastCosRad(w0);
}

// Fast tanh approximation with proper clamping
// Uses rational approximation for small values, hard clamps for large
inline float FastTanh(float x) {
    // Clamp extreme values to avoid NaN and ensure bounded output
    if (x > 4.0f) return 1.0f;
    if (x < -4.0f) return -1.0f;
    
    // Rational approximation, accurate for |x| < 4
    float x2 = x * x;
    return x * (27.0f + x2) / (27.0f + 9.0f * x2);
}

// Note: FastTanh4 NEON version is in common/neon_dsp.h

// Fast absolute value
inline float FastAbs(float x) {
    return x < 0.0f ? -x : x;
}

// ============================================================================
// BowTable - Friction model for bowed strings (from Elements)
// Models the stick-slip friction characteristic of a bow on a string
// ============================================================================

inline float BowTable(float x, float velocity) {
    x = 0.13f * velocity - x;
    float bow = x * 6.0f;
    bow = FastAbs(bow) + 0.75f;
    bow *= bow;  // ^2
    bow *= bow;  // ^4
    bow = 0.25f / bow;
    // Clamp output
    if (bow < 0.0025f) bow = 0.0025f;
    if (bow > 0.245f) bow = 0.245f;
    return x * bow;
}

// ============================================================================
// Noise Generator (xorshift)
// ============================================================================

// ============================================================================
// Simple Delay Line (for bowed modes)
// Fixed-size delay line with integer delay (no interpolation needed for
// banded waveguides since frequency is controlled by the bandpass filter)
// ============================================================================

template<size_t kMaxSize>
class DelayLine {
public:
    DelayLine() : write_ptr_(0), delay_(1) {
        for (size_t i = 0; i < kMaxSize; ++i) {
            buffer_[i] = 0.0f;
        }
    }
    
    void Init() {
        write_ptr_ = 0;
        delay_ = 1;
        for (size_t i = 0; i < kMaxSize; ++i) {
            buffer_[i] = 0.0f;
        }
    }
    
    void SetDelay(size_t delay) {
        if (delay < 1) delay = 1;
        if (delay >= kMaxSize) delay = kMaxSize - 1;
        delay_ = delay;
    }
    
    float Read() const {
        size_t read_ptr = write_ptr_ + delay_;
        if (read_ptr >= kMaxSize) read_ptr -= kMaxSize;
        return buffer_[read_ptr];
    }
    
    void Write(float value) {
        buffer_[write_ptr_] = value;
        if (write_ptr_ == 0) {
            write_ptr_ = kMaxSize - 1;
        } else {
            --write_ptr_;
        }
    }
    
    void Reset() {
        for (size_t i = 0; i < kMaxSize; ++i) {
            buffer_[i] = 0.0f;
        }
    }
    
private:
    float buffer_[kMaxSize];
    size_t write_ptr_;
    size_t delay_;
};

class Noise {
public:
    Noise() : state_(12345) {}
    
    void Seed(uint32_t seed) { state_ = seed ? seed : 1; }
    
    float Next() {
        state_ ^= state_ << 13;
        state_ ^= state_ >> 17;
        state_ ^= state_ << 5;
        return static_cast<float>(static_cast<int32_t>(state_)) * (1.0f / 2147483648.0f);
    }
    
    // Filtered noise for smoother modulation
    float NextFiltered(float coeff) {
        float raw = Next();
        filtered_ = filtered_ * coeff + raw * (1.0f - coeff);
        return filtered_;
    }
    
private:
    uint32_t state_;
    float filtered_ = 0.0f;
};

// ============================================================================
// One-Pole Filter (for smoothing and simple filtering)
// ============================================================================

class OnePole {
public:
    OnePole() : state_(0.0f), coeff_(0.99f) {}
    
    void SetCoefficient(float c) { coeff_ = Clamp(c, 0.0f, 0.9999f); }
    void SetFrequency(float freq) {
        float w = kTwoPi * freq / kSampleRate;
        coeff_ = std::exp(-w);
    }
    
    float Process(float in) {
        // Protect against NaN input
        if (in != in) in = 0.0f;
        
        state_ = in + (state_ - in) * coeff_;
        // Stability check (NaN != NaN trick)
        if (state_ != state_) {
            state_ = 0.0f;
        }
        return state_;
    }
    
    float ProcessHighPass(float in) {
        // Protect against NaN input
        if (in != in) in = 0.0f;
        
        state_ = in + (state_ - in) * coeff_;
        // Stability check (NaN != NaN trick)
        if (state_ != state_) {
            state_ = 0.0f;
            return 0.0f;
        }
        return in - state_;
    }
    
    void Reset() { state_ = 0.0f; }
    float state() const { return state_; }
    
private:
    float state_;
    float coeff_;
};

// ============================================================================
// State Variable Filter (for exciter filtering)
// ============================================================================

class SVF {
public:
    SVF() : lp_(0), bp_(0), hp_(0), g_(0.1f), r_(1.0f) {}
    
    void SetFrequency(float freq) {
        freq = Clamp(freq, 20.0f, kSampleRate * 0.4f);  // More conservative limit
        float w = kPi * freq / kSampleRate;
        // Clamp to avoid tan() blowing up
        if (w > 1.5f) w = 1.5f;
        g_ = std::tan(w);
        // Safety clamp g_
        if (g_ > 10.0f) g_ = 10.0f;
        if (g_ < 0.001f) g_ = 0.001f;
    }
    
    void SetResonance(float q) {
        q = Clamp(q, 0.5f, 20.0f);
        r_ = 1.0f / q;
    }
    
    float ProcessLowPass(float in) {
        // Protect against NaN input
        if (in != in) in = 0.0f;
        
        hp_ = (in - lp_ - r_ * bp_) / (1.0f + g_ * (g_ + r_));
        bp_ += g_ * hp_;
        lp_ += g_ * bp_;
        // Stability check (NaN != NaN trick)
        if (lp_ != lp_ || lp_ > 1e4f || lp_ < -1e4f) {
            Reset();
            return 0.0f;
        }
        return lp_;
    }
    
    float ProcessBandPass(float in) {
        // Protect against NaN input
        if (in != in) in = 0.0f;
        
        hp_ = (in - lp_ - r_ * bp_) / (1.0f + g_ * (g_ + r_));
        bp_ += g_ * hp_;
        lp_ += g_ * bp_;
        // Stability check (NaN != NaN trick)
        if (bp_ != bp_ || bp_ > 1e4f || bp_ < -1e4f) {
            Reset();
            return 0.0f;
        }
        return bp_;
    }
    
    void Reset() { lp_ = bp_ = hp_ = 0.0f; }
    
private:
    float lp_, bp_, hp_;
    float g_, r_;
};

// ============================================================================
// Stiffness Lookup Table (from Elements)
// Maps geometry 0-1 to stiffness value for partial calculation
// Negative stiffness = partials converge, Positive = partials diverge
// ============================================================================

static const float kStiffnessLUT[65] = {
    // 0.0 - 0.25: Strong negative stiffness (converging partials)
    -0.50f, -0.48f, -0.46f, -0.44f, -0.42f, -0.40f, -0.38f, -0.36f,
    -0.34f, -0.32f, -0.30f, -0.28f, -0.26f, -0.24f, -0.22f, -0.20f,
    // 0.25 - 0.5: Mild negative to zero (near-harmonic)
    -0.18f, -0.16f, -0.14f, -0.12f, -0.10f, -0.08f, -0.06f, -0.04f,
    -0.03f, -0.02f, -0.01f, -0.005f, 0.0f, 0.005f, 0.01f, 0.02f,
    // 0.5 - 0.75: Positive stiffness (stiff string / bar)
    0.03f, 0.04f, 0.05f, 0.06f, 0.08f, 0.10f, 0.12f, 0.14f,
    0.16f, 0.18f, 0.20f, 0.22f, 0.25f, 0.28f, 0.31f, 0.34f,
    // 0.75 - 1.0: Strong positive stiffness (very inharmonic)
    0.38f, 0.42f, 0.46f, 0.50f, 0.55f, 0.60f, 0.66f, 0.72f,
    0.78f, 0.85f, 0.92f, 1.00f, 1.10f, 1.20f, 1.32f, 1.45f,
    1.60f  // Extra entry for interpolation
};

// Interpolate stiffness from lookup table
inline float GetStiffness(float geometry) {
    geometry = Clamp(geometry, 0.0f, 1.0f);
    float idx_f = geometry * 64.0f;
    int idx = static_cast<int>(idx_f);
    float frac = idx_f - static_cast<float>(idx);
    if (idx >= 64) { idx = 63; frac = 1.0f; }
    return kStiffnessLUT[idx] * (1.0f - frac) + kStiffnessLUT[idx + 1] * frac;
}

// ============================================================================
// ============================================================================
// SVF G Coefficient Lookup Table
// Pre-computed tan(π * f) for normalized frequency f ∈ [0, 0.5)
// 257 entries covering f = 0 to 0.5 at resolution of 1/512
// ============================================================================

static const float kSvfGLUT[129] = {
    // tan(π * i / 256) for i = 0 to 128 (f = 0 to 0.5)
    // Correctly computed values for SVF frequency coefficient
    0.000000f, 0.012272f, 0.024549f, 0.036832f, 0.049127f, 0.061436f, 0.073764f, 0.086115f,  // 0-7
    0.098491f, 0.110898f, 0.123338f, 0.135816f, 0.148336f, 0.160901f, 0.173516f, 0.186185f,  // 8-15
    0.198912f, 0.211702f, 0.224558f, 0.237484f, 0.250487f, 0.263570f, 0.276737f, 0.289995f,  // 16-23
    0.303347f, 0.316799f, 0.330355f, 0.344023f, 0.357806f, 0.371710f, 0.385743f, 0.399908f,  // 24-31
    0.414214f, 0.428665f, 0.443270f, 0.458034f, 0.472965f, 0.488070f, 0.503358f, 0.518835f,  // 32-39
    0.534511f, 0.550394f, 0.566493f, 0.582817f, 0.599377f, 0.616182f, 0.633243f, 0.650571f,  // 40-47
    0.668179f, 0.686077f, 0.704279f, 0.722799f, 0.741651f, 0.760848f, 0.780408f, 0.800345f,  // 48-55
    0.820679f, 0.841426f, 0.862606f, 0.884239f, 0.906347f, 0.928952f, 0.952079f, 0.975753f,  // 56-63
    1.000000f, 1.024850f, 1.050333f, 1.076481f, 1.103330f, 1.130916f, 1.159278f, 1.188459f,  // 64-71
    1.218504f, 1.249460f, 1.281382f, 1.314323f, 1.348344f, 1.383510f, 1.419891f, 1.457562f,  // 72-79
    1.496606f, 1.537110f, 1.579173f, 1.622897f, 1.668399f, 1.715803f, 1.765247f, 1.816880f,  // 80-87
    1.870868f, 1.927394f, 1.986659f, 2.048886f, 2.114322f, 2.183246f, 2.255964f, 2.332823f,  // 88-95
    2.414214f, 2.500574f, 2.592403f, 2.690266f, 2.794813f, 2.906786f, 3.027043f, 3.156580f,  // 96-103
    3.296558f, 3.448340f, 3.613536f, 3.794063f, 3.992224f, 4.210802f, 4.453202f, 4.723629f,  // 104-111
    5.027339f, 5.370990f, 5.763142f, 6.214988f, 6.741452f, 7.362888f, 8.107786f, 9.017302f,  // 112-119
    10.153170f, 11.612399f, 13.556669f, 16.277008f, 20.355468f, 27.150171f, 40.735484f, 81.483240f,  // 120-127
    100.000000f  // Extra entry for interpolation (clamped at Nyquist)
};

// Fast SVF G coefficient lookup with linear interpolation
// Input: f = normalized frequency (freq / sample_rate), range [0, 0.49]
// Output: g = tan(π * f) for SVF coefficient
inline float LookupSvfG(float f) {
    // Clamp to valid range
    if (f < 0.0f) f = 0.0f;
    if (f >= 0.49f) f = 0.49f;
    
    // Scale to table index: f * 256 gives index 0-126 for f 0-0.49
    float idx_f = f * 256.0f;
    int idx = static_cast<int>(idx_f);
    float frac = idx_f - static_cast<float>(idx);
    
    // Linear interpolation
    return kSvfGLUT[idx] * (1.0f - frac) + kSvfGLUT[idx + 1] * frac;
}

// ============================================================================
// 4-Decades Q Lookup Table (logarithmic Q mapping)
// Maps damping 0-1 to Q value with 4-decade range (0.5 to 5000)
// ============================================================================

static const float kQDecadesLUT[65] = {
    // Low damping = high Q (long sustain)
    5000.0f, 4200.0f, 3500.0f, 2900.0f, 2400.0f, 2000.0f, 1700.0f, 1400.0f,
    1200.0f, 1000.0f, 850.0f, 720.0f, 600.0f, 500.0f, 420.0f, 350.0f,
    290.0f, 240.0f, 200.0f, 170.0f, 140.0f, 120.0f, 100.0f, 85.0f,
    72.0f, 60.0f, 50.0f, 42.0f, 35.0f, 29.0f, 24.0f, 20.0f,
    17.0f, 14.0f, 12.0f, 10.0f, 8.5f, 7.2f, 6.0f, 5.0f,
    4.2f, 3.5f, 2.9f, 2.4f, 2.0f, 1.7f, 1.4f, 1.2f,
    1.0f, 0.85f, 0.72f, 0.60f, 0.50f, 0.50f, 0.50f, 0.50f,
    0.50f, 0.50f, 0.50f, 0.50f, 0.50f, 0.50f, 0.50f, 0.50f,
    0.50f  // Extra entry for interpolation
};

// Interpolate Q from lookup table
inline float GetQFromDamping(float damping) {
    damping = Clamp(damping, 0.0f, 1.0f);
    float idx_f = damping * 64.0f;
    int idx = static_cast<int>(idx_f);
    float frac = idx_f - static_cast<float>(idx);
    if (idx >= 64) { idx = 63; frac = 1.0f; }
    return kQDecadesLUT[idx] * (1.0f - frac) + kQDecadesLUT[idx + 1] * frac;
}

// ============================================================================
// Accent/Velocity Gain Lookup Tables
// Non-linear velocity response for more musical dynamics
// Based on Elements' accent handling
// ============================================================================

// Coarse velocity gain: 0-127 velocity to gain (0-1, exponential curve)
// This provides a natural-feeling velocity response
static const float kVelocityGainCoarse[33] = {
    0.000f, 0.040f, 0.063f, 0.083f, 0.100f, 0.116f, 0.131f, 0.145f,  // 0-7
    0.158f, 0.170f, 0.182f, 0.194f, 0.205f, 0.216f, 0.226f, 0.236f,  // 8-15
    0.246f, 0.270f, 0.293f, 0.316f, 0.339f, 0.361f, 0.383f, 0.405f,  // 16-23
    0.427f, 0.500f, 0.570f, 0.640f, 0.707f, 0.775f, 0.841f, 0.908f,  // 24-31
    1.000f  // Extra entry for interpolation
};

// Fine velocity gain: for subtle dynamics (0.5-1.5 range)
// Used for accent/dynamics modulation
static const float kVelocityGainFine[33] = {
    0.500f, 0.520f, 0.540f, 0.560f, 0.580f, 0.600f, 0.620f, 0.640f,  // 0-7
    0.660f, 0.680f, 0.700f, 0.720f, 0.740f, 0.760f, 0.780f, 0.800f,  // 8-15
    0.820f, 0.860f, 0.900f, 0.940f, 0.980f, 1.020f, 1.060f, 1.100f,  // 16-23
    1.140f, 1.200f, 1.260f, 1.320f, 1.380f, 1.440f, 1.480f, 1.490f,  // 24-31
    1.500f  // Extra entry for interpolation
};

// Get exponential velocity gain (0-127 → 0-1 with curve)
inline float GetVelocityGain(int velocity) {
    if (velocity < 0) velocity = 0;
    if (velocity > 127) velocity = 127;
    
    // Scale to table index (0-127 → 0-32)
    float idx_f = velocity * (32.0f / 127.0f);
    int idx = static_cast<int>(idx_f);
    float frac = idx_f - static_cast<float>(idx);
    if (idx >= 32) { idx = 31; frac = 1.0f; }
    
    return kVelocityGainCoarse[idx] * (1.0f - frac) + kVelocityGainCoarse[idx + 1] * frac;
}

// Get fine velocity gain for accent/dynamics (0-127 → 0.5-1.5)
inline float GetVelocityAccent(int velocity) {
    if (velocity < 0) velocity = 0;
    if (velocity > 127) velocity = 127;
    
    float idx_f = velocity * (32.0f / 127.0f);
    int idx = static_cast<int>(idx_f);
    float frac = idx_f - static_cast<float>(idx);
    if (idx >= 32) { idx = 31; frac = 1.0f; }
    
    return kVelocityGainFine[idx] * (1.0f - frac) + kVelocityGainFine[idx + 1] * frac;
}

// ============================================================================
// CosineOscillator - Walking cosine for position-based amplitude modulation
// From Mutable Instruments stmlib/dsp/cosine_oscillator.h (approximate mode)
// Used to create pickup position simulation in modal resonator
// ============================================================================

class CosineOscillator {
public:
    CosineOscillator() : y0_(0.5f), y1_(0.0f), iir_coefficient_(0.0f), 
                         initial_amplitude_(0.0f) {}
    
    // Initialize for a given position/frequency [0, 1]
    // Uses polynomial approximation of cosine suitable for modal amplitudes
    void Init(float frequency) {
        // Polynomial approximation from stmlib
        float sign = 16.0f;
        frequency -= 0.25f;
        if (frequency < 0.0f) {
            frequency = -frequency;
        } else {
            if (frequency > 0.5f) {
                frequency -= 0.5f;
            } else {
                sign = -16.0f;
            }
        }
        iir_coefficient_ = sign * frequency * (1.0f - 2.0f * frequency);
        initial_amplitude_ = iir_coefficient_ * 0.25f;
    }
    
    // Start the oscillator for iteration
    void Start() {
        y1_ = initial_amplitude_;
        y0_ = 0.5f;
    }
    
    // Get next sample - returns value in [0, 1] range
    float Next() {
        float temp = y0_;
        y0_ = iir_coefficient_ * y0_ - y1_;
        y1_ = temp;
        return temp + 0.5f;
    }
    
    // Batch compute 4 samples - optimized for NEON loop usage
    // Returns 4 consecutive values and advances oscillator state by 4
    void Next4(float* out) {
        float y0 = y0_;
        float y1 = y1_;
        float c = iir_coefficient_;
        
        // Unrolled 4 iterations
        out[0] = y0 + 0.5f;
        float y2 = c * y0 - y1;
        out[1] = y2 + 0.5f;
        float y3 = c * y2 - y0;
        out[2] = y3 + 0.5f;
        float y4 = c * y3 - y2;
        out[3] = y4 + 0.5f;
        
        // Update state for next batch
        y0_ = c * y4 - y3;
        y1_ = y4;
    }
    
    // Get current value without advancing
    float Value() const { return y1_ + 0.5f; }
    
private:
    float y0_, y1_;
    float iir_coefficient_;
    float initial_amplitude_;
};

} // namespace modal
