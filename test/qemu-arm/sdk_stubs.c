/**
 * @file sdk_stubs.c
 * @brief Logue SDK runtime stubs implementation
 * 
 * Provides minimal implementations of the logue SDK runtime functions
 * needed by .drmlgunit files when running in the QEMU test environment.
 */

#include "sdk_stubs.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

// Global runtime descriptor
static unit_runtime_desc_t* g_runtime_desc = NULL;

// Dummy sample data for units that access samples
static sample_wrapper_t g_dummy_sample = {
    .frames = 1024,
    .channels = 1,
    .sample_ptr = NULL  // Will be allocated on first use
};

static float* g_dummy_sample_data = NULL;

/**
 * @brief Initialize SDK stubs
 */
void sdk_stubs_init(void) {
    // Create dummy sample data (1 second of silence at 48kHz)
    const size_t sample_frames = 48000;
    g_dummy_sample_data = calloc(sample_frames, sizeof(float));
    
    if (g_dummy_sample_data) {
        // Generate a simple sine wave for testing
        for (size_t i = 0; i < sample_frames; i++) {
            g_dummy_sample_data[i] = 0.1f * sinf(2.0f * 3.14159f * 440.0f * i / 48000.0f);
        }
        
        g_dummy_sample.frames = sample_frames;
        g_dummy_sample.channels = 1;
        g_dummy_sample.sample_ptr = g_dummy_sample_data;
    }
    
    printf("SDK stubs initialized\n");
}

/**
 * @brief Create runtime descriptor for unit testing
 */
unit_runtime_desc_t* sdk_stubs_create_runtime_desc(uint32_t sample_rate, 
                                                   uint16_t buffer_size,
                                                   uint8_t channels) {
    if (g_runtime_desc) {
        free(g_runtime_desc);
    }
    
    g_runtime_desc = malloc(sizeof(unit_runtime_desc_t));
    if (!g_runtime_desc) {
        return NULL;
    }
    
    // Fill runtime descriptor
    // Note: target is just platform, unit will add module type
    g_runtime_desc->target = UNIT_TARGET_PLATFORM;
    g_runtime_desc->api = UNIT_API_VERSION;
    g_runtime_desc->samplerate = sample_rate;
    g_runtime_desc->frames_per_buffer = buffer_size;
    g_runtime_desc->input_channels = channels;
    g_runtime_desc->output_channels = channels;
    
    // Set sample access function pointers
    g_runtime_desc->get_num_sample_banks = stub_get_num_sample_banks;
    g_runtime_desc->get_num_samples_for_bank = stub_get_num_samples_for_bank;
    g_runtime_desc->get_sample = stub_get_sample;
    
    return g_runtime_desc;
}

/**
 * @brief Update runtime descriptor target from loaded unit header
 */
void sdk_stubs_set_target(uint16_t target) {
    if (g_runtime_desc) {
        g_runtime_desc->target = target;
    }
}

/**
 * @brief Clean up SDK stubs
 */
void sdk_stubs_cleanup(void) {
    if (g_runtime_desc) {
        free(g_runtime_desc);
        g_runtime_desc = NULL;
    }
    
    if (g_dummy_sample_data) {
        free(g_dummy_sample_data);
        g_dummy_sample_data = NULL;
    }
    
    printf("SDK stubs cleaned up\n");
}

/**
 * @brief Get number of sample banks (stub implementation)
 */
uint8_t stub_get_num_sample_banks(void) {
    // Return minimal sample bank count for testing
    return 1;
}

/**
 * @brief Get number of samples in a bank (stub implementation)
 */
uint8_t stub_get_num_samples_for_bank(uint8_t bank) {
    (void)bank;  // Unused parameter
    
    // Return minimal sample count for testing
    return 1;
}

/**
 * @brief Get sample data (stub implementation)
 */
const sample_wrapper_t* stub_get_sample(uint8_t bank, uint8_t sample) {
    (void)bank;    // Unused parameter
    (void)sample;  // Unused parameter
    
    // Return dummy sample for any request
    return &g_dummy_sample;
}