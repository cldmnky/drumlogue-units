#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef PORTAUDIO_PRESENT
#include <portaudio.h>
#define pa_sleep(ms) Pa_Sleep(ms)
#else
static void pa_sleep(int ms) { (void)ms; }
#endif

#include "../audio/audio_engine.h"
#include "../core/unit_loader.h"
#include "../sdk/runtime_stubs.h"

static void print_usage(const char* prog) {
    fprintf(stderr, "Usage: %s --unit <path> [--frames N] [--rate Hz] [--channels C] [--rt seconds]\n", prog);
}

int main(int argc, char** argv) {
    const char* unit_path = NULL;
    uint32_t sample_rate = 48000;
    uint16_t frames = 128;
    uint8_t channels = 2;
    int run_rt_seconds = 0;  // 0 => skip real-time

    for (int i = 1; i < argc; ++i) {
        if ((strcmp(argv[i], "--unit") == 0 || strcmp(argv[i], "-u") == 0) && i + 1 < argc) {
            unit_path = argv[++i];
        } else if (strcmp(argv[i], "--frames") == 0 && i + 1 < argc) {
            frames = (uint16_t)atoi(argv[++i]);
        } else if (strcmp(argv[i], "--rate") == 0 && i + 1 < argc) {
            sample_rate = (uint32_t)atoi(argv[++i]);
        } else if (strcmp(argv[i], "--channels") == 0 && i + 1 < argc) {
            channels = (uint8_t)atoi(argv[++i]);
        } else if (strcmp(argv[i], "--rt") == 0 && i + 1 < argc) {
            run_rt_seconds = atoi(argv[++i]);
        } else if (strcmp(argv[i], "--help") == 0 || strcmp(argv[i], "-h") == 0) {
            print_usage(argv[0]);
            return 0;
        }
    }

    if (!unit_path) {
        print_usage(argv[0]);
        return 1;
    }

    runtime_stub_state_t stub_state;
    if (runtime_stubs_init(&stub_state, sample_rate, frames, channels) != 0) {
        fprintf(stderr, "Failed to init runtime stubs\n");
        return 1;
    }

    unit_loader_t loader;
    if (unit_loader_open(unit_path, &loader) != 0) {
        fprintf(stderr, "Failed to load unit: %s\n", unit_path);
        runtime_stubs_teardown(&stub_state);
        return 1;
    }

    printf("Loaded unit: %s\n", loader.header ? loader.header->name : "<unknown>");
    printf("Params: %u\n", loader.header ? loader.header->num_params : 0);

    // Align runtime target with the loaded module (synth/revfx/etc.) before init
    if (loader.header && stub_state.runtime_desc) {
        stub_state.runtime_desc->target = loader.header->target;
    }

    if (unit_loader_init(&loader, stub_state.runtime_desc) != 0) {
        fprintf(stderr, "unit_init failed\n");
        unit_loader_close(&loader);
        runtime_stubs_teardown(&stub_state);
        return 1;
    }

    // Render one buffer (silence in) for sanity
    {
        const size_t samples = (size_t)frames * (size_t)channels;
        float* input = calloc(samples, sizeof(float));
        float* output = calloc(samples, sizeof(float));
        if (!input || !output) {
            fprintf(stderr, "Allocation failed\n");
            free(input);
            free(output);
            unit_loader_close(&loader);
            runtime_stubs_teardown(&stub_state);
            return 1;
        }
        unit_loader_render(&loader, input, output, frames);
        printf("One-shot render OK. First sample L: %f\n", output[0]);
        if (channels > 1) {
            printf("First sample R: %f\n", output[1]);
        }
        free(input);
        free(output);
    }

    if (run_rt_seconds > 0) {
        audio_config_t cfg = {
            .sample_rate = sample_rate,
            .frames_per_buffer = frames,
            .input_channels = channels,  // loopback/passthrough if available
            .output_channels = channels,
        };

        audio_engine_t* engine = audio_engine_create(&cfg, &loader, &stub_state);
        if (!engine) {
            fprintf(stderr, "Failed to create audio engine (PortAudio). Is PortAudio installed?\n");
            unit_loader_close(&loader);
            runtime_stubs_teardown(&stub_state);
            return 1;
        }

        if (audio_engine_start(engine) != 0) {
            audio_engine_destroy(engine);
            unit_loader_close(&loader);
            runtime_stubs_teardown(&stub_state);
            return 1;
        }

        printf("Real-time audio running for %d seconds...\n", run_rt_seconds);
        pa_sleep(run_rt_seconds * 1000);
        audio_engine_stop(engine);
        printf("CPU load: %.2f\n", audio_engine_cpu_load(engine));
        audio_engine_destroy(engine);
    }

    unit_loader_close(&loader);
    runtime_stubs_teardown(&stub_state);

    return 0;
}
