/**
 * @file wavetable_osc.h
 * @brief Reusable wavetable oscillator for drumlogue units
 *
 * Uses integrated wavetable synthesis (Franck & Välimäki, DAFX-12)
 * with differentiation for anti-aliasing and smooth morphing.
 *
 * Inspired by Mutable Instruments Plaits and VAST Dynamics Vaporizer2.
 *
 * Usage:
 *   1. Include this header
 *   2. Provide wavetable data as int16_t arrays with guard samples
 *   3. Call Process() with wavetable pointers
 *
 * Wavetable Format:
 *   - Each wave: table_size + 4 samples (guard samples for interpolation)
 *   - Data format: int16_t (-32768 to 32767)
 *   - Should be pre-integrated for anti-aliasing (cumulative sum)
 */

#pragma once

#include <cstdint>
#include <cmath>

// NEON SIMD for batch operations (optional)
#ifdef USE_NEON
#include <arm_neon.h>
#endif

namespace common {

/**
 * @brief One-pole lowpass filter for smoothing
 */
class OnePole {
public:
    OnePole() : state_(0.0f) {}
    
    void Reset() { state_ = 0.0f; }
    
    float Process(float in, float coefficient) {
        state_ += coefficient * (in - state_);
        return state_;
    }
    
private:
    float state_;
};

/**
 * @brief Differentiator for integrated wavetable playback
 * 
 * Converts integrated wavetable back to original waveform
 * while providing natural anti-aliasing.
 */
class Differentiator {
public:
    Differentiator() : previous_(0.0f), lp_(0.0f) {}
    
    void Reset() {
        previous_ = 0.0f;
        lp_ = 0.0f;
    }
    
    float Process(float coefficient, float sample) {
        // Differentiate and lowpass filter
        float diff = sample - previous_;
        lp_ += coefficient * (diff - lp_);
        previous_ = sample;
        return lp_;
    }
    
private:
    float previous_;
    float lp_;
};

/**
 * @brief Linear interpolation between two wavetable samples
 * @param table Pointer to wavetable data (int16_t)
 * @param index Base sample index
 * @param frac Fractional position (0.0 to 1.0)
 * @return Interpolated sample value
 */
inline float InterpolateWaveLinear(const int16_t* table, int32_t index, float frac) {
    const float a = static_cast<float>(table[index]);
    const float b = static_cast<float>(table[index + 1]);
    return a + (b - a) * frac;
}

/**
 * @brief Hermite (cubic) interpolation for higher quality
 * @param table Pointer to wavetable data (int16_t), needs index-1 to index+2
 * @param index Base sample index (must have valid data at index-1)
 * @param frac Fractional position (0.0 to 1.0)
 * @return Interpolated sample value
 *
 * Note: Requires guard samples before and after the main wave data
 */
inline float InterpolateWaveHermite(const int16_t* table, int32_t index, float frac) {
    const float xm1 = static_cast<float>(table[index]);
    const float x0 = static_cast<float>(table[index + 1]);
    const float x1 = static_cast<float>(table[index + 2]);
    const float x2 = static_cast<float>(table[index + 3]);
    const float c = (x1 - xm1) * 0.5f;
    const float v = x0 - x1;
    const float w = c + v;
    const float a = w + v + (x2 - x0) * 0.5f;
    const float b_neg = w + a;
    return (((a * frac) - b_neg) * frac + c) * frac + x0;
}

#ifdef USE_NEON
/**
 * @brief NEON-optimized batch linear interpolation
 * 
 * Performs 4 linear interpolations simultaneously using NEON SIMD.
 * All four lookups must be in the same wavetable.
 *
 * @param table Pointer to wavetable data
 * @param indices Array of 4 base sample indices
 * @param fracs Vector of 4 fractional positions
 * @return Vector of 4 interpolated samples
 */
inline float32x4_t InterpolateWaveLinear4(const int16_t* table, 
                                          const int32_t* indices, 
                                          float32x4_t fracs) {
    // Load 4 pairs of samples and interpolate
    float a0 = static_cast<float>(table[indices[0]]);
    float b0 = static_cast<float>(table[indices[0] + 1]);
    float a1 = static_cast<float>(table[indices[1]]);
    float b1 = static_cast<float>(table[indices[1] + 1]);
    float a2 = static_cast<float>(table[indices[2]]);
    float b2 = static_cast<float>(table[indices[2] + 1]);
    float a3 = static_cast<float>(table[indices[3]]);
    float b3 = static_cast<float>(table[indices[3] + 1]);
    
    float32x4_t a_vec = {a0, a1, a2, a3};
    float32x4_t b_vec = {b0, b1, b2, b3};
    
    // result = a + (b - a) * frac
    return vmlaq_f32(a_vec, vsubq_f32(b_vec, a_vec), fracs);
}

/**
 * @brief NEON-optimized batch Hermite interpolation
 */
inline float32x4_t InterpolateWaveHermite4(const int16_t* table,
                                           const int32_t* indices,
                                           float32x4_t fracs) {
    // Load 4 sets of 4 samples each
    float xm1_0 = static_cast<float>(table[indices[0]]);
    float x0_0 = static_cast<float>(table[indices[0] + 1]);
    float x1_0 = static_cast<float>(table[indices[0] + 2]);
    float x2_0 = static_cast<float>(table[indices[0] + 3]);
    
    float xm1_1 = static_cast<float>(table[indices[1]]);
    float x0_1 = static_cast<float>(table[indices[1] + 1]);
    float x1_1 = static_cast<float>(table[indices[1] + 2]);
    float x2_1 = static_cast<float>(table[indices[1] + 3]);
    
    float xm1_2 = static_cast<float>(table[indices[2]]);
    float x0_2 = static_cast<float>(table[indices[2] + 1]);
    float x1_2 = static_cast<float>(table[indices[2] + 2]);
    float x2_2 = static_cast<float>(table[indices[2] + 3]);
    
    float xm1_3 = static_cast<float>(table[indices[3]]);
    float x0_3 = static_cast<float>(table[indices[3] + 1]);
    float x1_3 = static_cast<float>(table[indices[3] + 2]);
    float x2_3 = static_cast<float>(table[indices[3] + 3]);
    
    float32x4_t xm1 = {xm1_0, xm1_1, xm1_2, xm1_3};
    float32x4_t x0 = {x0_0, x0_1, x0_2, x0_3};
    float32x4_t x1 = {x1_0, x1_1, x1_2, x1_3};
    float32x4_t x2 = {x2_0, x2_1, x2_2, x2_3};
    
    float32x4_t half = vdupq_n_f32(0.5f);
    
    // c = (x1 - xm1) * 0.5
    float32x4_t c = vmulq_f32(vsubq_f32(x1, xm1), half);
    // v = x0 - x1
    float32x4_t v = vsubq_f32(x0, x1);
    // w = c + v
    float32x4_t w = vaddq_f32(c, v);
    // a = w + v + (x2 - x0) * 0.5
    float32x4_t a = vaddq_f32(vaddq_f32(w, v), vmulq_f32(vsubq_f32(x2, x0), half));
    // b_neg = w + a
    float32x4_t b_neg = vaddq_f32(w, a);
    
    // result = (((a * frac) - b_neg) * frac + c) * frac + x0
    float32x4_t result = vmulq_f32(a, fracs);              // a * frac
    result = vsubq_f32(result, b_neg);                      // - b_neg
    result = vmlaq_f32(c, result, fracs);                   // * frac + c
    result = vmlaq_f32(x0, result, fracs);                  // * frac + x0
    
    return result;
}
#endif // USE_NEON

/**
 * @brief Wavetable oscillator with morphing and anti-aliasing
 *
 * Template parameters allow compile-time configuration:
 * @tparam TABLE_SIZE Number of samples per wave cycle (e.g., 256, 512, 1024)
 * @tparam WAVES_PER_BANK Number of waves available for morphing (e.g., 8, 16)
 */
template<uint32_t TABLE_SIZE = 256, uint32_t WAVES_PER_BANK = 8>
class WavetableOsc {
public:
    WavetableOsc() : phase_(0.0f) {}
    ~WavetableOsc() {}
    
    void Init() {
        phase_ = 0.0f;
        diff_.Reset();
        lp_.Reset();
    }
    
    void Reset() {
        phase_ = 0.0f;
        diff_.Reset();
        lp_.Reset();
    }
    
    /**
     * @brief Set phase directly (useful for hard sync)
     * @param phase New phase value (0.0 to 1.0)
     */
    void SetPhase(float phase) {
        phase_ = phase;
        if (phase_ >= 1.0f) phase_ -= 1.0f;
        if (phase_ < 0.0f) phase_ += 1.0f;
    }
    
    /**
     * @brief Get current phase
     * @return Phase value (0.0 to 1.0)
     */
    float GetPhase() const { return phase_; }
    
    /**
     * @brief Process one sample with external wavetable data
     * @param frequency Normalized frequency (freq_hz / sample_rate)
     * @param morph Morph position (0.0 to 1.0)
     * @param wavetable Array of wave pointers (WAVES_PER_BANK+1 pointers)
     * @return Audio sample (approximately -1.0 to +1.0)
     *
     * The wavetable parameter should point to an array of (WAVES_PER_BANK+1)
     * wave pointers to allow interpolation at morph=1.0
     */
    float Process(float frequency, float morph, const int16_t* const* wavetable) {
        // Clamp frequency to prevent aliasing
        constexpr float kMaxFreq = 0.25f;  // Nyquist / 2
        constexpr float kMinFreq = 0.000001f;
        if (frequency > kMaxFreq) frequency = kMaxFreq;
        if (frequency < kMinFreq) frequency = kMinFreq;
        
        // High-frequency attenuation (natural anti-aliasing behavior)
        float amplitude = 1.0f - 2.0f * frequency;
        if (amplitude < 0.0f) amplitude = 0.0f;
        
        // Scale factor for integrated wavetable
        // 256.0f is the int16 to float normalization factor
        const float scale = 1.0f / (frequency * static_cast<float>(TABLE_SIZE) * 256.0f);
        
        // Advance phase
        phase_ += frequency;
        if (phase_ >= 1.0f) {
            phase_ -= 1.0f;
        }
        
        // Calculate morph position between waves
        const float wave_pos = morph * static_cast<float>(WAVES_PER_BANK - 1);
        const int wave_idx = static_cast<int>(wave_pos);
        const float wave_frac = wave_pos - static_cast<float>(wave_idx);
        
        // Calculate sample position in wavetable
        const float table_pos = phase_ * static_cast<float>(TABLE_SIZE);
        const int sample_idx = static_cast<int>(table_pos);
        const float sample_frac = table_pos - static_cast<float>(sample_idx);
        
        // Interpolate between two adjacent waves (morphing)
        const float s0 = InterpolateWaveLinear(wavetable[wave_idx], sample_idx, sample_frac);
        const float s1 = InterpolateWaveLinear(wavetable[wave_idx + 1], sample_idx, sample_frac);
        const float raw_sample = s0 + (s1 - s0) * wave_frac;
        
        // Differentiate to convert integrated wavetable back
        const float cutoff = fminf(static_cast<float>(TABLE_SIZE) * frequency, 1.0f);
        float sample = diff_.Process(cutoff, raw_sample * scale);
        
        // Final lowpass smoothing
        sample = lp_.Process(sample, cutoff);
        
        return sample * amplitude;
    }
    
    /**
     * @brief Process with Hermite interpolation (higher quality, more CPU)
     */
    float ProcessHQ(float frequency, float morph, const int16_t* const* wavetable) {
        // Clamp frequency to prevent aliasing
        constexpr float kMaxFreq = 0.25f;
        constexpr float kMinFreq = 0.000001f;
        if (frequency > kMaxFreq) frequency = kMaxFreq;
        if (frequency < kMinFreq) frequency = kMinFreq;
        
        float amplitude = 1.0f - 2.0f * frequency;
        if (amplitude < 0.0f) amplitude = 0.0f;
        
        const float scale = 1.0f / (frequency * static_cast<float>(TABLE_SIZE) * 256.0f);
        
        phase_ += frequency;
        if (phase_ >= 1.0f) {
            phase_ -= 1.0f;
        }
        
        const float wave_pos = morph * static_cast<float>(WAVES_PER_BANK - 1);
        const int wave_idx = static_cast<int>(wave_pos);
        const float wave_frac = wave_pos - static_cast<float>(wave_idx);
        
        const float table_pos = phase_ * static_cast<float>(TABLE_SIZE);
        const int sample_idx = static_cast<int>(table_pos);
        const float sample_frac = table_pos - static_cast<float>(sample_idx);
        
        // Use Hermite interpolation for higher quality
        const float s0 = InterpolateWaveHermite(wavetable[wave_idx], sample_idx, sample_frac);
        const float s1 = InterpolateWaveHermite(wavetable[wave_idx + 1], sample_idx, sample_frac);
        const float raw_sample = s0 + (s1 - s0) * wave_frac;
        
        const float cutoff = fminf(static_cast<float>(TABLE_SIZE) * frequency, 1.0f);
        float sample = diff_.Process(cutoff, raw_sample * scale);
        sample = lp_.Process(sample, cutoff);
        
        return sample * amplitude;
    }
    
#ifdef USE_NEON
    /**
     * @brief Process a block of samples using NEON SIMD
     *
     * Processes 4 samples at a time for improved performance.
     * Note: This bypasses the differentiator/lowpass for raw wavetable output.
     * Use for LFO-like applications or when integrated wavetables aren't needed.
     *
     * @param output Output buffer (must be at least 'count' floats)
     * @param count Number of samples to generate
     * @param frequency Normalized frequency (constant for block)
     * @param morph Morph position (constant for block)
     * @param wavetable Array of wave pointers
     */
    void ProcessBlock(float* output, uint32_t count, float frequency, float morph,
                      const int16_t* const* wavetable) {
        // Clamp parameters
        constexpr float kMaxFreq = 0.25f;
        constexpr float kMinFreq = 0.000001f;
        if (frequency > kMaxFreq) frequency = kMaxFreq;
        if (frequency < kMinFreq) frequency = kMinFreq;
        
        const float amplitude = fmaxf(0.0f, 1.0f - 2.0f * frequency);
        const float table_size_f = static_cast<float>(TABLE_SIZE);
        const float norm = 1.0f / 32768.0f;  // int16 to float normalization
        
        // Calculate morph position between waves (constant for block)
        const float wave_pos = morph * static_cast<float>(WAVES_PER_BANK - 1);
        const int wave_idx = static_cast<int>(wave_pos);
        const float wave_frac = wave_pos - static_cast<float>(wave_idx);
        
        const int16_t* wave0 = wavetable[wave_idx];
        const int16_t* wave1 = wavetable[wave_idx + 1];
        
        // NEON vectors for phase advancement
        float32x4_t phase_vec = {phase_, phase_ + frequency, 
                                  phase_ + 2.0f * frequency, 
                                  phase_ + 3.0f * frequency};
        float32x4_t freq_x4 = vdupq_n_f32(4.0f * frequency);
        float32x4_t one_vec = vdupq_n_f32(1.0f);
        float32x4_t table_size_vec = vdupq_n_f32(table_size_f);
        float32x4_t wave_frac_vec = vdupq_n_f32(wave_frac);
        float32x4_t amp_norm_vec = vdupq_n_f32(amplitude * norm);
        
        uint32_t i = 0;
        for (; i + 4 <= count; i += 4) {
            // Wrap phase to [0, 1)
            // phase = phase - floor(phase)
            float32x4_t phase_floor = vcvtq_f32_s32(vcvtq_s32_f32(phase_vec));
            uint32x4_t needs_wrap = vcgeq_f32(phase_vec, one_vec);
            phase_vec = vbslq_f32(needs_wrap, vsubq_f32(phase_vec, phase_floor), phase_vec);
            
            // Calculate table positions
            float32x4_t table_pos = vmulq_f32(phase_vec, table_size_vec);
            
            // Extract indices and fractions
            int32x4_t sample_idx_vec = vcvtq_s32_f32(table_pos);
            float32x4_t sample_frac = vsubq_f32(table_pos, vcvtq_f32_s32(sample_idx_vec));
            
            // Extract individual indices for table lookups
            int32_t indices[4];
            vst1q_s32(indices, sample_idx_vec);
            
            // Interpolate from both waves using NEON
            float32x4_t s0 = InterpolateWaveLinear4(wave0, indices, sample_frac);
            float32x4_t s1 = InterpolateWaveLinear4(wave1, indices, sample_frac);
            
            // Morph between waves: s0 + (s1 - s0) * wave_frac
            float32x4_t morphed = vmlaq_f32(s0, vsubq_f32(s1, s0), wave_frac_vec);
            
            // Scale and store: sample * amplitude * norm
            float32x4_t result = vmulq_f32(morphed, amp_norm_vec);
            vst1q_f32(&output[i], result);
            
            // Advance phase
            phase_vec = vaddq_f32(phase_vec, freq_x4);
        }
        
        // Update phase state (extract last phase + frequency)
        float phases[4];
        vst1q_f32(phases, phase_vec);
        phase_ = phases[0];  // First element of next iteration = current phase
        if (phase_ >= 1.0f) phase_ -= 1.0f;
        
        // Scalar fallback for remaining samples
        for (; i < count; ++i) {
            output[i] = ProcessSimple(frequency, morph, wavetable) * amplitude;
        }
    }
    
    /**
     * @brief Process block with modulation (frequency and morph can vary per sample)
     *
     * @param output Output buffer
     * @param count Number of samples
     * @param freq_buffer Per-sample frequencies (or nullptr for constant)
     * @param morph_buffer Per-sample morph values (or nullptr for constant)
     * @param base_freq Base frequency if freq_buffer is nullptr
     * @param base_morph Base morph if morph_buffer is nullptr
     * @param wavetable Array of wave pointers
     */
    void ProcessBlockMod(float* output, uint32_t count,
                         const float* freq_buffer, const float* morph_buffer,
                         float base_freq, float base_morph,
                         const int16_t* const* wavetable) {
        const float norm = 1.0f / 32768.0f;
        const float table_size_f = static_cast<float>(TABLE_SIZE);
        
        for (uint32_t i = 0; i < count; ++i) {
            float freq = freq_buffer ? freq_buffer[i] : base_freq;
            float morph = morph_buffer ? morph_buffer[i] : base_morph;
            
            // Clamp frequency
            if (freq > 0.25f) freq = 0.25f;
            if (freq < 0.000001f) freq = 0.000001f;
            
            float amplitude = fmaxf(0.0f, 1.0f - 2.0f * freq);
            
            // Advance phase
            phase_ += freq;
            if (phase_ >= 1.0f) phase_ -= 1.0f;
            
            // Morph position
            const float wave_pos = morph * static_cast<float>(WAVES_PER_BANK - 1);
            const int wave_idx = static_cast<int>(wave_pos);
            const float wave_frac = wave_pos - static_cast<float>(wave_idx);
            
            // Table position
            const float table_pos = phase_ * table_size_f;
            const int sample_idx = static_cast<int>(table_pos);
            const float sample_frac = table_pos - static_cast<float>(sample_idx);
            
            // Interpolate
            const float s0 = InterpolateWaveLinear(wavetable[wave_idx], sample_idx, sample_frac);
            const float s1 = InterpolateWaveLinear(wavetable[wave_idx + 1], sample_idx, sample_frac);
            
            output[i] = (s0 + (s1 - s0) * wave_frac) * norm * amplitude;
        }
    }
#endif // USE_NEON
    
    /**
     * @brief Simple process without differentiator (for raw wavetable access)
     */
    float ProcessSimple(float frequency, float morph, const int16_t* const* wavetable) {
        phase_ += frequency;
        if (phase_ >= 1.0f) phase_ -= 1.0f;
        
        const float wave_pos = morph * static_cast<float>(WAVES_PER_BANK - 1);
        const int wave_idx = static_cast<int>(wave_pos);
        const float wave_frac = wave_pos - static_cast<float>(wave_idx);
        
        const float table_pos = phase_ * static_cast<float>(TABLE_SIZE);
        const int sample_idx = static_cast<int>(table_pos);
        const float sample_frac = table_pos - static_cast<float>(sample_idx);
        
        const float s0 = InterpolateWaveLinear(wavetable[wave_idx], sample_idx, sample_frac);
        const float s1 = InterpolateWaveLinear(wavetable[wave_idx + 1], sample_idx, sample_frac);
        
        return (s0 + (s1 - s0) * wave_frac) / 32768.0f;
    }
    
private:
    float phase_;
    Differentiator diff_;
    OnePole lp_;
};

/**
 * @brief Simple wavetable oscillator without anti-aliasing (for LFOs, etc.)
 *
 * Lighter weight version for modulation sources where anti-aliasing
 * is not needed.
 */
template<uint32_t TABLE_SIZE = 256, uint32_t WAVES_PER_BANK = 8>
class SimpleWavetableOsc {
public:
    SimpleWavetableOsc() : phase_(0.0f) {}
    
    void Init() { phase_ = 0.0f; }
    void Reset() { phase_ = 0.0f; }
    
    void SetPhase(float phase) {
        phase_ = phase;
        if (phase_ >= 1.0f) phase_ -= 1.0f;
        if (phase_ < 0.0f) phase_ += 1.0f;
    }
    
    float GetPhase() const { return phase_; }
    
    /**
     * @brief Process one sample (no anti-aliasing)
     * @param frequency Normalized frequency (freq_hz / sample_rate)
     * @param morph Morph position (0.0 to 1.0)
     * @param wavetable Array of wave pointers
     * @return Audio sample
     */
    float Process(float frequency, float morph, const int16_t* const* wavetable) {
        phase_ += frequency;
        if (phase_ >= 1.0f) {
            phase_ -= 1.0f;
        }
        
        const float wave_pos = morph * static_cast<float>(WAVES_PER_BANK - 1);
        const int wave_idx = static_cast<int>(wave_pos);
        const float wave_frac = wave_pos - static_cast<float>(wave_idx);
        
        const float table_pos = phase_ * static_cast<float>(TABLE_SIZE);
        const int sample_idx = static_cast<int>(table_pos);
        const float sample_frac = table_pos - static_cast<float>(sample_idx);
        
        const float s0 = InterpolateWaveLinear(wavetable[wave_idx], sample_idx, sample_frac);
        const float s1 = InterpolateWaveLinear(wavetable[wave_idx + 1], sample_idx, sample_frac);
        
        // Normalize int16 to float (-1 to +1 approximately)
        return (s0 + (s1 - s0) * wave_frac) / 32768.0f;
    }
    
private:
    float phase_;
};

} // namespace common
