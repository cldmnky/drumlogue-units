#pragma once

#include <stdint.h>

#include "../sdk/runtime_stubs.h"
#include "../../../logue-sdk/platform/drumlogue/common/runtime.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct unit_loader {
    void* handle;
    unit_header_t* header;

    unit_init_func unit_init;
    unit_render_func unit_render;
    unit_set_param_value_func unit_set_param_value;
    unit_get_param_value_func unit_get_param_value;
    unit_get_param_str_value_func unit_get_param_str_value;
    unit_get_param_bmp_value_func unit_get_param_bmp_value;
    unit_load_preset_func unit_load_preset;
    unit_get_preset_name_func unit_get_preset_name;
    
    // Note callbacks (for synth units)
    unit_note_on_func unit_note_on;
    unit_note_off_func unit_note_off;
    unit_all_note_off_func unit_all_note_off;
} unit_loader_t;

// Load shared library and resolve required symbols
int unit_loader_open(const char* path, unit_loader_t* loader);

// Initialize unit with provided runtime descriptor
int unit_loader_init(unit_loader_t* loader, const unit_runtime_desc_t* runtime);

// Render audio (frames = frames_per_buffer)
void unit_loader_render(unit_loader_t* loader,
                        const float* input,
                        float* output,
                        uint32_t frames);

// Set a parameter value
void unit_loader_set_param(unit_loader_t* loader, uint8_t param_id, int32_t value);

// Unload shared library
void unit_loader_close(unit_loader_t* loader);

#ifdef __cplusplus
}  // extern "C"
#endif
