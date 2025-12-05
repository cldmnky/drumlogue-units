// Test harness for clouds-revfx DSP code
// Compiles for local testing without drumlogue hardware

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <vector>
#include <string>

#include "wav_file.h"

// Define TEST before including the DSP code to enable desktop-compatible implementations
#ifndef TEST
#define TEST
#endif

// Include the stub unit.h (which also declares unit_header extern)
#include "unit.h"

// Define the unit_header stub for desktop testing
// This provides the parameter definitions that clouds_fx.cc references
const unit_header_t unit_header = {
  .header_size = sizeof(unit_header_t),
  .target = 0,
  .api = 0,
  .dev_id = 0x636C646D,  // "cldm"
  .unit_id = 0x01010000,
  .version = 0x00010000,  // v1.0.0
  .name = "TestCloudsRev",
  .num_presets = 0,
  .num_params = 12,
  .params = {
    // idx 0: Dry/Wet (0-100%)
    {0, 100, 50, 50, k_unit_param_type_percent, 0, 0, 0, "Dry/Wet"},
    // idx 1: Reverb Time (0-100%)
    {0, 100, 50, 50, k_unit_param_type_percent, 0, 0, 0, "Time"},
    // idx 2: Diffusion (0-100%)
    {0, 100, 50, 50, k_unit_param_type_percent, 0, 0, 0, "Diffusion"},
    // idx 3: LP Filter (0-100%)
    {0, 100, 70, 70, k_unit_param_type_percent, 0, 0, 0, "LP"},
    // idx 4: Input Gain (0-100%)
    {0, 100, 20, 20, k_unit_param_type_percent, 0, 0, 0, "Input Gain"},
    // idx 5: Texture (0-100%)
    {0, 100, 0, 0, k_unit_param_type_percent, 0, 0, 0, "Texture"},
    // idx 6: Grain Amount (0-100%)
    {0, 100, 0, 0, k_unit_param_type_percent, 0, 0, 0, "Grain Amt"},
    // idx 7: Grain Size (0-100%)
    {0, 100, 50, 50, k_unit_param_type_percent, 0, 0, 0, "Grain Size"},
    // idx 8: Grain Density (0-100%)
    {0, 100, 50, 50, k_unit_param_type_percent, 0, 0, 0, "Grain Dens"},
    // idx 9: Grain Pitch (-24 to +24 semitones)
    {-24, 24, 0, 0, k_unit_param_type_semi, 0, 0, 0, "Grain Pitch"},
    // idx 10: Shift Amount (0-100%)
    {0, 100, 0, 0, k_unit_param_type_percent, 0, 0, 0, "Shift Amt"},
    // idx 11: Shift Pitch (-24 to +24 semitones)
    {-24, 24, 0, 0, k_unit_param_type_semi, 0, 0, 0, "Shift Pitch"},
    // Remaining slots are blank
    {0, 0, 0, 0, k_unit_param_type_none, 0, 0, 0, ""},
    {0, 0, 0, 0, k_unit_param_type_none, 0, 0, 0, ""},
    {0, 0, 0, 0, k_unit_param_type_none, 0, 0, 0, ""},
    {0, 0, 0, 0, k_unit_param_type_none, 0, 0, 0, ""},
    {0, 0, 0, 0, k_unit_param_type_none, 0, 0, 0, ""},
    {0, 0, 0, 0, k_unit_param_type_none, 0, 0, 0, ""},
    {0, 0, 0, 0, k_unit_param_type_none, 0, 0, 0, ""},
    {0, 0, 0, 0, k_unit_param_type_none, 0, 0, 0, ""},
    {0, 0, 0, 0, k_unit_param_type_none, 0, 0, 0, ""},
    {0, 0, 0, 0, k_unit_param_type_none, 0, 0, 0, ""},
    {0, 0, 0, 0, k_unit_param_type_none, 0, 0, 0, ""},
    {0, 0, 0, 0, k_unit_param_type_none, 0, 0, 0, ""},
  }
};

// Now include the actual DSP code
// The include paths are set up in the Makefile
#include "clouds_fx.h"

// Preset definitions - must match clouds_fx.cc
struct PresetDef {
  const char* name;
  int params[16];  // DRY/WET, TIME, DIFFUSION, LP_DAMP, IN_GAIN, TEXTURE,
                   // GRAIN_AMT, GRAIN_SIZE, GRAIN_DENS, GRAIN_PITCH, GRAIN_POS, FREEZE,
                   // SHIFT_AMT, SHIFT_PITCH, SHIFT_SIZE, reserved
};

static const PresetDef kPresets[] = {
  {"INIT",     {100, 80, 80, 90, 50, 0, 0, 64, 64, 64, 64, 0, 0, 64, 64, 0}},
  {"HALL",     {120, 110, 100, 100, 40, 30, 0, 64, 64, 64, 64, 0, 0, 64, 64, 0}},
  {"PLATE",    {100, 70, 127, 127, 60, 0, 0, 64, 64, 64, 64, 0, 0, 64, 64, 0}},
  {"SHIMMER",  {90, 100, 90, 80, 45, 40, 0, 64, 64, 64, 64, 0, 80, 88, 80, 0}},
  {"CLOUD",    {80, 90, 90, 85, 50, 60, 80, 90, 70, 64, 64, 0, 0, 64, 64, 0}},
  {"FREEZE",   {100, 127, 100, 95, 30, 80, 60, 100, 50, 64, 64, 0, 0, 64, 64, 0}},
  {"OCTAVER",  {90, 85, 80, 90, 50, 20, 0, 64, 64, 64, 64, 0, 100, 52, 70, 0}},
  {"AMBIENT",  {140, 120, 110, 75, 35, 50, 40, 80, 40, 64, 64, 0, 30, 76, 90, 0}},
};
static const int kNumPresets = sizeof(kPresets) / sizeof(kPresets[0]);

void PrintUsage(const char* program) {
  printf("Clouds RevFX Test Harness\n\n");
  printf("Usage: %s <input.wav> <output.wav> [options]\n", program);
  printf("       %s --generate-impulse <output.wav>\n", program);
  printf("       %s --generate-sine <output.wav> [frequency]\n", program);
  printf("       %s --generate-noise <output.wav>\n", program);
  printf("       %s --generate-drums <output.wav>\n", program);
  printf("       %s --list-presets\n", program);
  printf("\nOptions:\n");
  printf("  --preset <name|num>   Use a preset (0-7 or name like HALL, SHIMMER)\n");
  printf("  --dry-wet <0-100>     Dry/wet mix (default: 50)\n");
  printf("  --time <0-100>        Reverb time (default: 50)\n");
  printf("  --diffusion <0-100>   Diffusion (default: 50)\n");
  printf("  --lp <0-100>          Low-pass filter (default: 70)\n");
  printf("  --input-gain <0-100>  Input gain (default: 20)\n");
  printf("  --texture <0-100>     Diffuser texture (default: 0)\n");
  printf("  --grain-amt <0-100>   Granular amount (default: 0)\n");
  printf("  --grain-size <0-100>  Grain size (default: 50)\n");
  printf("  --grain-dens <0-100>  Grain density (default: 50)\n");
  printf("  --grain-pitch <-24 to 24>  Grain pitch (default: 0)\n");
  printf("  --shift-amt <0-100>   Pitch shifter amount (default: 0)\n");
  printf("  --shift-pitch <-24 to 24>  Pitch shift semitones (default: 0)\n");
  printf("\nPresets:\n");
  for (int i = 0; i < kNumPresets; ++i) {
    printf("  %d: %s\n", i, kPresets[i].name);
  }
  printf("\nExamples:\n");
  printf("  %s input.wav output.wav --preset HALL\n", program);
  printf("  %s input.wav output.wav --dry-wet 70 --time 80\n", program);
  printf("  %s --generate-impulse impulse.wav\n", program);
}

int FindPreset(const char* name_or_num) {
  // Try as number first
  char* endptr;
  long num = strtol(name_or_num, &endptr, 10);
  if (*endptr == '\0' && num >= 0 && num < kNumPresets) {
    return static_cast<int>(num);
  }
  // Try as name (case-insensitive)
  for (int i = 0; i < kNumPresets; ++i) {
    if (strcasecmp(name_or_num, kPresets[i].name) == 0) {
      return i;
    }
  }
  return -1;
}

int main(int argc, char* argv[]) {
  if (argc < 2) {
    PrintUsage(argv[0]);
    return 1;
  }

  // Check for generate commands
  std::string cmd = argv[1];
  
  if (cmd == "--list-presets") {
    printf("Available presets:\n");
    for (int i = 0; i < kNumPresets; ++i) {
      printf("  %d: %-10s  DW=%3d TM=%3d DF=%3d LP=%3d IG=%3d TX=%3d GA=%3d SH=%3d\n", 
             i, kPresets[i].name,
             kPresets[i].params[0], kPresets[i].params[1], 
             kPresets[i].params[2], kPresets[i].params[3],
             kPresets[i].params[4], kPresets[i].params[5],
             kPresets[i].params[6], kPresets[i].params[12]);
    }
    return 0;
  }
  
  if (cmd == "--generate-impulse") {
    if (argc < 3) {
      fprintf(stderr, "Error: Missing output filename\n");
      return 1;
    }
    if (WavFile::GenerateImpulse(argv[2])) {
      printf("Generated impulse: %s\n", argv[2]);
      return 0;
    }
    return 1;
  }
  
  if (cmd == "--generate-sine") {
    if (argc < 3) {
      fprintf(stderr, "Error: Missing output filename\n");
      return 1;
    }
    float freq = 440.0f;
    if (argc >= 4) {
      freq = static_cast<float>(atof(argv[3]));
    }
    if (WavFile::GenerateSine(argv[2], freq)) {
      printf("Generated sine wave (%.1f Hz): %s\n", freq, argv[2]);
      return 0;
    }
    return 1;
  }
  
  if (cmd == "--generate-noise") {
    if (argc < 3) {
      fprintf(stderr, "Error: Missing output filename\n");
      return 1;
    }
    if (WavFile::GenerateNoise(argv[2])) {
      printf("Generated noise: %s\n", argv[2]);
      return 0;
    }
    return 1;
  }
  
  if (cmd == "--generate-drums") {
    if (argc < 3) {
      fprintf(stderr, "Error: Missing output filename\n");
      return 1;
    }
    if (WavFile::GenerateDrumPattern(argv[2])) {
      printf("Generated drum pattern: %s\n", argv[2]);
      return 0;
    }
    return 1;
  }
  
  if (cmd == "--help" || cmd == "-h") {
    PrintUsage(argv[0]);
    return 0;
  }

  // Process mode: need input and output files
  if (argc < 3) {
    fprintf(stderr, "Error: Missing input or output filename\n");
    PrintUsage(argv[0]);
    return 1;
  }
  
  const char* input_file = argv[1];
  const char* output_file = argv[2];
  
  // Default parameters (raw values as in preset arrays - 0-127 range)
  int dry_wet = 100;      // ~50%
  int reverb_time = 80;
  int diffusion = 80;
  int lp = 90;
  int input_gain = 50;
  int texture = 0;
  int grain_amt = 0;
  int grain_size = 64;
  int grain_dens = 64;
  int grain_pitch = 64;   // center = no shift
  int grain_pos = 64;
  int freeze = 0;
  int shift_amt = 0;
  int shift_pitch = 64;   // center = no shift
  int shift_size = 64;
  
  int preset_idx = -1;
  const char* preset_name = nullptr;
  
  // Parse options
  for (int i = 3; i < argc; i += 2) {
    if (i + 1 >= argc) {
      fprintf(stderr, "Error: Missing value for %s\n", argv[i]);
      return 1;
    }
    std::string opt = argv[i];
    
    if (opt == "--preset") {
      preset_idx = FindPreset(argv[i + 1]);
      if (preset_idx < 0) {
        fprintf(stderr, "Error: Unknown preset '%s'\n", argv[i + 1]);
        printf("Available presets: ");
        for (int j = 0; j < kNumPresets; ++j) {
          printf("%s%s", j > 0 ? ", " : "", kPresets[j].name);
        }
        printf("\n");
        return 1;
      }
      preset_name = kPresets[preset_idx].name;
      // Load preset values
      dry_wet = kPresets[preset_idx].params[0];
      reverb_time = kPresets[preset_idx].params[1];
      diffusion = kPresets[preset_idx].params[2];
      lp = kPresets[preset_idx].params[3];
      input_gain = kPresets[preset_idx].params[4];
      texture = kPresets[preset_idx].params[5];
      grain_amt = kPresets[preset_idx].params[6];
      grain_size = kPresets[preset_idx].params[7];
      grain_dens = kPresets[preset_idx].params[8];
      grain_pitch = kPresets[preset_idx].params[9];
      grain_pos = kPresets[preset_idx].params[10];
      freeze = kPresets[preset_idx].params[11];
      shift_amt = kPresets[preset_idx].params[12];
      shift_pitch = kPresets[preset_idx].params[13];
      shift_size = kPresets[preset_idx].params[14];
      continue;
    }
    
    int val = atoi(argv[i + 1]);
    
    // Map 0-100 UI values to 0-127 internal range where needed
    if (opt == "--dry-wet") dry_wet = val * 2;  // 0-100 -> 0-200
    else if (opt == "--time") reverb_time = val * 127 / 100;
    else if (opt == "--diffusion") diffusion = val * 127 / 100;
    else if (opt == "--lp") lp = val * 127 / 100;
    else if (opt == "--input-gain") input_gain = val * 127 / 100;
    else if (opt == "--texture") texture = val * 127 / 100;
    else if (opt == "--grain-amt") grain_amt = val * 127 / 100;
    else if (opt == "--grain-size") grain_size = val * 127 / 100;
    else if (opt == "--grain-dens") grain_dens = val * 127 / 100;
    else if (opt == "--grain-pitch") grain_pitch = val + 64;  // -24..24 -> 40..88
    else if (opt == "--shift-amt") shift_amt = val * 127 / 100;
    else if (opt == "--shift-pitch") shift_pitch = val + 64;  // -24..24 -> 40..88
    else {
      fprintf(stderr, "Unknown option: %s\n", argv[i]);
      return 1;
    }
  }
  
  // Open input file
  WavFile input;
  if (!input.OpenRead(input_file)) {
    return 1;
  }
  
  printf("Input: %s (%d Hz, %d channels, %zu frames)\n",
         input_file, input.sample_rate(), input.channels(), input.frames());
  
  // Validate sample rate
  if (input.sample_rate() != 48000) {
    fprintf(stderr, "Warning: Input is %d Hz, converting to 48000 Hz processing\n",
            input.sample_rate());
  }
  
  // Read all input audio
  std::vector<float> audio;
  if (!input.ReadAll(audio)) {
    fprintf(stderr, "Error reading input file\n");
    return 1;
  }
  input.Close();
  
  // Convert to stereo if mono
  std::vector<float> stereo_input;
  if (input.channels() == 1) {
    stereo_input.resize(input.frames() * 2);
    for (size_t i = 0; i < input.frames(); ++i) {
      stereo_input[i * 2 + 0] = audio[i];
      stereo_input[i * 2 + 1] = audio[i];
    }
  } else if (input.channels() == 2) {
    stereo_input = std::move(audio);
  } else {
    // Take first two channels
    stereo_input.resize(input.frames() * 2);
    for (size_t i = 0; i < input.frames(); ++i) {
      stereo_input[i * 2 + 0] = audio[i * input.channels() + 0];
      stereo_input[i * 2 + 1] = audio[i * input.channels() + 1];
    }
  }
  
  // Initialize the DSP processor
  CloudsFx fx;
  unit_runtime_desc_t desc = {};
  desc.samplerate = 48000;
  desc.frames_per_buffer = 64;
  desc.input_channels = 2;
  desc.output_channels = 2;
  
  int8_t err = fx.Init(&desc);
  if (err != k_unit_err_none) {
    fprintf(stderr, "Error initializing CloudsFx: %d\n", err);
    return 1;
  }
  
  // Set parameters - using raw values (0-127 range from presets)
  fx.setParameter(PARAM_DRY_WET, dry_wet);
  fx.setParameter(PARAM_TIME, reverb_time);
  fx.setParameter(PARAM_DIFFUSION, diffusion);
  fx.setParameter(PARAM_LP, lp);
  fx.setParameter(PARAM_INPUT_GAIN, input_gain);
  fx.setParameter(PARAM_TEXTURE, texture);
  fx.setParameter(PARAM_GRAIN_AMT, grain_amt);
  fx.setParameter(PARAM_GRAIN_SIZE, grain_size);
  fx.setParameter(PARAM_GRAIN_DENS, grain_dens);
  fx.setParameter(PARAM_GRAIN_PITCH, grain_pitch);
  fx.setParameter(PARAM_GRAIN_POS, grain_pos);
  fx.setParameter(PARAM_FREEZE, freeze);
  fx.setParameter(PARAM_SHIFT_AMT, shift_amt);
  fx.setParameter(PARAM_SHIFT_PITCH, shift_pitch);
  fx.setParameter(PARAM_SHIFT_SIZE, shift_size);
  
  if (preset_name) {
    printf("Using preset: %s (idx %d)\n", preset_name, preset_idx);
  }
  printf("Processing with:\n");
  printf("  dry-wet: %d, time: %d, diffusion: %d, lp: %d\n", 
         dry_wet, reverb_time, diffusion, lp);
  printf("  input-gain: %d, texture: %d\n", input_gain, texture);
  printf("  grain: amt=%d size=%d dens=%d pitch=%d\n", 
         grain_amt, grain_size, grain_dens, grain_pitch);
  printf("  shift: amt=%d pitch=%d size=%d\n", shift_amt, shift_pitch, shift_size);
  
  // Process audio in blocks
  std::vector<float> output(stereo_input.size());
  const size_t block_size = 64;
  size_t frames = stereo_input.size() / 2;
  
  for (size_t pos = 0; pos < frames; pos += block_size) {
    size_t this_block = std::min(block_size, frames - pos);
    fx.Process(&stereo_input[pos * 2], &output[pos * 2], 
               static_cast<uint32_t>(this_block), 2, 2);
  }
  
  // Add reverb tail by processing silence
  printf("Adding reverb tail...\n");
  size_t tail_frames = 48000 * 3;  // 3 seconds of tail
  std::vector<float> silence(block_size * 2, 0.0f);
  std::vector<float> tail(tail_frames * 2);
  
  for (size_t pos = 0; pos < tail_frames; pos += block_size) {
    size_t this_block = std::min(block_size, tail_frames - pos);
    fx.Process(silence.data(), &tail[pos * 2], 
               static_cast<uint32_t>(this_block), 2, 2);
  }
  
  // Write output
  WavFile wav_out;
  if (!wav_out.OpenWrite(output_file, 48000, 2)) {
    return 1;
  }
  
  wav_out.Write(output);
  wav_out.Write(tail);
  wav_out.Close();
  
  printf("Output: %s (%zu frames + %zu tail frames)\n", 
         output_file, frames, tail_frames);
  
  // Cleanup
  fx.Teardown();
  
  return 0;
}
