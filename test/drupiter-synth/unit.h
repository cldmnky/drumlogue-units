/**
 * @file unit.h
 * @brief Minimal unit.h for desktop testing (mocks drumlogue SDK)
 */

#ifndef UNIT_H_
#define UNIT_H_

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

// Error codes
#define k_unit_err_none 0
#define k_unit_err_undef -1
#define k_unit_err_target -2
#define k_unit_err_api_version -3

// API version macros
#define UNIT_API_INIT(major, minor) ((major << 16) | minor)
#define UNIT_API_IS_COMPAT(api) ((api & 0xFFFF0000) == (0 << 16))  // Always compatible for testing

// Unit runtime descriptor
typedef struct {
    uint16_t target;
    uint32_t api;
    uint32_t samplerate;
    uint16_t frames_per_buffer;
    uint8_t input_channels;
    uint8_t output_channels;
    // ... other fields not needed for testing
} unit_runtime_desc_t;

// Unit header (mock)
typedef struct {
    uint32_t header_size;
    uint16_t target;
    uint32_t api;
    uint32_t dev_id;
    uint32_t unit_id;
    uint32_t version;
    char name[16];  // UNIT_NAME_LEN + 1
    uint32_t num_presets;
    uint32_t num_params;
    // unit_param_t params[24];  // Not needed for testing
} unit_header_t;

extern const unit_header_t unit_header;

// Function declarations (stubs for testing)
int8_t unit_init(const unit_runtime_desc_t *);
void unit_teardown();
void unit_reset();
void unit_resume();
void unit_suspend();
void unit_render(const float *, float *, uint32_t);
uint8_t unit_get_preset_index();
const char * unit_get_preset_name(uint8_t);
void unit_load_preset(uint8_t);
int32_t unit_get_param_value(uint8_t);
const char * unit_get_param_str_value(uint8_t, int32_t);
const uint8_t * unit_get_param_bmp_value(uint8_t, int32_t);
void unit_set_param_value(uint8_t, int32_t);
void unit_set_tempo(uint32_t);
void unit_note_on(uint8_t, uint8_t);
void unit_note_off(uint8_t);
void unit_gate_on(uint8_t);
void unit_gate_off(void);
void unit_all_note_off(void);
void unit_platform_exclusive(uint8_t, void *, uint32_t dataSize);

#ifdef __cplusplus
}  // extern "C"
#endif

#endif  // UNIT_H_