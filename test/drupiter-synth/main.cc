// Simple test harness for Jupiter DCO
// Tests Q31 interpolation implementation

#include <iostream>
#include <vector>
#include <cmath>

// Define TEST to enable desktop testing
#ifndef TEST
#define TEST
#endif

// Include DSP utilities
#include "../../drumlogue/common/dsp_utils.h"

// Include the Jupiter DCO
#include "../../drumlogue/drupiter-synth/dsp/jupiter_dco.h"
// Include drupiter voice allocator wrapper
#include "../../drumlogue/drupiter-synth/dsp/voice_allocator.h"
// Include MIDI helpers
#include "../../drumlogue/common/midi_helper.h"
// Include smoothed value
#include "../../drumlogue/common/smoothed_value.h"

static int FindVoiceForNote(const dsp::VoiceAllocator& allocator, uint8_t note) {
    for (uint8_t i = 0; i < DRUPITER_MAX_VOICES; ++i) {
        if (allocator.GetVoice(i).midi_note == note) {
            return i;
        }
    }
    return -1;
}

static bool TestPolyphonicRetrigger() {
    dsp::VoiceAllocator allocator;
    allocator.Init(48000.0f);
    allocator.SetMode(dsp::SYNTH_MODE_POLYPHONIC);

    // Fill all voices
    allocator.NoteOn(60, 100);
    allocator.NoteOn(64, 100);
    allocator.NoteOn(67, 100);
    allocator.NoteOn(71, 100);

    // Release all notes to enter release phase
    allocator.NoteOff(60);
    allocator.NoteOff(64);
    allocator.NoteOff(67);
    allocator.NoteOff(71);

    // Advance envelopes a bit (simulate release decay)
    for (uint8_t i = 0; i < DRUPITER_MAX_VOICES; ++i) {
        dsp::Voice& voice = allocator.GetVoiceMutable(i);
        for (int s = 0; s < 128; ++s) {
            voice.env_amp.Process();
        }
    }

    // New note should retrigger envelope attack even if stealing a releasing voice
    allocator.NoteOn(72, 100);
    const int voice_idx = FindVoiceForNote(allocator, 72);
    if (voice_idx < 0) {
        std::cout << "ERROR: Poly retrigger test - new note not assigned to any voice" << std::endl;
        return false;
    }

    const dsp::Voice& voice = allocator.GetVoice(static_cast<uint8_t>(voice_idx));
    if (voice.env_amp.GetState() != dsp::JupiterEnvelope::STATE_ATTACK) {
        std::cout << "ERROR: Poly retrigger test - envelope not in ATTACK" << std::endl;
        return false;
    }

    return true;
}

static bool TestUnisonRetrigger() {
    dsp::VoiceAllocator allocator;
    allocator.Init(48000.0f);
    allocator.SetMode(dsp::SYNTH_MODE_UNISON);

    allocator.NoteOn(60, 100);
    allocator.NoteOff(60);

    // Advance release a bit
    dsp::Voice& voice0 = allocator.GetVoiceMutable(0);
    for (int s = 0; s < 128; ++s) {
        voice0.env_amp.Process();
    }

    // New note should retrigger attack when no held notes
    allocator.NoteOn(64, 100);
    if (voice0.env_amp.GetState() != dsp::JupiterEnvelope::STATE_ATTACK) {
        std::cout << "ERROR: Unison retrigger test - envelope not in ATTACK" << std::endl;
        return false;
    }

    return true;
}

static bool TestMonoLastNotePriority() {
    dsp::VoiceAllocator allocator;
    allocator.Init(48000.0f);
    allocator.SetMode(dsp::SYNTH_MODE_MONOPHONIC);

    allocator.NoteOn(60, 100);  // C
    allocator.NoteOn(69, 100);  // A
    allocator.NoteOff(69);      // Release A

    const dsp::Voice& voice0 = allocator.GetVoice(0);
    if (voice0.midi_note != 60) {
        std::cout << "ERROR: Mono last-note test - expected C after releasing A" << std::endl;
        return false;
    }

    return true;
}

// ============================================================================
// MIDI Control Tests (Velocity, Pitch Bend, Pressure, Aftertouch)
// ============================================================================

static bool TestVelocitySensitivity() {
    std::cout << "\n=== Testing Velocity Sensitivity ===" << std::endl;
    
    using common::MidiHelper;
    
    // Test velocity conversion
    uint8_t vel_soft = 30;
    uint8_t vel_loud = 120;
    
    float vel_soft_norm = MidiHelper::VelocityToFloat(vel_soft);
    float vel_loud_norm = MidiHelper::VelocityToFloat(vel_loud);
    
    std::cout << "Velocity " << (int)vel_soft << " -> " << vel_soft_norm << std::endl;
    std::cout << "Velocity " << (int)vel_loud << " -> " << vel_loud_norm << std::endl;
    
    // Soft velocity should be lower than loud
    if (vel_soft_norm >= vel_loud_norm) {
        std::cout << "ERROR: Velocity ordering incorrect!" << std::endl;
        return false;
    }
    
    // Velocity 0 should map to 0.0, velocity 127 should map to 1.0
    if (std::abs(MidiHelper::VelocityToFloat(0) - 0.0f) > 0.01f) {
        std::cout << "ERROR: Velocity 0 not mapping to 0.0" << std::endl;
        return false;
    }
    if (std::abs(MidiHelper::VelocityToFloat(127) - 1.0f) > 0.01f) {
        std::cout << "ERROR: Velocity 127 not mapping to 1.0" << std::endl;
        return false;
    }
    
    std::cout << "✓ Velocity sensitivity test PASSED" << std::endl;
    return true;
}

static bool TestPitchBendRange() {
    std::cout << "\n=== Testing Pitch Bend Range ===" << std::endl;
    
    using common::MidiHelper;
    
    // Test pitch bend conversion (±2 semitones)
    uint16_t pb_center = 8192;  // Center (no bend)
    uint16_t pb_down = 0;        // Maximum down
    uint16_t pb_up = 16383;      // Maximum up
    
    float pb_center_st = MidiHelper::PitchBendToSemitones(pb_center, 2.0f);
    float pb_down_st = MidiHelper::PitchBendToSemitones(pb_down, 2.0f);
    float pb_up_st = MidiHelper::PitchBendToSemitones(pb_up, 2.0f);
    
    std::cout << "Pitch bend center (8192) -> " << pb_center_st << " semitones" << std::endl;
    std::cout << "Pitch bend down (0) -> " << pb_down_st << " semitones" << std::endl;
    std::cout << "Pitch bend up (16383) -> " << pb_up_st << " semitones" << std::endl;
    
    // Center should be near 0
    if (std::abs(pb_center_st) > 0.1f) {
        std::cout << "ERROR: Center pitch bend not at 0!" << std::endl;
        return false;
    }
    
    // Down should be negative, up should be positive
    if (pb_down_st >= 0.0f || pb_up_st <= 0.0f) {
        std::cout << "ERROR: Pitch bend polarity incorrect!" << std::endl;
        return false;
    }
    
    // Range should be ±2 semitones
    if (std::abs(pb_down_st - (-2.0f)) > 0.1f) {
        std::cout << "ERROR: Pitch bend down range incorrect!" << std::endl;
        return false;
    }
    if (std::abs(pb_up_st - 2.0f) > 0.1f) {
        std::cout << "ERROR: Pitch bend up range incorrect!" << std::endl;
        return false;
    }
    
    // Test frequency ratio conversion
    float ratio_center = MidiHelper::PitchBendToMultiplier(pb_center, 2.0f);
    float ratio_down = MidiHelper::PitchBendToMultiplier(pb_down, 2.0f);
    float ratio_up = MidiHelper::PitchBendToMultiplier(pb_up, 2.0f);
    
    std::cout << "Frequency ratios: center=" << ratio_center << ", down=" << ratio_down << ", up=" << ratio_up << std::endl;
    
    // Ratios should bracket 1.0
    if (ratio_center < 0.99f || ratio_center > 1.01f) {
        std::cout << "ERROR: Center frequency ratio not 1.0!" << std::endl;
        return false;
    }
    if (ratio_down >= 1.0f || ratio_up <= 1.0f) {
        std::cout << "ERROR: Pitch bend frequency polarity incorrect!" << std::endl;
        return false;
    }
    
    std::cout << "✓ Pitch bend range test PASSED" << std::endl;
    return true;
}

static bool TestPressureConversion() {
    std::cout << "\n=== Testing Channel Pressure Conversion ===" << std::endl;
    
    using common::MidiHelper;
    
    // Test channel pressure conversion
    uint8_t pressure_off = 0;
    uint8_t pressure_half = 64;
    uint8_t pressure_max = 127;
    
    float p_off = MidiHelper::PressureToFloat(pressure_off);
    float p_half = MidiHelper::PressureToFloat(pressure_half);
    float p_max = MidiHelper::PressureToFloat(pressure_max);
    
    std::cout << "Pressure " << (int)pressure_off << " -> " << p_off << std::endl;
    std::cout << "Pressure " << (int)pressure_half << " -> " << p_half << std::endl;
    std::cout << "Pressure " << (int)pressure_max << " -> " << p_max << std::endl;
    
    // Values should be 0.0, ~0.5, 1.0
    if (std::abs(p_off - 0.0f) > 0.01f) {
        std::cout << "ERROR: Pressure 0 not mapping to 0.0" << std::endl;
        return false;
    }
    if (std::abs(p_half - 0.5f) > 0.02f) {
        std::cout << "ERROR: Pressure 64 not mapping to ~0.5" << std::endl;
        return false;
    }
    if (std::abs(p_max - 1.0f) > 0.01f) {
        std::cout << "ERROR: Pressure 127 not mapping to 1.0" << std::endl;
        return false;
    }
    
    std::cout << "✓ Pressure conversion test PASSED" << std::endl;
    return true;
}

static bool TestAftertouchConversion() {
    std::cout << "\n=== Testing Aftertouch Conversion ===" << std::endl;
    
    using common::MidiHelper;
    
    // Test aftertouch conversion (same as pressure)
    uint8_t aftertouch_off = 0;
    uint8_t aftertouch_max = 127;
    
    float at_off = MidiHelper::AftertouchToFloat(aftertouch_off);
    float at_max = MidiHelper::AftertouchToFloat(aftertouch_max);
    
    std::cout << "Aftertouch " << (int)aftertouch_off << " -> " << at_off << std::endl;
    std::cout << "Aftertouch " << (int)aftertouch_max << " -> " << at_max << std::endl;
    
    if (std::abs(at_off - 0.0f) > 0.01f || std::abs(at_max - 1.0f) > 0.01f) {
        std::cout << "ERROR: Aftertouch conversion failed!" << std::endl;
        return false;
    }
    
    std::cout << "✓ Aftertouch conversion test PASSED" << std::endl;
    return true;
}

static bool TestSmoothingFilter() {
    std::cout << "\n=== Testing Pitch Bend Smoothing Filter ===" << std::endl;
    
    // Create a smoothed value for pitch bend
    dsp::SmoothedValue pitch_smooth;
    pitch_smooth.Init(0.0f, 0.005f);  // 0.005f coefficient for smooth vibrato
    
    // Set a target pitch bend
    pitch_smooth.SetTarget(2.0f);  // ±2 semitones
    
    // Process several times and verify smooth transition
    std::cout << "Target: 2.0 semitones (±2)" << std::endl;
    std::cout << "Processing with coefficient 0.005f:" << std::endl;
    
    float prev_val = 0.0f;
    for (int i = 0; i < 5; ++i) {
        float current = pitch_smooth.Process();
        float delta = current - prev_val;
        
        std::cout << "  Step " << i << ": value=" << current << ", delta=" << delta << std::endl;
        
        // Ensure we're moving towards target
        if (i > 0 && delta < 0.0f) {
            std::cout << "ERROR: Smoothing went backwards!" << std::endl;
            return false;
        }
        
        prev_val = current;
    }
    
    // After a few iterations, should be closer to target
    float final_val = pitch_smooth.Process();
    if (final_val < 0.01f) {
        std::cout << "ERROR: Smoothing filter not making progress!" << std::endl;
        return false;
    }
    
    std::cout << "✓ Smoothing filter test PASSED" << std::endl;
    return true;
}

int main(int argc, char** argv) {
    (void)argc;
    (void)argv;
    std::cout << "Testing Jupiter DCO with Q31 interpolation..." << std::endl;

    // Create oscillator
    dsp::JupiterDCO osc;
    osc.Init(48000.0f);
    osc.SetFrequency(440.0f);
    osc.SetWaveform(dsp::JupiterDCO::WAVEFORM_SAW);

    // Generate some samples
    const int num_samples = 1000;
    std::vector<float> output(num_samples);

    for (int i = 0; i < num_samples; ++i) {
        output[i] = osc.Process();
    }

    // Basic checks
    bool has_signal = false;
    float max_val = -INFINITY;
    float min_val = INFINITY;
    float sum = 0.0f;
    int over_count = 0;

    for (float sample : output) {
        if (std::abs(sample) > 0.001f) has_signal = true;
        max_val = std::max(max_val, sample);
        min_val = std::min(min_val, sample);
        sum += sample;
        if (sample > 1.1f || sample < -1.1f) over_count++;
    }

    float avg = sum / num_samples;

    std::cout << "Generated " << num_samples << " samples" << std::endl;
    std::cout << "Has signal: " << (has_signal ? "YES" : "NO") << std::endl;
    std::cout << "Range: " << min_val << " to " << max_val << std::endl;
    std::cout << "Average: " << avg << std::endl;
    std::cout << "Samples over ±1.1: " << over_count << std::endl;

    // Check that signal is within reasonable bounds for bandlimited signal
    // Allow some overshoot due to PolyBLEP transitions
    if (max_val > 1.2f || min_val < -1.2f) {
        std::cout << "ERROR: Signal excessively out of bounds!" << std::endl;
        return 1;
    }

    // Check that we have oscillation (not just DC)
    if (!has_signal) {
        std::cout << "ERROR: No oscillation detected!" << std::endl;
        return 1;
    }

    // Check that DC offset is reasonable
    if (std::abs(avg) > 0.1f) {
        std::cout << "ERROR: Excessive DC offset!" << std::endl;
        return 1;
    }

    std::cout << "Basic functionality test PASSED!" << std::endl;

    std::cout << "Testing VoiceAllocator retrigger behavior..." << std::endl;
    bool ok = true;
    if (!TestPolyphonicRetrigger()) {
        ok = false;
    }
    if (!TestUnisonRetrigger()) {
        ok = false;
    }
    if (!TestMonoLastNotePriority()) {
        ok = false;
    }

    if (!ok) {
        return 1;
    }

    std::cout << "VoiceAllocator retrigger tests PASSED!" << std::endl;
    
    // Run MIDI control tests
    std::cout << "\n" << "========================================" << std::endl;
    std::cout << "MIDI Control Tests" << std::endl;
    std::cout << "========================================" << std::endl;
    
    ok = true;
    if (!TestVelocitySensitivity()) {
        ok = false;
    }
    if (!TestPitchBendRange()) {
        ok = false;
    }
    if (!TestPressureConversion()) {
        ok = false;
    }
    if (!TestAftertouchConversion()) {
        ok = false;
    }
    if (!TestSmoothingFilter()) {
        ok = false;
    }
    
    if (!ok) {
        return 1;
    }
    
    std::cout << "\n" << "========================================" << std::endl;
    std::cout << "All MIDI tests PASSED!" << std::endl;
    std::cout << "========================================" << std::endl;
    return 0;
}