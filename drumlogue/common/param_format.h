/**
 * @file param_format.h
 * @brief Standard parameter value formatting utilities
 *
 * Provides consistent string formatting for common parameter types:
 * - Percentages (0-100%)
 * - Bipolar ranges (±100%)
 * - Frequencies (Hz/kHz auto-scaling)
 * - Time (ms/s auto-scaling)
 * - Decibels
 * - Pitch (cents/semitones)
 * - Octave ranges (16'/8'/4')
 *
 * All functions use provided buffers to avoid dynamic allocation.
 * Suitable for real-time audio callbacks.
 */

#pragma once

#include <cstdio>
#include <cstdint>
#include <cmath>

namespace common {

/**
 * @brief Parameter formatting utilities
 * 
 * All methods are static and thread-safe (if using separate buffers).
 */
class ParamFormat {
 public:
  /**
   * @brief Format as percentage: "50%"
   * @param buf Output buffer
   * @param size Buffer size
   * @param value Parameter value
   * @param min Minimum value (default 0)
   * @param max Maximum value (default 100)
   * @return Pointer to buffer
   */
  static const char* Percent(char* buf, size_t size, int32_t value, 
                             int32_t min = 0, int32_t max = 100) {
    // Map value from [min,max] to [0,100]
    int32_t percent = ((value - min) * 100) / (max - min);
    snprintf(buf, size, "%d%%", percent);
    return buf;
  }
  
  /**
   * @brief Format as bipolar percentage: "-50%" to "+50%"
   * @param buf Output buffer
   * @param size Buffer size
   * @param value Current value
   * @param min Minimum value
   * @param max Maximum value
   * @return Pointer to buffer
   * 
   * Centers the range: (min+max)/2 displays as "0%"
   */
  static const char* BipolarPercent(char* buf, size_t size, int32_t value,
                                    int32_t min, int32_t max) {
    int32_t center = (min + max) / 2;
    int32_t offset = value - center;
    int32_t range = (max - min) / 2;
    int32_t percent = (offset * 100) / range;
    snprintf(buf, size, "%+d%%", percent);
    return buf;
  }
  
  /**
   * @brief Format as bipolar value: "-50" to "+50"
   * @param buf Output buffer
   * @param size Buffer size
   * @param value Current value
   * @param min Minimum value
   * @param max Maximum value
   * @return Pointer to buffer
   * 
   * Centers the range: (min+max)/2 displays as "0"
   */
  static const char* BipolarValue(char* buf, size_t size, int32_t value,
                                  int32_t min, int32_t max) {
    int32_t center = (min + max) / 2;
    int32_t offset = value - center;
    (void)(max - min);  // Suppress unused variable warning; range available for future use
    // Map to -range to +range
    int32_t bipolar = offset;
    snprintf(buf, size, "%+d", bipolar);
    return buf;
  }
  
  /**
   * @brief Format as frequency: "440Hz" or "4.4kHz"
   * @param buf Output buffer
   * @param size Buffer size
   * @param freq_hz Frequency in Hz
   * @return Pointer to buffer
   * 
   * Auto-scales to kHz if >= 1000 Hz
   */
  static const char* Frequency(char* buf, size_t size, float freq_hz) {
    if (freq_hz >= 10000.0f) {
      // >= 10kHz: show as integer kHz
      snprintf(buf, size, "%.0fkHz", freq_hz / 1000.0f);
    } else if (freq_hz >= 1000.0f) {
      // 1-10kHz: show 1 decimal place
      snprintf(buf, size, "%.1fkHz", freq_hz / 1000.0f);
    } else if (freq_hz >= 100.0f) {
      // 100-999Hz: show as integer Hz
      snprintf(buf, size, "%.0fHz", freq_hz);
    } else {
      // < 100Hz: show 1 decimal place
      snprintf(buf, size, "%.1fHz", freq_hz);
    }
    return buf;
  }
  
  /**
   * @brief Format as time: "50ms" or "1.5s"
   * @param buf Output buffer
   * @param size Buffer size
   * @param time_ms Time in milliseconds
   * @return Pointer to buffer
   * 
   * Auto-scales to seconds if >= 1000 ms
   */
  static const char* Time(char* buf, size_t size, float time_ms) {
    if (time_ms >= 10000.0f) {
      // >= 10s: show as integer seconds
      snprintf(buf, size, "%.0fs", time_ms / 1000.0f);
    } else if (time_ms >= 1000.0f) {
      // 1-10s: show 1 decimal place
      snprintf(buf, size, "%.1fs", time_ms / 1000.0f);
    } else if (time_ms >= 100.0f) {
      // 100-999ms: show as integer ms
      snprintf(buf, size, "%.0fms", time_ms);
    } else {
      // < 100ms: show 1 decimal place
      snprintf(buf, size, "%.1fms", time_ms);
    }
    return buf;
  }
  
  /**
   * @brief Format as decibels: "-12.0dB"
   * @param buf Output buffer
   * @param size Buffer size
   * @param db Decibel value
   * @return Pointer to buffer
   */
  static const char* Decibels(char* buf, size_t size, float db) {
    if (fabsf(db) >= 10.0f) {
      snprintf(buf, size, "%+.0fdB", db);
    } else {
      snprintf(buf, size, "%+.1fdB", db);
    }
    return buf;
  }
  
  /**
   * @brief Format as pitch: "+7st" (semitones) or "-25c" (cents)
   * @param buf Output buffer
   * @param size Buffer size
   * @param cents Pitch deviation in cents
   * @return Pointer to buffer
   * 
   * Shows semitones if |cents| >= 100, otherwise shows cents
   */
  static const char* Pitch(char* buf, size_t size, float cents) {
    if (fabsf(cents) >= 100.0f) {
      // Show semitones
      float semitones = cents / 100.0f;
      if (fabsf(semitones) >= 10.0f) {
        snprintf(buf, size, "%+.0fst", semitones);
      } else {
        snprintf(buf, size, "%+.1fst", semitones);
      }
    } else {
      // Show cents
      snprintf(buf, size, "%+.0fc", cents);
    }
    return buf;
  }
  
  /**
   * @brief Format octave range: "16'" / "8'" / "4'" / "2'"
   * @param octave Octave selector (0-3)
   * @return Static string for octave range
   */
  static const char* OctaveRange(int32_t octave) {
    static const char* ranges[] = {"16'", "8'", "4'", "2'"};
    if (octave >= 0 && octave < 4) {
      return ranges[octave];
    }
    return "??";
  }
  
  /**
   * @brief Format waveform name
   * @param buf Output buffer
   * @param size Buffer size
   * @param waveforms Array of waveform names
   * @param num_waveforms Number of waveforms in array
   * @param index Current waveform index
   * @return Pointer to buffer
   */
  static const char* Waveform(char* buf, size_t size,
                              const char** waveforms,
                              uint8_t num_waveforms,
                              uint8_t index) {
    if (index < num_waveforms) {
      snprintf(buf, size, "%s", waveforms[index]);
    } else {
      snprintf(buf, size, "???");
    }
    return buf;
  }
  
  /**
   * @brief Format ratio: "1:2" or "3:1"
   * @param buf Output buffer
   * @param size Buffer size
   * @param numerator Ratio numerator
   * @param denominator Ratio denominator
   * @return Pointer to buffer
   */
  static const char* Ratio(char* buf, size_t size, 
                          int32_t numerator, int32_t denominator) {
    snprintf(buf, size, "%d:%d", numerator, denominator);
    return buf;
  }
  
  /**
   * @brief Format note name: "C4", "F#5", etc.
   * @param buf Output buffer
   * @param size Buffer size
   * @param midi_note MIDI note number (0-127)
   * @return Pointer to buffer
   */
  static const char* NoteName(char* buf, size_t size, uint8_t midi_note) {
    static const char* note_names[] = {
      "C", "C#", "D", "D#", "E", "F", 
      "F#", "G", "G#", "A", "A#", "B"
    };
    int octave = (midi_note / 12) - 1;
    const char* note = note_names[midi_note % 12];
    snprintf(buf, size, "%s%d", note, octave);
    return buf;
  }
  
  /**
   * @brief Format on/off state
   * @param enabled True for ON, false for OFF
   * @return Static string "ON" or "OFF"
   */
  static const char* OnOff(bool enabled) {
    return enabled ? "ON" : "OFF";
  }
};

}  // namespace common
