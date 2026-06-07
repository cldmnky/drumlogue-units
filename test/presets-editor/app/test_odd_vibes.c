/*
 * test_odd_vibes.c — Diagnostic test for the "Odd Vibes" preset.
 *
 * Loads the druteus synth unit, sets PATCH=12 (Odd Vibes), then renders the
 * same MIDI note three times with different LAYERS param values:
 *   - LAYERS=BOTH (default 0): both primary (i1) and secondary (i2) layers
 *   - LAYERS=PRI  (1):         primary layer only  (i1instrument)
 *   - LAYERS=SEC  (2):         secondary layer only (i2instrument)
 *
 * Each render is captured to a 16-bit PCM stereo WAV in ./fixtures/odd_vibes/
 * along with peak amplitude measurements so the layers can be compared
 * without listening.
 */
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

#define FRAMES        128
#define CH            2
#define SAMPLE_RATE   48000
#define RENDER_SECS   3
#define TOTAL_FRAMES  (SAMPLE_RATE * RENDER_SECS)
#define NOTE          60
#define VELOCITY      100

/* preset index of "Odd Vibes" in the proteus_patches.h table */
#define ODD_VIBES_IDX 12

static int fail_count = 0;
#define CHECK(cond, msg) do { \
    if (!(cond)) { printf("  FAIL: %s\n", msg); fail_count++; } \
    else         { printf("  PASS: %s\n", msg); } \
} while (0)

/* ---------- minimal 16-bit PCM stereo WAV writer ---------- */
static int write_wav_stereo_16(const char *path, const float *interleaved,
                               uint32_t frames, uint32_t sample_rate) {
    FILE *fp = fopen(path, "wb");
    if (!fp) { perror(path); return -1; }

    const uint16_t n_channels   = 2;
    const uint16_t bits_per_sam = 16;
    const uint32_t byte_rate    = sample_rate * n_channels * (bits_per_sam / 8);
    const uint16_t block_align  = n_channels * (bits_per_sam / 8);
    const uint32_t data_bytes   = frames * n_channels * (bits_per_sam / 8);
    const uint32_t chunk_size   = 36 + data_bytes;

    fwrite("RIFF", 1, 4, fp);
    fwrite(&chunk_size, 4, 1, fp);
    fwrite("WAVE", 1, 4, fp);
    fwrite("fmt ", 1, 4, fp);
    const uint32_t fmt_size = 16;
    fwrite(&fmt_size, 4, 1, fp);
    const uint16_t pcm = 1;
    fwrite(&pcm, 2, 1, fp);
    fwrite(&n_channels, 2, 1, fp);
    fwrite(&sample_rate, 4, 1, fp);
    fwrite(&byte_rate, 4, 1, fp);
    fwrite(&block_align, 2, 1, fp);
    fwrite(&bits_per_sam, 2, 1, fp);
    fwrite("data", 1, 4, fp);
    fwrite(&data_bytes, 4, 1, fp);

    /* Convert float [-1,1] to int16 (clipping). */
    for (uint32_t i = 0; i < frames * n_channels; i++) {
        float s = interleaved[i];
        if (s >  1.0f) s =  1.0f;
        if (s < -1.0f) s = -1.0f;
        int16_t q = (int16_t)lrintf(s * 32767.0f);
        fwrite(&q, 2, 1, fp);
    }
    fclose(fp);
    return 0;
}

/* ---------- helpers ---------- */
static float peak(const float *out, uint32_t frames) {
    float m = 0.0f;
    for (uint32_t j = 0; j < frames * CH; j++) {
        float a = out[j] >= 0 ? out[j] : -out[j];
        if (a > m) m = a;
    }
    return m;
}

static float peak_l(const float *out, uint32_t frames) {
    float m = 0.0f;
    for (uint32_t j = 0; j < frames * CH; j += CH) {
        float a = out[j] >= 0 ? out[j] : -out[j];
        if (a > m) m = a;
    }
    return m;
}

static float peak_r(const float *out, uint32_t frames) {
    float m = 0.0f;
    for (uint32_t j = 1; j < frames * CH; j += CH) {
        float a = out[j] >= 0 ? out[j] : -out[j];
        if (a > m) m = a;
    }
    return m;
}

static int load_unit(unit_loader_t *loader, runtime_stub_state_t *stub) {
    runtime_stubs_init(stub, SAMPLE_RATE, FRAMES, CH);
    memset(loader, 0, sizeof(*loader));
    const char *paths[] = {
        "units/druteus" UNIT_LIB_EXT,
        "test/presets-editor/units/druteus" UNIT_LIB_EXT,
    };
    int rc = -1;
    for (size_t i = 0; i < sizeof(paths) / sizeof(paths[0]); ++i) {
        rc = unit_loader_open(paths[i], loader);
        if (rc == 0) break;
    }
    if (rc != 0) {
        fprintf(stderr, "Failed to load druteus shared library\n");
        return -1;
    }
    if (loader->header && stub->runtime_desc)
        stub->runtime_desc->target = loader->header->target;
    unit_loader_init(loader, stub->runtime_desc);

    /* Let init settle (and SF2 async-load complete). */
    float *tmp = calloc(FRAMES * CH, sizeof(float));
    for (int i = 0; i < 4000; i++)   /* ~10.7s of 128-frame buffers */
        loader->unit_render(NULL, tmp, FRAMES);
    free(tmp);
    return 0;
}

/* Capture `frames` of audio with the given layer mode active.
 * Triggers NOTE, then renders `note_on_offset` frames of silence, then
 * `capture_frames` of audio. Returns peak amplitude. */
static float capture(unit_loader_t *loader, const char *wav_path,
                     int layers_mode, const char *label,
                     float *captured) {
    loader->unit_set_param_value(9, layers_mode);   /* param_layers */
    /* Allow layer change to settle. */
    float *tmp = calloc(FRAMES * CH, sizeof(float));
    for (int i = 0; i < 4; i++) loader->unit_render(NULL, tmp, FRAMES);
    free(tmp);

    loader->unit_all_note_off();
    tmp = calloc(FRAMES * CH, sizeof(float));
    for (int i = 0; i < 50; i++) loader->unit_render(NULL, tmp, FRAMES);
    free(tmp);

    /* Render the rest of the capture window. Note-off after 0.5s. */
    uint32_t total = TOTAL_FRAMES;
    uint32_t note_off_at = SAMPLE_RATE / 2;

    loader->unit_note_on(NOTE, VELOCITY);

    float pmax = 0.0f, pmax_l = 0.0f, pmax_r = 0.0f;
    int note_off_sent = 0;
    for (uint32_t i = 0; i < total; i += FRAMES) {
        uint32_t n = (total - i) < FRAMES ? (total - i) : FRAMES;
        memset(captured + i * CH, 0, n * CH * sizeof(float));
        loader->unit_render(NULL, captured + i * CH, n);
        if (i >= note_off_at && loader->unit_note_off && !note_off_sent) {
            loader->unit_note_off(NOTE);
            note_off_sent = 1;
        }
        float p = peak(captured + i * CH, n);
        if (p > pmax) pmax = p;
        float pl = peak_l(captured + i * CH, n);
        if (pl > pmax_l) pmax_l = pl;
        float pr = peak_r(captured + i * CH, n);
        if (pr > pmax_r) pmax_r = pr;
    }

    if (wav_path) {
        if (write_wav_stereo_16(wav_path, captured, total, SAMPLE_RATE) == 0)
            printf("  Wrote %s\n", wav_path);
        else
            printf("  ERROR writing %s\n", wav_path);
    }

    printf("  [%s] layers=%d  peak=%.4f  L=%.4f  R=%.4f\n",
           label, layers_mode, (double)pmax, (double)pmax_l, (double)pmax_r);
    return pmax;
}

int main(void) {
    /* Make sure the output dir exists. */
    if (system("mkdir -p fixtures/odd_vibes") != 0) {
        fprintf(stderr, "Failed to create fixtures dir\n");
        return 1;
    }

    unit_loader_t loader;
    runtime_stub_state_t stub;
    if (load_unit(&loader, &stub) != 0) {
        runtime_stubs_teardown(&stub);
        return 1;
    }
    printf("=== druteus unit loaded ===\n\n");

    /* Set Odd Vibes (preset index 12) */
    loader.unit_set_param_value(1, ODD_VIBES_IDX);
    /* Allow patch_dirty to be picked up by audio thread. */
    float *tmp = calloc(FRAMES * CH, sizeof(float));
    for (int i = 0; i < 8; i++) loader.unit_render(NULL, tmp, FRAMES);
    free(tmp);

    const char *preset_name = loader.unit_get_param_str_value
        ? loader.unit_get_param_str_value(1, ODD_VIBES_IDX) : NULL;
    printf("=== Preset %d = '%s' ===\n", ODD_VIBES_IDX,
           preset_name ? preset_name : "(name unavailable)");

    float *captured = calloc(TOTAL_FRAMES * CH, sizeof(float));
    if (!captured) {
        fprintf(stderr, "OOM\n");
        return 1;
    }

    printf("\n=== Capture 1: LAYERS=BOTH (default 0) ===\n");
    float p_both = capture(&loader, "fixtures/odd_vibes/both.wav",
                           0, "BOTH", captured);

    printf("\n=== Capture 2: LAYERS=PRI (1) — primary only ===\n");
    float p_pri = capture(&loader, "fixtures/odd_vibes/pri.wav",
                          1, "PRI ", captured);

    printf("\n=== Capture 3: LAYERS=SEC (2) — secondary only ===\n");
    float p_sec = capture(&loader, "fixtures/odd_vibes/sec.wav",
                          2, "SEC ", captured);

    printf("\n=== Analysis ===\n");
    printf("  Patch data: i1instrument=50 (Vibraphone)  i1volume=105\n");
    printf("               i2instrument=89 (Low Evens)  i2volume=103\n");
    printf("  Peak BOTH = %.4f  (sum of both layers, mixed)\n", (double)p_both);
    printf("  Peak PRI  = %.4f  (Vibraphone alone)\n",          (double)p_pri);
    printf("  Peak SEC  = %.4f  (Low Evens alone)\n",           (double)p_sec);
    if (p_pri > 0.001f && p_sec > 0.001f) {
        printf("  Both layers are active and audible. "
               "The 'drumkit' you hear is the secondary layer "
               "(Low Evens) layered on the primary Vibraphone.\n");
    } else if (p_pri > 0.001f && p_sec <= 0.001f) {
        printf("  Only the primary (Vibraphone) is audible.\n");
    } else if (p_pri <= 0.001f && p_sec > 0.001f) {
        printf("  Only the secondary (Low Evens) is audible.\n");
    } else {
        printf("  No audio output detected. Check SF2 path and load state.\n");
    }
    CHECK(p_both > 0.001f, "BOTH layers produce audio");
    CHECK(p_pri  > 0.001f, "PRI  layer (i1=50 Vibraphone) produces audio");
    CHECK(p_sec  > 0.001f, "SEC  layer (i2=89 Low Evens) produces audio");

    free(captured);
    loader.unit_set_param_value(9, 0);
    loader.unit_all_note_off();
    if (loader.unit_teardown) loader.unit_teardown();
    unit_loader_close(&loader);
    runtime_stubs_teardown(&stub);

    printf("\n=== %s (%d failures) ===\n",
           fail_count == 0 ? "ALL PASSED" : "FAILURES", fail_count);
    return fail_count ? 1 : 0;
}
