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
} unit_runtime_desc_t;

// Parameter types (for header.c compatibility)
enum {
    k_unit_param_type_none = 0,
    k_unit_param_type_percent = 1,
    k_unit_param_type_db = 2,
    k_unit_param_type_cents = 3,
    k_unit_param_type_semi = 4,
    k_unit_param_type_oct = 5,
    k_unit_param_type_hertz = 6,
    k_unit_param_type_khertz = 7,
    k_unit_param_type_bpm = 8,
    k_unit_param_type_msec = 9,
    k_unit_param_type_sec = 10,
    k_unit_param_type_strings = 11,
    k_unit_param_type_bitmaps = 12,
};

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
} unit_header_t;

extern const unit_header_t unit_header;

// SDK attributes
#define __unit_header __attribute__((used, section(".unit_header")))
#define __unit_callback

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
void unit_gate_off();
void unit_all_note_off();
void unit_pitch_bend(uint16_t);
void unit_channel_pressure(uint8_t);
void unit_aftertouch(uint8_t, uint8_t);

#ifdef __cplusplus
}
#endif

#endif  // UNIT_H_