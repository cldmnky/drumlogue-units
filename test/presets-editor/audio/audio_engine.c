#include "audio_engine.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef PORTAUDIO_PRESENT
#include <portaudio.h>
#endif

#include "ring_buffer.h"

#define PARAM_QUEUE_CAPACITY 64
#define PITCH_BUFFER_SIZE 4096  // ~85ms at 48kHz for pitch detection

typedef struct {
    uint8_t id;
    int32_t value;
} param_msg_t;

struct audio_engine {
    audio_config_t cfg;
    unit_loader_t* loader;
    runtime_stub_state_t* runtime_state;
#ifdef PORTAUDIO_PRESENT
    PaStream* stream;
#else
    void* stream;
#endif
    ring_buffer_t param_queue;
    float master_volume;  // Thread-safe read in audio callback
    
    // Pitch detection
    float pitch_buffer[PITCH_BUFFER_SIZE];
    uint32_t pitch_buffer_pos;
    float detected_pitch;  // In Hz, 0 if no pitch detected
};

#ifdef PORTAUDIO_PRESENT

// Soft-clipping function using tanh for smooth saturation
static inline float soft_clip(float x) {
    // tanh provides smooth saturation at ±1.0
    // For values near 0, tanh(x) ≈ x, so no distortion at low levels
    // Scale to make clipping more gradual
    return tanhf(x * 0.9f) / 0.9f;
}

// YIN-style autocorrelation pitch detection with hysteresis
static float detect_pitch(const float* buffer, uint32_t buffer_size, uint32_t sample_rate) {
    static float last_pitch = 0.0f;
    
    if (buffer_size < 128) return last_pitch;
    
    // Compute RMS to check if signal is strong enough
    float rms = 0.0f;
    for (uint32_t i = 0; i < buffer_size; i++) {
        rms += buffer[i] * buffer[i];
    }
    rms = sqrtf(rms / buffer_size);
    
#ifdef DEBUG
    static int debug_log_counter = 0;
    if (++debug_log_counter >= 10) {  // Log every 10th detection (~2 Hz)
        debug_log_counter = 0;
        fprintf(stderr, "[Tuner] RMS: %.6f", rms);
    }
#endif
    
    // Lower threshold - be more sensitive to detect signal
    if (rms < 0.001f) {
#ifdef DEBUG
        if (debug_log_counter == 1) {  // Only log on counter reset
            fprintf(stderr, " -> No signal (RMS too low)\n");
        }
#endif
        last_pitch = 0.0f;
        return 0.0f;
    }
    
    const uint32_t min_period = sample_rate / 1500;  // Up to 1500 Hz
    const uint32_t max_period = sample_rate / 50;    // Down to 50 Hz
    const uint32_t search_end = buffer_size / 2 < max_period ? buffer_size / 2 : max_period;
    
    // YIN difference function
    float* diff = (float*)alloca(search_end * sizeof(float));
    
    // Compute difference function
    diff[0] = 1.0f;
    for (uint32_t lag = 1; lag < search_end; lag++) {
        float sum = 0.0f;
        for (uint32_t i = 0; i < buffer_size - lag; i++) {
            float delta = buffer[i] - buffer[i + lag];
            sum += delta * delta;
        }
        diff[lag] = sum;
    }
    
    // Cumulative mean normalized difference
    float running_sum = 0.0f;
    for (uint32_t lag = 1; lag < search_end; lag++) {
        running_sum += diff[lag];
        diff[lag] = diff[lag] * (float)lag / running_sum;
    }
    
    // Find first minimum below threshold (more lenient threshold)
    const float threshold = 0.25f;  // Increased from 0.15 to be less strict
    for (uint32_t lag = min_period; lag < search_end; lag++) {
        if (diff[lag] < threshold) {
            // Parabolic interpolation for better accuracy
            float detected_pitch;
            if (lag > 0 && lag < search_end - 1) {
                float alpha = diff[lag - 1];
                float beta = diff[lag];
                float gamma = diff[lag + 1];
                float delta_interp = (alpha - gamma) / (2.0f * (2.0f * beta - alpha - gamma));
                float better_lag = (float)lag + delta_interp;
                detected_pitch = (float)sample_rate / better_lag;
            } else {
                detected_pitch = (float)sample_rate / (float)lag;
            }
            
            // Hysteresis: only update if change is significant or first detection
            if (last_pitch == 0.0f || fabsf(detected_pitch - last_pitch) > 2.0f) {
                last_pitch = detected_pitch;
            }
            
#ifdef DEBUG
            if (debug_log_counter == 1) {  // Only log on counter reset
                fprintf(stderr, ", Lag: %u, Pitch: %.2f Hz", lag, last_pitch);
                if (detected_pitch != last_pitch) {
                    fprintf(stderr, " (hysteresis: was %.2f)", detected_pitch);
                }
                fprintf(stderr, "\n");
                fflush(stderr);
            }
#endif
            
            return last_pitch;
        }
    }
    
#ifdef DEBUG
    if (debug_log_counter == 1) {  // Only log on counter reset
        fprintf(stderr, " -> No pitch found (no minimum below threshold)\n");
        fflush(stderr);
    }
#endif
    
    // No pitch found - keep last detected pitch (provides stability)
    return last_pitch;
}

static int audio_cb(const void* input,
                    void* output,
                    unsigned long frame_count,
                    const PaStreamCallbackTimeInfo* time_info,
                    PaStreamCallbackFlags status_flags,
                    void* user_data) {
    (void)time_info;
    (void)status_flags;

    audio_engine_t* engine = (audio_engine_t*)user_data;
    if (!engine || !engine->loader) {
        return paContinue;
    }

    // Apply queued parameter updates
    param_msg_t msg;
    while (ring_buffer_pop(&engine->param_queue, &msg) == 0) {
        unit_loader_set_param(engine->loader, msg.id, msg.value);
    }

    const float* in = (const float*)input;
    float* out = (float*)output;

    // If input is NULL, PortAudio supplies silence; make sure out is zeroed
    if (!in) {
        memset(out, 0, sizeof(float) * frame_count * engine->cfg.output_channels);
    }

    // If we got a different frames_per_buffer than expected, clamp to min to stay safe
    uint32_t frames = (uint32_t)frame_count;
    uint32_t max_frames = engine->cfg.frames_per_buffer;
    if (frames > max_frames && max_frames > 0) {
        frames = max_frames;
    }

    unit_loader_render(engine->loader, in ? in : out, out, frames);
    
    // Capture UNPROCESSED mono signal for pitch analysis BEFORE volume/clipping
    for (uint32_t i = 0; i < frames; i++) {
        engine->pitch_buffer[engine->pitch_buffer_pos] = out[i * engine->cfg.output_channels];
        engine->pitch_buffer_pos = (engine->pitch_buffer_pos + 1) % PITCH_BUFFER_SIZE;
    }
    
    // Apply master volume and soft-clipping to prevent distortion
    const float volume = engine->master_volume;
    const uint32_t total_samples = frames * engine->cfg.output_channels;
    for (uint32_t i = 0; i < total_samples; i++) {
        // Apply volume then soft-clip to prevent digital clipping
        out[i] = soft_clip(out[i] * volume);
    }
    
    // Update pitch detection periodically (throttle to reduce CPU load)
    static uint32_t pitch_counter = 0;
    if (++pitch_counter >= 24) {  // ~20 Hz update rate
        pitch_counter = 0;
        engine->detected_pitch = detect_pitch(engine->pitch_buffer, PITCH_BUFFER_SIZE, engine->cfg.sample_rate);
    }
    
    return paContinue;
}

audio_engine_t* audio_engine_create(const audio_config_t* cfg,
                                    unit_loader_t* loader,
                                    runtime_stub_state_t* runtime_state) {
    if (!cfg || !loader || !runtime_state || !runtime_state->runtime_desc) {
        return NULL;
    }

    audio_engine_t* engine = calloc(1, sizeof(audio_engine_t));
    if (!engine) return NULL;

    engine->cfg = *cfg;
    engine->loader = loader;
    engine->runtime_state = runtime_state;
    engine->master_volume = cfg->master_volume > 0.0f ? cfg->master_volume : 0.5f;
    
    // Initialize pitch detection
    memset(engine->pitch_buffer, 0, sizeof(engine->pitch_buffer));
    engine->pitch_buffer_pos = 0;
    engine->detected_pitch = 0.0f;

    if (ring_buffer_init(&engine->param_queue, PARAM_QUEUE_CAPACITY, sizeof(param_msg_t)) != 0) {
        free(engine);
        return NULL;
    }

    PaError err = Pa_Initialize();
    if (err != paNoError) {
        fprintf(stderr, "PortAudio init failed: %s\n", Pa_GetErrorText(err));
        ring_buffer_free(&engine->param_queue);
        free(engine);
        return NULL;
    }

    PaStreamParameters inputParams = {0};
    PaStreamParameters outputParams = {0};

    if (cfg->input_channels > 0) {
        inputParams.device = Pa_GetDefaultInputDevice();
        inputParams.channelCount = cfg->input_channels;
        inputParams.sampleFormat = paFloat32;
        inputParams.suggestedLatency = Pa_GetDeviceInfo(inputParams.device)->defaultLowInputLatency;
    }

    outputParams.device = Pa_GetDefaultOutputDevice();
    outputParams.channelCount = cfg->output_channels;
    outputParams.sampleFormat = paFloat32;
    outputParams.suggestedLatency = Pa_GetDeviceInfo(outputParams.device)->defaultLowOutputLatency;

    // Open stream
    err = Pa_OpenStream(&engine->stream,
                        cfg->input_channels > 0 ? &inputParams : NULL,
                        &outputParams,
                        cfg->sample_rate,
                        cfg->frames_per_buffer,
                        paNoFlag,
                        audio_cb,
                        engine);
    if (err != paNoError) {
        fprintf(stderr, "PortAudio open failed: %s\n", Pa_GetErrorText(err));
        Pa_Terminate();
        ring_buffer_free(&engine->param_queue);
        free(engine);
        return NULL;
    }
    
    // Debug: Check actual sample rate
    const PaStreamInfo* stream_info = Pa_GetStreamInfo(engine->stream);
    if (stream_info) {
        printf("[Audio Engine] Requested sample rate: %u Hz, Actual: %.0f Hz\n",
               cfg->sample_rate, stream_info->sampleRate);
        if (fabsf(stream_info->sampleRate - cfg->sample_rate) > 100.0f) {
            fprintf(stderr, "WARNING: Sample rate mismatch! This will cause tuning problems.\n");
        }
    }

    return engine;
}

void audio_engine_destroy(audio_engine_t* engine) {
    if (!engine) return;
    if (engine->stream) {
        Pa_CloseStream(engine->stream);
    }
    Pa_Terminate();
    ring_buffer_free(&engine->param_queue);
    free(engine);
}

int audio_engine_start(audio_engine_t* engine) {
    if (!engine || !engine->stream) return -1;
    PaError err = Pa_StartStream(engine->stream);
    if (err != paNoError) {
        fprintf(stderr, "PortAudio start failed: %s\n", Pa_GetErrorText(err));
        return -1;
    }
    return 0;
}

void audio_engine_stop(audio_engine_t* engine) {
    if (!engine || !engine->stream) return;
    Pa_StopStream(engine->stream);
}

int audio_engine_set_param(audio_engine_t* engine, uint8_t id, int32_t value) {
    if (!engine) return -1;
    param_msg_t msg = {.id = id, .value = value};
    return ring_buffer_push(&engine->param_queue, &msg);
}

void audio_engine_set_master_volume(audio_engine_t* engine, float volume) {
    if (!engine) return;
    // Clamp to valid range
    if (volume < 0.0f) volume = 0.0f;
    if (volume > 1.0f) volume = 1.0f;
    engine->master_volume = volume;
}

float audio_engine_cpu_load(audio_engine_t* engine) {
    if (!engine || !engine->stream) return -1.0f;
    return (float)Pa_GetStreamCpuLoad(engine->stream);
}

float audio_engine_get_detected_pitch(audio_engine_t* engine) {
    if (!engine) return 0.0f;
    return engine->detected_pitch;
}

#else  // PORTAUDIO_PRESENT not available

audio_engine_t* audio_engine_create(const audio_config_t* cfg,
                                    unit_loader_t* loader,
                                    runtime_stub_state_t* runtime_state) {
    (void)cfg;
    (void)loader;
    (void)runtime_state;
    fprintf(stderr, "PortAudio not available; rebuild after installing portaudio\n");
    return NULL;
}

void audio_engine_destroy(audio_engine_t* engine) { (void)engine; }

int audio_engine_start(audio_engine_t* engine) { (void)engine; return -1; }

void audio_engine_stop(audio_engine_t* engine) { (void)engine; }

int audio_engine_set_param(audio_engine_t* engine, uint8_t id, int32_t value) {
    (void)engine; (void)id; (void)value; return -1; }

void audio_engine_set_master_volume(audio_engine_t* engine, float volume) {
    (void)engine; (void)volume; }

float audio_engine_cpu_load(audio_engine_t* engine) { (void)engine; return -1.0f; }

float audio_engine_get_detected_pitch(audio_engine_t* engine) { (void)engine; return 0.0f; }

#endif
