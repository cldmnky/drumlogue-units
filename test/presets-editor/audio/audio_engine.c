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
};

#ifdef PORTAUDIO_PRESENT

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

float audio_engine_cpu_load(audio_engine_t* engine) {
    if (!engine || !engine->stream) return -1.0f;
    return (float)Pa_GetStreamCpuLoad(engine->stream);
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

float audio_engine_cpu_load(audio_engine_t* engine) { (void)engine; return -1.0f; }

#endif
