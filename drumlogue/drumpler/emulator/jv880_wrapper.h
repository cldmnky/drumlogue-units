/*
 * JV-880 Emulator Wrapper for Korg drumlogue
 * 
 * Wraps NukeYKT's Nuked-SC55/JV-880 emulator for drumlogue hardware.
 * Removes JUCE dependencies and provides drumlogue-compatible audio/MIDI interface.
 * 
 * Resampling now handled internally by MCU (matches JUCE plugin architecture)
 * where the MCU renders at 64kHz and resamples to output rate using libresample.
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
     * Process audio (float stereo at configured sample rate)
     * MCU handles internal 64kHz→output_rate resampling using libresample
     * @param output_l Left channel output buffer
     * @param output_r Right channel output buffer
     * @param frames Number of frames to render (typically 32-64)
     * @param sample_rate Output sample rate (e.g., 48000, 44100). If 0, uses 48000.
     */
    void Render(float* output_l, float* output_r, uint32_t frames, uint32_t sample_rate = 48000);
    
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
     * Send MIDI program change (low-level MIDI only)
     * NOTE: For switching patches, use SetCurrentProgram() instead.
     * Standard MIDI Program Change alone does NOT work for JV-880 patch switching;
     * the firmware requires patch data to be written directly to nvram.
     * @param channel MIDI channel (0-15)
     * @param program Program number (0-127)
     */
    void ProgramChange(uint8_t channel, uint8_t program);
    
    /**
     * Set current program by copying patch data directly to MCU nvram.
     * This matches the JUCE plugin's setCurrentProgram() behavior.
     * The JV-880 firmware reads patch data from nvram, not via MIDI Program Change.
     *
     * For Internal ROM:
     *   - 0-63: Internal A patches (rom2 offset 0x010ce0)
     *   - 64-127: Internal B patches (rom2 offset 0x018ce0)
     * For Expansion ROM:
     *   - Reads patch count and offset from ROM header
     *
     * @param index Program number (0-127)
     * @return true if patch was loaded successfully
     */
    bool SetCurrentProgram(uint8_t index);
    
    /**
     * Send Roland SysEx DT1 (Data Set 1) message for a Patch Common parameter.
     * The JV-880 uses proprietary SysEx for real-time parameter changes.
     * Address format: F0 41 10 46 12 00 08 20 <offset> <value> <checksum> F7
     *
     * Known offsets:
     *   0x0d: Reverb Type     0x0e: Reverb Level    0x0f: Reverb Time
     *   0x10: Delay Feedback   0x11: Chorus Type     0x12: Chorus Level
     *   0x13: Chorus Depth     0x14: Chorus Rate     0x15: Chorus Feedback
     *   0x16: Chorus Output
     *
     * @param offset Parameter offset within Patch Common block
     * @param value Parameter value (0-127)
     */
    void SendSysexPatchCommonParam(uint8_t offset, uint8_t value);
    
    /**
     * Send Roland SysEx DT1 message for a Patch Tone parameter.
     * Address format: F0 41 10 46 12 00 08 <0x28+tone> <offset> <value> <checksum> F7
     *
     * Tone base addresses: 0x28 + toneCount (0x28, 0x29, 0x2A, 0x2B)
     * Matches JUCE EditToneTab::sendSysexPatchToneChange1Byte()
     *
     * Key parameter offsets (from JUCE EditToneTab):
     *   TVF Cutoff Frequency: 0x4A
     *   TVF Resonance: 0x4B
     *   TVA Env Time 1 (Attack): 0x69
     *   TVA Level: 0x5C
     *   Dry Send: 0x70, Reverb Send: 0x71, Chorus Send: 0x72
     *
     * @param tone Tone number (0-3)
     * @param offset Parameter offset within Patch Tone block (0-127)
     * @param value Parameter value (0-127)
     */
    void SendSysexPatchToneParam(uint8_t tone, uint8_t offset, uint8_t value);
    
    /**
     * Send Roland SysEx DT1 message for a System parameter.
     * Address format: F0 41 10 46 12 <addr3> <addr2> <addr1> <addr0> <value> <checksum> F7
     *
     * Key system addresses (from JUCE SettingsTab):
     *   0x04: Reverb Switch (0=off, 1=on)
     *   0x05: Chorus Switch (0=off, 1=on)
     *   0x01: Master Tune
     *
     * @param address Full 32-bit system address (each byte is 7-bit)
     * @param value Parameter value (0-127)
     */
    void SendSysexSystemParam(uint32_t address, uint8_t value);
    
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
    
#ifdef PERF_MON
    uint8_t perf_mcu_update_ = 0xFF;
#endif
    
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
    
};

}  // namespace drumpler
