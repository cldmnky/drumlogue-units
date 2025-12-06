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
    Mode() : state_1_(0.0f), state_2_(0.0f), g_(0.0f), r_(0.01f), h_(1.0f) {}
    
    // Set frequency (normalized 0-0.5) and Q directly
    void SetFrequencyAndQ(float freq, float q) {
        freq = Clamp(freq, 20.0f, kSampleRate * 0.49f);
        q = Clamp(q, 0.5f, 500.0f);
        
        // Normalized frequency for coefficient calculation
        float f = freq / kSampleRate;
        
        // Use fast tan approximation for g coefficient
        g_ = FastTan(f);
        
        // r = 1/Q for resonance
        r_ = 1.0f / q;
        
        // h = 1 / (1 + r*g + g*g) - normalization factor
        h_ = 1.0f / (1.0f + r_ * g_ + g_ * g_);
    }
    
    // Set coefficients directly (for clock-divider optimization)
    void SetCoefficients(float g, float r) {
        g_ = g;
        r_ = r;
        h_ = 1.0f / (1.0f + r_ * g_ + g_ * g_);
    }
    
    // Get g coefficient (for bowed mode sharing)
    float g() const { return g_; }
    
    // Process one sample - returns bandpass output
    float Process(float in) {
        // Protect against NaN input
        if (in != in) in = 0.0f;
        
        // Zero-delay feedback SVF
        float hp = (in - r_ * state_1_ - g_ * state_1_ - state_2_) * h_;
        float bp = g_ * hp + state_1_;
        state_1_ = g_ * hp + bp;
        float lp = g_ * bp + state_2_;
        state_2_ = g_ * bp + lp;
        
        // Stability check - reset if state becomes unstable
        if (state_1_ != state_1_ || state_1_ > 1e4f || state_1_ < -1e4f) {
            Reset();
            return 0.0f;
        }
        
        return bp;  // Bandpass for modal resonance
    }
    
    // Process with normalized bandpass output (for bowing)
    float ProcessNormalized(float in) {
        if (in != in) in = 0.0f;
        
        float hp = (in - r_ * state_1_ - g_ * state_1_ - state_2_) * h_;
        float bp = g_ * hp + state_1_;
        state_1_ = g_ * hp + bp;
        float lp = g_ * bp + state_2_;
        state_2_ = g_ * bp + lp;
        
        if (state_1_ != state_1_ || state_1_ > 1e4f || state_1_ < -1e4f) {
            Reset();
            return 0.0f;
        }
        
        // Normalize by r to maintain consistent amplitude across Q values
        return bp * r_;
    }
    
    void Reset() { state_1_ = state_2_ = 0.0f; }
    
private:
    float state_1_, state_2_;  // Filter state
    float g_, r_, h_;          // Coefficients
};

// ============================================================================
// Bowed Mode - Bandpass filter + Delay line for banded waveguide synthesis
// Each bowed mode simulates a string resonating at a specific partial
// The delay line provides the waveguide behavior, filter shapes the resonance
// ============================================================================

class BowedMode {
public:
    BowedMode() : g_(0.1f), r_(0.01f), h_(1.0f), state_1_(0.0f), state_2_(0.0f) {
        delay_.Init();
    }
    
    void Init() {
        delay_.Init();
        state_1_ = state_2_ = 0.0f;
        g_ = 0.1f;
        r_ = 0.01f;
        h_ = 1.0f;
    }
    
    // Set g coefficient from main mode and higher Q for bowing
    void SetGAndQ(float g, float q) {
        g_ = g;
        r_ = 1.0f / Clamp(q, 0.5f, 2000.0f);
        h_ = 1.0f / (1.0f + r_ * g_ + g_ * g_);
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
        // Bandpass filter (normalized output for consistent level)
        float hp = (in - r_ * state_1_ - g_ * state_1_ - state_2_) * h_;
        float bp = g_ * hp + state_1_;
        state_1_ = g_ * hp + bp;
        float lp = g_ * bp + state_2_;
        state_2_ = g_ * bp + lp;
        
        // Stability check
        if (state_1_ != state_1_ || state_1_ > 1e4f || state_1_ < -1e4f) {
            state_1_ = state_2_ = 0.0f;
            bp = 0.0f;
        }
        
        // Write normalized bandpass to delay
        delay_.Write(bp * r_);
    }
    
    void Reset() {
        delay_.Reset();
        state_1_ = state_2_ = 0.0f;
    }
    
private:
    DelayLine<kMaxDelayLineSize> delay_;
    float g_, r_, h_;
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
        
        // Initialize all modes
        for (int i = 0; i < kNumModes; ++i) {
            modes_[i].Reset();
            cached_g_[i] = 0.1f;
            cached_r_[i] = 0.01f;
        }
        
        // Initialize bowed modes
        for (size_t i = 0; i < kMaxBowedModes; ++i) {
            bowed_modes_[i].Init();
        }
        
        ComputeFilters();
    }
    
    void SetFrequency(float freq) {
        frequency_ = Clamp(freq, 20.0f, 8000.0f) / kSampleRate;
    }
    
    void SetGeometry(float geometry) {
        geometry_ = Clamp(geometry, 0.0f, 1.0f);
    }
    
    // Alias for compatibility
    void SetStructure(float s) { SetGeometry(s); }
    
    void SetBrightness(float brightness) {
        brightness_ = Clamp(brightness, 0.0f, 1.0f);
    }
    
    void SetDamping(float damping) {
        damping_ = Clamp(damping, 0.0f, 1.0f);
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
        
        // Process all active normal modes
        for (size_t i = 0; i < num_modes; ++i) {
            float s = modes_[i].Process(excitation);
            float amp = amplitudes.Next();
            float aux_amp = aux_amplitudes.Next();
            sum_center += s * amp;
            sum_side += s * aux_amp;
        }
        
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
        out_center = sum_center * 8.0f;  // Gain compensation
        out_side = (sum_side - sum_center) * 8.0f * space_;
        
        // Safety clamp
        out_center = Clamp(out_center, -5.0f, 5.0f);
        out_side = Clamp(out_side, -5.0f, 5.0f);
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
        float base_q = GetQFromDamping(damping_);
        
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
    
    // Compute filters with clock divider optimization
    // Returns number of active modes
    size_t ComputeFilters() {
        ++clock_divider_;
        
        size_t num_modes = 0;
        float stiffness = GetStiffness(geometry_);
        float harmonic = frequency_;
        float stretch_factor = 1.0f;
        float base_q = GetQFromDamping(damping_);
        
        // Brightness attenuation
        float brightness_attenuation = 1.0f - geometry_;
        brightness_attenuation *= brightness_attenuation;
        brightness_attenuation *= brightness_attenuation;
        brightness_attenuation *= brightness_attenuation;
        float brightness = brightness_ * (1.0f - 0.2f * brightness_attenuation);
        float q_loss = brightness * (2.0f - brightness) * 0.85f + 0.15f;
        float q_loss_damping_rate = geometry_ * (2.0f - geometry_) * 0.1f;
        
        for (size_t i = 0; i < static_cast<size_t>(kNumModes); ++i) {
            // Clock divider: update first 24 modes every time (2kHz @ 48kHz/24 samples)
            // Higher modes updated at half rate
            bool update = i <= 24 || ((i & 1) == (clock_divider_ & 1));
            
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
        
        return num_modes > 0 ? num_modes : 1;
    }
    
    Mode modes_[kNumModes];
    BowedMode bowed_modes_[kMaxBowedModes];  // Banded waveguides for bowing
    float cached_g_[kNumModes];  // Cached g coefficients
    float cached_r_[kNumModes];  // Cached r coefficients
    
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
        damping_filter_.Init();
        dispersion_filter_.Reset();
        UpdateCoefficients();
    }
    
    void SetFrequency(float freq) {
        freq_ = Clamp(freq, 20.0f, 4000.0f);
        UpdateCoefficients();
    }
    
    void SetDamping(float d) {
        damping_ = Clamp(d, 0.0f, 1.0f);
        // Configure damping filter with smooth transition
        float feedback = 0.998f - damping_ * 0.05f;
        damping_filter_.Configure(feedback, brightness_, 48);  // ~1ms transition
    }
    
    void SetBrightness(float b) {
        brightness_ = Clamp(b, 0.0f, 1.0f);
        float feedback = 0.998f - damping_ * 0.05f;
        damping_filter_.Configure(feedback, brightness_, 48);
    }
    
    // Set dispersion amount (0 = none, 1 = piano-like)
    void SetDispersion(float d) {
        dispersion_ = Clamp(d, 0.0f, 1.0f);
        dispersion_filter_.Configure(freq_, dispersion_);
    }
    
    float Process(float excitation) {
        // Protect against NaN input
        if (excitation != excitation) excitation = 0.0f;
        
        // Read from delay line (linear interpolation)
        float read_pos = static_cast<float>(write_ptr_) - delay_samples_;
        if (read_pos < 0.0f) read_pos += kMaxDelay;
        
        int read_idx = static_cast<int>(read_pos);
        float frac = read_pos - static_cast<float>(read_idx);
        
        int idx0 = read_idx & (kMaxDelay - 1);
        int idx1 = (read_idx + 1) & (kMaxDelay - 1);
        
        float delayed = delay_[idx0] * (1.0f - frac) + delay_[idx1] * frac;
        
        // Apply damping filter (3-tap FIR with feedback)
        float filtered = damping_filter_.Process(delayed);
        
        // Apply dispersion (piano-like inharmonicity)
        filtered = dispersion_filter_.Process(filtered);
        
        // Stability check
        if (filtered != filtered || filtered > 1e4f || filtered < -1e4f) {
            Reset();
            return 0.0f;
        }
        
        // Write to delay line (excitation + feedback)
        delay_[write_ptr_] = excitation + filtered;
        write_ptr_ = (write_ptr_ + 1) & (kMaxDelay - 1);
        
        return filtered * 3.0f;
    }
    
private:
    void UpdateCoefficients() {
        delay_samples_ = kSampleRate / freq_;
        if (delay_samples_ > kMaxDelay - 2) delay_samples_ = kMaxDelay - 2;
        if (delay_samples_ < 2) delay_samples_ = 2;
        // Update dispersion filter when frequency changes
        dispersion_filter_.Configure(freq_, dispersion_);
    }
    
    float delay_[kMaxDelay];
    int write_ptr_;
    float delay_samples_;
    DampingFilter damping_filter_;
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
    // Strings 1-4: slightly detuned for chorus/shimmer effect
    static constexpr float kDetuning[kNumStrings] = {
        0.0f,    // Main string
        -7.0f,   // Slightly flat
        7.0f,    // Slightly sharp
        -12.0f,  // More flat (creates beating)
        12.0f    // More sharp
    };
    
    // Amplitude ratios for each string
    static constexpr float kAmplitude[kNumStrings] = {
        1.0f,    // Main string full volume
        0.5f,    // Sympathetic strings quieter
        0.5f,
        0.3f,
        0.3f
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
        
        // Sympathetic strings get reduced excitation (they resonate)
        float sympathetic_input = excitation * 0.3f;
        for (int i = 1; i < kNumStrings; ++i) {
            out += strings_[i].Process(sympathetic_input) * kAmplitude[i];
        }
        
        // Normalize output
        return out * 0.5f;
    }
    
private:
    void UpdateFrequencies() {
        for (int i = 0; i < kNumStrings; ++i) {
            // Convert cents to frequency ratio: ratio = 2^(cents/1200)
            float cents = kDetuning[i] * detune_amount_;
            float ratio = 1.0f + cents * (1.0f / 1200.0f) * 0.693147f;  // Approximation
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
