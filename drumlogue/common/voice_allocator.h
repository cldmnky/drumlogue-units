/*
 * File: voice_allocator.h
 * 
 * Generic voice allocator core for synth units.
 * Does not own DSP state or envelopes; it only manages note/voice assignment.
 */

#pragma once

#include <cstdint>
#include <cstring>

namespace common {

// Synthesis modes
enum class VoiceMode {
    Monophonic = 0,
    Polyphonic = 1,
    Unison = 2
};

// Voice allocation strategies for polyphonic mode
enum class VoiceAllocationStrategy {
    RoundRobin = 0,
    OldestNote = 1,
    FirstAvailable = 2
};

struct VoiceSlot {
    bool active = false;
    uint8_t midi_note = 0;
    float velocity = 0.0f;
    uint32_t note_on_time = 0;
};

struct NoteOnResult {
    int8_t voice_index = -1;
    bool allow_legato = false;
};

struct NoteOffResult {
    bool retrigger = false;
    uint8_t retrigger_note = 0;
    bool has_held_notes = false;
};

class VoiceAllocatorCore {
 public:
    VoiceAllocatorCore();

    void Init(uint8_t max_voices);
    void SetMode(VoiceMode mode);
    void SetAllocationStrategy(VoiceAllocationStrategy strategy);

    NoteOnResult NoteOn(uint8_t note, uint8_t velocity);
    NoteOffResult NoteOff(uint8_t note);
    void AllNotesOff();

    void SetVoiceActive(uint8_t idx, bool active);

    bool HasHeldNotes() const { return num_held_notes_ > 0; }
    uint8_t GetMaxVoices() const { return max_voices_; }

    const VoiceSlot& GetVoice(uint8_t idx) const { return voices_[idx]; }
    VoiceSlot& GetVoiceMutable(uint8_t idx) { return voices_[idx]; }

 private:
    VoiceSlot voices_[16];  // Max voices across units (clamped by max_voices_)
    uint8_t max_voices_ = 0;
    uint8_t round_robin_index_ = 0;
    uint32_t timestamp_ = 0;

    VoiceMode mode_ = VoiceMode::Monophonic;
    VoiceAllocationStrategy allocation_strategy_ = VoiceAllocationStrategy::RoundRobin;

    uint8_t current_note_ = 0;

    // Held notes tracking (for mono/unison last-note priority)
    static constexpr uint8_t kMaxHeldNotes = 16;
    uint8_t held_notes_[kMaxHeldNotes];
    uint8_t num_held_notes_ = 0;

    int8_t AllocateVoiceIndex();
    int8_t StealVoiceIndex();
    int8_t StealOldestVoiceIndex();
    int8_t StealRoundRobinVoiceIndex();

    void AddHeldNote(uint8_t note);
    void RemoveHeldNote(uint8_t note);
    uint8_t GetLastHeldNote() const;
};

}  // namespace common
