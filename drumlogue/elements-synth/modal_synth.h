/*
 * Modal Synth for Drumlogue
 * Inspired by Mutable Instruments Elements
 * 
 * Features:
 * - Modal resonator with 32 bandpass modes
 * - Karplus-Strong string model alternative
 * - 4-pole Moog ladder filter
 * - Multistage envelope with 4 modes
 * - LFO with presets
 * - Stereo output with Elements-style panning
 */

#pragma once

#include "dsp/dsp_core.h"
#include "dsp/envelope.h"
#include "dsp/exciter.h"
#include "dsp/resonator.h"
#ifndef ELEMENTS_LIGHTWEIGHT
#include "dsp/filter.h"
#endif

// ============================================================================
// DSP Profiling Support (Test Harness Only)
// ============================================================================
#if defined(TEST) && defined(DSP_PROFILE)
#include <chrono>

// Forward declarations for profiling stats from main.cc
extern struct DSPProfileStats g_profile_exciter;
extern struct DSPProfileStats g_profile_resonator;
extern struct DSPProfileStats g_profile_string;
extern struct DSPProfileStats g_profile_multistring;
extern struct DSPProfileStats g_profile_filter;

// Helper class for scoped timing
class ProfileTimer {
public:
    ProfileTimer(DSPProfileStats& stats) : stats_(stats) {
        start_ = std::chrono::high_resolution_clock::now();
    }
    ~ProfileTimer() {
        auto end = std::chrono::high_resolution_clock::now();
        double us = std::chrono::duration<double, std::micro>(end - start_).count();
        stats_.Record(us);
    }
private:
    DSPProfileStats& stats_;
    std::chrono::high_resolution_clock::time_point start_;
};

#define PROFILE_EXCITER_BEGIN() auto _exc_start = std::chrono::high_resolution_clock::now()
#define PROFILE_EXCITER_END() do { \
    auto _exc_end = std::chrono::high_resolution_clock::now(); \
    g_profile_exciter.Record(std::chrono::duration<double, std::micro>(_exc_end - _exc_start).count()); \
} while(0)

#define PROFILE_RESONATOR_BEGIN() auto _res_start = std::chrono::high_resolution_clock::now()
#define PROFILE_RESONATOR_END() do { \
    auto _res_end = std::chrono::high_resolution_clock::now(); \
    g_profile_resonator.Record(std::chrono::duration<double, std::micro>(_res_end - _res_start).count()); \
} while(0)

#define PROFILE_STRING_BEGIN() auto _str_start = std::chrono::high_resolution_clock::now()
#define PROFILE_STRING_END() do { \
    auto _str_end = std::chrono::high_resolution_clock::now(); \
    g_profile_string.Record(std::chrono::duration<double, std::micro>(_str_end - _str_start).count()); \
} while(0)

#define PROFILE_MULTISTRING_BEGIN() auto _mstr_start = std::chrono::high_resolution_clock::now()
#define PROFILE_MULTISTRING_END() do { \
    auto _mstr_end = std::chrono::high_resolution_clock::now(); \
    g_profile_multistring.Record(std::chrono::duration<double, std::micro>(_mstr_end - _mstr_start).count()); \
} while(0)

#define PROFILE_FILTER_BEGIN() auto _flt_start = std::chrono::high_resolution_clock::now()
#define PROFILE_FILTER_END() do { \
    auto _flt_end = std::chrono::high_resolution_clock::now(); \
    g_profile_filter.Record(std::chrono::duration<double, std::micro>(_flt_end - _flt_start).count()); \
} while(0)

#else
// No-op macros when profiling is disabled
#define PROFILE_EXCITER_BEGIN()
#define PROFILE_EXCITER_END()
#define PROFILE_RESONATOR_BEGIN()
#define PROFILE_RESONATOR_END()
#define PROFILE_STRING_BEGIN()
#define PROFILE_STRING_END()
#define PROFILE_MULTISTRING_BEGIN()
#define PROFILE_MULTISTRING_END()
#define PROFILE_FILTER_BEGIN()
#define PROFILE_FILTER_END()
#endif // TEST && DSP_PROFILE

namespace modal {

// ============================================================================
// Complete Modal Synth - Main synthesis engine
// ============================================================================

class ModalSynth {
public:
    enum Model { kModal, kString, kMultiString };
    
    ModalSynth() : model_(kModal), pitch_(60.0f), output_level_(0.8f) {}
    
    void Init() {
        exciter_.Reset();
        resonator_.SetFrequency(MidiToFrequency(60.0f));
        resonator_.Update();
        string_.SetFrequency(MidiToFrequency(60.0f));
        multi_string_.SetFrequency(MidiToFrequency(60.0f));
#ifndef ELEMENTS_LIGHTWEIGHT
        filter_.Reset();
#endif
        
        env_.SetADSR(0.001f, 0.2f, 0.0f, 0.3f);
#ifndef ELEMENTS_LIGHTWEIGHT
        filter_env_.SetADSR(0.001f, 0.3f, 0.3f, 0.2f);
        
        filter_.SetCutoff(8000.0f);
#endif
    }
    
    // Exciter controls
    void SetBow(float v) { exciter_.SetBow(v); }
    void SetBlow(float v) { exciter_.SetBlow(v); }
    void SetStrike(float v) { exciter_.SetStrike(v); }
    void SetBowTimbre(float v) { exciter_.SetBowTimbre(v); }
    void SetBlowTimbre(float v) { exciter_.SetBlowTimbre(v); }
    void SetStrikeTimbre(float v) { exciter_.SetStrikeTimbre(v); }
    
    // Strike sample: 0=mallet_soft, 1=mallet_med, 2=mallet_hard, 3=plectrum, 4=stick, 5=bow_attack
    void SetStrikeSample(int idx) { exciter_.SetStrikeSample(idx); }
    
    // Strike mode: 0=Sample, 1=Granular, 2=Noise
    void SetStrikeMode(int mode) { exciter_.SetStrikeMode(mode); }
    
    // Granular controls (for granular strike mode)
    void SetGranularPosition(float v) { exciter_.SetGranularPosition(v); }
    void SetGranularDensity(float v) { exciter_.SetGranularDensity(v); }
    
    // Resonator controls
    void SetStructure(float v) { 
        structure_base_ = v;
        resonator_.SetStructure(v);
        resonator_.Update();  // Apply immediately for live parameter changes
    }
    void SetBrightness(float v) { 
        brightness_base_ = v;
        resonator_.SetBrightness(v);
        resonator_.Update();  // Apply immediately for live parameter changes
        string_.SetBrightness(v);
        multi_string_.SetBrightness(v);
    }
    void SetDamping(float v) { 
        resonator_.SetDamping(v);
        resonator_.Update();  // Apply immediately for live parameter changes
        string_.SetDamping(v);
        multi_string_.SetDamping(v);
    }
    
    // Dispersion control (piano-like inharmonicity, String/MultiString only)
    void SetDispersion(float v) {
        string_.SetDispersion(v);
        multi_string_.SetDispersion(v);
    }
    
    void SetPosition(float v) { 
        position_base_ = v;
        resonator_.SetPosition(v);
        resonator_.Update();  // Apply immediately for live parameter changes
    }
    
    // Filter controls
#ifndef ELEMENTS_LIGHTWEIGHT
    void SetFilterCutoff(float v) {
        filter_cutoff_base_ = 20.0f * std::pow(900.0f, v);
        filter_.SetCutoff(filter_cutoff_base_);  // Apply immediately
    }
    void SetFilterResonance(float v) { filter_.SetResonance(v); }
    void SetFilterEnvAmount(float v) { filter_env_amount_ = v; }
#else
    // Stubs when filter is disabled
    void SetFilterCutoff(float v) { (void)v; }
    void SetFilterResonance(float v) { (void)v; }
    void SetFilterEnvAmount(float v) { (void)v; }
#endif
    
    // Envelope controls (using MultistageEnvelope)
    void SetAttack(float v) { 
        attack_time_ = 0.001f + v * 2.0f;
        updateEnvelope();
    }
    void SetDecay(float v) { 
        decay_time_ = 0.01f + v * 3.0f;
        updateEnvelope();
    }
    void SetSustain(float v) { 
        sustain_level_ = v;
        updateEnvelope();
    }
    void SetRelease(float v) { 
        release_time_ = 0.01f + v * 5.0f;
        updateEnvelope();
    }
    
    // Envelope mode: 0=ADR, 1=AD, 2=AR, 3=AD-Loop
    void SetEnvMode(int mode) {
        env_mode_ = mode;
        updateEnvelope();
    }
    
    // Model selection: 0=Modal, 1=String, 2=MultiString
    void SetModel(int m) { 
        if (m == 0) model_ = kModal;
        else if (m == 1) model_ = kString;
        else model_ = kMultiString;
    }
    
    // Multi-string detuning amount (0=unison, 1=full chorus)
    void SetMultiStringDetune(float v) {
        multi_string_.SetDetuneAmount(Clamp(v, 0.0f, 1.0f));
    }
    
    // Stereo space control (Elements-style)
    void SetSpace(float v) { 
        space_ = Clamp(v, 0.0f, 1.0f);
        resonator_.SetSpace(v);
    }
    
    // Force resonator coefficient update (call after bulk parameter changes)
    void ForceResonatorUpdate() {
        resonator_.ForceUpdate();
    }
#ifndef ELEMENTS_LIGHTWEIGHT
    // LFO controls
    void SetLfoRate(float v) {
        // 0.1 Hz to 20 Hz, exponential scaling
        lfo_rate_ = 0.1f * std::pow(200.0f, v);
    }
    
    void SetLfoDepth(float v) {
        // 0 to 1 mapped to 0% to 100% modulation depth
        lfo_depth_ = Clamp(v, 0.0f, 1.0f);
    }
    
    void SetLfoPreset(int preset) {
        // Presets combine shape and destination
        // 0=OFF, 1=TRI>CUT, 2=SIN>GEO, 3=SQR>POS, 4=TRI>BRI, 5=SIN>SPC, 6=SAW>CUT
        switch (preset) {
            case 0: // OFF
                lfo_dest_ = 0;
                lfo_shape_ = 0; // TRI
                break;
            case 1: // TRI>CUT
                lfo_shape_ = 0; // TRI
                lfo_dest_ = 1;  // CUTOFF
                break;
            case 2: // SIN>GEO
                lfo_shape_ = 1; // SIN
                lfo_dest_ = 2;  // GEOMETRY
                break;
            case 3: // SQR>POS
                lfo_shape_ = 2; // SQR
                lfo_dest_ = 3;  // POSITION
                break;
            case 4: // TRI>BRI
                lfo_shape_ = 0; // TRI
                lfo_dest_ = 4;  // BRIGHTNESS
                break;
            case 5: // SIN>SPC
                lfo_shape_ = 1; // SIN
                lfo_dest_ = 5;  // SPACE
                break;
            case 6: // SAW>CUT
                lfo_shape_ = 3; // SAW
                lfo_dest_ = 1;  // CUTOFF
                break;
            case 7: // RND>SPC (random/S&H -> Space)
                lfo_shape_ = 4; // RND (sample & hold)
                lfo_dest_ = 5;  // SPACE
                break;
            default:
                lfo_dest_ = 0;
                break;
        }
    }
#else
    // Stubs when LFO is disabled
    void SetLfoRate(float v) { (void)v; }
    void SetLfoDepth(float v) { (void)v; }
    void SetLfoPreset(int preset) { (void)preset; }
#endif
    
    void SetOutputLevel(float v) { output_level_ = v; }
    
    void NoteOn(uint8_t note, uint8_t velocity) {
        pitch_ = (float)note;
        float freq = MidiToFrequency(pitch_);
        
        resonator_.SetFrequency(freq);
        resonator_.Update();
        string_.SetFrequency(freq);
        multi_string_.SetFrequency(freq);
        
        // Set blow frequency for tube resonance (tracks pitch)
        exciter_.SetBlowFrequency(freq);
        
        exciter_.Trigger();
        env_.Trigger();
#ifndef ELEMENTS_LIGHTWEIGHT
        filter_env_.Trigger();
#endif
        
        // Use exponential velocity curve for more musical dynamics
        velocity_ = GetVelocityGain(velocity);
    }
    
    void NoteOff() {
        env_.Release();
#ifndef ELEMENTS_LIGHTWEIGHT
        filter_env_.Release();
#endif
    }
    
    void Process(float* out_l, float* out_r, uint32_t frames) {
#ifndef ELEMENTS_LIGHTWEIGHT
        // LFO control rate divisor - update every 32 samples (~1.5kHz control rate)
        static constexpr int kLfoUpdateRate = 32;
#endif
        
        for (uint32_t i = 0; i < frames; ++i) {
#ifndef ELEMENTS_LIGHTWEIGHT
            // Update LFO at control rate (not audio rate) to save CPU
            if (lfo_counter_ == 0 && lfo_dest_ != 0) {
                // Update LFO phase
                lfo_phase_ += lfo_rate_ * kLfoUpdateRate / kSampleRate;
                if (lfo_phase_ >= 1.0f) lfo_phase_ -= 1.0f;
                
                // Generate LFO waveform based on shape
                float lfo;
                switch (lfo_shape_) {
                    case 0: // TRI (triangle)
                        lfo = (lfo_phase_ < 0.5f) ? (lfo_phase_ * 4.0f - 1.0f) : (3.0f - lfo_phase_ * 4.0f);
                        break;
                    case 1: // SIN (sine) - use fast approximation
                        {
                            float x = lfo_phase_ * 2.0f - 1.0f;  // -1 to 1
                            lfo = x * (2.0f - (x < 0 ? -x : x));  // Parabolic sine approx
                        }
                        break;
                    case 2: // SQR (square)
                        lfo = (lfo_phase_ < 0.5f) ? 1.0f : -1.0f;
                        break;
                    case 3: // SAW (sawtooth)
                        lfo = 2.0f * lfo_phase_ - 1.0f;
                        break;
                    case 4: // RND (sample & hold random)
                        {
                            // Trigger new random value when phase wraps
                            static float last_phase = 0.0f;
                            if (lfo_phase_ < last_phase) {
                                // Phase wrapped, generate new random value
                                lfo_random_value_ = (float)((lfo_random_state_ = lfo_random_state_ * 1103515245 + 12345) & 0x7FFFFFFF) / (float)0x7FFFFFFF * 2.0f - 1.0f;
                            }
                            last_phase = lfo_phase_;
                            lfo = lfo_random_value_;
                        }
                        break;
                    default:
                        lfo = 0.0f;
                        break;
                }
                
                // Apply LFO depth and to destination
                float lfo_mod = lfo * lfo_depth_ * 0.5f;
                switch (lfo_dest_) {
                    case 1: // CUTOFF
                        filter_.SetCutoff(filter_cutoff_base_ * (1.0f + lfo_mod));
                        break;
                    case 2: // GEOMETRY
                        resonator_.SetStructure(Clamp(structure_base_ + lfo_mod * 0.5f, 0.0f, 1.0f));
                        resonator_.Update();
                        break;
                    case 3: // POSITION
                        resonator_.SetPosition(Clamp(position_base_ + lfo_mod * 0.5f, 0.0f, 1.0f));
                        resonator_.Update();
                        break;
                    case 4: // BRIGHTNESS
                        resonator_.SetBrightness(Clamp(brightness_base_ + lfo_mod * 0.5f, 0.0f, 1.0f));
                        resonator_.Update();
                        break;
                    case 5: // SPACE
                        resonator_.SetSpace(Clamp(space_ + lfo_mod * 0.5f, 0.0f, 1.0f));
                        break;
                    default:
                        break;
                }
            }
            lfo_counter_ = (lfo_counter_ + 1) & (kLfoUpdateRate - 1);
#endif  // ELEMENTS_LIGHTWEIGHT
            
            // Generate excitation
            PROFILE_EXCITER_BEGIN();
            float exc = exciter_.Process() * velocity_;
            PROFILE_EXCITER_END();
            
            // Get bow strength for resonator bowing
            float bow_strength = exciter_.GetBowStrength() * velocity_;
            
            // Resonate with stereo output
            float center, side;
            if (model_ == kModal) {
                // Modal resonator outputs stereo directly (Elements-style)
                // Pass bow strength for banded waveguide bowing
                PROFILE_RESONATOR_BEGIN();
                resonator_.Process(exc, bow_strength, center, side);
                PROFILE_RESONATOR_END();
            } else if (model_ == kString) {
                // Single string model is mono
                PROFILE_STRING_BEGIN();
                center = string_.Process(exc);
                PROFILE_STRING_END();
                side = 0.0f;
                // Gain compensation: STRING is ~9.7dB quieter than MODAL
                center *= 3.0f;
            } else {
                // Multi-string model (5 sympathetic strings)
                PROFILE_MULTISTRING_BEGIN();
                center = multi_string_.Process(exc);
                PROFILE_MULTISTRING_END();
                side = 0.0f;
                // Gain compensation: MSTRING is ~15.7dB quieter than MODAL
                center *= 6.0f;
            }
            
#ifndef ELEMENTS_LIGHTWEIGHT
            // Apply filter with envelope modulation (only if LFO not targeting cutoff)
            PROFILE_FILTER_BEGIN();
            float env_val = filter_env_.Process();
            if (lfo_dest_ != 1) {
                float cutoff = filter_cutoff_base_ * (1.0f + env_val * filter_env_amount_ * 4.0f);
                cutoff = Clamp(cutoff, 20.0f, 18000.0f);
                filter_.SetCutoff(cutoff);
            }
            
            // Filter center channel (side is already difference signal)
            float filtered_center = filter_.Process(center);
            PROFILE_FILTER_END();
#else
            // No filter in lightweight mode - pass through directly
            float filtered_center = center;
#endif
            
            // Apply amplitude envelope
            float amp = env_.Process() * output_level_;
            
            // Convert mid-side to left-right with soft limiting
            // L = center + side, R = center - side
            float mid = FastTanh(filtered_center) * amp;
            float side_scaled = side * amp;
            
            float out_left = mid + side_scaled;
            float out_right = mid - side_scaled;
            
            // Soft limit final output to prevent clipping
            out_left = FastTanh(out_left);
            out_right = FastTanh(out_right);
            
            // Robust NaN/Inf protection (NaN != NaN trick)
            if (out_left != out_left) out_left = 0.0f;
            if (out_right != out_right) out_right = 0.0f;
            
            out_l[i] = out_left;
            out_r[i] = out_right;
        }
    }
    
    void Reset() {
        resonator_.Reset();
        string_.Reset();
        multi_string_.Reset();
#ifndef ELEMENTS_LIGHTWEIGHT
        filter_.Reset();
#endif
    }
    
private:
    void updateEnvelope() {
        // Apply current envelope mode with stored parameters
        // ADR mode uses a fixed sustain level of 0.0 (full decay to zero)
        switch (env_mode_) {
            case 0: // ADR (Attack-Decay-Release, sustain at 0)
                // ADR mode: attack, decay to sustain, then release
                // For struck/plucked sounds, sustain is non-zero to let resonator ring
                // The decay time controls how long until sustain level
                // The release time controls fadeout after note-off
                env_.SetADSR(attack_time_, decay_time_, 0.7f, release_time_);
#ifndef ELEMENTS_LIGHTWEIGHT
                filter_env_.SetADSR(attack_time_ * 0.25f, decay_time_ * 0.33f, 
                                    0.5f, release_time_ * 0.4f);
#endif
                break;
            case 1: // AD (no sustain, immediate decay to zero)
                env_.SetAD(attack_time_, decay_time_ + release_time_);
#ifndef ELEMENTS_LIGHTWEIGHT
                filter_env_.SetAD(attack_time_ * 0.25f, (decay_time_ + release_time_) * 0.33f);
#endif
                break;
            case 2: // AR (attack to peak, hold at peak, then release)
                env_.SetAR(attack_time_, release_time_);
#ifndef ELEMENTS_LIGHTWEIGHT
                filter_env_.SetAR(attack_time_ * 0.25f, release_time_ * 0.4f);
#endif
                break;
            case 3: // AD-Loop (looping envelope for drones/pads)
                env_.SetADLoop(attack_time_, decay_time_);
#ifndef ELEMENTS_LIGHTWEIGHT
                filter_env_.SetADLoop(attack_time_ * 0.5f, decay_time_ * 0.5f);
#endif
                break;
        }
    }

    Exciter exciter_;
    Resonator resonator_;
    String string_;
    MultiString multi_string_;
#ifndef ELEMENTS_LIGHTWEIGHT
    MoogLadder filter_;
    MultistageEnvelope filter_env_;
#endif
    MultistageEnvelope env_;
    
    Model model_;
    float pitch_;
    float velocity_ = 1.0f;
    float output_level_;
    float space_ = 0.7f;  // Default stereo width 70%
#ifndef ELEMENTS_LIGHTWEIGHT
    float filter_cutoff_base_ = 8000.0f;
    float filter_env_amount_ = 0.5f;  // Default filter envelope amount (gives character to plucks)
#endif
    
    // Envelope parameters
    int env_mode_ = 0;
    float attack_time_ = 0.001f;
    float decay_time_ = 0.1f;
    float sustain_level_ = 0.7f;
    float release_time_ = 0.3f;
    
#ifndef ELEMENTS_LIGHTWEIGHT
    // LFO parameters
    float lfo_rate_ = 1.0f;
    float lfo_phase_ = 0.0f;
    float lfo_depth_ = 0.0f;
    int lfo_shape_ = 0;  // 0=TRI, 1=SIN, 2=SQR, 3=SAW, 4=RND
    int lfo_dest_ = 0;
    int lfo_counter_ = 0;  // For control-rate LFO updates
    float lfo_random_value_ = 0.0f;  // For RND (sample & hold) waveform
    uint32_t lfo_random_state_ = 12345;  // Random state for S&H
#endif
    
    // Base values for LFO modulation targets
    float structure_base_ = 0.5f;
    float position_base_ = 0.5f;
    float brightness_base_ = 0.5f;
};

} // namespace modal
