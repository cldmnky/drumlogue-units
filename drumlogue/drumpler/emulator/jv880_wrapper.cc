/*
 * JV-880 Emulator Wrapper for Korg drumlogue - Implementation
 */
#include "jv880_wrapper.h"
#include "mcu.h"
#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <new>

namespace drumpler {

// ============================================================================
// LinearResampler Implementation
// ============================================================================

LinearResampler::LinearResampler()
    : pos_(0.0f) {
}

LinearResampler::~LinearResampler() {
}

void LinearResampler::Reset() {
    pos_ = 0.0f;
}

int LinearResampler::Resample(const float* input_l, const float* input_r, int input_frames,
                                float* output_l, float* output_r, int output_frames) {
    int out_idx = 0;
    
    // Ratio is 64000/48000 = 4/3 = 1.333...
    // For each output sample, we advance by 4/3 input samples
    
    // Reset position if it's beyond the current input buffer
    if (pos_ >= static_cast<float>(input_frames - 1)) {
        pos_ = 0.0f;
    }
    
    for (int i = 0; i < output_frames; ++i) {
        // Current fractional position
        int idx = static_cast<int>(pos_);
        float frac = pos_ - static_cast<float>(idx);
        
        // Bounds check
        if (idx + 1 >= input_frames) break;
        
        // Linear interpolation
        float s0_l = input_l[idx];
        float s0_r = input_r[idx];
        float s1_l = input_l[idx + 1];
        float s1_r = input_r[idx + 1];
        
        output_l[i] = s0_l + frac * (s1_l - s0_l);
        output_r[i] = s0_r + frac * (s1_r - s0_r);
        
        // Advance position
        pos_ += kRatio;
        out_idx++;
    }
    
    // Update state for next call - wrap position relative to buffer
    if (input_frames > 0) {
        while (pos_ >= input_frames) {
            pos_ -= input_frames;  // Wrap position back
        }
    }
    
    return out_idx;
}

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
    mcu_->startSC55(rom1_, rom2_, waverom1_, waverom2_, nvram_);
    
    // Unscramble expansion ROM and load into PCM engine
    // Must be called AFTER startSC55() since it writes to mcu_->pcm.waverom_exp
    if (waverom_exp_ != nullptr) {
        UnscrambleAndLoadExpansionROM();
    }
    
    // Reset resamplers
    resampler_.Reset();
    
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

void JV880Emulator::Render(float* output_l, float* output_r, uint32_t frames) {
    if (!mcu_ || frames == 0 || frames > kMaxRenderFrames) {
#ifdef DEBUG
        static int error_count = 0;
        if (error_count++ < 3) {
            fprintf(stderr, "[Drumpler] jv880_wrapper.cc Render ERROR: mcu_=%p frames=%u (max=%d)\n", 
                    (void*)mcu_, frames, kMaxRenderFrames);
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
    
    // Calculate number of 64kHz frames needed
    // frames @ 48kHz → (frames * 64000 / 48000) @ 64kHz
    int internal_frames = (int)ceilf((float)frames * 64000.0f / 48000.0f) + 1;
    
    if (internal_frames > kInternalBufferSize) {
        internal_frames = kInternalBufferSize;
    }
    
#ifdef DEBUG
    static int render_count = 0;
    bool should_debug = (render_count < 5) || (render_count >= 750 && render_count < 760);
    if (should_debug) {
        render_count++;
        fprintf(stderr, "[Drumpler] jv880_wrapper.cc Render: frames=%u, internal_frames=%d\n", 
                frames, internal_frames);
        fflush(stderr);
    } else {
        render_count++;
    }
#endif
    
    // Render at 64kHz into internal buffer
    mcu_->updateSC55WithSampleRate(internal_buffer_l_, internal_buffer_r_,
                                   static_cast<unsigned int>(internal_frames),
                                   64000);
    
#ifdef DEBUG
    if (should_debug) {
        // Check MCU output
        float max_internal = 0.0f;
        for (int i = 0; i < internal_frames; ++i) {
            float abs_l = fabsf(internal_buffer_l_[i]);
            float abs_r = fabsf(internal_buffer_r_[i]);
            if (abs_l > max_internal) max_internal = abs_l;
            if (abs_r > max_internal) max_internal = abs_r;
        }
        fprintf(stderr, "[Drumpler] jv880_wrapper.cc Render: MCU output max=%f\n", max_internal);
        fflush(stderr);
    }
#endif
    
    // Resample to 48kHz
    int actual_frames = resampler_.Resample(
        internal_buffer_l_, internal_buffer_r_, internal_frames,
        output_l, output_r, frames
    );
    
#ifdef DEBUG
    if (should_debug) {
        // Check resampler output
        float max_output = 0.0f;
        for (int i = 0; i < actual_frames; ++i) {
            float abs_l = fabsf(output_l[i]);
            float abs_r = fabsf(output_r[i]);
            if (abs_l > max_output) max_output = abs_l;
            if (abs_r > max_output) max_output = abs_r;
        }
        fprintf(stderr, "[Drumpler] jv880_wrapper.cc Render: resampled output max=%f (actual_frames=%d)\n", 
                max_output, actual_frames);
        fflush(stderr);
    }
#endif
    
    // Fill remaining with zeros if resampler didn't produce enough
    for (int i = actual_frames; i < (int)frames; ++i) {
        output_l[i] = 0.0f;
        output_r[i] = 0.0f;
    }
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

void JV880Emulator::Reset() {
    if (!mcu_) return;
    
    mcu_->SC55_Reset();
    
    resampler_.Reset();
}

void JV880Emulator::UnscrambleAndLoadExpansionROM() {
    if (!mcu_ || !waverom_exp_) return;
    
    // Roland SR-JV80 expansion ROMs use address + data bit scrambling.
    // This unscramble algorithm is from mcu.cpp unscramble() / rom.cpp unscrambleRom()
    // in the jv880_juce project.
    //
    // Address bits are permuted using aa[] lookup table (20-bit address space, 1MB blocks)
    // Data bits are permuted using dd[] lookup table (8-bit)
    
    static const int aa[] = {
        2, 0, 3, 4, 1, 9, 13, 10, 18, 17, 6, 15, 11, 16, 8, 5, 12, 7, 14, 19
    };
    static const int dd[] = {
        2, 0, 4, 5, 7, 6, 3, 1
    };
    
    const int len = 0x800000;  // 8MB expansion ROM
    uint8_t* dst = mcu_->pcm.waverom_exp;
    const uint8_t* src = waverom_exp_;
    
    for (int i = 0; i < len; i++) {
        // Address unscramble: permute address bits within 1MB blocks
        int address = i & ~0xfffff;  // Keep upper bits (block selector)
        for (int j = 0; j < 20; j++) {
            if (i & (1 << j))
                address |= 1 << aa[j];
        }
        
        // Read source byte from scrambled address
        uint8_t srcdata = src[address];
        
        // Data unscramble: permute data bits
        uint8_t data = 0;
        for (int j = 0; j < 8; j++) {
            if (srcdata & (1 << dd[j]))
                data |= 1 << j;
        }
        
        dst[i] = data;
    }
    
    // Update waverom_exp_ to point to the unscrambled copy in PCM
    // This ensures GetPatchName() reads unscrambled data
    waverom_exp_ = dst;
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
