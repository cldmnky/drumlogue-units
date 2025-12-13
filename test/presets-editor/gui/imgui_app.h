#pragma once

#include <stdint.h>
#include <string>
#include <vector>

struct ImGuiContext;
struct SDL_Window;
typedef void* SDL_GLContext;

#ifdef __cplusplus
extern "C" {
#endif

#include "../sdk/runtime_stubs.h"
#include "../core/unit_loader.h"
#include "../audio/audio_engine.h"
#include "../presets/preset_manager.h"

#ifdef __cplusplus
}
#endif

class ImGuiApp {
 public:
  ImGuiApp(const std::string& unit_path,
           uint32_t sample_rate,
           uint16_t frames_per_buffer,
           uint8_t channels);
  ~ImGuiApp();

  bool Init();
  void Run();

 private:
  bool init_runtime();
  bool init_sdl();
  bool init_imgui();
  void shutdown_audio();
  void shutdown();
  void render_ui();
  void render_presets_panel();
  void render_piano_roll();
  void save_current_preset();
  void load_preset(const preset_t* preset);

  std::string unit_path_;
  uint32_t sample_rate_;
  uint16_t frames_per_buffer_;
  uint8_t channels_;

  runtime_stub_state_t* runtime_state_;
  unit_loader_t* loader_;

  SDL_Window* window_;
  SDL_GLContext gl_context_;
  ImGuiContext* imgui_ctx_;

  bool running_;
  bool audio_running_;

  audio_engine_t* audio_engine_;
  std::vector<int> param_values_;
  
  preset_manager_t* preset_manager_;
  char new_preset_name_[64];
  
  // Piano roll state
  std::vector<bool> active_notes_;  // 128 notes (MIDI range)
  int octave_offset_;               // Octave shift for piano roll
  
  // Arpeggiator state
  bool arp_enabled_;
  float arp_bpm_;
  int arp_pattern_;                 // 0=up, 1=down, 2=up-down, 3=random
  int arp_division_;                // 0=1/4, 1=1/8, 2=1/16, 3=1/32
  bool arp_hold_;                   // Latch mode
  float arp_gate_length_;           // Note duration as % of step (10-100%)
  std::vector<uint8_t> arp_notes_;  // Notes to arpeggiate
  size_t arp_step_;
  double arp_last_step_time_;
  double arp_note_off_time_;        // Scheduled time for note off
  uint8_t arp_current_note_;
  
  void update_arpeggiator();
};
