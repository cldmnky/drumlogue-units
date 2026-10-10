/**
 * @file perf_test.cc
 * @brief CPU benchmark for Pepege-Synth that mirrors the drumlogue audio callback.
 *
 * Real hardware: 48 kHz, 64 frames per callback, Cortex-A7 @ 900 MHz
 *   -> 1.333 ms of audio and a ~1.2M cycle budget per callback.
 *
 * The numbers below come from an x86 host, so they are NOT hardware numbers.
 * Use them to compare before/after on the same machine: the libm calls in the
 * hot loop (powf/sinf) cost relatively more on ARM than on x86, so the gains
 * measured here understate what they save on the drumlogue.
 *
 * Usage:
 *   ./perf_test [--buffers N] [--dump PREFIX] [--scenario NAME]
 *
 * Machine-readable output (one line per scenario):
 *   PERF scenario=<name> avg_us=.. median_us=.. p95_us=.. max_us=..
 *        cycles=.. budget_pct=.. rt_factor=.. peak=.. hash=.. nonfinite=..
 */

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

#ifndef TEST
#define TEST
#endif
#include "unit.h"  // desktop mock of the drumlogue SDK types
#include "../../drumlogue/pepege-synth/pepege_synth.h"

// unit.cc defines this for the unit build (C++14 ODR-use of a static constexpr
// member); the benchmark needs the same definition when the compiler does not
// fold the preset table away (observed with the drumlogue GCC 6.5 toolchain).
constexpr PepegeSynth::Preset PepegeSynth::kPresets[];

namespace {

constexpr uint32_t kSampleRate = 48000;
constexpr uint32_t kFrames = 64;                                // drumlogue hardware buffer
constexpr double kBufferMicros = 1e6 * kFrames / kSampleRate;   // 1333.33 us
constexpr double kCyclesPerMicro = 900.0;                       // i.MX6 ULZ Cortex-A7 @ 900 MHz
constexpr double kCycleBudget = kBufferMicros * kCyclesPerMicro;  // ~1.2M cycles per callback

struct Param { uint8_t id; int32_t value; };
struct Hub { uint8_t dest; int32_t value; };

struct Scenario {
    const char* name;
    uint8_t preset;
    std::vector<uint8_t> notes;
    std::vector<Param> params;
    std::vector<Hub> hub;
    uint32_t retrigger_buffers;  // 0 = hold the notes for the whole run
};

std::vector<Scenario> Scenarios() {
    return {
        // Two held notes, factory preset: the everyday polyphonic load.
        {"poly2-keys", 0, {60, 64}, {}, {}, 0},

        // Two held notes, bass preset with the filter envelope moving.
        {"poly2-bass", 2, {36, 43}, {}, {}, 0},

        // Heavy per-sample modulation: LFO to morph and to filter.
        {"poly2-mod", 1, {55, 62}, {}, {{MOD_LFO_TO_MORPH, 100}, {MOD_LFO_TO_FILTER, 100}}, 0},

        // Same modulation, mono (SPACE = -64), so before/after dumps can be
        // compared directly: the only differences left are the control-rate
        // filter coefficient update and the polynomial LFO sine.
        {"mod-mono", 1, {55, 62}, {{P_SPACE, -64}},
         {{MOD_LFO_TO_MORPH, 100}, {MOD_LFO_TO_FILTER, 100}}, 0},

        // Modulation held below the filter's remap knee, so this isolates the
        // control-rate coefficient update from the cutoff mapping change: the
        // cutoff is 60/127 and neither the filter envelope, velocity, key
        // tracking nor the LFO can push it past the knee (0.6281).
        {"mod-low", 1, {55, 62},
         {{P_SPACE, -64}, {P_FILTER_CUTOFF, 60}, {P_FILTER_ENV, 0}},
         {{MOD_LFO_TO_MORPH, 100}, {MOD_LFO_TO_FILTER, 60}, {MOD_VEL_TO_FILTER, 0},
          {MOD_KEY_TRACK, 0}}, 0},

        // Note churn: two voices retriggered, envelopes restarting (the QEMU
        // harness plays like this, one note per second).
        {"churn", 4, {60, 67}, {}, {}, kSampleRate / kFrames},

        // Idle: no notes at all, the floor cost of a callback.
        {"idle", 0, {}, {}, {}, 0},

        // Static patch, mono, no modulation: bit-comparable before/after the
        // optimizations (see README in this folder). The cutoff stays below the
        // filter's remap knee (0.6281 -> parameter 79.8) on purpose: above it
        // the cutoff mapping itself changed, so a comparison there would show
        // that change rather than the render path.
        {"null-static", 0, {60, 64},
         {{P_FILTER_ENV, 0}, {P_SPACE, -64}, {P_FILTER_CUTOFF, 60}},
         {{MOD_LFO_TO_MORPH, 50}, {MOD_LFO_TO_FILTER, 50}, {MOD_VEL_TO_FILTER, 0},
          {MOD_KEY_TRACK, 0}, {MOD_OSC_B_DETUNE, 50}, {MOD_PB_RANGE, 0}},
         0},
    };
}

double Median(std::vector<double> values) {
    std::sort(values.begin(), values.end());
    return values[values.size() / 2];
}

double Percentile(std::vector<double> values, double fraction) {
    std::sort(values.begin(), values.end());
    const size_t index = static_cast<size_t>(fraction * (values.size() - 1) + 0.5);
    return values[index];
}

/** FNV-1a over the output bits: detects any change in the rendered audio. */
uint64_t Hash(const std::vector<float>& samples) {
    uint64_t hash = 1469598103934665603ull;
    for (float sample : samples) {
        uint32_t bits;
        std::memcpy(&bits, &sample, sizeof(bits));
        for (int byte = 0; byte < 4; ++byte) {
            hash ^= (bits >> (byte * 8)) & 0xFF;
            hash *= 1099511628211ull;
        }
    }
    return hash;
}

struct Result {
    double average_us = 0;
    double median_us = 0;
    double p95_us = 0;
    double max_us = 0;
    double peak = 0;
    uint64_t hash = 0;
    uint32_t nonfinite = 0;
};

Result Run(const Scenario& scenario, uint32_t buffers, uint32_t warmup, const char* dump_prefix) {
    PepegeSynth synth;
    synth.Init(static_cast<float>(kSampleRate));
    synth.LoadPreset(scenario.preset);

    for (const Param& param : scenario.params) {
        synth.SetParameter(param.id, param.value);
    }
    for (const Hub& hub : scenario.hub) {
        synth.SetParameter(P_MOD_SELECT, hub.dest);
        synth.SetParameter(P_MOD_VALUE, hub.value);
    }

    std::vector<float> out(kFrames * 2);
    std::vector<double> times;
    times.reserve(buffers);

    Result result;
    bool notes_on = false;
    const uint32_t note_velocity = 100;

    for (uint32_t buffer = 0; buffer < buffers + warmup; ++buffer) {
        const bool measuring = buffer >= warmup;

        // Note handling: hold, or retrigger every retrigger_buffers.
        if (scenario.retrigger_buffers == 0) {
            if (!notes_on) {
                for (uint8_t note : scenario.notes) synth.NoteOn(note, note_velocity);
                notes_on = true;
            }
        } else if ((buffer % scenario.retrigger_buffers) == 0) {
            for (uint8_t note : scenario.notes) synth.NoteOff(note);
            for (uint8_t note : scenario.notes) synth.NoteOn(note, note_velocity);
        }

        const auto start = std::chrono::steady_clock::now();
        synth.Render(out.data(), kFrames);
        const auto end = std::chrono::steady_clock::now();

        if (measuring) {
            times.push_back(std::chrono::duration<double, std::micro>(end - start).count());

            // Hash and peak describe the measured window only: the warm-up
            // still contains the parameter-smoothing transient, where a
            // control-rate coefficient update legitimately differs from a
            // per-sample one (it converges to the same steady state).
            for (float sample : out) {
                if (!std::isfinite(sample)) result.nonfinite++;
                const float magnitude = std::fabs(sample);
                if (magnitude > result.peak) result.peak = magnitude;
            }
            result.hash ^= Hash(out) + 0x9e3779b97f4a7c15ull + (result.hash << 6) + (result.hash >> 2);
        }

        if (dump_prefix != nullptr && measuring) {
            const std::string path = std::string(dump_prefix) + "-" + scenario.name + ".f32";
            FILE* file = std::fopen(path.c_str(), buffer == warmup ? "wb" : "ab");
            if (file != nullptr) {
                std::fwrite(out.data(), sizeof(float), out.size(), file);
                std::fclose(file);
            }
        }
    }

    double total = 0;
    for (double time : times) total += time;
    result.average_us = total / times.size();
    result.median_us = Median(times);
    result.p95_us = Percentile(times, 0.95);
    result.max_us = *std::max_element(times.begin(), times.end());

    return result;
}

}  // namespace

int main(int argc, char** argv) {
    uint32_t buffers = 3000;  // 4 s of audio per scenario
    uint32_t warmup = 200;
    const char* dump_prefix = nullptr;
    std::string only;

    for (int i = 1; i < argc; ++i) {
        if (std::strcmp(argv[i], "--buffers") == 0 && i + 1 < argc) {
            buffers = static_cast<uint32_t>(std::atoi(argv[++i]));
        } else if (std::strcmp(argv[i], "--dump") == 0 && i + 1 < argc) {
            dump_prefix = argv[++i];
        } else if (std::strcmp(argv[i], "--scenario") == 0 && i + 1 < argc) {
            only = argv[++i];
        } else {
            std::fprintf(stderr, "usage: %s [--buffers N] [--dump PREFIX] [--scenario NAME]\n", argv[0]);
            return 1;
        }
    }

    std::printf("Pepege-Synth callback benchmark\n");
    std::printf("  %u Hz, %u frames per callback (%.2f ms audio, %.0f cycles @ 900 MHz)\n",
                kSampleRate, kFrames, kBufferMicros / 1000.0, kCycleBudget);
    std::printf("  %u measured callbacks per scenario, %u warmup\n\n", buffers, warmup);
    std::printf("%-12s %10s %10s %10s %10s %10s %8s\n",
                "scenario", "avg_us", "median_us", "p95_us", "max_us", "budget_%", "rt_x");
    std::printf("--------------------------------------------------------------------------------\n");

    for (const Scenario& scenario : Scenarios()) {
        if (!only.empty() && only != scenario.name) continue;

        const Result result = Run(scenario, buffers, warmup, dump_prefix);

        std::printf("%-12s %10.2f %10.2f %10.2f %10.2f %10.2f %8.1f\n",
                    scenario.name, result.average_us, result.median_us, result.p95_us,
                    result.max_us, 100.0 * result.average_us / kBufferMicros,
                    kBufferMicros / result.average_us);
        std::printf("PERF scenario=%s avg_us=%.2f median_us=%.2f p95_us=%.2f max_us=%.2f "
                    "cycles=%.0f budget_pct=%.2f rt_factor=%.2f peak=%.4f hash=%016llx nonfinite=%u\n",
                    scenario.name, result.average_us, result.median_us, result.p95_us,
                    result.max_us, result.average_us * kCyclesPerMicro,
                    100.0 * result.average_us / kBufferMicros, kBufferMicros / result.average_us,
                    result.peak, static_cast<unsigned long long>(result.hash), result.nonfinite);
    }

    return 0;
}
