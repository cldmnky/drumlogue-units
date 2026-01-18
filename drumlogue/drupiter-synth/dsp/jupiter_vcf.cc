/**
 * @file jupiter_vcf.cc
 * @brief Voltage Controlled Filter implementation
 *
 * Jupiter-8 style IR3109 4-pole OTA cascade filter.
 * Based on Krajeski/Stilson improved ladder with TPT topology.
 * 
 * Key characteristics:
 * - 4 cascaded one-pole lowpass filters
 * - Global resonance feedback (pre-saturation)
 * - Q compensation to maintain passband gain
 * - Cutoff-resonance decoupling via polynomial correction
 */

#include "jupiter_vcf.h"
#include <algorithm>
#include <cmath>

#ifdef __ARM_NEON
#include <arm_neon.h>
#endif

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

// Krajeski "compromise" pole coefficients (z = -0.3)
static constexpr float kA1 = 0.23076923f;  // 0.3 / 1.3
static constexpr float kA2 = 0.76923077f;  // 1.0 / 1.3

// Tanh/sigmoid approximation coefficients
static constexpr float kTanhDiv = 0.1111111f;  // 1/9
static constexpr float kSigmoidDiv = 0.1666667f;  // 1/6

// Denormal threshold
static constexpr float kDenormalThreshold = 1e-15f;

// Static lookup table initialization
namespace dsp {
float JupiterVCF::s_tanh_table[JupiterVCF::kTanhTableSize] = {};
float JupiterVCF::s_kbd_tracking_table[JupiterVCF::kKbdTrackingTableSize] = {};
bool JupiterVCF::s_tables_initialized = false;
}

// Fast Math Approximations
// ============================================================================

// Flush denormal numbers to zero (prevents CPU spikes on ARM)
inline float FlushDenormal(float x) {
    if (fabsf(x) < kDenormalThreshold) return 0.0f;
    return x;
}

// Fast tanh approximation using rational function
// Accurate for full range, proper clamping
// tanh(x) ≈ x * (27 + x²) / (27 + 9*x²)
inline float FastTanh(float x) {
    if (x > 4.0f) return 1.0f;
    if (x < -4.0f) return -1.0f;
    float x2 = x * x;
    return x * (27.0f + x2) / (27.0f + 9.0f * x2);
}

#ifdef __ARM_NEON
// NEON-optimized tanh using vectorized polynomial evaluation
// Processes single float but uses NEON for internal calculations
inline float FastTanh_NEON(float x) {
    if (x > 4.0f) return 1.0f;
    if (x < -4.0f) return -1.0f;
    
    // Load single value into NEON register
    float32x4_t vx = vdupq_n_f32(x);
    float32x4_t vx2 = vmulq_f32(vx, vx);
    
    // Compute numerator: x * (27 + x²)
    float32x4_t num = vmulq_f32(vx, vaddq_f32(vdupq_n_f32(27.0f), vx2));
    
    // Compute denominator: 27 + 9*x²
    float32x4_t den = vaddq_f32(vdupq_n_f32(27.0f), 
                               vmulq_f32(vdupq_n_f32(9.0f), vx2));
    
    // Use reciprocal approximation for division (faster than vdivq_f32)
    float32x4_t recip_den = vrecpeq_f32(den);
    recip_den = vmulq_f32(recip_den, vrecpsq_f32(recip_den, den));  // Newton-Raphson iteration
    
    // Compute result
    float32x4_t result = vmulq_f32(num, recip_den);
    
    // Extract single float result
    return vgetq_lane_f32(result, 0);
}
#endif

// Fast pow(2, x) approximation for keyboard tracking
// Uses bit manipulation for integer part + polynomial for fractional
inline float FastPow2(float x) {
    // Clamp to reasonable range
    if (x < -126.0f) return 0.0f;
    if (x > 126.0f) return 3.4028235e38f;  // Near FLT_MAX
    
    // Split into integer and fractional parts
    int32_t xi = static_cast<int32_t>(x);
    float xf = x - xi;
    
    // Polynomial approximation for 2^xf where xf in [0,1]
    // 2^xf ≈ 1 + xf*(0.6931 + xf*0.2402)
    float scale = 1.0f + xf * (0.6931471805599453f + xf * 0.24022650695910071f);
    
    // Combine with integer part using bit manipulation
    union { float f; int32_t i; } u;
    u.i = (xi + 127) << 23;
    
    return u.f * scale;
}

// Fast tan(pi*x) approximation for filter coefficients
// Optimized for x in [0, 0.49] (normalized frequencies below Nyquist)
// Uses 5th order polynomial (from Mutable Instruments stmlib)
inline float FastTanPi(float x) {
    // Clamp to valid range
    if (x < 0.0001f) x = 0.0001f;
    if (x > 0.49f) x = 0.49f;
    
    const float a = 3.260e-01f;  // Optimized for audio range
    const float b = 1.823e-01f;
    
    // tan(pi*x) ≈ pi*x * (1 + a*(pi*x)² + b*(pi*x)⁴)
    const float pi_x = M_PI * x;
    const float pi_x2 = pi_x * pi_x;
    
    return pi_x * (1.0f + pi_x2 * (a + b * pi_x2));
}

// Fast sin approximation for SVF coefficients
inline float FastSin(float x) {
    // Normalize to [-pi, pi]
    while (x > M_PI) x -= 2.0f * M_PI;
    while (x < -M_PI) x += 2.0f * M_PI;
    
    // Taylor series: sin(x) ≈ x - x³/6 + x⁵/120
    const float x2 = x * x;
    const float x3 = x2 * x;
    const float x5 = x3 * x2;
    
    return x - x3 / 6.0f + x5 / 120.0f;
}

namespace dsp {

JupiterVCF::JupiterVCF()
    : tables_initialized_(false)
    , sample_rate_(48000.0f)
    , cutoff_hz_(1000.0f)
    , base_cutoff_hz_(1000.0f)
    , resonance_(0.0f)
    , mode_(MODE_LP12)
    , kbd_tracking_(0.5f)  // 50% keyboard tracking (Jupiter-8 typical)
    , coefficients_dirty_(true)
    , ota_s1_(0.0f)
    , ota_s2_(0.0f)
    , ota_s3_(0.0f)
    , ota_s4_(0.0f)
    , ota_d1_(0.0f)
    , ota_d2_(0.0f)
    , ota_d3_(0.0f)
    , ota_d4_(0.0f)
    , ota_y2_(0.0f)
    , ota_y4_(0.0f)
    , ota_g_(0.0f)
    , ota_res_k_(0.0f)
    , ota_gain_comp_(0.0f)
    , ota_output_gain_(1.0f)
    , hp_lp_state_(0.0f)
    , hp_a_(0.0f)
    , lp_(0.0f)
    , bp_(0.0f)
    , hp_(0.0f)
    , f_(0.0f)
    , q_(1.0f)
{
}

JupiterVCF::~JupiterVCF() {
}

void JupiterVCF::Init(float sample_rate) {
    InitializeLookupTables();  // Initialize once per instance
    sample_rate_ = sample_rate;
    Reset();
    UpdateCoefficients();
}

void JupiterVCF::SetCutoff(float freq_hz) {
    base_cutoff_hz_ = ClampCutoff(freq_hz);
    cutoff_hz_ = base_cutoff_hz_;
    coefficients_dirty_ = true;
}

void JupiterVCF::SetCutoffModulated(float freq_hz) {
    float new_cutoff = ClampCutoff(freq_hz);
    // Phase 6: Parameter change detection - only mark dirty if cutoff actually changed
    if (fabsf(new_cutoff - prev_cutoff_hz_) > 1e-6f) {
        cutoff_hz_ = new_cutoff;
        prev_cutoff_hz_ = new_cutoff;
        coefficients_dirty_ = true;
    }
}

void JupiterVCF::SetResonance(float resonance) {
    float new_resonance = clampf(resonance, 0.0f, 1.0f);
    // Phase 6: Parameter change detection - only mark dirty if resonance actually changed
    if (fabsf(new_resonance - prev_resonance_) > 1e-6f) {
        resonance_ = new_resonance;
        prev_resonance_ = new_resonance;
        coefficients_dirty_ = true;
    }
}

void JupiterVCF::SetMode(Mode mode) {
    mode_ = mode;
}

void JupiterVCF::SetKeyboardTracking(float amount) {
    kbd_tracking_ = clampf(amount, 0.0f, 1.0f);
}

void JupiterVCF::ApplyKeyboardTracking(uint8_t note) {
    if (kbd_tracking_ > 0.0f) {
        // Linear interpolation between no tracking (1.0) and full tracking
        // This maintains the original behavior while using the lookup table
        float full_tracking_ratio = s_kbd_tracking_table[note];
        float freq_mult = 1.0f + (full_tracking_ratio - 1.0f) * kbd_tracking_;
        cutoff_hz_ = ClampCutoff(base_cutoff_hz_ * freq_mult);
        coefficients_dirty_ = true;
    }
}

float JupiterVCF::Process(float input) {
    // Update coefficients if dirty
    if (coefficients_dirty_) {
        UpdateCoefficients();
        coefficients_dirty_ = false;
    }
    
    // JP-8 style LP modes: 4-pole OTA cascade (IR3109 family).
    // Using Krajeski-style improved ladder filter with:
    // - Compromise poles at z = -0.3 for cutoff-resonance decoupling
    // - Tanh saturation for analog character
    // - Gain compensation to maintain passband level
    
    if (mode_ == MODE_LP12 || mode_ == MODE_LP24) {
        float output = 0.0f;
        
        for (int i = 0; i < kOversamplingFactor; ++i) {
            // Resonance feedback from 4th stage with gain compensation
            // gComp helps maintain passband level at high resonance
            float feedback = ota_res_k_ * (ota_s4_ - ota_gain_comp_ * input);
            
            // Input stage with soft saturation (transistor-like nonlinearity)
            float u = input - feedback;
            
            // Use NEON-optimized tanh approximation for saturation
            // This creates the characteristic "warmth" of analog filters
#ifdef __ARM_NEON
            u = FastTanh_NEON(u);
#else
            u = FastTanh(u);
#endif
            
            // 4 cascaded one-pole sections using Krajeski-style improved coefficients
            // Each pole: y[n] = g * (0.3/1.3 * x[n] + 1/1.3 * x[n-1] - y[n-1]) + y[n-1]
            // The 0.3/1.3 and 1/1.3 terms create the "compromise" zero at z = -0.3
            // that decouples cutoff from resonance
            
            const float g = ota_g_;
            
            // Stage 1
            float y1_new = g * (kA1 * u + kA2 * ota_d1_ - ota_s1_) + ota_s1_;
            ota_d1_ = u;
            ota_s1_ = FlushDenormal(y1_new);
            
            // Stage 2
            float y2_new = g * (kA1 * y1_new + kA2 * ota_d2_ - ota_s2_) + ota_s2_;
            ota_d2_ = y1_new;
            ota_s2_ = FlushDenormal(y2_new);
            ota_y2_ = ota_s2_;  // LP12 output tap
            
            // Stage 3
            float y3_new = g * (kA1 * y2_new + kA2 * ota_d3_ - ota_s3_) + ota_s3_;
            ota_d3_ = y2_new;
            ota_s3_ = FlushDenormal(y3_new);
            
            // Stage 4
            float y4_new = g * (kA1 * y3_new + kA2 * ota_d4_ - ota_s4_) + ota_s4_;
            ota_d4_ = y3_new;
            ota_s4_ = FlushDenormal(y4_new);
            ota_y4_ = ota_s4_;  // LP24 output tap
            
            // Soft clipping on output to prevent blow-ups
            float y = ota_y4_;
            float y3 = y * y * y;
            output = y - y3 * kSigmoidDiv;  // Band-limited sigmoid: y - y³/6
        }

        // Select 12dB or 24dB output with gain compensation
        float result = (mode_ == MODE_LP12) ? ota_y2_ : output;
        // Phase 6: Explicit denormal flushing at output
        return FlushDenormal(result * ota_output_gain_);
    }

    // HP mode: non-resonant 6dB high-pass (simple DC blocker style)
    if (mode_ == MODE_HP12) {
        for (int i = 0; i < kOversamplingFactor; ++i) {
            float v = (input - hp_lp_state_) * hp_a_;
            float lp = v + hp_lp_state_;
            hp_lp_state_ = FlushDenormal(lp + v);
            hp_ = input - lp;
        }
        // Phase 6: Explicit denormal flushing at output
        return FlushDenormal(hp_);
    }

    // BP mode: Chamberlin SVF bandpass
    for (int i = 0; i < kOversamplingFactor; i++) {
        hp_ = input - lp_ - q_ * bp_;
        bp_ = FlushDenormal(bp_ + f_ * hp_);
        lp_ = FlushDenormal(lp_ + f_ * bp_);

        // Clamp to prevent blowup
        if (bp_ > 10.0f) bp_ = 10.0f;
        else if (bp_ < -10.0f) bp_ = -10.0f;
        if (lp_ > 10.0f) lp_ = 10.0f;
        else if (lp_ < -10.0f) lp_ = -10.0f;
    }

    // Phase 6: Explicit denormal flushing at output
    return FlushDenormal(bp_ * (1.0f + resonance_ * 0.5f));
}

void JupiterVCF::Reset() {
    ota_s1_ = 0.0f;
    ota_s2_ = 0.0f;
    ota_s3_ = 0.0f;
    ota_s4_ = 0.0f;
    ota_y2_ = 0.0f;
    ota_y4_ = 0.0f;
    ota_d1_ = 0.0f;
    ota_d2_ = 0.0f;
    ota_d3_ = 0.0f;
    ota_d4_ = 0.0f;
    hp_lp_state_ = 0.0f;

    lp_ = 0.0f;
    bp_ = 0.0f;
    hp_ = 0.0f;
    
    // Phase 6: Initialize coefficient caching fields
    prev_cutoff_hz_ = cutoff_hz_;
    prev_resonance_ = resonance_;
}

void JupiterVCF::UpdateCoefficients() {
    // Jupiter-8/Krajeski style filter coefficient calculation
    // 
    // Key improvements:
    // 1. Better frequency mapping that doesn't over-attenuate at low cutoffs
    // 2. Cutoff-resonance decoupling via polynomial corrections
    // 3. Proper gain compensation to maintain passband level
    
    // Calculate for oversampled rate (48kHz * 4 = 192kHz)
    const float oversampled_rate = sample_rate_ * kOversamplingFactor;

    // Clamp cutoff frequency to reasonable range
    // Minimum 80Hz to prevent over-resonant bass rumble
    // Maximum 0.45 * Nyquist for stability
    float fc = cutoff_hz_;
    const float fc_min = 80.0f;
    const float fc_max = 0.45f * oversampled_rate;
    if (fc < fc_min) fc = fc_min;
    if (fc > fc_max) fc = fc_max;

    // Normalized cutoff: fc / sample_rate
    const float wc = 2.0f * static_cast<float>(M_PI) * fc / oversampled_rate;
    
    // Krajeski-style g coefficient with polynomial correction
    // This provides better tuning accuracy across the frequency range
    const float wc2 = wc * wc;
    const float wc3 = wc2 * wc;
    const float wc4 = wc3 * wc;
    
#ifdef __ARM_NEON
    // NEON-optimized polynomial evaluation for g coefficient
    // ota_g_ = 0.9892f * wc - 0.4342f * wc2 + 0.1381f * wc3 - 0.0202f * wc4
    float32x4_t wc_powers = {wc, wc2, wc3, wc4};
    float32x4_t g_coeffs = {0.9892f, -0.4342f, 0.1381f, -0.0202f};
    float32x4_t g_result = vmulq_f32(wc_powers, g_coeffs);
    // Horizontal sum using pairwise addition (compatible with older NEON)
    float32x2_t g_sum2 = vpadd_f32(vget_low_f32(g_result), vget_high_f32(g_result));
    ota_g_ = vget_lane_f32(vpadd_f32(g_sum2, g_sum2), 0);
#else
    ota_g_ = 0.9892f * wc - 0.4342f * wc2 + 0.1381f * wc3 - 0.0202f * wc4;
#endif
    
    // Clamp g to prevent instability
    if (ota_g_ < 0.0f) ota_g_ = 0.0f;
    if (ota_g_ > 1.0f) ota_g_ = 1.0f;

    // Resonance mapping
    // res_k = 4 * r for maximum self-oscillation at r = 1.0
    // Use polynomial correction for resonance to match cutoff changes (decoupling)
    const float r = clampf(resonance_, 0.0f, 1.0f);
    
    // Krajeski resonance correction: compensate resonance for cutoff changes
    // gRes = r * (1.0029 + 0.0526*wc - 0.926*wc² + 0.0218*wc³)
#ifdef __ARM_NEON
    // NEON-optimized polynomial evaluation for resonance correction
    float32x4_t res_powers = {1.0f, wc, wc2, wc3};
    float32x4_t res_coeffs = {1.0029f, 0.0526f, -0.926f, 0.0218f};
    float32x4_t res_result = vmulq_f32(res_powers, res_coeffs);
    // Horizontal sum using pairwise addition
    float32x2_t res_sum2 = vpadd_f32(vget_low_f32(res_result), vget_high_f32(res_result));
    const float res_correction = vget_lane_f32(vpadd_f32(res_sum2, res_sum2), 0);
#else
    const float res_correction = 1.0029f + 0.0526f * wc - 0.926f * wc2 + 0.0218f * wc3;
#endif
    ota_res_k_ = 4.0f * r * res_correction;
    
    // Limit resonance to prevent self-oscillation going out of control
    if (ota_res_k_ > 3.8f) ota_res_k_ = 3.8f;

    // Gain compensation to maintain passband level
    // At high resonance, the passband can drop by up to 12dB
    // Compensate with 0.5 factor at max resonance (6dB compensation)
    ota_gain_comp_ = 0.5f * r;  // Used in feedback: (state[4] - gComp * input)
    
    // Output gain to compensate for resonance-induced passband drop
    // More aggressive compensation at higher resonance
    ota_output_gain_ = 1.0f + r * 1.5f;

    // High-pass 6dB (non-resonant): share the same base cutoff
    // Use simpler TPT coefficient with fast approximation
    const float fc_norm_hp = fc / oversampled_rate;
    const float g_hp = FastTanPi(fc_norm_hp);
    hp_a_ = g_hp / (1.0f + g_hp);

    // SVF coefficients (used for BP mode only)
    float cutoff_normalized = fc / oversampled_rate;
    if (cutoff_normalized > 0.45f) cutoff_normalized = 0.45f;
    f_ = 2.0f * FastSin(static_cast<float>(M_PI) * cutoff_normalized);
    q_ = 2.0f - r * 1.95f;
    if (q_ < 0.05f) q_ = 0.05f;
}

float JupiterVCF::ClampCutoff(float freq) const {
    // Minimum 80Hz to prevent muddy resonance
    // Maximum 20kHz (will be further limited by Nyquist in UpdateCoefficients)
    return clampf(freq, 80.0f, 20000.0f);
}

void JupiterVCF::InitializeLookupTables() {
    // Initialize lookup tables only once per process (at Init time)
    if (tables_initialized_) return;
    tables_initialized_ = true;
    
    // Initialize tanh lookup table if not already done globally
    if (!s_tables_initialized) {
        // Tanh table: 512 entries covering [-4, 4]
        for (int i = 0; i < kTanhTableSize; i++) {
            float x = -4.0f + (8.0f * i) / (kTanhTableSize - 1);
            s_tanh_table[i] = tanhf(x);
        }
        
        // Keyboard tracking table: 128 entries for MIDI notes 0-127
        // Relative to MIDI note 60 (C4)
        for (int note = 0; note < kKbdTrackingTableSize; note++) {
            float note_offset = (note - 60) / 12.0f;  // Octaves from C4
            s_kbd_tracking_table[note] = powf(2.0f, note_offset);
        }
        
        s_tables_initialized = true;
    }
}

float JupiterVCF::TanhLookup(float x) const {
    // Lookup-based tanh with linear interpolation
    // Maps [-4, 4] to table indices
    
    if (x >= 4.0f) return 1.0f;
    if (x <= -4.0f) return -1.0f;
    
    // Map x from [-4, 4] to index [0, 512]
    float idx_f = ((x + 4.0f) * (kTanhTableSize - 1)) * 0.125f;  // / 8.0f
    int idx = static_cast<int>(idx_f);
    float frac = idx_f - idx;
    
    // Boundary check
    if (idx < 0) idx = 0;
    if (idx >= kTanhTableSize - 1) return s_tanh_table[kTanhTableSize - 1];
    
    // Linear interpolation between table entries
    return s_tanh_table[idx] * (1.0f - frac) + s_tanh_table[idx + 1] * frac;
}

} // namespace dsp
