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
// Noise wavetable for the granular BLOW exciter
// 512 entries of a smooth-ish white noise (deterministic, -1..1 scaled to
// int16). Scanned like the sample used by Elements' granular blow player.
// ============================================================================

static const int16_t kBlowNoiseTable[512] = {
       -43,   137,  -109,    38,   196,  -177,    54,  -211,   119,    32,  -168,    -9,   226,  -150,    88,  -244,
        203,   -61,    -2,   171,  -210,   -27,   143,  -238,   187,  -128,    52,   221,  -198,    20,   -73,   241,
       -190,    77,   163,  -214,    91,    48,  -236,    -6,   199,  -155,   123,  -225,    68,   176,  -104,   215,
        -49,  -164,   207,   -89,  -120,   231,  -179,    58,    -1,   183,  -213,    97,    44,  -222,    18,  -133,
        247,  -187,    84,   145,  -208,    12,  -151,   229,  -176,    62,   197,  -105,   -37,  -228,    30,   169,
       -243,   118,    -8,  -184,   209,   -95,   -60,   239,  -164,    71,   133,  -226,    92,    -3,  -158,   222,
       -172,    24,   140,  -246,   173,  -118,   -51,   198,  -212,    86,    11,  -191,   154,  -137,   -32,   235,
       -181,   105,    47,  -200,   167,  -125,   -63,   195,  -218,    35,   148,  -230,   160,   -16,  -121,   204,
       -162,    17,    94,  -244,   126,   -56,  -190,   219,  -140,   -72,   185,  -207,    96,     3,  -173,   157,
       -236,    78,    41,  -199,   172,  -110,   -20,  -232,   111,  -131,    60,   203,  -214,    -5,   190,  -149,
         67,  -221,   106,    28,  -172,   163,  -126,   -48,   228,  -158,    89,    -2,  -187,    -7,   144,  -243,
        174,  -115,   -42,   197,  -210,    55,    25,  -222,   130,  -144,    12,   201,  -169,    92,    -1,  -196,
        218,  -137,    45,   178,  -204,    72,   -18,  -241,   153,  -108,   -30,   208,  -166,    58,    39,  -227,
        185,  -154,    -7,   134,  -238,   101,   -68,  -191,   204,  -143,    14,   172,  -220,    66,    -1,  -198,
        226,  -159,    83,   128,  -210,   118,   -52,  -183,   215,  -123,    34,   197,  -174,    90,   -29,  -249,
        167,  -138,   -59,   202,  -188,    76,    -4,  -170,   224,  -106,    40,   181,  -209,    97,   -19,  -230,
        148,  -156,    -5,   193,  -199,    65,    31,  -227,   123,  -170,    52,   210,  -184,    84,   -14,  -242,
        175,  -131,   -38,   198,  -214,    71,   -23,  -221,   136,  -149,     8,   204,  -176,    99,   -41,  -244,
        161,  -137,    -9,   186,  -207,    54,    43,  -239,   118,  -173,    68,   219,  -194,    -1,    -1,  -236,
        149,  -142,   -36,   195,  -211,    90,    11,  -228,   127,  -162,    22,   208,  -183,    75,   -33,  -245,
        173,  -128,   -18,   201,  -205,    62,    35,  -226,   131,  -151,    -2,   192,  -201,    88,   -22,  -233,
        153,  -146,   -25,   210,  -196,    59,    26,  -234,   121,  -160,    44,   203,  -187,    80,    -6,  -240,
        165,  -132,   -49,   190,  -208,    67,    -3,  -222,   142,  -155,    18,   196,  -192,    93,   -27,  -238,
        156,  -139,   -53,   207,  -198,    71,    23,  -231,   124,  -168,    37,   205,  -189,    82,   -16,  -246,
        170,  -121,   -13,   199,  -203,    63,     -1,  -225,   137,  -145,    15,   193,  -195,    86,   -24,  -235,
        160,  -136,   -42,   203,  -199,    57,    28,  -232,   129,  -158,    33,   198,  -184,    90,   -38,  -241,
        168,  -125,   -45,   205,  -197,    66,     -2,  -227,   140,  -150,     9,   190,  -190,    97,   -29,  -237,
        157,  -141,   -31,   209,  -204,    60,    24,  -233,   122,  -167,    45,   201,  -185,    85,   -13,  -243,
        163,  -130,   -54,   191,  -206,    72,    -5,  -224,   135,  -154,    12,   204,  -191,    91,   -23,  -239,
        159,  -144,   -21,   213,  -193,    64,    26,  -229,   128,  -161,    40,   197,  -187,    87,   -35,  -248,
        166,  -129,   -16,   204,  -196,    68,     -7,  -226,   138,  -148,    21,   195,  -200,    95,   -31,  -236,
        158,  -137,   -48,   207,  -201,    58,    30,  -234,   125,  -166,    36,   206,  -181,    89,   -11,  -245
};

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
        table_ = elements_samples::sample_ptrs[0];
        table_len_ = elements_samples::sample_sizes[0];
        position_ = 0.5f;
        pitch_ = 1.0f;
        density_ = 0.5f;
    }
    
    void SetSample(int idx) {
        if (idx >= 0 && idx < static_cast<int>(elements_samples::NUM_SAMPLES)) {
            sample_idx_ = idx;
            table_ = elements_samples::sample_ptrs[idx];
            table_len_ = elements_samples::sample_sizes[idx];
        }
    }
    
    // Use a custom sample table (e.g. the built-in noise wavetable).
    void SetTable(const int16_t* table, uint32_t len) {
        if (table != nullptr && len > 1) {
            table_ = table;
            table_len_ = len;
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
        const int16_t* data = table_;
        uint32_t length = table_len_;
        
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
    const int16_t* table_ = nullptr;
    uint32_t table_len_ = 0;
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
        blow_player_.Reset();
        blow_player_.SetTable(kBlowNoiseTable, 512);
        blow_player_.SetPitch(0.5f);   // Unison
        blow_player_.SetPosition(0.5f);
        blow_player_.SetDensity(0.2f);
        blow_frequency_ = 440.0f;
        blow_envelope_ = 0.0f;
        blow_flow_ = 0.5f;
        
        // Plectrum mode state
        plectrum_delay_ = 0;
        plectrum_damp_ = 0.0f;
        plectrum_impulse_ = 0.0f;
        
        // Particles mode state
        particle_state_ = 0.5f;
        particle_range_ = 1.0f;
        particle_delay_ = 0;
    }
    
    // Reset only the transient/voice state so a note can be retriggered cleanly.
    // Preserves all parameter values (levels, timbres, modes) that were set via
    // SetBow/SetBlow/SetStrike/Set*Timbre/SetStrikeSample/SetStrikeMode etc.
    void ResetRuntime() {
        strike_amp_ = 0.0f;
        bow_filter_.Reset();
        blow_filter_.Reset();
        strike_filter_.Reset();
        tube_.Init();
        sample_player_.ResetRuntime();
        granular_player_.Reset();
        blow_player_.Reset();
        blow_player_.SetTable(kBlowNoiseTable, 512);
        blow_player_.SetPitch(blow_timbre_);
        blow_player_.SetPosition(0.5f);
        blow_player_.SetDensity(blow_flow_);
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
        // BLOW TIMBRE maps to pitch: -30 to +42 semitones (Elements uses
        // 72*timbre - 60, offset here for a more breathy default range).
        blow_player_.SetPitch(blow_timbre_);
    }
    
    // Air turbulence / flow control. Higher flow = more random granular
    // restarts (like scanning a wavetable of noise in Elements).
    void SetBlowFlow(float flow) {
        blow_flow_ = Clamp(flow, 0.0f, 1.0f);
        blow_player_.SetDensity(blow_flow_);
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
        
        // BLOW: Granular sample-player breath through the tube waveguide.
        // This mirrors Elements' EXCITER_MODEL_GRANULAR_SAMPLE_PLAYER for the
        // blow exciter: a noise wavetable is scanned with random restarts
        // (density = FLOW) and a pitch controlled by BLOW TIMBRE.
        if (blow_level_ > 0.001f) {
            // Smooth envelope for breath
            float target_envelope = blow_level_;
            blow_envelope_ += (target_envelope - blow_envelope_) * 0.001f;
            
            // Granular noise scanning (Elements-style granular blow source)
            float blow_sig = blow_player_.Process();
            
            // Gentle turbulence modulation on top of the granular texture
            float breath_mod = 1.0f + noise_.NextFiltered(0.999f) * 0.3f;
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
    GranularPlayer blow_player_;   // Granular BLOW source (noise wavetable scan)
    Tube tube_;              // Waveguide tube for blow excitation
    SVF bow_filter_;
    SVF blow_filter_;
    SVF strike_filter_;
    
    float bow_level_, bow_timbre_;
    float blow_level_, blow_timbre_, blow_flow_;
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
