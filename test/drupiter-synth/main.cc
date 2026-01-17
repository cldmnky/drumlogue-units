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
    return 0;
}