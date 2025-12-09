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
 * - 256 total waveforms organized into 16 banks of 16 waves each
 *   (we use the first 8 of each bank for compatibility)
 */

#pragma once

#include <cstdint>

// Number of PPG waves (original data has 256 waves)
#define PPG_NUM_WAVES 256
#define PPG_SAMPLES_PER_WAVE 64
#define PPG_NUM_BANKS 16
#define PPG_WAVES_PER_BANK 8  // Use 8 waves per bank for morphing

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

/**
 * @brief Get the first wave of a bank
 * @param bank Bank index (0-15)
 * @return Pointer to 64 bytes of the first wave in the bank
 */
inline const uint8_t* GetPpgBankFirstWave(uint8_t bank) {
    if (bank >= PPG_NUM_BANKS) bank = PPG_NUM_BANKS - 1;
    // Each bank starts at wave bank*16, but we only use 8 waves for morphing
    return GetPpgWave(bank * 16);
}

/**
 * @brief Get a wave within a bank
 * @param bank Bank index (0-15)
 * @param wave_in_bank Wave index within bank (0-7, will be doubled for spacing)
 * @return Pointer to 64 bytes of wave data
 */
inline const uint8_t* GetPpgBankWave(uint8_t bank, uint8_t wave_in_bank) {
    if (bank >= PPG_NUM_BANKS) bank = PPG_NUM_BANKS - 1;
    if (wave_in_bank >= PPG_WAVES_PER_BANK) wave_in_bank = PPG_WAVES_PER_BANK - 1;
    // Space waves evenly across the 16 waves in each bank (use every other wave)
    return GetPpgWave(bank * 16 + wave_in_bank * 2);
}
