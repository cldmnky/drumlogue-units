/*
 * File: voice_allocator.h
 * Project: Drupiter-Synth Hoover Synthesis Implementation
 * 
 * Description: Voice management abstraction for three synthesis modes:
 *   - MONOPHONIC: Single voice (backward compatible with v1.x)
 *   - POLYPHONIC: Multiple independent voices (2-6 configurable)
 *   - UNISON: Single note with detuned copies (3-7 configurable)
 * 
 * Author: Hoover Synthesis Team
 * Date: 2025-12-25
 * Version: 2.0.0-dev
 * 
 * License: MIT (matching parent project)
 */

#pragma once

#include <cstdint>
#include <cstring>
#include "jupiter_dco.h"
#include "jupiter_vcf.h"
#include "jupiter_env.h"
#include "unison_oscillator.h"
#include "../common/midi_helper.h"

#ifndef UNISON_VOICES
#define UNISON_VOICES 4
#endif

#ifndef DRUPITER_MAX_VOICES
#define DRUPITER_MAX_VOICES 4  // Phase 1 optimization: Reduced from 7 to 4 for CPU budget (-27%)
#endif

namespace dsp {

// Synthesis modes
enum SynthMode {
    SYNTH_MODE_MONOPHONIC = 0,  // Single voice, last-note priority
    SYNTH_MODE_POLYPHONIC = 1,  // Multiple independent voices
    SYNTH_MODE_UNISON = 2       // Single note + detuned copies
};

// Voice allocation strategies for polyphonic mode
enum VoiceAllocationStrategy {
    ALLOC_ROUND_ROBIN = 0,      // Cycle through voices sequentially
    ALLOC_OLDEST_NOTE = 1,      // Steal voice with longest held note
    ALLOC_FIRST_AVAILABLE = 2   // Use first inactive voice
};

// Per-voice state structure
struct Voice {
    // Voice activity
    bool active;                 // Voice currently in use
    uint8_t midi_note;           // MIDI note number (0-127)
    float velocity;              // Normalized velocity (0.0-1.0)
    float pitch_hz;              // Calculated pitch in Hz
    uint32_t note_on_time;       // Timestamp for voice stealing
    
    // DSP state (per-voice oscillators and filters)
    JupiterDCO dco1;
    JupiterDCO dco2;
    JupiterVCF vcf;
    JupiterEnvelope env_amp;     // Amplitude envelope
    JupiterEnvelope env_filter;  // Filter modulation envelope
    
    // Per-voice pitch envelope (Phase 2 - Task 2.2.1)
    JupiterEnvelope env_pitch;  // Enabled for per-voice pitch modulation
    
    // Portamento/glide state (Phase 2 - Task 2.2.4)
    float glide_target_hz;        // Target pitch for glide
    float glide_increment;        // Log-space increment per sample
    bool is_gliding;              // Currently gliding to target
    
    // Constructor
    Voice() : active(false), midi_note(0), velocity(0.0f), 
              pitch_hz(0.0f), note_on_time(0),
              glide_target_hz(0.0f), glide_increment(0.0f), is_gliding(false) {}
    
    // Initialize DSP components
    void Init(float sample_rate);
    
    // Reset voice state
    void Reset();
};

/**
 * VoiceAllocator: Unified voice management for all synthesis modes
 * 
 * Architecture:
 * - Monophonic: Uses voices_[0] as single voice
 * - Polyphonic: Uses voices_[0..N-1] with allocation strategy
 * - Unison: Uses voices_[0] as primary, UnisonOscillator manages copies
 * 
 * Performance optimizations:
 * - Fixed-size array (no dynamic allocation)
 * - Compile-time mode selection (no runtime branching)
 * - Efficient voice stealing (timestamp-based)
 * - NEON SIMD ready (Phase 2)
 */
class VoiceAllocator {
public:
    VoiceAllocator();
    ~VoiceAllocator();
    
    // Initialization
    void Init(float sample_rate);
    
    // Mode control
    void SetMode(SynthMode mode);
    
    // Unison control (Hoover v2.0)
    void SetUnisonDetune(float detune_cents) {
        unison_detune_cents_ = detune_cents;
        unison_osc_.SetDetune(detune_cents);
    }
    
    // MIDI interface
    void NoteOn(uint8_t note, uint8_t velocity);
    void NoteOff(uint8_t note);
    void AllNotesOff();
    
    // Rendering
    void RenderMonophonic(float* left, float* right, uint32_t frames,
                         const float* params);
    void RenderPolyphonic(float* left, float* right, uint32_t frames,
                         const float* params);
    void RenderUnison(float* left, float* right, uint32_t frames,
                     const float* params);
    
    // Main render dispatch (runtime mode selection)
    inline void Render(float* left, float* right, uint32_t frames,
                      const float* params) {
        switch (mode_) {
            case SYNTH_MODE_MONOPHONIC:
                RenderMonophonic(left, right, frames, params);
                break;
            case SYNTH_MODE_POLYPHONIC:
                RenderPolyphonic(left, right, frames, params);
                break;
            case SYNTH_MODE_UNISON:
                RenderUnison(left, right, frames, params);
                break;
        }
    }
    
    // Getters
    bool IsAnyVoiceActive() const { return active_voices_ > 0; }
    const Voice& GetVoice(uint8_t idx) const { return voices_[idx]; }
    Voice& GetVoiceMutable(uint8_t idx) { return voices_[idx]; }
    SynthMode GetMode() const { return mode_; }
    UnisonOscillator& GetUnisonOscillator() { return unison_osc_; }
    
    // Check if any notes are currently held (keys down)
    bool HasHeldNotes() const { return num_held_notes_ > 0; }
    
    // Portamento control (Phase 2 - Task 2.2.4)
    void SetPortamentoTime(float time_ms) { portamento_time_ms_ = time_ms; }
    float GetPortamentoTime() const { return portamento_time_ms_; }
    
    void SetAllocationStrategy(VoiceAllocationStrategy strategy) {
        allocation_strategy_ = strategy;
    }
    
private:
    // Voice pool
    Voice voices_[DRUPITER_MAX_VOICES];
    uint8_t max_voices_;           // Mode-dependent voice limit
    uint8_t active_voices_;        // Currently active voices
    uint8_t round_robin_index_;    // Round-robin allocation index
    uint32_t timestamp_;           // Time tracking for voice stealing
    
    // Phase 1 optimization: Active voice tracking (-20% CPU)
    // Instead of checking all max_voices_ entries per frame, maintain a list of active voice indices
    uint8_t active_voice_list_[DRUPITER_MAX_VOICES];  // Indices of active voices
    uint8_t num_active_voices_;                       // Count of active voices (0-4)
    
    // Mode configuration
    SynthMode mode_;
    VoiceAllocationStrategy allocation_strategy_;
    
    // Unison mode oscillator (Hoover v2.0)
    UnisonOscillator unison_osc_;
    float unison_detune_cents_;    // Detune amount for unison mode (5-20 cents)
    
    // Portamento/glide state (Phase 2 - Task 2.2.4)
    float portamento_time_ms_;     // Portamento time in milliseconds (10-500ms)
    float sample_rate_;            // Cached sample rate for glide calculation
    
    // Held notes tracking (for proper last-note priority in mono/unison modes)
    static constexpr uint8_t kMaxHeldNotes = 16;  // Track up to 16 simultaneously held keys
    uint8_t held_notes_[kMaxHeldNotes];           // Buffer of held MIDI note numbers
    uint8_t num_held_notes_;                       // Count of currently held notes
    
    // Helper functions
    Voice* AllocateVoice();        // Find or steal a voice
    Voice* StealVoice();           // Voice stealing dispatcher
    Voice* StealOldestVoice();     // Steal oldest note
    Voice* StealRoundRobinVoice(); // Steal via round-robin
    void TriggerVoice(Voice* voice, uint8_t note, uint8_t velocity);  // Initialize voice state
    
    // Active voice tracking helpers (Phase 1 optimization)
    void AddActiveVoice(uint8_t voice_idx);      // Add voice to active list
    void RemoveActiveVoice(uint8_t voice_idx);   // Remove voice from active list
    void UpdateActiveVoiceList();                // Scan for finished envelopes
    
    // Held notes tracking helpers
    void AddHeldNote(uint8_t note);              // Add note to held notes buffer
    void RemoveHeldNote(uint8_t note);           // Remove note from held notes buffer
    uint8_t GetLastHeldNote() const;             // Get last note added to held-notes buffer
                                              // (most recent NoteOn among currently held notes, 0 if none)
    
    // Prevent copying
    VoiceAllocator(const VoiceAllocator&) = delete;
    VoiceAllocator& operator=(const VoiceAllocator&) = delete;
};

}  // namespace dsp

// Use common MIDI utilities
using common::MidiHelper;
