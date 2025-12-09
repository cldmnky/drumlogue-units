/**
 * @file ppg_osc.h
 * @brief PPG Wave style wavetable oscillator for drumlogue units
 *
 * Implements a faithful recreation of the PPG Wave 2.2/2.3 oscillator
 * architecture, including:
 * - 64-sample half-waves with antisymmetric mirroring
 * - 8-bit waveform data
 * - Wavetable sweep with key-wave interpolation
 * - Three modes: full interpolation, sample-only, no interpolation
 * - NEON SIMD batch processing for improved performance
 *
 * Based on research from:
 * - Hermann Seib's PPG Wave documentation
 * - Electric Druid's wavetable oscillator article
 * - Jacajack/usynth PPG implementation
 * - vuki/WvTable-logue implementation
 */

#pragma once

#include <cstdint>
#include <cmath>

#ifdef USE_NEON
#include <arm_neon.h>
#endif

namespace common {

/**
 * @brief Wavetable entry for PPG-style oscillator
 *
 * Stores pointers to two waves for interpolation, the interpolation
 * factor, and a flag indicating if this position contains a key-wave.
 */
struct PpgWavetableEntry {
    const uint8_t* ptr_l;  // Left wave pointer (lower key-wave)
    const uint8_t* ptr_r;  // Right wave pointer (upper key-wave)
    uint8_t factor;        // Interpolation factor (0-255)
    uint8_t is_key;        // 1 if this is a key-wave position
};

/**
 * @brief PPG Wave oscillator modes
 */
enum class PpgMode : uint8_t {
    INTERP_2D = 0,  // Bilinear: interpolate both wave position and samples
    INTERP_1D = 1,  // Linear: interpolate samples only (integer wave positions)
    NO_INTERP = 2   // No interpolation (original PPG character)
};

/**
 * @brief PPG Wave style wavetable oscillator
 *
 * Features:
 * - Authentic PPG Wave 2.2/2.3 architecture
 * - 64-sample half-waves with antisymmetric mirroring (128 effective samples)
 * - 8-bit waveform data for that classic lo-fi character
 * - Key-wave based wavetable interpolation
 * - Three modes for different interpolation quality/character
 * - Phase skew/warp for timbral variation
 *
 * Template parameters:
 * @tparam WAVETABLE_SIZE Number of positions in the wavetable (default 61)
 */
template<uint32_t WAVETABLE_SIZE = 61>
class PpgOsc {
public:
    PpgOsc() : phase_(0), step_(0), mode_(PpgMode::INTERP_2D), wave_pos_(0) {}
    ~PpgOsc() {}
    
    /**
     * @brief Initialize the oscillator
     * @param sample_rate Audio sample rate in Hz
     */
    void Init(float sample_rate) {
        sample_rate_ = sample_rate;
        phase_ = 0;
        step_ = 0;
        mode_ = PpgMode::INTERP_2D;
        wave_pos_ = 0;
        skew_bp_ = 0;  // No skew by default
        skew_r1_ = 1.0f;
        skew_r2_ = 1.0f;
        
        // Clear wavetable entries
        for (uint32_t i = 0; i < WAVETABLE_SIZE; i++) {
            wavetable_[i].ptr_l = nullptr;
            wavetable_[i].ptr_r = nullptr;
            wavetable_[i].factor = 0;
            wavetable_[i].is_key = 0;
        }
    }
    
    /**
     * @brief Reset oscillator phase
     */
    void Reset() {
        phase_ = 0;
    }
    
    /**
     * @brief Set oscillator frequency
     * @param freq Frequency in Hz
     */
    void SetFrequency(float freq) {
        // Phase step for 32-bit phase accumulator (0x100000000 = full cycle)
        // step = freq / sample_rate * 2^32
        step_ = static_cast<uint32_t>(freq / sample_rate_ * 4294967296.0f);
    }
    
    /**
     * @brief Set interpolation mode
     * @param mode PpgMode value
     */
    void SetMode(PpgMode mode) {
        mode_ = mode;
    }
    
    /**
     * @brief Set wave position within the wavetable
     * @param pos Position (0.0 to 1.0, maps to 0 to WAVETABLE_SIZE-1)
     */
    void SetWavePosition(float pos) {
        // Clamp and scale to wavetable size
        if (pos < 0.0f) pos = 0.0f;
        if (pos > 1.0f) pos = 1.0f;
        wave_pos_ = pos * static_cast<float>(WAVETABLE_SIZE - 1);
    }
    
    /**
     * @brief Set wave position directly (integer index)
     * @param index Wavetable index (0 to WAVETABLE_SIZE-1)
     */
    void SetWaveIndex(uint8_t index) {
        if (index >= WAVETABLE_SIZE) index = WAVETABLE_SIZE - 1;
        wave_pos_ = static_cast<float>(index);
    }
    
    /**
     * @brief Set phase skew/warp for timbral variation
     * @param skew Skew amount (0.0 to 1.0, 0.5 = no skew)
     *
     * Phase skew shifts the breakpoint of the waveform:
     * - 0.5 = symmetric (no skew)
     * - <0.5 = shifts early, compresses first half
     * - >0.5 = shifts late, compresses second half
     */
    void SetSkew(float skew) {
        // Convert skew (0-1) to breakpoint (0 = center at 64, 1 = center at 127)
        // skew_bp in phase units (0 to 2^32)
        if (skew <= 0.0f) {
            skew_bp_ = 0;
            skew_r1_ = 1.0f;
            skew_r2_ = 1.0f;
        } else {
            // Breakpoint at skew * 128 samples
            const float bp_samples = skew * 128.0f;
            skew_bp_ = static_cast<uint32_t>(bp_samples / 128.0f * 4294967296.0f);
            
            // Rate multipliers for each half
            // First half: original 0-64 mapped to 0-bp_samples
            // Second half: original 64-128 mapped to bp_samples-128
            if (bp_samples > 0.0f) {
                skew_r1_ = 64.0f / bp_samples;
            } else {
                skew_r1_ = 1.0f;
            }
            if (bp_samples < 128.0f) {
                skew_r2_ = 64.0f / (128.0f - bp_samples);
            } else {
                skew_r2_ = 1.0f;
            }
        }
    }
    
    /**
     * @brief Load a wavetable definition
     *
     * PPG wavetables use "key waves" at specific positions with interpolation
     * between them. This function sets up the wavetable entries.
     *
     * @param waves_data Pointer to raw 64-sample wave data (8-bit unsigned)
     * @param wavetable_def Array of (position, wave_index) pairs, terminated by 0xFF
     */
    void LoadWavetable(const uint8_t* waves_data, const uint8_t* wavetable_def) {
        // Clear existing entries
        for (uint32_t i = 0; i < WAVETABLE_SIZE; i++) {
            wavetable_[i].ptr_l = nullptr;
            wavetable_[i].ptr_r = nullptr;
            wavetable_[i].factor = 0;
            wavetable_[i].is_key = 0;
        }
        
        // Read key waves from definition
        const uint8_t* ptr = wavetable_def;
        while (*ptr != 0xFF) {
            uint8_t wave_idx = *ptr++;
            uint8_t pos = *ptr++;
            
            if (pos < WAVETABLE_SIZE) {
                // Each wave is 64 bytes
                wavetable_[pos].ptr_l = waves_data + (wave_idx << 6);
                wavetable_[pos].is_key = 1;
            }
        }
        
        // Generate interpolation coefficients between key waves
        const PpgWavetableEntry* el = nullptr;
        const PpgWavetableEntry* er = nullptr;
        
        for (uint32_t i = 0; i < WAVETABLE_SIZE; i++) {
            if (wavetable_[i].is_key) {
                el = er = &wavetable_[i];
                
                // Find next key wave
                for (uint32_t j = i + 1; j < WAVETABLE_SIZE; j++) {
                    if (wavetable_[j].is_key) {
                        er = &wavetable_[j];
                        break;
                    }
                }
            }
            
            if (el && er) {
                wavetable_[i].ptr_l = el->ptr_l;
                wavetable_[i].ptr_r = er->ptr_l;
                
                // Calculate interpolation factor
                uint32_t distance_total = er - el;
                uint32_t distance_l = &wavetable_[i] - el;
                
                if (distance_total > 0) {
                    wavetable_[i].factor = (255 * distance_l) / distance_total;
                } else {
                    wavetable_[i].factor = 0;
                }
            }
        }
    }
    
    /**
     * @brief Load a wavetable from pre-built entry array
     * @param entries Pre-built wavetable entries
     */
    void LoadWavetableEntries(const PpgWavetableEntry* entries) {
        for (uint32_t i = 0; i < WAVETABLE_SIZE; i++) {
            wavetable_[i] = entries[i];
        }
    }
    
    /**
     * @brief Set a wave directly at a position (for simple wavetables)
     * @param pos Position in wavetable
     * @param wave_data Pointer to 64-sample wave data
     */
    void SetWave(uint8_t pos, const uint8_t* wave_data) {
        if (pos < WAVETABLE_SIZE) {
            wavetable_[pos].ptr_l = wave_data;
            wavetable_[pos].ptr_r = wave_data;
            wavetable_[pos].factor = 0;
            wavetable_[pos].is_key = 1;
        }
    }
    
    /**
     * @brief Process one sample
     * @return Audio sample (-1.0 to +1.0)
     */
    float Process() {
        float sample;
        
        switch (mode_) {
            case PpgMode::INTERP_2D:
                sample = ProcessInterp2D();
                break;
            case PpgMode::INTERP_1D:
                sample = ProcessInterp1D();
                break;
            case PpgMode::NO_INTERP:
            default:
                sample = ProcessNoInterp();
                break;
        }
        
        // Advance phase
        phase_ += step_;
        
        return sample;
    }
    
    /**
     * @brief Get current phase (0.0 to 1.0)
     */
    float GetPhase() const {
        return static_cast<float>(phase_) / 4294967296.0f;
    }
    
    /**
     * @brief Set phase directly
     * @param phase Phase value (0.0 to 1.0)
     */
    void SetPhase(float phase) {
        phase_ = static_cast<uint32_t>(phase * 4294967296.0f);
    }
    
private:
    float sample_rate_;
    uint32_t phase_;     // 32-bit phase accumulator
    uint32_t step_;      // Phase increment per sample
    PpgMode mode_;
    float wave_pos_;     // Current position in wavetable (fractional)
    
    // Phase skew parameters
    uint32_t skew_bp_;   // Skew breakpoint in phase units
    float skew_r1_;      // Rate multiplier for first half
    float skew_r2_;      // Rate multiplier for second half
    
    // Wavetable entries
    PpgWavetableEntry wavetable_[WAVETABLE_SIZE];
    
    /**
     * @brief Get sample from PPG-style waveform with mirroring
     *
     * PPG waves are stored as 64 samples of the first half-cycle.
     * The second half is mirrored in both time and amplitude:
     * - Samples 0-63: read directly
     * - Samples 64-127: read from (127-pos) and invert
     *
     * @param wave_data Pointer to 64-sample wave data (8-bit unsigned)
     * @param phase 7-bit sample position (0-127)
     * @return Sample value (0-255)
     */
    uint8_t GetWaveSample(const uint8_t* wave_data, uint8_t phase) const {
        // Phase is 7 bits (0-127)
        if (phase < 64) {
            // First half: read directly
            return wave_data[phase];
        } else {
            // Second half: mirror in time and amplitude
            // Position mirroring: 127-phase maps 64->63, 127->0
            // Amplitude mirroring: 255 - sample (inverts around 128)
            return 255 - wave_data[127 - phase];
        }
    }
    
    /**
     * @brief Apply phase skew/warp
     * @param phase Raw phase position (0-127)
     * @return Warped phase position (0-127)
     */
    float ApplySkew(uint8_t phase) const {
        if (skew_bp_ == 0) {
            return static_cast<float>(phase);
        }
        
        // Convert phase to full 32-bit range for comparison
        uint32_t phase32 = static_cast<uint32_t>(phase) << 25;
        
        if (phase32 <= skew_bp_) {
            return static_cast<float>(phase) * skew_r1_;
        } else {
            // Map phase from (skew_bp to 128) to (64 to 128)
            float normalized = static_cast<float>(phase32 - skew_bp_) / 
                              static_cast<float>(0x80000000 - skew_bp_) * 64.0f;
            return 64.0f + normalized * skew_r2_;
        }
    }
    
    /**
     * @brief Process with bilinear interpolation (highest quality)
     *
     * Interpolates both between samples and between waves.
     */
    float ProcessInterp2D() {
        // Get wavetable entry for current position
        uint8_t wt_idx = static_cast<uint8_t>(wave_pos_);
        if (wt_idx >= WAVETABLE_SIZE) wt_idx = WAVETABLE_SIZE - 1;
        
        float wt_frac = wave_pos_ - static_cast<float>(wt_idx);
        const PpgWavetableEntry& entry = wavetable_[wt_idx];
        
        if (!entry.ptr_l || !entry.ptr_r) {
            return 0.0f;  // No wave loaded
        }
        
        // Convert 32-bit phase to 7-bit sample position with fraction
        // Phase high bits give sample index (0-127)
        const uint8_t phase7 = static_cast<uint8_t>(phase_ >> 25);  // 7 bits
        const float phase_frac = static_cast<float>(phase_ & 0x1FFFFFF) / 33554432.0f;
        
        // Get samples from both waves at current and next positions
        const uint8_t phase7_next = (phase7 + 1) & 0x7F;
        
        // Wave L: current and next samples
        float s_l0 = static_cast<float>(GetWaveSample(entry.ptr_l, phase7));
        float s_l1 = static_cast<float>(GetWaveSample(entry.ptr_l, phase7_next));
        float s_l = s_l0 + (s_l1 - s_l0) * phase_frac;
        
        // Wave R: current and next samples
        float s_r0 = static_cast<float>(GetWaveSample(entry.ptr_r, phase7));
        float s_r1 = static_cast<float>(GetWaveSample(entry.ptr_r, phase7_next));
        float s_r = s_r0 + (s_r1 - s_r0) * phase_frac;
        
        // Interpolate between waves based on position within wavetable
        // Use entry.factor for inter-key-wave interpolation
        // and wt_frac for sub-position interpolation if in INTERP_2D mode
        float wave_blend = static_cast<float>(entry.factor) / 255.0f;
        if (static_cast<uint32_t>(wt_idx + 1) < WAVETABLE_SIZE && wavetable_[wt_idx + 1].ptr_l) {
            // Blend towards next position
            wave_blend += wt_frac;
            if (wave_blend > 1.0f) wave_blend = 1.0f;
        }
        
        float sample = s_l + (s_r - s_l) * wave_blend;
        
        // Convert from 8-bit unsigned (0-255) to float (-1 to +1)
        return (sample - 127.5f) / 127.5f;
    }
    
    /**
     * @brief Process with sample interpolation only (integer wave positions)
     */
    float ProcessInterp1D() {
        // Snap to integer wave position
        uint8_t wt_idx = static_cast<uint8_t>(wave_pos_ + 0.5f);
        if (wt_idx >= WAVETABLE_SIZE) wt_idx = WAVETABLE_SIZE - 1;
        
        const PpgWavetableEntry& entry = wavetable_[wt_idx];
        
        if (!entry.ptr_l || !entry.ptr_r) {
            return 0.0f;
        }
        
        const uint8_t phase7 = static_cast<uint8_t>(phase_ >> 25);
        const float phase_frac = static_cast<float>(phase_ & 0x1FFFFFF) / 33554432.0f;
        const uint8_t phase7_next = (phase7 + 1) & 0x7F;
        
        // Interpolate between waves at this position
        float wave_blend = static_cast<float>(entry.factor) / 255.0f;
        
        // Get blended samples at current and next positions
        float s0_l = static_cast<float>(GetWaveSample(entry.ptr_l, phase7));
        float s0_r = static_cast<float>(GetWaveSample(entry.ptr_r, phase7));
        float s0 = s0_l + (s0_r - s0_l) * wave_blend;
        
        float s1_l = static_cast<float>(GetWaveSample(entry.ptr_l, phase7_next));
        float s1_r = static_cast<float>(GetWaveSample(entry.ptr_r, phase7_next));
        float s1 = s1_l + (s1_r - s1_l) * wave_blend;
        
        // Interpolate between samples
        float sample = s0 + (s1 - s0) * phase_frac;
        
        return (sample - 127.5f) / 127.5f;
    }
    
    /**
     * @brief Process with no interpolation (original PPG character)
     *
     * This produces the characteristic stepped, lo-fi PPG sound.
     */
    float ProcessNoInterp() {
        uint8_t wt_idx = static_cast<uint8_t>(wave_pos_ + 0.5f);
        if (wt_idx >= WAVETABLE_SIZE) wt_idx = WAVETABLE_SIZE - 1;
        
        const PpgWavetableEntry& entry = wavetable_[wt_idx];
        
        if (!entry.ptr_l || !entry.ptr_r) {
            return 0.0f;
        }
        
        const uint8_t phase7 = static_cast<uint8_t>(phase_ >> 25);
        
        // Interpolate between waves (no sample interpolation)
        float wave_blend = static_cast<float>(entry.factor) / 255.0f;
        
        float s_l = static_cast<float>(GetWaveSample(entry.ptr_l, phase7));
        float s_r = static_cast<float>(GetWaveSample(entry.ptr_r, phase7));
        float sample = s_l + (s_r - s_l) * wave_blend;
        
        return (sample - 127.5f) / 127.5f;
    }
    
public:
#ifdef USE_NEON
    /**
     * @brief Process a block of samples using NEON SIMD
     *
     * Processes 4 samples at a time for the INTERP_2D mode.
     * Falls back to scalar processing for other modes or remaining samples.
     *
     * @param output Output buffer (must be at least 'count' floats)
     * @param count Number of samples to generate
     */
    void ProcessBlock(float* output, uint32_t count) {
        if (mode_ != PpgMode::INTERP_2D) {
            // Fallback for non-INTERP_2D modes
            for (uint32_t i = 0; i < count; ++i) {
                output[i] = Process();
            }
            return;
        }
        
        // Get wavetable entry for current position
        uint8_t wt_idx = static_cast<uint8_t>(wave_pos_);
        if (wt_idx >= WAVETABLE_SIZE) wt_idx = WAVETABLE_SIZE - 1;
        
        float wt_frac = wave_pos_ - static_cast<float>(wt_idx);
        const PpgWavetableEntry& entry = wavetable_[wt_idx];
        
        if (!entry.ptr_l || !entry.ptr_r) {
            // No wave loaded - clear output
            for (uint32_t i = 0; i < count; ++i) output[i] = 0.0f;
            return;
        }
        
        // Pre-calculate wave blend factor
        float wave_blend = static_cast<float>(entry.factor) / 255.0f;
        if (static_cast<uint32_t>(wt_idx + 1) < WAVETABLE_SIZE && wavetable_[wt_idx + 1].ptr_l) {
            wave_blend += wt_frac;
            if (wave_blend > 1.0f) wave_blend = 1.0f;
        }
        
        // NEON constants
        float32x4_t blend_vec = vdupq_n_f32(wave_blend);
        float32x4_t offset_vec = vdupq_n_f32(127.5f);
        float32x4_t scale_vec = vdupq_n_f32(1.0f / 127.5f);
        
        // Phase step for 4 samples
        uint32_t step_x4 = step_ << 2;
        
        uint32_t i = 0;
        for (; i + 4 <= count; i += 4) {
            // Get phase values for 4 samples
            uint32_t phases[4] = {
                phase_,
                phase_ + step_,
                phase_ + (step_ << 1),
                phase_ + step_ + (step_ << 1)
            };
            
            // Extract 7-bit phase indices and fractional parts
            uint8_t phase7[4];
            float phase_frac[4];
            for (int j = 0; j < 4; ++j) {
                phase7[j] = static_cast<uint8_t>(phases[j] >> 25);
                phase_frac[j] = static_cast<float>(phases[j] & 0x1FFFFFF) / 33554432.0f;
            }
            
            // Load fractions into NEON vector
            float32x4_t frac_vec = vld1q_f32(phase_frac);
            
            // Get samples from wave L (current and next positions)
            float s_l0[4], s_l1[4];
            for (int j = 0; j < 4; ++j) {
                s_l0[j] = static_cast<float>(GetWaveSample(entry.ptr_l, phase7[j]));
                s_l1[j] = static_cast<float>(GetWaveSample(entry.ptr_l, (phase7[j] + 1) & 0x7F));
            }
            
            // Get samples from wave R
            float s_r0[4], s_r1[4];
            for (int j = 0; j < 4; ++j) {
                s_r0[j] = static_cast<float>(GetWaveSample(entry.ptr_r, phase7[j]));
                s_r1[j] = static_cast<float>(GetWaveSample(entry.ptr_r, (phase7[j] + 1) & 0x7F));
            }
            
            // Load into NEON vectors
            float32x4_t sl0_vec = vld1q_f32(s_l0);
            float32x4_t sl1_vec = vld1q_f32(s_l1);
            float32x4_t sr0_vec = vld1q_f32(s_r0);
            float32x4_t sr1_vec = vld1q_f32(s_r1);
            
            // Interpolate within waves: s = s0 + (s1 - s0) * frac
            float32x4_t s_l = vmlaq_f32(sl0_vec, vsubq_f32(sl1_vec, sl0_vec), frac_vec);
            float32x4_t s_r = vmlaq_f32(sr0_vec, vsubq_f32(sr1_vec, sr0_vec), frac_vec);
            
            // Blend between waves: sample = s_l + (s_r - s_l) * blend
            float32x4_t sample = vmlaq_f32(s_l, vsubq_f32(s_r, s_l), blend_vec);
            
            // Convert to float output: (sample - 127.5) / 127.5
            sample = vmulq_f32(vsubq_f32(sample, offset_vec), scale_vec);
            
            vst1q_f32(&output[i], sample);
            
            // Advance phase
            phase_ += step_x4;
        }
        
        // Scalar fallback for remaining samples
        for (; i < count; ++i) {
            output[i] = Process();
        }
    }
    
    /**
     * @brief Process block with wave position modulation
     *
     * @param output Output buffer
     * @param count Number of samples
     * @param wave_pos_buffer Per-sample wave positions (0.0-1.0), or nullptr
     * @param base_wave_pos Base wave position if buffer is nullptr
     */
    void ProcessBlockMod(float* output, uint32_t count,
                         const float* wave_pos_buffer, float base_wave_pos) {
        for (uint32_t i = 0; i < count; ++i) {
            if (wave_pos_buffer) {
                SetWavePosition(wave_pos_buffer[i]);
            } else if (i == 0) {
                SetWavePosition(base_wave_pos);
            }
            output[i] = Process();
        }
    }
#else
    // Non-NEON fallback implementations
    void ProcessBlock(float* output, uint32_t count) {
        for (uint32_t i = 0; i < count; ++i) {
            output[i] = Process();
        }
    }
    
    void ProcessBlockMod(float* output, uint32_t count,
                         const float* wave_pos_buffer, float base_wave_pos) {
        for (uint32_t i = 0; i < count; ++i) {
            if (wave_pos_buffer) {
                SetWavePosition(wave_pos_buffer[i]);
            } else if (i == 0) {
                SetWavePosition(base_wave_pos);
            }
            output[i] = Process();
        }
    }
#endif // USE_NEON
};

/**
 * @brief Simple PPG oscillator using external wave data
 *
 * A lighter-weight version that takes wave pointers directly
 * rather than managing a full wavetable internally.
 */
class SimplePpgOsc {
public:
    SimplePpgOsc() : phase_(0), step_(0) {}
    
    void Init(float sample_rate) {
        sample_rate_ = sample_rate;
        phase_ = 0;
        step_ = 0;
        wave_l_ = nullptr;
        wave_r_ = nullptr;
        blend_ = 0.0f;
    }
    
    void Reset() { phase_ = 0; }
    
    void SetFrequency(float freq) {
        step_ = static_cast<uint32_t>(freq / sample_rate_ * 4294967296.0f);
    }
    
    /**
     * @brief Set waves for morphing
     * @param wave_l Left wave (64 samples, 8-bit)
     * @param wave_r Right wave (64 samples, 8-bit)
     * @param blend Blend factor (0.0 = wave_l, 1.0 = wave_r)
     */
    void SetWaves(const uint8_t* wave_l, const uint8_t* wave_r, float blend) {
        wave_l_ = wave_l;
        wave_r_ = wave_r;
        blend_ = blend;
        if (blend_ < 0.0f) blend_ = 0.0f;
        if (blend_ > 1.0f) blend_ = 1.0f;
    }
    
    float Process() {
        if (!wave_l_) return 0.0f;
        
        const uint8_t phase7 = static_cast<uint8_t>(phase_ >> 25);
        const float phase_frac = static_cast<float>(phase_ & 0x1FFFFFF) / 33554432.0f;
        const uint8_t phase7_next = (phase7 + 1) & 0x7F;
        
        // Get samples with mirroring
        float s_l0 = GetSample(wave_l_, phase7);
        float s_l1 = GetSample(wave_l_, phase7_next);
        float s_l = s_l0 + (s_l1 - s_l0) * phase_frac;
        
        float sample = s_l;
        
        if (wave_r_ && blend_ > 0.0f) {
            float s_r0 = GetSample(wave_r_, phase7);
            float s_r1 = GetSample(wave_r_, phase7_next);
            float s_r = s_r0 + (s_r1 - s_r0) * phase_frac;
            sample = s_l + (s_r - s_l) * blend_;
        }
        
        phase_ += step_;
        
        return (sample - 127.5f) / 127.5f;
    }
    
#ifdef USE_NEON
    /**
     * @brief Process a block of samples using NEON SIMD
     *
     * @param output Output buffer (must be at least 'count' floats)
     * @param count Number of samples to generate
     */
    void ProcessBlock(float* output, uint32_t count) {
        if (!wave_l_) {
            for (uint32_t i = 0; i < count; ++i) output[i] = 0.0f;
            return;
        }
        
        const bool has_blend = (wave_r_ && blend_ > 0.0f);
        
        // NEON constants
        float32x4_t blend_vec = vdupq_n_f32(blend_);
        float32x4_t offset_vec = vdupq_n_f32(127.5f);
        float32x4_t scale_vec = vdupq_n_f32(1.0f / 127.5f);
        
        uint32_t step_x4 = step_ << 2;
        
        uint32_t i = 0;
        for (; i + 4 <= count; i += 4) {
            // Calculate 4 phases
            uint32_t phases[4] = {
                phase_,
                phase_ + step_,
                phase_ + (step_ << 1),
                phase_ + step_ + (step_ << 1)
            };
            
            // Extract phase indices and fractions
            uint8_t phase7[4];
            float phase_frac[4];
            for (int j = 0; j < 4; ++j) {
                phase7[j] = static_cast<uint8_t>(phases[j] >> 25);
                phase_frac[j] = static_cast<float>(phases[j] & 0x1FFFFFF) / 33554432.0f;
            }
            
            float32x4_t frac_vec = vld1q_f32(phase_frac);
            
            // Get wave L samples
            float sl0[4], sl1[4];
            for (int j = 0; j < 4; ++j) {
                sl0[j] = GetSample(wave_l_, phase7[j]);
                sl1[j] = GetSample(wave_l_, (phase7[j] + 1) & 0x7F);
            }
            
            float32x4_t sl0_vec = vld1q_f32(sl0);
            float32x4_t sl1_vec = vld1q_f32(sl1);
            
            // Interpolate wave L
            float32x4_t s_l = vmlaq_f32(sl0_vec, vsubq_f32(sl1_vec, sl0_vec), frac_vec);
            
            float32x4_t sample;
            if (has_blend) {
                // Get wave R samples
                float sr0[4], sr1[4];
                for (int j = 0; j < 4; ++j) {
                    sr0[j] = GetSample(wave_r_, phase7[j]);
                    sr1[j] = GetSample(wave_r_, (phase7[j] + 1) & 0x7F);
                }
                
                float32x4_t sr0_vec = vld1q_f32(sr0);
                float32x4_t sr1_vec = vld1q_f32(sr1);
                
                // Interpolate wave R
                float32x4_t s_r = vmlaq_f32(sr0_vec, vsubq_f32(sr1_vec, sr0_vec), frac_vec);
                
                // Blend: s_l + (s_r - s_l) * blend
                sample = vmlaq_f32(s_l, vsubq_f32(s_r, s_l), blend_vec);
            } else {
                sample = s_l;
            }
            
            // Convert to output: (sample - 127.5) / 127.5
            sample = vmulq_f32(vsubq_f32(sample, offset_vec), scale_vec);
            vst1q_f32(&output[i], sample);
            
            phase_ += step_x4;
        }
        
        // Scalar fallback
        for (; i < count; ++i) {
            output[i] = Process();
        }
    }
#else
    void ProcessBlock(float* output, uint32_t count) {
        for (uint32_t i = 0; i < count; ++i) {
            output[i] = Process();
        }
    }
#endif // USE_NEON
    
private:
    float sample_rate_;
    uint32_t phase_;
    uint32_t step_;
    const uint8_t* wave_l_;
    const uint8_t* wave_r_;
    float blend_;
    
    float GetSample(const uint8_t* wave, uint8_t phase7) const {
        if (phase7 < 64) {
            return static_cast<float>(wave[phase7]);
        } else {
            return static_cast<float>(255 - wave[127 - phase7]);
        }
    }
};

} // namespace common
