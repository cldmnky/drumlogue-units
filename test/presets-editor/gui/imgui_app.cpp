#include "imgui_app.h"

#include <SDL.h>
#include <SDL_opengl.h>

#include <cstdio>
#include <cstdlib>
#include <stdexcept>
#include <string>

#include "imgui.h"
#include "imgui_impl_opengl3.h"
#include "imgui_impl_sdl2.h"

namespace {
const char* kGlslVersion = "#version 150";  // GL 3.2 core

int clamp_int(int v, int lo, int hi) {
  if (v < lo) return lo;
  if (v > hi) return hi;
  return v;
}
}

ImGuiApp::ImGuiApp(const std::string& unit_path,
                   uint32_t sample_rate,
                   uint16_t frames_per_buffer,
                   uint8_t channels)
    : unit_path_(unit_path),
      sample_rate_(sample_rate),
      frames_per_buffer_(frames_per_buffer),
      channels_(channels),
      runtime_state_(nullptr),
      loader_(nullptr),
      window_(nullptr),
      gl_context_(nullptr),
      imgui_ctx_(nullptr),
      running_(false),
      audio_running_(false),
      audio_engine_(nullptr),
      preset_manager_(nullptr),
      octave_offset_(0),
      arp_enabled_(false),
      arp_bpm_(120.0f),
      arp_pattern_(0),
      arp_division_(2),
      arp_hold_(false),
      arp_gate_length_(80.0f),
      arp_step_(0),
      arp_last_step_time_(0.0),
      arp_note_off_time_(0.0),
      arp_current_note_(0) {
  memset(new_preset_name_, 0, sizeof(new_preset_name_));
  active_notes_.resize(128, false);
}

ImGuiApp::~ImGuiApp() { shutdown(); }

bool ImGuiApp::Init() {
  if (!init_runtime()) {
    return false;
  }
  if (!init_sdl()) {
    return false;
  }
  if (!init_imgui()) {
    return false;
  }
  running_ = true;
  return true;
}

bool ImGuiApp::init_runtime() {
  runtime_state_ = new runtime_stub_state_t();
  if (runtime_stubs_init(runtime_state_, sample_rate_, frames_per_buffer_, channels_) != 0) {
    fprintf(stderr, "runtime stubs init failed\n");
    return false;
  }

  loader_ = new unit_loader_t();
  if (unit_loader_open(unit_path_.c_str(), loader_) != 0) {
    fprintf(stderr, "unit load failed: %s\n", unit_path_.c_str());
    return false;
  }

  if (loader_->header && runtime_state_->runtime_desc) {
    runtime_state_->runtime_desc->target = loader_->header->target;
  }

  if (unit_loader_init(loader_, runtime_state_->runtime_desc) != 0) {
    fprintf(stderr, "unit_init failed\n");
    return false;
  }

  // Seed parameter cache from current values or init values
  param_values_.assign(loader_->header ? loader_->header->num_params : 0, 0);
  for (size_t i = 0; i < param_values_.size(); ++i) {
    if (loader_->unit_get_param_value) {
      param_values_[i] = loader_->unit_get_param_value(static_cast<uint8_t>(i));
    } else if (loader_->header) {
      param_values_[i] = loader_->header->params[i].init;
    }
  }
  
  // Create preset manager
  preset_manager_ = preset_manager_create("presets");
  if (preset_manager_) {
    preset_manager_scan(preset_manager_);
  }

  return true;
}

bool ImGuiApp::init_sdl() {
  if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_TIMER) != 0) {
    fprintf(stderr, "SDL init error: %s\n", SDL_GetError());
    return false;
  }

  SDL_GL_SetAttribute(SDL_GL_CONTEXT_FLAGS, 0);
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 2);
  SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);
  SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, 24);
  SDL_GL_SetAttribute(SDL_GL_STENCIL_SIZE, 8);

  window_ = SDL_CreateWindow("drumlogue presets editor",
                             SDL_WINDOWPOS_CENTERED,
                             SDL_WINDOWPOS_CENTERED,
                             900,
                             640,
                             SDL_WINDOW_OPENGL | SDL_WINDOW_RESIZABLE);
  if (!window_) {
    fprintf(stderr, "SDL window error: %s\n", SDL_GetError());
    return false;
  }

  gl_context_ = SDL_GL_CreateContext(window_);
  if (!gl_context_) {
    fprintf(stderr, "GL context error: %s\n", SDL_GetError());
    return false;
  }

  SDL_GL_SetSwapInterval(1);  // enable vsync
  return true;
}

bool ImGuiApp::init_imgui() {
  IMGUI_CHECKVERSION();
  imgui_ctx_ = ImGui::CreateContext();
  if (!imgui_ctx_) {
    fprintf(stderr, "ImGui context creation failed\n");
    return false;
  }
  ImGui::StyleColorsDark();

  if (!ImGui_ImplSDL2_InitForOpenGL(window_, gl_context_)) {
    fprintf(stderr, "ImGui SDL2 init failed\n");
    return false;
  }
  if (!ImGui_ImplOpenGL3_Init(kGlslVersion)) {
    fprintf(stderr, "ImGui OpenGL init failed\n");
    return false;
  }
  return true;
}

void ImGuiApp::shutdown_audio() {
#ifdef PORTAUDIO_PRESENT
  if (audio_engine_) {
    // Turn off all notes before stopping
    if (loader_ && loader_->unit_all_note_off) {
      loader_->unit_all_note_off();
    }
    for (size_t i = 0; i < active_notes_.size(); ++i) {
      active_notes_[i] = false;
    }
    
    // Reset arpeggiator
    arp_current_note_ = 0;
    arp_step_ = 0;
    arp_notes_.clear();
    
    audio_engine_stop(audio_engine_);
    audio_engine_destroy(audio_engine_);
    audio_engine_ = nullptr;
  }
#endif
  audio_running_ = false;
}

void ImGuiApp::shutdown() {
  shutdown_audio();

  if (preset_manager_) {
    preset_manager_destroy(preset_manager_);
    preset_manager_ = nullptr;
  }

  if (loader_) {
    unit_loader_close(loader_);
    delete loader_;
    loader_ = nullptr;
  }
  if (runtime_state_) {
    runtime_stubs_teardown(runtime_state_);
    delete runtime_state_;
    runtime_state_ = nullptr;
  }

  if (imgui_ctx_) {
    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplSDL2_Shutdown();
    ImGui::DestroyContext(imgui_ctx_);
    imgui_ctx_ = nullptr;
  }

  if (gl_context_) {
    SDL_GL_DeleteContext(gl_context_);
    gl_context_ = nullptr;
  }
  if (window_) {
    SDL_DestroyWindow(window_);
    window_ = nullptr;
  }
  SDL_Quit();
}

void ImGuiApp::render_ui() {
  const unit_header_t* hdr = loader_ ? loader_->header : nullptr;
  
  // Main menu bar
  if (ImGui::BeginMainMenuBar()) {
    if (ImGui::BeginMenu("File")) {
      if (ImGui::MenuItem("Load Unit...")) {}
      ImGui::Separator();
      if (ImGui::MenuItem("Save Preset...")) {
        save_current_preset();
      }
      ImGui::Separator();
      if (ImGui::MenuItem("Exit")) { running_ = false; }
      ImGui::EndMenu();
    }
    if (ImGui::BeginMenu("Audio")) {
#ifdef PORTAUDIO_PRESENT
      if (!audio_running_) {
        if (ImGui::MenuItem("Start Audio")) {
          // Synth units don't need input channels, effects do
          uint8_t in_ch = 0;  // No input for synths
          if (hdr && (hdr->target & UNIT_TARGET_MODULE_MASK) != k_unit_module_synth) {
            in_ch = channels_;  // Effects units process input
          }
          audio_config_t cfg = {sample_rate_, frames_per_buffer_, in_ch, channels_};
          audio_engine_ = audio_engine_create(&cfg, loader_, runtime_state_);
          if (audio_engine_ && audio_engine_start(audio_engine_) == 0) {
            audio_running_ = true;
          } else {
            shutdown_audio();
          }
        }
      } else {
        if (ImGui::MenuItem("Stop Audio")) {
          shutdown_audio();
        }
      }
#else
      ImGui::MenuItem("PortAudio not available", nullptr, false, false);
#endif
      ImGui::EndMenu();
    }
    
    ImGui::SameLine(ImGui::GetWindowWidth() - 200);
    if (hdr) {
      ImGui::Text("%s", hdr->name);
    }
#ifdef PORTAUDIO_PRESENT
    if (audio_running_) {
      ImGui::SameLine();
      ImGui::TextColored(ImVec4(0.2f, 1.0f, 0.2f, 1.0f), "● %.1f%%", audio_engine_cpu_load(audio_engine_));
    }
#endif
    ImGui::EndMainMenuBar();
  }

  // Parameters panel
  ImGui::SetNextWindowPos(ImVec2(10, 30), ImGuiCond_FirstUseEver);
  ImGui::SetNextWindowSize(ImVec2(450, 600), ImGuiCond_FirstUseEver);
  ImGui::Begin("Parameters", nullptr, ImGuiWindowFlags_NoCollapse);

  if (!hdr) {
    ImGui::TextDisabled("No unit loaded");
    ImGui::End();
    return;
  }

  // Group parameters by pages (4 params per page)
  const int params_per_page = 4;
  const int num_pages = (hdr->num_params + params_per_page - 1) / params_per_page;
  
  static int current_page = 0;
  
  // Page selector
  ImGui::Text("Page:");
  ImGui::SameLine();
  for (int page = 0; page < num_pages; ++page) {
    if (page > 0) ImGui::SameLine();
    if (ImGui::RadioButton(std::to_string(page + 1).c_str(), &current_page, page)) {
      // Page changed
    }
  }
  
  ImGui::Separator();
  
  // Display current page parameters
  const int start_param = current_page * params_per_page;
  const int end_param = std::min(start_param + params_per_page, static_cast<int>(hdr->num_params));
  
  for (int i = start_param; i < end_param && i < static_cast<int>(param_values_.size()); ++i) {
    const unit_param_t& p = hdr->params[i];
    
    // Skip blank/unused params (name is empty)
    if (p.name[0] == '\0') {
      continue;
    }
    
    int val = clamp_int(param_values_[i], p.min, p.max);
    
    ImGui::PushID(i);
    ImGui::Text("%s", p.name);
    
    bool changed = false;
    
    // String-based parameters use combo box
    if (p.type == k_unit_param_type_strings && loader_->unit_get_param_str_value) {
      const char* current_str = loader_->unit_get_param_str_value(static_cast<uint8_t>(i), val);
      if (ImGui::BeginCombo("##value", current_str ? current_str : "")) {
        for (int v = p.min; v <= p.max; ++v) {
          const char* str = loader_->unit_get_param_str_value(static_cast<uint8_t>(i), v);
          bool is_selected = (val == v);
          if (ImGui::Selectable(str ? str : "", is_selected)) {
            val = v;
            changed = true;
          }
          if (is_selected) {
            ImGui::SetItemDefaultFocus();
          }
        }
        ImGui::EndCombo();
      }
    } else {
      // Numeric slider with value display
      ImGui::SetNextItemWidth(-80);
      changed = ImGui::SliderInt("##slider", &val, p.min, p.max);
      ImGui::SameLine();
      ImGui::Text("%d", val);
    }
    
    if (changed) {
      val = clamp_int(val, p.min, p.max);
      param_values_[i] = val;
      unit_loader_set_param(loader_, static_cast<uint8_t>(i), val);
    }
    
    ImGui::PopID();
    ImGui::Spacing();
  }

  ImGui::End();
  
  // Presets panel
  render_presets_panel();
  
  // Piano roll (only for synth units)
  if (hdr && (hdr->target & UNIT_TARGET_MODULE_MASK) == k_unit_module_synth) {
    render_piano_roll();
  }
}

void ImGuiApp::render_piano_roll() {
  const unit_header_t* hdr = loader_ ? loader_->header : nullptr;
  if (!hdr || !loader_->unit_note_on || !loader_->unit_note_off) {
    return;
  }
  
  // Piano roll window at bottom
  ImGui::SetNextWindowPos(ImVec2(10, 650), ImGuiCond_FirstUseEver);
  ImGui::SetNextWindowSize(ImVec2(860, 200), ImGuiCond_FirstUseEver);
  ImGui::Begin("Piano Roll", nullptr, ImGuiWindowFlags_NoCollapse);
  
  // Arpeggiator controls
  ImGui::Checkbox("Arpeggiator", &arp_enabled_);
  ImGui::SameLine();
  ImGui::Checkbox("Hold", &arp_hold_);
  
  ImGui::SetNextItemWidth(100);
  ImGui::SliderFloat("BPM", &arp_bpm_, 40.0f, 240.0f, "%.0f");
  ImGui::SameLine();
  
  const char* divisions[] = {"1/4", "1/8", "1/16", "1/32"};
  ImGui::SetNextItemWidth(80);
  ImGui::Combo("Div", &arp_division_, divisions, 4);
  ImGui::SameLine();
  
  const char* patterns[] = {"Up", "Down", "Up-Down", "Random"};
  ImGui::SetNextItemWidth(100);
  ImGui::Combo("Pattern", &arp_pattern_, patterns, 4);
  ImGui::SameLine();
  
  ImGui::SetNextItemWidth(100);
  ImGui::SliderFloat("Gate", &arp_gate_length_, 10.0f, 100.0f, "%.0f%%");
  ImGui::SameLine();
  
  // Octave selector
  ImGui::Text("Octave:");
  ImGui::SameLine();
  if (ImGui::Button("-")) octave_offset_ = std::max(octave_offset_ - 1, -2);
  ImGui::SameLine();
  ImGui::Text("%+d (C%d-B%d)", octave_offset_, 4 + octave_offset_, 4 + octave_offset_);
  ImGui::SameLine();
  if (ImGui::Button("+")) octave_offset_ = std::min(octave_offset_ + 1, 3);
  
  // Update arpeggiator
  if (arp_enabled_) {
    update_arpeggiator();
  }
  
  ImGui::Separator();
  ImGui::Text("Keys: A-K=notes, W,E,T,Y,U=sharps, Z/X=octave");
  ImGui::Spacing();
  
  // Piano keyboard (white and black keys)
  const int base_note = 60 + (octave_offset_ * 12);  // Middle C + octave offset
  const float white_key_width = 50.0f;
  const float white_key_height = 100.0f;
  const float black_key_width = 30.0f;
  const float black_key_height = 60.0f;
  
  ImVec2 canvas_p0 = ImGui::GetCursorScreenPos();
  ImVec2 canvas_sz = ImVec2(white_key_width * 7 + 10, white_key_height + 10);
  ImDrawList* draw_list = ImGui::GetWindowDrawList();
  
  // Handle keyboard input for piano keys
  // Keyboard mapping for white keys (C to B)
  struct KeyMap { ImGuiKey key; int note_offset; };
  const KeyMap white_keys[] = {
    {ImGuiKey_A, 0},   // C
    {ImGuiKey_S, 2},   // D
    {ImGuiKey_D, 4},   // E
    {ImGuiKey_F, 5},   // F
    {ImGuiKey_G, 7},   // G
    {ImGuiKey_H, 9},   // A
    {ImGuiKey_J, 11},  // B
    {ImGuiKey_K, 12}   // C (next octave)
  };
  
  const KeyMap black_keys[] = {
    {ImGuiKey_W, 1},   // C#
    {ImGuiKey_E, 3},   // D#
    {ImGuiKey_T, 6},   // F#
    {ImGuiKey_Y, 8},   // G#
    {ImGuiKey_U, 10}   // A#
  };
  
  // Octave control with Z/X keys
  if (ImGui::IsKeyPressed(ImGuiKey_Z)) {
    octave_offset_ = std::max(octave_offset_ - 1, -2);
  }
  if (ImGui::IsKeyPressed(ImGuiKey_X)) {
    octave_offset_ = std::min(octave_offset_ + 1, 3);
  }
  
  // Process key presses/releases for notes
  int keyboard_base_note = 60 + (octave_offset_ * 12);  // Middle C + octave offset
  
  // White keys
  for (const auto& km : white_keys) {
    uint8_t note = keyboard_base_note + km.note_offset;
    if (note < 128) {
      bool was_down = active_notes_[note];
      bool is_down = ImGui::IsKeyDown(km.key);
      
      if (is_down && !was_down) {
        // Key pressed
        if (!arp_enabled_) {
          if (loader_ && loader_->unit_note_on) {
            loader_->unit_note_on(note, 100);  // Default velocity
          }
        }
        active_notes_[note] = true;
      } else if (!is_down && was_down) {
        // Key released
        if (!arp_enabled_) {
          // Only turn off note directly if arp is disabled
          if (loader_ && loader_->unit_note_off) {
            loader_->unit_note_off(note);
          }
          active_notes_[note] = false;
        } else if (!arp_hold_) {
          // In arp mode without hold, clear the note from active list
          active_notes_[note] = false;
        }
        // In arp + hold mode, keep note active
      }
    }
  }
  
  // Black keys
  for (const auto& km : black_keys) {
    uint8_t note = keyboard_base_note + km.note_offset;
    if (note < 128) {
      bool was_down = active_notes_[note];
      bool is_down = ImGui::IsKeyDown(km.key);
      
      if (is_down && !was_down) {
        // Key pressed
        if (!arp_enabled_) {
          if (loader_ && loader_->unit_note_on) {
            loader_->unit_note_on(note, 100);
          }
        }
        active_notes_[note] = true;
      } else if (!is_down && was_down) {
        // Key released
        if (!arp_enabled_) {
          // Only turn off note directly if arp is disabled
          if (loader_ && loader_->unit_note_off) {
            loader_->unit_note_off(note);
          }
          active_notes_[note] = false;
        } else if (!arp_hold_) {
          // In arp mode without hold, clear the note from active list
          active_notes_[note] = false;
        }
        // In arp + hold mode, keep note active
      }
    }
  }
  
  // Collect notes for arpeggiator (only when arp is enabled)
  if (arp_enabled_) {
    if (!arp_hold_) {
      // In non-hold mode, update notes from currently held keys
      arp_notes_.clear();
      for (int i = 0; i < 128; ++i) {
        if (active_notes_[i]) {
          arp_notes_.push_back(i);
        }
      }
    } else {
      // In hold mode, only add new notes, don't remove
      for (int i = 0; i < 128; ++i) {
        if (active_notes_[i]) {
          // Check if note is not already in the list
          bool found = false;
          for (auto note : arp_notes_) {
            if (note == i) {
              found = true;
              break;
            }
          }
          if (!found) {
            arp_notes_.push_back(i);
          }
        }
      }
    }
  } else {
    // When arp is disabled, clear the held notes
    arp_notes_.clear();
  }
  
  // Draw white keys first
  int white_key_index = 0;
  for (int i = 0; i < 12; ++i) {
    int note_in_scale = i % 12;
    bool is_black = (note_in_scale == 1 || note_in_scale == 3 || note_in_scale == 6 || 
                     note_in_scale == 8 || note_in_scale == 10);
    
    if (!is_black) {
      int note = base_note + i;
      if (note >= 0 && note < 128) {
        ImVec2 key_min(canvas_p0.x + white_key_index * white_key_width, canvas_p0.y);
        ImVec2 key_max(key_min.x + white_key_width - 2, key_min.y + white_key_height);
        
        bool is_active = active_notes_[note];
        ImU32 col = is_active ? IM_COL32(100, 150, 255, 255) : IM_COL32(255, 255, 255, 255);
        
        // Check mouse interaction
        if (ImGui::IsMouseHoveringRect(key_min, key_max)) {
          col = is_active ? IM_COL32(80, 130, 235, 255) : IM_COL32(230, 230, 230, 255);
          if (ImGui::IsMouseClicked(0)) {
            if (!arp_enabled_) {
              // Direct note triggering when arp is off
              loader_->unit_note_on(note, 100);
            }
            active_notes_[note] = true;
          }
        }
        
        if (is_active && ImGui::IsMouseReleased(0)) {
          if (!arp_enabled_) {
            // Direct note off when arp is off
            loader_->unit_note_off(note);
            active_notes_[note] = false;
          } else if (!arp_hold_) {
            // In arp mode without hold, clear the note
            active_notes_[note] = false;
          }
          // In arp + hold mode, keep note active
        }
        
        draw_list->AddRectFilled(key_min, key_max, col);
        draw_list->AddRect(key_min, key_max, IM_COL32(0, 0, 0, 255), 0.0f, 0, 2.0f);
        
        // Note label
        const char* note_names[] = {"C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B"};
        draw_list->AddText(ImVec2(key_min.x + 5, key_max.y - 20), IM_COL32(0, 0, 0, 255), note_names[i]);
      }
      white_key_index++;
    }
  }
  
  // Draw black keys on top
  white_key_index = 0;
  for (int i = 0; i < 12; ++i) {
    int note_in_scale = i % 12;
    bool is_black = (note_in_scale == 1 || note_in_scale == 3 || note_in_scale == 6 || 
                     note_in_scale == 8 || note_in_scale == 10);
    
    if (!is_black) {
      // Check if there's a black key after this white key
      int next_note = (i + 1) % 12;
      bool has_black_after = (next_note == 1 || next_note == 3 || next_note == 6 || 
                              next_note == 8 || next_note == 10);
      
      if (has_black_after && i < 11) {
        int note = base_note + i + 1;
        if (note >= 0 && note < 128) {
          ImVec2 key_min(canvas_p0.x + white_key_index * white_key_width + white_key_width - black_key_width/2, canvas_p0.y);
          ImVec2 key_max(key_min.x + black_key_width, key_min.y + black_key_height);
          
          bool is_active = active_notes_[note];
          ImU32 col = is_active ? IM_COL32(50, 100, 200, 255) : IM_COL32(0, 0, 0, 255);
          
          // Check mouse interaction
          if (ImGui::IsMouseHoveringRect(key_min, key_max)) {
            col = is_active ? IM_COL32(40, 80, 180, 255) : IM_COL32(50, 50, 50, 255);
            if (ImGui::IsMouseClicked(0)) {
              if (!arp_enabled_) {
                loader_->unit_note_on(note, 100);
              }
              active_notes_[note] = true;
            }
          }
          
          if (is_active && ImGui::IsMouseReleased(0)) {
            if (!arp_enabled_) {
              loader_->unit_note_off(note);
              active_notes_[note] = false;
            } else if (!arp_hold_) {
              // In arp mode without hold, clear the note
              active_notes_[note] = false;
            }
            // In arp + hold mode, keep note active
          }
          
          draw_list->AddRectFilled(key_min, key_max, col);
          draw_list->AddRect(key_min, key_max, IM_COL32(0, 0, 0, 255), 0.0f, 0, 1.0f);
        }
      }
      white_key_index++;
    }
  }
  
  // Reserve space for the keyboard
  ImGui::Dummy(canvas_sz);
  
  ImGui::Spacing();
  if (arp_enabled_ && !arp_notes_.empty()) {
    ImGui::Text("Arpeggiator active - %zu notes", arp_notes_.size());
  } else {
    ImGui::Text("Click and hold keys to play notes (velocity=100)");
  }
  
  ImGui::End();
}

void ImGuiApp::update_arpeggiator() {
  if (!loader_ || !loader_->unit_note_on || !loader_->unit_note_off) {
    return;
  }
  
  // In hold mode, keep playing even if no notes are held
  if (!arp_hold_) {
    // No notes to arpeggiate and not in hold mode
    if (arp_notes_.empty()) {
      if (arp_current_note_ != 0) {
        loader_->unit_note_off(arp_current_note_);
        arp_current_note_ = 0;
      }
      arp_step_ = 0;
      return;
    }
  } else {
    // In hold mode, if we had notes but now have none, keep the last set
    // Only clear if arp is disabled
  }
  
  if (arp_notes_.empty()) {
    return;  // Nothing to play
  }
  
  // Calculate step time based on BPM and division
  // Division: 0=1/4, 1=1/8, 2=1/16, 3=1/32
  float steps_per_beat = 1.0f;
  switch (arp_division_) {
    case 0: steps_per_beat = 1.0f; break;   // Quarter notes
    case 1: steps_per_beat = 2.0f; break;   // Eighth notes
    case 2: steps_per_beat = 4.0f; break;   // Sixteenth notes
    case 3: steps_per_beat = 8.0f; break;   // Thirty-second notes
  }
  
  double step_duration = 60.0 / (arp_bpm_ * steps_per_beat);
  double current_time = ImGui::GetTime();
  
  // Check if it's time to turn off the current note (based on gate length)
  if (arp_current_note_ != 0 && current_time >= arp_note_off_time_) {
    loader_->unit_note_off(arp_current_note_);
    arp_current_note_ = 0;
  }
  
  // Check if it's time for the next step
  if (current_time - arp_last_step_time_ >= step_duration) {
    // Turn off previous note if still on
    if (arp_current_note_ != 0) {
      loader_->unit_note_off(arp_current_note_);
      arp_current_note_ = 0;
    }
    
    // Determine next note based on pattern
    size_t note_index = 0;
    switch (arp_pattern_) {
      case 0:  // Up
        note_index = arp_step_ % arp_notes_.size();
        break;
        
      case 1:  // Down
        note_index = (arp_notes_.size() - 1) - (arp_step_ % arp_notes_.size());
        break;
        
      case 2:  // Up-Down
        {
          size_t cycle_len = (arp_notes_.size() - 1) * 2;
          if (cycle_len == 0) cycle_len = 1;
          size_t pos = arp_step_ % cycle_len;
          if (pos < arp_notes_.size()) {
            note_index = pos;
          } else {
            note_index = cycle_len - pos;
          }
        }
        break;
        
      case 3:  // Random
        note_index = rand() % arp_notes_.size();
        break;
    }
    
    // Play new note
    arp_current_note_ = arp_notes_[note_index];
    loader_->unit_note_on(arp_current_note_, 100);
    
    // Schedule note off based on gate length
    double gate_duration = step_duration * (arp_gate_length_ / 100.0);
    arp_note_off_time_ = current_time + gate_duration;
    
    arp_step_++;
    arp_last_step_time_ = current_time;
  }
}

void ImGuiApp::render_presets_panel() {
  ImGui::SetNextWindowPos(ImVec2(470, 30), ImGuiCond_FirstUseEver);
  ImGui::SetNextWindowSize(ImVec2(400, 600), ImGuiCond_FirstUseEver);
  ImGui::Begin("Presets", nullptr, ImGuiWindowFlags_NoCollapse);
  
  const unit_header_t* hdr = loader_ ? loader_->header : nullptr;
  if (!hdr || !preset_manager_) {
    ImGui::TextDisabled("No unit loaded");
    ImGui::End();
    return;
  }
  
  // Save new preset
  ImGui::Text("Save Current State:");
  ImGui::InputText("##name", new_preset_name_, sizeof(new_preset_name_));
  ImGui::SameLine();
  if (ImGui::Button("Save")) {
    if (new_preset_name_[0] != '\0') {
      save_current_preset();
      new_preset_name_[0] = '\0';
      preset_manager_scan(preset_manager_);
    }
  }
  
  ImGui::Separator();
  ImGui::Text("Saved Presets:");
  
  int count = preset_manager_count(preset_manager_);
  if (count == 0) {
    ImGui::TextDisabled("No presets saved");
  } else {
    for (int i = 0; i < count; ++i) {
      const preset_t* preset = preset_manager_get(preset_manager_, i);
      if (!preset) continue;
      
      ImGui::PushID(i);
      
      if (ImGui::Button("Load")) {
        load_preset(preset);
      }
      ImGui::SameLine();
      
      if (ImGui::Button("Delete")) {
        preset_manager_delete(preset_manager_, preset->name);
        preset_manager_scan(preset_manager_);
      }
      ImGui::SameLine();
      
      ImGui::Text("%s", preset->name);
      
      ImGui::PopID();
    }
  }
  
  // Factory presets from unit
  if (hdr->num_presets > 0 && loader_->unit_load_preset && loader_->unit_get_preset_name) {
    ImGui::Separator();
    ImGui::Text("Factory Presets:");
    
    for (uint32_t i = 0; i < hdr->num_presets; ++i) {
      ImGui::PushID(1000 + i);  // Offset to avoid ID conflicts
      
      if (ImGui::Button("Load")) {
        // Load factory preset
        loader_->unit_load_preset(static_cast<uint8_t>(i));
        
        // Update param cache from unit
        for (uint32_t p = 0; p < hdr->num_params && p < param_values_.size(); ++p) {
          if (loader_->unit_get_param_value) {
            param_values_[p] = loader_->unit_get_param_value(static_cast<uint8_t>(p));
          }
        }
      }
      ImGui::SameLine();
      
      const char* preset_name = loader_->unit_get_preset_name(static_cast<uint8_t>(i));
      ImGui::Text("%s", preset_name ? preset_name : "Factory Preset");
      
      ImGui::PopID();
    }
  }
  
  ImGui::End();
}

void ImGuiApp::save_current_preset() {
  const unit_header_t* hdr = loader_ ? loader_->header : nullptr;
  if (!hdr || !preset_manager_) return;
  
  preset_t preset;
  memset(&preset, 0, sizeof(preset));
  strncpy(preset.name, new_preset_name_[0] != '\0' ? new_preset_name_ : "Untitled", sizeof(preset.name) - 1);
  strncpy(preset.unit_name, hdr->name, sizeof(preset.unit_name) - 1);
  preset.dev_id = hdr->dev_id;
  preset.unit_id = hdr->unit_id;
  preset.num_params = std::min(static_cast<uint32_t>(hdr->num_params), static_cast<uint32_t>(PRESET_MAX_PARAMS));
  
  for (uint8_t i = 0; i < preset.num_params && i < param_values_.size(); ++i) {
    preset.param_values[i] = param_values_[i];
  }
  
  preset_manager_save(preset_manager_, &preset);
}

void ImGuiApp::load_preset(const preset_t* preset) {
  if (!preset || !loader_) return;
  
  // Validate preset matches unit
  const unit_header_t* hdr = loader_->header;
  if (!hdr) return;
  
  if (preset->dev_id != hdr->dev_id || preset->unit_id != hdr->unit_id) {
    fprintf(stderr, "Warning: Preset is for different unit\n");
    // Could show UI warning here
  }
  
  // Apply parameters
  uint32_t count = std::min(static_cast<uint32_t>(preset->num_params), static_cast<uint32_t>(hdr->num_params));
  for (uint32_t i = 0; i < count && i < param_values_.size(); ++i) {
    param_values_[i] = preset->param_values[i];
    unit_loader_set_param(loader_, static_cast<uint8_t>(i), preset->param_values[i]);
  }
}

void ImGuiApp::Run() {
  SDL_Event event;
  while (running_) {
    while (SDL_PollEvent(&event)) {
      ImGui_ImplSDL2_ProcessEvent(&event);
      if (event.type == SDL_QUIT) {
        running_ = false;
      }
      if (event.type == SDL_WINDOWEVENT && event.window.event == SDL_WINDOWEVENT_CLOSE && event.window.windowID == SDL_GetWindowID(window_)) {
        running_ = false;
      }
    }

    ImGui_ImplOpenGL3_NewFrame();
    ImGui_ImplSDL2_NewFrame();
    ImGui::NewFrame();

    render_ui();

    ImGui::Render();
    int display_w, display_h;
    SDL_GL_GetDrawableSize(window_, &display_w, &display_h);
    glViewport(0, 0, display_w, display_h);
    glClearColor(0.05f, 0.05f, 0.08f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);
    ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

    SDL_GL_SwapWindow(window_);
  }
}
