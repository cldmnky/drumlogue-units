#include "preset_manager.h"

#include <dirent.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

// Simple JSON writer/reader (no dependencies)
// Format: {"name":"...", "unit":"...", "dev_id":..., "unit_id":..., "params":[...]}

#define MAX_PRESETS 256

struct preset_manager {
    char presets_dir[256];
    preset_t presets[MAX_PRESETS];
    int count;
};

preset_manager_t* preset_manager_create(const char* presets_dir) {
    if (!presets_dir) return NULL;
    
    preset_manager_t* mgr = calloc(1, sizeof(preset_manager_t));
    if (!mgr) return NULL;
    
    strncpy(mgr->presets_dir, presets_dir, sizeof(mgr->presets_dir) - 1);
    
    // Create directory if it doesn't exist
    mkdir(presets_dir, 0755);
    
    return mgr;
}

void preset_manager_destroy(preset_manager_t* mgr) {
    free(mgr);
}

int preset_manager_scan(preset_manager_t* mgr) {
    if (!mgr) return -1;
    
    mgr->count = 0;
    
    DIR* dir = opendir(mgr->presets_dir);
    if (!dir) return -1;
    
    struct dirent* entry;
    while ((entry = readdir(dir)) != NULL && mgr->count < MAX_PRESETS) {
        // Look for .json files
        size_t len = strlen(entry->d_name);
        if (len < 6 || strcmp(entry->d_name + len - 5, ".json") != 0) {
            continue;
        }
        
        char path[512];
        snprintf(path, sizeof(path), "%s/%s", mgr->presets_dir, entry->d_name);
        
        preset_t preset;
        if (preset_manager_load(mgr, entry->d_name, &preset) == 0) {
            mgr->presets[mgr->count++] = preset;
        }
    }
    
    closedir(dir);
    return mgr->count;
}

int preset_manager_count(preset_manager_t* mgr) {
    return mgr ? mgr->count : 0;
}

const preset_t* preset_manager_get(preset_manager_t* mgr, int index) {
    if (!mgr || index < 0 || index >= mgr->count) return NULL;
    return &mgr->presets[index];
}

int preset_manager_save(preset_manager_t* mgr, const preset_t* preset) {
    if (!mgr || !preset) return -1;
    
    char path[512];
    snprintf(path, sizeof(path), "%s/%s.json", mgr->presets_dir, preset->name);
    
    FILE* f = fopen(path, "w");
    if (!f) return -1;
    
    fprintf(f, "{\n");
    fprintf(f, "  \"name\": \"%s\",\n", preset->name);
    fprintf(f, "  \"unit\": \"%s\",\n", preset->unit_name);
    fprintf(f, "  \"dev_id\": %u,\n", preset->dev_id);
    fprintf(f, "  \"unit_id\": %u,\n", preset->unit_id);
    fprintf(f, "  \"num_params\": %u,\n", preset->num_params);
    fprintf(f, "  \"params\": [");
    
    for (uint8_t i = 0; i < preset->num_params; ++i) {
        if (i > 0) fprintf(f, ", ");
        fprintf(f, "%d", preset->param_values[i]);
    }
    
    fprintf(f, "]\n}\n");
    fclose(f);
    
    return 0;
}

int preset_manager_load(preset_manager_t* mgr, const char* name, preset_t* preset) {
    if (!mgr || !name || !preset) return -1;
    
    char path[512];
    // If name already has .json, use as-is; otherwise append
    if (strstr(name, ".json")) {
        snprintf(path, sizeof(path), "%s/%s", mgr->presets_dir, name);
    } else {
        snprintf(path, sizeof(path), "%s/%s.json", mgr->presets_dir, name);
    }
    
    FILE* f = fopen(path, "r");
    if (!f) return -1;
    
    memset(preset, 0, sizeof(*preset));
    
    // Simple JSON parser (line-based, fragile but sufficient)
    char line[512];
    while (fgets(line, sizeof(line), f)) {
        if (sscanf(line, "  \"name\": \"%63[^\"]\"", preset->name) == 1) continue;
        if (sscanf(line, "  \"unit\": \"%15[^\"]\"", preset->unit_name) == 1) continue;
        if (sscanf(line, "  \"dev_id\": %u", &preset->dev_id) == 1) continue;
        if (sscanf(line, "  \"unit_id\": %u", &preset->unit_id) == 1) continue;
        if (sscanf(line, "  \"num_params\": %hhu", &preset->num_params) == 1) continue;
        
        // Parse params array
        if (strstr(line, "\"params\":")) {
            char* p = strchr(line, '[');
            if (p) {
                p++;
                for (uint8_t i = 0; i < PRESET_MAX_PARAMS && *p != ']'; ++i) {
                    preset->param_values[i] = atoi(p);
                    p = strchr(p, ',');
                    if (p) p++;
                    else break;
                }
            }
        }
    }
    
    fclose(f);
    return 0;
}

int preset_manager_delete(preset_manager_t* mgr, const char* name) {
    if (!mgr || !name) return -1;
    
    char path[512];
    if (strstr(name, ".json")) {
        snprintf(path, sizeof(path), "%s/%s", mgr->presets_dir, name);
    } else {
        snprintf(path, sizeof(path), "%s/%s.json", mgr->presets_dir, name);
    }
    
    return remove(path);
}
