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

#ifdef __cplusplus
}  // extern "C"
#endif
