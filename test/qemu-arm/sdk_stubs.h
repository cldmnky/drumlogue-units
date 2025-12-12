/**
 * @file sdk_stubs.h
 * @brief Logue SDK runtime stubs for QEMU ARM testing
 * 
 * Provides minimal implementations of the logue SDK runtime functions
 * needed by .drmlgunit files when running in the QEMU test environment.
 */

#ifndef SDK_STUBS_H_
#define SDK_STUBS_H_

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

// SDK API version (from logue-sdk)
// k_unit_api_2_0_0 = ((2U << 16) | (0U << 8) | (0U)) = 0x00020000
#define UNIT_API_VERSION 0x00020000U

// Drumlogue platform target (4U << 8 = 0x0400)
#define UNIT_TARGET_PLATFORM (0x0400U)  // k_unit_target_drumlogue

// Unit module types (from logue SDK runtime.h - these are ENUMS, not bit flags!)
enum {
    k_unit_module_global = 0,
    k_unit_module_modfx,     // 1
    k_unit_module_delfx,     // 2
    k_unit_module_revfx,     // 3
    k_unit_module_osc,       // 4
    k_unit_module_synth,     // 5
    k_unit_module_masterfx,  // 6
};

// Parameter types
enum {
    k_unit_param_type_none = 0,
    k_unit_param_type_percent,
    k_unit_param_type_db,
    k_unit_param_type_cents,
    k_unit_param_type_semi,
    k_unit_param_type_oct,
    k_unit_param_type_hertz,
    k_unit_param_type_khertz,
    k_unit_param_type_bpm,
    k_unit_param_type_time,
    k_unit_param_type_strings,
    k_unit_param_type_bitmaps,
    k_unit_param_type_fixed,
    k_unit_param_type_count
};

// Error codes
enum {
    k_unit_err_none = 0,
    k_unit_err_target = -1,
    k_unit_err_api_version = -2,
    k_unit_err_samplerate = -4,
    k_unit_err_geometry = -8,
    k_unit_err_memory = -16,
    k_unit_err_undef = -32
};

// Sample wrapper for sample access API
typedef struct {
    uint32_t frames;
    uint8_t channels;
    const float* sample_ptr;
} sample_wrapper_t;

// Runtime function pointers for sample access
typedef uint8_t (*unit_runtime_get_num_sample_banks_ptr)(void);
typedef uint8_t (*unit_runtime_get_num_samples_for_bank_ptr)(uint8_t);
typedef const sample_wrapper_t* (*unit_runtime_get_sample_ptr)(uint8_t, uint8_t);

// Runtime descriptor (matches logue SDK)
#pragma pack(push, 1)
typedef struct unit_runtime_desc {
    uint16_t target;
    uint32_t api;
    uint32_t samplerate;
    uint16_t frames_per_buffer;
    uint8_t input_channels;
    uint8_t output_channels;
    unit_runtime_get_num_sample_banks_ptr get_num_sample_banks;
    unit_runtime_get_num_samples_for_bank_ptr get_num_samples_for_bank;
    unit_runtime_get_sample_ptr get_sample;
} unit_runtime_desc_t;
#pragma pack(pop)

// Parameter descriptor (matches logue SDK - must use bitfields and pack(1))
#pragma pack(push, 1)
typedef struct unit_param {
    int16_t min;
    int16_t max;
    int16_t center;
    int16_t init;
    uint8_t type;
    uint8_t frac : 4;       // fractional bits / decimals according to frac_mode
    uint8_t frac_mode : 1;  // 0: fixed point, 1: decimal
    uint8_t reserved : 3;
    char name[14];          // UNIT_PARAM_NAME_LEN (13) + 1 for null = 14
} unit_param_t;  // 24 bytes
#pragma pack(pop)

// Unit header (matches logue SDK - must match EXACTLY including packing and types)
#pragma pack(push, 1)
typedef struct unit_header {
    uint32_t header_size;
    uint16_t target;      // NOTE: uint16_t, not uint32_t!
    uint32_t api;
    uint32_t dev_id;
    uint32_t unit_id;
    uint32_t version;
    char name[14];        // UNIT_NAME_LEN is 13, +1 for null terminator = 14
    uint32_t num_presets;
    uint32_t num_params;
    unit_param_t params[24];
} unit_header_t;
#pragma pack(pop)

// SDK attributes
#define __unit_header __attribute__((used, section(".unit_header")))
#define __unit_callback

// API compatibility check
#define UNIT_API_IS_COMPAT(api) (((api) & 0xFF000000U) == (UNIT_API_VERSION & 0xFF000000U))

/**
 * @brief Initialize SDK stubs
 */
void sdk_stubs_init(void);

/**
 * @brief Create runtime descriptor for unit testing
 */
unit_runtime_desc_t* sdk_stubs_create_runtime_desc(uint32_t sample_rate, 
                                                   uint16_t buffer_size,
                                                   uint8_t channels);

/**
 * @brief Update runtime descriptor target from loaded unit header
 */
void sdk_stubs_set_target(uint16_t target);

/**
 * @brief Clean up SDK stubs
 */
void sdk_stubs_cleanup(void);

// Sample access stubs (return empty/dummy data for units that need samples)
uint8_t stub_get_num_sample_banks(void);
uint8_t stub_get_num_samples_for_bank(uint8_t bank);
const sample_wrapper_t* stub_get_sample(uint8_t bank, uint8_t sample);

#ifdef __cplusplus
}
#endif

#endif  // SDK_STUBS_H_