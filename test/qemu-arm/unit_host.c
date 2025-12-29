/**
 * @file unit_host.c
 * @brief QEMU ARM unit host implementation
 * 
 * Minimal unit host for loading and testing .drmlgunit files in QEMU ARM environment.
 * Loads units as shared libraries and processes WAV files through them.
 */

#define _POSIX_C_SOURCE 199309L  // For clock_gettime

#include "unit_host.h"
#include "sdk_stubs.h"
#include "wav_file.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dlfcn.h>
#include <unistd.h>
#include <errno.h>
#include <getopt.h>
#include <time.h>
#include <math.h>

// Global state
static unit_host_state_t g_state = {0};
static unit_callbacks_t g_callbacks = {0};

/**
 * @brief Load unit callback symbols from shared library
 */
static int load_unit_symbols(unit_host_state_t* state) {
    if (!state->unit_handle) {
        fprintf(stderr, "Error: Unit not loaded\n");
        return UNIT_HOST_ERR_LOAD;
    }
    
    // Clear previous symbols
    memset(&g_callbacks, 0, sizeof(g_callbacks));
    
    // Load unit header symbol
    state->unit_header = (unit_header_t*)dlsym(state->unit_handle, "unit_header");
    if (!state->unit_header) {
        fprintf(stderr, "Error: Failed to find unit_header symbol: %s\n", dlerror());
        return UNIT_HOST_ERR_SYMBOL;
    }
    
    // Load essential callbacks
    g_callbacks.unit_init = dlsym(state->unit_handle, "unit_init");
    g_callbacks.unit_teardown = dlsym(state->unit_handle, "unit_teardown");
    g_callbacks.unit_reset = dlsym(state->unit_handle, "unit_reset");
    g_callbacks.unit_resume = dlsym(state->unit_handle, "unit_resume");
    g_callbacks.unit_suspend = dlsym(state->unit_handle, "unit_suspend");
    g_callbacks.unit_render = dlsym(state->unit_handle, "unit_render");
    g_callbacks.unit_set_param_value = dlsym(state->unit_handle, "unit_set_param_value");
    
    // Check essential symbols
    if (!g_callbacks.unit_init || !g_callbacks.unit_render) {
        fprintf(stderr, "Error: Missing essential unit symbols (unit_init, unit_render)\n");
        return UNIT_HOST_ERR_SYMBOL;
    }
    
    // Load optional callbacks (may not be present in all units)
    g_callbacks.unit_get_param_value = dlsym(state->unit_handle, "unit_get_param_value");
    g_callbacks.unit_get_param_str_value = dlsym(state->unit_handle, "unit_get_param_str_value");
    g_callbacks.unit_get_param_bmp_value = dlsym(state->unit_handle, "unit_get_param_bmp_value");
    g_callbacks.unit_set_tempo = dlsym(state->unit_handle, "unit_set_tempo");
    g_callbacks.unit_note_on = dlsym(state->unit_handle, "unit_note_on");
    g_callbacks.unit_note_off = dlsym(state->unit_handle, "unit_note_off");
    g_callbacks.unit_gate_on = dlsym(state->unit_handle, "unit_gate_on");
    g_callbacks.unit_gate_off = dlsym(state->unit_handle, "unit_gate_off");
    g_callbacks.unit_all_note_off = dlsym(state->unit_handle, "unit_all_note_off");
    g_callbacks.unit_pitch_bend = dlsym(state->unit_handle, "unit_pitch_bend");
    g_callbacks.unit_channel_pressure = dlsym(state->unit_handle, "unit_channel_pressure");
    g_callbacks.unit_aftertouch = dlsym(state->unit_handle, "unit_aftertouch");
    g_callbacks.unit_load_preset = dlsym(state->unit_handle, "unit_load_preset");
    g_callbacks.unit_get_preset_index = dlsym(state->unit_handle, "unit_get_preset_index");
    g_callbacks.unit_get_preset_name = dlsym(state->unit_handle, "unit_get_preset_name");
    
    return UNIT_HOST_OK;
}

/**
 * @brief Initialize unit host
 */
int unit_host_init(unit_host_config_t* config, unit_host_state_t* state) {
    if (!config || !state) {
        return UNIT_HOST_ERR_ARGS;
    }
    
    // Initialize state
    memset(state, 0, sizeof(unit_host_state_t));
    
    // Initialize SDK stubs
    sdk_stubs_init();
    
    // Create runtime descriptor
    state->runtime_desc = sdk_stubs_create_runtime_desc(
        config->sample_rate,
        config->buffer_size,
        config->channels
    );
    
    if (!state->runtime_desc) {
        fprintf(stderr, "Error: Failed to create runtime descriptor\n");
        return UNIT_HOST_ERR_INIT;
    }
    
    if (config->verbose) {
        printf("Unit host initialized:\n");
        printf("  Sample rate: %u Hz\n", config->sample_rate);
        printf("  Buffer size: %u frames\n", config->buffer_size);
        printf("  Channels: %u\n", config->channels);
    }
    
    return UNIT_HOST_OK;
}

/**
 * @brief Load .drmlgunit file
 */
int unit_host_load_unit(const char* unit_path, unit_host_state_t* state) {
    if (!unit_path || !state) {
        return UNIT_HOST_ERR_ARGS;
    }
    
    // Check if file exists
    if (access(unit_path, R_OK) != 0) {
        fprintf(stderr, "Error: Cannot access unit file: %s (%s)\n", 
                unit_path, strerror(errno));
        return UNIT_HOST_ERR_FILE;
    }
    
    // Load as shared library
    state->unit_handle = dlopen(unit_path, RTLD_LAZY | RTLD_LOCAL);
    if (!state->unit_handle) {
        fprintf(stderr, "Error: Failed to load unit: %s\n", dlerror());
        return UNIT_HOST_ERR_LOAD;
    }
    
    // Load symbols
    int result = load_unit_symbols(state);
    if (result != UNIT_HOST_OK) {
        dlclose(state->unit_handle);
        state->unit_handle = NULL;
        return result;
    }
    
    printf("✓ Loaded unit: %s\n", unit_path);
    return UNIT_HOST_OK;
}

/**
 * @brief Initialize loaded unit
 */
int unit_host_init_unit(unit_host_state_t* state) {
    if (!state || !g_callbacks.unit_init || !state->runtime_desc) {
        return UNIT_HOST_ERR_ARGS;
    }
    
    // Update runtime descriptor target to match loaded unit's target
    if (state->unit_header) {
        sdk_stubs_set_target(state->unit_header->target);
    }
    
    // Initialize the unit
    int8_t result = g_callbacks.unit_init(state->runtime_desc);
    if (result != 0) {
        fprintf(stderr, "Error: Unit initialization failed: %d\n", result);
        return UNIT_HOST_ERR_INIT;
    }
    
    state->unit_initialized = true;
    
    // Reset unit to neutral state
    if (g_callbacks.unit_reset) {
        g_callbacks.unit_reset();
    }
    
    // Resume unit
    if (g_callbacks.unit_resume) {
        g_callbacks.unit_resume();
    }
    
    printf("✓ Unit initialized successfully\n");
    return UNIT_HOST_OK;
}

/**
 * @brief Process WAV file through unit
 */
int unit_host_process_wav(const char* input_path, const char* output_path,
                         unit_host_state_t* state, unit_host_config_t* config) {
    if (!input_path || !output_path || !state || !config) {
        return UNIT_HOST_ERR_ARGS;
    }
    
    if (!state->unit_initialized) {
        fprintf(stderr, "Error: Unit not initialized\n");
        return UNIT_HOST_ERR_INIT;
    }
    
    // Check if this is a synth unit
    uint8_t module_type = state->unit_header->target & 0xFF;
    bool is_synth = (module_type == k_unit_module_synth);
    
    // For profiling synths, we want 10 seconds of output even if input is shorter
    uint32_t target_frames = config->sample_rate;  // Default: 1 second
    if (config->profile && is_synth) {
        target_frames = config->sample_rate * 10;  // 10 seconds for synth profiling
    }
    
    // Open input WAV file
    wav_file_t input_wav = {0};
    int result = wav_file_open_read(&input_wav, input_path);
    if (result != WAV_FILE_OK) {
        fprintf(stderr, "Error: Failed to open input WAV: %s\n", input_path);
        return UNIT_HOST_ERR_FILE;
    }
    
    // Validate input format
    uint32_t input_sr = wav_file_get_sample_rate(&input_wav);
    uint8_t input_channels = wav_file_get_channels(&input_wav);
    uint32_t input_frames = wav_file_get_frames(&input_wav);
    
    if (input_sr != config->sample_rate) {
        fprintf(stderr, "Warning: Input sample rate (%u) != config (%u)\n", 
                input_sr, config->sample_rate);
    }
    
    // Open output WAV file
    wav_file_t output_wav = {0};
    result = wav_file_open_write(&output_wav, output_path, config->sample_rate, config->channels);
    if (result != WAV_FILE_OK) {
        wav_file_close(&input_wav);
        fprintf(stderr, "Error: Failed to open output WAV: %s\n", output_path);
        return UNIT_HOST_ERR_FILE;
    }
    
    if (config->verbose) {
        printf("Processing audio...\n");
        wav_file_print_info(&input_wav);
        printf("Output: %s (%u Hz, %u channels)\n", 
               output_path, config->sample_rate, config->channels);
        if (config->profile && is_synth) {
            printf("Profiling mode: will generate %u seconds of output\n", target_frames / config->sample_rate);
        }
    }
    
    // Process audio in buffers
    const size_t buffer_frames = config->buffer_size;
    const size_t input_samples = buffer_frames * input_channels;
    const size_t output_samples = buffer_frames * config->channels;
    
    float* input_buffer = malloc(input_samples * sizeof(float));
    float* output_buffer = malloc(output_samples * sizeof(float));
    
    if (!input_buffer || !output_buffer) {
        wav_file_close(&input_wav);
        wav_file_close(&output_wav);
        free(input_buffer);
        free(output_buffer);
        fprintf(stderr, "Error: Memory allocation failed\n");
        return UNIT_HOST_ERR_PROCESS;
    }
    
    size_t total_frames = 0;
    size_t frames_read;
    bool note_triggered = false;
    bool input_exhausted = false;
    
    // Initialize profiling stats
    if (config->profile) {
        state->profile_stats.total_render_time = 0.0;
        state->profile_stats.min_render_time = 1e9;  // Start with huge value
        state->profile_stats.max_render_time = 0.0;
        state->profile_stats.render_count = 0;
        state->profile_stats.total_audio_time = 0.0;
    }
    
    // For profiling: parameter change tracking for synths
    uint32_t param_change_interval = config->sample_rate;  // Change parameters every 1 second
    uint32_t next_param_change = param_change_interval;
    uint32_t param_change_count = 0;
    const uint32_t max_param_changes = 10;  // 10 different configurations
    
    // For synths: MIDI note triggering at intervals
    uint32_t note_trigger_interval = config->sample_rate;  // Trigger new note every 1 second
    uint32_t next_note_trigger = 0;  // First note at start
    uint8_t current_note = 60;  // Start with C4
    const uint8_t note_sequence[] = {60, 64, 67, 72, 55, 62, 69, 48, 76, 52};  // C4, E4, G4, C5, G3, D4, A4, C3, E5, E3
    const uint8_t note_velocities[] = {100, 90, 110, 80, 95, 105, 85, 100, 120, 75};
    uint32_t note_index = 0;
    const uint32_t num_notes = sizeof(note_sequence) / sizeof(note_sequence[0]);
    
    // Seed random number generator for parameter changes
    srand(time(NULL));
    
    // Main processing loop
    while (total_frames < target_frames) {
        // Read from input WAV if available, otherwise use silence
        if (!input_exhausted) {
            frames_read = wav_file_read_frames(&input_wav, input_buffer, buffer_frames);
            if (frames_read == 0) {
                input_exhausted = true;
            }
        }
        
        // For synths in profiling mode, continue with silence after input ends
        if (input_exhausted) {
            frames_read = buffer_frames;
            if (total_frames + frames_read > target_frames) {
                frames_read = target_frames - total_frames;
            }
            memset(input_buffer, 0, input_samples * sizeof(float));
        }
        
        if (frames_read == 0) break;
        // Clear output buffer
        memset(output_buffer, 0, output_samples * sizeof(float));
        
        // For synth units, trigger MIDI notes at intervals
        if (is_synth && total_frames >= next_note_trigger && note_index < num_notes) {
            if (g_callbacks.unit_note_off && note_triggered) {
                // Send note off for previous note
                g_callbacks.unit_note_off(current_note);
            }
            
            if (g_callbacks.unit_note_on) {
                // Trigger new note from sequence
                current_note = note_sequence[note_index];
                uint8_t velocity = note_velocities[note_index];
                g_callbacks.unit_note_on(current_note, velocity);
                note_triggered = true;
                
                if (config->verbose) {
                    const char* note_names[] = {"C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B"};
                    uint8_t octave = (current_note / 12) - 1;
                    const char* note_name = note_names[current_note % 12];
                    printf("Triggered MIDI note #%u: %s%u (%u) velocity %u at %.1fs\n", 
                           note_index + 1, note_name, octave, current_note, velocity,
                           (float)total_frames / config->sample_rate);
                }
            }
            
            note_index++;
            next_note_trigger += note_trigger_interval;
        }
        
        // For profiling synths: Change random parameters at intervals
        if (config->profile && is_synth && total_frames >= next_param_change && 
            param_change_count < max_param_changes) {
            // Change 3-5 random parameters
            uint32_t num_params = state->unit_header->num_params;
            if (num_params > 0) {
                uint32_t params_to_change = 3 + (rand() % 3);  // 3-5 params
                if (config->verbose) {
                    printf("Changing %u parameters (variation %u/%u)...\n", 
                           params_to_change, param_change_count + 1, max_param_changes);
                }
                
                for (uint32_t i = 0; i < params_to_change && i < num_params; i++) {
                    // Pick a random parameter
                    uint8_t param_id = rand() % num_params;
                    unit_param_t* param = &state->unit_header->params[param_id];
                    
                    // Skip if parameter type is none
                    if (param->type == k_unit_param_type_none) {
                        continue;
                    }
                    
                    // Generate random value within parameter range
                    int32_t range = param->max - param->min;
                    if (range > 0) {
                        int32_t value = param->min + (rand() % (range + 1));
                        
                        if (g_callbacks.unit_set_param_value) {
                            g_callbacks.unit_set_param_value(param_id, value);
                            if (config->verbose) {
                                printf("  Param %u (%s): %d\n", param_id, param->name, value);
                            }
                        }
                    }
                }
            }
            
            param_change_count++;
            next_param_change += param_change_interval;
        }
        
        // Handle channel mapping for input
        const float* unit_input = input_buffer;
        if (input_channels > config->channels) {
            // Downmix if input has more channels than unit expects
            for (size_t i = 0; i < frames_read; i++) {
                float sum = 0.0f;
                for (uint8_t ch = 0; ch < input_channels; ch++) {
                    sum += input_buffer[i * input_channels + ch];
                }
                input_buffer[i] = sum / input_channels;  // Mix to mono
            }
        }
        
        // Process through unit with profiling
        struct timespec render_start, render_end;
        if (config->profile) {
            clock_gettime(CLOCK_MONOTONIC, &render_start);
        }
        
        g_callbacks.unit_render(unit_input, output_buffer, frames_read);
        
        if (config->profile) {
            clock_gettime(CLOCK_MONOTONIC, &render_end);
            
            // Calculate elapsed time for this render call
            double elapsed = (render_end.tv_sec - render_start.tv_sec) +
                           (render_end.tv_nsec - render_start.tv_nsec) / 1e9;
            
            state->profile_stats.total_render_time += elapsed;
            if (elapsed < state->profile_stats.min_render_time) {
                state->profile_stats.min_render_time = elapsed;
            }
            if (elapsed > state->profile_stats.max_render_time) {
                state->profile_stats.max_render_time = elapsed;
            }
            state->profile_stats.render_count++;
            state->profile_stats.total_audio_time += (double)frames_read / config->sample_rate;
        }
        
        // Write processed audio
        wav_file_write_frames(&output_wav, output_buffer, frames_read);
        total_frames += frames_read;
        
        if (config->verbose && (total_frames % (config->sample_rate / 4)) == 0) {
            printf("Processed %.1f seconds...\n", (float)total_frames / config->sample_rate);
        }
    }
    
    // Cleanup
    free(input_buffer);
    free(output_buffer);
    wav_file_close(&input_wav);
    wav_file_close(&output_wav);
    
    if (config->verbose) {
        printf("✓ Processing complete: %.2f seconds processed\n", 
               (float)total_frames / config->sample_rate);
    }
    
    return UNIT_HOST_OK;
}

/**
 * @brief Test preset loading and verification
 */
int unit_host_test_presets(unit_host_state_t* state, unit_host_config_t* config) {
    if (!state || !config) {
        return UNIT_HOST_ERR_ARGS;
    }
    
    if (!state->unit_initialized) {
        return UNIT_HOST_ERR_INIT;
    }
    
    // Check if unit has presets
    uint8_t num_presets = state->unit_header->num_presets;
    if (num_presets == 0) {
        printf("Unit has no presets to test\n");
        return UNIT_HOST_OK;
    }
    
    // Check if preset callbacks are available
    if (!g_callbacks.unit_load_preset || !g_callbacks.unit_get_preset_index || 
        !g_callbacks.unit_get_preset_name) {
        fprintf(stderr, "Error: Unit missing preset callback functions\n");
        return UNIT_HOST_ERR_SYMBOL;
    }
    
    printf("\n");
    printf("═══════════════════════════════════════════════════════════════\n");
    printf("                     PRESET TEST REPORT                        \n");
    printf("═══════════════════════════════════════════════════════════════\n");
    printf("\n");
    printf("Unit: %s\n", state->unit_header->name);
    printf("Number of presets: %u\n", num_presets);
    printf("\n");
    
    // List all preset names first
    printf("Available presets:\n");
    for (uint8_t i = 0; i < num_presets; i++) {
        const char* name = g_callbacks.unit_get_preset_name(i);
        if (name && name[0] != '\0') {
            printf("  [%u] %s\n", i, name);
        } else {
            printf("  [%u] <unnamed or error>\n", i);
        }
    }
    printf("\n");
    
    // Test loading each preset
    printf("Testing preset loading...\n");
    bool all_passed = true;
    
    for (uint8_t i = 0; i < num_presets; i++) {
        const char* expected_name = g_callbacks.unit_get_preset_name(i);
        
        printf("  Testing preset %u (%s)...\n", i, expected_name ? expected_name : "<null>");
        
        // Load preset
        g_callbacks.unit_load_preset(i);
        
        // Give the unit a moment to settle (simulate hardware timing)
        // This is important because some units may defer preset loading
        usleep(1000);  // 1ms delay
        
        // Verify preset index matches
        uint8_t actual_index = g_callbacks.unit_get_preset_index();
        if (actual_index != i) {
            printf("    ❌ FAIL: Expected index %u, got %u\n", i, actual_index);
            all_passed = false;
        } else {
            printf("    ✅ Index correct: %u\n", actual_index);
        }
        
        // Verify name still returns correctly
        const char* verify_name = g_callbacks.unit_get_preset_name(i);
        if (!verify_name || verify_name[0] == '\0') {
            printf("    ❌ FAIL: Name query returned null/empty after load\n");
            all_passed = false;
        } else if (expected_name && strcmp(verify_name, expected_name) != 0) {
            printf("    ❌ FAIL: Name changed from '%s' to '%s'\n", expected_name, verify_name);
            all_passed = false;
        } else {
            printf("    ✅ Name verified: %s\n", verify_name);
        }
        
        // Check some parameters to ensure they've changed
        // (this is a heuristic - we can't know exact values without unit internals)
        bool params_changed = false;
        if (g_callbacks.unit_get_param_value) {
            for (uint8_t p = 0; p < state->unit_header->num_params; p++) {
                int32_t value = g_callbacks.unit_get_param_value(p);
                // Just check that we can read parameters
                if (p == 0 && config->verbose) {
                    printf("    Parameter 0 value: %d\n", value);
                }
            }
        }
        
        printf("\n");
    }
    
    // Test switching between presets rapidly
    printf("Testing rapid preset switching...\n");
    for (int cycle = 0; cycle < 3; cycle++) {
        for (uint8_t i = 0; i < num_presets; i++) {
            g_callbacks.unit_load_preset(i);
            uint8_t actual = g_callbacks.unit_get_preset_index();
            if (actual != i) {
                printf("  ❌ FAIL at cycle %d, preset %u: got index %u\n", cycle, i, actual);
                all_passed = false;
            }
        }
    }
    printf("  ✅ Rapid switching test completed\n");
    printf("\n");
    
    // Test Reset() behavior with presets
    printf("Testing unit_reset() behavior with presets...\n");
    // Load preset 0
    if (num_presets > 0) {
        g_callbacks.unit_load_preset(0);
        uint8_t before_reset = g_callbacks.unit_get_preset_index();
        
        // Call reset (this is the critical test for the hardware bug!)
        if (g_callbacks.unit_reset) {
            g_callbacks.unit_reset();
        }
        
        // Check if preset index persists
        uint8_t after_reset = g_callbacks.unit_get_preset_index();
        if (after_reset != before_reset) {
            printf("  ⚠️  WARNING: Reset changed preset index from %u to %u\n", 
                   before_reset, after_reset);
        } else {
            printf("  ✅ Preset index preserved after reset: %u\n", after_reset);
        }
        
        // Verify parameters still reflect the loaded preset
        // (We can't know exact values, but we can check they're readable)
        if (g_callbacks.unit_get_param_value) {
            int32_t param0_after = g_callbacks.unit_get_param_value(0);
            printf("  Parameter 0 after reset: %d\n", param0_after);
        }
    }
    printf("\n");
    
    // Summary
    printf("═══════════════════════════════════════════════════════════════\n");
    if (all_passed) {
        printf("✅ All preset tests PASSED\n");
        printf("═══════════════════════════════════════════════════════════════\n");
        return UNIT_HOST_OK;
    } else {
        printf("❌ Some preset tests FAILED\n");
        printf("═══════════════════════════════════════════════════════════════\n");
        return UNIT_HOST_ERR_PROCESS;
    }
}

/**
 * @brief Set unit parameter
 */
int unit_host_set_param(unit_host_state_t* state, uint8_t param_id, int32_t value) {
    if (!state || param_id >= 24) {
        return UNIT_HOST_ERR_ARGS;
    }
    
    if (!state->unit_initialized) {
        return UNIT_HOST_ERR_INIT;
    }
    
    // Store parameter value
    state->param_values[param_id] = value;
    
    // Set in unit if callback exists
    if (g_callbacks.unit_set_param_value) {
        g_callbacks.unit_set_param_value(param_id, value);
    }
    
    return UNIT_HOST_OK;
}

/**
 * @brief Get unit parameter
 */
int32_t unit_host_get_param(unit_host_state_t* state, uint8_t param_id) {
    if (!state || param_id >= 24) {
        return 0;
    }
    
    // Try to get from unit first
    if (state->unit_initialized && g_callbacks.unit_get_param_value) {
        return g_callbacks.unit_get_param_value(param_id);
    }
    
    // Fallback to stored value
    return state->param_values[param_id];
}

/**
 * @brief Print CPU profiling statistics
 */
void unit_host_print_profiling_stats(unit_host_state_t* state, unit_host_config_t* config) {
    if (!state || !config || !config->profile) {
        return;
    }
    
    unit_profiling_stats_t* stats = &state->profile_stats;
    
    if (stats->render_count == 0) {
        printf("\n⚠️  No profiling data collected\n");
        return;
    }
    
    // Determine unit type
    uint8_t module_type = state->unit_header->target & 0xFF;
    bool is_synth = (module_type == k_unit_module_synth);
    const char* unit_type_str = is_synth ? "Synth" : "Effect";
    
    // Calculate statistics
    double avg_render_time = stats->total_render_time / stats->render_count;
    double avg_buffer_time = (double)config->buffer_size / config->sample_rate;
    
    // CPU usage: (time spent processing) / (real-time audio duration) * 100
    double cpu_usage_percent = (stats->total_render_time / stats->total_audio_time) * 100.0;
    
    // Real-time factor: how many times faster than real-time
    double realtime_factor = stats->total_audio_time / stats->total_render_time;
    
    printf("\n");
    printf("═══════════════════════════════════════════════════════════════\n");
    printf("                     CPU PROFILING REPORT                      \n");
    printf("═══════════════════════════════════════════════════════════════\n");
    printf("\n");
    printf("Unit Information:\n");
    printf("  Name:               %s\n", state->unit_header->name);
    printf("  Type:               %s\n", unit_type_str);
    if (is_synth) {
        printf("  Test method:        10 parameter variations over duration\n");
    } else {
        printf("  Test method:        Single input signal (for multiple inputs, use test-all-inputs.sh)\n");
    }
    printf("\n");
    printf("Audio Configuration:\n");
    printf("  Sample rate:        %u Hz\n", config->sample_rate);
    printf("  Buffer size:        %u frames (%.2f ms)\n", 
           config->buffer_size, avg_buffer_time * 1000.0);
    printf("  Total audio time:   %.3f seconds\n", stats->total_audio_time);
    printf("  Render calls:       %u\n", stats->render_count);
    printf("\n");
    printf("Timing Statistics:\n");
    printf("  Total render time:  %.6f seconds\n", stats->total_render_time);
    printf("  Average per buffer: %.6f seconds (%.3f ms)\n", 
           avg_render_time, avg_render_time * 1000.0);
    printf("  Minimum:            %.6f seconds (%.3f ms)\n", 
           stats->min_render_time, stats->min_render_time * 1000.0);
    printf("  Maximum:            %.6f seconds (%.3f ms)\n", 
           stats->max_render_time, stats->max_render_time * 1000.0);
    printf("\n");
    printf("Performance Metrics:\n");
    printf("  CPU usage:          %.2f%%", cpu_usage_percent);
    
    // Color code based on CPU usage
    if (cpu_usage_percent < 50.0) {
        printf(" ✅ Excellent\n");
    } else if (cpu_usage_percent < 80.0) {
        printf(" ⚠️  Good\n");
    } else if (cpu_usage_percent < 100.0) {
        printf(" ⚠️  Heavy\n");
    } else {
        printf(" ❌ OVERLOAD\n");
    }
    
    printf("  Real-time factor:   %.2fx %s\n", 
           realtime_factor,
           (realtime_factor >= 1.0) ? "✅" : "❌ UNDERRUN");
    
    // Per-buffer overhead
    double overhead_percent = (avg_render_time / avg_buffer_time) * 100.0;
    printf("  Buffer overhead:    %.2f%% of buffer time\n", overhead_percent);
    
    printf("\n");
    
    // Headroom analysis
    if (realtime_factor >= 1.0) {
        double headroom = (1.0 - (1.0 / realtime_factor)) * 100.0;
        printf("Headroom Analysis:\n");
        printf("  Available headroom: %.2f%%\n", headroom);
        
        if (headroom > 50.0) {
            printf("  Assessment:         ✅ Plenty of CPU headroom\n");
        } else if (headroom > 20.0) {
            printf("  Assessment:         ⚠️  Moderate headroom\n");
        } else {
            printf("  Assessment:         ⚠️  Tight! Close to real-time limit\n");
        }
    } else {
        printf("Performance Issues:\n");
        printf("  ❌ Cannot run in real-time!\n");
        printf("  Speed needed:       %.2fx faster\n", 1.0 / realtime_factor);
    }
    
    printf("\n");
    printf("═══════════════════════════════════════════════════════════════\n");
}

/**
 * @brief Print PERF_MON performance data from unit
 * Reads performance counters if unit was built with PERF_MON=1
 */
void unit_host_print_perf_mon(unit_host_state_t* state) {
    if (!state || !state->unit_handle) {
        fprintf(stderr, "Warning: No unit loaded for PERF_MON data\n");
        return;
    }
    
    // Try to load PERF_MON symbols from unit
    // These are C++ symbols from dsp::PerfMon class
    typedef uint8_t (*get_counter_count_func)(void);
    typedef const char* (*get_counter_name_func)(uint8_t);
    typedef uint32_t (*get_average_cycles_func)(uint8_t);
    typedef uint32_t (*get_peak_cycles_func)(uint8_t);
    typedef uint32_t (*get_min_cycles_func)(uint8_t);
    typedef uint32_t (*get_frame_count_func)(uint8_t);
    
    // Load function pointers (mangled C++ names for dsp::PerfMon static methods)
    get_counter_count_func get_count = (get_counter_count_func)dlsym(state->unit_handle, "_ZN3dsp7PerfMon15GetCounterCountEv");
    get_counter_name_func get_name = (get_counter_name_func)dlsym(state->unit_handle, "_ZN3dsp7PerfMon14GetCounterNameEh");
    get_average_cycles_func get_avg = (get_average_cycles_func)dlsym(state->unit_handle, "_ZN3dsp7PerfMon17GetAverageCyclesEh");
    get_peak_cycles_func get_peak = (get_peak_cycles_func)dlsym(state->unit_handle, "_ZN3dsp7PerfMon14GetPeakCyclesEh");
    get_min_cycles_func get_min = (get_min_cycles_func)dlsym(state->unit_handle, "_ZN3dsp7PerfMon13GetMinCyclesEh");
    get_frame_count_func get_frames = (get_frame_count_func)dlsym(state->unit_handle, "_ZN3dsp7PerfMon13GetFrameCountEh");
    
    // Check if PERF_MON symbols exist
    if (!get_count || !get_name || !get_avg || !get_peak || !get_min || !get_frames) {
        printf("\n");
        printf("⚠️  PERF_MON Data: Not available\n");
        printf("   (Unit must be built with: ./build.sh <unit> build PERF_MON=1)\n");
        return;
    }
    
    uint8_t counter_count = get_count();
    
    if (counter_count == 0) {
        printf("\n");
        printf("PERF_MON Data: No performance counters registered\n");
        return;
    }
    
    // Print header
    printf("\n");
    printf("═══════════════════════════════════════════════════════════════\n");
    printf("                    PERF_MON CYCLE COUNTS\n");
    printf("═══════════════════════════════════════════════════════════════\n");
    printf("\n");
    
    // Assume 48kHz sample rate and typical ARM Cortex-A7 clock (~900MHz)
    const double CPU_FREQ_MHZ = 900.0;
    const double SAMPLE_RATE = 48000.0;
    const double CYCLES_PER_SAMPLE = CPU_FREQ_MHZ * 1e6 / SAMPLE_RATE;  // ~18750 cycles/sample @ 48kHz
    
    printf("Counter metrics (assuming %0.f MHz CPU, %.0f Hz sample rate):\n\n", CPU_FREQ_MHZ, SAMPLE_RATE);
    printf("%-16s %12s %12s %12s %10s %12s\n", 
           "COUNTER", "AVG CYCLES", "MIN CYCLES", "MAX CYCLES", "FRAMES", "% OF BUDGET");
    printf("%-16s %12s %12s %12s %10s %12s\n", 
           "────────────────", "──────────", "──────────", "──────────", "────────", "───────────");
    
    uint32_t total_avg_cycles = 0;
    
    for (uint8_t i = 0; i < counter_count; i++) {
        const char* name = get_name(i);
        uint32_t avg_cycles = get_avg(i);
        uint32_t min_cycles = get_min(i);
        uint32_t max_cycles = get_peak(i);
        uint32_t frames = get_frames(i);
        
        if (frames == 0) continue;  // Skip unused counters
        
        double percent_of_budget = (avg_cycles / CYCLES_PER_SAMPLE) * 100.0;
        
        printf("%-16s %12u %12u %12u %10u %11.2f%%\n",
               name ? name : "<unnamed>",
               avg_cycles,
               min_cycles,
               max_cycles,
               frames,
               percent_of_budget);
        
        total_avg_cycles += avg_cycles;
    }
    
    printf("%-16s %12s %12s %12s %10s %12s\n", 
           "────────────────", "──────────", "──────────", "──────────", "────────", "───────────");
    
    double total_percent = (total_avg_cycles / CYCLES_PER_SAMPLE) * 100.0;
    printf("%-16s %12u %12s %12s %10s %11.2f%%", 
           "TOTAL", total_avg_cycles, "─", "─", "─", total_percent);
    
    // Assessment
    if (total_percent < 50.0) {
        printf(" ✅ Excellent\n");
    } else if (total_percent < 80.0) {
        printf(" ⚠️  Good\n");
    } else if (total_percent < 100.0) {
        printf(" ⚠️  Heavy\n");
    } else {
        printf(" ❌ OVERLOAD\n");
    }
    
    printf("\n");
    printf("Notes:\n");
    printf("  • Cycle counts are measured using ARM cycle counters\n");
    printf("  • %% of budget assumes %.0f cycles available per sample\n", CYCLES_PER_SAMPLE);
    printf("  • Actual performance depends on CPU frequency and load\n");
    printf("\n");
    printf("═══════════════════════════════════════════════════════════════\n");
}

/**
 * @brief Print unit information
 */
void unit_host_print_info(unit_host_state_t* state) {
    if (!state || !state->unit_header) {
        printf("No unit loaded\n");
        return;
    }
    
    unit_header_t* header = state->unit_header;
    
    printf("Unit Information:\n");
    printf("  Name: %s\n", header->name);
    printf("  Developer ID: 0x%08X\n", header->dev_id);
    printf("  Unit ID: 0x%08X\n", header->unit_id);
    printf("  Version: 0x%08X (v%u.%u.%u)\n", 
           header->version,
           (header->version >> 16) & 0xFF,
           (header->version >> 8) & 0xFF,
           header->version & 0xFF);
    printf("  Target: 0x%08X\n", header->target);
    printf("  API: 0x%08X\n", header->api);
    printf("  Parameters: %u\n", header->num_params);
    printf("  Presets: %u\n", header->num_presets);
    
    // List preset names if available
    if (header->num_presets > 0 && g_callbacks.unit_get_preset_name) {
        printf("\n  Available presets:\n");
        for (uint8_t i = 0; i < header->num_presets; i++) {
            const char* name = g_callbacks.unit_get_preset_name(i);
            if (name && name[0] != '\0') {
                printf("    [%u] %s\n", i, name);
            }
        }
    }
    
    // Print parameters
    if (header->num_params > 0) {
        printf("  Parameter list:\n");
        for (uint32_t i = 0; i < header->num_params && i < 24; i++) {
            unit_param_t* param = &header->params[i];
            if (param->type != k_unit_param_type_none) {
                printf("    [%2u] %s (min:%d, max:%d, center:%d, init:%d)\n",
                       i, param->name, param->min, param->max, 
                       param->center, param->init);
            }
        }
    }
}

/**
 * @brief Parse command line arguments
 */
int unit_host_parse_args(int argc, char* argv[], unit_host_config_t* config) {
    if (!config) {
        return UNIT_HOST_ERR_ARGS;
    }
    
    // Set defaults
    memset(config, 0, sizeof(unit_host_config_t));
    config->sample_rate = 48000;
    config->buffer_size = 256;
    config->channels = 2;  // Default to stereo
    config->verbose = false;
    config->profile = false;
    config->test_presets = false;
    
    // Parse command line
    if (argc < 4) {
        fprintf(stderr, "Usage: %s <unit.drmlgunit> <input.wav> <output.wav> [options]\n", 
                argv[0]);
        fprintf(stderr, "Options:\n");
        fprintf(stderr, "  --param-<id> <value>    Set parameter (0-23)\n");
        fprintf(stderr, "  --sample-rate <rate>    Sample rate (default: 48000)\n");
        fprintf(stderr, "  --buffer-size <frames>  Buffer size (default: 256)\n");
        fprintf(stderr, "  --channels <1|2>        Output channels (default: 2)\n");
        fprintf(stderr, "  --test-presets          Test preset loading/switching\n");
        fprintf(stderr, "  --profile               Enable CPU profiling\n");
        fprintf(stderr, "  --verbose               Verbose output\n");
        return UNIT_HOST_ERR_ARGS;
    }
    
    config->unit_file = argv[1];
    config->input_wav = argv[2];
    config->output_wav = argv[3];
    
    // Parse optional arguments
    for (int i = 4; i < argc; i++) {
        if (strncmp(argv[i], "--param-", 8) == 0) {
            // Parameter setting: --param-0 50
            int param_id = atoi(argv[i] + 8);
            if (i + 1 < argc && param_id >= 0 && param_id < 24) {
                int param_value = atoi(argv[i + 1]);
                printf("Parameter %d = %d\n", param_id, param_value);
                i++;  // Skip value argument
            }
        } else if (strcmp(argv[i], "--sample-rate") == 0) {
            if (i + 1 < argc) {
                config->sample_rate = atoi(argv[i + 1]);
                i++;
            }
        } else if (strcmp(argv[i], "--buffer-size") == 0) {
            if (i + 1 < argc) {
                config->buffer_size = atoi(argv[i + 1]);
                i++;
            }
        } else if (strcmp(argv[i], "--channels") == 0) {
            if (i + 1 < argc) {
                config->channels = atoi(argv[i + 1]);
                if (config->channels > 2) config->channels = 2;
                i++;
            }
        } else if (strcmp(argv[i], "--test-presets") == 0) {
            config->test_presets = true;
        } else if (strcmp(argv[i], "--profile") == 0) {
            config->profile = true;
        } else if (strcmp(argv[i], "--perf-mon") == 0) {
            config->perf_mon = true;
        } else if (strcmp(argv[i], "--verbose") == 0) {
            config->verbose = true;
        }
    }
    
    return UNIT_HOST_OK;
}

/**
 * @brief Cleanup and unload unit
 */
void unit_host_cleanup(unit_host_state_t* state) {
    if (!state) {
        return;
    }
    
    // Suspend and teardown unit
    if (state->unit_initialized) {
        if (g_callbacks.unit_suspend) {
            g_callbacks.unit_suspend();
        }
        if (g_callbacks.unit_teardown) {
            g_callbacks.unit_teardown();
        }
        state->unit_initialized = false;
    }
    
    // Close shared library
    if (state->unit_handle) {
        dlclose(state->unit_handle);
        state->unit_handle = NULL;
    }
    
    // Cleanup SDK stubs
    sdk_stubs_cleanup();
    
    memset(state, 0, sizeof(unit_host_state_t));
}

/**
 * @brief Main unit host entry point
 */
int unit_host_main(int argc, char* argv[]) {
    unit_host_config_t config;
    int result;
    
    // Parse arguments
    result = unit_host_parse_args(argc, argv, &config);
    if (result != UNIT_HOST_OK) {
        return result;
    }
    
    // Initialize unit host
    result = unit_host_init(&config, &g_state);
    if (result != UNIT_HOST_OK) {
        fprintf(stderr, "Failed to initialize unit host\n");
        return result;
    }
    
    // Load unit
    result = unit_host_load_unit(config.unit_file, &g_state);
    if (result != UNIT_HOST_OK) {
        unit_host_cleanup(&g_state);
        return result;
    }
    
    // Print unit information
    if (config.verbose) {
        unit_host_print_info(&g_state);
    }
    
    // Initialize unit
    result = unit_host_init_unit(&g_state);
    if (result != UNIT_HOST_OK) {
        unit_host_cleanup(&g_state);
        return result;
    }
    
    // Set parameters (parse again to actually set them now that unit is loaded)
    for (int i = 4; i < argc; i++) {
        if (strncmp(argv[i], "--param-", 8) == 0) {
            int param_id = atoi(argv[i] + 8);
            if (i + 1 < argc && param_id >= 0 && param_id < 24) {
                int param_value = atoi(argv[i + 1]);
                unit_host_set_param(&g_state, param_id, param_value);
                if (config.verbose) {
                    printf("Set parameter %d = %d\n", param_id, param_value);
                }
                i++;
            }
        }
    }
    
    // Test presets if requested
    if (config.test_presets) {
        result = unit_host_test_presets(&g_state, &config);
        if (result != UNIT_HOST_OK) {
            fprintf(stderr, "Preset test failed\n");
            unit_host_cleanup(&g_state);
            return result;
        }
    }
    
    // Process WAV file
    result = unit_host_process_wav(config.input_wav, config.output_wav, &g_state, &config);
    if (result != UNIT_HOST_OK) {
        fprintf(stderr, "Failed to process WAV file\n");
        unit_host_cleanup(&g_state);
        return result;
    }
    
    // Print profiling statistics if enabled
    if (config.profile) {
        unit_host_print_profiling_stats(&g_state, &config);
    }
    
    // Print PERF_MON data if enabled and unit has performance symbols
    if (config.perf_mon) {
        unit_host_print_perf_mon(&g_state);
    }
    
    // Cleanup
    unit_host_cleanup(&g_state);
    
    printf("✓ Unit test completed successfully\n");
    return UNIT_HOST_OK;
}

// Main entry point
int main(int argc, char* argv[]) {
    return unit_host_main(argc, argv);
}