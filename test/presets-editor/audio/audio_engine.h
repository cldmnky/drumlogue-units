#pragma once

#include <stdint.h>
#include <stdbool.h>

#include "../core/unit_loader.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    uint32_t sample_rate;     // e.g., 48000
    uint16_t frames_per_buffer; // e.g., 128 or 256
    uint8_t input_channels;   // 0,1,2
    uint8_t output_channels;  // 1 or 2
    float master_volume;      // 0.0 to 1.0, default 0.5
} audio_config_t;

typedef struct audio_engine audio_engine_t;

audio_engine_t* audio_engine_create(const audio_config_t* cfg,
                                    unit_loader_t* loader,
                                    runtime_stub_state_t* runtime_state);
void audio_engine_destroy(audio_engine_t* engine);

int audio_engine_start(audio_engine_t* engine);
void audio_engine_stop(audio_engine_t* engine);

// Thread-safe enqueue of parameter updates (applied in audio thread)
int audio_engine_set_param(audio_engine_t* engine, uint8_t id, int32_t value);

// Set master output volume (0.0 to 1.0)
void audio_engine_set_master_volume(audio_engine_t* engine, float volume);

// Returns PortAudio stream CPU load (0..1), or -1 on error
float audio_engine_cpu_load(audio_engine_t* engine);

// Get detected pitch in Hz (0 if no pitch detected)
float audio_engine_get_detected_pitch(audio_engine_t* engine);

// Get waveform samples for visualization
// buffer: output buffer to copy samples to
// buffer_size: size of output buffer
// Returns: number of samples copied
uint32_t audio_engine_get_waveform_samples(audio_engine_t* engine, 
                                           float* buffer, 
                                           uint32_t buffer_size);

// Enable or disable tuner (pitch detection)
// When disabled, tuner completely bypasses pitch detection to save CPU
void audio_engine_set_tuner_enabled(audio_engine_t* engine, bool enabled);

// Check if tuner is enabled
bool audio_engine_is_tuner_enabled(audio_engine_t* engine);

// Performance statistics
typedef struct {
    double render_time_avg_us;      // Average render time in microseconds
    double render_time_min_us;      // Minimum render time
    double render_time_max_us;      // Maximum render time
    double callback_time_avg_us;    // Total callback time
    uint64_t total_frames_processed;
    uint32_t buffer_underruns;      // Count of potential audio glitches
} audio_perf_stats_t;

// Get performance statistics
void audio_engine_get_perf_stats(audio_engine_t* engine, audio_perf_stats_t* stats);

// Reset performance statistics
void audio_engine_reset_perf_stats(audio_engine_t* engine);

#ifdef __cplusplus
}  // extern "C"
#endif
