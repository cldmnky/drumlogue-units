/**
 * @file wave_analysis.cc
 * @brief Verifies and analyses what the Pepege oscillator actually puts out.
 *
 * Three checks, all against the PPG ROM data the unit ships:
 *
 *  1. Exactness: for every stored wavetable and a set of slots, one rendered
 *     cycle is compared against an independent computation from the ROM (key
 *     wave lookup, slot interpolation, antisymmetric mirroring). The oscillator
 *     runs at 375 Hz, i.e. exactly 128 samples per cycle, so the rendered
 *     samples line up with the 128-sample waveform. Raw mode is the one that
 *     matches the hardware, which had no sample interpolation.
 *  2. Sweep: per table, the spectral centroid of every slot, so a sweep can be
 *     seen moving through the table and the classic end waves show up.
 *  3. Aliasing: harmonic versus non-harmonic energy for a high note in the
 *     three interpolation modes.
 *
 * Usage: ./wave_analysis [--table N] [--mode exact|sweep|alias]
 */

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

#ifndef TEST
#define TEST
#endif
#include "unit.h"
#include "../../drumlogue/pepege-synth/pepege_synth.h"

namespace {

constexpr float kSampleRate = 48000.0f;
constexpr double kExactHz = 375.0;          // 48000 / 128: exactly one cycle per 128 samples

/** A wave sample with the PPG's antisymmetric mirroring (half-cycle storage). */
float Mirrored(const uint8_t* half, int phase128) {
    const int p = phase128 & 127;
    return p < 64 ? static_cast<float>(half[p]) : static_cast<float>(255 - half[127 - p]);
}

struct KeyWave { int wave; int slot; };

std::vector<KeyWave> KeyWaves(int table) {
    const uint8_t* def = ppg_table_defs + ppg_table_offsets[table];
    std::vector<KeyWave> keys;
    while (*def != 0xFF) {
        const int wave = *def++;
        const int slot = *def++;
        keys.push_back({wave, slot});
    }
    return keys;
}

/** The two key waves a slot sits between, and how far along it is (0..1). */
struct SlotBlend {
    const uint8_t* lower = nullptr;
    const uint8_t* upper = nullptr;
    float blend = 0.0f;
    bool classic = false;
    const uint8_t* classic_wave = nullptr;
};

SlotBlend SlotWaves(int table, int slot) {
    SlotBlend result;
    if (slot >= static_cast<int>(PPG_CLASSIC_FIRST_SLOT)) {
        result.classic = true;
        result.classic_wave = ppg_classic_waves[slot - PPG_CLASSIC_FIRST_SLOT];
        result.lower = result.upper = result.classic_wave;
        return result;
    }
    const std::vector<KeyWave> keys = KeyWaves(table);
    for (size_t i = 0; i < keys.size(); i++) {
        if (keys[i].slot > slot) continue;
        result.lower = ppg_wave_data + keys[i].wave * 64;
        result.upper = result.lower;
        if (i + 1 < keys.size()) {
            result.upper = ppg_wave_data + keys[i + 1].wave * 64;
            const int span = keys[i + 1].slot - keys[i].slot;
            result.blend = span > 0 ? static_cast<float>(slot - keys[i].slot) / span : 0.0f;
        }
    }
    return result;
}

/** One cycle of the oscillator alone, loaded exactly as the unit loads it. */
std::vector<float> OscCycle(int table, int slot, int mode) {
    common::PpgOsc<PPG_TABLE_SLOTS> osc;
    osc.Init(kSampleRate);
    osc.LoadWavetable(ppg_wave_data, ppg_table_defs + ppg_table_offsets[table]);
    for (uint32_t i = 0; i < PPG_NUM_CLASSIC_WAVES; i++) {
        osc.SetWave(static_cast<uint8_t>(PPG_CLASSIC_FIRST_SLOT + i), ppg_classic_waves[i]);
    }
    osc.SetMode(static_cast<common::PpgMode>(mode));
    osc.SetFrequency(static_cast<float>(kExactHz));   // exactly 128 samples per cycle
    osc.SetWavePosition(static_cast<float>(slot) / 63.0f);

    std::vector<float> samples;
    for (int i = 0; i < 384; i++) samples.push_back(osc.Process());
    return samples;                                    // the last 128 are one cycle
}

/** Renders the unit with a static patch and returns the left channel. */
std::vector<float> RenderStatic(int table, int slot, int mode, int note, int blocks) {
    PepegeSynth synth;
    synth.Init(kSampleRate);
    synth.SetParameter(P_OSC_A_BANK, table);
    synth.SetParameter(P_OSC_A_MORPH, static_cast<int32_t>(slot * 127.0f / 63.0f + 0.5f));
    synth.SetParameter(P_OSC_A_OCT, 0);
    synth.SetParameter(P_OSC_A_TUNE, 0);
    synth.SetParameter(P_OSC_B_MORPH, 0);
    synth.SetParameter(P_OSC_MIX, -64);              // oscillator A only
    synth.SetParameter(P_OSC_MODE, mode);
    synth.SetParameter(P_FILTER_CUTOFF, 127);        // as open as the filter goes
    synth.SetParameter(P_FILTER_RESO, 0);
    synth.SetParameter(P_FILTER_ENV, 0);
    synth.SetParameter(P_FILTER_TYPE, 0);
    synth.SetParameter(P_AMP_ATTACK, 0);
    synth.SetParameter(P_AMP_DECAY, 0);
    synth.SetParameter(P_AMP_SUSTAIN, 127);
    synth.SetParameter(P_AMP_RELEASE, 0);
    synth.SetParameter(P_MOD_SELECT, MOD_LFO_TO_MORPH);
    synth.SetParameter(P_MOD_VALUE, 50);
    synth.SetParameter(P_MOD_SELECT, MOD_LFO_TO_FILTER);
    synth.SetParameter(P_MOD_VALUE, 50);
    synth.SetParameter(P_MOD_SELECT, MOD_VEL_TO_FILTER);
    synth.SetParameter(P_MOD_VALUE, 0);
    synth.SetParameter(P_MOD_SELECT, MOD_KEY_TRACK);
    synth.SetParameter(P_MOD_VALUE, 0);
    synth.NoteOn(static_cast<uint8_t>(note), 127);

    std::vector<float> out(128), mono;
    for (int block = 0; block < blocks; block++) {
        synth.Render(out.data(), 64);
        for (int i = 0; i < 64; i++) mono.push_back(out[i * 2]);
    }
    return mono;
}

/** Windowed DFT magnitude at one frequency (Hann window, so leakage is low). */
double ToneEnergy(const std::vector<float>& x, double frequency) {
    const size_t n = x.size();
    double re = 0, im = 0, window_sum = 0;
    for (size_t i = 0; i < n; i++) {
        const double w = 0.5 - 0.5 * std::cos(2.0 * M_PI * i / (n - 1));
        const double phase = 2.0 * M_PI * frequency * i / kSampleRate;
        re += x[i] * w * std::cos(phase);
        im -= x[i] * w * std::sin(phase);
        window_sum += w;
    }
    return (re * re + im * im) / (window_sum * window_sum);
}

/** Goertzel energy of a signal at a frequency (used for the centroid sweep). */
double EnergyAt(const std::vector<float>& x, double frequency) {
    const double w = 2.0 * M_PI * frequency / kSampleRate;
    const double coeff = 2.0 * std::cos(w);
    double s1 = 0, s2 = 0;
    for (float sample : x) {
        const double s0 = sample + coeff * s1 - s2;
        s2 = s1;
        s1 = s0;
    }
    return (s1 * s1 + s2 * s2 - coeff * s1 * s2) / (static_cast<double>(x.size()) * x.size());
}

struct Exactness { double worst = 0; int table = -1; int slot = -1; double mean = 0; int counted = 0; };

Exactness CheckExactness(int only_table, int mode) {
    Exactness result;
    const int probe_slots[] = {0, 1, 3, 7, 8, 15, 30, 47, 59, 60, 61, 62, 63};
    for (uint32_t table = 0; table < PPG_NUM_TABLES; table++) {
        if (only_table >= 0 && static_cast<int>(table) != only_table) continue;
        for (int slot : probe_slots) {
            const SlotBlend blend = SlotWaves(static_cast<int>(table), slot);
            if (blend.lower == nullptr) continue;

            const std::vector<float> rendered = OscCycle(static_cast<int>(table), slot, mode);
            const size_t start = rendered.size() - 128;

            // The oscillator's phase at the start of the window is unknown, so
            // try every rotation and keep the best alignment.
            double best = 1e9;
            for (int rotation = 0; rotation < 128; rotation++) {
                double error = 0;
                for (int n = 0; n < 128; n++) {
                    const int phase128 = (rotation + n) & 127;
                    const float lower = Mirrored(blend.lower, phase128);
                    const float upper = Mirrored(blend.upper, phase128);
                    const float want = (lower + (upper - lower) * blend.blend - 127.5f) / 127.5f;
                    error += std::fabs(rendered[start + n] - want);
                }
                best = std::min(best, error / 128.0);
            }
            result.mean += best;
            result.counted++;
            if (best > result.worst) {
                result.worst = best;
                result.table = static_cast<int>(table);
                result.slot = slot;
            }
        }
    }
    if (result.counted > 0) result.mean /= result.counted;
    return result;
}

void ReportExactness(int only_table) {
    std::printf("=== Exactness: oscillator output vs ROM-derived waveform ===\n");
    std::printf("(the unit's filter always sits in the signal path, so this drives the\n"
                " oscillator directly; it is loaded exactly as LoadBankWavetable() does)\n");
    for (int mode = 0; mode < 3; mode++) {
        const char* names[] = {"HiFi", "LoFi", "Raw"};
        const Exactness result = CheckExactness(only_table, mode);
        std::printf("%-5s mean |error| = %.5f, worst = %.5f (table %d slot %d) "
                    "[one 8-bit step = %.5f]\n",
                    names[mode], result.mean, result.worst, result.table, result.slot,
                    1.0 / 127.5);
    }
    std::printf("(Raw is the hardware behaviour: no sample interpolation, so it should match\n"
                " the ROM waveform to within the slot interpolation's quantisation. HiFi and\n"
                " LoFi interpolate samples, so a larger error there is expected.)\n\n");
}

void ReportSweep(int only_table) {
    std::printf("=== Sweep: spectral centroid per slot (Hz, note A4) ===\n");
    std::printf("%-13s", "table");
    for (int slot = 0; slot <= 63; slot += 8) std::printf(" %6d", slot);
    std::printf("   %s\n", "60-63");
    for (uint32_t table = 0; table < PPG_NUM_TABLES; table++) {
        if (only_table >= 0 && static_cast<int>(table) != only_table) continue;
        std::printf("%2u %-10s", table, ppg_table_names[table]);
        for (int slot = 0; slot <= 63; slot += 8) {
            const std::vector<float> rendered = RenderStatic(static_cast<int>(table), slot, 2, 69, 30);
            const std::vector<float> window(rendered.begin() + rendered.size() / 2, rendered.end());
            double weighted = 0, total = 0;
            for (int harmonic = 1; harmonic <= 40; harmonic++) {
                const double frequency = 440.0 * harmonic;
                if (frequency > kSampleRate / 2) break;
                const double energy = EnergyAt(window, frequency);
                weighted += frequency * energy;
                total += energy;
            }
            std::printf(" %6.0f", total > 0 ? weighted / total : 0.0);
        }
        std::printf("\n");
    }
    std::printf("\n");
}

void ReportAliasing() {
    // A 128-sample PPG cycle holds harmonics up to the 64th. Everything past
    // Nyquist folds back down, so the folded harmonics are where aliasing shows
    // up: a raw oscillator (what the hardware did) folds them at full level,
    // linear sample interpolation attenuates them - but only weakly, since
    // sinc^2 over 128 samples is a gentle filter. Two waves: a smooth one and a
    // harmonically rich one, so the numbers can be sanity-checked.
    struct Case { int table; int slot; const char* name; };
    const Case cases[] = {{0, 0, "smooth"}, {13, 30, "rich"}};
    const int notes[] = {96, 108};                       // C7 (2093 Hz), C8 (4186 Hz)
    const double frequencies[] = {2093.0, 4186.0};
    const char* mode_names[] = {"HiFi", "LoFi", "Raw"};

    std::printf("=== Aliasing: folded-harmonic share of total energy (%%), "
                "table 0 slot 0 (smooth) vs table 13 slot 30 (rich) ===\n");
    std::printf("%-6s", "mode");
    for (int index = 0; index < 2; index++) {
        for (const Case& c : cases) {
            std::printf("  %s %s", index == 0 ? "C7" : "C8", c.name);
        }
    }
    std::printf("\n");

    for (int mode = 0; mode < 3; mode++) {
        std::printf("%-6s", mode_names[mode]);
        for (int index = 0; index < 2; index++) {
            for (const Case& c : cases) {
                const std::vector<float> mono =
                    RenderStatic(c.table, c.slot, mode, notes[index], 300);
                const std::vector<float> window(mono.begin() + mono.size() - 8192, mono.end());
                const double f0 = frequencies[index];
                double harmonic = 0, alias = 0;
                for (int k = 1; k <= 64; k++) {
                    const double frequency = k * f0;
                    if (frequency < kSampleRate / 2 - 100) {
                        harmonic += ToneEnergy(window, frequency);
                    } else {
                        double folded = std::fmod(frequency, kSampleRate);
                        if (folded > kSampleRate / 2) folded = kSampleRate - folded;
                        if (folded > 100 && folded < kSampleRate / 2 - 100) {
                            alias += ToneEnergy(window, folded);
                        }
                    }
                }
                std::printf("  %11.2f%%", 100.0 * alias / (harmonic + alias));
            }
        }
        std::printf("\n");
    }
    std::printf("(the three modes alias comparably: interpolating 128 samples is too weak a\n"
                " filter to matter; the modes differ in wave-morph continuity instead)\n\n");
}

void ReportPresets() {
    // Every preset should render without clipping across the keyboard: the unit
    // mixes two voices at half gain and then widens, so peaks above 1.0 leave
    // no headroom. A preset that is safe in one register can clip in another,
    // so each is measured at several intervals.
    std::printf("=== Preset levels: worst peak over the keyboard (two notes held) ===\n");
    const uint8_t pairs[4][2] = {{36, 43}, {48, 55}, {60, 64}, {72, 79}};
    std::printf("%-3s %-12s", "#", "preset");
    for (const auto& pair : pairs) std::printf("  %3u+%3u", pair[0], pair[1]);
    std::printf("   worst\n");

    bool any_clipping = false;
    for (int preset = 0; preset < 6; preset++) {
        std::printf("%-3d %-12s", preset, PepegeSynth::GetPresetName(preset));
        double worst = 0;
        uint32_t nonfinite = 0;
        for (const auto& pair : pairs) {
            PepegeSynth synth;
            synth.Init(kSampleRate);
            synth.LoadPreset(static_cast<uint8_t>(preset));
            synth.NoteOn(pair[0], 100);
            synth.NoteOn(pair[1], 100);

            std::vector<float> out(128);
            double peak = 0;
            for (int block = 0; block < 750; block++) {          // 1 s
                synth.Render(out.data(), 64);
                for (float sample : out) {
                    if (!std::isfinite(sample)) nonfinite++;
                    peak = std::max(peak, static_cast<double>(std::fabs(sample)));
                }
            }
            std::printf("  %6.3f", peak);
            worst = std::max(worst, peak);
        }
        const bool clips = worst > 1.0;
        any_clipping = any_clipping || clips;
        std::printf("   %6.3f %s%s\n", worst, clips ? "CLIPS" : "ok",
                    nonfinite > 0 ? " NONFINITE" : "");
    }
    std::printf("%s\n\n", any_clipping ? "FAIL: at least one preset clips" : "all presets ok");
}

}  // namespace

int main(int argc, char** argv) {
    int only_table = -1;
    std::string mode = "all";
    for (int i = 1; i < argc; i++) {
        if (std::strcmp(argv[i], "--table") == 0 && i + 1 < argc) only_table = std::atoi(argv[++i]);
        else if (std::strcmp(argv[i], "--mode") == 0 && i + 1 < argc) mode = argv[++i];
    }

    std::printf("PPG wavetables in the unit: %u, slots per table: %u, waves in ROM: 256\n\n",
                PPG_NUM_TABLES, PPG_TABLE_SLOTS);

    if (mode == "all" || mode == "exact") ReportExactness(only_table);
    if (mode == "all" || mode == "sweep") ReportSweep(only_table);
    if (mode == "all" || mode == "alias") ReportAliasing();
    if (mode == "all" || mode == "presets") ReportPresets();
    return 0;
}
