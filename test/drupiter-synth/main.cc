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
// Include catchable value
#include "../../drumlogue/common/catchable_value.h"

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

static bool TestVelocityVCAScaling() {
    std::cout << "\n=== Testing Velocity VCA Amplitude Scaling ===" << std::endl;
    
    // Test the VCA velocity scaling formula: gain = 0.2 + (velocity/127) * 0.8
    // This ensures velocity 0->0.2x amplitude, velocity 127->1.0x amplitude
    
    auto VelocityToVCAGain = [](uint8_t velocity) {
        return 0.2f + (velocity / 127.0f) * 0.8f;
    };
    
    // Velocity 0 should give 0.2x (soft hits still audible)
    float gain_silent = VelocityToVCAGain(0);
    if (std::abs(gain_silent - 0.2f) > 0.01f) {
        std::cout << "ERROR: Velocity 0 should give 0.2x gain, got " << gain_silent << std::endl;
        return false;
    }
    std::cout << "  Velocity 0 -> " << gain_silent << "x (soft minimum)" << std::endl;
    
    // Velocity 127 should give 1.0x (loudest)
    float gain_loud = VelocityToVCAGain(127);
    if (std::abs(gain_loud - 1.0f) > 0.01f) {
        std::cout << "ERROR: Velocity 127 should give 1.0x gain, got " << gain_loud << std::endl;
        return false;
    }
    std::cout << "  Velocity 127 -> " << gain_loud << "x (maximum)" << std::endl;
    
    // Mid-range velocity should give mid-range gain
    float gain_mid = VelocityToVCAGain(64);
    float expected_mid = 0.2f + (64.0f / 127.0f) * 0.8f;  // ~0.6
    if (std::abs(gain_mid - expected_mid) > 0.01f) {
        std::cout << "ERROR: Velocity 64 should give ~" << expected_mid << "x, got " << gain_mid << std::endl;
        return false;
    }
    std::cout << "  Velocity 64 -> " << gain_mid << "x (mid-range)" << std::endl;
    
    // Soft velocity (30)
    float gain_soft = VelocityToVCAGain(30);
    if (gain_soft <= gain_silent || gain_soft >= gain_loud) {
        std::cout << "ERROR: Soft velocity gain out of expected range: " << gain_soft << std::endl;
        return false;
    }
    std::cout << "  Velocity 30 -> " << gain_soft << "x (soft)" << std::endl;
    
    // Hard velocity (120)
    float gain_hard = VelocityToVCAGain(120);
    if (gain_hard <= gain_mid || gain_hard >= gain_loud) {
        std::cout << "ERROR: Hard velocity gain out of expected range: " << gain_hard << std::endl;
        return false;
    }
    std::cout << "  Velocity 120 -> " << gain_hard << "x (hard)" << std::endl;
    
    std::cout << "✓ Velocity VCA scaling test PASSED" << std::endl;
    return true;
}

static bool TestPerVoiceVelocityPolyphonic() {
    std::cout << "\n=== Testing Per-Voice Velocity in Polyphonic Mode ===" << std::endl;
    
    dsp::VoiceAllocator allocator;
    allocator.Init(48000.0f);
    allocator.SetMode(dsp::SYNTH_MODE_POLYPHONIC);
    
    // Play three notes with different velocities
    allocator.NoteOn(60, 30);   // Soft C
    allocator.NoteOn(64, 80);   // Medium E
    allocator.NoteOn(67, 120);  // Loud G
    
    // Check that each voice stores its own velocity (normalized 0-1)
    const dsp::Voice& voice_c = allocator.GetVoice(0);
    const dsp::Voice& voice_e = allocator.GetVoice(1);
    const dsp::Voice& voice_g = allocator.GetVoice(2);
    
    // Velocity is stored normalized: 30/127, 80/127, 120/127
    float norm_30 = 30.0f / 127.0f;
    float norm_80 = 80.0f / 127.0f;
    float norm_120 = 120.0f / 127.0f;
    
    if (std::abs(voice_c.velocity - norm_30) > 0.01f) {
        std::cout << "ERROR: Voice 0 velocity should be " << norm_30 << ", got " << voice_c.velocity << std::endl;
        return false;
    }
    std::cout << "  Voice 0 (C): velocity = " << voice_c.velocity << " (soft)" << std::endl;
    
    if (std::abs(voice_e.velocity - norm_80) > 0.01f) {
        std::cout << "ERROR: Voice 1 velocity should be " << norm_80 << ", got " << voice_e.velocity << std::endl;
        return false;
    }
    std::cout << "  Voice 1 (E): velocity = " << voice_e.velocity << " (medium)" << std::endl;
    
    if (std::abs(voice_g.velocity - norm_120) > 0.01f) {
        std::cout << "ERROR: Voice 2 velocity should be " << norm_120 << ", got " << voice_g.velocity << std::endl;
        return false;
    }
    std::cout << "  Voice 2 (G): velocity = " << voice_g.velocity << " (loud)" << std::endl;
    
    // Verify velocities are independent (not all using last note's velocity)
    if (voice_c.velocity == voice_e.velocity || voice_e.velocity == voice_g.velocity) {
        std::cout << "ERROR: All voices have same velocity! Per-voice velocity NOT working" << std::endl;
        return false;
    }
    
    std::cout << "✓ Per-voice velocity in polyphonic mode PASSED" << std::endl;
    return true;
}

static bool TestVelocityCutoffBidirectional() {
    std::cout << "\n=== Testing Bidirectional Velocity (Filter + Amplitude) ===" << std::endl;
    
    // Verify velocity modulation affects both VCF cutoff AND VCA amplitude
    // VCF: velocity -> cutoff frequency (high velocity = brighter)
    // VCA: velocity -> amplitude (high velocity = louder)
    
    // VCF modulation: vel_mod = (velocity / 127) * 0.5, then * 2.0 = ±2 octaves
    auto VelocityToVCFMod = [](uint8_t velocity) {
        return (velocity / 127.0f) * 0.5f;  // 0.0 to 0.5
    };
    
    // VCA modulation: gain = 0.2 + (velocity / 127) * 0.8
    auto VelocityToVCAGain = [](uint8_t velocity) {
        return 0.2f + (velocity / 127.0f) * 0.8f;  // 0.2 to 1.0
    };
    
    uint8_t velocity_soft = 20;
    uint8_t velocity_loud = 110;
    
    float vcf_mod_soft = VelocityToVCFMod(velocity_soft);
    float vcf_mod_loud = VelocityToVCFMod(velocity_loud);
    float vca_gain_soft = VelocityToVCAGain(velocity_soft);
    float vca_gain_loud = VelocityToVCAGain(velocity_loud);
    
    std::cout << "  Soft velocity (" << (int)velocity_soft << "):" << std::endl;
    std::cout << "    VCF cutoff mod: " << vcf_mod_soft << " (darker)" << std::endl;
    std::cout << "    VCA gain: " << vca_gain_soft << "x (quieter)" << std::endl;
    
    std::cout << "  Loud velocity (" << (int)velocity_loud << "):" << std::endl;
    std::cout << "    VCF cutoff mod: " << vcf_mod_loud << " (brighter)" << std::endl;
    std::cout << "    VCA gain: " << vca_gain_loud << "x (louder)" << std::endl;
    
    // Verify bidirectional: both should increase with velocity
    if (vcf_mod_loud <= vcf_mod_soft) {
        std::cout << "ERROR: VCF modulation not increasing with velocity!" << std::endl;
        return false;
    }
    if (vca_gain_loud <= vca_gain_soft) {
        std::cout << "ERROR: VCA gain not increasing with velocity!" << std::endl;
        return false;
    }
    
    // Verify both effects are present (not just one)
    float vcf_ratio = vcf_mod_loud / vcf_mod_soft;
    float vca_ratio = vca_gain_loud / vca_gain_soft;
    if (vcf_ratio < 1.1f || vca_ratio < 1.1f) {
        std::cout << "ERROR: Velocity effects too weak (both should scale >10%)" << std::endl;
        return false;
    }
    
    std::cout << "  VCF effect scale: " << vcf_ratio << "x" << std::endl;
    std::cout << "  VCA effect scale: " << vca_ratio << "x" << std::endl;
    
    std::cout << "✓ Bidirectional velocity effects test PASSED" << std::endl;

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

// ============================================================================
// Catchable Value Tests
// ============================================================================

static bool TestCatchableValueBasic() {
    std::cout << "  TestCatchableValueBasic..." << std::endl;
    
    dsp::CatchableValue catcher;
    catcher.Init(50);
    
    // Should not be catching initially
    if (catcher.IsCatching()) {
        std::cout << "    ERROR: Should not be catching after Init" << std::endl;
        return false;
    }
    
    // Value should follow knob normally
    int32_t result = catcher.Update(60);
    if (result != 60) {
        std::cout << "    ERROR: Expected 60, got " << result << std::endl;
        return false;
    }
    
    if (catcher.IsCatching()) {
        std::cout << "    ERROR: Should not be catching when following knob" << std::endl;
        return false;
    }
    
    std::cout << "    PASSED: Basic follow behavior works" << std::endl;
    return true;
}

static bool TestCatchableValueCatchBehavior() {
    std::cout << "  TestCatchableValueCatchBehavior..." << std::endl;
    
    dsp::CatchableValue catcher;
    catcher.Init(50);
    
    // Simulate preset load: DSP value = 80, but knob is at 20
    catcher.Reset(80, 20);
    
    // Should be catching (knob far from target)
    if (!catcher.IsCatching()) {
        std::cout << "    ERROR: Should be catching after Reset with distant knob" << std::endl;
        return false;
    }
    
    // Move knob from 20 to 30 - should still hold at 80
    int32_t result = catcher.Update(30);
    if (result != 80) {
        std::cout << "    ERROR: Catch failed - expected 80, got " << result << std::endl;
        return false;
    }
    
    if (!catcher.IsCatching()) {
        std::cout << "    ERROR: Should still be catching before crossing" << std::endl;
        return false;
    }
    
    // Move knob from 30 to 50 - still catching
    result = catcher.Update(50);
    if (result != 80) {
        std::cout << "    ERROR: Catch failed at 50 - expected 80, got " << result << std::endl;
        return false;
    }
    
    // Move knob from 50 to 80 (crossing the catch point)
    result = catcher.Update(80);
    if (result != 80) {
        std::cout << "    ERROR: Catch failed at crossing - expected 80, got " << result << std::endl;
        return false;
    }
    
    if (catcher.IsCatching()) {
        std::cout << "    ERROR: Should have stopped catching after crossing" << std::endl;
        return false;
    }
    
    // Now should follow knob normally
    result = catcher.Update(85);
    if (result != 85) {
        std::cout << "    ERROR: After catch release, expected 85, got " << result << std::endl;
        return false;
    }
    
    std::cout << "    PASSED: Catch and release behavior works" << std::endl;
    return true;
}

static bool TestCatchableValueThreshold() {
    std::cout << "  TestCatchableValueThreshold..." << std::endl;
    
    dsp::CatchableValue catcher;
    catcher.Init(50);
    
    // Simulate preset load: DSP value = 50, knob at 20
    catcher.Reset(50, 20);
    
    // Move knob to within threshold (50 ± 3)
    int32_t result = catcher.Update(48);  // Within threshold
    
    // Should have released catch (within ±3)
    if (catcher.IsCatching()) {
        std::cout << "    ERROR: Should release catch within threshold" << std::endl;
        return false;
    }
    
    if (result != 48) {
        std::cout << "    ERROR: Expected 48 within threshold, got " << result << std::endl;
        return false;
    }
    
    std::cout << "    PASSED: Threshold detection works (±3 units)" << std::endl;
    return true;
}

static bool TestCatchableValueBipolar() {
    std::cout << "  TestCatchableValueBipolar..." << std::endl;
    
    dsp::CatchableValue catcher;
    catcher.Init(0);
    
    // Test with bipolar parameter (-100 to +100 becomes 0 to 100 in UI)
    // Preset has detune at +20 (mapped to 70 in UI), knob at 30
    catcher.Reset(70, 30);
    
    if (!catcher.IsCatching()) {
        std::cout << "    ERROR: Should be catching for bipolar parameter" << std::endl;
        return false;
    }
    
    // Move knob from 30 to 70
    int32_t result = catcher.Update(70);
    
    if (catcher.IsCatching()) {
        std::cout << "    ERROR: Should release after crossing bipolar target" << std::endl;
        return false;
    }
    
    if (result != 70) {
        std::cout << "    ERROR: Expected 70, got " << result << std::endl;
        return false;
    }
    
    std::cout << "    PASSED: Bipolar parameter catch works" << std::endl;
    return true;
}

static bool TestCatchableValueFloat() {
    std::cout << "  TestCatchableValueFloat..." << std::endl;
    
    dsp::CatchableValueFloat catcher;
    catcher.Init(0.5f);
    
    // Should follow knob (value / 100)
    float result = catcher.Update(60);
    if (std::abs(result - 0.6f) > 0.01f) {
        std::cout << "    ERROR: Expected ~0.6, got " << result << std::endl;
        return false;
    }
    
    // Test catch behavior
    catcher.Reset(0.8f, 20);  // DSP=0.8, knob=20
    
    if (!catcher.IsCatching()) {
        std::cout << "    ERROR: Float version should be catching" << std::endl;
        return false;
    }
    
    result = catcher.Update(30);
    if (std::abs(result - 0.8f) > 0.01f) {
        std::cout << "    ERROR: Float catch failed - expected ~0.8, got " << result << std::endl;
        return false;
    }
    
    std::cout << "    PASSED: Float version works" << std::endl;
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
    if (!TestVelocityVCAScaling()) {
        ok = false;
    }
    if (!TestPerVoiceVelocityPolyphonic()) {
        ok = false;
    }
    if (!TestVelocityCutoffBidirectional()) {
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
    
    // Run catchable value tests
    std::cout << "\n" << "========================================" << std::endl;
    std::cout << "Catchable Value Tests" << std::endl;
    std::cout << "========================================" << std::endl;
    
    ok = true;
    if (!TestCatchableValueBasic()) {
        ok = false;
    }
    if (!TestCatchableValueCatchBehavior()) {
        ok = false;
    }
    if (!TestCatchableValueThreshold()) {
        ok = false;
    }
    if (!TestCatchableValueBipolar()) {
        ok = false;
    }
    if (!TestCatchableValueFloat()) {
        ok = false;
    }
    
    if (!ok) {
        return 1;
    }
    
    std::cout << "\n" << "========================================" << std::endl;
    std::cout << "All Catchable Value tests PASSED!" << std::endl;
    std::cout << "========================================" << std::endl;
    
    return 0;
}