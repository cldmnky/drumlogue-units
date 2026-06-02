#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <math.h>

#include "../core/unit_loader.h"
#include "../sdk/runtime_stubs.h"

#if defined(__APPLE__)
#define UNIT_LIB_EXT ".dylib"
#else
#define UNIT_LIB_EXT ".so"
#endif

#define FRAMES 128
#define CH 2
#define SAMPLE_RATE 48000

static float peak(const float *out, int frames, int ch) {
    float m = 0;
    for (int j = 0; j < frames * ch; j++) {
        float a = out[j] >= 0 ? out[j] : -out[j];
        if (a > m) m = a;
    }
    return m;
}

static int count_clipping(const float *out, int frames, int ch) {
    int c = 0;
    for (int j = 0; j < frames * ch; j++) {
        if (out[j] >= 1.0f || out[j] <= -1.0f) c++;
    }
    return c;
}

static void render_n(unit_loader_t *loader, float *out, int n) {
    for (int i = 0; i < n; i++) {
        memset(out, 0, FRAMES * CH * sizeof(float));
        unit_loader_render(loader, NULL, out, FRAMES);
    }
}

int main(int argc, char **argv) {
    if (argc < 2) {
        fprintf(stderr, "Usage: %s <preset_number>\n", argv[0]);
        return 1;
    }
    int preset = atoi(argv[1]);

    runtime_stub_state_t stub;
    runtime_stubs_init(&stub, SAMPLE_RATE, FRAMES, CH);

    unit_loader_t loader;
    memset(&loader, 0, sizeof(loader));
    const char *unit_paths[] = {
        "units/druteus" UNIT_LIB_EXT,
        "test/presets-editor/units/druteus" UNIT_LIB_EXT,
    };
    int load_result = -1;
    for (size_t i = 0; i < sizeof(unit_paths) / sizeof(unit_paths[0]); ++i) {
        load_result = unit_loader_open(unit_paths[i], &loader);
        if (load_result == 0) break;
    }
    if (load_result != 0) {
        fprintf(stderr, "Failed to load druteus\n");
        return 1;
    }
    if (loader.header && stub.runtime_desc)
        stub.runtime_desc->target = loader.header->target;
    unit_loader_init(&loader, stub.runtime_desc);

    float *out = calloc(FRAMES * CH, sizeof(float));
    float *tmp = calloc(FRAMES * CH, sizeof(float));

    // Settle
    for (int i = 0; i < 32; i++)
        unit_loader_render(&loader, NULL, tmp, FRAMES);

    // Set preset
    loader.unit_set_param_value(1, preset);  // PATCH param
    loader.unit_set_param_value(2, 16);      // VOICES
    loader.unit_set_param_value(5, 100);     // VOLUME
    loader.unit_set_param_value(6, 64);      // PAN center
    loader.unit_set_param_value(3, 0);       // TUNE
    loader.unit_set_param_value(9, 0);       // LAYERS=both
    render_n(&loader, out, 16);

    // Try several notes across the keyboard
    int notes[] = {36, 48, 60, 72, 84};
    int n_notes = sizeof(notes) / sizeof(notes[0]);

    printf("Preset %d:\n", preset);
    for (int n = 0; n < n_notes; n++) {
        loader.unit_all_note_off();
        render_n(&loader, out, 100);
        loader.unit_note_on(notes[n], 100);
        float max_peak = 0;
        int total_clip = 0;
        for (int f = 0; f < 20; f++) {
            render_n(&loader, out, 1);
            float p = peak(out, FRAMES, CH);
            if (p > max_peak) max_peak = p;
            total_clip += count_clipping(out, FRAMES, CH);
        }
        printf("  note %3d: peak=%.4f clip_samples=%d %s\n",
               notes[n], (double)max_peak, total_clip,
               max_peak > 1.0f ? "** CLIPPING **" : "");
    }

    free(out);
    free(tmp);
    return 0;
}
