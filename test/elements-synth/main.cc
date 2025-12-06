// Test harness for elements-synth DSP code
// Compiles for local testing without drumlogue hardware

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <vector>
#include <string>
#include <cmath>

#include "wav_file.h"

// Define TEST before including the DSP code to enable desktop-compatible implementations
#ifndef TEST
#define TEST
#endif

// Include the stub unit.h
#include "unit.h"

// Define the unit_header stub for desktop testing
const unit_header_t unit_header = {
  .header_size = sizeof(unit_header_t),
  .target = 0,
  .api = 0,
  .dev_id = 0x636C646D,  // "cldm"
  .unit_id = 0x02010000,
  .version = 0x00010000,  // v1.0.0
  .name = "TestElements",
  .num_presets = 8,
  .num_params = 24,
  .params = {
    // Page 1: Exciter Mix
    {0, 127, 0, 0, k_unit_param_type_percent, 0, 0, 0, "BOW"},
    {0, 127, 0, 0, k_unit_param_type_percent, 0, 0, 0, "BLOW"},
    {0, 127, 0, 100, k_unit_param_type_percent, 0, 0, 0, "STRIKE"},
    {0, 11, 0, 0, k_unit_param_type_strings, 0, 0, 0, "MALLET"},
    // Page 2: Exciter Timbre
    {-64, 63, 0, 0, k_unit_param_type_none, 0, 0, 0, "BOW TIM"},
    {-64, 63, 0, 0, k_unit_param_type_none, 0, 0, 0, "BLW TIM"},
    {0, 4, 0, 0, k_unit_param_type_strings, 0, 0, 0, "STK MOD"},
    {-64, 63, 0, 0, k_unit_param_type_none, 0, 0, 0, "DENSITY"},
    // Page 3: Resonator
    {-64, 63, 0, 0, k_unit_param_type_none, 0, 0, 0, "GEOMETRY"},
    {-64, 63, 0, 0, k_unit_param_type_none, 0, 0, 0, "BRIGHT"},
    {-64, 63, 0, 0, k_unit_param_type_none, 0, 0, 0, "DAMPING"},
    {-64, 63, 0, 0, k_unit_param_type_none, 0, 0, 0, "POSITION"},
    // Page 4: Filter
    {0, 127, 0, 127, k_unit_param_type_percent, 0, 0, 0, "CUTOFF"},
    {0, 127, 0, 0, k_unit_param_type_percent, 0, 0, 0, "RESO"},
    {0, 127, 0, 64, k_unit_param_type_percent, 0, 0, 0, "FLT ENV"},
    {0, 2, 0, 0, k_unit_param_type_strings, 0, 0, 0, "MODEL"},
    // Page 5: Envelope
    {0, 127, 0, 5, k_unit_param_type_percent, 0, 0, 0, "ATTACK"},
    {0, 127, 0, 40, k_unit_param_type_percent, 0, 0, 0, "DECAY"},
    {0, 127, 0, 40, k_unit_param_type_percent, 0, 0, 0, "RELEASE"},
    {0, 3, 0, 0, k_unit_param_type_strings, 0, 0, 0, "ENV MOD"},
    // Page 6: LFO
    {0, 127, 0, 40, k_unit_param_type_percent, 0, 0, 0, "LFO RT"},
    {0, 127, 0, 0, k_unit_param_type_percent, 0, 0, 0, "LFO DEP"},
    {0, 7, 0, 0, k_unit_param_type_strings, 0, 0, 0, "LFO PRE"},
    {-64, 63, 0, 0, k_unit_param_type_none, 0, 0, 0, "COARSE"},
  }
};

// Now include the actual DSP code
#include "elements_synth_v2.h"

// Preset definitions
struct PresetDef {
  const char* name;
  int bow, blow, strike, mallet;
  int bowT, blwT, stkMode, granD;
  int geo, bright, damp, pos;
  int cutoff, reso, fltEnv, model;
  int atk, dec, rel, envMode;
  int lfoRt, lfoDep, lfoPre, coarse;
};

static const PresetDef kPresets[] = {
  {"INIT",     0, 0, 100, 0,   0, 0, 0, 0,   0, 0, 0, 0,   127, 0, 64, 0,   5, 40, 40, 0,   40, 0, 0, 0},
  {"MARIMBA",  0, 0, 100, 0,   0, 0, 0, 0,   -20, 10, -10, 0,   100, 0, 80, 0,   2, 30, 50, 0,   40, 0, 0, 0},
  {"VIBES",    0, 0, 100, 2,   0, 0, 0, 0,   10, 20, -30, 20,   127, 10, 60, 0,   3, 60, 70, 0,   40, 0, 0, 0},
  {"PLUCK",    0, 0, 100, 6,   0, 0, 3, 0,   0, 0, 10, 0,   80, 20, 90, 1,   1, 20, 30, 0,   40, 0, 0, 0},
  {"BOW",      100, 0, 0, 0,   20, 0, 0, 0,   -10, 30, -20, 10,   100, 30, 40, 0,   30, 50, 80, 2,   40, 0, 0, 0},
  {"FLUTE",    0, 100, 0, 0,   0, 30, 0, 0,   20, 20, -10, 0,   90, 20, 50, 0,   10, 40, 60, 2,   40, 0, 0, 0},
  {"STRING",   0, 0, 100, 6,   0, 0, 3, 0,   0, 10, -20, 0,   127, 10, 70, 1,   5, 50, 80, 0,   40, 0, 0, 0},
  {"MSTRING",  0, 0, 100, 0,   0, 0, 0, 0,   0, 20, -10, 0,   127, 0, 60, 2,   5, 60, 90, 0,   40, 0, 0, 0},
};
static const int kNumPresets = sizeof(kPresets) / sizeof(kPresets[0]);

void PrintUsage(const char* program) {
  printf("Elements Synth Test Harness\n\n");
  printf("Usage: %s <output.wav> [options]\n", program);
  printf("       %s --list-presets\n", program);
  printf("       %s --analyze  (check for NaN/Inf in output)\n", program);
  printf("\nOptions:\n");
  printf("  --preset <name|num>   Use a preset (0-7 or name like MARIMBA, PLUCK)\n");
  printf("  --note <0-127>        MIDI note number (default: 60 = C4)\n");
  printf("  --velocity <1-127>    Note velocity (default: 100)\n");
  printf("  --duration <seconds>  Duration in seconds (default: 2.0)\n");
  printf("  --notes <n1,n2,...>   Play a sequence of notes\n");
  printf("  --bow <0-127>         Bow level\n");
  printf("  --blow <0-127>        Blow level\n");
  printf("  --strike <0-127>      Strike level\n");
  printf("  --mallet <0-11>       Mallet type\n");
  printf("  --geometry <-64 to 63>  Resonator geometry\n");
  printf("  --brightness <-64 to 63>  Resonator brightness\n");
  printf("  --damping <-64 to 63>   Resonator damping\n");
  printf("  --cutoff <0-127>      Filter cutoff\n");
  printf("  --resonance <0-127>   Filter resonance\n");
  printf("  --model <0-2>         Model (0=MODAL, 1=STRING, 2=MSTRING)\n");
  printf("  --attack <0-127>      Envelope attack\n");
  printf("  --decay <0-127>       Envelope decay\n");
  printf("  --release <0-127>     Envelope release\n");
  printf("\nPresets:\n");
  for (int i = 0; i < kNumPresets; ++i) {
    printf("  %d: %s\n", i, kPresets[i].name);
  }
  printf("\nExamples:\n");
  printf("  %s output.wav --preset MARIMBA --note 60\n", program);
  printf("  %s output.wav --bow 100 --model 0 --duration 3\n", program);
  printf("  %s output.wav --notes 60,64,67,72 --preset PLUCK\n", program);
}

int FindPreset(const char* name_or_num) {
  char* endptr;
  long num = strtol(name_or_num, &endptr, 10);
  if (*endptr == '\0' && num >= 0 && num < kNumPresets) {
    return static_cast<int>(num);
  }
  for (int i = 0; i < kNumPresets; ++i) {
    if (strcasecmp(name_or_num, kPresets[i].name) == 0) {
      return i;
    }
  }
  return -1;
}

void ApplyPreset(ElementsSynth& synth, int preset_idx) {
  if (preset_idx < 0 || preset_idx >= kNumPresets) return;
  const PresetDef& p = kPresets[preset_idx];
  
  synth.setParameter(0, p.bow);
  synth.setParameter(1, p.blow);
  synth.setParameter(2, p.strike);
  synth.setParameter(3, p.mallet);
  synth.setParameter(4, p.bowT);
  synth.setParameter(5, p.blwT);
  synth.setParameter(6, p.stkMode);
  synth.setParameter(7, p.granD);
  synth.setParameter(8, p.geo);
  synth.setParameter(9, p.bright);
  synth.setParameter(10, p.damp);
  synth.setParameter(11, p.pos);
  synth.setParameter(12, p.cutoff);
  synth.setParameter(13, p.reso);
  synth.setParameter(14, p.fltEnv);
  synth.setParameter(15, p.model);
  synth.setParameter(16, p.atk);
  synth.setParameter(17, p.dec);
  synth.setParameter(18, p.rel);
  synth.setParameter(19, p.envMode);
  synth.setParameter(20, p.lfoRt);
  synth.setParameter(21, p.lfoDep);
  synth.setParameter(22, p.lfoPre);
  synth.setParameter(23, p.coarse);
}

// Analyze output for problems
struct AnalysisResult {
  bool has_nan = false;
  bool has_inf = false;
  bool has_clipping = false;
  float max_amplitude = 0.0f;
  float rms = 0.0f;
  int nan_count = 0;
  int inf_count = 0;
  int clip_count = 0;
};

AnalysisResult AnalyzeBuffer(const std::vector<float>& buffer) {
  AnalysisResult result;
  double sum_sq = 0.0;
  
  for (size_t i = 0; i < buffer.size(); ++i) {
    float s = buffer[i];
    
    if (std::isnan(s)) {
      result.has_nan = true;
      result.nan_count++;
      continue;
    }
    if (std::isinf(s)) {
      result.has_inf = true;
      result.inf_count++;
      continue;
    }
    
    float abs_s = std::fabs(s);
    if (abs_s > result.max_amplitude) {
      result.max_amplitude = abs_s;
    }
    if (abs_s > 0.99f) {
      result.has_clipping = true;
      result.clip_count++;
    }
    sum_sq += s * s;
  }
  
  result.rms = std::sqrt(sum_sq / buffer.size());
  return result;
}

void PrintAnalysis(const AnalysisResult& r) {
  printf("\n=== Audio Analysis ===\n");
  printf("Max amplitude: %.4f (%.1f dB)\n", r.max_amplitude, 
         20.0 * std::log10(r.max_amplitude + 1e-10));
  printf("RMS: %.4f (%.1f dB)\n", r.rms,
         20.0 * std::log10(r.rms + 1e-10));
  
  if (r.has_nan) {
    printf("WARNING: %d NaN samples detected!\n", r.nan_count);
  }
  if (r.has_inf) {
    printf("WARNING: %d Inf samples detected!\n", r.inf_count);
  }
  if (r.has_clipping) {
    printf("WARNING: %d samples clipping (>0.99)!\n", r.clip_count);
  }
  if (!r.has_nan && !r.has_inf && !r.has_clipping) {
    printf("Status: OK - No issues detected\n");
  }
}

std::vector<int> ParseNotes(const char* notes_str) {
  std::vector<int> notes;
  std::string s(notes_str);
  size_t pos = 0;
  while (pos < s.length()) {
    size_t comma = s.find(',', pos);
    if (comma == std::string::npos) comma = s.length();
    int note = std::atoi(s.substr(pos, comma - pos).c_str());
    if (note >= 0 && note <= 127) {
      notes.push_back(note);
    }
    pos = comma + 1;
  }
  return notes;
}

int main(int argc, char* argv[]) {
  if (argc < 2) {
    PrintUsage(argv[0]);
    return 1;
  }

  // Check for special commands
  if (strcmp(argv[1], "--list-presets") == 0) {
    printf("Available presets:\n");
    for (int i = 0; i < kNumPresets; ++i) {
      printf("  %d: %s\n", i, kPresets[i].name);
    }
    return 0;
  }

  // Parse arguments
  std::string output_path;
  int preset_idx = -1;
  int note = 60;
  int velocity = 100;
  float duration = 2.0f;
  std::vector<int> notes;
  bool analyze_mode = false;
  
  // Individual parameters (use -999 as "not set")
  int param_bow = -999, param_blow = -999, param_strike = -999;
  int param_mallet = -999, param_geometry = -999, param_brightness = -999;
  int param_damping = -999, param_cutoff = -999, param_resonance = -999;
  int param_model = -999, param_attack = -999, param_decay = -999;
  int param_release = -999;
  
  for (int i = 1; i < argc; ++i) {
    if (strcmp(argv[i], "--preset") == 0 && i + 1 < argc) {
      preset_idx = FindPreset(argv[++i]);
      if (preset_idx < 0) {
        fprintf(stderr, "Unknown preset: %s\n", argv[i]);
        return 1;
      }
    } else if (strcmp(argv[i], "--note") == 0 && i + 1 < argc) {
      note = atoi(argv[++i]);
    } else if (strcmp(argv[i], "--velocity") == 0 && i + 1 < argc) {
      velocity = atoi(argv[++i]);
    } else if (strcmp(argv[i], "--duration") == 0 && i + 1 < argc) {
      duration = atof(argv[++i]);
    } else if (strcmp(argv[i], "--notes") == 0 && i + 1 < argc) {
      notes = ParseNotes(argv[++i]);
    } else if (strcmp(argv[i], "--analyze") == 0) {
      analyze_mode = true;
    } else if (strcmp(argv[i], "--bow") == 0 && i + 1 < argc) {
      param_bow = atoi(argv[++i]);
    } else if (strcmp(argv[i], "--blow") == 0 && i + 1 < argc) {
      param_blow = atoi(argv[++i]);
    } else if (strcmp(argv[i], "--strike") == 0 && i + 1 < argc) {
      param_strike = atoi(argv[++i]);
    } else if (strcmp(argv[i], "--mallet") == 0 && i + 1 < argc) {
      param_mallet = atoi(argv[++i]);
    } else if (strcmp(argv[i], "--geometry") == 0 && i + 1 < argc) {
      param_geometry = atoi(argv[++i]);
    } else if (strcmp(argv[i], "--brightness") == 0 && i + 1 < argc) {
      param_brightness = atoi(argv[++i]);
    } else if (strcmp(argv[i], "--damping") == 0 && i + 1 < argc) {
      param_damping = atoi(argv[++i]);
    } else if (strcmp(argv[i], "--cutoff") == 0 && i + 1 < argc) {
      param_cutoff = atoi(argv[++i]);
    } else if (strcmp(argv[i], "--resonance") == 0 && i + 1 < argc) {
      param_resonance = atoi(argv[++i]);
    } else if (strcmp(argv[i], "--model") == 0 && i + 1 < argc) {
      param_model = atoi(argv[++i]);
    } else if (strcmp(argv[i], "--attack") == 0 && i + 1 < argc) {
      param_attack = atoi(argv[++i]);
    } else if (strcmp(argv[i], "--decay") == 0 && i + 1 < argc) {
      param_decay = atoi(argv[++i]);
    } else if (strcmp(argv[i], "--release") == 0 && i + 1 < argc) {
      param_release = atoi(argv[++i]);
    } else if (argv[i][0] != '-') {
      output_path = argv[i];
    }
  }

  if (output_path.empty()) {
    fprintf(stderr, "Error: No output file specified\n");
    PrintUsage(argv[0]);
    return 1;
  }

  // Initialize synth
  ElementsSynth synth;
  unit_runtime_desc_t runtime = {
    .target = 0,
    .api = 0,
    .samplerate = 48000,
    .frames_per_buffer = 64,
    .input_channels = 0,
    .output_channels = 2,
    .padding = {0, 0}
  };
  
  if (synth.Init(&runtime) != k_unit_err_none) {
    fprintf(stderr, "Failed to initialize synth\n");
    return 1;
  }

  // Apply preset if specified
  if (preset_idx >= 0) {
    printf("Using preset: %s\n", kPresets[preset_idx].name);
    ApplyPreset(synth, preset_idx);
  }

  // Override with individual parameters
  if (param_bow != -999) synth.setParameter(0, param_bow);
  if (param_blow != -999) synth.setParameter(1, param_blow);
  if (param_strike != -999) synth.setParameter(2, param_strike);
  if (param_mallet != -999) synth.setParameter(3, param_mallet);
  if (param_geometry != -999) synth.setParameter(8, param_geometry);
  if (param_brightness != -999) synth.setParameter(9, param_brightness);
  if (param_damping != -999) synth.setParameter(10, param_damping);
  if (param_cutoff != -999) synth.setParameter(12, param_cutoff);
  if (param_resonance != -999) synth.setParameter(13, param_resonance);
  if (param_model != -999) synth.setParameter(15, param_model);
  if (param_attack != -999) synth.setParameter(16, param_attack);
  if (param_decay != -999) synth.setParameter(17, param_decay);
  if (param_release != -999) synth.setParameter(18, param_release);

  // Open output file
  WavFile wav;
  if (!wav.OpenWrite(output_path, 48000, 2)) {
    return 1;
  }

  const int sample_rate = 48000;
  const int block_size = 64;
  std::vector<float> output_buffer;
  std::vector<float> block_buffer(block_size * 2);

  // Generate notes
  if (notes.empty()) {
    notes.push_back(note);
  }

  float note_duration = duration / notes.size();
  int note_samples = static_cast<int>(note_duration * sample_rate);
  int release_samples = static_cast<int>(0.5f * sample_rate);  // 500ms tail

  for (size_t n = 0; n < notes.size(); ++n) {
    int current_note = notes[n];
    printf("Playing note %d (MIDI %d) for %.2fs\n", 
           static_cast<int>(n + 1), current_note, note_duration);

    // Note on
    synth.NoteOn(current_note, velocity);

    // Render note
    int samples_rendered = 0;
    bool note_off_sent = false;
    int gate_samples = static_cast<int>(note_duration * 0.8f * sample_rate);  // 80% gate

    while (samples_rendered < note_samples + (n == notes.size() - 1 ? release_samples : 0)) {
      // Send note off at 80% of note duration
      if (!note_off_sent && samples_rendered >= gate_samples) {
        synth.NoteOff(current_note);
        note_off_sent = true;
      }

      synth.Render(block_buffer.data(), block_size);
      output_buffer.insert(output_buffer.end(), block_buffer.begin(), block_buffer.end());
      samples_rendered += block_size;
    }
  }

  // Write to file
  wav.Write(output_buffer);
  wav.Close();

  printf("Wrote %zu frames to %s\n", output_buffer.size() / 2, output_path.c_str());

  // Analyze if requested
  if (analyze_mode) {
    AnalysisResult analysis = AnalyzeBuffer(output_buffer);
    PrintAnalysis(analysis);
    
    if (analysis.has_nan || analysis.has_inf) {
      return 2;  // Error code for DSP problems
    }
  }

  return 0;
}
