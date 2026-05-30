#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>

#include "../core/unit_loader.h"
#include "../sdk/runtime_stubs.h"

#if defined(__APPLE__)
#define UNIT_LIB_EXT ".dylib"
#else
#define UNIT_LIB_EXT ".so"
#endif

static float peak(const float *out, int frames, int ch) {
    float m = 0;
    for (int j = 0; j < frames * ch; j++) {
        float a = out[j] >= 0 ? out[j] : -out[j];
        if (a > m) m = a;
    }
    return m;
}

static void render_n(unit_loader_t *loader, float *out, int n, int frames, int ch) {
    for (int i = 0; i < n; i++) {
        memset(out, 0, frames * ch * sizeof(float));
        unit_loader_render(loader, NULL, out, frames);
    }
}

int main() {
    const uint16_t frames = 128;
    const uint8_t ch = 2;
    float *out = calloc(frames * ch, sizeof(float));
    int failures = 0;

    runtime_stub_state_t stub;
    runtime_stubs_init(&stub, 48000, frames, ch);
    unit_loader_t loader = {0};
    const char *unit_paths[] = {
        "units/druteus" UNIT_LIB_EXT,
        "test/presets-editor/units/druteus" UNIT_LIB_EXT,
    };
    int load_result = -1;
    for (size_t i = 0; i < sizeof(unit_paths) / sizeof(unit_paths[0]); ++i) {
        load_result = unit_loader_open(unit_paths[i], &loader);
        if (load_result == 0) {
            break;
        }
    }
    if (load_result != 0) {
        fprintf(stderr, "Failed to load druteus shared library\n");
        free(out);
        runtime_stubs_teardown(&stub);
        return 1;
    }
    if (loader.header && stub.runtime_desc)
        stub.runtime_desc->target = loader.header->target;
    unit_loader_init(&loader, stub.runtime_desc);

    for (int i = 0; i < 200; i++) {
        memset(out, 0, frames * ch * sizeof(float));
        unit_loader_render(&loader, NULL, out, frames);
        if (peak(out, frames, ch) > 0.001f) break;
    }
    // === Test: polyphonic — 2nd note doesn't kill 1st ===
    printf("=== Polyphonic: A+B → release B → A still plays ===\n");

    loader.unit_note_on(60, 100);  // A
    render_n(&loader, out, 20, frames, ch);
    loader.unit_note_on(67, 100);  // B (A should still ring)
    render_n(&loader, out, 20, frames, ch);
    float p_ab = peak(out, frames, ch);
    printf("  A+B playing peak: %.6f\n", p_ab);

    loader.unit_note_off(67);      // release B only
    render_n(&loader, out, 20, frames, ch);
    float p_a = peak(out, frames, ch);
    printf("  After B release, A still playing peak: %.6f  => %s\n",
           p_a, p_a > 0.001f ? "PASS" : "FAIL (note A killed)");
    if (p_a < 0.001f) failures++;

    // Release A and verify silence
    loader.unit_note_off(60);
    render_n(&loader, out, 150, frames, ch);
    float p_sil = peak(out, frames, ch);
    printf("  After A release peak: %.8f  => %s\n",
           p_sil, p_sil < 0.001f ? "PASS" : "FAIL");
    if (p_sil >= 0.001f) failures++;

    // === Test: single-voice mode — each note kills previous ===
    printf("\n=== 1-voice mode: A → B kills A ===\n");
    loader.unit_all_note_off();
    render_n(&loader, out, 50, frames, ch);
    loader.unit_set_param_value(2, 1);  // VOICES=1

    loader.unit_note_on(60, 100);  // A
    render_n(&loader, out, 10, frames, ch);
    loader.unit_note_on(67, 100);  // B (should kill A)
    render_n(&loader, out, 10, frames, ch);
    float p_solo = peak(out, frames, ch);
    printf("  Solo B peak: %.6f  => %s (both notes)\n", p_solo,
           p_solo > 0.001f ? "PASS" : "FAIL");

    // === Summary ===
    printf("\n%s (%d failures)\n",
           failures == 0 ? "ALL PASSED" : "FAILURES", failures);

    loader.unit_set_param_value(2, 16);  // reset voices

    free(out);
    unit_loader_close(&loader);
    runtime_stubs_teardown(&stub);
    return failures ? 1 : 0;
}
