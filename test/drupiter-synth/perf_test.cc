/**
 * @file perf_test.cc
 * @brief Performance monitoring test for Drupiter synth in different modes
 *
 * Measures whole-callback CPU cost for mono / poly / unison modes using the
 * built-in PERF_MON system, mirroring the real drumlogue callback:
 *   - 48 kHz sample rate
 *   - 64 frames per buffer (hardware buffer size)
 *   - i.MX6 ULZ Cortex-A7 @ 900 MHz => ~1.2M cycle budget per callback
 *
 * The headline metric is the RenderTotal counter ONLY. The DCO/VCF/Effects
 * counters nest inside RenderTotal and are displayed for informational
 * purposes; they must never be summed with it.
 *
 * Usage:
 *   Build with PERF_MON=1: make PERF_MON=1 perf_test
 *   Run test: ./perf_test
 */

#include <iostream>
#include <iomanip>
#include <vector>
#include <string>
#include <chrono>
#include <thread>
#include <cstring>

// Include the Drupiter synth
#include "../../drumlogue/drupiter-synth/drupiter_synth.h"

// Include performance monitoring
#include "../common/perf_mon.h"

// Include test mocks
#include "unit.h"

// Define the unit_header for testing
const unit_header_t unit_header = {
    .header_size = sizeof(unit_header_t),
    .target = 0,  // Mock target
    .api = 0,
    .dev_id = 0,
    .unit_id = 0,
    .version = 0,
    .name = "Test Unit",
    .num_presets = 0,
    .num_params = 0
};

// Test configuration - mirrors real hardware
static constexpr uint32_t kSampleRate = 48000;
static constexpr uint32_t kTestDurationSeconds = 2;  // Test each mode for 2 seconds
static constexpr uint32_t kFramesPerBuffer = 64;     // drumlogue hardware buffer size

// CPU frequency for utilization calculation.
// DRUSYS report: NXP i.MX6 ULZ, single Cortex-A7 @ 900 MHz.
static constexpr uint32_t kCpuFrequencyHz = 900000000;

// Cycle budget for one full audio callback
static constexpr uint32_t kCyclesPerSample = kCpuFrequencyHz / kSampleRate;
static constexpr uint32_t kBudgetPerBuffer = kCyclesPerSample * kFramesPerBuffer;

// Structure to store performance results for summary table
struct PerfResult {
    std::string mode_name;
    uint32_t total_avg_cycles;
    uint32_t total_peak_cycles;
    float total_avg_util;
    float total_peak_util;
    std::string rating;
};

class PerfTest {
public:
    PerfTest() : synth_(), test_buffer_(kFramesPerBuffer * 2) {}

    void RunAllTests() {
        std::cout << "=== Drupiter Synth Performance Test ===\n";
        std::cout << "Sample Rate: " << kSampleRate << " Hz\n";
        std::cout << "Buffer Size: " << kFramesPerBuffer << " frames (hardware)\n";
        std::cout << "Test Duration: " << kTestDurationSeconds << " seconds per mode\n";
        std::cout << "CPU Frequency: " << (kCpuFrequencyHz / 1000000) << " MHz\n";
        std::cout << "Callback budget: " << kBudgetPerBuffer << " cycles ("
                  << std::fixed << std::setprecision(3)
                  << (static_cast<float>(kFramesPerBuffer) / kSampleRate * 1000.0f)
                  << " ms)\n\n";

        // Test each synthesis mode
        TestScenario("Mono (1 voice)", 1);
        TestScenario("Poly (2 voices)", 2);
        TestScenario("Poly (4 voices)", 4);
        TestScenario("Unison (held note)", 5);

        // Print summary table
        PrintSummaryTable();

        std::cout << "=== Performance Summary ===\n";
        PrintUtilizationGuide();
    }

private:
    DrupiterSynth synth_;
    std::vector<float> test_buffer_;
    std::vector<PerfResult> results_;

    // Find a counter index by name (counters are registered inside Init())
    static uint8_t FindCounter(const char* name) {
        const uint8_t count = ::dsp::PerfMon::GetCounterCount();
        for (uint8_t i = 0; i < count; ++i) {
            if (std::strcmp(::dsp::PerfMon::GetCounterName(i), name) == 0) {
                return i;
            }
        }
        return 0xFF;
    }

    void TestScenario(const std::string& mode_name, int voice_count) {
        std::cout << "Testing " << mode_name << "...\n";

        // Initialize synth
        unit_runtime_desc_t runtime_desc = {};
        runtime_desc.api = UNIT_API_INIT(0, 1);
        runtime_desc.target = unit_header.target;
        runtime_desc.frames_per_buffer = kFramesPerBuffer;
        runtime_desc.input_channels = 0;  // Synth, no input
        runtime_desc.output_channels = 2; // Stereo output
        runtime_desc.samplerate = kSampleRate;

        int result = synth_.Init(&runtime_desc);
        if (result != k_unit_err_none) {
            std::cerr << "Failed to initialize synth: " << result << "\n";
            return;
        }

        // Set synthesis mode via hub control using NATIVE destination values:
        // MOD_SYNTH_MODE range is 0..2 (MONO/POLY/UNISON).
        uint8_t hub_mode = 0;
        if (voice_count >= 2 && voice_count <= 4) hub_mode = 1;  // POLY
        if (voice_count == 5) hub_mode = 2;                      // UNISON
        synth_.SetHubValue(MOD_SYNTH_MODE, hub_mode);

        // Reset performance counters
        PERF_MON_RESET();

        // Warm up (1 second)
        RunTestSequence(1.0f, voice_count);

        // Reset counters again for actual test
        PERF_MON_RESET();

        // Run actual test
        RunTestSequence(static_cast<float>(kTestDurationSeconds), voice_count);

        // Collect and display results
        CollectPerformanceResults(mode_name);
        PrintPerformanceResults(mode_name);
        std::cout << "\n";
    }

    void CollectPerformanceResults(const std::string& mode_name) {
        // Headline metric: whole-callback cost (RenderTotal counter).
        const uint8_t total_idx = FindCounter("RenderTotal");
        const uint32_t total_avg_cycles = (total_idx != 0xFF)
            ? ::dsp::PerfMon::GetAverageCycles(total_idx) : 0;
        const uint32_t total_peak_cycles = (total_idx != 0xFF)
            ? ::dsp::PerfMon::GetPeakCycles(total_idx) : 0;

        // Utilization against the FULL CALLBACK budget (not per-sample).
        float total_avg_util =
            (static_cast<float>(total_avg_cycles) / static_cast<float>(kBudgetPerBuffer)) * 100.0f;
        float total_peak_util =
            (static_cast<float>(total_peak_cycles) / static_cast<float>(kBudgetPerBuffer)) * 100.0f;

        // Performance rating
        std::string rating;
        if (total_avg_util < 50.0f) rating = "EXCELLENT";
        else if (total_avg_util < 70.0f) rating = "GOOD";
        else if (total_avg_util < 80.0f) rating = "FAIR";
        else rating = "POOR";

        // Store result
        results_.push_back({
            mode_name,
            total_avg_cycles,
            total_peak_cycles,
            total_avg_util,
            total_peak_util,
            rating
        });
    }

    void PrintSummaryTable() {
        std::cout << "=== Performance Summary Table ===\n";
        std::cout << std::fixed << std::setprecision(1);
        std::cout << "+----------------------+---------+---------+---------+---------+\n";
        std::cout << "| Mode                 | Avg CPU | Peak CPU| Avg Cyc | Peak Cyc|\n";
        std::cout << "+----------------------+---------+---------+---------+---------+\n";

        for (const auto& result : results_) {
            std::cout << "| " << std::left << std::setw(20) << result.mode_name << " | "
                      << std::right << std::setw(6) << result.total_avg_util << "% | "
                      << std::setw(6) << result.total_peak_util << "% | "
                      << std::setw(7) << result.total_avg_cycles << " | "
                      << std::setw(7) << result.total_peak_cycles << " |\n";
        }

        std::cout << "+----------------------+---------+---------+---------+---------+\n";

        // Show ratings
        std::cout << "Performance Ratings:\n";
        for (const auto& result : results_) {
            std::cout << "  " << std::left << std::setw(20) << result.mode_name
                      << ": " << result.rating << "\n";
        }
        std::cout << "\n";
    }

    void RunTestSequence(float duration_seconds, int voice_count) {
        const uint32_t total_frames = static_cast<uint32_t>(duration_seconds * kSampleRate);
        const uint32_t buffers_to_process = total_frames / kFramesPerBuffer;

        // MIDI note sequence for testing
        const uint8_t notes[] = {60, 64, 67, 72};  // C4, E4, G4, C5
        const uint8_t velocities[] = {100, 80, 90, 70};
        int note_index = 0;

        for (uint32_t buffer = 0; buffer < buffers_to_process; ++buffer) {
            // Trigger up to `voice_count` notes at the start so sustained
            // voices exercise the requested mode during measurement.
            if (buffer == 0) {
                const int notes_to_trigger =
                    (voice_count == 5) ? 1 :  // Unison: one held note drives the stack
                    (voice_count > 4 ? 4 : voice_count);
                for (int n = 0; n < notes_to_trigger; ++n) {
                    synth_.NoteOn(notes[n % 4], velocities[n % 4]);
                }
                note_index = notes_to_trigger;
            }

            // Process audio buffer
            synth_.Render(test_buffer_.data(), kFramesPerBuffer);
        }

        // Release all notes that were actually triggered
        for (int i = 0; i < note_index; ++i) {
            synth_.NoteOff(notes[i % 4]);
        }

        // Let envelopes finish
        for (int i = 0; i < 100; ++i) {
            synth_.Render(test_buffer_.data(), kFramesPerBuffer);
        }
    }

    void PrintPerformanceResults(const std::string& mode_name) {
        std::cout << "  " << mode_name << " Results:\n";

        // Display each counter (informational; DCO/VCF/Effects nest in RenderTotal)
        for (uint8_t i = 0; i < ::dsp::PerfMon::GetCounterCount(); ++i) {
            ::dsp::PerfStats stats = ::dsp::PerfMon::GetStats(i);

            if (stats.frame_count == 0) continue;

            std::cout << "    " << stats.name << ":\n";
            std::cout << "      Avg: " << stats.average_cycles
                      << " cycles/buffer, Peak: " << stats.peak_cycles
                      << ", Min: " << stats.min_cycles
                      << " (" << stats.frame_count << " buffers)\n";
        }

        // Headline utilization from stored result
        const PerfResult& r = results_.back();
        std::cout << std::fixed << std::setprecision(1);
        std::cout << "    CALLBACK TOTAL:\n";
        std::cout << "      Avg: " << r.total_avg_cycles << " cycles ("
                  << r.total_avg_util << "% of budget)\n";
        std::cout << "      Peak: " << r.total_peak_cycles << " cycles ("
                  << r.total_peak_util << "% of budget)\n";
        std::cout << "      Rating: " << r.rating << "\n";
    }

    void PrintUtilizationGuide() {
        std::cout << "CPU Utilization Guide (per 64-frame callback @ 900 MHz):\n";
        std::cout << "  < 50%: Excellent - plenty of headroom for modulation/effects\n";
        std::cout << "  50-70%: Good - reasonable headroom, stable performance\n";
        std::cout << "  70-80%: Fair - near limit, monitor carefully\n";
        std::cout << "  > 80%: Poor - may cause audio dropouts (xruns)\n\n";

        std::cout << "Notes:\n";
        std::cout << "  - DCO/VCF/Effects counters are nested inside RenderTotal;\n";
        std::cout << "    they overlap and must NOT be summed.\n";
        std::cout << "  - Desktop/QEMU numbers use a wall-clock simulation and are\n";
        std::cout << "    indicative only; validate on hardware before release.\n";
    }
};

int main(int argc, char** argv) {
    (void)argc;
    (void)argv;
    std::cout << "Drupiter Synth Performance Monitor Test\n";
    std::cout << "=======================================\n\n";

    // Check if PERF_MON is enabled
#ifndef PERF_MON
    std::cerr << "ERROR: PERF_MON not enabled!\n";
    std::cerr << "Build with: make clean && make PERF_MON=1 perf_test\n";
    return 1;
#endif

    PerfTest test;
    test.RunAllTests();

    return 0;
}
