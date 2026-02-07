/*
 * JV-880 Emulator Wrapper for Korg drumlogue
 * 
 * Wraps NukeYKT's Nuked-SC55/JV-880 emulator for drumlogue hardware.
 * Removes JUCE dependencies, handles sample rate conversion, and provides
 * drumlogue-compatible audio/MIDI interface.
 * 
 * Original emulator: https://github.com/nukeykt/Nuked-SC55
 * JUCE port: https://github.com/giulioz/jv880_juce
 * 
 * License: Non-commercial use only (MAME-style BSD)
 */
#pragma once

#include <stdint.h>
#include <string.h>

// Forward declaration - MCU from emulator
struct MCU;

namespace drumpler {

/**
 * Simple linear interpolation resampler (64kHz → 48kHz)
 */
class LinearResampler {
public:
    LinearResampler();
    ~LinearResampler();
    
    /**
     * Resample from 64kHz internal buffer to 48kHz output
     * @param input_l Left channel input (64kHz float samples)
     * @param input_r Right channel input (64kHz float samples)
     * @param input_frames Number of input frames
     * @param output_l Left channel output (48kHz)
     * @param output_r Right channel output (48kHz)
     * @param output_frames Number of output frames requested
     * @return Actual number of output frames generated
     */
    int Resample(const float* input_l, const float* input_r, int input_frames,
                 float* output_l, float* output_r, int output_frames);
    
    void Reset();
    
private:
    // State for fractional sample position
    float pos_;          // Current fractional position in input buffer
    
    static constexpr float kRate64kHz = 64000.0f;
    static constexpr float kRate48kHz = 48000.0f;
    static constexpr float kRatio = kRate64kHz / kRate48kHz;  // 4/3
};

/**
 * Main JV-880 emulator wrapper for drumlogue
 */
class JV880Emulator {
public:
    JV880Emulator();
    ~JV880Emulator();
    
    /**
     * Initialize emulator with embedded ROM data
     * @param rom_data Pointer to embedded ROM binary
     * @param rom_size Size of ROM binary
     * @return true on success
     */
    bool Init(const uint8_t* rom_data, uint32_t rom_size);
    
    /**
     * Process audio (48kHz float stereo)
     * @param output_l Left channel output buffer
     * @param output_r Right channel output buffer
     * @param frames Number of frames to render (typically 32-64)
     */
    void Render(float* output_l, float* output_r, uint32_t frames);
    
    /**
     * Send MIDI message immediately
     * @param data MIDI bytes
     * @param length Number of bytes
     */
    void SendMidi(const uint8_t* data, int length);
    
    /**
     * Send MIDI note on
     * @param channel MIDI channel (0-15)
     * @param note Note number (0-127)
     * @param velocity Velocity (0-127), 0 = note off
     */
    void NoteOn(uint8_t channel, uint8_t note, uint8_t velocity);
    
    /**
     * Send MIDI note off
     * @param channel MIDI channel (0-15)
     * @param note Note number (0-127)
     */
    void NoteOff(uint8_t channel, uint8_t note);
    
    /**
     * Send MIDI control change
     * @param channel MIDI channel (0-15)
     * @param cc Controller number (0-127)
     * @param value Controller value (0-127)
     */
    void ControlChange(uint8_t channel, uint8_t cc, uint8_t value);
    
    /**
     * Send MIDI program change
     * @param channel MIDI channel (0-15)
     * @param program Program number (0-127)
     */
    void ProgramChange(uint8_t channel, uint8_t program);
    
    /**
     * Reset emulator (GS Reset)
     */
    void Reset();
    
    /**
     * Get patch name from ROM for a given program index
     * @param index Program number (0-127)
     * @param name Output buffer for null-terminated name string
     * @param max_len Maximum buffer length (should be >= 13 for 12-char name + NUL)
     * @return true if name was found
     */
    bool GetPatchName(uint8_t index, char* name, int max_len) const;
    
    /**
     * Check if expansion ROM is loaded
     */
    bool HasExpansionROM() const { return waverom_exp_ != nullptr; }
    
    /**
     * Get CPU load estimate (0.0 - 1.0)
     */
    float GetCpuLoad() const { return cpu_load_; }
    
private:
    // Emulator core (defined in mcu.h)
    MCU* mcu_;
    
    // Resampling (64kHz → 48kHz)
    LinearResampler resampler_;
    
    // Internal 64kHz render buffer
    static constexpr int kMaxRenderFrames = 256;  // Max frames per Render() call
    static constexpr int kInternalBufferSize = (kMaxRenderFrames * 64000) / 48000 + 32;  // ~384 frames
    float internal_buffer_l_[kInternalBufferSize];
    float internal_buffer_r_[kInternalBufferSize];
    
    // ROM storage (persistent pointers for PCM class)
    const uint8_t* rom1_;
    const uint8_t* rom2_;
    const uint8_t* waverom1_;
    const uint8_t* waverom2_;
    const uint8_t* nvram_;
    const uint8_t* waverom_exp_;  // Expansion ROM (SR-JV80 series)
    
    // CPU load tracking
    float cpu_load_;
    uint64_t last_cycles_;
    
    // ROM validation and unpacking
    bool ValidateROM(const uint8_t* rom_data, uint32_t rom_size);
    bool UnpackROM(const uint8_t* rom_data, uint32_t rom_size);
    
    /**
     * Unscramble expansion ROM and load into PCM engine.
     * Roland SR-JV80 expansion ROMs use address + data bit scrambling.
     * Source: mcu.cpp unscramble() / rom.cpp unscrambleRom() from jv880_juce.
     */
    void UnscrambleAndLoadExpansionROM();
    
};

}  // namespace drumpler
