/*
 * File: voice_allocator.cc
 * Project: Drupiter-Synth Hoover Synthesis Implementation
 * 
 * Description: Voice management for mono/poly/unison synthesis modes
 * 
 * Author: Hoover Synthesis Team
 * Date: 2025-12-25
 * Version: 2.0.0-dev
 */

#include "voice_allocator.h"
#include "../../common/midi_helper.h"
#include <cstring>
#include <algorithm>

namespace dsp {

// ============================================================================
// Voice Implementation
// ============================================================================

void Voice::Init(float sample_rate) {
    dco1.Init(sample_rate);
    dco2.Init(sample_rate);
    vcf.Init(sample_rate);
    env_amp.Init(sample_rate);
    env_filter.Init(sample_rate);
    env_pitch.Init(sample_rate);  // Task 2.2.1: Per-voice pitch envelope
    
    Reset();
}

void Voice::Reset() {
    active = false;
    midi_note = 0;
    velocity = 0.0f;
    pitch_hz = 0.0f;
    note_on_time = 0;
    
    // Reset envelopes to idle state
    env_amp.Reset();
    env_filter.Reset();
    env_pitch.Reset();  // Task 2.2.1: Reset pitch envelope
}

// ============================================================================
// VoiceAllocator Implementation
// ============================================================================

VoiceAllocator::VoiceAllocator()
    : mode_(SYNTH_MODE_MONOPHONIC)
    , max_voices_(DRUPITER_MAX_VOICES)
    , active_voices_(0)
    , allocation_strategy_(ALLOC_ROUND_ROBIN)
    , round_robin_index_(0)
    , timestamp_(0)
    , unison_detune_cents_(10.0f) {  // Default 10 cents detune
    // Zero-initialize all voices
    memset(voices_, 0, sizeof(voices_));
}

VoiceAllocator::~VoiceAllocator() {
}

void VoiceAllocator::Init(float sample_rate) {
    for (uint8_t i = 0; i < max_voices_; i++) {
        voices_[i].Init(sample_rate);
    }
    
    // Initialize unison oscillator (Hoover v2.0)
    unison_osc_.Init(sample_rate, max_voices_);
    unison_osc_.SetDetune(unison_detune_cents_);
    unison_osc_.SetStereoSpread(0.7f);  // 70% stereo width
}

void VoiceAllocator::SetMode(SynthMode mode) {
    mode_ = mode;
   // Note: Voice count limits are set at compile time (DRUPITER_MAX_VOICES)
}

void VoiceAllocator::NoteOn(uint8_t note, uint8_t velocity) {
    timestamp_++;  // Increment for voice stealing
    
    Voice* voice = nullptr;
    
    switch (mode_) {
        case SYNTH_MODE_MONOPHONIC:
            // Monophonic: always use voice 0
            voice = &voices_[0];
            break;
            
        case SYNTH_MODE_POLYPHONIC:
            // Polyphonic: find available voice or steal
            voice = AllocateVoice();
            break;
            
        case SYNTH_MODE_UNISON:
            // Unison: set frequency for unison oscillator and trigger voice 0 for envelope
            {
                float frequency = common::MidiHelper::NoteToFreq(note);
                unison_osc_.SetFrequency(frequency);
                
                // Trigger voice 0 for envelope and parameter tracking
                voice = &voices_[0];
            }
            break;
    }
    
    if (voice) {
        TriggerVoice(voice, note, velocity);
    }
}

void VoiceAllocator::NoteOff(uint8_t note) {
    switch (mode_) {
        case SYNTH_MODE_MONOPHONIC:
            // Monophonic: release voice 0 if it matches
            if (voices_[0].active && voices_[0].midi_note == note) {
                voices_[0].env_amp.NoteOff();
                voices_[0].env_filter.NoteOff();
                voices_[0].env_pitch.NoteOff();
            }
            break;
            
        case SYNTH_MODE_POLYPHONIC:
            // Polyphonic: find matching note and release
            for (uint8_t i = 0; i < max_voices_; i++) {
                if (voices_[i].active && voices_[i].midi_note == note) {
                    voices_[i].env_amp.NoteOff();
                    voices_[i].env_filter.NoteOff();
                    voices_[i].env_pitch.NoteOff();
                }
            }
            break;
            
        case SYNTH_MODE_UNISON:
            // Unison: release all voices
            for (uint8_t i = 0; i < max_voices_; i++) {
                if (voices_[i].active) {
                    voices_[i].env_amp.NoteOff();
                    voices_[i].env_filter.NoteOff();
                    voices_[i].env_pitch.NoteOff();
                }
            }
            break;
    }
}

void VoiceAllocator::AllNotesOff() {
    for (uint8_t i = 0; i < max_voices_; i++) {
        if (voices_[i].active) {
            voices_[i].env_amp.NoteOff();
            voices_[i].env_filter.NoteOff();
            voices_[i].env_pitch.NoteOff();  // Task 2.2.1
        }
    }
}

// ============================================================================
// Render Methods (called by drupiter_synth.cc)
// ============================================================================

void VoiceAllocator::RenderMonophonic(float* left, float* right, uint32_t frames, const float* params) {
    Voice& voice = voices_[0];
    
    // Early exit if voice inactive (saves CPU)
    if (!voice.active && !voice.env_amp.IsActive()) {
        memset(left, 0, frames * sizeof(float));
        memset(right, 0, frames * sizeof(float));
        return;
    }
    
    // Placeholder: Actual DSP rendering will be implemented in drupiter_synth.cc
    // This just clears the buffers for now
    memset(left, 0, frames * sizeof(float));
    memset(right, 0, frames * sizeof(float));
}

void VoiceAllocator::RenderPolyphonic(float* left, float* right, uint32_t frames, const float* params) {
    // Zero output buffers
    memset(left, 0, frames * sizeof(float));
    memset(right, 0, frames * sizeof(float));
    
    // Mix all active voices
    uint8_t active_count = 0;
    for (uint8_t i = 0; i < max_voices_; i++) {
        Voice& voice = voices_[i];
        
        if (!voice.active && !voice.env_amp.IsActive()) {
            continue;  // Skip inactive voices
        }
        
        active_count++;
        
        // Placeholder: Actual voice rendering will be done in drupiter_synth.cc
        // For now, just count active voices
    }
    
    // Scale output by active voice count (prevent clipping)
    if (active_count > 1) {
        float scale = 1.0f / sqrtf(static_cast<float>(active_count));
        for (uint32_t i = 0; i < frames; i++) {
            left[i] *= scale;
            right[i] *= scale;
        }
    }
}

void VoiceAllocator::RenderUnison(float* left, float* right, uint32_t frames, const float* params) {
    // Unison mode: Use UnisonOscillator for multi-voice detuned stack
    // All voices trigger simultaneously and use the unison oscillator
    
    // Check if any voice is active
    bool any_active = false;
    for (uint8_t i = 0; i < max_voices_; i++) {
        if (voices_[i].active || voices_[i].env_amp.IsActive()) {
            any_active = true;
            break;
        }
    }
    
    if (!any_active) {
        // No active voices - silence
        memset(left, 0, frames * sizeof(float));
        memset(right, 0, frames * sizeof(float));
        return;
    }
    
    // Use voice 0's settings for the unison oscillator
    Voice& lead_voice = voices_[0];
    
    // Process frame by frame using unison oscillator
    for (uint32_t i = 0; i < frames; i++) {
        unison_osc_.Process(&left[i], &right[i]);
    }
}

// ============================================================================
// Private Helper Methods
// ============================================================================

Voice* VoiceAllocator::AllocateVoice() {
    // First, try to find inactive voice
    for (uint8_t i = 0; i < max_voices_; i++) {
        if (!voices_[i].active || !voices_[i].env_amp.IsActive()) {
            return &voices_[i];
        }
    }
    
    // All voices active - need to steal
    return StealVoice();
}

Voice* VoiceAllocator::StealVoice() {
    switch (allocation_strategy_) {
        case ALLOC_OLDEST_NOTE:
            return StealOldestVoice();
            
        case ALLOC_ROUND_ROBIN:
            return StealRoundRobinVoice();
            
        case ALLOC_FIRST_AVAILABLE:
        default:
            return &voices_[0];  // Fallback
    }
}

Voice* VoiceAllocator::StealOldestVoice() {
    uint8_t oldest_idx = 0;
    uint32_t oldest_time = voices_[0].note_on_time;
    
    for (uint8_t i = 1; i < max_voices_; i++) {
        if (voices_[i].note_on_time < oldest_time) {
            oldest_time = voices_[i].note_on_time;
            oldest_idx = i;
        }
    }
    
    return &voices_[oldest_idx];
}

Voice* VoiceAllocator::StealRoundRobinVoice() {
    round_robin_index_ = (round_robin_index_ + 1) % max_voices_;
    return &voices_[round_robin_index_];
}

void VoiceAllocator::TriggerVoice(Voice* voice, uint8_t note, uint8_t velocity) {
    voice->active = true;
    voice->midi_note = note;
    voice->velocity = common::MidiHelper::VelocityToFloat(velocity);
    voice->pitch_hz = common::MidiHelper::NoteToFreq(note);
    voice->note_on_time = timestamp_;
    
#ifdef DEBUG
    int voice_idx = voice - voices_;
    fprintf(stderr, "[VoiceAlloc] TriggerVoice: voice_idx=%d note=%d freq=%.2f Hz\n",
            voice_idx, note, voice->pitch_hz);
    fflush(stderr);
#endif
    
    // Trigger envelopes
    voice->env_amp.NoteOn();
    voice->env_filter.NoteOn();
    voice->env_pitch.NoteOn();  // Task 2.2.1: Trigger per-voice pitch envelope
}

}  // namespace dsp
