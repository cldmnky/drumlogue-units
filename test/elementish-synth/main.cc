// Test harness for elements-synth DSP code
// Compiles for local testing without drumlogue hardware

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <vector>
#include <string>
#include <cmath>
#include <chrono>
#include <algorithm>

#include "wav_file.h"

// Define TEST before including the DSP code to enable desktop-compatible implementations
#ifndef TEST
#define TEST
#endif

// Enable DSP profiling for test harness
// This enables timing instrumentation in DSP code
#ifndef DSP_PROFILE
#define DSP_PROFILE
#endif

// Include the stub unit.h
#include "unit.h"

// ============================================================================
// DSP Profiling Infrastructure
// ============================================================================
#ifdef DSP_PROFILE

struct DSPProfileStats {
  const char* name;
  uint64_t total_calls;
  double total_time_us;
  double min_time_us;
  double max_time_us;
  
  DSPProfileStats(const char* n = "unknown") 
    : name(n), total_calls(0), total_time_us(0), 
      min_time_us(1e12), max_time_us(0) {}
  
  void Record(double time_us) {
    total_calls++;
    total_time_us += time_us;
    if (time_us < min_time_us) min_time_us = time_us;
    if (time_us > max_time_us) max_time_us = time_us;
  }
  
  double AvgTimeUs() const {
    return total_calls > 0 ? total_time_us / total_calls : 0;
  }
};

// Global profiling stats
static DSPProfileStats g_profile_exciter("Exciter::Process");
static DSPProfileStats g_profile_resonator("Resonator::Process");
static DSPProfileStats g_profile_string("String::Process");
static DSPProfileStats g_profile_multistring("MultiString::Process");
static DSPProfileStats g_profile_render("ElementsSynth::Render");
static DSPProfileStats g_profile_filter("MoogLadder::Process");

// Helper class for automatic timing
class ScopedTimer {
public:
  ScopedTimer(DSPProfileStats& stats) : stats_(stats) {
    start_ = std::chrono::high_resolution_clock::now();
  }
  ~ScopedTimer() {
    auto end = std::chrono::high_resolution_clock::now();
    double us = std::chrono::duration<double, std::micro>(end - start_).count();
    stats_.Record(us);
  }
private:
  DSPProfileStats& stats_;
  std::chrono::high_resolution_clock::time_point start_;
};

// Macros for profiling DSP functions
#define DSP_PROFILE_SCOPE(stats) ScopedTimer _timer(stats)
#define DSP_PROFILE_EXCITER() DSP_PROFILE_SCOPE(g_profile_exciter)
#define DSP_PROFILE_RESONATOR() DSP_PROFILE_SCOPE(g_profile_resonator)
#define DSP_PROFILE_STRING() DSP_PROFILE_SCOPE(g_profile_string)
#define DSP_PROFILE_MULTISTRING() DSP_PROFILE_SCOPE(g_profile_multistring)
#define DSP_PROFILE_RENDER() DSP_PROFILE_SCOPE(g_profile_render)
#define DSP_PROFILE_FILTER() DSP_PROFILE_SCOPE(g_profile_filter)

void ResetProfileStats() {
  g_profile_exciter = DSPProfileStats("Exciter::Process");
  g_profile_resonator = DSPProfileStats("Resonator::Process");
  g_profile_string = DSPProfileStats("String::Process");
  g_profile_multistring = DSPProfileStats("MultiString::Process");
  g_profile_render = DSPProfileStats("ElementsSynth::Render");
  g_profile_filter = DSPProfileStats("MoogLadder::Process");
}

void PrintProfileStats() {
  printf("\n=== DSP Profile Statistics ===\n");
  printf("%-25s %10s %12s %12s %12s %12s\n", 
         "Function", "Calls", "Total(ms)", "Avg(us)", "Min(us)", "Max(us)");
  printf("%-25s %10s %12s %12s %12s %12s\n", 
         "------------------------", "----------", "------------", "------------", "------------", "------------");
  
  auto PrintRow = [](const DSPProfileStats& s) {
    if (s.total_calls > 0) {
      printf("%-25s %10lu %12.3f %12.3f %12.3f %12.3f\n",
             s.name, (unsigned long)s.total_calls, s.total_time_us / 1000.0,
             s.AvgTimeUs(), s.min_time_us, s.max_time_us);
    }
  };
  
  PrintRow(g_profile_render);
  PrintRow(g_profile_exciter);
  PrintRow(g_profile_resonator);
  PrintRow(g_profile_string);
  PrintRow(g_profile_multistring);
  PrintRow(g_profile_filter);
  
  // Calculate CPU load estimate (assuming 48kHz, 64-sample blocks)
  double samples_per_block = 64.0;
  double block_time_us = samples_per_block / 48000.0 * 1e6;  // ~1333us per block
  double avg_render_us = g_profile_render.AvgTimeUs();
  double cpu_load = (avg_render_us / block_time_us) * 100.0;
  
  printf("\n--- Performance Estimate ---\n");
  printf("Block time budget:    %.1f us (64 samples @ 48kHz)\n", block_time_us);
  printf("Average render time:  %.1f us\n", avg_render_us);
  printf("Estimated CPU load:   %.1f%%\n", cpu_load);
  if (cpu_load > 100) {
    printf("WARNING: Render time exceeds real-time budget!\n");
  }
}

#else
// No-op macros when profiling is disabled
#define DSP_PROFILE_EXCITER()
#define DSP_PROFILE_RESONATOR()
#define DSP_PROFILE_STRING()
#define DSP_PROFILE_MULTISTRING()
#define DSP_PROFILE_RENDER()
#define DSP_PROFILE_FILTER()
#define ResetProfileStats()
#define PrintProfileStats()
#endif // DSP_PROFILE

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
// Parameter layout differs between LIGHTWEIGHT and FULL modes:
//
// FULL MODE (24 params):
//   Page 1: BOW(0), BLOW(1), STRIKE(2), MALLET(3)
//   Page 2: BOW TIM(4), FLOW(5), STK MOD(6), DENSITY(7)
//   Page 3: GEOMETRY(8), BRIGHT(9), DAMPING(10), POSITION(11)
//   Page 4: CUTOFF(12), RESO(13), FLT ENV(14), MODEL(15)
//   Page 5: ATTACK(16), DECAY(17), RELEASE(18), CONTOUR(19)
//   Page 6: LFO RT(20), LFO DEP(21), LFO PRE(22), COARSE(23)
//
// LIGHTWEIGHT MODE (24 params, filter/LFO removed):
//   Page 1: BOW(0), BLOW(1), STRIKE(2), MALLET(3)
//   Page 2: BOW TIM(4), FLOW(5), STK MOD(6), DENSITY(7)
//   Page 3: GEOMETRY(8), BRIGHT(9), DAMPING(10), POSITION(11)
//   Page 4: MODEL(12), SPACE(13), VOLUME(14), blank(15)
//   Page 5: ATTACK(16), DECAY(17), RELEASE(18), CONTOUR(19)
//   Page 6: COARSE(20), FINE(21), blank(22), blank(23)

struct PresetDef {
  const char* name;
  int bow, blow, strike, mallet;         // Page 1: Exciter mix
  int bowT, flow, stkMode, granD;        // Page 2: Exciter timbre  
  int geo, bright, damp, pos;            // Page 3: Resonator
#ifdef ELEMENTS_LIGHTWEIGHT
  int model, space, volume;              // Page 4: Model & Output (lightweight)
  int atk, dec, rel, contour;            // Page 5: Envelope
  int coarse, fine;                      // Page 6: Tuning (lightweight)
#else
  int cutoff, reso, fltEnv, model;       // Page 4: Filter & Model (full)
  int atk, dec, rel, contour;            // Page 5: Envelope  
  int lfoRt, lfoDep, lfoPre, coarse;     // Page 6: LFO (full)
#endif
};

#ifdef ELEMENTS_LIGHTWEIGHT
// Presets for LIGHTWEIGHT mode (no filter/LFO, with SPACE/VOLUME/FINE)
static const PresetDef kPresets[] = {
  //           Exciter Mix          Exciter Timbre       Resonator            Model/Space/Vol   Envelope          Tuning
  //           bow  blo  str  mal   bowT flow stk  grn   geo  bri  dmp  pos   mod  spc  vol     atk  dec  rel  cnt   crs  fin
  {"INIT",     0,   0,  100,  0,    0,   0,   0,   0,    0,   0,   0,   0,    0,   70, 100,     5,  40,  40,   0,    0,   0},
  {"MARIMBA",  0,   0,  100,  0,    0,   0,   0,   0,   -20,  10, -10,  0,    0,   80, 100,     2,  30,  50,   0,    0,   0},
  {"VIBES",    0,   0,  100,  2,    0,   0,   0,   0,    10,  20, -30, 20,    0,   90, 100,     3,  60,  70,   0,    0,   0},
  {"PLUCK",    0,   0,  100,  6,    0,   0,   3,   0,    0,   0,  10,   0,    1,   70, 110,     1,  20,  30,   0,    0,   0},
  {"BOW",    100,   0,    0,  0,   20,   0,   0,   0,   -10,  30, -20, 10,    0,   80, 100,    30,  50,  80,   2,    0,   0},
  {"FLUTE",    0, 100,    0,  0,    0,  30,   0,   0,    20,  20, -10,  0,    0,   70, 100,    10,  40,  60,   2,    0,   0},
  {"STRING",   0,   0,  100,  6,    0,   0,   3,   0,    0,   10, -20,  0,    1,   60, 115,     5,  50,  80,   0,    0,   0},
  {"MSTRING",  0,   0,  100,  0,    0,   0,   0,   0,    0,   20, -10,  0,    2,   50, 120,     5,  60,  90,   0,    0,   0},
};
#else
// Presets for FULL mode (with filter and LFO)
static const PresetDef kPresets[] = {
  //           Exciter Mix          Exciter Timbre       Resonator            Filter/Model      Envelope          LFO
  //           bow  blo  str  mal   bowT flow stk  grn   geo  bri  dmp  pos   cut  res  flt  mod  atk  dec  rel  cnt   lfoR lfoD lfoP crs
  {"INIT",     0,   0,  100,  0,    0,   0,   0,   0,    0,   0,   0,   0,   127,   0,  64,  0,    5,  40,  40,   0,   40,   0,   0,   0},
  {"MARIMBA",  0,   0,  100,  0,    0,   0,   0,   0,   -20,  10, -10,  0,   100,   0,  80,  0,    2,  30,  50,   0,   40,   0,   0,   0},
  {"VIBES",    0,   0,  100,  2,    0,   0,   0,   0,    10,  20, -30, 20,   127,  10,  60,  0,    3,  60,  70,   0,   40,   0,   0,   0},
  {"PLUCK",    0,   0,  100,  6,    0,   0,   3,   0,    0,   0,  10,   0,    80,  20,  90,  1,    1,  20,  30,   0,   40,   0,   0,   0},
  {"BOW",    100,   0,    0,  0,   20,   0,   0,   0,   -10,  30, -20, 10,   100,  30,  40,  0,   30,  50,  80,   2,   40,   0,   0,   0},
  {"FLUTE",    0, 100,    0,  0,    0,  30,   0,   0,    20,  20, -10,  0,    90,  20,  50,  0,   10,  40,  60,   2,   40,   0,   0,   0},
  {"STRING",   0,   0,  100,  6,    0,   0,   3,   0,    0,   10, -20,  0,   127,  10,  70,  1,    5,  50,  80,   0,   40,   0,   0,   0},
  {"MSTRING",  0,   0,  100,  0,    0,   0,   0,   0,    0,   20, -10,  0,   127,   0,  60,  2,    5,  60,  90,   0,   40,   0,   0,   0},
};
#endif
static const int kNumPresets = sizeof(kPresets) / sizeof(kPresets[0]);

void PrintUsage(const char* program) {
  printf("Elements Synth Test Harness\n\n");
  printf("Usage: %s <output.wav> [options]\n", program);
  printf("       %s --list-presets\n", program);
  printf("       %s --compare-modes [options]      Compare all resonator models\n", program);
  printf("       %s --multi-note <notes> [options] Compare models across notes\n", program);
  printf("       %s --seq-test <prefix>            Run Marbles sequencer test suite\n", program);
  printf("       %s --output <file.wav> --analyze  (generate and analyze)\n", program);
  printf("\nGeneral Options:\n");
  printf("  --preset <name|num>   Use a preset (0-7 or name like MARIMBA, PLUCK)\n");
  printf("  --note <0-127>        MIDI note number (default: 60 = C4)\n");
  printf("  --velocity <1-127>    Note velocity (default: 100)\n");
  printf("  --duration <seconds>  Duration in seconds (default: 2.0)\n");
  printf("  --notes <n1,n2,...>   Play a sequence of notes\n");
  printf("  --analyze             Analyze output for issues (NaN, clipping, etc)\n");
  printf("  --verbose             Show detailed waveform analysis\n");
  printf("  --profile             Show DSP profiling statistics\n");
  printf("\nMode Comparison Options:\n");
  printf("  --compare-modes       Compare MODAL, STRING, MSTRING resonator modes\n");
  printf("  --save-comparison     Save individual WAV files for each mode\n");
  printf("  --multi-note <notes>  Compare modes across multiple notes (e.g., 36,48,60,72,84)\n");
#ifdef ELEMENTS_LIGHTWEIGHT
  printf("\nMarbles Sequencer Options (LIGHTWEIGHT mode only):\n");
  printf("  --seq-test <prefix>   Run sequencer test suite (steady quarter notes)\n");
  printf("  --pattern-test <pfx>  Run pattern+sequencer test suite (various rhythms)\n");
  printf("  --seq <0-15>          Set SEQ preset for single file output\n");
  printf("  --spread <0-127>      Set SPREAD parameter\n");
  printf("  --dejavu <0-127>      Set DEJA VU parameter\n");
  printf("  --bpm <tempo>         Set tempo in BPM (default: 120)\n");
  printf("  --bars <count>        Number of 4/4 bars (default: 4)\n");
  printf("\n  Patterns: four_floor, offbeat, sparse, sixteenths, swing, melodic, breakbeat, halftime\n");
  printf("  SEQ presets: OFF(0), SLOW(1), MED(2), FAST(3), X2(4), X4(5), MAJ(6), MIN(7), PENT(8)...\n");
#endif
  printf("\nParameter Options:\n");
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
  printf("  %s output.wav --preset VIBES --analyze --verbose\n", program);
  printf("  %s --compare-modes --note 60 --save-comparison\n", program);
  printf("  %s --multi-note 36,48,60,72,84 --velocity 100\n", program);
  printf("  %s output.wav --preset STRING --profile\n", program);
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
  
  // Page 1: Exciter Mix (same for both modes)
  synth.setParameter(0, p.bow);
  synth.setParameter(1, p.blow);
  synth.setParameter(2, p.strike);
  synth.setParameter(3, p.mallet);
  
  // Page 2: Exciter Timbre (same for both modes)
  synth.setParameter(4, p.bowT);
  synth.setParameter(5, p.flow);
  synth.setParameter(6, p.stkMode);
  synth.setParameter(7, p.granD);
  
  // Page 3: Resonator (same for both modes)
  synth.setParameter(8, p.geo);
  synth.setParameter(9, p.bright);
  synth.setParameter(10, p.damp);
  synth.setParameter(11, p.pos);
  
#ifdef ELEMENTS_LIGHTWEIGHT
  // Page 4: Model, Space, Volume (lightweight mode)
  synth.setParameter(12, p.model);
  synth.setParameter(13, p.space);
  synth.setParameter(14, p.volume);
  // 15 is blank
  
  // Page 5: Envelope (same for both modes)
  synth.setParameter(16, p.atk);
  synth.setParameter(17, p.dec);
  synth.setParameter(18, p.rel);
  synth.setParameter(19, p.contour);
  
  // Page 6: Tuning (lightweight mode)
  synth.setParameter(20, p.coarse);
  synth.setParameter(21, p.fine);
  // 22, 23 are blank
#else
  // Page 4: Filter & Model (full mode)
  synth.setParameter(12, p.cutoff);
  synth.setParameter(13, p.reso);
  synth.setParameter(14, p.fltEnv);
  synth.setParameter(15, p.model);
  
  // Page 5: Envelope (same for both modes)
  synth.setParameter(16, p.atk);
  synth.setParameter(17, p.dec);
  synth.setParameter(18, p.rel);
  synth.setParameter(19, p.contour);
  
  // Page 6: LFO (full mode)
  synth.setParameter(20, p.lfoRt);
  synth.setParameter(21, p.lfoDep);
  synth.setParameter(22, p.lfoPre);
  synth.setParameter(23, p.coarse);
#endif
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
  float zero_crossing_rate = 0.0f;
  float estimated_freq = 0.0f;
  std::vector<float> rms_timeline;  // RMS per 50ms window
};

AnalysisResult AnalyzeBuffer(const std::vector<float>& buffer, int sample_rate = 48000, int channels = 2) {
  AnalysisResult result;
  double sum_sq = 0.0;
  int stride = channels;
  size_t num_frames = buffer.size() / channels;
  
  for (size_t i = 0; i < num_frames; ++i) {
    float s = buffer[i * stride];  // Left channel only
    
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
  
  result.rms = std::sqrt(sum_sq / num_frames);
  
  // Zero crossing rate (first 1 second)
  size_t analyze_frames = std::min(num_frames, (size_t)sample_rate);
  int zero_crossings = 0;
  for (size_t i = 1; i < analyze_frames; i++) {
    float s0 = buffer[(i-1) * stride];
    float s1 = buffer[i * stride];
    if ((s0 >= 0 && s1 < 0) || (s0 < 0 && s1 >= 0)) {
      zero_crossings++;
    }
  }
  result.zero_crossing_rate = (float)zero_crossings / analyze_frames * sample_rate;
  result.estimated_freq = result.zero_crossing_rate / 2.0f;
  
  // RMS timeline (every 50ms window)
  int window_samples = 50 * sample_rate / 1000;  // 50ms
  for (size_t t = 0; t < num_frames; t += window_samples) {
    size_t end = std::min(t + window_samples, num_frames);
    double win_sum = 0.0;
    for (size_t i = t; i < end; i++) {
      float s = buffer[i * stride];
      if (!std::isnan(s) && !std::isinf(s)) {
        win_sum += s * s;
      }
    }
    result.rms_timeline.push_back(std::sqrt(win_sum / (end - t)));
  }
  
  return result;
}

void PrintAnalysis(const AnalysisResult& r, bool verbose = false) {
  printf("\n=== Audio Analysis ===\n");
  printf("Peak amplitude: %.4f (%.1f dB)\n", r.max_amplitude, 
         20.0 * std::log10(r.max_amplitude + 1e-10));
  printf("RMS level:      %.4f (%.1f dB)\n", r.rms,
         20.0 * std::log10(r.rms + 1e-10));
  printf("Zero crossings: %.0f/sec (estimated freq: ~%.0f Hz)\n",
         r.zero_crossing_rate, r.estimated_freq);
  
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
  
  // Print RMS waveform visualization
  if (verbose && !r.rms_timeline.empty()) {
    printf("\nRMS Envelope (50ms windows):\n");
    float max_rms = 0.0f;
    for (float v : r.rms_timeline) {
      if (v > max_rms) max_rms = v;
    }
    
    for (size_t i = 0; i < r.rms_timeline.size(); ++i) {
      int t_ms = i * 50;
      float rms = r.rms_timeline[i];
      int bars = (max_rms > 0) ? (int)(rms / max_rms * 50) : 0;
      
      printf("%5dms: %.4f |", t_ms, rms);
      for (int b = 0; b < bars; b++) printf("*");
      printf("\n");
    }
  }
}

// ============================================================================
// Mode Comparison Functionality
// ============================================================================

struct ModeComparisonResult {
  const char* mode_name;
  int model_id;
  AnalysisResult analysis;
  double render_time_ms;
  double avg_render_us;
  bool success;
};

// Generate audio for a specific model and analyze it
ModeComparisonResult GenerateAndAnalyzeMode(int model_id, int midi_note, int velocity,
                                            float duration_sec, int sample_rate = 48000) {
  ModeComparisonResult result;
  result.model_id = model_id;
  result.success = false;
  
  switch (model_id) {
    case 0: result.mode_name = "MODAL"; break;
    case 1: result.mode_name = "STRING"; break;
    case 2: result.mode_name = "MSTRING"; break;
    default: result.mode_name = "UNKNOWN"; return result;
  }
  
  // Initialize synth
  ElementsSynth synth;
  unit_runtime_desc_t runtime = {
    .target = 0,
    .api = 0,
    .samplerate = (uint32_t)sample_rate,
    .frames_per_buffer = 64,
    .input_channels = 0,
    .output_channels = 2,
    .padding = {0, 0}
  };
  
  if (synth.Init(&runtime) != k_unit_err_none) {
    return result;
  }
  
  // Set up a standard test configuration
  // Common parameters (same layout for both modes)
  synth.setParameter(0, 0);      // bow = 0
  synth.setParameter(1, 0);      // blow = 0
  synth.setParameter(2, 100);    // strike = 100
  synth.setParameter(3, 0);      // mallet = 0
  synth.setParameter(8, 0);      // geometry = center
  synth.setParameter(9, 0);      // brightness = center
  synth.setParameter(10, -20);   // damping = moderate
  synth.setParameter(11, 0);     // position = center
  
#ifdef ELEMENTS_LIGHTWEIGHT
  // Lightweight: MODEL at param 12, SPACE at 13, VOLUME at 14
  synth.setParameter(12, model_id);  // MODEL
  synth.setParameter(13, 70);        // space = 70%
  synth.setParameter(14, 100);       // volume = default
#else
  // Full: CUTOFF at 12, RESO at 13, FLT ENV at 14, MODEL at 15
  synth.setParameter(12, 127);       // cutoff = max
  synth.setParameter(13, 0);         // resonance = 0
  synth.setParameter(14, 64);        // filter env = 50%
  synth.setParameter(15, model_id);  // MODEL
#endif

  synth.setParameter(16, 5);     // attack = fast
  synth.setParameter(17, 50);    // decay = medium
  synth.setParameter(18, 60);    // release = medium
  
  // Reset profiling
  ResetProfileStats();
  
  const int block_size = 64;
  int total_samples = static_cast<int>(duration_sec * sample_rate);
  int gate_samples = static_cast<int>(duration_sec * 0.7f * sample_rate);  // 70% gate
  
  std::vector<float> output_buffer;
  std::vector<float> block_buffer(block_size * 2);
  
  auto start_time = std::chrono::high_resolution_clock::now();
  
  // Note on
  synth.NoteOn(midi_note, velocity);
  
  int samples_rendered = 0;
  bool note_off_sent = false;
  
  while (samples_rendered < total_samples) {
    if (!note_off_sent && samples_rendered >= gate_samples) {
      synth.NoteOff(midi_note);
      note_off_sent = true;
    }
    
    synth.Render(block_buffer.data(), block_size);
    output_buffer.insert(output_buffer.end(), block_buffer.begin(), block_buffer.end());
    samples_rendered += block_size;
  }
  
  auto end_time = std::chrono::high_resolution_clock::now();
  result.render_time_ms = std::chrono::duration<double, std::milli>(end_time - start_time).count();
  
#ifdef DSP_PROFILE
  result.avg_render_us = g_profile_render.AvgTimeUs();
#else
  result.avg_render_us = 0;
#endif
  
  // Analyze
  result.analysis = AnalyzeBuffer(output_buffer, sample_rate, 2);
  result.success = true;
  
  return result;
}

void RunModeComparison(int midi_note, int velocity, float duration_sec, 
                       bool save_files, const std::string& output_prefix) {
  printf("\n");
  printf("╔══════════════════════════════════════════════════════════════════════════════╗\n");
  printf("║                         RESONATOR MODE COMPARISON                            ║\n");
  printf("╠══════════════════════════════════════════════════════════════════════════════╣\n");
  printf("║  Note: MIDI %d | Velocity: %d | Duration: %.1fs                              ║\n", 
         midi_note, velocity, duration_sec);
  printf("╚══════════════════════════════════════════════════════════════════════════════╝\n");
  
  std::vector<ModeComparisonResult> results;
  
  // Test all three models
  for (int model = 0; model <= 2; ++model) {
    printf("\nGenerating %s mode...\n", 
           model == 0 ? "MODAL" : (model == 1 ? "STRING" : "MSTRING"));
    
    ModeComparisonResult r = GenerateAndAnalyzeMode(model, midi_note, velocity, duration_sec);
    results.push_back(r);
    
    if (!r.success) {
      printf("  ERROR: Failed to generate audio for model %d\n", model);
    } else {
      printf("  Peak: %.4f (%.1f dB), RMS: %.4f (%.1f dB)\n",
             r.analysis.max_amplitude, 
             20.0 * std::log10(r.analysis.max_amplitude + 1e-10),
             r.analysis.rms,
             20.0 * std::log10(r.analysis.rms + 1e-10));
    }
    
    // Save individual files if requested
    if (save_files && r.success) {
      // Re-generate and save (simpler than storing buffer)
      ElementsSynth synth;
      unit_runtime_desc_t runtime = {
        .target = 0, .api = 0, .samplerate = 48000,
        .frames_per_buffer = 64, .input_channels = 0, .output_channels = 2,
        .padding = {0, 0}
      };
      synth.Init(&runtime);
      synth.setParameter(2, 100);    // strike
      synth.setParameter(10, -20);   // damping
#ifdef ELEMENTS_LIGHTWEIGHT
      synth.setParameter(12, model); // MODEL (lightweight)
      synth.setParameter(13, 70);    // space
      synth.setParameter(14, 100);   // volume
#else
      synth.setParameter(12, 127);   // cutoff (full)
      synth.setParameter(15, model); // MODEL (full)
#endif
      synth.setParameter(16, 5);
      synth.setParameter(17, 50);
      synth.setParameter(18, 60);
      
      std::vector<float> buf;
      std::vector<float> block(128);
      synth.NoteOn(midi_note, velocity);
      int samples = static_cast<int>(duration_sec * 48000);
      int gate = static_cast<int>(duration_sec * 0.7f * 48000);
      int rendered = 0;
      bool note_off = false;
      while (rendered < samples) {
        if (!note_off && rendered >= gate) {
          synth.NoteOff(midi_note);
          note_off = true;
        }
        synth.Render(block.data(), 64);
        buf.insert(buf.end(), block.begin(), block.end());
        rendered += 64;
      }
      
      std::string filename = output_prefix + "_" + r.mode_name + ".wav";
      WavFile wav;
      if (wav.OpenWrite(filename, 48000, 2)) {
        wav.Write(buf);
        wav.Close();
        printf("  Saved: %s\n", filename.c_str());
      }
    }
  }
  
  // Print comparison table
  printf("\n┌──────────────────────────────────────────────────────────────────────────────┐\n");
  printf("│                            COMPARISON RESULTS                                │\n");
  printf("├──────────┬────────────┬────────────┬────────────┬────────────┬───────────────┤\n");
  printf("│ Mode     │ Peak (dB)  │ RMS (dB)   │ Est. Freq  │ Render/blk │ Status        │\n");
  printf("├──────────┼────────────┼────────────┼────────────┼────────────┼───────────────┤\n");
  
  for (const auto& r : results) {
    if (r.success) {
      float peak_db = 20.0f * std::log10(r.analysis.max_amplitude + 1e-10f);
      float rms_db = 20.0f * std::log10(r.analysis.rms + 1e-10f);
      const char* status = "OK";
      if (r.analysis.has_nan) status = "NaN ERROR";
      else if (r.analysis.has_inf) status = "Inf ERROR";
      else if (r.analysis.has_clipping) status = "CLIPPING";
      else if (r.analysis.rms < 0.01f) status = "TOO QUIET";
      
      printf("│ %-8s │ %+7.1f dB │ %+7.1f dB │ %7.0f Hz │ %7.1f us │ %-13s │\n",
             r.mode_name, peak_db, rms_db, r.analysis.estimated_freq,
             r.avg_render_us, status);
    } else {
      printf("│ %-8s │    FAILED  │    FAILED  │    FAILED  │    FAILED  │ INIT ERROR    │\n",
             r.mode_name);
    }
  }
  printf("└──────────┴────────────┴────────────┴────────────┴────────────┴───────────────┘\n");
  
  // Level balance check
  if (results.size() >= 3 && results[0].success && results[1].success && results[2].success) {
    float modal_rms = results[0].analysis.rms;
    float string_rms = results[1].analysis.rms;
    float mstring_rms = results[2].analysis.rms;
    
    float max_rms = std::max({modal_rms, string_rms, mstring_rms});
    float min_rms = std::min({modal_rms, string_rms, mstring_rms});
    float ratio_db = 20.0f * std::log10(max_rms / (min_rms + 1e-10f));
    
    printf("\n=== Level Balance Analysis ===\n");
    printf("RMS ratio (loudest/quietest): %.1f dB\n", ratio_db);
    
    if (ratio_db < 3.0f) {
      printf("Status: GOOD - Modes are well balanced (< 3dB difference)\n");
    } else if (ratio_db < 6.0f) {
      printf("Status: ACCEPTABLE - Modes have moderate level difference (3-6dB)\n");
    } else {
      printf("Status: WARNING - Modes have significant level difference (> 6dB)\n");
      printf("        Consider adjusting output gains for better balance.\n");
    }
    
    // Identify which modes need adjustment
    if (ratio_db >= 3.0f) {
      printf("\nPer-mode adjustments needed to match MODAL level:\n");
      printf("  MODAL:   %+.1f dB (reference)\n", 0.0f);
      printf("  STRING:  %+.1f dB\n", 20.0f * std::log10(modal_rms / (string_rms + 1e-10f)));
      printf("  MSTRING: %+.1f dB\n", 20.0f * std::log10(modal_rms / (mstring_rms + 1e-10f)));
    }
  }
  
  // Performance summary
#ifdef DSP_PROFILE
  printf("\n=== Performance Summary ===\n");
  double block_budget_us = 64.0 / 48000.0 * 1e6;  // ~1333us
  printf("Real-time budget: %.0f us per 64-sample block\n", block_budget_us);
  for (const auto& r : results) {
    if (r.success && r.avg_render_us > 0) {
      double load = (r.avg_render_us / block_budget_us) * 100.0;
      printf("  %-8s: %.1f us avg (%.1f%% CPU)\n", r.mode_name, r.avg_render_us, load);
    }
  }
#endif
}

// Test multiple notes for each mode
void RunMultiNoteComparison(const std::vector<int>& notes, int velocity, float note_duration) {
  printf("\n");
  printf("╔══════════════════════════════════════════════════════════════════════════════╗\n");
  printf("║                      MULTI-NOTE MODE COMPARISON                              ║\n");
  printf("╚══════════════════════════════════════════════════════════════════════════════╝\n");
  
  printf("\n%-8s", "Note");
  for (int model = 0; model <= 2; ++model) {
    const char* name = model == 0 ? "MODAL" : (model == 1 ? "STRING" : "MSTRING");
    printf(" │ %-12s", name);
  }
  printf("\n");
  printf("────────");
  for (int i = 0; i < 3; ++i) printf("─┼──────────────");
  printf("\n");
  
  for (int note : notes) {
    printf("MIDI %-3d", note);
    
    for (int model = 0; model <= 2; ++model) {
      auto r = GenerateAndAnalyzeMode(model, note, velocity, note_duration);
      if (r.success) {
        float rms_db = 20.0f * std::log10(r.analysis.rms + 1e-10f);
        const char* flag = "";
        if (r.analysis.has_nan) flag = " NaN!";
        else if (r.analysis.has_inf) flag = " Inf!";
        else if (r.analysis.has_clipping) flag = " CLIP";
        printf(" │ %+6.1f dB%s", rms_db, flag);
      } else {
        printf(" │    ERROR    ");
      }
    }
    printf("\n");
  }
}

// ============================================================================
// Marbles Sequencer Test Functionality (ELEMENTS_LIGHTWEIGHT only)
// ============================================================================
#ifdef ELEMENTS_LIGHTWEIGHT

struct SequencerTestConfig {
  const char* name;
  int seq_preset;      // SEQ parameter (0-15)
  int spread;          // SPREAD parameter (0-127)
  int deja_vu;         // DEJA VU parameter (0-127)
  int base_note;       // Base MIDI note
  float bpm;           // Tempo in BPM
  int bars;            // Number of 4/4 bars
};

// Rhythmic pattern step: timing in 16th notes (0-15 per bar), note, velocity
struct PatternStep {
  int sixteenth;       // Position in 16ths (0-15 within bar, or 0-63 for 4 bars)
  int note;            // MIDI note
  int velocity;        // Velocity 1-127
  int gate_16ths;      // Gate length in 16th notes
};

// Pre-defined rhythmic patterns
struct RhythmPattern {
  const char* name;
  const char* description;
  std::vector<PatternStep> steps;
  float bpm;
};

// Create common rhythm patterns
static std::vector<RhythmPattern> GetRhythmPatterns() {
  return {
    // Basic 4-on-floor
    {"four_floor", "Four-on-the-floor kick pattern", {
      {0, 60, 100, 2}, {4, 60, 100, 2}, {8, 60, 100, 2}, {12, 60, 100, 2},  // Bar 1
      {16, 60, 100, 2}, {20, 60, 100, 2}, {24, 60, 100, 2}, {28, 60, 100, 2}, // Bar 2
      {32, 60, 100, 2}, {36, 60, 100, 2}, {40, 60, 100, 2}, {44, 60, 100, 2}, // Bar 3
      {48, 60, 100, 2}, {52, 60, 100, 2}, {56, 60, 100, 2}, {60, 60, 100, 2}, // Bar 4
    }, 120.0f},
    
    // Offbeat / syncopated
    {"offbeat", "Offbeat syncopated pattern", {
      {2, 60, 100, 2}, {6, 60, 80, 2}, {10, 60, 100, 2}, {14, 60, 80, 2},   // Bar 1
      {18, 60, 100, 2}, {22, 60, 80, 2}, {26, 60, 100, 2}, {30, 60, 80, 2}, // Bar 2
      {34, 60, 100, 2}, {38, 60, 80, 2}, {42, 60, 100, 2}, {46, 60, 80, 2}, // Bar 3
      {50, 60, 100, 2}, {54, 60, 80, 2}, {58, 60, 100, 2}, {62, 60, 80, 2}, // Bar 4
    }, 120.0f},
    
    // Sparse / minimal
    {"sparse", "Sparse minimal hits", {
      {0, 60, 110, 3},  {12, 60, 90, 2},                                      // Bar 1
      {20, 60, 100, 3}, {28, 60, 80, 2},                                      // Bar 2
      {32, 60, 110, 3}, {44, 60, 90, 2},                                      // Bar 3
      {52, 60, 100, 3}, {60, 60, 80, 2},                                      // Bar 4
    }, 100.0f},
    
    // Fast 16ths
    {"sixteenths", "Rapid 16th note pattern", {
      {0, 60, 100, 1}, {1, 62, 70, 1}, {2, 64, 80, 1}, {3, 60, 70, 1},
      {4, 60, 100, 1}, {5, 62, 70, 1}, {6, 64, 80, 1}, {7, 60, 70, 1},
      {8, 60, 100, 1}, {9, 62, 70, 1}, {10, 64, 80, 1}, {11, 60, 70, 1},
      {12, 60, 100, 1}, {13, 62, 70, 1}, {14, 64, 80, 1}, {15, 60, 70, 1},
      // Bar 2 same
      {16, 60, 100, 1}, {17, 62, 70, 1}, {18, 64, 80, 1}, {19, 60, 70, 1},
      {20, 60, 100, 1}, {21, 62, 70, 1}, {22, 64, 80, 1}, {23, 60, 70, 1},
      {24, 60, 100, 1}, {25, 62, 70, 1}, {26, 64, 80, 1}, {27, 60, 70, 1},
      {28, 60, 100, 1}, {29, 62, 70, 1}, {30, 64, 80, 1}, {31, 60, 70, 1},
    }, 110.0f},
    
    // Swing / triplet feel (approximated with 16ths)
    {"swing", "Swing/shuffle feel", {
      {0, 60, 100, 2}, {3, 60, 70, 1},   {4, 60, 90, 2}, {7, 60, 70, 1},
      {8, 60, 100, 2}, {11, 60, 70, 1},  {12, 60, 90, 2}, {15, 60, 70, 1},
      {16, 60, 100, 2}, {19, 60, 70, 1}, {20, 60, 90, 2}, {23, 60, 70, 1},
      {24, 60, 100, 2}, {27, 60, 70, 1}, {28, 60, 90, 2}, {31, 60, 70, 1},
      {32, 60, 100, 2}, {35, 60, 70, 1}, {36, 60, 90, 2}, {39, 60, 70, 1},
      {40, 60, 100, 2}, {43, 60, 70, 1}, {44, 60, 90, 2}, {47, 60, 70, 1},
      {48, 60, 100, 2}, {51, 60, 70, 1}, {52, 60, 90, 2}, {55, 60, 70, 1},
      {56, 60, 100, 2}, {59, 60, 70, 1}, {60, 60, 90, 2}, {63, 60, 70, 1},
    }, 95.0f},
    
    // Melodic phrase with different notes
    {"melodic", "Melodic phrase with pitch variation", {
      {0, 48, 100, 3},  {4, 55, 80, 2},  {8, 60, 90, 2},  {12, 55, 70, 2},   // Bar 1: C-G-C-G
      {16, 53, 100, 3}, {20, 60, 80, 2}, {24, 65, 90, 2}, {28, 60, 70, 2},   // Bar 2: F-C-F-C
      {32, 55, 100, 3}, {36, 60, 80, 2}, {40, 67, 90, 2}, {44, 60, 70, 2},   // Bar 3: G-C-G-C
      {48, 48, 110, 4}, {56, 60, 90, 4},                                      // Bar 4: C (long) - C
    }, 115.0f},
    
    // Breakbeat style
    {"breakbeat", "Breakbeat-style pattern", {
      {0, 60, 110, 2},  {6, 60, 90, 1},  {8, 60, 100, 2}, {10, 60, 70, 1}, {14, 60, 80, 1},
      {16, 60, 110, 2}, {22, 60, 90, 1}, {24, 60, 100, 2}, {26, 60, 70, 1}, {30, 60, 80, 1},
      {32, 60, 110, 2}, {38, 60, 90, 1}, {40, 60, 100, 2}, {42, 60, 70, 1}, {46, 60, 80, 1},
      {48, 60, 110, 2}, {54, 60, 90, 1}, {56, 60, 100, 2}, {58, 60, 70, 1}, {62, 60, 80, 1},
    }, 130.0f},
    
    // Half-time
    {"halftime", "Half-time feel", {
      {0, 48, 110, 4},  {8, 60, 90, 4},                                       // Bar 1
      {16, 48, 100, 4}, {24, 60, 80, 4},                                      // Bar 2
      {32, 48, 110, 4}, {40, 60, 90, 4},                                      // Bar 3
      {48, 48, 100, 6}, {56, 60, 90, 6},                                      // Bar 4
    }, 80.0f},
  };
}

// Test the Marbles sequencer with a simulated 4-bar pattern
void RunSequencerTest(const SequencerTestConfig& config, const std::string& output_path) {
  printf("\n=== Marbles Sequencer Test: %s ===\n", config.name);
  printf("SEQ=%d (%s), SPREAD=%d, DEJA_VU=%d, Base=%d, BPM=%.0f, Bars=%d\n",
         config.seq_preset, marbles::MarblesSequencer::GetPresetName(config.seq_preset),
         config.spread, config.deja_vu, config.base_note, config.bpm, config.bars);
  
  const int sample_rate = 48000;
  const int block_size = 64;
  
  // Calculate timing
  float beat_duration = 60.0f / config.bpm;  // seconds per beat
  int samples_per_beat = static_cast<int>(beat_duration * sample_rate);
  int total_beats = config.bars * 4;  // 4 beats per bar
  int total_samples = samples_per_beat * total_beats;
  
  printf("Beat duration: %.3fs (%d samples)\n", beat_duration, samples_per_beat);
  printf("Total duration: %.2fs\n", (float)total_samples / sample_rate);
  
  // Initialize synth
  ElementsSynth synth;
  unit_runtime_desc_t runtime = {
    .target = 0,
    .api = 0,
    .samplerate = (uint32_t)sample_rate,
    .frames_per_buffer = 64,
    .input_channels = 0,
    .output_channels = 2,
    .padding = {0, 0}
  };
  
  if (synth.Init(&runtime) != k_unit_err_none) {
    printf("ERROR: Failed to initialize synth\n");
    return;
  }
  
  // Set up a nice marimba-like sound for clear note articulation
  synth.setParameter(0, 0);      // bow = 0
  synth.setParameter(1, 0);      // blow = 0
  synth.setParameter(2, 100);    // strike = 100
  synth.setParameter(3, 0);      // mallet = soft
  synth.setParameter(8, -20);    // geometry = slightly stringy
  synth.setParameter(9, 10);     // brightness = slightly bright
  synth.setParameter(10, -20);   // damping = moderate sustain
  synth.setParameter(11, 0);     // position = center
  synth.setParameter(12, 0);     // model = MODAL
  synth.setParameter(13, 70);    // space = 70%
  synth.setParameter(14, 100);   // volume = 100%
  synth.setParameter(16, 2);     // attack = fast
  synth.setParameter(17, 40);    // decay = medium
  synth.setParameter(18, 50);    // release = medium
  synth.setParameter(19, 0);     // contour = ADR
  
  // Set sequencer parameters (Page 6)
  synth.setParameter(20, 0);                // coarse = 0
  synth.setParameter(21, config.seq_preset); // SEQ preset
  synth.setParameter(22, config.spread);     // SPREAD
  synth.setParameter(23, config.deja_vu);    // DEJA VU
  
  // Set tempo (16.16 fixed point)
  uint32_t tempo_fixed = static_cast<uint32_t>(config.bpm * 65536.0f);
  synth.SetTempo(tempo_fixed);
  
  // Generate audio
  std::vector<float> output_buffer;
  std::vector<float> block_buffer(block_size * 2);
  
  int samples_rendered = 0;
  int current_beat = 0;
  int next_beat_sample = 0;
  bool note_on = false;
  int note_off_sample = 0;
  
  // Track notes for visualization
  std::vector<std::pair<int, int>> note_events;  // (sample, note)
  
  printf("\nGenerating %d beats...\n", total_beats);
  
  while (samples_rendered < total_samples) {
    // Check for beat boundaries (pattern steps)
    if (samples_rendered >= next_beat_sample && current_beat < total_beats) {
      // Note off from previous beat
      if (note_on) {
        synth.NoteOff(config.base_note);
        note_on = false;
      }
      
      // Note on for this beat
      synth.NoteOn(config.base_note, 100);
      note_on = true;
      note_events.push_back({samples_rendered, config.base_note});
      
      // Schedule note off at 80% of beat
      note_off_sample = next_beat_sample + static_cast<int>(samples_per_beat * 0.8f);
      
      current_beat++;
      next_beat_sample = current_beat * samples_per_beat;
    }
    
    // Note off at 80% through beat
    if (note_on && samples_rendered >= note_off_sample) {
      synth.NoteOff(config.base_note);
      note_on = false;
    }
    
    // Render block
    synth.Render(block_buffer.data(), block_size);
    output_buffer.insert(output_buffer.end(), block_buffer.begin(), block_buffer.end());
    samples_rendered += block_size;
  }
  
  // Final note off
  if (note_on) {
    synth.NoteOff(config.base_note);
  }
  
  // Add release tail
  int release_samples = static_cast<int>(0.5f * sample_rate);
  for (int i = 0; i < release_samples; i += block_size) {
    synth.Render(block_buffer.data(), block_size);
    output_buffer.insert(output_buffer.end(), block_buffer.begin(), block_buffer.end());
  }
  
  // Analyze
  AnalysisResult analysis = AnalyzeBuffer(output_buffer, sample_rate, 2);
  
  printf("\n=== Audio Analysis ===\n");
  printf("Peak: %.4f (%.1f dB)\n", analysis.max_amplitude,
         20.0f * std::log10(analysis.max_amplitude + 1e-10f));
  printf("RMS:  %.4f (%.1f dB)\n", analysis.rms,
         20.0f * std::log10(analysis.rms + 1e-10f));
  if (analysis.has_nan) printf("WARNING: %d NaN samples!\n", analysis.nan_count);
  if (analysis.has_inf) printf("WARNING: %d Inf samples!\n", analysis.inf_count);
  if (analysis.has_clipping) printf("WARNING: %d clipping samples!\n", analysis.clip_count);
  
  // Write WAV file
  if (!output_path.empty()) {
    WavFile wav;
    if (wav.OpenWrite(output_path, sample_rate, 2)) {
      wav.Write(output_buffer);
      wav.Close();
      printf("\nSaved: %s (%.2fs)\n", output_path.c_str(), 
             (float)output_buffer.size() / 2 / sample_rate);
    }
  }
}

// Run a pattern-based test with actual rhythmic content
void RunPatternSequencerTest(const RhythmPattern& pattern, int seq_preset, int spread, int deja_vu,
                             const std::string& output_path) {
  printf("\n=== Pattern Test: %s + SEQ=%d (%s) ===\n", pattern.name,
         seq_preset, marbles::MarblesSequencer::GetPresetName(seq_preset));
  printf("Pattern: %s\n", pattern.description);
  printf("SEQ=%d, SPREAD=%d, DEJA_VU=%d, BPM=%.0f\n", seq_preset, spread, deja_vu, pattern.bpm);
  
  const int sample_rate = 48000;
  const int block_size = 64;
  const int bars = 4;
  
  // Calculate timing (16th note resolution)
  float beat_duration = 60.0f / pattern.bpm;
  float sixteenth_duration = beat_duration / 4.0f;
  int samples_per_16th = static_cast<int>(sixteenth_duration * sample_rate);
  int total_16ths = bars * 16;  // 16 sixteenths per bar
  int total_samples = samples_per_16th * total_16ths;
  
  printf("16th note: %.3fs (%d samples)\n", sixteenth_duration, samples_per_16th);
  printf("Total duration: %.2fs\n", (float)total_samples / sample_rate);
  
  // Initialize synth
  ElementsSynth synth;
  unit_runtime_desc_t runtime = {
    .target = 0,
    .api = 0,
    .samplerate = (uint32_t)sample_rate,
    .frames_per_buffer = 64,
    .input_channels = 0,
    .output_channels = 2,
    .padding = {0, 0}
  };
  
  if (synth.Init(&runtime) != k_unit_err_none) {
    printf("ERROR: Failed to initialize synth\n");
    return;
  }
  
  // Set up a nice marimba-like sound for clear note articulation
  synth.setParameter(0, 0);      // bow = 0
  synth.setParameter(1, 0);      // blow = 0
  synth.setParameter(2, 100);    // strike = 100
  synth.setParameter(3, 0);      // mallet = soft
  synth.setParameter(8, -20);    // geometry
  synth.setParameter(9, 10);     // brightness
  synth.setParameter(10, -20);   // damping
  synth.setParameter(11, 0);     // position
  synth.setParameter(12, 0);     // model = MODAL
  synth.setParameter(13, 70);    // space = 70%
  synth.setParameter(14, 100);   // volume = 100%
  synth.setParameter(16, 2);     // attack = fast
  synth.setParameter(17, 40);    // decay
  synth.setParameter(18, 50);    // release
  synth.setParameter(19, 0);     // contour = ADR
  
  // Set sequencer parameters (Page 6)
  synth.setParameter(20, 0);          // coarse = 0
  synth.setParameter(21, seq_preset); // SEQ preset
  synth.setParameter(22, spread);     // SPREAD
  synth.setParameter(23, deja_vu);    // DEJA VU
  
  // Set tempo
  uint32_t tempo_fixed = static_cast<uint32_t>(pattern.bpm * 65536.0f);
  synth.SetTempo(tempo_fixed);
  
  // Generate audio with pattern-based triggering
  std::vector<float> output_buffer;
  std::vector<float> block_buffer(block_size * 2);
  
  int samples_rendered = 0;
  size_t step_index = 0;
  int current_note = -1;
  int note_off_sample = -1;
  
  // Sort steps by time
  std::vector<PatternStep> sorted_steps = pattern.steps;
  std::sort(sorted_steps.begin(), sorted_steps.end(), 
            [](const PatternStep& a, const PatternStep& b) { return a.sixteenth < b.sixteenth; });
  
  printf("\nPattern steps:\n");
  for (const auto& step : sorted_steps) {
    float time_sec = step.sixteenth * sixteenth_duration;
    printf("  [%2d] t=%.3fs note=%d vel=%d gate=%d\n", 
           step.sixteenth, time_sec, step.note, step.velocity, step.gate_16ths);
  }
  printf("\nGenerating audio...\n");
  
  while (samples_rendered < total_samples) {
    int current_16th = samples_rendered / samples_per_16th;
    
    // Check for note on events
    while (step_index < sorted_steps.size() && 
           sorted_steps[step_index].sixteenth <= current_16th &&
           samples_rendered >= sorted_steps[step_index].sixteenth * samples_per_16th) {
      const auto& step = sorted_steps[step_index];
      
      // Note off previous if active
      if (current_note >= 0) {
        synth.NoteOff(current_note);
      }
      
      // Note on
      synth.NoteOn(step.note, step.velocity);
      current_note = step.note;
      
      // Schedule note off
      note_off_sample = (step.sixteenth + step.gate_16ths) * samples_per_16th;
      
      step_index++;
    }
    
    // Check for note off
    if (current_note >= 0 && samples_rendered >= note_off_sample) {
      synth.NoteOff(current_note);
      current_note = -1;
    }
    
    // Render block
    synth.Render(block_buffer.data(), block_size);
    output_buffer.insert(output_buffer.end(), block_buffer.begin(), block_buffer.end());
    samples_rendered += block_size;
  }
  
  // Final note off
  if (current_note >= 0) {
    synth.NoteOff(current_note);
  }
  
  // Add release tail
  int release_samples = static_cast<int>(0.5f * sample_rate);
  for (int i = 0; i < release_samples; i += block_size) {
    synth.Render(block_buffer.data(), block_size);
    output_buffer.insert(output_buffer.end(), block_buffer.begin(), block_buffer.end());
  }
  
  // Analyze
  AnalysisResult analysis = AnalyzeBuffer(output_buffer, sample_rate, 2);
  
  printf("\n=== Audio Analysis ===\n");
  printf("Peak: %.4f (%.1f dB)\n", analysis.max_amplitude,
         20.0f * std::log10(analysis.max_amplitude + 1e-10f));
  printf("RMS:  %.4f (%.1f dB)\n", analysis.rms,
         20.0f * std::log10(analysis.rms + 1e-10f));
  if (analysis.has_nan) printf("WARNING: %d NaN samples!\n", analysis.nan_count);
  if (analysis.has_inf) printf("WARNING: %d Inf samples!\n", analysis.inf_count);
  if (analysis.has_clipping) printf("WARNING: %d clipping samples!\n", analysis.clip_count);
  
  // Write WAV file
  if (!output_path.empty()) {
    WavFile wav;
    if (wav.OpenWrite(output_path, sample_rate, 2)) {
      wav.Write(output_buffer);
      wav.Close();
      printf("\nSaved: %s (%.2fs)\n", output_path.c_str(), 
             (float)output_buffer.size() / 2 / sample_rate);
    }
  }
}

// Run comprehensive pattern + sequencer test suite
void RunPatternTestSuite(const std::string& output_prefix) {
  printf("\n");
  printf("╔══════════════════════════════════════════════════════════════════════════════╗\n");
  printf("║               PATTERN + SEQUENCER COMPREHENSIVE TEST SUITE                   ║\n");
  printf("╚══════════════════════════════════════════════════════════════════════════════╝\n");
  
  auto patterns = GetRhythmPatterns();
  
  // Test configurations: {seq_preset, spread, deja_vu, suffix}
  struct SeqConfig {
    int preset;
    int spread;
    int deja_vu;
    const char* suffix;
  };
  
  std::vector<SeqConfig> seq_configs = {
    {0,  64,   0, "off"},          // SEQ OFF - hear original pattern
    {3,  64,   0, "fast_random"},  // FAST + random
    {3,  64, 127, "fast_locked"},  // FAST + locked loop
    {8,  50, 100, "pent_locked"},  // Pentatonic + mostly locked
    {6,  64,  80, "major"},        // Major scale
    {7,  64,  80, "minor"},        // Minor scale  
    {10, 80,   0, "octaves"},      // Octave jumps
    {4,  40, 127, "x2_locked"},    // X2 (32nds) + locked
  };
  
  int file_count = 0;
  
  for (const auto& pattern : patterns) {
    printf("\n────────────────────────────────────────────────────────────────────────────────\n");
    printf("Testing pattern: %s\n", pattern.name);
    printf("────────────────────────────────────────────────────────────────────────────────\n");
    
    for (const auto& seq : seq_configs) {
      std::string filename = output_prefix + "_" + pattern.name + "_" + seq.suffix + ".wav";
      RunPatternSequencerTest(pattern, seq.preset, seq.spread, seq.deja_vu, filename);
      file_count++;
    }
  }
  
  printf("\n╔══════════════════════════════════════════════════════════════════════════════╗\n");
  printf("║                        PATTERN TEST SUITE COMPLETE                           ║\n");
  printf("╚══════════════════════════════════════════════════════════════════════════════╝\n");
  printf("\nGenerated %d test files with prefix: %s\n", file_count, output_prefix.c_str());
  printf("Patterns: %zu | Sequencer configs: %zu\n", patterns.size(), seq_configs.size());
}

// Run a suite of sequencer tests
void RunSequencerTestSuite(const std::string& output_prefix) {
  printf("\n");
  printf("╔══════════════════════════════════════════════════════════════════════════════╗\n");
  printf("║                    MARBLES SEQUENCER TEST SUITE                              ║\n");
  printf("╚══════════════════════════════════════════════════════════════════════════════╝\n");
  
  std::vector<SequencerTestConfig> tests = {
    // Basic rate tests - same note, different subdivision rates
    {"seq_off",       0,  64, 0,   60, 120.0f, 4},  // SEQ=OFF (pass-through)
    {"seq_slow",      1,  64, 0,   60, 120.0f, 4},  // 1 note/beat
    {"seq_med",       2,  64, 0,   60, 120.0f, 4},  // 2 notes/beat
    {"seq_fast",      3,  64, 0,   60, 120.0f, 4},  // 4 notes/beat (16ths)
    {"seq_x2",        4,  64, 0,   60, 120.0f, 4},  // 8 notes/beat (32nds)
    
    // Scale tests - different musical scales
    {"seq_major",     6,  64, 0,   60, 120.0f, 4},  // Major scale
    {"seq_minor",     7,  64, 0,   60, 120.0f, 4},  // Minor scale
    {"seq_pent",      8,  64, 0,   60, 120.0f, 4},  // Pentatonic
    {"seq_octaves",  10,  80, 0,   60, 120.0f, 4},  // Octave jumps
    {"seq_fifths",   11,  80, 0,   60, 120.0f, 4},  // Fifths
    
    // Spread tests - narrow vs wide range
    {"spread_narrow", 3,  20, 0,   60, 120.0f, 4},  // Narrow range (±4 semi)
    {"spread_wide",   3, 127, 0,   60, 120.0f, 4},  // Wide range (±24 semi)
    
    // Déjà vu tests - randomness vs looping
    {"dejavu_random", 3,  64, 0,   60, 120.0f, 4},  // Pure random
    {"dejavu_50",     3,  64, 64,  60, 120.0f, 4},  // 50% loop probability
    {"dejavu_locked", 3,  64, 127, 60, 120.0f, 4},  // Locked 8-step loop
    
    // Musical patterns - combining features
    {"pattern_arp",   8,  50, 100, 48, 140.0f, 4},  // Pentatonic arp, locked
    {"pattern_bass", 10,  40, 127, 36, 100.0f, 4},  // Octave bass pattern
    {"pattern_lead",  6,  64, 80,  72, 130.0f, 4},  // Major scale lead
  };
  
  for (const auto& test : tests) {
    std::string filename = output_prefix + "_" + test.name + ".wav";
    RunSequencerTest(test, filename);
  }
  
  printf("\n=== Test Suite Complete ===\n");
  printf("Generated %zu test files with prefix: %s\n", tests.size(), output_prefix.c_str());
}

#endif // ELEMENTS_LIGHTWEIGHT

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
  bool verbose_mode = false;
  bool profile_mode = false;
  bool compare_modes = false;
  bool save_comparison = false;
  std::vector<int> multi_notes;
  
#ifdef ELEMENTS_LIGHTWEIGHT
  // Sequencer test parameters
  std::string seq_test_prefix;
  std::string pattern_test_prefix;
  int param_seq = -999;
  int param_spread = -999;
  int param_dejavu = -999;
  float param_bpm = 120.0f;
  int param_bars = 4;
#endif
  
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
    } else if (strcmp(argv[i], "--verbose") == 0) {
      verbose_mode = true;
    } else if (strcmp(argv[i], "--profile") == 0) {
      profile_mode = true;
    } else if (strcmp(argv[i], "--compare-modes") == 0) {
      compare_modes = true;
    } else if (strcmp(argv[i], "--save-comparison") == 0) {
      save_comparison = true;
    } else if (strcmp(argv[i], "--multi-note") == 0 && i + 1 < argc) {
      multi_notes = ParseNotes(argv[++i]);
#ifdef ELEMENTS_LIGHTWEIGHT
    } else if (strcmp(argv[i], "--seq-test") == 0 && i + 1 < argc) {
      seq_test_prefix = argv[++i];
    } else if (strcmp(argv[i], "--pattern-test") == 0 && i + 1 < argc) {
      pattern_test_prefix = argv[++i];
    } else if (strcmp(argv[i], "--seq") == 0 && i + 1 < argc) {
      param_seq = atoi(argv[++i]);
    } else if (strcmp(argv[i], "--spread") == 0 && i + 1 < argc) {
      param_spread = atoi(argv[++i]);
    } else if (strcmp(argv[i], "--dejavu") == 0 && i + 1 < argc) {
      param_dejavu = atoi(argv[++i]);
    } else if (strcmp(argv[i], "--bpm") == 0 && i + 1 < argc) {
      param_bpm = atof(argv[++i]);
    } else if (strcmp(argv[i], "--bars") == 0 && i + 1 < argc) {
      param_bars = atoi(argv[++i]);
#endif
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

  // Handle multi-note comparison mode
  if (!multi_notes.empty()) {
    RunMultiNoteComparison(multi_notes, velocity, duration);
    return 0;
  }

#ifdef ELEMENTS_LIGHTWEIGHT
  // Handle pattern test suite (comprehensive rhythmic patterns)
  if (!pattern_test_prefix.empty()) {
    RunPatternTestSuite(pattern_test_prefix);
    return 0;
  }
  
  // Handle sequencer test suite (steady quarter notes)
  if (!seq_test_prefix.empty()) {
    RunSequencerTestSuite(seq_test_prefix);
    return 0;
  }
  
  // Handle single sequencer test with custom params
  if (param_seq != -999) {
    SequencerTestConfig config = {
      "custom",
      param_seq,
      param_spread != -999 ? param_spread : 64,
      param_dejavu != -999 ? param_dejavu : 0,
      note,
      param_bpm,
      param_bars
    };
    RunSequencerTest(config, output_path);
    return 0;
  }
#endif

  // Handle compare-modes
  if (compare_modes) {
    std::string prefix = output_path.empty() ? "comparison" : output_path;
    // Remove .wav extension if present
    size_t ext_pos = prefix.rfind(".wav");
    if (ext_pos != std::string::npos) {
      prefix = prefix.substr(0, ext_pos);
    }
    RunModeComparison(note, velocity, duration, save_comparison, prefix);
    return 0;
  }

  if (output_path.empty()) {
    fprintf(stderr, "Error: No output file specified\n");
    PrintUsage(argv[0]);
    return 1;
  }

  // Reset profiling stats
  ResetProfileStats();

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

  // Analyze if requested (or always do basic analysis with verbose)
  if (analyze_mode || verbose_mode) {
    AnalysisResult analysis = AnalyzeBuffer(output_buffer, sample_rate, 2);
    PrintAnalysis(analysis, verbose_mode);
    
    if (analysis.has_nan || analysis.has_inf) {
      return 2;  // Error code for DSP problems
    }
  }

  // Print profiling stats if requested
  if (profile_mode) {
    PrintProfileStats();
  }

  return 0;
}
