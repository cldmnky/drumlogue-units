/**
 * @file wav_file.h
 * @brief WAV file I/O utilities for ARM unit testing
 * 
 * Provides WAV file reading/writing compatible with ARM cross-compilation.
 * Uses libsndfile for cross-platform audio file support.
 */

#ifndef WAV_FILE_H_
#define WAV_FILE_H_

#include <stdint.h>
#include <stdbool.h>
#include <sndfile.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief WAV file handle
 */
typedef struct {
    SNDFILE* file;
    SF_INFO info;
    bool is_open;
    bool is_writing;
    char* filename;
} wav_file_t;

/**
 * @brief Open WAV file for reading
 */
int wav_file_open_read(wav_file_t* wav, const char* filename);

/**
 * @brief Open WAV file for writing
 */
int wav_file_open_write(wav_file_t* wav, const char* filename, 
                       uint32_t sample_rate, uint8_t channels);

/**
 * @brief Read audio frames from WAV file
 * @param wav WAV file handle
 * @param buffer Output buffer (interleaved float samples)
 * @param frames Number of frames to read
 * @return Number of frames actually read
 */
size_t wav_file_read_frames(wav_file_t* wav, float* buffer, size_t frames);

/**
 * @brief Write audio frames to WAV file
 * @param wav WAV file handle
 * @param buffer Input buffer (interleaved float samples)
 * @param frames Number of frames to write
 * @return Number of frames actually written
 */
size_t wav_file_write_frames(wav_file_t* wav, const float* buffer, size_t frames);

/**
 * @brief Get WAV file sample rate
 */
uint32_t wav_file_get_sample_rate(const wav_file_t* wav);

/**
 * @brief Get WAV file channel count
 */
uint8_t wav_file_get_channels(const wav_file_t* wav);

/**
 * @brief Get WAV file frame count (total length)
 */
size_t wav_file_get_frames(const wav_file_t* wav);

/**
 * @brief Close WAV file
 */
void wav_file_close(wav_file_t* wav);

/**
 * @brief Check if WAV file is valid/open
 */
bool wav_file_is_valid(const wav_file_t* wav);

/**
 * @brief Print WAV file information
 */
void wav_file_print_info(const wav_file_t* wav);

// Error codes
#define WAV_FILE_OK         0
#define WAV_FILE_ERR_OPEN   -1
#define WAV_FILE_ERR_FORMAT -2
#define WAV_FILE_ERR_READ   -3
#define WAV_FILE_ERR_WRITE  -4

#ifdef __cplusplus
}
#endif

#endif  // WAV_FILE_H_