/*
 * Resonator - Modal and Karplus-Strong string models
 * Part of Modal Synth for Drumlogue
 * 
 * Based on Mutable Instruments Elements resonator
 * Implements:
 * - SVF (State Variable Filter) for modal modes
 * - Position interpolation (anti-zipper)
 * - Clock divider for efficient mode updates
 * - CosineOscillator for smooth amplitude modulation
 * - Dynamic stiffness-based partial calculation
 * - Bowed mode support with BowTable friction model
 */

#pragma once

#include "dsp_core.h"

namespace modal {

// Bowing model constants
constexpr size_t kMaxBowedModes = 8;       // Number of bowed modes (from Elements)
constexpr size_t kMaxDelayLineSize = 1024;  // Max delay line size for bowed modes

// ============================================================================
// SVF-based Modal Mode (like Elements)
// Zero-delay feedback state variable filter configured as bandpass
// More stable at high Q than biquad, especially near Nyquist
// ============================================================================

class Mode {
public:
    Mode() : state_1_(0.0f), state_2_(0.0f), g_(0.0f), k_(2.0f), a1_(0.0f), a2_(0.0f), a3_(0.0f) {}
    
    // Set frequency (normalized 0-0.5) and Q directly
    void SetFrequencyAndQ(float freq, float q) {
        freq = Clamp(freq, 20.0f, kSampleRate * 0.49f);
        q = Clamp(q, 0.5f, 500.0f);
        
        // Normalized frequency for coefficient calculation
        float f = freq / kSampleRate;
        
        // g = tan(π * f) - use lookup table
        g_ = LookupSvfG(f);
        
        // k = 1/Q for damping (k=2 is critically damped, k<2 is underdamped)
        k_ = 1.0f / q;
        
        // Pre-compute coefficients for efficiency
        // a1 = 1 / (1 + g*(g + k))
        // a2 = g * a1
        // a3 = g * a2
        a1_ = 1.0f / (1.0f + g_ * (g_ + k_));
        a2_ = g_ * a1_;
        a3_ = g_ * a2_;
    }
    
    // Set coefficients directly (for clock-divider optimization)
    void SetCoefficients(float g, float r) {
        g_ = g;
        k_ = r;  // r = 1/Q
        a1_ = 1.0f / (1.0f + g_ * (g_ + k_));
        a2_ = g_ * a1_;
        a3_ = g_ * a2_;
    }
    
    // Get g coefficient (for bowed mode sharing)
    float g() const { return g_; }
    
    // Process one sample - returns bandpass output
    // Using the SVF topology from "The Art of VA Filter Design" by Vadim Zavalishin
    float Process(float in) {
        // Protect against NaN input
        if (in != in) in = 0.0f;
        
        // v3 = in - ic2eq (input minus lowpass state)
        float v3 = in - state_2_;
        
        // v1 = a1*ic1eq + a2*v3 (bandpass calculation)
        float v1 = a1_ * state_1_ + a2_ * v3;
        
        // v2 = ic2eq + a2*ic1eq + a3*v3 (lowpass calculation)
        float v2 = state_2_ + a2_ * state_1_ + a3_ * v3;
        
        // Update states (trapezoidal integration)
        state_1_ = 2.0f * v1 - state_1_;
        state_2_ = 2.0f * v2 - state_2_;
        
        // Stability check - reset if state becomes unstable
        if (state_1_ != state_1_ || state_1_ > 1e6f || state_1_ < -1e6f ||
            state_2_ != state_2_ || state_2_ > 1e6f || state_2_ < -1e6f) {
            Reset();
            return 0.0f;
        }
        
        return v1;  // Bandpass for modal resonance
    }
    
    // Process with normalized bandpass output (for bowing)
    float ProcessNormalized(float in) {
        float bp = Process(in);
        // Normalize by k to maintain consistent amplitude across Q values
        return bp * k_;
    }
    
    void Reset() { state_1_ = state_2_ = 0.0f; }
    
private:
    float state_1_, state_2_;  // Filter state (ic1eq, ic2eq)
    float g_, k_;              // g = tan(pi*f), k = 1/Q
    float a1_, a2_, a3_;       // Pre-computed coefficients
};

// ============================================================================
// Bowed Mode - Bandpass filter + Delay line for banded waveguide synthesis
// Each bowed mode simulates a string resonating at a specific partial
// The delay line provides the waveguide behavior, filter shapes the resonance
// ============================================================================

class BowedMode {
public:
    BowedMode() : g_(0.1f), k_(0.01f), a1_(0.5f), a2_(0.05f), a3_(0.005f),
                  state_1_(0.0f), state_2_(0.0f) {
        delay_.Init();
    }
    
    void Init() {
        delay_.Init();
        state_1_ = state_2_ = 0.0f;
        g_ = 0.1f;
        k_ = 0.01f;
        a1_ = 1.0f / (1.0f + g_ * (g_ + k_));
        a2_ = g_ * a1_;
        a3_ = g_ * a2_;
    }
    
    // Set g coefficient from main mode and higher Q for bowing
    void SetGAndQ(float g, float q) {
        g_ = g;
        k_ = 1.0f / Clamp(q, 0.5f, 2000.0f);
        a1_ = 1.0f / (1.0f + g_ * (g_ + k_));
        a2_ = g_ * a1_;
        a3_ = g_ * a2_;
    }
    
    void SetDelay(size_t period) {
        delay_.SetDelay(period);
    }
    
    // Read from delay line (waveguide output)
    float Read() const {
        return delay_.Read();
    }
    
    // Process and write to delay line
    void Write(float in) {
        // SVF Bandpass filter using Zavalishin TPT structure
        float v3 = in - state_2_;
        float v1 = a1_ * state_1_ + a2_ * v3;  // Bandpass
        float v2 = state_2_ + a2_ * state_1_ + a3_ * v3;  // Lowpass
        
        // Update states
        state_1_ = 2.0f * v1 - state_1_;
        state_2_ = 2.0f * v2 - state_2_;
        
        // Stability check
        if (state_1_ != state_1_ || state_1_ > 1e6f || state_1_ < -1e6f ||
            state_2_ != state_2_ || state_2_ > 1e6f || state_2_ < -1e6f) {
            state_1_ = state_2_ = 0.0f;
            v1 = 0.0f;
        }
        
        // Write normalized bandpass to delay
        delay_.Write(v1 * k_);
    }
    
    void Reset() {
        delay_.Reset();
        state_1_ = state_2_ = 0.0f;
    }
    
private:
    DelayLine<kMaxDelayLineSize> delay_;
    float g_, k_;
    float a1_, a2_, a3_;
    float state_1_, state_2_;
};

// ============================================================================
// Modal Resonator with Elements-style features
// Includes bowed mode support for sustained sounds
// ============================================================================

class Resonator {
public:
    Resonator() {
        frequency_ = 220.0f / kSampleRate;  // Normalized frequency
        geometry_ = 0.25f;
        brightness_ = 0.5f;
        damping_ = 0.3f;
        position_ = 0.5f;
        previous_position_ = 0.5f;
        space_ = 0.5f;
        modulation_frequency_ = 0.5f / kSampleRate;  // 0.5 Hz default
        modulation_offset_ = 0.25f;
        lfo_phase_ = 0.0f;
        clock_divider_ = 0;
        bow_signal_ = 0.0f;
        
        // Coefficient caching state (Priority 1 optimization)
        params_dirty_ = true;
        cached_frequency_ = -1.0f;
        cached_geometry_ = -1.0f;
        cached_brightness_ = -1.0f;
        cached_damping_ = -1.0f;
        
        // Initialize all modes
        for (int i = 0; i < kNumModes; ++i) {
            modes_[i].Reset();
            cached_g_[i] = 0.1f;
            cached_r_[i] = 0.01f;
            // Initialize SoA arrays
            soa_a1_[i] = 0.5f;
            soa_a2_[i] = 0.05f;
            soa_a3_[i] = 0.005f;
            soa_state1_[i] = 0.0f;
            soa_state2_[i] = 0.0f;
        }
        
        // Initialize bowed modes
        for (size_t i = 0; i < kMaxBowedModes; ++i) {
            bowed_modes_[i].Init();
        }
        
        ComputeFilters();
    }
    
    void SetFrequency(float freq) {
        float new_freq = Clamp(freq, 20.0f, 8000.0f) / kSampleRate;
        if (new_freq != frequency_) {
            frequency_ = new_freq;
            params_dirty_ = true;
        }
    }
    
    void SetGeometry(float geometry) {
        float new_geom = Clamp(geometry, 0.0f, 1.0f);
        if (new_geom != geometry_) {
            geometry_ = new_geom;
            params_dirty_ = true;
        }
    }
    
    // Alias for compatibility
    void SetStructure(float s) { SetGeometry(s); }
    
    void SetBrightness(float brightness) {
        float new_bright = Clamp(brightness, 0.0f, 1.0f);
        if (new_bright != brightness_) {
            brightness_ = new_bright;
            params_dirty_ = true;
        }
    }
    
    void SetDamping(float damping) {
        float new_damp = Clamp(damping, 0.0f, 1.0f);
        if (new_damp != damping_) {
            damping_ = new_damp;
            params_dirty_ = true;
        }
    }
    
    void SetPosition(float position) {
        position_ = Clamp(position, 0.0f, 1.0f);
    }
    
    void SetSpace(float space) {
        space_ = Clamp(space, 0.0f, 1.0f);
    }
    
    void SetModulationFrequency(float freq) {
        modulation_frequency_ = Clamp(freq, 0.1f, 10.0f) / kSampleRate;
    }
    
    void SetModulationOffset(float offset) {
        modulation_offset_ = Clamp(offset, 0.0f, 1.0f);
    }
    
    // Backward compatibility - Update() is now handled automatically in Process()
    // This is a no-op; filters are recomputed every Process() call with clock divider
    void Update() {
        // No-op: ComputeFilters() is called automatically in Process()
    }
    
    // Force full coefficient update (for initialization)
    void ForceUpdate() {
        clock_divider_ = 0;
        for (int i = 0; i < kNumModes; ++i) {
            UpdateMode(i);
        }
    }
    
    // Process with bowing support (full Elements-style)
    // bow_strength: 0 = no bow, >0 = bow pressure/velocity
    void Process(float excitation, float bow_strength, float& out_center, float& out_side) {
        // Compute filter coefficients (with clock divider optimization)
        size_t num_modes = ComputeFilters();
        size_t num_bowed = (num_modes < kMaxBowedModes) ? num_modes : kMaxBowedModes;
        
        // Protect input
        if (excitation != excitation) excitation = 0.0f;
        excitation = Clamp(excitation, -10.0f, 10.0f) * 0.125f;  // Scale down like Elements
        
        // Position interpolation for anti-zipper (per-sample smoothing)
        float current_position = previous_position_ + 
            (position_ - previous_position_) * 0.001f;  // Smooth ~1ms
        previous_position_ = current_position;
        
        // Update LFO for stereo modulation
        lfo_phase_ += modulation_frequency_;
        if (lfo_phase_ >= 1.0f) lfo_phase_ -= 1.0f;
        
        // Triangle LFO (like Elements)
        float lfo = lfo_phase_ > 0.5f ? 1.0f - lfo_phase_ : lfo_phase_;
        lfo = lfo * 4.0f - 1.0f;  // Scale to [-1, 1]
        
        // Initialize cosine oscillators for amplitude modulation
        CosineOscillator amplitudes;
        CosineOscillator aux_amplitudes;
        amplitudes.Init(current_position);
        aux_amplitudes.Init(modulation_offset_ + lfo * 0.25f);
        
        amplitudes.Start();
        aux_amplitudes.Start();
        
        float sum_center = 0.0f;
        float sum_side = 0.0f;
        
#ifdef USE_NEON
        // NEON-optimized mode processing: 4 modes at a time
        // Using Structure-of-Arrays (SoA) layout for direct SIMD loads
        float32x4_t exc_vec = vdupq_n_f32(excitation);
        float32x4_t sum_center_vec = vdupq_n_f32(0.0f);
        float32x4_t sum_side_vec = vdupq_n_f32(0.0f);
        
        // Constants for stability checking
        const float32x4_t stability_limit = vdupq_n_f32(1e6f);
        const float32x4_t neg_stability_limit = vdupq_n_f32(-1e6f);
        const float32x4_t zero_vec = vdupq_n_f32(0.0f);
        const float32x4_t k2 = vdupq_n_f32(2.0f);
        
        // Process modes in batches of 4 using SoA layout
        size_t i = 0;
        for (; i + 4 <= num_modes; i += 4) {
            // Direct SIMD loads from contiguous SoA arrays - no scatter-gather!
            float32x4_t a1 = vld1q_f32(&soa_a1_[i]);
            float32x4_t a2 = vld1q_f32(&soa_a2_[i]);
            float32x4_t a3 = vld1q_f32(&soa_a3_[i]);
            float32x4_t state_1 = vld1q_f32(&soa_state1_[i]);
            float32x4_t state_2 = vld1q_f32(&soa_state2_[i]);
            
            // SVF processing for 4 modes
            // v3 = in - state_2
            float32x4_t v3 = vsubq_f32(exc_vec, state_2);
            
            // v1 = a1*state_1 + a2*v3 (bandpass)
            float32x4_t v1 = vmlaq_f32(vmulq_f32(a1, state_1), a2, v3);
            
            // v2 = state_2 + a2*state_1 + a3*v3 (lowpass)
            float32x4_t v2 = vaddq_f32(state_2, vmlaq_f32(vmulq_f32(a2, state_1), a3, v3));
            
            // Update states: state = 2*v - state
            state_1 = vsubq_f32(vmulq_f32(k2, v1), state_1);
            state_2 = vsubq_f32(vmulq_f32(k2, v2), state_2);
            
            // Stability check: detect NaN or values exceeding ±1e6
            // NaN check: x != x is true for NaN
            uint32x4_t nan_1 = vmvnq_u32(vceqq_f32(state_1, state_1));  // NaN mask
            uint32x4_t nan_2 = vmvnq_u32(vceqq_f32(state_2, state_2));
            uint32x4_t unstable_1 = vorrq_u32(vcgtq_f32(state_1, stability_limit),
                                              vcltq_f32(state_1, neg_stability_limit));
            uint32x4_t unstable_2 = vorrq_u32(vcgtq_f32(state_2, stability_limit),
                                              vcltq_f32(state_2, neg_stability_limit));
            uint32x4_t reset_mask = vorrq_u32(vorrq_u32(nan_1, nan_2),
                                              vorrq_u32(unstable_1, unstable_2));
            
            // Reset unstable states to zero
            state_1 = vbslq_f32(reset_mask, zero_vec, state_1);
            state_2 = vbslq_f32(reset_mask, zero_vec, state_2);
            // Zero output for unstable modes
            v1 = vbslq_f32(reset_mask, zero_vec, v1);
            
            // Direct SIMD stores to SoA arrays - no scatter!
            vst1q_f32(&soa_state1_[i], state_1);
            vst1q_f32(&soa_state2_[i], state_2);
            
            // Batch compute amplitudes using optimized Next4()
            float amp_arr[4], aux_arr[4];
            amplitudes.Next4(amp_arr);
            aux_amplitudes.Next4(aux_arr);
            float32x4_t amp_vec = vld1q_f32(amp_arr);
            float32x4_t aux_vec = vld1q_f32(aux_arr);
            
            // Accumulate weighted outputs
            sum_center_vec = vmlaq_f32(sum_center_vec, v1, amp_vec);
            sum_side_vec = vmlaq_f32(sum_side_vec, v1, aux_vec);
        }
        
        // Horizontal sum of vectors
        float32x2_t sum_c_low = vget_low_f32(sum_center_vec);
        float32x2_t sum_c_high = vget_high_f32(sum_center_vec);
        sum_c_low = vadd_f32(sum_c_low, sum_c_high);
        sum_center = vget_lane_f32(vpadd_f32(sum_c_low, sum_c_low), 0);
        
        float32x2_t sum_s_low = vget_low_f32(sum_side_vec);
        float32x2_t sum_s_high = vget_high_f32(sum_side_vec);
        sum_s_low = vadd_f32(sum_s_low, sum_s_high);
        sum_side = vget_lane_f32(vpadd_f32(sum_s_low, sum_s_low), 0);
        
        // Process remaining modes (less than 4) using scalar path
        for (; i < num_modes; ++i) {
            // Use SoA state for consistency with NEON path
            float state_1 = soa_state1_[i];
            float state_2 = soa_state2_[i];
            float a1 = soa_a1_[i];
            float a2 = soa_a2_[i];
            float a3 = soa_a3_[i];
            
            // SVF processing
            float v3 = excitation - state_2;
            float v1 = a1 * state_1 + a2 * v3;
            float v2 = state_2 + a2 * state_1 + a3 * v3;
            state_1 = 2.0f * v1 - state_1;
            state_2 = 2.0f * v2 - state_2;
            
            // Stability check
            if (state_1 != state_1 || state_1 > 1e6f || state_1 < -1e6f ||
                state_2 != state_2 || state_2 > 1e6f || state_2 < -1e6f) {
                state_1 = state_2 = 0.0f;
                v1 = 0.0f;
            }
            
            soa_state1_[i] = state_1;
            soa_state2_[i] = state_2;
            
            float amp = amplitudes.Next();
            float aux_amp = aux_amplitudes.Next();
            sum_center += v1 * amp;
            sum_side += v1 * aux_amp;
        }
#else
        // Scalar fallback: Process all active normal modes using SoA arrays
        for (size_t i = 0; i < num_modes; ++i) {
            float state_1 = soa_state1_[i];
            float state_2 = soa_state2_[i];
            float a1 = soa_a1_[i];
            float a2 = soa_a2_[i];
            float a3 = soa_a3_[i];
            
            // SVF processing
            float v3 = excitation - state_2;
            float v1 = a1 * state_1 + a2 * v3;
            float v2 = state_2 + a2 * state_1 + a3 * v3;
            state_1 = 2.0f * v1 - state_1;
            state_2 = 2.0f * v2 - state_2;
            
            // Stability check
            if (state_1 != state_1 || state_1 > 1e6f || state_1 < -1e6f ||
                state_2 != state_2 || state_2 > 1e6f || state_2 < -1e6f) {
                state_1 = state_2 = 0.0f;
                v1 = 0.0f;
            }
            
            soa_state1_[i] = state_1;
            soa_state2_[i] = state_2;
            
            float amp = amplitudes.Next();
            float aux_amp = aux_amplitudes.Next();
            sum_center += v1 * amp;
            sum_side += v1 * aux_amp;
        }
#endif
        
        // Process bowed modes if bow_strength > 0
        if (bow_strength > 0.001f) {
            // Render bowed modes with banded waveguide
            float bow_input = excitation + bow_signal_;
            float bow_signal_sum = 0.0f;
            
            // Reset amplitude oscillator for bowed modes
            amplitudes.Init(current_position);
            amplitudes.Start();
            
            for (size_t i = 0; i < num_bowed; ++i) {
                // Read delayed signal from waveguide
                float s = 0.99f * bowed_modes_[i].Read();
                bow_signal_sum += s;
                
                // Process through bandpass filter and write back
                bowed_modes_[i].Write(bow_input + s);
                
                // Add to output with position-based amplitude
                sum_center += s * amplitudes.Next() * 8.0f;
            }
            
            // Apply bow friction model to generate feedback signal
            bow_signal_ = BowTable(bow_signal_sum, bow_strength);
        } else {
            // Decay bow signal when not bowing
            bow_signal_ *= 0.99f;
        }
        
        // Output with stereo spread
        // Use moderate gain to leave headroom for filter and envelope
        out_center = sum_center * 4.0f;
        out_side = (sum_side - sum_center) * 4.0f * space_;
        
        // Soft clamp to avoid harsh clipping
#ifdef USE_NEON
        // Use NEON FastTanh for both channels with clamping for |x| > 4
        float32x2_t input = {out_center * 0.5f, out_side * 0.5f};
        const float32x2_t k27_2 = vdup_n_f32(27.0f);
        const float32x2_t k9_2 = vdup_n_f32(9.0f);
        const float32x2_t k4 = vdup_n_f32(4.0f);
        const float32x2_t kOne = vdup_n_f32(1.0f);
        const float32x2_t kNegOne = vdup_n_f32(-1.0f);
        
        float32x2_t x2 = vmul_f32(input, input);
        float32x2_t num = vmul_f32(input, vadd_f32(k27_2, x2));
        float32x2_t denom = vmla_f32(k27_2, k9_2, x2);
        // Use reciprocal estimate + Newton-Raphson
        float32x2_t recip = vrecpe_f32(denom);
        recip = vmul_f32(vrecps_f32(denom, recip), recip);
        float32x2_t result = vmul_f32(num, recip);
        
        // Clamp to ±1 for |x| > 4, matching FastTanh4 behavior
        uint32x2_t gt4 = vcgt_f32(input, k4);
        uint32x2_t ltneg4 = vclt_f32(input, vneg_f32(k4));
        result = vbsl_f32(gt4, kOne, result);
        result = vbsl_f32(ltneg4, kNegOne, result);
        
        result = vmul_f32(result, vdup_n_f32(2.0f));  // Scale by 2
        out_center = vget_lane_f32(result, 0);
        out_side = vget_lane_f32(result, 1);
#else
        out_center = FastTanh(out_center * 0.5f) * 2.0f;
        out_side = FastTanh(out_side * 0.5f) * 2.0f;
#endif
    }
    
    // Process with stereo output (Elements-style, no bowing)
    void Process(float excitation, float& out_center, float& out_side) {
        Process(excitation, 0.0f, out_center, out_side);
    }
    
    // Legacy mono process
    float Process(float excitation) {
        float center, side;
        Process(excitation, center, side);
        return center;
    }
    
    void Reset() {
        for (int i = 0; i < kNumModes; ++i) {
            modes_[i].Reset();
            soa_state1_[i] = 0.0f;
            soa_state2_[i] = 0.0f;
        }
        for (size_t i = 0; i < kMaxBowedModes; ++i) {
            bowed_modes_[i].Reset();
        }
        lfo_phase_ = 0.0f;
        previous_position_ = position_;
        bow_signal_ = 0.0f;
    }
    
private:
    // Update a single mode's coefficients
    void UpdateMode(int i) {
        // Get stiffness from geometry
        float stiffness = GetStiffness(geometry_);
        
        // Calculate partial frequency using cumulative stiffness (like Elements)
        // This creates the inharmonic series characteristic of physical objects
        float harmonic = frequency_ * (float)(i + 1);
        float stretch_factor = 1.0f;
        
        // Accumulate stiffness effect
        for (int j = 0; j < i; ++j) {
            stretch_factor += stiffness;
            if (stiffness < 0.0f) {
                stiffness *= 0.93f;  // Prevent fold-back for negative stiffness
            } else {
                stiffness *= 0.98f;  // Slight decay for positive stiffness
            }
        }
        
        float partial_frequency = harmonic * stretch_factor;
        
        // Clamp to Nyquist
        if (partial_frequency >= 0.49f) {
            partial_frequency = 0.49f;
        }
        
        // Q calculation (frequency-dependent, like Elements)
        // Higher partials naturally have higher Q to compensate for losses
        // Elements uses: q = 500.0f * lut_4_decades[damping * 0.8]
        float base_q = 500.0f * GetQFromDamping(damping_);
        
        // Brightness attenuation at low geometry (prevents clipping)
        float brightness_attenuation = 1.0f - geometry_;
        brightness_attenuation *= brightness_attenuation;  // ^2
        brightness_attenuation *= brightness_attenuation;  // ^4
        brightness_attenuation *= brightness_attenuation;  // ^8
        float brightness = brightness_ * (1.0f - 0.2f * brightness_attenuation);
        
        // Q loss for higher modes (brightness control)
        float q_loss = brightness * (2.0f - brightness) * 0.85f + 0.15f;
        float q_loss_damping_rate = geometry_ * (2.0f - geometry_) * 0.1f;
        
        // Apply Q with frequency dependency
        float mode_q = 1.0f + partial_frequency * base_q;
        
        // Apply progressive Q loss for higher modes
        for (int j = 0; j < i; ++j) {
            mode_q *= q_loss;
            q_loss += q_loss_damping_rate * (1.0f - q_loss);
        }
        
        // Calculate and cache coefficients
        float f = partial_frequency;
        cached_g_[i] = LookupSvfG(f);
        cached_r_[i] = 1.0f / Clamp(mode_q, 0.5f, 500.0f);
        
        modes_[i].SetCoefficients(cached_g_[i], cached_r_[i]);
    }
    
    // Compute filters with coefficient caching and aggressive clock divider
    // Returns number of active modes
    // Priority 1 optimizations:
    //   A. Skip full recalc if params unchanged (coefficient caching)
    //   B. Aggressive clock divider for higher modes
    size_t ComputeFilters() {
        ++clock_divider_;
        
        // Priority 1A: Coefficient caching - skip entirely if params unchanged
        // and we've done at least one full update cycle
        if (!params_dirty_ && clock_divider_ > static_cast<size_t>(kNumModes)) {
            // Still need to return cached_num_modes_
            return cached_num_modes_ > 0 ? cached_num_modes_ : 1;
        }
        
        // Check if we need full recalculation
        bool full_update = params_dirty_;
        if (params_dirty_) {
            // Cache current params for dirty tracking
            cached_frequency_ = frequency_;
            cached_geometry_ = geometry_;
            cached_brightness_ = brightness_;
            cached_damping_ = damping_;
            params_dirty_ = false;
        }
        
        size_t num_modes = 0;
        float stiffness = GetStiffness(geometry_);
        float harmonic = frequency_;
        float stretch_factor = 1.0f;
        // Elements uses: q = 500.0f * lut_4_decades[damping * 0.8]
        // Our GetQFromDamping returns values already in 0.5-5000 range
        // Scale by 500 to match Elements' q multiplier behavior
        float base_q = 500.0f * GetQFromDamping(damping_);
        
        // Brightness attenuation
        float brightness_attenuation = 1.0f - geometry_;
        brightness_attenuation *= brightness_attenuation;
        brightness_attenuation *= brightness_attenuation;
        brightness_attenuation *= brightness_attenuation;
        float brightness = brightness_ * (1.0f - 0.2f * brightness_attenuation);
        float q_loss = brightness * (2.0f - brightness) * 0.85f + 0.15f;
        float q_loss_damping_rate = geometry_ * (2.0f - geometry_) * 0.1f;
        
        for (size_t i = 0; i < static_cast<size_t>(kNumModes); ++i) {
            // Priority 1B: Aggressive clock divider for higher modes
            // Critical modes (0-3): Always update - carry most perceptual weight
            // Primary modes (4-7): Every 2 samples - important harmonics
            // Secondary modes (8-15): Every 4 samples - less critical
            // Tertiary modes (16+): Every 8 samples - least audible
            bool update;
            if (full_update) {
                update = true;  // Force update all when params changed
            } else if (i <= 3) {
                update = true;  // Critical modes: every sample
            } else if (i <= 7) {
                update = (clock_divider_ & 1) == 0;  // Every 2 samples
            } else if (i <= 15) {
                update = (clock_divider_ & 3) == 0;  // Every 4 samples
            } else {
                update = (clock_divider_ & 7) == 0;  // Every 8 samples
            }
            
            float partial_frequency = harmonic * stretch_factor;
            
            if (partial_frequency >= 0.49f) {
                partial_frequency = 0.49f;
            } else {
                num_modes = i + 1;
            }
            
            if (update) {
                float mode_q = 1.0f + partial_frequency * base_q;
                
                cached_g_[i] = LookupSvfG(partial_frequency);
                cached_r_[i] = 1.0f / Clamp(mode_q, 0.5f, 500.0f);
                
                modes_[i].SetCoefficients(cached_g_[i], cached_r_[i]);
                
                // Update SoA arrays for NEON optimization
                float g = cached_g_[i];
                float k = cached_r_[i];
                soa_a1_[i] = 1.0f / (1.0f + g * (g + k));
                soa_a2_[i] = g * soa_a1_[i];
                soa_a3_[i] = g * soa_a2_[i];
                
                // Also update bowed modes (first kMaxBowedModes)
                if (i < kMaxBowedModes) {
                    // Calculate delay line period (samples per cycle)
                    size_t period = static_cast<size_t>(1.0f / partial_frequency);
                    while (period >= kMaxDelayLineSize) period >>= 1;  // Halve if too long
                    
                    bowed_modes_[i].SetDelay(period);
                    // Bowed modes use higher Q for better sustain
                    bowed_modes_[i].SetGAndQ(cached_g_[i], 1.0f + partial_frequency * 1500.0f);
                }
            }
            
            // Update stretch factor for next mode
            stretch_factor += stiffness;
            if (stiffness < 0.0f) {
                stiffness *= 0.93f;
            } else {
                stiffness *= 0.98f;
            }
            
            // Update Q loss
            q_loss += q_loss_damping_rate * (1.0f - q_loss);
            harmonic += frequency_;
            base_q *= q_loss;
        }
        
        cached_num_modes_ = num_modes > 0 ? num_modes : 1;
        return cached_num_modes_;
    }
    
    Mode modes_[kNumModes];
    BowedMode bowed_modes_[kMaxBowedModes];  // Banded waveguides for bowing
    float cached_g_[kNumModes];  // Cached g coefficients
    float cached_r_[kNumModes];  // Cached r coefficients
    
    // Structure-of-Arrays (SoA) layout for NEON optimization
    // Allows direct vld1q_f32 loads without scatter-gather
    float soa_a1_[kNumModes];    // Pre-computed a1 coefficients
    float soa_a2_[kNumModes];    // Pre-computed a2 coefficients
    float soa_a3_[kNumModes];    // Pre-computed a3 coefficients
    float soa_state1_[kNumModes]; // Filter state 1 (bandpass)
    float soa_state2_[kNumModes]; // Filter state 2 (lowpass)
    
    float frequency_;           // Normalized frequency (freq/sample_rate)
    float geometry_;            // Structure/stiffness control
    float brightness_;          // High frequency content
    float damping_;             // Decay time
    float position_;            // Excitation/pickup position
    float previous_position_;   // For interpolation
    float space_;               // Stereo spread
    
    float modulation_frequency_; // LFO rate (normalized)
    float modulation_offset_;    // Stereo offset
    float lfo_phase_;
    
    float bow_signal_;          // Accumulated bow friction signal
    size_t clock_divider_;      // For staggered mode updates
    
    // Priority 1A: Coefficient caching state
    bool params_dirty_;         // True when params changed, need recalc
    float cached_frequency_;    // Last computed frequency
    float cached_geometry_;     // Last computed geometry
    float cached_brightness_;   // Last computed brightness
    float cached_damping_;      // Last computed damping
    size_t cached_num_modes_;   // Cached number of active modes
};

// ============================================================================
// Karplus-Strong String Model with Enhanced Damping Filter
// Based on Elements string.h
// ============================================================================

class DampingFilter {
public:
    DampingFilter() : x_(0.0f), x__(0.0f), brightness_(0.5f), damping_(0.998f) {}
    
    void Init() {
        x_ = 0.0f;
        x__ = 0.0f;
        brightness_ = 0.5f;
        brightness_increment_ = 0.0f;
        damping_ = 0.998f;
        damping_increment_ = 0.0f;
    }
    
    void Configure(float damping, float brightness, size_t size) {
        if (size == 0) {
            damping_ = damping;
            brightness_ = brightness;
            damping_increment_ = 0.0f;
            brightness_increment_ = 0.0f;
        } else {
            float step = 1.0f / static_cast<float>(size);
            damping_increment_ = (damping - damping_) * step;
            brightness_increment_ = (brightness - brightness_) * step;
        }
    }
    
    float Process(float x) {
        // 3-tap FIR lowpass with brightness control
        float h0 = (1.0f + brightness_) * 0.5f;
        float h1 = (1.0f - brightness_) * 0.25f;
        float y = damping_ * (h0 * x_ + h1 * (x + x__));
        x__ = x_;
        x_ = x;
        brightness_ += brightness_increment_;
        damping_ += damping_increment_;
        
        // Flush denormals/NaN in filter states
        if (x_ != x_ || x_ > 1e4f || x_ < -1e4f) x_ = 0.0f;
        if (x__ != x__ || x__ > 1e4f || x__ < -1e4f) x__ = 0.0f;
        
        return y;
    }
    
    void Reset() { x_ = x__ = 0.0f; }
    
private:
    float x_, x__;
    float brightness_, brightness_increment_;
    float damping_, damping_increment_;
};

// ============================================================================
// Dispersion Allpass Filter for Piano-Like Inharmonicity
// Based on Elements' dispersion model
// ============================================================================

class DispersionFilter {
public:
    static constexpr int kNumStages = 4;  // Number of allpass stages
    
    DispersionFilter() {
        Reset();
    }
    
    void Reset() {
        for (int i = 0; i < kNumStages; ++i) {
            state_[i] = 0.0f;
        }
        amount_ = 0.0f;
        coefficient_ = 0.0f;
    }
    
    // Set dispersion amount (0 = none, 1 = maximum)
    void SetAmount(float amount) {
        amount_ = Clamp(amount, 0.0f, 1.0f);
        // Coefficient derived from amount - higher = more dispersion
        // Range approximately -0.7 to 0.7 for stability
        coefficient_ = amount_ * 0.65f;
    }
    
    // Set coefficient based on frequency (lower freqs = more dispersion)
    void Configure(float frequency, float amount) {
        // Higher frequencies need less dispersion for stability
        float freq_scale = 1.0f - Clamp(frequency / 4000.0f, 0.0f, 0.8f);
        SetAmount(amount * freq_scale);
    }
    
    float Process(float x) {
        if (amount_ < 0.01f) return x;  // Bypass if minimal
        
        // Cascade of first-order allpass filters
        // Each stage adds phase shift that increases with frequency
        // First-order allpass: y[n] = -a*x[n] + x[n-1] + a*y[n-1]
        float y = x;
        for (int i = 0; i < kNumStages; ++i) {
            float x_in = y;
            float y_out = -coefficient_ * x_in + state_[i];
            state_[i] = x_in + coefficient_ * y_out;
            y = y_out;
            
            // Stability check - flush denormals/NaN
            if (state_[i] != state_[i] || state_[i] > 1e4f || state_[i] < -1e4f) {
                state_[i] = 0.0f;
            }
        }
        
        return y;
    }
    
private:
    float state_[kNumStages];
    float amount_;
    float coefficient_;
};

class String {
public:
    static constexpr int kMaxDelay = 2048;
    
    String() {
        Reset();
    }
    
    void Reset() {
        for (int i = 0; i < kMaxDelay; ++i) delay_[i] = 0.0f;
        write_ptr_ = 0;
        freq_ = 220.0f;
        damping_ = 0.5f;
        brightness_ = 0.5f;
        dispersion_ = 0.0f;
        lp_state_ = 0.0f;
        dc_blocker_x_ = 0.0f;
        dc_blocker_y_ = 0.0f;
        UpdateCoefficients();
    }
    
    void SetFrequency(float freq) {
        freq_ = Clamp(freq, 20.0f, 4000.0f);
        UpdateCoefficients();
    }
    
    void SetDamping(float d) {
        damping_ = Clamp(d, 0.0f, 1.0f);
        UpdateCoefficients();
    }
    
    void SetBrightness(float b) {
        brightness_ = Clamp(b, 0.0f, 1.0f);
        UpdateCoefficients();
    }
    
    // Set dispersion amount (0 = none, 1 = piano-like)
    void SetDispersion(float d) {
        dispersion_ = Clamp(d, 0.0f, 1.0f);
        dispersion_filter_.Configure(freq_, dispersion_);
    }
    
    float Process(float excitation) {
        // Protect against NaN input
        if (excitation != excitation) excitation = 0.0f;
        
        // Read from delay line with linear interpolation
        float read_pos = static_cast<float>(write_ptr_) - delay_samples_;
        if (read_pos < 0.0f) read_pos += kMaxDelay;
        
        int read_idx = static_cast<int>(read_pos);
        float frac = read_pos - static_cast<float>(read_idx);
        
        int idx0 = read_idx & (kMaxDelay - 1);
        int idx1 = (read_idx + 1) & (kMaxDelay - 1);
        
        float delayed = delay_[idx0] * (1.0f - frac) + delay_[idx1] * frac;
        
        // Simple one-pole lowpass for brightness control
        // Lower cutoff = darker sound (more high freq damping)
        lp_state_ += lp_coeff_ * (delayed - lp_state_);
        float filtered = lp_state_;
        
        // Apply feedback (damping controls decay time)
        filtered *= feedback_;
        
        // Apply dispersion (piano-like inharmonicity) if enabled
        if (dispersion_ > 0.01f) {
            filtered = dispersion_filter_.Process(filtered);
        }
        
        // DC blocker to prevent drift
        float dc_out = filtered - dc_blocker_x_ + 0.995f * dc_blocker_y_;
        dc_blocker_x_ = filtered;
        dc_blocker_y_ = dc_out;
        filtered = dc_out;
        
        // Stability check
        if (filtered != filtered || filtered > 1e4f || filtered < -1e4f) {
            Reset();
            return 0.0f;
        }
        
        // Write to delay line (excitation + feedback)
        delay_[write_ptr_] = excitation + filtered;
        write_ptr_ = (write_ptr_ + 1) & (kMaxDelay - 1);
        
        return filtered;
    }
    
private:
    void UpdateCoefficients() {
        // Calculate delay in samples for the fundamental frequency
        delay_samples_ = kSampleRate / freq_;
        if (delay_samples_ > kMaxDelay - 2) delay_samples_ = kMaxDelay - 2;
        if (delay_samples_ < 2) delay_samples_ = 2;
        
        // Feedback coefficient for decay time
        // damping = 0 -> long decay (feedback ~0.9995)
        // damping = 1 -> short decay (feedback ~0.98)
        // Higher frequencies need slightly more damping to sound natural
        float freq_compensation = 1.0f - (freq_ / 8000.0f) * 0.1f;
        feedback_ = (0.9998f - damping_ * 0.02f) * freq_compensation;
        feedback_ = Clamp(feedback_, 0.9f, 0.9998f);
        
        // One-pole lowpass coefficient for brightness
        // brightness = 0 -> very dark (lp_coeff ~0.1)
        // brightness = 1 -> bright (lp_coeff ~0.9)
        lp_coeff_ = 0.1f + brightness_ * 0.85f;
        
        // Update dispersion filter when frequency changes
        dispersion_filter_.Configure(freq_, dispersion_);
    }
    
    float delay_[kMaxDelay];
    int write_ptr_;
    float delay_samples_;
    float feedback_;
    float lp_coeff_;
    float lp_state_;
    float dc_blocker_x_, dc_blocker_y_;
    DispersionFilter dispersion_filter_;
    float freq_, damping_, brightness_, dispersion_;
};

// ============================================================================
// MultiString - 5 sympathetic strings for rich 12-string guitar/piano sounds
// Based on Elements RESONATOR_MODEL_STRINGS
// ============================================================================

class MultiString {
public:
    static constexpr int kNumStrings = 5;
    
    // Detuning ratios for sympathetic strings (in cents)
    // String 0: main string (0 cents)
    // Strings 1-4: sympathetic strings with subtle detuning for chorus effect
    static constexpr float kDetuning[kNumStrings] = {
        0.0f,    // Main string
        -5.0f,   // Slightly flat
        5.0f,    // Slightly sharp
        -10.0f,  // More flat (creates beating)
        10.0f    // More sharp
    };
    
    // Amplitude ratios for each string (main louder, sympathetics softer)
    static constexpr float kAmplitude[kNumStrings] = {
        1.0f,    // Main string full volume
        0.4f,    // Sympathetic strings quieter
        0.4f,
        0.25f,
        0.25f
    };
    
    MultiString() {
        Reset();
    }
    
    void Reset() {
        for (int i = 0; i < kNumStrings; ++i) {
            strings_[i].Reset();
        }
        detune_amount_ = 0.5f;
        freq_ = 220.0f;
    }
    
    void SetFrequency(float freq) {
        freq_ = Clamp(freq, 20.0f, 4000.0f);
        UpdateFrequencies();
    }
    
    void SetDamping(float d) {
        for (int i = 0; i < kNumStrings; ++i) {
            strings_[i].SetDamping(d);
        }
    }
    
    void SetBrightness(float b) {
        for (int i = 0; i < kNumStrings; ++i) {
            strings_[i].SetBrightness(b);
        }
    }
    
    // Set dispersion for piano-like inharmonicity
    void SetDispersion(float d) {
        for (int i = 0; i < kNumStrings; ++i) {
            strings_[i].SetDispersion(d);
        }
    }
    
    // Control amount of detuning (0 = unison, 1 = full detuning)
    void SetDetuneAmount(float amount) {
        detune_amount_ = Clamp(amount, 0.0f, 1.0f);
        UpdateFrequencies();
    }
    
    float Process(float excitation) {
        float out = 0.0f;
        
        // Main string gets full excitation
        out += strings_[0].Process(excitation) * kAmplitude[0];
        
        // Sympathetic strings get reduced excitation
        // They "ring along" with the main string via acoustic coupling
        // Note: NEON optimization not used here - String::Process() is inherently
        // serial due to stateful delay line, so NEON overhead exceeds benefit
        float sympathetic_input = excitation * 0.2f;
        for (int i = 1; i < kNumStrings; ++i) {
            out += strings_[i].Process(sympathetic_input) * kAmplitude[i];
        }
        
        // Normalize output (sum of amplitudes is ~2.3)
        return out * 0.45f;
    }
    
private:
    void UpdateFrequencies() {
        for (int i = 0; i < kNumStrings; ++i) {
            // Convert cents to frequency ratio: ratio = 2^(cents/1200)
            float cents = kDetuning[i] * detune_amount_;
            // More accurate cents to ratio: 2^(c/1200) ≈ 1 + c * 0.0005778
            float ratio = 1.0f + cents * 0.0005778f;
            strings_[i].SetFrequency(freq_ * ratio);
        }
    }
    
    String strings_[kNumStrings];
    float freq_;
    float detune_amount_;
};

// Out-of-class definitions for static constexpr arrays (required pre-C++17)
constexpr float MultiString::kDetuning[MultiString::kNumStrings];
constexpr float MultiString::kAmplitude[MultiString::kNumStrings];

} // namespace modal
