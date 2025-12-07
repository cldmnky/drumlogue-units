/*
 * Exciter - Bow, Blow, Strike + Granular excitation
 * Part of Modal Synth for Drumlogue
 */

#pragma once

#include "dsp_core.h"
#include "tube.h"
#include "../samples.h"

using elements_samples::SamplePlayer;

namespace modal {

// ============================================================================
// Granular Sample Player (Elements-style)
// Plays samples with random restart points for textural excitation
// ============================================================================

class GranularPlayer {
public:
    GranularPlayer() {
        Reset();
    }
    
    void Reset() {
        phase_ = 0;
        sample_idx_ = 0;
        position_ = 0.5f;
        pitch_ = 1.0f;
        density_ = 0.5f;
    }
    
    void SetSample(int idx) {
        if (idx >= 0 && idx < static_cast<int>(elements_samples::NUM_SAMPLES)) {
            sample_idx_ = idx;
        }
    }
    
    void SetPosition(float pos) {
        position_ = Clamp(pos, 0.0f, 1.0f);
    }
    
    void SetPitch(float pitch) {
        // Map 0-1 to pitch range: -1 to +1 octaves
        pitch_ = SemitonesToRatio((pitch - 0.5f) * 24.0f);
    }
    
    void SetDensity(float density) {
        density_ = Clamp(density, 0.0f, 1.0f);
    }
    
    float Process() {
        const int16_t* data = elements_samples::sample_ptrs[sample_idx_];
        uint32_t length = elements_samples::sample_sizes[sample_idx_];
        
        // Calculate restart probability based on density
        // Higher density = more frequent random restarts = more granular texture
        uint32_t restart_prob = static_cast<uint32_t>(
            density_ * 0.02f * 4294967296.0f);
        
        // Calculate restart point based on position
        uint32_t restart_point = static_cast<uint32_t>(
            position_ * (length - 1)) << 16;
        
        // Phase increment based on pitch
        uint32_t phase_inc = static_cast<uint32_t>(pitch_ * 65536.0f);
        
        // Read sample with interpolation
        uint32_t idx = phase_ >> 16;
        if (idx >= length - 1) {
            phase_ = restart_point;
            idx = phase_ >> 16;
        }
        
        float frac = static_cast<float>(phase_ & 0xFFFF) / 65536.0f;
        float s1 = static_cast<float>(data[idx]) / 32768.0f;
        float s2 = static_cast<float>(data[idx + 1]) / 32768.0f;
        
        // Advance phase
        phase_ += phase_inc;
        
        // Random restart for granular texture
        if (noise_state_ < restart_prob) {
            phase_ = restart_point;
        }
        
        // Update noise state (simple xorshift)
        noise_state_ ^= noise_state_ << 13;
        noise_state_ ^= noise_state_ >> 17;
        noise_state_ ^= noise_state_ << 5;
        
        return s1 + (s2 - s1) * frac;
    }
    
private:
    uint32_t phase_ = 0;
    uint32_t noise_state_ = 12345;
    int sample_idx_ = 0;
    float position_ = 0.5f;
    float pitch_ = 1.0f;
    float density_ = 0.5f;
};

// ============================================================================
// Exciter - Bow, Blow, Strike + Granular + Plectrum + Particles
// ============================================================================

class Exciter {
public:
    enum StrikeMode {
        STRIKE_MODE_SAMPLE,     // Normal sample playback
        STRIKE_MODE_GRANULAR,   // Granular texture
        STRIKE_MODE_NOISE,      // Pure noise
        STRIKE_MODE_PLECTRUM,   // Guitar pick (delayed release)
        STRIKE_MODE_PARTICLES   // Random impulse train (rain/gravel)
    };
    
    Exciter() {
        Reset();
    }
    
    void Reset() {
        bow_level_ = 0.0f;
        blow_level_ = 0.0f;
        strike_level_ = 1.0f;
        timbre_ = 0.5f;
        strike_amp_ = 0.0f;
        strike_mode_ = STRIKE_MODE_SAMPLE;
        bow_filter_.Reset();
        blow_filter_.Reset();
        strike_filter_.Reset();
        tube_.Init();
        sample_player_.SetPitch(1.0f);
        granular_player_.Reset();
        blow_frequency_ = 440.0f;
        blow_envelope_ = 0.0f;
        
        // Plectrum mode state
        plectrum_delay_ = 0;
        plectrum_damp_ = 0.0f;
        plectrum_impulse_ = 0.0f;
        
        // Particles mode state
        particle_state_ = 0.5f;
        particle_range_ = 1.0f;
        particle_delay_ = 0;
    }
    
    void SetBow(float level) { bow_level_ = Clamp(level, 0.0f, 1.0f); }
    void SetBlow(float level) { blow_level_ = Clamp(level, 0.0f, 1.0f); }
    void SetStrike(float level) { strike_level_ = Clamp(level, 0.0f, 1.0f); }
    
    // Set blow frequency (for tube resonance to track pitch)
    void SetBlowFrequency(float freq) {
        blow_frequency_ = Clamp(freq, 20.0f, 8000.0f);
    }
    
    void SetStrikeSample(int idx) {
        // 12 variants: each of 6 samples has 2 timbre variations
        // 0=SOFT DK, 1=SOFT BR, 2=MED DK, 3=MED BR, 4=HARD DK, 5=HARD BR
        // 6=PLEC DK, 7=PLEC BR, 8=STICK DK, 9=STICK BR, 10=BOW DK, 11=BOW BR
        idx = Clamp(idx, 0, 11);
        int sample_idx = idx / 2;  // 0-5 sample selection
        bool bright = (idx & 1);   // odd = bright variant
        
        sample_player_.SetSample(sample_idx);
        granular_player_.SetSample(sample_idx);
        
        // Set timbre: dark variants 0.15-0.45, bright variants 0.55-0.95
        float base_timbre = (float)sample_idx * 0.1f;
        float timbre = bright ? (0.55f + base_timbre) : (0.15f + base_timbre * 0.6f);
        SetStrikeTimbre(timbre);
    }
    
    void SetStrikeMode(StrikeMode mode) { strike_mode_ = mode; }
    void SetStrikeMode(int mode) { 
        strike_mode_ = static_cast<StrikeMode>(Clamp(mode, 0, 4)); 
    }
    
    void SetBowTimbre(float t) { 
        bow_timbre_ = Clamp(t, 0.0f, 1.0f);
        bow_filter_.SetFrequency(200.0f + bow_timbre_ * 4000.0f);
    }
    
    void SetBlowTimbre(float t) {
        blow_timbre_ = Clamp(t, 0.0f, 1.0f);
        blow_filter_.SetFrequency(500.0f + blow_timbre_ * 8000.0f);
        blow_filter_.SetResonance(1.0f + blow_timbre_ * 3.0f);
    }
    
    void SetStrikeTimbre(float t) {
        timbre_ = Clamp(t, 0.0f, 1.0f);
        strike_filter_.SetFrequency(500.0f + timbre_ * 12000.0f);
        
        // Set granular pitch from timbre
        granular_player_.SetPitch(timbre_);
        
        // Sample playback pitch variation
        float pitch = 0.8f + timbre_ * 0.4f;
        sample_player_.SetPitch(pitch);
    }
    
    // Granular-specific controls
    void SetGranularPosition(float pos) {
        granular_player_.SetPosition(pos);
    }
    
    void SetGranularDensity(float density) {
        granular_player_.SetDensity(density);
    }
    
    void Trigger() {
        strike_amp_ = strike_level_;
        if (strike_mode_ == STRIKE_MODE_SAMPLE && strike_level_ > 0.01f) {
            sample_player_.Trigger();
        }
        // Reset blow envelope for new note
        blow_envelope_ = 0.0f;
        
        // Plectrum: initial negative impulse, then delayed positive release
        if (strike_mode_ == STRIKE_MODE_PLECTRUM) {
            // Delay based on timbre (0-4096 samples, ~0-85ms at 48kHz)
            plectrum_delay_ = static_cast<uint32_t>(4096.0f * timbre_ * timbre_) + 64;
            plectrum_damp_ = 0.0f;
            plectrum_impulse_ = -strike_level_ * (0.05f + 0.2f);  // Initial negative pluck
        }
        
        // Particles: initialize random state
        if (strike_mode_ == STRIKE_MODE_PARTICLES) {
            // Random initial particle state
            particle_state_ = noise_.Next() * 0.5f + 0.5f;
            particle_state_ = 1.0f - 0.6f * particle_state_ * particle_state_;
            particle_delay_ = 0;
            particle_range_ = 1.0f;
        }
    }
    
    float Process() {
        float out = 0.0f;
        
        // BOW: Continuous friction noise
        if (bow_level_ > 0.001f) {
            float bow_noise = noise_.Next();
            float bow_sig = bow_filter_.ProcessLowPass(bow_noise);
            bow_sig = FastTanh(bow_sig * 2.0f) * bow_level_;
            out += bow_sig * 0.5f;
        }
        
        // BLOW: Turbulent air noise through tube waveguide
        if (blow_level_ > 0.001f) {
            // Smooth envelope for breath
            float target_envelope = blow_level_;
            blow_envelope_ += (target_envelope - blow_envelope_) * 0.001f;
            
            // Generate breath noise
            float blow_noise = noise_.Next();
            
            // Pre-filter for breath character
            float breath_mod = 1.0f + noise_.NextFiltered(0.999f) * 0.3f;
            float blow_sig = blow_filter_.ProcessBandPass(blow_noise);
            blow_sig *= breath_mod;
            
            // Process through tube waveguide for formant resonance
            float tube_out = tube_.Process(blow_sig, blow_frequency_, 
                                           blow_envelope_, 
                                           1.0f - blow_timbre_ * 0.5f,  // damping
                                           blow_timbre_);                // timbre
            
            out += tube_out * 0.7f;
        } else {
            // Decay envelope when not blowing
            blow_envelope_ *= 0.999f;
        }
        
        // STRIKE: Multiple modes
        if (strike_level_ > 0.001f) {
            float strike_sig = 0.0f;
            
            switch (strike_mode_) {
                case STRIKE_MODE_SAMPLE:
                    // Normal sample playback
                    if (sample_player_.IsPlaying()) {
                        strike_sig = sample_player_.Process() * strike_level_;
                    }
                    // Add noise tail
                    if (strike_amp_ > 0.001f) {
                        float noise_sig = strike_filter_.ProcessLowPass(
                            noise_.Next() * strike_amp_);
                        float blend = sample_player_.IsPlaying() ? 0.3f : 1.0f;
                        strike_sig += noise_sig * blend;
                        strike_amp_ *= 0.995f;
                    }
                    break;
                    
                case STRIKE_MODE_GRANULAR:
                    // Continuous granular texture
                    strike_sig = granular_player_.Process() * strike_level_ * 0.5f;
                    break;
                    
                case STRIKE_MODE_NOISE:
                    // Pure filtered noise
                    if (strike_amp_ > 0.001f) {
                        strike_sig = strike_filter_.ProcessLowPass(
                            noise_.Next() * strike_amp_ * strike_level_);
                        strike_amp_ *= 0.997f;
                    }
                    break;
                    
                case STRIKE_MODE_PLECTRUM:
                    // Guitar pick model: negative impulse, delay, positive release
                    {
                        float impulse = 0.0f;
                        if (plectrum_delay_ > 0) {
                            --plectrum_delay_;
                            if (plectrum_delay_ == 0) {
                                // Release impulse (positive)
                                impulse = strike_level_;
                            }
                            plectrum_damp_ = 1.0f - 0.997f * (1.0f - plectrum_damp_);
                        } else {
                            plectrum_damp_ *= 0.9f;
                        }
                        // Add any impulse (initial negative or release positive)
                        strike_sig = plectrum_impulse_ + impulse;
                        plectrum_impulse_ = 0.0f;  // Clear after use
                    }
                    break;
                    
                case STRIKE_MODE_PARTICLES:
                    // Random impulse train (rain/gravel on resonator)
                    // Only while gate is active (controlled by strike_amp_)
                    if (strike_amp_ > 0.001f) {
                        if (particle_delay_ == 0) {
                            // Generate new particle
                            float amount = noise_.Next() * 0.5f + 0.5f;
                            amount = 1.05f + 0.5f * amount * amount;
                            
                            // Random walk for particle energy
                            float rand = noise_.Next();
                            if (rand > 0.3f) {
                                particle_state_ *= amount;
                                if (particle_state_ > particle_range_ + 0.25f) {
                                    particle_state_ = particle_range_ + 0.25f;
                                }
                            } else if (rand < -0.4f) {
                                particle_state_ /= amount;
                                if (particle_state_ < 0.02f) {
                                    particle_state_ = 0.02f;
                                }
                            }
                            
                            // Schedule next particle based on current state
                            particle_delay_ = static_cast<uint32_t>(
                                particle_state_ * 0.15f * kSampleRate);
                            
                            // Output particle impulse with range-based gain
                            float gain = 1.0f - particle_range_;
                            gain *= gain;
                            strike_sig = particle_state_ * strike_level_ * (1.0f - gain);
                            
                            // Decay range (particles become sparser over time)
                            float decay_factor = 1.0f - timbre_;
                            particle_range_ *= 1.0f - decay_factor * decay_factor * 0.5f;
                        } else {
                            --particle_delay_;
                        }
                        strike_amp_ *= 0.9999f;  // Slow decay
                    }
                    break;
            }
            
            out += strike_sig;
        }
        
        return out;
    }
    
    // Get current bow level for resonator bowing
    float GetBowStrength() const { return bow_level_; }
    
private:
    Noise noise_;
    SamplePlayer sample_player_;
    GranularPlayer granular_player_;
    Tube tube_;              // Waveguide tube for blow excitation
    SVF bow_filter_;
    SVF blow_filter_;
    SVF strike_filter_;
    
    float bow_level_, bow_timbre_;
    float blow_level_, blow_timbre_;
    float blow_frequency_;   // Tube resonant frequency (tracks pitch)
    float blow_envelope_;    // Smooth breath envelope
    float strike_level_, timbre_;
    float strike_amp_;
    StrikeMode strike_mode_;
    
    // Plectrum mode state
    uint32_t plectrum_delay_;
    float plectrum_damp_;
    float plectrum_impulse_;
    
    // Particles mode state
    float particle_state_;
    float particle_range_;
    uint32_t particle_delay_;
};

} // namespace modal
