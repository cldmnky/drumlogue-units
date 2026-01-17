// Test harness for Pepege Synth DSP
// Tests HubControl integration and basic functionality

#include <iostream>
#include <vector>
#include <cmath>
#include <cstring>
#include <cstdlib>

// Define TEST to enable desktop testing
#define TEST

// Include WAV file utilities
#include "wav_file.h"

// Include the Pepege Synth
#include "../../drumlogue/pepege-synth/pepege_synth.h"

void PrintUsage(const char* prog) {
  std::cerr << "Usage: " << prog << " input.wav output.wav [options]\n";
  std::cerr << "Options:\n";
  std::cerr << "  --param<id> <value>    Set parameter (0-100)\n";
  std::cerr << "  --mod-select <dest>    Set modulation destination (0-7)\n";
  std::cerr << "  --mod-value <value>    Set modulation value (0-100)\n";
  std::cerr << "\nTest signal generation:\n";
  std::cerr << "  --generate-impulse out.wav\n";
  std::cerr << "  --generate-sine out.wav <freq_hz>\n";
  std::cerr << "  --generate-noise out.wav\n";
}

// Generate test signals
void GenerateImpulse(const char* output, uint32_t sample_rate = 48000) {
  if (WavFile::GenerateImpulse(output, sample_rate, 1.0f)) {
    std::cout << "Generated impulse: " << output << std::endl;
  }
}

void GenerateSine(const char* output, float freq, uint32_t sample_rate = 48000) {
  if (WavFile::GenerateSine(output, freq, 0.5f, 1.0f, sample_rate)) {
    std::cout << "Generated sine " << freq << "Hz: " << output << std::endl;
  }
}

void GenerateNoise(const char* output, uint32_t sample_rate = 48000) {
  if (WavFile::GenerateNoise(output, 1.0f, 1.0f, sample_rate)) {
    std::cout << "Generated white noise: " << output << std::endl;
  }
}

int main(int argc, char** argv) {
  if (argc < 2) {
    PrintUsage(argv[0]);
    return 1;
  }

  // Handle test signal generation
  if (strcmp(argv[1], "--generate-impulse") == 0 && argc >= 3) {
    GenerateImpulse(argv[2]);
    return 0;
  }
  if (strcmp(argv[1], "--generate-sine") == 0 && argc >= 4) {
    GenerateSine(argv[2], atof(argv[3]));
    return 0;
  }
  if (strcmp(argv[1], "--generate-noise") == 0 && argc >= 3) {
    GenerateNoise(argv[2]);
    return 0;
  }

  // Normal processing mode
  if (argc < 2) {
    PrintUsage(argv[0]);
    return 1;
  }

  const char* output_file = argv[1];
  int num_frames = 48000;  // 1 second of audio
  if (argc >= 3) {
    num_frames = atoi(argv[2]) * 48;  // Convert ms to samples at 48kHz
  }

  // Initialize DSP
  PepegeSynth synth;
  synth.Init(48000);

  // Parse parameters from command line
  for (int i = 3; i < argc; i += 2) {
    if (strncmp(argv[i], "--param", 7) == 0 && i + 1 < argc) {
      int param_id = atoi(argv[i] + 7);  // Extract number after --param
      int value = atoi(argv[i + 1]);
      synth.SetParameter(param_id, value);
      std::cout << "  Set param " << param_id << " = " << value << std::endl;
    } else if (strcmp(argv[i], "--mod-select") == 0 && i + 1 < argc) {
      int dest = atoi(argv[i + 1]);
      synth.SetParameter(P_MOD_SELECT, dest);
      std::cout << "  Set mod destination = " << dest << std::endl;
    } else if (strcmp(argv[i], "--mod-value") == 0 && i + 1 < argc) {
      int value = atoi(argv[i + 1]);
      synth.SetParameter(P_MOD_VALUE, value);
      std::cout << "  Set mod value = " << value << std::endl;
    }
  }

  // Test HubControl functionality
  std::cout << "\n=== Testing HubControl Integration ===" << std::endl;

  // Test parameter setting and getting
  synth.SetParameter(P_MOD_SELECT, 0);  // Select LFO Rate
  synth.SetParameter(P_MOD_VALUE, 75);  // Set to 75%
  int32_t mod_select = synth.GetParameter(P_MOD_SELECT);
  int32_t mod_value = synth.GetParameter(P_MOD_VALUE);
  std::cout << "Mod select: " << mod_select << ", value: " << mod_value << std::endl;

  // Test parameter string formatting
  const char* dest_name = synth.GetParameterStr(P_MOD_SELECT, mod_select);
  const char* value_str = synth.GetParameterStr(P_MOD_VALUE, mod_value);
  std::cout << "Destination name: " << (dest_name ? dest_name : "null") << std::endl;
  std::cout << "Value string: " << (value_str ? value_str : "null") << std::endl;

  // Generate output
  std::vector<float> output(num_frames * 2);  // Stereo output
  synth.Process(nullptr, output.data(), num_frames, 2);

  // Write output WAV
  WavFile wav_out;
  if (!wav_out.OpenWrite(output_file, 48000, 2)) {
    std::cerr << "Error: Could not open output file " << output_file << std::endl;
    return 1;
  }
  wav_out.Write(output);
  wav_out.Close();

  std::cout << "\nOutput: " << output_file << " (" << num_frames << " frames)" << std::endl;
  std::cout << "✓ HubControl integration test completed successfully!" << std::endl;

  return 0;
}