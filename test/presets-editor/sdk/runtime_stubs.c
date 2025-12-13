#include "runtime_stubs.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// Simple sine sample for units that call sample APIs
static float* g_sample_data = NULL;
static sample_wrapper_t g_sample_wrapper = {0};

int runtime_stubs_init(runtime_stub_state_t* state,
                       uint32_t sample_rate,
                       uint16_t frames_per_buffer,
                       uint8_t channels) {
    if (!state) {
        return -1;
    }

    memset(state, 0, sizeof(*state));

    // Allocate and populate runtime descriptor
    state->runtime_desc = calloc(1, sizeof(unit_runtime_desc_t));
    if (!state->runtime_desc) {
        return -1;
    }

    state->runtime_desc->target = UNIT_TARGET_PLATFORM;
    state->runtime_desc->api = UNIT_API_VERSION;
    state->runtime_desc->samplerate = sample_rate;
    state->runtime_desc->frames_per_buffer = frames_per_buffer;
    state->runtime_desc->input_channels = channels;
    state->runtime_desc->output_channels = channels;
    state->runtime_desc->get_num_sample_banks = runtime_stub_get_num_sample_banks;
    state->runtime_desc->get_num_samples_for_bank = runtime_stub_get_num_samples_for_bank;
    state->runtime_desc->get_sample = runtime_stub_get_sample;

    // Create a tiny sine sample bank (1 bank, 1 sample)
    const uint32_t sample_frames = sample_rate;  // 1 second
    g_sample_data = calloc(sample_frames, sizeof(float));
    if (!g_sample_data) {
        return -1;
    }

    for (uint32_t i = 0; i < sample_frames; i++) {
        g_sample_data[i] = 0.1f * sinf(2.0f * 3.14159265f * 440.0f * (float)i / (float)sample_rate);
    }

    g_sample_wrapper.frames = sample_frames;
    g_sample_wrapper.channels = 1;
    g_sample_wrapper.sample_ptr = g_sample_data;

    return 0;
}

void runtime_stubs_teardown(runtime_stub_state_t* state) {
    if (!state) {
        return;
    }

    free(g_sample_data);
    g_sample_data = NULL;
    g_sample_wrapper.sample_ptr = NULL;

    free(state->runtime_desc);
    state->runtime_desc = NULL;
}

uint8_t runtime_stub_get_num_sample_banks(void) {
    return 1;
}

uint8_t runtime_stub_get_num_samples_for_bank(uint8_t bank) {
    (void)bank;
    return 1;
}

const sample_wrapper_t* runtime_stub_get_sample(uint8_t bank, uint8_t sample) {
    (void)bank;
    (void)sample;
    return &g_sample_wrapper;
}
