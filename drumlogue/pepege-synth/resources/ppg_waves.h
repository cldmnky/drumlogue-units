/**
 * @file ppg_waves.h
 * @brief PPG Wave 2.2 8-bit waveform data for PPG oscillator
 *
 * This file provides the original 8-bit PPG waveform data in the format
 * required by the PPG oscillator (common/ppg_osc.h).
 *
 * Format:
 * - 64 samples per wave (half-cycle, mirrored for full cycle)
 * - 8-bit unsigned (0-255, center at 128)
 * - 256 total waveforms from the EPROM; a wavetable selects a subset of
 *   them by (wave index, slot) pairs (see resources/ppg_tables.h)
 */

#pragma once

#include <cstdint>

// Number of PPG waves (original data has 256 waves)
#define PPG_NUM_WAVES 256
#define PPG_SAMPLES_PER_WAVE 64

// The raw 8-bit PPG waveform data (64 samples per wave, 256 waves total)
// This is extracted from ppg_data.c's ppg_waveforms_data[]
extern const uint8_t ppg_wave_data[PPG_NUM_WAVES * PPG_SAMPLES_PER_WAVE];

/**
 * @brief Get pointer to a specific wave's 64-sample data
 * @param wave_index Wave number (0-255)
 * @return Pointer to 64 bytes of wave data
 */
inline const uint8_t* GetPpgWave(uint8_t wave_index) {
    return &ppg_wave_data[wave_index * PPG_SAMPLES_PER_WAVE];
}
