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

int main(int argc, char **argv) {
    int preset = 81;  // Emperor
    if (argc >= 2) preset = atoi(argv[1]);

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

    float *tmp = calloc(FRAMES * CH, sizeof(float));
    for (int i = 0; i < 32; i++)
        unit_loader_render(&loader, NULL, tmp, FRAMES);

    loader.unit_set_param_value(1, preset);
    loader.unit_set_param_value(2, 16);
    loader.unit_set_param_value(5, 100);
    loader.unit_set_param_value(6, 64);
    loader.unit_set_param_value(3, 0);
    loader.unit_set_param_value(9, 0);
    for (int i = 0; i < 16; i++)
        unit_loader_render(&loader, NULL, tmp, FRAMES);

    printf("Preset %d (Emperor) — rapid note repeat test\n", preset);

    // Play 64 rapid notes in sequence
    for (int n = 0; n < 64; n++) {
        int note = 36 + (n % 48);

        // Note on
        loader.unit_note_on(note, 100);

        // Render 4 frames (~11ms)
        float npeak = 0;
        for (int f = 0; f < 4; f++) {
            unit_loader_render(&loader, NULL, tmp, FRAMES);
            float p = peak(tmp, FRAMES, CH);
            if (p > npeak) npeak = p;
        }

        if (npeak > 1.0f)
            printf("  ** CLIP ** note %d (iter %d): peak=%.4f\n", note, n, (double)npeak);

        // Note off
        loader.unit_note_off(note);

        // Check if output dies
        for (int f = 0; f < 2; f++)
            unit_loader_render(&loader, NULL, tmp, FRAMES);
    }

    // After all rapid notes, play one more note and hold for 200 frames
    loader.unit_note_on(60, 100);
    float final_peak = 0;
    int died = 1;
    for (int f = 0; f < 200; f++) {
        unit_loader_render(&loader, NULL, tmp, FRAMES);
        float p = peak(tmp, FRAMES, CH);
        if (p > final_peak) final_peak = p;
        if (p > 0.001f) died = 0;
    }
    if (died)
        printf("  ** DIED ** Final note produced silence (peak=%.8f)\n", (double)final_peak);
    else
        printf("  OK: Final note peak=%.4f\n", (double)final_peak);

    // Check all frames between note-off and final note for silence bursts
    loader.unit_all_note_off();
    loader.unit_note_on(72, 127);  // Loud, high note
    printf("  Last test: note 72 vel=127\n");
    for (int f = 0; f < 50; f++) {
        unit_loader_render(&loader, NULL, tmp, FRAMES);
        float p = peak(tmp, FRAMES, CH);
        if (p > 1.0f)
            printf("    ** CLIP ** frame %d: peak=%.4f\n", f, (double)p);
        if (f > 10 && p < 0.0001f)
            printf("    ** SILENCE ** frame %d: peak=%.8f\n", f, (double)p);
    }
    printf("  Done. Final peak: %.4f\n", (double)peak(tmp, FRAMES, CH));

    free(tmp);
    return 0;
}
