#pragma once

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

#define PRESET_MAX_NAME_LEN 64
#define PRESET_MAX_PARAMS 24

typedef struct {
    char name[PRESET_MAX_NAME_LEN];
    char unit_name[16];
    uint32_t dev_id;
    uint32_t unit_id;
    uint8_t num_params;
    int32_t param_values[PRESET_MAX_PARAMS];
} preset_t;

typedef struct preset_manager preset_manager_t;

// Create preset manager for a specific presets directory
preset_manager_t* preset_manager_create(const char* presets_dir);
void preset_manager_destroy(preset_manager_t* mgr);

// Scan directory and build preset list
int preset_manager_scan(preset_manager_t* mgr);

// Get number of presets
int preset_manager_count(preset_manager_t* mgr);

// Get preset by index (for iteration)
const preset_t* preset_manager_get(preset_manager_t* mgr, int index);

// Save preset to file
int preset_manager_save(preset_manager_t* mgr, const preset_t* preset);

// Load preset from file by name
int preset_manager_load(preset_manager_t* mgr, const char* name, preset_t* preset);

// Delete preset file
int preset_manager_delete(preset_manager_t* mgr, const char* name);

#ifdef __cplusplus
}
#endif
