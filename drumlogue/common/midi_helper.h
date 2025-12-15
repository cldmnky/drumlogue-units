/**
 * @file midi_helper.h
 * @brief MIDI utility functions
 *
 * Common MIDI conversions used across synthesizer units:
 * - Note to frequency conversion
 * - Velocity scaling
 * - Pitch bend processing
 * - Note name formatting
 */

#pragma once

#include <cstdint>
#include <cmath>

namespace common {

/**
 * @brief MIDI utility functions
 * 
 * All methods are static and constexpr where possible.
 */
class MidiHelper {
 public:
  /**
   * @brief Convert MIDI note number to frequency
   * @param note MIDI note number (0-127, A4=69)
   * @return Frequency in Hz (A4 = 440Hz tuning)
   * 
   * Formula: f = 440 * 2^((note - 69) / 12)
   */
  static inline float NoteToFreq(uint8_t note) {
    // Fast approximation using powf
    return 440.0f * powf(2.0f, (static_cast<float>(note) - 69.0f) / 12.0f);
  }
  
  /**
   * @brief Convert MIDI note to frequency with custom A4 tuning
   * @param note MIDI note number
   * @param a4_freq A4 frequency in Hz (default 440Hz)
   * @return Frequency in Hz
   */
  static inline float NoteToFreqWithTuning(uint8_t note, float a4_freq = 440.0f) {
    return a4_freq * powf(2.0f, (static_cast<float>(note) - 69.0f) / 12.0f);
  }
  
  /**
   * @brief Convert MIDI velocity (0-127) to normalized float (0.0-1.0)
   * @param velocity MIDI velocity value
   * @return Normalized velocity (0.0 to 1.0)
   */
  static constexpr float VelocityToFloat(uint8_t velocity) {
    return static_cast<float>(velocity) / 127.0f;
  }
  
  /**
   * @brief Convert MIDI velocity with exponential curve
   * @param velocity MIDI velocity (0-127)
   * @param curve Curve amount (1.0 = linear, <1 = softer, >1 = harder)
   * @return Normalized velocity with curve applied
   */
  static inline float VelocityToFloatCurved(uint8_t velocity, float curve = 1.0f) {
    float linear = VelocityToFloat(velocity);
    return powf(linear, curve);
  }
  
  /**
   * @brief Convert pitch bend value to semitones
   * @param bend Pitch bend value (0-16383, center=8192)
   * @param range_semitones Pitch bend range in semitones (default ±2)
   * @return Pitch deviation in semitones (-range to +range)
   */
  static inline float PitchBendToSemitones(uint16_t bend, 
                                           float range_semitones = 2.0f) {
    // Center at 8192: bend - 8192 gives -8192 to +8191
    const float centered = (static_cast<float>(bend) - 8192.0f) / 8192.0f;
    return centered * range_semitones;
  }
  
  /**
   * @brief Convert pitch bend to frequency multiplier
   * @param bend Pitch bend value (0-16383)
   * @param range_semitones Pitch bend range in semitones
   * @return Frequency multiplier (0.5 to 2.0 for ±12 semitones)
   */
  static inline float PitchBendToMultiplier(uint16_t bend, 
                                            float range_semitones = 2.0f) {
    float semitones = PitchBendToSemitones(bend, range_semitones);
    return powf(2.0f, semitones / 12.0f);
  }
  
  /**
   * @brief Get note name from MIDI note number
   * @param note MIDI note number (0-127)
   * @return Note name string (e.g., "C", "F#", "Bb")
   */
  static const char* NoteName(uint8_t note) {
    static const char* names[] = {
      "C", "C#", "D", "D#", "E", "F", 
      "F#", "G", "G#", "A", "A#", "B"
    };
    return names[note % 12];
  }
  
  /**
   * @brief Get octave number from MIDI note
   * @param note MIDI note number (C4 = note 60)
   * @return Octave number (-1 to 9)
   */
  static constexpr int8_t NoteOctave(uint8_t note) {
    return (note / 12) - 1;
  }
  
  /**
   * @brief Convert cents to frequency ratio
   * @param cents Pitch deviation in cents
   * @return Frequency multiplier
   * 
   * Formula: ratio = 2^(cents / 1200)
   */
  static inline float CentsToRatio(float cents) {
    return powf(2.0f, cents / 1200.0f);
  }
  
  /**
   * @brief Convert semitones to frequency ratio
   * @param semitones Pitch deviation in semitones
   * @return Frequency multiplier
   */
  static inline float SemitonesToRatio(float semitones) {
    return powf(2.0f, semitones / 12.0f);
  }
  
  /**
   * @brief Convert frequency ratio to cents
   * @param ratio Frequency ratio (e.g., 1.0595 for 1 semitone)
   * @return Pitch deviation in cents
   */
  static inline float RatioToCents(float ratio) {
    return 1200.0f * log2f(ratio);
  }
  
  /**
   * @brief Clamp MIDI value to valid range
   * @param value Value to clamp
   * @return Clamped value (0-127)
   */
  static constexpr uint8_t ClampMidi(int32_t value) {
    if (value < 0) return 0;
    if (value > 127) return 127;
    return static_cast<uint8_t>(value);
  }
  
  /**
   * @brief Convert CC value (0-127) to normalized float
   * @param cc_value CC value
   * @return Normalized value (0.0 to 1.0)
   */
  static constexpr float CCToFloat(uint8_t cc_value) {
    return static_cast<float>(cc_value) / 127.0f;
  }
  
  /**
   * @brief Convert CC value to bipolar float (-1.0 to +1.0)
   * @param cc_value CC value (0-127, center=64)
   * @return Bipolar value (-1.0 to +1.0)
   */
  static constexpr float CCToBipolar(uint8_t cc_value) {
    return (static_cast<float>(cc_value) - 64.0f) / 64.0f;
  }
  
  /**
   * @brief Check if note is a black key (sharp/flat)
   * @param note MIDI note number
   * @return True if black key, false if white key
   */
  static constexpr bool IsBlackKey(uint8_t note) {
    const uint8_t pitch_class = note % 12;
    // Black keys: C#, D#, F#, G#, A# (1, 3, 6, 8, 10)
    return (pitch_class == 1 || pitch_class == 3 || 
            pitch_class == 6 || pitch_class == 8 || pitch_class == 10);
  }
  
  /**
   * @brief Get MIDI note number from note name and octave
   * @param pitch_class Note pitch class (0=C, 1=C#, etc.)
   * @param octave Octave number (C4 is octave 4)
   * @return MIDI note number, or 255 if invalid
   */
  static constexpr uint8_t NoteFromPitchClass(uint8_t pitch_class, int8_t octave) {
    if (pitch_class >= 12 || octave < -1 || octave > 9) {
      return 255;  // Invalid
    }
    return (octave + 1) * 12 + pitch_class;
  }
};

}  // namespace common
