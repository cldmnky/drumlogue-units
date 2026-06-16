#include "unit_loader.h"

#include <dlfcn.h>
#include <stdio.h>
#include <string.h>

static void clear_loader(unit_loader_t* loader) {
    if (!loader) return;
    memset(loader, 0, sizeof(*loader));
}

int unit_loader_open(const char* path, unit_loader_t* loader) {
    if (!path || !loader) {
        return -1;
    }

    clear_loader(loader);

    loader->handle = dlopen(path, RTLD_NOW);
    if (!loader->handle) {
        fprintf(stderr, "dlopen failed: %s\n", dlerror());
        return -1;
    }

    // Required header and callbacks
    loader->header = (unit_header_t*)dlsym(loader->handle, "unit_header");
    loader->unit_init = (unit_init_func)dlsym(loader->handle, "unit_init");
    loader->unit_render = (unit_render_func)dlsym(loader->handle, "unit_render");
    loader->unit_set_param_value = (unit_set_param_value_func)dlsym(loader->handle, "unit_set_param_value");

    if (!loader->header || !loader->unit_init || !loader->unit_render) {
        fprintf(stderr, "Missing required symbols in %s\n", path);
        unit_loader_close(loader);
        return -1;
    }

    // Optional helpers
    loader->unit_get_param_value = (unit_get_param_value_func)dlsym(loader->handle, "unit_get_param_value");
    loader->unit_get_param_str_value = (unit_get_param_str_value_func)dlsym(loader->handle, "unit_get_param_str_value");
    loader->unit_get_param_bmp_value = (unit_get_param_bmp_value_func)dlsym(loader->handle, "unit_get_param_bmp_value");
    loader->unit_load_preset = (unit_load_preset_func)dlsym(loader->handle, "unit_load_preset");
    loader->unit_get_preset_name = (unit_get_preset_name_func)dlsym(loader->handle, "unit_get_preset_name");
    
    // Note callbacks (for synth units)
    loader->unit_note_on = (unit_note_on_func)dlsym(loader->handle, "unit_note_on");
    loader->unit_note_off = (unit_note_off_func)dlsym(loader->handle, "unit_note_off");
    loader->unit_all_note_off = (unit_all_note_off_func)dlsym(loader->handle, "unit_all_note_off");

    // Gate / drum trigger callbacks
    loader->unit_gate_on = (unit_gate_on_func)dlsym(loader->handle, "unit_gate_on");
    loader->unit_gate_off = (unit_gate_off_func)dlsym(loader->handle, "unit_gate_off");

    // Performance callbacks
    loader->unit_pitch_bend = (unit_pitch_bend_func)dlsym(loader->handle, "unit_pitch_bend");
    loader->unit_channel_pressure = (unit_channel_pressure_func)dlsym(loader->handle, "unit_channel_pressure");
    loader->unit_aftertouch = (unit_aftertouch_func)dlsym(loader->handle, "unit_aftertouch");

    // Lifecycle
    loader->unit_set_tempo = (unit_set_tempo_func)dlsym(loader->handle, "unit_set_tempo");
    loader->unit_teardown = (unit_teardown_func)dlsym(loader->handle, "unit_teardown");

    return 0;
}

int unit_loader_init(unit_loader_t* loader, const unit_runtime_desc_t* runtime) {
    if (!loader || !runtime || !loader->unit_init) {
        return -1;
    }
    return loader->unit_init(runtime);
}

void unit_loader_render(unit_loader_t* loader,
                        const float* input,
                        float* output,
                        uint32_t frames) {
    if (!loader || !loader->unit_render) {
        return;
    }
    loader->unit_render(input, output, frames);
}

void unit_loader_set_param(unit_loader_t* loader, uint8_t param_id, int32_t value) {
    if (!loader || !loader->unit_set_param_value) {
        return;
    }
    loader->unit_set_param_value(param_id, value);
}

void unit_loader_set_tempo(unit_loader_t* loader, uint32_t tempo_fixed) {
    if (!loader || !loader->unit_set_tempo) {
        return;
    }
    loader->unit_set_tempo(tempo_fixed);
}

void unit_loader_close(unit_loader_t* loader) {
    if (!loader) {
        return;
    }

    if (loader->handle) {
        dlclose(loader->handle);
    }

    clear_loader(loader);
}
