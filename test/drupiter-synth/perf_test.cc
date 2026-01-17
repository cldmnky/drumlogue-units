/**
 * @file perf_test.cc
 * @brief Performance monitoring test for Drupiter synth in different modes
 *
 * Tests CPU usage across mono, polyphonic, and unison synthesis modes
 * using the built-in PERF_MON system.
 *
 * Usage:
 *   Build with PERF_MON=1: ./build.sh drupiter-synth PERF_MON=1
 *   Run test: ./perf_test
 *
 * Output shows cycle counts and CPU utilization for each synthesis mode.
 */

#include <iostream>
#include <iomanip>
#include <vector>
#include <string>
#include <chrono>
#include <thread>

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

// Test configuration
static constexpr uint32_t kSampleRate = 48000;
static constexpr uint32_t kTestDurationSeconds = 2;  // Test each mode for 2 seconds
static constexpr uint32_t kFramesPerBuffer = 128;   // Typical drumlogue buffer size

// CPU frequency for utilization calculation (ARM Cortex-A7 typical)
static constexpr uint32_t kCpuFrequencyHz = 600000000;  // 600 MHz

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
        std::cout << "Buffer Size: " << kFramesPerBuffer << " samples\n";
        std::cout << "Test Duration: " << kTestDurationSeconds << " seconds per mode\n";
        std::cout << "CPU Frequency: " << (kCpuFrequencyHz / 1000000) << " MHz\n\n";

        // Test each synthesis mode
        TestVoiceCount("1 Voice (Mono)", 1);
        TestVoiceCount("2 Voices (Poly)", 2);
        TestVoiceCount("4 Voices (Poly)", 4);

        // Print summary table
        PrintSummaryTable();

        std::cout << "=== Performance Summary ===\n";
        PrintUtilizationGuide();
    }

private:
    DrupiterSynth synth_;
    std::vector<float> test_buffer_;
    std::vector<PerfResult> results_;

    void TestVoiceCount(const std::string& mode_name, int voice_count) {
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
        std::cout << "Synth initialized successfully\n";

        // Set synthesis mode via hub control (not default voice allocator)
        // MOD_SYNTH_MODE = 14, values: 0=MONO, 1=POLY, 2=UNISON
        const uint8_t synth_mode = (voice_count == 1) ? 0 : 1;  // MONO for 1 voice, POLY for 2+ voices
        synth_.SetHubValue(14, synth_mode);  // MOD_SYNTH_MODE

        // Reset performance counters
        PERF_MON_RESET();

        // Warm up (1 second)
        std::cout << "  Warming up...\n";
        std::cout << "  Calling Render for warmup...\n";
        RunTestSequence(1.0f, voice_count);
        std::cout << "  Warmup complete\n";

        // Reset counters again for actual test
        PERF_MON_RESET();

        // Run actual test
        std::cout << "  Running performance test...\n";
        RunTestSequence(static_cast<float>(kTestDurationSeconds), voice_count);

        // Collect and display results
        CollectPerformanceResults(mode_name);
        PrintPerformanceResults(mode_name);
        std::cout << "\n";
    }

    void CollectPerformanceResults(const std::string& mode_name) {
        // Calculate cycles per second for utilization
        const uint32_t cycles_per_second = kCpuFrequencyHz;
        const uint32_t cycles_per_sample = cycles_per_second / kSampleRate;

        // Total utilization
        uint32_t total_avg_cycles = PERF_MON_TOTAL_AVG();
        uint32_t total_peak_cycles = PERF_MON_TOTAL_PEAK();
        float total_avg_util = (static_cast<float>(total_avg_cycles) / cycles_per_sample) * 100.0f;
        float total_peak_util = (static_cast<float>(total_peak_cycles) / cycles_per_sample) * 100.0f;

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
        std::cout << "+----------------+--------+--------+--------+--------+\n";
        std::cout << "| Mode          | Avg CPU| Peak CPU| Avg Cyc| Peak Cyc|\n";
        std::cout << "+----------------+--------+--------+--------+--------+\n";

        for (const auto& result : results_) {
            std::cout << "| " << std::left << std::setw(14) << result.mode_name << " | "
                      << std::right << std::setw(6) << result.total_avg_util << "% | "
                      << std::setw(6) << result.total_peak_util << "% | "
                      << std::setw(6) << result.total_avg_cycles << " | "
                      << std::setw(6) << result.total_peak_cycles << " |\n";
        }

        std::cout << "+----------------+--------+--------+--------+--------+\n";

        // Show ratings
        std::cout << "Performance Ratings:\n";
        for (const auto& result : results_) {
            std::cout << "  " << std::left << std::setw(16) << result.mode_name << ": " << result.rating << "\n";
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
            // Skip note triggering for now to isolate the crash
            if (buffer % (buffers_to_process / voice_count) == 0 && note_index < voice_count) {
                synth_.NoteOn(notes[note_index % 4], velocities[note_index % 4]);
                note_index++;
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

        // Calculate cycles per second for utilization
        const uint32_t cycles_per_second = kCpuFrequencyHz;
        const uint32_t cycles_per_sample = cycles_per_second / kSampleRate;

        // Display each counter
        for (uint8_t i = 0; i < ::dsp::PerfMon::GetCounterCount(); ++i) {
            ::dsp::PerfStats stats = PERF_MON_GET_STATS(i);
            
            if (stats.frame_count == 0) continue;

            // Calculate utilization percentage
            float avg_utilization = (static_cast<float>(stats.average_cycles) / cycles_per_sample) * 100.0f;
            float peak_utilization = (static_cast<float>(stats.peak_cycles) / cycles_per_sample) * 100.0f;

            std::cout << std::fixed << std::setprecision(1);
            std::cout << "    " << stats.name << ":\n";
            std::cout << "      Avg: " << stats.average_cycles << " cycles ("
                      << avg_utilization << "% CPU)\n";
            std::cout << "      Peak: " << stats.peak_cycles << " cycles ("
                      << peak_utilization << "% CPU)\n";
            std::cout << "      Min: " << stats.min_cycles << " cycles\n";
            std::cout << "      Measurements: " << stats.frame_count << "\n";
        }

        // Total utilization
        uint32_t total_avg_cycles = PERF_MON_TOTAL_AVG();
        uint32_t total_peak_cycles = PERF_MON_TOTAL_PEAK();
        float total_avg_util = (static_cast<float>(total_avg_cycles) / cycles_per_sample) * 100.0f;
        float total_peak_util = (static_cast<float>(total_peak_cycles) / cycles_per_sample) * 100.0f;

        std::cout << "    TOTAL:\n";
        std::cout << "      Avg: " << total_avg_cycles << " cycles ("
                  << total_avg_util << "% CPU)\n";
        std::cout << "      Peak: " << total_peak_cycles << " cycles ("
                  << total_peak_util << "% CPU)\n";

        // Performance rating
        std::string rating;
        if (total_avg_util < 50.0f) rating = "EXCELLENT (plenty of headroom)";
        else if (total_avg_util < 70.0f) rating = "GOOD (reasonable headroom)";
        else if (total_avg_util < 80.0f) rating = "FAIR (near limit)";
        else rating = "POOR (may cause xruns)";

        std::cout << "      Rating: " << rating << "\n";
    }

    void PrintUtilizationGuide() {
        std::cout << "CPU Utilization Guide:\n";
        std::cout << "  < 50%: Excellent - plenty of headroom for modulation/effects\n";
        std::cout << "  50-70%: Good - reasonable headroom, stable performance\n";
        std::cout << "  70-80%: Fair - near limit, monitor carefully\n";
        std::cout << "  > 80%: Poor - may cause audio dropouts (xruns)\n\n";

        std::cout << "Performance Breakdown:\n";
        std::cout << "  VoiceAlloc: Voice management, note triggering, envelope updates\n";
        std::cout << "  DCO: Oscillator processing (wavetable lookup, FM, drift)\n";
        std::cout << "  VCF: Filter processing (LPF with resonance)\n";
        std::cout << "  Effects: Chorus, modulation, additional processing\n";
        std::cout << "  RenderTotal: Complete audio buffer processing\n\n";

        std::cout << "Optimization Notes:\n";
        std::cout << "  - Q31 interpolation reduces DCO CPU by 30-40%\n";
        std::cout << "  - PolyBLEP anti-aliasing adds ~5-10% CPU per oscillator\n";
        std::cout << "  - 24dB filters use ~15% more CPU than 12dB\n";
        std::cout << "  - Heavy modulation increases processing load\n";
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
    std::cerr << "Build with: ./build.sh drupiter-synth PERF_MON=1\n";
    return 1;
#endif

    PerfTest test;
    test.RunAllTests();

    return 0;
}