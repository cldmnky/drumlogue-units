/**
 * @file unit_host.h
 * @brief QEMU ARM unit host header
 * 
 * Minimal unit host for loading and testing .drmlgunit files in QEMU ARM environment.
 * Emulates the drumlogue's ARM Cortex-A7 runtime for unit testing.
 */

#ifndef UNIT_HOST_H_
#define UNIT_HOST_H_

#include <stdint.h>
#include <stdbool.h>
#include <sys/types.h>

#ifdef __cplusplus
extern "C" {
#endif

// Forward declarations
typedef struct unit_runtime_desc unit_runtime_desc_t;
typedef struct unit_header unit_header_t;

/**
 * @brief CPU profiling statistics
 */
typedef struct {
    double total_render_time;   // Total time spent in unit_render (seconds)
    double min_render_time;     // Minimum render time per buffer (seconds)
    double max_render_time;     // Maximum render time per buffer (seconds)
    uint32_t render_count;      // Number of render calls
    double total_audio_time;    // Total audio time processed (seconds)
} unit_profiling_stats_t;

/**
 * @brief Unit host configuration
 */
typedef struct {
    const char* unit_file;      // Path to .drmlgunit file
    const char* input_wav;      // Input WAV file
    const char* output_wav;     // Output WAV file
    uint32_t sample_rate;       // Sample rate (default: 48000)
    uint32_t buffer_size;       // Buffer size in frames (default: 256)
    uint8_t channels;           // Number of channels (1=mono, 2=stereo)
    bool verbose;               // Verbose logging
    bool profile;               // Enable CPU profiling
} unit_host_config_t;

/**
 * @brief Unit host runtime state
 */
typedef struct {
    void* unit_handle;          // dlopen handle to .drmlgunit
    unit_header_t* unit_header; // Unit header from loaded library
    unit_runtime_desc_t* runtime_desc; // Runtime descriptor for unit
    uint32_t param_values[24];  // Current parameter values
    bool unit_initialized;      // Unit initialization state
    unit_profiling_stats_t profile_stats; // CPU profiling statistics
} unit_host_state_t;

/**
 * @brief Unit callback function pointers
 * These match the logue SDK unit API
 */
typedef struct {
    int8_t (*unit_init)(const unit_runtime_desc_t*);
    void (*unit_teardown)(void);
    void (*unit_reset)(void);
    void (*unit_resume)(void);
    void (*unit_suspend)(void);
    void (*unit_render)(const float*, float*, uint32_t);
    void (*unit_set_param_value)(uint8_t, int32_t);
    int32_t (*unit_get_param_value)(uint8_t);
    const char* (*unit_get_param_str_value)(uint8_t, int32_t);
    const uint8_t* (*unit_get_param_bmp_value)(uint8_t, int32_t);
    void (*unit_set_tempo)(uint32_t);
    void (*unit_note_on)(uint8_t, uint8_t);
    void (*unit_note_off)(uint8_t);
    void (*unit_gate_on)(uint8_t);
    void (*unit_gate_off)(void);
    void (*unit_all_note_off)(void);
    void (*unit_pitch_bend)(uint16_t);
    void (*unit_channel_pressure)(uint8_t);
    void (*unit_aftertouch)(uint8_t, uint8_t);
    void (*unit_load_preset)(uint8_t);
    uint8_t (*unit_get_preset_index)(void);
    const char* (*unit_get_preset_name)(uint8_t);
} unit_callbacks_t;

/**
 * @brief Initialize unit host
 */
int unit_host_init(unit_host_config_t* config, unit_host_state_t* state);

/**
 * @brief Load .drmlgunit file
 */
int unit_host_load_unit(const char* unit_path, unit_host_state_t* state);

/**
 * @brief Initialize loaded unit
 */
int unit_host_init_unit(unit_host_state_t* state);

/**
 * @brief Process WAV file through unit
 */
int unit_host_process_wav(const char* input_path, const char* output_path,
                         unit_host_state_t* state, unit_host_config_t* config);

/**
 * @brief Set unit parameter
 */
int unit_host_set_param(unit_host_state_t* state, uint8_t param_id, int32_t value);

/**
 * @brief Get unit parameter
 */
int32_t unit_host_get_param(unit_host_state_t* state, uint8_t param_id);

/**
 * @brief Cleanup and unload unit
 */
void unit_host_cleanup(unit_host_state_t* state);

/**
 * @brief Print unit information
 */
void unit_host_print_info(unit_host_state_t* state);

/**
 * @brief Print CPU profiling statistics
 */
void unit_host_print_profiling_stats(unit_host_state_t* state, unit_host_config_t* config);

/**
 * @brief Parse command line arguments
 */
int unit_host_parse_args(int argc, char* argv[], unit_host_config_t* config);

/**
 * @brief Main unit host entry point
 */
int unit_host_main(int argc, char* argv[]);

// Error codes
#define UNIT_HOST_OK            0
#define UNIT_HOST_ERR_ARGS      -1
#define UNIT_HOST_ERR_FILE      -2
#define UNIT_HOST_ERR_LOAD      -3
#define UNIT_HOST_ERR_INIT      -4
#define UNIT_HOST_ERR_PROCESS   -5
#define UNIT_HOST_ERR_SYMBOL    -6

#ifdef __cplusplus
}
#endif

#endif  // UNIT_HOST_H_