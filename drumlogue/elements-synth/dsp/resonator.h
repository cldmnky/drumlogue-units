/*
 * Resonator - Modal and Karplus-Strong string models
 * Part of Modal Synth for Drumlogue
 */

#pragma once

#include "dsp_core.h"

namespace modal {

// ============================================================================
// Modal Resonator - 32 bandpass modes (like Elements)
// ============================================================================

class Mode {
public:
    Mode() : x1_(0), x2_(0), y1_(0), y2_(0), b0_(0), a1_(0), a2_(0) {}
    
    void SetFrequencyAndQ(float freq, float q) {
        freq = Clamp(freq, 20.0f, kSampleRate * 0.45f);
        q = Clamp(q, 1.0f, 500.0f);
        
        float w0 = kTwoPi * freq / kSampleRate;
        float sin_w0 = std::sin(w0);
        float cos_w0 = std::cos(w0);
        float alpha = sin_w0 / (2.0f * q);
        
        float norm = 1.0f / (1.0f + alpha);
        b0_ = alpha * norm;
        a1_ = -2.0f * cos_w0 * norm;
        a2_ = (1.0f - alpha) * norm;
    }
    
    float Process(float in) {
        // Protect against NaN input
        if (in != in) in = 0.0f;
        
        float out = b0_ * (in - x2_) - a1_ * y1_ - a2_ * y2_;
        x2_ = x1_;
        x1_ = in;
        y2_ = y1_;
        y1_ = out;
        
        // Stability check - reset if state becomes unstable (NaN or too large)
        if (y1_ != y1_ || y1_ > 1e4f || y1_ < -1e4f) {
            Reset();
            return 0.0f;
        }
        return out;
    }
    
    void Reset() { x1_ = x2_ = y1_ = y2_ = 0.0f; }
    
private:
    float x1_, x2_, y1_, y2_;
    float b0_, a1_, a2_;
};

class Resonator {
public:
    // Mode ratio presets
    enum Structure {
        kString,      // Harmonic series (1, 2, 3, 4...)
        kBeam,        // Beam/bar (1, 2.76, 5.40, 8.93...)
        kPlate,       // Circular plate (1, 1.59, 2.14, 2.65...)
        kBell         // Inharmonic bell-like
    };
    
    Resonator() {
        SetStructure(0.0f);
        SetBrightness(0.5f);
        SetDamping(0.5f);
        SetPosition(0.5f);
        base_freq_ = 220.0f;
        needs_update_ = true;
        Update();  // Force initial coefficient calculation
    }
    
    void SetFrequency(float freq) {
        base_freq_ = Clamp(freq, 20.0f, 8000.0f);
        needs_update_ = true;
    }
    
    void SetStructure(float s) {
        structure_ = Clamp(s, 0.0f, 1.0f);
        needs_update_ = true;
    }
    
    void SetBrightness(float b) {
        brightness_ = Clamp(b, 0.0f, 1.0f);
        needs_update_ = true;
    }
    
    void SetDamping(float d) {
        damping_ = Clamp(d, 0.0f, 1.0f);
        needs_update_ = true;
    }
    
    void SetPosition(float p) {
        position_ = Clamp(p, 0.0f, 1.0f);
        needs_update_ = true;
    }
    
    // Force update even if needs_update_ is false (for bulk parameter changes)
    void ForceUpdate() {
        needs_update_ = true;
        Update();
    }
    
    void Update() {
        if (!needs_update_) return;
        needs_update_ = false;
        
        // Mode ratios for different structures (16 modes)
        // String (harmonic series)
        static const float string_ratios[16] = {
            1.0f, 2.0f, 3.0f, 4.0f, 5.0f, 6.0f, 7.0f, 8.0f,
            9.0f, 10.0f, 11.0f, 12.0f, 13.0f, 14.0f, 15.0f, 16.0f
        };
        
        // Beam/bar (stiff string, inharmonic)
        static const float beam_ratios[16] = {
            1.0f, 2.76f, 5.40f, 8.93f, 13.34f, 18.64f, 24.81f, 31.87f,
            39.81f, 48.63f, 58.33f, 68.91f, 80.37f, 92.71f, 105.93f, 120.03f
        };
        
        // Membrane/plate (circular)
        static const float plate_ratios[16] = {
            1.0f, 1.59f, 2.14f, 2.30f, 2.65f, 2.92f, 3.16f, 3.50f,
            3.60f, 3.89f, 4.06f, 4.15f, 4.35f, 4.60f, 4.83f, 5.10f
        };
        
        // Bell-like (inharmonic)
        static const float bell_ratios[16] = {
            1.0f, 1.18f, 1.47f, 1.57f, 2.0f, 2.09f, 2.44f, 2.56f,
            3.0f, 3.14f, 3.5f, 4.0f, 4.13f, 4.87f, 5.2f, 5.95f
        };
        
        // Interpolate between structures based on structure_ parameter
        // 0.0 = string, 0.33 = beam, 0.66 = plate, 1.0 = bell
        const float* ratios_a;
        const float* ratios_b;
        float blend;
        
        if (structure_ < 0.33f) {
            ratios_a = string_ratios;
            ratios_b = beam_ratios;
            blend = structure_ * 3.0f;
        } else if (structure_ < 0.66f) {
            ratios_a = beam_ratios;
            ratios_b = plate_ratios;
            blend = (structure_ - 0.33f) * 3.0f;
        } else {
            ratios_a = plate_ratios;
            ratios_b = bell_ratios;
            blend = (structure_ - 0.66f) * 3.0f;
        }
        
        // Base Q from damping (inverted - low damping = high Q)
        // Conservative Q range for stability
        float base_q = 10.0f + (1.0f - damping_) * 40.0f;  // 10 to 50
        
        for (int i = 0; i < kNumModes; ++i) {
            // Interpolate ratio
            float ratio = ratios_a[i] * (1.0f - blend) + ratios_b[i] * blend;
            float freq = base_freq_ * ratio;
            
            // Q decreases for higher modes (brightness control)
            float q_falloff = 1.0f + (float)i * brightness_ * 0.2f;
            float mode_q = base_q / q_falloff;
            
            // Prevent super high frequencies - more conservative limit
            if (freq > kSampleRate * 0.4f) {
                freq = kSampleRate * 0.4f;
                mode_q = 5.0f;  // Heavy damping for aliased modes
            }
            
            modes_[i].SetFrequencyAndQ(freq, mode_q);
            
            // Amplitude based on excitation position (like Elements pickup position)
            float pos_rad = position_ * kPi;
            float amp = std::sin(pos_rad * (float)(i + 1));
            amp *= amp;  // Square for smoother response
            
            // Natural falloff for higher modes
            amp /= (1.0f + (float)i * 0.15f);
            
            amplitudes_[i] = amp;
        }
    }
    
    void SetSpace(float space) {
        space_ = Clamp(space, 0.0f, 1.0f);
    }
    
    // Process with stereo output (Elements-style: different mode amplitudes per channel)
    void Process(float excitation, float& out_center, float& out_side) {
        // Protect input
        if (excitation != excitation) excitation = 0.0f;
        if (excitation > 10.0f) excitation = 10.0f;
        if (excitation < -10.0f) excitation = -10.0f;
        
        // Update LFO for stereo modulation (slow ~0.5Hz at 48kHz)
        lfo_phase_ += 0.5f / kSampleRate;
        if (lfo_phase_ >= 1.0f) lfo_phase_ -= 1.0f;
        
        // Triangle LFO
        float lfo = lfo_phase_ > 0.5f ? 1.0f - lfo_phase_ : lfo_phase_;
        lfo *= 4.0f;  // Scale to 0-2 range
        
        float sum_center = 0.0f;
        float sum_side = 0.0f;
        
        // Stereo position offset (modulated by LFO)
        float stereo_offset = space_ * 0.5f + lfo * space_ * 0.25f;
        
        for (int i = 0; i < kNumModes; ++i) {
            float mode_out = modes_[i].Process(excitation);
            
            // Center channel uses normal position-based amplitude
            sum_center += mode_out * amplitudes_[i];
            
            // Side channel uses offset position (different modes emphasized)
            float side_pos = position_ + stereo_offset;
            float side_rad = side_pos * kPi;
            float side_amp = std::sin(side_rad * (float)(i + 1));
            side_amp *= side_amp;
            side_amp /= (1.0f + (float)i * 0.15f);
            
            sum_side += mode_out * side_amp;
        }
        
        // Normalize and output - boosted gain for audible volume
        out_center = sum_center * 5.0f;
        out_side = (sum_side - sum_center) * 5.0f * space_;
        
        // Final safety clamp
        if (out_center > 5.0f) out_center = 5.0f;
        if (out_center < -5.0f) out_center = -5.0f;
        if (out_side > 5.0f) out_side = 5.0f;
        if (out_side < -5.0f) out_side = -5.0f;
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
        }
        lfo_phase_ = 0.0f;
    }
    
private:
    Mode modes_[kNumModes];
    float amplitudes_[kNumModes];
    float base_freq_;
    float structure_, brightness_, damping_, position_;
    float space_ = 0.5f;
    float lfo_phase_ = 0.0f;
    bool needs_update_ = true;
};

// ============================================================================
// Karplus-Strong String Model (alternative to modal)
// ============================================================================

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
    
    float Process(float excitation) {
        // Protect against NaN input
        if (excitation != excitation) excitation = 0.0f;
        
        // Read from delay line (linear interpolation)
        float read_pos = (float)write_ptr_ - delay_samples_;
        if (read_pos < 0) read_pos += kMaxDelay;
        
        int read_idx = (int)read_pos;
        float frac = read_pos - (float)read_idx;
        
        int idx0 = read_idx & (kMaxDelay - 1);
        int idx1 = (read_idx + 1) & (kMaxDelay - 1);
        
        float delayed = delay_[idx0] * (1.0f - frac) + delay_[idx1] * frac;
        
        // Low-pass filter (damping + brightness)
        filtered_ = filtered_ * filter_coeff_ + delayed * (1.0f - filter_coeff_);
        
        // Stability check (NaN != NaN trick)
        if (filtered_ != filtered_ || filtered_ > 1e4f || filtered_ < -1e4f) {
            Reset();
            return 0.0f;
        }
        
        // Feedback with damping
        float feedback = filtered_ * (1.0f - damping_ * 0.3f);
        
        // Write to delay line (excitation + feedback)
        delay_[write_ptr_] = excitation + feedback;
        write_ptr_ = (write_ptr_ + 1) & (kMaxDelay - 1);
        
        return filtered_;
    }
    
private:
    void UpdateCoefficients() {
        delay_samples_ = kSampleRate / freq_;
        if (delay_samples_ > kMaxDelay - 2) delay_samples_ = kMaxDelay - 2;
        if (delay_samples_ < 2) delay_samples_ = 2;
        
        // Brightness controls filter cutoff
        filter_coeff_ = 0.1f + (1.0f - brightness_) * 0.85f;
    }
    
    float delay_[kMaxDelay];
    int write_ptr_;
    float delay_samples_;
    float filtered_ = 0.0f;
    float filter_coeff_;
    float freq_, damping_, brightness_;
};

} // namespace modal
