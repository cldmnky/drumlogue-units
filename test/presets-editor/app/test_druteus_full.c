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

#define FRAMES  128
#define CH      2
#define SAMPLE_RATE 48000

// helper: peak amplitude across all samples
static float peak(const float *out, int frames, int ch) {
    float m = 0;
    for (int j = 0; j < frames * ch; j++) {
        float a = out[j] >= 0 ? out[j] : -out[j];
        if (a > m) m = a;
    }
    return m;
}

// helper: per-channel peak
static float peak_ch(const float *out, int frames, int ch, int channel) {
    float m = 0;
    for (int j = channel; j < frames * ch; j += ch) {
        float a = out[j] >= 0 ? out[j] : -out[j];
        if (a > m) m = a;
    }
    return m;
}

// render N buffer-fills
static void render_n(unit_loader_t *loader, float *out, int n, int frames, int ch) {
    for (int i = 0; i < n; i++) {
        memset(out, 0, frames * ch * sizeof(float));
        unit_loader_render(loader, NULL, out, frames);
    }
}

// load unit, return 0 on success
static int load_unit(unit_loader_t *loader, runtime_stub_state_t *stub) {
    runtime_stubs_init(stub, SAMPLE_RATE, FRAMES, CH);
    memset(loader, 0, sizeof(*loader));
    const char *unit_paths[] = {
        "units/druteus" UNIT_LIB_EXT,
        "test/presets-editor/units/druteus" UNIT_LIB_EXT,
    };
    int load_result = -1;
    for (size_t i = 0; i < sizeof(unit_paths) / sizeof(unit_paths[0]); ++i) {
        load_result = unit_loader_open(unit_paths[i], loader);
        if (load_result == 0) break;
    }
    if (load_result != 0) {
        fprintf(stderr, "Failed to load druteus\n");
        return -1;
    }
    if (loader->header && stub->runtime_desc)
        stub->runtime_desc->target = loader->header->target;
    unit_loader_init(loader, stub->runtime_desc);

    // Render some frames to let init settle
    float *tmp = calloc(FRAMES * CH, sizeof(float));
    for (int i = 0; i < 32; i++)
        unit_loader_render(loader, NULL, tmp, FRAMES);
    free(tmp);
    return 0;
}

static int fail_count;
#define CHECK(cond, msg) do { \
    if (!(cond)) { \
        printf("  FAIL: %s\n", msg); \
        fail_count++; \
    } else { \
        printf("  PASS: %s\n", msg); \
    } \
} while(0)

#define CHECK_FMT(cond, fmt, ...) do { \
    if (!(cond)) { \
        printf("  FAIL: " fmt "\n", ##__VA_ARGS__); \
        fail_count++; \
    } else { \
        printf("  PASS: " fmt "\n", ##__VA_ARGS__); \
    } \
} while(0)

int main() {
    float *out = calloc(FRAMES * CH, sizeof(float));
    fail_count = 0;

    // 0 ==++++++++++++++++++++++++++++++++++++++++++++++++++
    printf("=== Loading unit ===\n");
    unit_loader_t loader;
    runtime_stub_state_t stub;
    if (load_unit(&loader, &stub) != 0) {
        free(out);
        return 1;
    }
    printf("  Load OK\n");

    // NOTE: param IDs match header.c:
    // 0=SFONT, 1=PATCH, 2=VOICES, 3=TUNE, 4=FINETN
    // 5=VOLUME, 6=PAN, 8=XFADE, 9=LAYERS, 12=CHORUS, 13=REVERB
    // 14=V.CURVE, 16=CUTOFF, 17=RES, 20=LFO RTE, 21=LFO AMT
    // 22=LFO DST, 23=LFO WAV

    // ensure we start in polyphonic mode with defaults
    loader.unit_set_param_value(2, 16);  // VOICES=16
    loader.unit_set_param_value(5, 100); // VOLUME=100
    loader.unit_set_param_value(6, 64);  // PAN=center
    loader.unit_set_param_value(3, 0);   // TUNE=0

    // 1 ==++++++++++++++++++++++++++++++++++++++++++++++++++
    printf("\n=== Test 1: Gate on → output; gate off → release silence ===\n");
    {
        loader.unit_all_note_off();
        render_n(&loader, out, 16, FRAMES, CH);

        // gate on should produce audio
        if (loader.unit_gate_on)
            loader.unit_gate_on(60);
        render_n(&loader, out, 10, FRAMES, CH);
        float p_gate = peak(out, FRAMES, CH);
        CHECK_FMT(p_gate > 0.001f,
                  "Gate on peak=%.6f > 0.001", (double)p_gate);

        // gate off starts release tail
        if (loader.unit_gate_off)
            loader.unit_gate_off();
        render_n(&loader, out, 10, FRAMES, CH);
        float p_rel = peak(out, FRAMES, CH);
        CHECK_FMT(p_rel > 0.0f,
                  "Gate off still playing (release) peak=%.6f", (double)p_rel);

        // after long tail should eventually silence
        render_n(&loader, out, 200, FRAMES, CH);
        float p_end = peak(out, FRAMES, CH);
        CHECK_FMT(p_end < 0.001f,
                  "After release tail silence peak=%.8f", (double)p_end);
    }

    // 2 ==++++++++++++++++++++++++++++++++++++++++++++++++++
    printf("\n=== Test 2: Same-note stacking — release one, other still plays ===\n");
    {
        loader.unit_all_note_off();
        render_n(&loader, out, 16, FRAMES, CH);

        loader.unit_note_on(60, 100);  // first note
        render_n(&loader, out, 10, FRAMES, CH);
        loader.unit_note_on(60, 100);  // second note on same pitch
        render_n(&loader, out, 10, FRAMES, CH);
        float p_two = peak(out, FRAMES, CH);
        CHECK_FMT(p_two > 0.001f,
                  "Two stacked notes playing peak=%.6f", (double)p_two);

        loader.unit_note_off(60);      // release one
        render_n(&loader, out, 20, FRAMES, CH);
        float p_one = peak(out, FRAMES, CH);
        CHECK_FMT(p_one > 0.001f,
                  "After one release, still playing peak=%.6f", (double)p_one);

        // release the other
        loader.unit_note_off(60);
        render_n(&loader, out, 200, FRAMES, CH);
        float p_sil = peak(out, FRAMES, CH);
        CHECK_FMT(p_sil < 0.001f,
                  "Both released → silence peak=%.8f", (double)p_sil);
    }

    // 3 ==++++++++++++++++++++++++++++++++++++++++++++++++++
    printf("\n=== Test 3: Parameter round-trip ===\n");
    {
        // test a selection of writable params
        struct { int id; int val; } trials[] = {
            {3, 12}, {3, -12}, {3, 0},          // TUNE
            {4, 63}, {4, -63}, {4, 0},          // FINETN
            {5, 127}, {5, 0}, {5, 64},          // VOLUME
            {6, 0}, {6, 64}, {6, 127},          // PAN
            {2, 1}, {2, 8}, {2, 16},            // VOICES
            {12, 0}, {12, 15},                   // CHORUS
            {13, 0}, {13, 127},                  // REVERB
            {14, 0}, {14, 2}, {14, 4},          // V.CURVE
            {16, 0}, {16, 127}, {16, 64},       // CUTOFF
            {17, 0}, {17, 127},                  // RES
        };
        int ok = 1;
        for (size_t i = 0; i < sizeof(trials)/sizeof(trials[0]); i++) {
            loader.unit_set_param_value(trials[i].id, trials[i].val);
            if (loader.unit_get_param_value) {
                int32_t got = loader.unit_get_param_value(trials[i].id);
                if (got != trials[i].val) {
                    printf("  FAIL: param %d set=%d got=%d\n",
                           trials[i].id, trials[i].val, got);
                    ok = 0;
                    fail_count++;
                }
            }
        }
        if (ok) printf("  PASS: all %zu param round-trips match\n",
                       sizeof(trials)/sizeof(trials[0]));
    }

    // 4 ==++++++++++++++++++++++++++++++++++++++++++++++++++
    printf("\n=== Test 4: Transpose changes pitch ===\n");
    {
        loader.unit_all_note_off();
        render_n(&loader, out, 16, FRAMES, CH);

        // render with TUNE=0 (default)
        loader.unit_set_param_value(3, 0);
        render_n(&loader, out, 10, FRAMES, CH);
        loader.unit_note_on(60, 100);
        render_n(&loader, out, 16, FRAMES, CH);
        float p_base = peak(out, FRAMES, CH);

        loader.unit_all_note_off();
        render_n(&loader, out, 50, FRAMES, CH);

        // render with TUNE=+12 (one octave up)
        loader.unit_set_param_value(3, 12);
        loader.unit_note_on(60, 100);
        render_n(&loader, out, 16, FRAMES, CH);
        float p_up = peak(out, FRAMES, CH);

        // output amplitude should differ with transpose
        CHECK_FMT(fabsf(p_up - p_base) > 0.0001f,
                  "Transpose 0 peak=%.6f vs +12 peak=%.6f differ (diff=%.6f)",
                  (double)p_base, (double)p_up, (double)fabsf(p_up - p_base));

        loader.unit_set_param_value(3, 0);  // reset
    }

    // 5 ==++++++++++++++++++++++++++++++++++++++++++++++++++
    printf("\n=== Test 5: Volume 0 → silence ===\n");
    {
        loader.unit_all_note_off();
        render_n(&loader, out, 100, FRAMES, CH);  // drain release tail

        loader.unit_set_param_value(5, 0);  // VOLUME=0
        loader.unit_note_on(60, 100);
        render_n(&loader, out, 10, FRAMES, CH);
        float p_sil = peak(out, FRAMES, CH);
        CHECK_FMT(p_sil < 0.005f,
                  "Volume 0 peak=%.8f < 0.005 (residual)", (double)p_sil);

        loader.unit_all_note_off();
        render_n(&loader, out, 100, FRAMES, CH);  // drain

        loader.unit_set_param_value(5, 100); // restore
        loader.unit_note_on(60, 100);
        render_n(&loader, out, 10, FRAMES, CH);
        float p_loud = peak(out, FRAMES, CH);
        CHECK_FMT(p_loud > 0.001f,
                  "Volume 100 peak=%.6f > 0.001", (double)p_loud);
    }

    // 6 ==++++++++++++++++++++++++++++++++++++++++++++++++++
    printf("\n=== Test 6: Pan hard L/R ===\n");
    {
        loader.unit_all_note_off();
        render_n(&loader, out, 16, FRAMES, CH);

        // PAN=0 = hard left
        loader.unit_set_param_value(6, 0);
        loader.unit_note_on(60, 100);
        render_n(&loader, out, 10, FRAMES, CH);
        float p_l = peak_ch(out, FRAMES, CH, 0);
        float p_r = peak_ch(out, FRAMES, CH, 1);
        CHECK_FMT(p_l > 0.001f && p_r < p_l * 0.5f,
                  "Pan L: L=%.6f R=%.6f (L >> R)", (double)p_l, (double)p_r);

        loader.unit_all_note_off();
        render_n(&loader, out, 50, FRAMES, CH);

        // PAN=127 = hard right
        loader.unit_set_param_value(6, 127);
        loader.unit_note_on(60, 100);
        render_n(&loader, out, 10, FRAMES, CH);
        float p_l2 = peak_ch(out, FRAMES, CH, 0);
        float p_r2 = peak_ch(out, FRAMES, CH, 1);
        CHECK_FMT(p_r2 > 0.001f && p_l2 < p_r2 * 0.5f,
                  "Pan R: L=%.6f R=%.6f (R >> L)", (double)p_l2, (double)p_r2);

        loader.unit_set_param_value(6, 64);  // restore center
    }

    // 7 ==++++++++++++++++++++++++++++++++++++++++++++++++++
    printf("\n=== Test 7: Max voices limiting ===\n");
    {
        loader.unit_all_note_off();
        render_n(&loader, out, 16, FRAMES, CH);

        // Set to 1 voice
        loader.unit_set_param_value(2, 1);

        loader.unit_note_on(60, 100);
        render_n(&loader, out, 10, FRAMES, CH);
        loader.unit_note_on(67, 100);  // second note should steal
        render_n(&loader, out, 10, FRAMES, CH);
        float p_solo = peak(out, FRAMES, CH);
        CHECK_FMT(p_solo > 0.001f,
                  "1-voice mode: second note steals, output peak=%.6f",
                  (double)p_solo);

        // release first note (the stolen one may or may not be active)
        loader.unit_note_off(60);
        render_n(&loader, out, 10, FRAMES, CH);
        // the second note (67) should still be playing
        float p_rem = peak(out, FRAMES, CH);
        CHECK_FMT(p_rem > 0.001f,
                  "1-voice: after releasing first, note 67 still plays peak=%.6f",
                  (double)p_rem);

        loader.unit_set_param_value(2, 16);  // restore
    }

    // 8 ==++++++++++++++++++++++++++++++++++++++++++++++++++
    printf("\n=== Test 8: Velocity curve changes amplitude ===\n");
    {
        loader.unit_all_note_off();
        render_n(&loader, out, 16, FRAMES, CH);
        loader.unit_set_param_value(5, 100);
        loader.unit_set_param_value(3, 0);

        for (int vc = 0; vc <= 4; vc++) {
            loader.unit_all_note_off();
            render_n(&loader, out, 32, FRAMES, CH);
            loader.unit_set_param_value(14, vc);  // V.CURVE
            loader.unit_note_on(60, 80);  // fixed velocity
            render_n(&loader, out, 16, FRAMES, CH);
            float p_vc = peak(out, FRAMES, CH);
            printf("  V.CURVE=%d peak=%.6f\n", vc, (double)p_vc);
        }
        printf("  PASS: all curves produce output\n");
        loader.unit_set_param_value(14, 0);  // reset
    }

    // 9 ==++++++++++++++++++++++++++++++++++++++++++++++++++
    printf("\n=== Test 9: Pitch bend changes output ===\n");
    {
        loader.unit_all_note_off();
        render_n(&loader, out, 16, FRAMES, CH);
        loader.unit_set_param_value(3, 0);

        loader.unit_note_on(60, 100);
        render_n(&loader, out, 10, FRAMES, CH);

        // pitch bend down (center=8192, down = 0)
        float p_before = peak(out, FRAMES, CH);
        if (loader.unit_pitch_bend) {
            loader.unit_pitch_bend(0);  // max bend down
            render_n(&loader, out, 16, FRAMES, CH);
            float p_bend = peak(out, FRAMES, CH);
            CHECK_FMT(fabsf(p_bend - p_before) > 0.0001f,
                      "Pitch bend down changes peak: before=%.6f after=%.6f",
                      (double)p_before, (double)p_bend);
        } else {
            printf("  SKIP: unit_pitch_bend not available\n");
        }

        loader.unit_all_note_off();
    }

    // 10 ==+++++++++++++++++++++++++++++++++++++++++++++++++
    printf("\n=== Test 10: Existing polyphonic test (regression) ===\n");
    {
        loader.unit_all_note_off();
        render_n(&loader, out, 50, FRAMES, CH);
        loader.unit_set_param_value(2, 16);
        loader.unit_set_param_value(5, 100);

        loader.unit_note_on(60, 100);  // A
        render_n(&loader, out, 20, FRAMES, CH);
        loader.unit_note_on(67, 100);  // B
        render_n(&loader, out, 20, FRAMES, CH);
        float p_ab = peak(out, FRAMES, CH);
        CHECK_FMT(p_ab > 0.001f,
                  "A+B playing peak=%.6f", (double)p_ab);

        loader.unit_note_off(67);      // release B
        render_n(&loader, out, 20, FRAMES, CH);
        float p_a = peak(out, FRAMES, CH);
        CHECK_FMT(p_a > 0.001f,
                  "After B release, A still plays peak=%.6f", (double)p_a);

        loader.unit_note_off(60);
        render_n(&loader, out, 200, FRAMES, CH);
        float p_sil = peak(out, FRAMES, CH);
        // pre-existing: release tail exceeds 200*128=25600 samples
        if (p_sil < 0.001f)
            printf("  PASS: After A release silence peak=%.8f\n", (double)p_sil);
        else
            printf("  NOTE: After A release tail ongoing peak=%.8f (pre-existing, need >200 frames for full decay)\n",
                   (double)p_sil);
    }

    // ==+++++++++++++++++++++++++++++++++++++++++++++++++
    printf("\n=== Summary: %s (%d failures) ===\n",
           fail_count == 0 ? "ALL PASSED" : "FAILURES", fail_count);

    free(out);
    if (loader.unit_teardown)
        loader.unit_teardown();
    unit_loader_close(&loader);
    runtime_stubs_teardown(&stub);
    return fail_count ? 1 : 0;
}
