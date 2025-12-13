/**
 * @file wav_file.c
 * @brief WAV file I/O utilities implementation
 * 
 * Provides WAV file reading/writing using libsndfile for ARM cross-compilation.
 */

#include "wav_file.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

/**
 * @brief Open WAV file for reading
 */
int wav_file_open_read(wav_file_t* wav, const char* filename) {
    if (!wav || !filename) {
        return WAV_FILE_ERR_OPEN;
    }
    
    memset(wav, 0, sizeof(wav_file_t));
    
    // Set up SF_INFO for reading
    wav->info.format = 0;  // Let libsndfile auto-detect format
    
    // Open file
    wav->file = sf_open(filename, SFM_READ, &wav->info);
    if (!wav->file) {
        fprintf(stderr, "Error opening WAV file for reading: %s\n", sf_strerror(NULL));
        return WAV_FILE_ERR_OPEN;
    }
    
    // Validate format
    if (wav->info.channels <= 0 || wav->info.samplerate <= 0) {
        sf_close(wav->file);
        memset(wav, 0, sizeof(wav_file_t));
        return WAV_FILE_ERR_FORMAT;
    }
    
    wav->is_open = true;
    wav->is_writing = false;
    wav->filename = strdup(filename);
    
    return WAV_FILE_OK;
}

/**
 * @brief Open WAV file for writing
 */
int wav_file_open_write(wav_file_t* wav, const char* filename, 
                       uint32_t sample_rate, uint8_t channels) {
    if (!wav || !filename || channels == 0 || sample_rate == 0) {
        return WAV_FILE_ERR_OPEN;
    }
    
    memset(wav, 0, sizeof(wav_file_t));
    
    // Set up SF_INFO for writing
    wav->info.samplerate = sample_rate;
    wav->info.channels = channels;
    wav->info.format = SF_FORMAT_WAV | SF_FORMAT_FLOAT;  // 32-bit float WAV
    
    // Open file
    wav->file = sf_open(filename, SFM_WRITE, &wav->info);
    if (!wav->file) {
        fprintf(stderr, "Error opening WAV file for writing: %s\n", sf_strerror(NULL));
        return WAV_FILE_ERR_OPEN;
    }
    
    wav->is_open = true;
    wav->is_writing = true;
    wav->filename = strdup(filename);
    
    return WAV_FILE_OK;
}

/**
 * @brief Read audio frames from WAV file
 */
size_t wav_file_read_frames(wav_file_t* wav, float* buffer, size_t frames) {
    if (!wav || !buffer || !wav->is_open || wav->is_writing) {
        return 0;
    }
    
    sf_count_t frames_read = sf_readf_float(wav->file, buffer, frames);
    
    if (frames_read < 0) {
        return 0;
    }
    
    return (size_t)frames_read;
}

/**
 * @brief Write audio frames to WAV file
 */
size_t wav_file_write_frames(wav_file_t* wav, const float* buffer, size_t frames) {
    if (!wav || !buffer || !wav->is_open || !wav->is_writing) {
        return 0;
    }
    
    sf_count_t frames_written = sf_writef_float(wav->file, buffer, frames);
    
    if (frames_written < 0) {
        return 0;
    }
    
    return (size_t)frames_written;
}

/**
 * @brief Get WAV file sample rate
 */
uint32_t wav_file_get_sample_rate(const wav_file_t* wav) {
    if (!wav || !wav->is_open) {
        return 0;
    }
    
    return (uint32_t)wav->info.samplerate;
}

/**
 * @brief Get WAV file channel count
 */
uint8_t wav_file_get_channels(const wav_file_t* wav) {
    if (!wav || !wav->is_open) {
        return 0;
    }
    
    return (uint8_t)wav->info.channels;
}

/**
 * @brief Get WAV file frame count (total length)
 */
size_t wav_file_get_frames(const wav_file_t* wav) {
    if (!wav || !wav->is_open) {
        return 0;
    }
    
    return (size_t)wav->info.frames;
}

/**
 * @brief Close WAV file
 */
void wav_file_close(wav_file_t* wav) {
    if (!wav) {
        return;
    }
    
    if (wav->file) {
        sf_close(wav->file);
        wav->file = NULL;
    }
    
    if (wav->filename) {
        free(wav->filename);
        wav->filename = NULL;
    }
    
    memset(wav, 0, sizeof(wav_file_t));
}

/**
 * @brief Check if WAV file is valid/open
 */
bool wav_file_is_valid(const wav_file_t* wav) {
    return wav && wav->is_open && wav->file;
}

/**
 * @brief Print WAV file information
 */
void wav_file_print_info(const wav_file_t* wav) {
    if (!wav || !wav->is_open) {
        printf("WAV file: Invalid or not open\n");
        return;
    }
    
    const char* format_name = "Unknown";
    switch (wav->info.format & SF_FORMAT_TYPEMASK) {
        case SF_FORMAT_WAV:  format_name = "WAV"; break;
        case SF_FORMAT_AIFF: format_name = "AIFF"; break;
        case SF_FORMAT_FLAC: format_name = "FLAC"; break;
        default: break;
    }
    
    const char* subformat_name = "Unknown";
    switch (wav->info.format & SF_FORMAT_SUBMASK) {
        case SF_FORMAT_PCM_16: subformat_name = "16-bit PCM"; break;
        case SF_FORMAT_PCM_24: subformat_name = "24-bit PCM"; break;
        case SF_FORMAT_PCM_32: subformat_name = "32-bit PCM"; break;
        case SF_FORMAT_FLOAT:  subformat_name = "32-bit float"; break;
        case SF_FORMAT_DOUBLE: subformat_name = "64-bit float"; break;
        default: break;
    }
    
    printf("WAV file: %s\n", wav->filename ? wav->filename : "unnamed");
    printf("  Format: %s (%s)\n", format_name, subformat_name);
    printf("  Sample rate: %d Hz\n", wav->info.samplerate);
    printf("  Channels: %d\n", wav->info.channels);
    printf("  Frames: %ld (%.2f seconds)\n", 
           wav->info.frames, 
           (double)wav->info.frames / wav->info.samplerate);
}