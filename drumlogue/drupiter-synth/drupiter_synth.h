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
#include "../common/hub_control.h"
#include "../common/param_format.h"

// Maximum frames per buffer (drumlogue typically uses 64)
// Maximum audio buffer size (drumlogue uses 64 frames max)
// Add 8 extra floats as safety margin for NEON operations and alignment
static constexpr uint32_t kMaxFrames = 64;
static constexpr uint32_t kBufferSize = kMaxFrames + 8;  // Safety margin

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

// Octave names (DCO1: 16'/8'/4', DCO2: 32'/16'/8'/4')
static const char* const kOctave1Names[] = {
    "16'", "8'", "4'"
};

static const char* const kOctave2Names[] = {
    "32'", "16'", "8'", "4'"
};

// Sync mode names
static const char* const kSyncNames[] = {
    "OFF", "SOFT", "HARD"
};

// ============================================================================
// MOD HUB - Jupiter-8 style modulation routing
// ============================================================================

/**
 * @brief MOD HUB destinations (9 total, matching UI.md Phase 1 design)
 */
enum ModDestination {
    MOD_LFO_TO_PWM = 0,    // LFO modulates pulse width (both VCOs)
    MOD_LFO_TO_VCF,        // LFO modulates filter cutoff
    MOD_LFO_TO_VCO,        // LFO modulates pitch (vibrato)
    MOD_ENV_TO_PWM,        // Filter ENV modulates pulse width
    MOD_ENV_TO_VCF,        // Filter ENV modulation (bipolar ±4 octaves)
    MOD_HPF,               // High-pass filter cutoff
    MOD_VCF_TYPE,          // Filter type selection (LP12/LP24/HP12/BP)
    MOD_LFO_DELAY,         // LFO delay (fade-in time)
    MOD_LFO_WAVE,          // LFO waveform selection
    MOD_LFO_ENV_AMT,       // Envelope modulation of LFO rate
    MOD_VCA_LEVEL,         // VCA output level (pre-envelope)
    MOD_VCA_LFO,           // LFO modulation of VCA (tremolo)
    MOD_VCA_KYBD,          // VCA keyboard tracking
    MOD_ENV_KYBD,          // Envelope keyboard tracking (faster at high notes)
    MOD_NUM_DESTINATIONS
};

// VCF filter type names (for VCF TYP hub mode)
static const char* const kVcfTypeNames[] = {
    "LP12", "LP24", "HP12", "BP12"
};

// LFO waveform names (for LFO WAV hub mode)
static const char* const kLfoWaveNames[] = {
    "TRI", "RAMP", "SQR", "S&H"
};

// Hub control destinations for MOD HUB
static constexpr common::HubControl<MOD_NUM_DESTINATIONS>::Destination kModDestinations[] = {
    {"LFO>PWM", "%",   0, 100, 0,  false, nullptr},        // LFO to pulse width
    {"LFO>VCF", "%",   0, 100, 0,  false, nullptr},        // LFO to filter
    {"LFO>VCO", "%",   0, 100, 0,  false, nullptr},        // LFO vibrato
    {"ENV>PWM", "%",   0, 100, 0,  false, nullptr},        // ENV to pulse width
    {"ENV>VCF", "%",   0, 100, 50, true,  nullptr},        // ENV to filter (bipolar!)
    {"HPF",     "Hz",  0, 100, 0,  false, nullptr},        // High-pass filter
    {"VCF TYP", "",    0, 3,   1,  false, kVcfTypeNames},  // Filter type (enum)
    {"LFO DLY", "ms",  0, 100, 0,  false, nullptr},        // LFO delay
    {"LFO WAV", "",    0, 3,   0,  false, kLfoWaveNames},  // LFO waveform (TRI/RAMP/SQR/S&H)
    {"ENV>LFO", "%",   0, 100, 0,  false, nullptr},        // ENV modulates LFO rate
    {"VCA LVL", "%",   0, 100, 100, false, nullptr},       // VCA output level
    {"LFO>VCA", "%",   0, 100, 0,  false, nullptr},        // LFO to VCA (tremolo)
    {"VCA KYB", "%",   0, 100, 0,  false, nullptr},        // VCA keyboard tracking
    {"ENV KYB", "%",   0, 100, 50, false, nullptr}         // ENV keyboard tracking
};

// Effect mode names
static const char* const kEffectNames[] = {
    "CHORUS", "SPACE", "DRY", "BOTH"
};

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
        PARAM_VCF_CUTOFF,        // 0-100: Filter cutoff
        PARAM_VCF_RESONANCE,     // 0-100: Filter resonance
        PARAM_VCF_KEYFLW,        // 0-100: Keyboard tracking (NEW!)
        
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
        
        // Page 6: Modulation (REDESIGNED!)
        PARAM_LFO_RATE,          // 0-100: Direct LFO rate control (NEW!)
        PARAM_MOD_HUB,           // 0-7: Modulation destination selector
        PARAM_MOD_AMT,           // 0-100: Amount for selected destination
        PARAM_EFFECT,            // 0-1: Effect mode (CHORUS/SPACE)
        
        PARAM_COUNT = 24
    };

    // Preset type (forward declaration from presets.h)
    struct Preset {
        uint8_t params[PARAM_COUNT];
        uint8_t hub_values[MOD_NUM_DESTINATIONS];
        char name[14];
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
    
    /**
     * @brief Get current preset index
     * @return Current preset index (0-5)
     */
    uint8_t GetPresetIndex() const { return current_preset_idx_; }
    
    /**
     * @brief Get preset name
     * @param preset_id Preset index (0-5)
     * @return Preset name
     */
    const char* GetPresetName(uint8_t preset_id) const;

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
    uint8_t current_preset_idx_;
    
    // Jupiter-8 specific oscillator controls
    uint8_t sync_mode_;            // 0=OFF, 1=SOFT, 2=HARD
    float xmod_depth_;             // Cross-mod depth 0-1
    
    // MOD HUB - Using common HubControl system (Phase 1 UI redesign)
    common::HubControl<MOD_NUM_DESTINATIONS> mod_hub_;
    mutable char mod_value_str_[16];  // Buffer for GetParameterStr display
    
    // Effect mode (0=CHORUS, 1=SPACE, 2=DRY, 3=BOTH)
    uint8_t effect_mode_;
    
    // Internal audio buffers (avoid dynamic allocation)
    float dco1_out_;
    float dco2_out_;
    float noise_out_;
    float mixed_;
    float filtered_;
    float vcf_env_out_;
    float vca_env_out_;
    float lfo_out_;
    
    // NEON-aligned intermediate buffers for block processing (with safety margin)
    alignas(16) float mix_buffer_[kBufferSize];
    alignas(16) float left_buffer_[kBufferSize];
    alignas(16) float right_buffer_[kBufferSize];
    
    // Guard value to detect buffer overflows (will be 0xDEADBEEF)
    uint64_t buffer_guard_;
    
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
    
    // HPF state variables (must be instance members, not static)
    float hpf_prev_output_;
    float hpf_prev_input_;
    
    /**
     * @brief Convert DCO1 octave parameter to frequency multiplier
     * @param octave_param Parameter value 0-2 (16'/8'/4')
     * @return Frequency multiplier (0.5, 1.0, 2.0)
     */
    float Dco1OctaveToMultiplier(uint8_t octave_param) const;
    
    /**
     * @brief Convert DCO2 octave parameter to frequency multiplier
     * @param octave_param Parameter value 0-3 (32'/16'/8'/4')
     * @return Frequency multiplier (0.25, 0.5, 1.0, 2.0)
     */
    float Dco2OctaveToMultiplier(uint8_t octave_param) const;
    
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
};
