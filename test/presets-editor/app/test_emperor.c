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

static float peak_ch(const float *out, int frames, int ch, int channel) {
    float m = 0;
    for (int j = channel; j < frames * ch; j += ch) {
        float a = out[j] >= 0 ? out[j] : -out[j];
        if (a > m) m = a;
    }
    return m;
}

int main(int argc, char **argv) {
    int preset = 81;  // Emperor
    int note = 60;
    int verbose = 0;
    if (argc >= 2) preset = atoi(argv[1]);
    if (argc >= 3) note = atoi(argv[2]);
    if (argc >= 4) verbose = atoi(argv[3]);

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

    loader.unit_note_on(note, 100);
    printf("Preset %d note %d (Emperor):\n", preset, note);

    // Render 300 frames (~3.2 seconds at 48kHz, 128 frames per call)
    // First pass: track peak
    float max_peak = 0;
    float first_peak = 0;
    int peak_frame = 0;
    float peaks[300];
    memset(peaks, 0, sizeof(peaks));
    for (int f = 0; f < 300; f++) {
        unit_loader_render(&loader, NULL, tmp, FRAMES);
        float p = peak(tmp, FRAMES, CH);
        peaks[f] = p;
        if (f == 0) first_peak = p;
        if (p > max_peak) { max_peak = p; peak_frame = f; }
    }
    printf("  First frame peak: %.6f\n", (double)first_peak);
    printf("  Max peak: %.6f at frame %d (%.0f ms)\n",
           (double)max_peak, peak_frame,
           (double)(peak_frame * FRAMES * 1000.0 / SAMPLE_RATE));

    // Check for clipping on first 5 frames
    printf("  Clipping (peak>1.0): ");
    int any_clip = 0;
    for (int f = 0; f < 5; f++) {
        if (peaks[f] > 1.0f) {
            printf("frame %d (p=%.4f) ", f, (double)peaks[f]);
            any_clip = 1;
        }
    }
    if (!any_clip) printf("none");
    printf("\n");

    // Check for silence at end
    float tail_avg = 0;
    for (int f = 280; f < 300; f++) tail_avg += peaks[f];
    tail_avg /= 20;
    printf("  Tail avg (frames 280-299): %.8f\n", (double)tail_avg);

    // Profile peak per block of 10 frames
    printf("  Peak profile (frames):\n");
    for (int f = 0; f < 300; f += 10) {
        float block_peak = 0;
        for (int i = f; i < f + 10 && i < 300; i++)
            if (peaks[i] > block_peak) block_peak = peaks[i];
        printf("    %3d-%3d: %.4f\n", f, f + 9, (double)block_peak);
    }

    // Per-channel peaks for first frame
    loader.unit_all_note_off();
    for (int i = 0; i < 50; i++)
        unit_loader_render(&loader, NULL, tmp, FRAMES);
    loader.unit_note_on(note, 100);
    unit_loader_render(&loader, NULL, tmp, FRAMES);
    float ch0 = peak_ch(tmp, FRAMES, CH, 0);
    float ch1 = peak_ch(tmp, FRAMES, CH, 1);
    printf("  First frame per-channel: L=%.4f R=%.4f\n", (double)ch0, (double)ch1);

    free(tmp);
    return 0;
}
