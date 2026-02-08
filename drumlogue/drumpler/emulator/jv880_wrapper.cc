/*
 * JV-880 Emulator Wrapper for Korg drumlogue - Implementation
 */
#include "jv880_wrapper.h"
#include "mcu.h"
#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <new>
#ifdef PERF_MON
#include "../../common/perf_mon.h"
#endif

namespace drumpler {

// ============================================================================
// JV880Emulator Implementation
// ============================================================================

JV880Emulator::JV880Emulator()
    : mcu_(nullptr)
    , rom1_(nullptr)
    , rom2_(nullptr)
    , waverom1_(nullptr)
    , waverom2_(nullptr)
    , nvram_(nullptr)
    , waverom_exp_(nullptr)
    , cpu_load_(0.0f)
    , last_cycles_(0) {
    
    // Allocate MCU emulator
    mcu_ = new (std::nothrow) MCU();
}

JV880Emulator::~JV880Emulator() {
    if (mcu_) {
        delete mcu_;
        mcu_ = nullptr;
    }
}

bool JV880Emulator::Init(const uint8_t* rom_data, uint32_t rom_size) {
    if (!mcu_ || !rom_data || rom_size == 0) {
        return false;
    }
    
    // Validate and unpack ROM
    if (!ValidateROM(rom_data, rom_size)) {
        return false;
    }
    
    if (!UnpackROM(rom_data, rom_size)) {
        return false;
    }
    
    // Initialize emulator with ROM pointers
    // startSC55(rom1, rom2, waverom1, waverom2, nvram)
    // Waveroms are already unscrambled at build time by unscramble_waverom tool
    mcu_->startSC55(rom1_, rom2_, waverom1_, waverom2_, nvram_);
    
    // Load expansion ROM into PCM engine (already unscrambled at build time)
    if (waverom_exp_ != nullptr) {
        mcu_->pcm.waverom_exp = waverom_exp_;
    }
    
#ifdef PERF_MON
    // Register performance counters for emulator internals
    perf_mcu_update_ = PERF_MON_REGISTER("MCU_Update");
#endif
    
    return true;
}

bool JV880Emulator::ValidateROM(const uint8_t* rom_data, uint32_t rom_size) {
    (void)rom_data;
    // For now, just check minimum size
    // Full JV-880 ROM pack should be ~4.3 MB base + 0-8 MB expansion
    const uint32_t kMinRomSize = 0x200000 + 0x200000;  // 2MB + 2MB waveroms minimum
    
    if (rom_size < kMinRomSize) {
        return false;
    }
    
    // TODO: Add CRC or magic number checks
    
    return true;
}

bool JV880Emulator::UnpackROM(const uint8_t* rom_data, uint32_t rom_size) {
    // ROM layout (for JV-880):
    // rom1: 0x8000 (32KB)
    // rom2: 0x40000 (256KB)
    // waverom1: 0x200000 (2MB)
    // waverom2: 0x200000 (2MB)
    // nvram: 0x8000 (32KB) - optional, initialize to zeros if missing
    // waverom_exp: 0x800000 (8MB) - expansion ROM, optional
    
    uint32_t offset = 0;
    
    // ROM1 (32KB)
    if (offset + 0x8000 > rom_size) return false;
    rom1_ = rom_data + offset;
    offset += 0x8000;
    
    // ROM2 (256KB)
    if (offset + 0x40000 > rom_size) return false;
    rom2_ = rom_data + offset;
    offset += 0x40000;
    
    // WaveROM1 (2MB)
    if (offset + 0x200000 > rom_size) return false;
    waverom1_ = rom_data + offset;
    offset += 0x200000;
    
    // WaveROM2 (2MB)
    if (offset + 0x200000 > rom_size) return false;
    waverom2_ = rom_data + offset;
    offset += 0x200000;
    
    // NVRAM (32KB) - optional, initialize to zeros if missing
    if (offset + 0x8000 <= rom_size) {
        nvram_ = rom_data + offset;
        offset += 0x8000;
    } else {
        // TODO: Allocate zeroed NVRAM buffer
        nvram_ = nullptr;  // For now, pass nullptr (will need to handle in MCU)
    }
    
    // Expansion ROM (8MB) - optional (SR-JV80 series)
    if (offset + 0x800000 <= rom_size) {
        waverom_exp_ = rom_data + offset;
        offset += 0x800000;
        // Note: expansion ROM data is scrambled at this point.
        // UnscrambleAndLoadExpansionROM() is called after startSC55()
        // to unscramble and load into PCM engine.
    } else {
        waverom_exp_ = nullptr;
    }
    
    return true;
}

void JV880Emulator::Render(float* output_l, float* output_r, uint32_t frames, uint32_t sample_rate) {
    // Default to 48kHz if not specified
    if (sample_rate == 0) {
        sample_rate = 48000;
    }
    
    if (!mcu_ || frames == 0) {
#ifdef DEBUG
        static int error_count = 0;
        if (error_count++ < 3) {
            fprintf(stderr, "[Drumpler] jv880_wrapper.cc Render ERROR: mcu_=%p frames=%u\n", 
                    (void*)mcu_, frames);
            fflush(stderr);
        }
#endif
        // Silent output on error
        for (uint32_t i = 0; i < frames; ++i) {
            output_l[i] = 0.0f;
            output_r[i] = 0.0f;
        }
        return;
    }
    
    // MCU handles internal 64kHz rendering and resampling to output rate
    // This matches the JUCE plugin architecture where resampling happens inside the MCU
#ifdef PERF_MON
    PERF_MON_START(perf_mcu_update_);
#endif
    mcu_->updateSC55WithSampleRate(output_l, output_r,
                                   static_cast<unsigned int>(frames),
                                   static_cast<int>(sample_rate));
#ifdef PERF_MON
    PERF_MON_END(perf_mcu_update_);
#endif
}

void JV880Emulator::SendMidi(const uint8_t* data, int length) {
    if (!mcu_ || !data || length == 0 || length > 32) {
#ifdef DEBUG
        fprintf(stderr, "[Drumpler] SendMidi: invalid params (mcu_=%p, data=%p, length=%d)\n",
                (void*)mcu_, (void*)data, length);
        fflush(stderr);
#endif
        return;
    }
    
#ifdef DEBUG
    fprintf(stderr, "[Drumpler] SendMidi: posting to MCU: ");
    for (int i = 0; i < length; ++i) {
        fprintf(stderr, "%02X ", data[i]);
    }
    fprintf(stderr, "\n");
    fflush(stderr);
#endif
    
    mcu_->postMidiSC55(data, length);
}

void JV880Emulator::NoteOn(uint8_t channel, uint8_t note, uint8_t velocity) {
    if (velocity == 0) {
        NoteOff(channel, note);
        return;
    }
    
    uint8_t msg[3] = {
        static_cast<uint8_t>(0x90 | (channel & 0x0F)),  // Note On + channel
        static_cast<uint8_t>(note & 0x7F),
        static_cast<uint8_t>(velocity & 0x7F)
    };
    
#ifdef DEBUG
    fprintf(stderr, "[Drumpler] jv880_wrapper.cc NoteOn: ch=%d note=%d vel=%d → MIDI: %02X %02X %02X\n",
            channel, note, velocity, msg[0], msg[1], msg[2]);
    fflush(stderr);
#endif
    
    SendMidi(msg, 3);
}

void JV880Emulator::NoteOff(uint8_t channel, uint8_t note) {
    uint8_t msg[3] = {
        static_cast<uint8_t>(0x80 | (channel & 0x0F)),  // Note Off + channel
        static_cast<uint8_t>(note & 0x7F),
        0x00  // Release velocity (typically 0)
    };
    
    SendMidi(msg, 3);
}

void JV880Emulator::ControlChange(uint8_t channel, uint8_t cc, uint8_t value) {
    uint8_t msg[3] = {
        static_cast<uint8_t>(0xB0 | (channel & 0x0F)),  // Control Change + channel
        static_cast<uint8_t>(cc & 0x7F),
        static_cast<uint8_t>(value & 0x7F)
    };
    
    SendMidi(msg, 3);
}

void JV880Emulator::ProgramChange(uint8_t channel, uint8_t program) {
    uint8_t msg[2] = {
        static_cast<uint8_t>(0xC0 | (channel & 0x0F)),  // Program Change + channel
        static_cast<uint8_t>(program & 0x7F)
    };
    
#ifdef DEBUG
    fprintf(stderr, "[Drumpler] jv880_wrapper.cc ProgramChange: ch=%d prog=%d → MIDI: %02X %02X\n",
            channel, program, msg[0], msg[1]);
    fflush(stderr);
#endif
    
    SendMidi(msg, 2);
}

bool JV880Emulator::SetCurrentProgram(uint8_t index) {
    if (!mcu_) return false;
    
    static constexpr int kPatchSize = 0x16a;
    const uint8_t* patch_data = nullptr;
    
    if (waverom_exp_ != nullptr) {
        // Expansion ROM: read patch count and offset from ROM header
        uint16_t nPatches = (uint16_t)(waverom_exp_[0x66] << 8) | waverom_exp_[0x67];
        uint32_t patchesOffset = 
            ((uint32_t)waverom_exp_[0x8c] << 24) |
            ((uint32_t)waverom_exp_[0x8d] << 16) |
            ((uint32_t)waverom_exp_[0x8e] << 8) |
            ((uint32_t)waverom_exp_[0x8f]);
        
        if (index < nPatches) {
            patch_data = &waverom_exp_[patchesOffset + index * kPatchSize];
        }
    } else if (rom2_) {
        // Internal ROM patches
        if (index < 64) {
            // Internal A: rom2 + 0x010ce0 + index * 0x16a
            patch_data = &rom2_[0x010ce0 + index * kPatchSize];
        } else if (index < 128) {
            // Internal B: rom2 + 0x018ce0 + (index-64) * 0x16a
            patch_data = &rom2_[0x018ce0 + (index - 64) * kPatchSize];
        }
    }
    
    if (!patch_data) {
#ifdef DEBUG
        fprintf(stderr, "[Drumpler] SetCurrentProgram: no patch data for index %d\n", index);
        fflush(stderr);
#endif
        return false;
    }
    
#ifdef DEBUG
    // Print first 12 bytes (patch name) for debugging
    fprintf(stderr, "[Drumpler] SetCurrentProgram(%d): copying 0x%x bytes to nvram[0x0d70], name='",
            index, kPatchSize);
    for (int i = 0; i < 12 && i < kPatchSize; ++i) {
        char c = static_cast<char>(patch_data[i]);
        if (c >= 0x20 && c <= 0x7E) fprintf(stderr, "%c", c);
        else fprintf(stderr, ".");
    }
    fprintf(stderr, "'\n");
    fflush(stderr);
#endif
    
    // Copy full patch data (0x16a bytes) to nvram at offset 0x0d70
    // This matches JUCE setCurrentProgram() behavior
    memcpy(&mcu_->nvram[0x0d70], patch_data, kPatchSize);
    
    // Check if we need a full reset or just a dummy program change
    if (mcu_->nvram[0x11] != 1) {
        // Switch to patch mode (from drum mode) - requires full reset
        mcu_->nvram[0x11] = 1;
#ifdef DEBUG
        fprintf(stderr, "[Drumpler] SetCurrentProgram: switching to patch mode, doing SC55_Reset\n");
        fflush(stderr);
#endif
        mcu_->SC55_Reset();
    } else {
        // Already in patch mode - send dummy Program Change to trigger firmware reload
        uint8_t buffer[2] = {0xC0, 0x00};
        mcu_->postMidiSC55(buffer, 2);
    }
    
    return true;
}

void JV880Emulator::SendSysexPatchCommonParam(uint8_t offset, uint8_t value) {
    if (!mcu_) return;
    
    // Roland SysEx DT1 format for JV-880 Patch Common parameters:
    // F0 41 10 46 12 00 08 20 <offset> <value> <checksum> F7
    uint8_t buf[12];
    buf[0]  = 0xF0;   // SysEx start
    buf[1]  = 0x41;   // Roland manufacturer ID
    buf[2]  = 0x10;   // Unit number (default 17 = 0x10)
    buf[3]  = 0x46;   // JV-880 model ID
    buf[4]  = 0x12;   // DT1 (Data Set 1) command
    buf[5]  = 0x00;   // Address MSB
    buf[6]  = 0x08;   // Address
    buf[7]  = 0x20;   // Address (Patch Common block)
    buf[8]  = offset & 0x7F;  // Address LSB (parameter offset)
    buf[9]  = value & 0x7F;   // Data
    
    // Roland checksum: 128 - (sum of address+data bytes % 128)
    uint32_t checksum = 0;
    for (int i = 5; i <= 9; i++) {
        checksum += buf[i];
    }
    checksum = 128 - (checksum % 128);
    if (checksum == 128) checksum = 0;
    
    buf[10] = static_cast<uint8_t>(checksum);
    buf[11] = 0xF7;   // SysEx end
    
#ifdef DEBUG
    fprintf(stderr, "[Drumpler] SysEx PatchCommon: offset=0x%02x value=%d checksum=0x%02x\n",
            offset, value, buf[10]);
    fflush(stderr);
#endif
    
    SendMidi(buf, 12);
}

void JV880Emulator::SendSysexPatchToneParam(uint8_t tone, uint8_t offset, uint8_t value) {
    if (!mcu_ || tone > 3) return;
    
    // Roland SysEx DT1 format for JV-880 Patch Tone parameters:
    // Tone base addresses: 0x28 + toneCount (matching JUCE EditToneTab)
    // F0 41 10 46 12 00 08 <0x28+tone> <offset> <value> <checksum> F7
    static const uint8_t tone_base[] = { 0x28, 0x29, 0x2A, 0x2B };
    
    uint8_t buf[12];
    buf[0]  = 0xF0;
    buf[1]  = 0x41;
    buf[2]  = 0x10;
    buf[3]  = 0x46;
    buf[4]  = 0x12;
    buf[5]  = 0x00;
    buf[6]  = 0x08;
    buf[7]  = tone_base[tone];
    buf[8]  = offset & 0x7F;
    buf[9]  = value & 0x7F;
    
    uint32_t checksum = 0;
    for (int i = 5; i <= 9; i++) {
        checksum += buf[i];
    }
    checksum = 128 - (checksum % 128);
    if (checksum == 128) checksum = 0;
    
    buf[10] = static_cast<uint8_t>(checksum);
    buf[11] = 0xF7;
    
#ifdef DEBUG
    fprintf(stderr, "[Drumpler] SysEx PatchTone: tone=%d offset=0x%02x value=%d\n",
            tone, offset, value);
    fflush(stderr);
#endif
    
    SendMidi(buf, 12);
}

void JV880Emulator::SendSysexSystemParam(uint32_t address, uint8_t value) {
    if (!mcu_) return;
    
    // Roland SysEx DT1 format for JV-880 System parameters:
    // F0 41 10 46 12 <addr3> <addr2> <addr1> <addr0> <value> <checksum> F7
    // Matches JUCE VirtualJVProcessor::sendSysexParamChange()
    uint8_t buf[12];
    buf[0]  = 0xF0;
    buf[1]  = 0x41;
    buf[2]  = 0x10;
    buf[3]  = 0x46;
    buf[4]  = 0x12;
    buf[5]  = (address >> 21) & 0x7F;
    buf[6]  = (address >> 14) & 0x7F;
    buf[7]  = (address >> 7) & 0x7F;
    buf[8]  = (address >> 0) & 0x7F;
    buf[9]  = value & 0x7F;
    
    uint32_t checksum = 0;
    for (int i = 5; i <= 9; i++) {
        checksum += buf[i];
    }
    checksum = 128 - (checksum % 128);
    if (checksum == 128) checksum = 0;
    
    buf[10] = static_cast<uint8_t>(checksum);
    buf[11] = 0xF7;
    
#ifdef DEBUG
    fprintf(stderr, "[Drumpler] SysEx System: addr=0x%08x value=%d\n", address, value);
    fflush(stderr);
#endif
    
    SendMidi(buf, 12);
}

void JV880Emulator::Reset() {
    if (!mcu_) return;
    
    mcu_->SC55_Reset();
}

bool JV880Emulator::GetPatchName(uint8_t index, char* name, int max_len) const {
    if (!name || max_len < 2) return false;
    
    // Patch struct is 0x16a bytes, name is first 12 bytes (char name[12])
    static constexpr int kPatchSize = 0x16a;
    static constexpr int kNameLen = 12;
    
    const uint8_t* name_ptr = nullptr;
    
    if (waverom_exp_ != nullptr) {
        // Expansion ROM: read patch count and offset from ROM header
        // Number of patches at bytes 0x66-0x67 (big-endian 16-bit)
        uint16_t nPatches = (uint16_t)(waverom_exp_[0x66] << 8) | waverom_exp_[0x67];
        
        // Patches offset at bytes 0x8c-0x8f (big-endian 32-bit)
        uint32_t patchesOffset = 
            ((uint32_t)waverom_exp_[0x8c] << 24) |
            ((uint32_t)waverom_exp_[0x8d] << 16) |
            ((uint32_t)waverom_exp_[0x8e] << 8) |
            ((uint32_t)waverom_exp_[0x8f]);
        
        if (index < nPatches) {
            name_ptr = &waverom_exp_[patchesOffset + index * kPatchSize];
        }
    } else if (rom2_ != nullptr) {
        // Internal ROM: patches stored in ROM2
        // Internal A (0-63):  rom2 + 0x010ce0
        // Internal B (64-127): rom2 + 0x018ce0
        if (index < 64) {
            name_ptr = &rom2_[0x010ce0 + index * kPatchSize];
        } else if (index < 128) {
            name_ptr = &rom2_[0x018ce0 + (index - 64) * kPatchSize];
        }
    }
    
    if (!name_ptr) {
        // Fallback: numeric name
        int len = max_len - 1;
        if (len > 4) len = 4;
        name[0] = 'P';
        name[1] = '0' + (index / 100) % 10;
        name[2] = '0' + (index / 10) % 10;
        name[3] = '0' + index % 10;
        name[len] = '\0';
        return false;
    }
    
    // Copy name, trimming trailing spaces, max kNameLen or max_len-1
    int copy_len = kNameLen;
    if (copy_len >= max_len) copy_len = max_len - 1;
    
    for (int i = 0; i < copy_len; ++i) {
        char c = static_cast<char>(name_ptr[i]);
        // Ensure 7-bit ASCII printable
        if (c < 0x20 || c > 0x7E) c = ' ';
        name[i] = c;
    }
    
    // Trim trailing spaces
    int end = copy_len;
    while (end > 0 && name[end - 1] == ' ') --end;
    name[end] = '\0';
    
    return true;
}

}  // namespace drumpler
