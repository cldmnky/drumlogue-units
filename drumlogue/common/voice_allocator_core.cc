/*
 * File: voice_allocator_core.cc
 * 
 * Generic voice allocator core for synth units.
 */

#include "voice_allocator.h"

namespace common {

VoiceAllocatorCore::VoiceAllocatorCore() {
  memset(held_notes_, 0, sizeof(held_notes_));
}

void VoiceAllocatorCore::Init(uint8_t max_voices) {
  max_voices_ = (max_voices > 16) ? 16 : max_voices;
  round_robin_index_ = 0;
  timestamp_ = 0;
  current_note_ = 0;
  num_held_notes_ = 0;
  for (uint8_t i = 0; i < max_voices_; ++i) {
    voices_[i] = VoiceSlot{};
  }
  memset(held_notes_, 0, sizeof(held_notes_));
}

void VoiceAllocatorCore::SetMode(VoiceMode mode) {
  mode_ = mode;
}

void VoiceAllocatorCore::SetAllocationStrategy(VoiceAllocationStrategy strategy) {
  allocation_strategy_ = strategy;
}

NoteOnResult VoiceAllocatorCore::NoteOn(uint8_t note, uint8_t velocity) {
  NoteOnResult result{};
  timestamp_++;

  const bool had_held_notes = (num_held_notes_ > 0);

  if (mode_ == VoiceMode::Monophonic || mode_ == VoiceMode::Unison) {
    AddHeldNote(note);
    current_note_ = note;
  }

  int8_t voice_index = 0;
  if (mode_ == VoiceMode::Polyphonic) {
    voice_index = AllocateVoiceIndex();
  }

  if (voice_index < 0 || voice_index >= max_voices_) {
    return result;
  }

  VoiceSlot& slot = voices_[voice_index];
  slot.active = true;
  slot.midi_note = note;
  slot.velocity = velocity / 127.0f;
  slot.note_on_time = timestamp_;

  result.voice_index = voice_index;
  result.allow_legato = (mode_ != VoiceMode::Polyphonic) && had_held_notes;
  return result;
}

NoteOffResult VoiceAllocatorCore::NoteOff(uint8_t note) {
  NoteOffResult result{};

  RemoveHeldNote(note);
  result.has_held_notes = (num_held_notes_ > 0);

  if (mode_ == VoiceMode::Monophonic || mode_ == VoiceMode::Unison) {
    bool current_held = false;
    for (uint8_t i = 0; i < num_held_notes_; ++i) {
      if (held_notes_[i] == current_note_) {
        current_held = true;
        break;
      }
    }

    if (!current_held) {
      if (result.has_held_notes) {
        result.retrigger = true;
        result.retrigger_note = GetLastHeldNote();
        current_note_ = result.retrigger_note;
      } else {
        current_note_ = 0;
      }
    }
  }

  return result;
}

void VoiceAllocatorCore::AllNotesOff() {
  num_held_notes_ = 0;
  current_note_ = 0;
  memset(held_notes_, 0, sizeof(held_notes_));
}

void VoiceAllocatorCore::SetVoiceActive(uint8_t idx, bool active) {
  if (idx < max_voices_) {
    voices_[idx].active = active;
  }
}

int8_t VoiceAllocatorCore::AllocateVoiceIndex() {
  // First, try to find inactive voice
  for (uint8_t i = 0; i < max_voices_; ++i) {
    if (!voices_[i].active) {
      return static_cast<int8_t>(i);
    }
  }

  return StealVoiceIndex();
}

int8_t VoiceAllocatorCore::StealVoiceIndex() {
  switch (allocation_strategy_) {
    case VoiceAllocationStrategy::OldestNote:
      return StealOldestVoiceIndex();
    case VoiceAllocationStrategy::RoundRobin:
      return StealRoundRobinVoiceIndex();
    case VoiceAllocationStrategy::FirstAvailable:
    default:
      return 0;
  }
}

int8_t VoiceAllocatorCore::StealOldestVoiceIndex() {
  uint8_t oldest_idx = 0;
  uint32_t oldest_time = voices_[0].note_on_time;

  for (uint8_t i = 1; i < max_voices_; ++i) {
    if (voices_[i].note_on_time < oldest_time) {
      oldest_time = voices_[i].note_on_time;
      oldest_idx = i;
    }
  }

  return static_cast<int8_t>(oldest_idx);
}

int8_t VoiceAllocatorCore::StealRoundRobinVoiceIndex() {
  round_robin_index_ = (round_robin_index_ + 1) % max_voices_;
  return static_cast<int8_t>(round_robin_index_);
}

void VoiceAllocatorCore::AddHeldNote(uint8_t note) {
  // Check if already in buffer (avoid duplicates)
  for (uint8_t i = 0; i < num_held_notes_; ++i) {
    if (held_notes_[i] == note) {
      // Move to end (most recent)
      for (uint8_t j = i; j < num_held_notes_ - 1; ++j) {
        held_notes_[j] = held_notes_[j + 1];
      }
      held_notes_[num_held_notes_ - 1] = note;
      return;
    }
  }

  if (num_held_notes_ < kMaxHeldNotes) {
    held_notes_[num_held_notes_++] = note;
  }
}

void VoiceAllocatorCore::RemoveHeldNote(uint8_t note) {
  uint8_t new_count = 0;
  for (uint8_t i = 0; i < num_held_notes_; ++i) {
    if (held_notes_[i] != note) {
      held_notes_[new_count++] = held_notes_[i];
    }
  }
  num_held_notes_ = new_count;
}

uint8_t VoiceAllocatorCore::GetLastHeldNote() const {
  return (num_held_notes_ > 0) ? held_notes_[num_held_notes_ - 1] : 0;
}

}  // namespace common
