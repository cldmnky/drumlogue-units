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

// Enable USE_NEON for ARM NEON optimizations
#ifndef TEST
#ifdef __ARM_NEON
#define USE_NEON 1
#endif
#endif

// Disable NEON DSP when testing on host
#ifndef TEST
#define NEON_DSP_NS drupiter
#include "../../common/neon_dsp.h"
#else
// Stub implementation for testing
namespace drupiter {
namespace neon {
inline void ClearStereoBuffers(float* left, float* right, uint32_t frames) {
    for (uint32_t i = 0; i < frames; ++i) { 
        left[i] = 0.0f; 
        right[i] = 0.0f; 
    }
}
}
}
#endif

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
    : max_voices_(DRUPITER_MAX_VOICES)
    , active_voices_(0)
    , round_robin_index_(0)
    , timestamp_(0)
    , active_voice_list_()
    , num_active_voices_(0)  // Phase 1: Initialize active voice tracking
    , mode_(SYNTH_MODE_MONOPHONIC)
    , allocation_strategy_(ALLOC_ROUND_ROBIN)
    , unison_detune_cents_(10.0f)  // Default 10 cents detune
    , portamento_time_ms_(0.0f)    // Task 2.2.4: Default no glide
    , sample_rate_(48000.0f) {     // Task 2.2.4: Default sample rate
    // Initialize all voices to inactive state (don't memset - corrupts C++ objects)
    for (uint8_t i = 0; i < max_voices_; i++) {
        voices_[i].active = false;
        voices_[i].midi_note = 0;
        voices_[i].velocity = 0.0f;
        voices_[i].pitch_hz = 0.0f;
        voices_[i].note_on_time = 0;
        voices_[i].glide_target_hz = 0.0f;
        voices_[i].glide_increment = 0.0f;
        voices_[i].is_gliding = false;
    }
    memset(active_voice_list_, 0, sizeof(active_voice_list_));  // Clear active voice list
}

VoiceAllocator::~VoiceAllocator() {
}

void VoiceAllocator::Init(float sample_rate) {
    sample_rate_ = sample_rate;  // Task 2.2.4: Cache for glide calculation
    
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

// ============================================================================
// Phase 1 Optimization: Active Voice Tracking
// ============================================================================

void VoiceAllocator::AddActiveVoice(uint8_t voice_idx) {
    // Check if already in list
    for (uint8_t i = 0; i < num_active_voices_; i++) {
        if (active_voice_list_[i] == voice_idx) {
            return;  // Already tracked
        }
    }
    
    // Add to list if there's room
    if (num_active_voices_ < max_voices_) {
        active_voice_list_[num_active_voices_++] = voice_idx;
    }
}

void VoiceAllocator::RemoveActiveVoice(uint8_t voice_idx) {
    // Rebuild list without this voice
    uint8_t new_count = 0;
    for (uint8_t i = 0; i < num_active_voices_; i++) {
        if (active_voice_list_[i] != voice_idx) {
            active_voice_list_[new_count++] = active_voice_list_[i];
        }
    }
    num_active_voices_ = new_count;
}

void VoiceAllocator::UpdateActiveVoiceList() {
    // Scan all tracked active voices and remove those with finished envelopes
    uint8_t new_count = 0;
    for (uint8_t i = 0; i < num_active_voices_; i++) {
        uint8_t v_idx = active_voice_list_[i];
        Voice& voice = voices_[v_idx];
        
        // Keep voice in list if still active or envelope still playing
        if (voice.active || voice.env_amp.IsActive()) {
            active_voice_list_[new_count++] = v_idx;
        }
    }
    num_active_voices_ = new_count;
}

void VoiceAllocator::NoteOn(uint8_t note, uint8_t velocity) {
    timestamp_++;  // Increment for voice stealing
    
    Voice* voice = nullptr;
    uint8_t voice_idx = 0;
    
    switch (mode_) {
        case SYNTH_MODE_MONOPHONIC:
            // Monophonic: always use voice 0
            voice = &voices_[0];
            voice_idx = 0;
            break;
            
        case SYNTH_MODE_POLYPHONIC:
            // Polyphonic: find available voice or steal
            voice = AllocateVoice();
            // Find which voice was allocated
            for (uint8_t i = 0; i < max_voices_; i++) {
                if (&voices_[i] == voice) {
                    voice_idx = i;
                    break;
                }
            }
            break;
            
        case SYNTH_MODE_UNISON:
            // Unison: set frequency for unison oscillator and trigger voice 0 for envelope
            {
                float frequency = common::MidiHelper::NoteToFreq(note);
                unison_osc_.SetFrequency(frequency);
                
                // Trigger voice 0 for envelope and parameter tracking
                voice = &voices_[0];
                voice_idx = 0;
            }
            break;
    }
    
    if (voice) {
        TriggerVoice(voice, note, velocity);
        // Phase 1: Add to active voice list (optimization)
        AddActiveVoice(voice_idx);
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

void VoiceAllocator::RenderMonophonic(float* left, float* right, uint32_t frames, const float* /*params*/) {
    Voice& voice = voices_[0];
    
    // Early exit if voice inactive (saves CPU)
    if (!voice.active && !voice.env_amp.IsActive()) {
        drupiter::neon::ClearStereoBuffers(left, right, frames);
        return;
    }
    
    // Placeholder: Actual DSP rendering will be implemented in drupiter_synth.cc
    // This just clears the buffers for now
    drupiter::neon::ClearStereoBuffers(left, right, frames);
}

void VoiceAllocator::RenderPolyphonic(float* left, float* right, uint32_t frames, const float* /*params*/) {
    // Zero output buffers using NEON-optimized function
    drupiter::neon::ClearStereoBuffers(left, right, frames);
    
    // Phase 1 optimization: Only iterate active voices, not all max_voices_
    // This reduces per-frame voice checks from 8 to typically 1-3 active voices
    uint8_t active_count = 0;
    for (uint8_t i = 0; i < num_active_voices_; i++) {
        uint8_t v = active_voice_list_[i];
        Voice& voice = voices_[v];
        
        if (!voice.active && !voice.env_amp.IsActive()) {
            continue;  // Skip finished voices
        }
        
        active_count++;
        
        // Placeholder: Actual voice rendering will be done in drupiter_synth.cc
        // For now, just count active voices
    }
    
    // Update active voice list at end of render (remove finished envelopes)
    UpdateActiveVoiceList();
    
    // Scale output by active voice count (prevent clipping)
    if (active_count > 1) {
        float scale = 1.0f / sqrtf(static_cast<float>(active_count));
        for (uint32_t i = 0; i < frames; i++) {
            left[i] *= scale;
            right[i] *= scale;
        }
    }
}

void VoiceAllocator::RenderUnison(float* left, float* right, uint32_t frames, const float* /*params*/) {
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
        drupiter::neon::ClearStereoBuffers(left, right, frames);
        return;
    }
    
    // Use voice 0 for the unison oscillator (currently unused in placeholder)
    
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
    float target_hz = common::MidiHelper::NoteToFreq(note);
    
    // Task 2.2.4: Portamento/glide logic
    // Only glide if the voice was already active (true legato), not on fresh note starts
    // Use the voice->active flag which tracks if the voice was previously playing
    bool voice_still_sounding = voice->active;
    
    if (portamento_time_ms_ > 0.01f && voice_still_sounding && voice->pitch_hz > 0.0f) {
        // Enable glide - voice is still sounding, glide to new pitch (legato)
        voice->glide_target_hz = target_hz;
        voice->is_gliding = true;
        
        // Calculate log-space increment for exponential glide
        float freq_ratio = target_hz / voice->pitch_hz;
        float log_ratio = logf(freq_ratio);
        float portamento_time_sec = portamento_time_ms_ / 1000.0f;
        voice->glide_increment = log_ratio / (portamento_time_sec * sample_rate_);
    } else {
        // No glide - jump immediately to target (new note or portamento off)
        voice->pitch_hz = target_hz;
        voice->glide_target_hz = target_hz;
        voice->is_gliding = false;
        voice->glide_increment = 0.0f;
    }
    
    // Update voice state after glide calculation
    voice->active = true;
    voice->midi_note = note;
    voice->velocity = common::MidiHelper::VelocityToFloat(velocity);
    voice->note_on_time = timestamp_;
    
#ifdef DEBUG
    int voice_idx = voice - voices_;
    fprintf(stderr, "[VoiceAlloc] TriggerVoice: voice_idx=%d note=%d freq=%.2f Hz glide=%d\n",
            voice_idx, note, voice->pitch_hz, voice->is_gliding);
    fflush(stderr);
#endif
    
    // Trigger envelopes
    voice->env_amp.NoteOn();
    voice->env_filter.NoteOn();
    voice->env_pitch.NoteOn();  // Task 2.2.1: Trigger per-voice pitch envelope
}

}  // namespace dsp
