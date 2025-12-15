/**
 * @file drupiter_synth.h
 * @brief Main synthesizer class for Drupiter (Jupiter-8 inspired)
 *
 * Based on Bristol Jupiter-8 emulation architecture
 * Adapted for Korg drumlogue embedded platform
 */

#pragma once

#include <cstdint>
#include <cmath>
#include "unit.h"

// NEON SIMD optimizations (Cortex-A7)
#ifdef USE_NEON
#include <arm_neon.h>
#endif

// Common DSP utilities (includes NEON helpers when USE_NEON defined)
#define NEON_DSP_NS drupiter
#include "../common/neon_dsp.h"
#include "../common/stereo_widener.h"

// Maximum frames per buffer (drumlogue typically uses 64)
static constexpr uint32_t kMaxFrames = 64;

// Forward declarations of DSP components
namespace dsp {
    class JupiterDCO;
    class JupiterVCF;
    class JupiterEnvelope;
    class JupiterLFO;
    class SmoothedValue;
}

// ============================================================================
// DCO Waveform and Option Names
// ============================================================================

// DCO1 waveform names
static const char* const kDco1WaveNames[] = {
    "SAW", "SQR", "PUL", "TRI"
};

// DCO2 waveform names
static const char* const kDco2WaveNames[] = {
    "SAW", "NSE", "PUL", "SIN"
};

// Octave names
static const char* const kOctaveNames[] = {
    "16'", "8'", "4'"
};

// Sync mode names
static const char* const kSyncNames[] = {
    "OFF", "SOFT", "HARD"
};

// ============================================================================
// MOD HUB - Jupiter-8 style modulation routing
// ============================================================================

/**
 * @brief MOD HUB destinations (selected via MOD_SELECT parameter)
 * 
 * All use 0-100 range. Bipolar params treat 50 as center (no effect).
 */
enum ModDestination {
    MOD_LFO_RATE = 0,      // 0-100: LFO speed (0.1Hz to 20Hz)
    MOD_LFO_WAVE,          // 0-3: LFO waveform (TRI/RAMP/SQR/S&H)
    MOD_LFO_TO_VCO,        // 0-100: LFO to pitch depth
    MOD_LFO_TO_VCF,        // 0-100: LFO to filter depth
    MOD_VEL_TO_VCF,        // 0-100: Velocity to filter cutoff
    MOD_KEY_TRACK,         // 0-100: Filter keyboard tracking (50=50%)
    MOD_PB_RANGE,          // 0-3: Pitch bend range (2/7/12/24 semitones)
    MOD_VCF_ENV_AMT,       // 0-100: VCF envelope amount (50=0, bipolar)
    MOD_NUM_DESTINATIONS
};

// MOD HUB destination names (7 chars max for display)
static const char* const kModDestNames[] = {
    "LFO SPD",   // 0: LFO Rate
    "LFO WAV",   // 1: LFO Waveform
    "LFO>VCO",   // 2: LFO to VCO depth
    "LFO>VCF",   // 3: LFO to VCF depth
    "VEL>VCF",   // 4: Velocity to VCF
    "KEYTRK",    // 5: Filter Key Tracking
    "PB RNG",    // 6: Pitch Bend Range
    "VCF ENV"    // 7: VCF Envelope Amount
};

// LFO waveform names
static const char* const kLfoWaveNames[] = {
    "TRI", "RAMP", "SQR", "S&H"
};

// Pitch bend range names and values
static const char* const kPbRangeNames[] = {
    "+/-2", "+/-7", "+/-12", "+/-24"
};
static const float kPbSemitones[] = {2.0f, 7.0f, 12.0f, 24.0f};

// ============================================================================
// DrupiterSynth Class
// ============================================================================

/**
 * @brief Main Drupiter synthesizer class
 * 
 * Monophonic Jupiter-8 inspired synthesizer with:
 * - Dual DCOs with multiple waveforms
 * - Multi-mode VCF (LP12/LP24/HP12/BP12)
 * - Dual ADSR envelopes (VCF and VCA)
 * - LFO with multiple waveforms
 * - Cross-modulation and sync capabilities
 */
class DrupiterSynth {
public:
    /**
     * @brief Parameter IDs matching header.c definitions
     */
    enum ParamId {
        // Page 1: DCO-1
        PARAM_DCO1_OCT = 0,      // 0-2: Octave (16', 8', 4')
        PARAM_DCO1_WAVE,         // 0-3: Waveform
        PARAM_DCO1_PW,           // 0-100: Pulse width
        PARAM_XMOD,              // 0-100: Cross-mod DCO2->DCO1
        
        // Page 2: DCO-2
        PARAM_DCO2_OCT,          // 0-2: Octave (16', 8', 4')
        PARAM_DCO2_WAVE,         // 0-3: Waveform
        PARAM_DCO2_TUNE,         // 0-100: Detune (50=center)
        PARAM_SYNC,              // 0-2: OFF/SOFT/HARD
        
        // Page 3: MIX & VCF
        PARAM_OSC_MIX,           // 0-100: Oscillator mix
        PARAM_VCF_CUTOFF,
        PARAM_VCF_RESONANCE,
        PARAM_VCF_TYPE,
        
        // Page 4: VCF Envelope
        PARAM_VCF_ATTACK,
        PARAM_VCF_DECAY,
        PARAM_VCF_SUSTAIN,
        PARAM_VCF_RELEASE,
        
        // Page 5: VCA Envelope
        PARAM_VCA_ATTACK,
        PARAM_VCA_DECAY,
        PARAM_VCA_SUSTAIN,
        PARAM_VCA_RELEASE,
        
        // Page 6: MOD HUB & Output
        PARAM_MOD_SELECT,        // Selects which mod destination to edit
        PARAM_MOD_VALUE,         // Value for selected mod destination
        PARAM_CHORUS_DEPTH,      // Chorus effect depth
        PARAM_SPACE,             // Stereo width
        
        PARAM_COUNT = 24
    };

    /**
     * @brief Preset structure
     */
    struct Preset {
        uint8_t params[PARAM_COUNT];  // Continuous params are 0-100; discrete params are small enums
        char name[14];                // 13 chars + null terminator
    };

    /**
     * @brief Constructor
     */
    DrupiterSynth();
    
    /**
     * @brief Destructor
     */
    ~DrupiterSynth();

    /**
     * @brief Initialize synthesizer
     * @param desc Runtime descriptor from SDK
     * @return 0 on success, negative on error
     */
    int8_t Init(const unit_runtime_desc_t* desc);
    
    /**
     * @brief Cleanup resources
     */
    void Teardown();
    
    /**
     * @brief Reset synthesizer state
     */
    void Reset();
    
    /**
     * @brief Resume after suspend
     */
    void Resume();
    
    /**
     * @brief Suspend (prepare for power-down)
     */
    void Suspend();
    
    /**
     * @brief Render audio
     * @param out Output buffer (stereo interleaved: L,R,L,R,...)
     * @param frames Number of stereo frames to render
     */
    void Render(float* out, uint32_t frames);
    
    /**
     * @brief Set parameter value
     * @param id Parameter ID (0-23)
     * @param value Parameter value (0-127 typically)
     */
    void SetParameter(uint8_t id, int32_t value);
    
    /**
     * @brief Get parameter value
     * @param id Parameter ID
     * @return Current parameter value
     */
    int32_t GetParameter(uint8_t id) const;
    
    /**
     * @brief Get parameter display string
     * @param id Parameter ID
     * @param value Parameter value
     * @return String representation (for STRINGS type params)
     */
    const char* GetParameterStr(uint8_t id, int32_t value);
    
    /**
     * @brief Note on event
     * @param note MIDI note number (0-127)
     * @param velocity Note velocity (0-127)
     */
    void NoteOn(uint8_t note, uint8_t velocity);
    
    /**
     * @brief Note off event
     * @param note MIDI note number
     */
    void NoteOff(uint8_t note);
    
    /**
     * @brief All notes off
     */
    void AllNoteOff();
    
    /**
     * @brief Load preset
     * @param preset_id Preset index (0-5)
     */
    void LoadPreset(uint8_t preset_id);
    
    /**
     * @brief Save current state to preset
     * @param preset_id Preset index (0-5)
     */
    void SavePreset(uint8_t preset_id);

private:
    // DSP component instances (forward declared)
    dsp::JupiterDCO* dco1_;
    dsp::JupiterDCO* dco2_;
    dsp::JupiterVCF* vcf_;
    dsp::JupiterLFO* lfo_;
    dsp::JupiterEnvelope* env_vcf_;
    dsp::JupiterEnvelope* env_vca_;
    
    // Synthesizer state
    float sample_rate_;
    bool gate_;                    // Note gate (on/off)
    uint8_t current_note_;         // Current MIDI note
    uint8_t current_velocity_;     // Current note velocity
    float current_freq_hz_;        // Current fundamental frequency
    
    // Parameter storage
    Preset current_preset_;
    Preset factory_presets_[6];
    
    // Jupiter-8 specific oscillator controls
    uint8_t sync_mode_;            // 0=OFF, 1=SOFT, 2=HARD
    float xmod_depth_;             // Cross-mod depth 0-1
    
    // MOD HUB values (0-100 range, stored separately from preset params)
    uint8_t mod_values_[MOD_NUM_DESTINATIONS];
    mutable char mod_value_str_[16];  // Buffer for GetParameterStr display
    
    // Internal audio buffers (avoid dynamic allocation)
    float dco1_out_;
    float dco2_out_;
    float noise_out_;
    float mixed_;
    float filtered_;
    float vcf_env_out_;
    float vca_env_out_;
    float lfo_out_;
    
    // NEON-aligned intermediate buffers for block processing
    alignas(16) float mix_buffer_[kMaxFrames];
    alignas(16) float left_buffer_[kMaxFrames];
    alignas(16) float right_buffer_[kMaxFrames];
    
    // Chorus stereo effect (delay-based stereo spread)
    common::ChorusStereoWidener* space_widener_;
    
    // Noise generator state
    uint32_t noise_seed_;
    
    // Parameter smoothing (prevents zipper noise)
    dsp::SmoothedValue* cutoff_smooth_;
    dsp::SmoothedValue* dco1_level_smooth_;
    dsp::SmoothedValue* dco2_level_smooth_;
    
    // Cached filter cutoff for coefficient update optimization
    float last_cutoff_hz_;
    
    /**
     * @brief Convert MIDI note to frequency
     * @param note MIDI note number
     * @return Frequency in Hz
     */
    float NoteToFrequency(uint8_t note) const;
    
    /**
     * @brief Convert octave parameter to multiplier
     * @param octave_param Parameter value 0-127
     * @return Frequency multiplier (0.5, 1.0, 2.0)
     */
    float OctaveToMultiplier(uint8_t octave_param) const;
    
    /**
     * @brief Generate white noise sample
     * @return Random noise value [-1.0, 1.0]
     */
    float GenerateNoise();
    
    /**
     * @brief Convert parameter value to time with quadratic scaling
     * @param value Parameter value 0-127
     * @return Time in seconds (0.001 to 5.0)
     */
    static float ParameterToEnvelopeTime(uint8_t value);
    
    /**
     * @brief Convert parameter value to frequency with quadratic scaling
     * @param value Parameter value 0-127
     * @param min_freq Minimum frequency (Hz)
     * @param max_freq Maximum frequency (Hz)
     * @return Frequency in Hz
     */
    static float ParameterToExponentialFreq(uint8_t value, float min_freq, float max_freq);
    
    /**
     * @brief Update DSP component parameters
     */
    void UpdateDCOParameters();
    void UpdateVCFParameters();
    void UpdateEnvelopeParameters();
    void UpdateLFOParameters();
    
    /**
     * @brief Initialize factory presets
     */
    void InitFactoryPresets();
};
