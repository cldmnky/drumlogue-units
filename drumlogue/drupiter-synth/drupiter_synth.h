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

// Forward declarations of DSP components
namespace dsp {
    class JupiterDCO;
    class JupiterVCF;
    class JupiterEnvelope;
    class JupiterLFO;
    class SmoothedValue;
}

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
        PARAM_DCO1_OCTAVE = 0,
        PARAM_DCO1_WAVE,
        PARAM_DCO1_PW,
        PARAM_DCO1_LEVEL,
        
        // Page 2: DCO-2
        PARAM_DCO2_OCTAVE,
        PARAM_DCO2_WAVE,
        PARAM_DCO2_DETUNE,
        PARAM_DCO2_LEVEL,
        
        // Page 3: VCF
        PARAM_VCF_CUTOFF,
        PARAM_VCF_RESONANCE,
        PARAM_VCF_ENV_AMT,
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
        
        // Page 6: LFO & Modulation
        PARAM_LFO_RATE = 20,
        PARAM_LFO_WAVE,
        PARAM_LFO_VCO_DEPTH,
        PARAM_LFO_VCF_DEPTH,  // LFO to VCF cutoff modulation
        
        // Note: Removed PARAM_XMOD_DEPTH to stay at 24 params
        // Xmod is now always active when DCO2 level > 0
        
        PARAM_COUNT = 24
    };

    /**
     * @brief Preset structure
     */
    struct Preset {
        uint8_t params[PARAM_COUNT];  // Parameter values 0-127
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
    
    // Internal audio buffers (avoid dynamic allocation)
    float dco1_out_;
    float dco2_out_;
    float noise_out_;
    float mixed_;
    float filtered_;
    float vcf_env_out_;
    float vca_env_out_;
    float lfo_out_;
    
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
     * @brief Convert parameter value to time with exponential scaling
     * @param value Parameter value 0-127
     * @return Time in seconds (0.001 to 5.0)
     */
    static float ParameterToEnvelopeTime(uint8_t value);
    
    /**
     * @brief Convert parameter value to frequency with exponential scaling
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
