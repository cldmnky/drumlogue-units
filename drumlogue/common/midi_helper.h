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

#ifdef DEBUG
#include <cstdio>
#endif

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
   * Uses pre-computed lookup table for optimal performance.
   * Formula: f = 440 * 2^((note - 69) / 12)
   */
  static inline float NoteToFreq(uint8_t note) {
    // Lookup table approach - much faster than powf()
    // Table is initialized on first call (lazy static initialization)
    static const float note_freq_table[128] = {
      // Octave -1 (MIDI 0-11)
      8.176f, 8.662f, 9.177f, 9.723f, 10.301f, 10.913f, 11.562f, 12.250f, 12.978f, 13.750f, 14.568f, 15.434f,
      // Octave 0 (MIDI 12-23)
      16.352f, 17.324f, 18.354f, 19.445f, 20.602f, 21.827f, 23.125f, 24.500f, 25.957f, 27.500f, 29.135f, 30.868f,
      // Octave 1 (MIDI 24-35)
      32.703f, 34.648f, 36.708f, 38.891f, 41.203f, 43.654f, 46.249f, 48.999f, 51.913f, 55.000f, 58.270f, 61.735f,
      // Octave 2 (MIDI 36-47)
      65.406f, 69.296f, 73.416f, 77.782f, 82.407f, 87.307f, 92.499f, 97.999f, 103.826f, 110.000f, 116.541f, 123.471f,
      // Octave 3 (MIDI 48-59)
      130.813f, 138.591f, 146.832f, 155.563f, 164.814f, 174.614f, 184.997f, 195.998f, 207.652f, 220.000f, 233.082f, 246.942f,
      // Octave 4 (MIDI 60-71) - Middle C = 60
      261.626f, 277.183f, 293.665f, 311.127f, 329.628f, 349.228f, 369.994f, 391.995f, 415.305f, 440.000f, 466.164f, 493.883f,
      // Octave 5 (MIDI 72-83)
      523.251f, 554.365f, 587.330f, 622.254f, 659.255f, 698.456f, 739.989f, 783.991f, 830.609f, 880.000f, 932.328f, 987.767f,
      // Octave 6 (MIDI 84-95)
      1046.502f, 1108.731f, 1174.659f, 1244.508f, 1318.510f, 1396.913f, 1479.978f, 1567.982f, 1661.219f, 1760.000f, 1864.655f, 1975.533f,
      // Octave 7 (MIDI 96-107)
      2093.005f, 2217.461f, 2349.318f, 2489.016f, 2637.020f, 2793.826f, 2959.955f, 3135.963f, 3322.438f, 3520.000f, 3729.310f, 3951.066f,
      // Octave 8 (MIDI 108-119)
      4186.009f, 4434.922f, 4698.636f, 4978.032f, 5274.041f, 5587.652f, 5919.911f, 6271.927f, 6644.875f, 7040.000f, 7458.620f, 7902.133f,
      // Octave 9 (MIDI 120-127) - Last 8 notes
      8372.018f, 8869.844f, 9397.273f, 9956.063f, 10548.082f, 11175.303f, 11839.822f, 12543.854f
    };

    if (note > 127) return 440.0f;  // Safety clamp
    
    float freq = note_freq_table[note];
    
#ifdef DEBUG
    fprintf(stderr, "[MidiHelper] Note %d (%s%d) -> %.3f Hz\n", 
            note, NoteName(note), NoteOctave(note), freq);
    fflush(stderr);
#endif
    
    return freq;
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
   * @brief Convert MIDI channel pressure (0-127) to normalized float (0.0-1.0)
   * @param pressure MIDI channel pressure value
   * @return Normalized pressure (0.0 to 1.0)
   */
  static constexpr float PressureToFloat(uint8_t pressure) {
    return static_cast<float>(pressure) / 127.0f;
  }
  
  /**
   * @brief Convert MIDI aftertouch (0-127) to normalized float (0.0-1.0)
   * @param aftertouch MIDI aftertouch value
   * @return Normalized aftertouch (0.0 to 1.0)
   */
  static constexpr float AftertouchToFloat(uint8_t aftertouch) {
    return static_cast<float>(aftertouch) / 127.0f;
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
