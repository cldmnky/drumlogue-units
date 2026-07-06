#include "sf_loader.h"
#include "druteus_state.h"
#include "params.h"
#include "patch_engine.h"
#include "voice_engine.h"   // for voice_all_note_off + voice_reset on reload
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h>

#define SOUNDFONT_PATH   "/var/lib/drumlogued/userfs/Programs"
#define OUTPUT_MODE      TSF_STEREO_INTERLEAVED
#define REQUIRED_PATCH_SF2 "Proteus1_Instruments.sf2"

const char *sf2_prefix = "";
const char *sf2_suffix = ".sf2";
// Use the parameterised constructor so path + suffix are known when
// refresh() scans.  Without a path, scandir() silently returns 0 and
// the SF2 drop-down shows "NO SF2".  The global-ctor scan is benign
// here — SOUNDFONT_PATH is a macro and the prefix/suffix are string
// literals, so no initialisation-order issues (review #19 addendum).
fs_dir soundfont_list = fs_dir(SOUNDFONT_PATH, sf2_prefix, sf2_suffix);
tsf *soundfont = nullptr;
char * __attribute__((aligned(32))) soundfont_buf = nullptr;

std::atomic<uint32_t> state{SF_LOAD_IDLE};
std::atomic<bool> reload_requested{false};
std::atomic<bool> suspended{false};
std::atomic<bool> patch_dirty{false};

std::atomic<bool> voices_dirty{false};
std::atomic<int>  pending_max_voices{16};
std::atomic<int>  pending_volume{100};
std::atomic<int>  pending_pan{64};
std::atomic<int>  pending_fine_tune{0};

void sf_teardown() {
  soundfont_list.cleanup();
  tsf_close(soundfont);
  soundfont = nullptr;
  free(soundfont_buf);
  soundfont_buf = nullptr;
}

void sf_reset() {
  if (soundfont != nullptr)
    tsf_reset(soundfont);
}

int sf_find_index_by_name(const char* name) {
  if (name == nullptr || soundfont_list.count <= 0)
    return -1;
  for (int i = 0; i < soundfont_list.count; ++i) {
    if (strcmp(soundfont_list.get(i), name) == 0)
      return i;
  }
  return -1;
}

// Apply deferred TSF/allocator changes raised by the control thread.
// This is the only thread that mutates `soundfont->voices` capacity and
// the TSF channel controls that walk live voices, so we serialize here.
void sf_apply_pending() {
  if (voices_dirty.load(std::memory_order_acquire)) {
    int v = pending_max_voices.load(std::memory_order_relaxed);
    if (v < 1)  v = 1;
    if (v > 16) v = 16;

    // Re-init the allocator (cheap, no audio side-effects).
    voice_allocator.Init((uint8_t)v);
    voice_allocator.SetMode(common::VoiceMode::Polyphonic);
    voice_allocator.SetAllocationStrategy(common::VoiceAllocationStrategy::OldestNote);

    // Apply to TSF only if the soundfont is loaded — tsf_set_max_voices
    // reallocates f->voices, which would race with tsf_render_float
    // (review #1).  The audio thread runs alone here, so it's safe.
    // With dual-layer patches, each MIDI note-on creates 2 TSF voices
    // (one per layer channel).  Double the pool so all requested notes
    // get voices; otherwise TSF silently steals voices causing distorted
    // audio when the pool is exhausted.
    if (soundfont != nullptr)
      tsf_set_max_voices(soundfont, patch_has_secondary ? v * 2 : v);

    voices_dirty.store(false, std::memory_order_release);
  }
}

void sf_load_step(uint32_t frames) {
  (void)frames;
  static FILE   *fp       = nullptr;
  static size_t  buf_size = 0;
  static size_t  buf_pos  = 0;

  // Fold cross-thread reload trigger into the state machine at a safe
  // point — review #2/#3.
  if (reload_requested.load(std::memory_order_acquire)) {
    reload_requested.store(false, std::memory_order_release);
    if (state.load(std::memory_order_relaxed) == SF_LOAD_IDLE) {
      // Kill live voices before tearing down the soundfont to avoid
      // dangling tsf_voice pointers — review #17.
      voice_all_note_off();
      voice_reset();
      state.store(SF_LOAD_START, std::memory_order_release);
    }
  }

  switch (state.load(std::memory_order_relaxed)) {
    case SF_LOAD_START: {
      char *path = (char *)malloc(PATH_MAX);
      if (path == nullptr) {
        state.store(SF_LOAD_IDLE, std::memory_order_relaxed);
        break;
      }
      snprintf(path, PATH_MAX, "%s/%s",
               SOUNDFONT_PATH,
               soundfont_list.get(Params[param_soundfont]));
      if (fp != nullptr) {
        fclose(fp);
        fp = nullptr;
      }
      fp = fopen(path, "rb");
      free(path);
      if (fp == nullptr) {
        state.store(SF_LOAD_IDLE, std::memory_order_relaxed);
        break;
      }
      break;
    }
    case SF_LOAD_ALLOC: {
      struct stat st;
      if (fstat(fileno(fp), &st) != 0) {
        fclose(fp);
        fp = nullptr;
        buf_size = 0;
        state.store(SF_LOAD_IDLE, std::memory_order_relaxed);
        break;
      }
      buf_size = (size_t)st.st_size;
      free(soundfont_buf);
      soundfont_buf = (char *)malloc(buf_size);
      if (soundfont_buf == nullptr) {
        fclose(fp);
        fp = nullptr;
        buf_size = 0;
        state.store(SF_LOAD_IDLE, std::memory_order_relaxed);
        break;
      }
      buf_pos = 0;
      break;
    }
    case SF_LOAD_READ: {
      // Read the entire file in one call — the state machine advances
      // one state per sf_load_step() invocation, so a chunked loop
      // would exit the READ state after the first 128 KB chunk and hand
      // an incomplete buffer to TSF_LOAD.  A single fread blocks for
      // <1 ms on a local SSD even for a 4 MB SF2, which is acceptable.
      if (buf_pos >= buf_size)
        break;
      size_t n = fread(soundfont_buf, 1, buf_size, fp);
      if (n < buf_size && !feof(fp)) {
        // Real I/O error — discard partial buffer.
        fclose(fp);
        fp = nullptr;
        free(soundfont_buf);
        soundfont_buf = nullptr;
        buf_size = 0;
        buf_pos  = 0;
        state.store(SF_LOAD_IDLE, std::memory_order_relaxed);
        break;
      }
      buf_pos = n;  // actual bytes read; auto-advance moves to CLOSE
      break;
    }
    case SF_LOAD_CLOSE: {
      if (fp != nullptr) {
        fclose(fp);
        fp = nullptr;
      }
      tsf_close(soundfont);
      soundfont = nullptr;
      break;
    }
    case SF_LOAD_TSF_LOAD: {
      soundfont = tsf_load_memory(soundfont_buf, (int)buf_size);
      if (soundfont == nullptr) {
        free(soundfont_buf);
        soundfont_buf = nullptr;
        buf_size = 0;
        buf_pos = 0;
        state.store(SF_LOAD_IDLE, std::memory_order_relaxed);
        break;
      }
      // NOTE: the param_preset clamp against SF2 preset count is
      // intentionally NOT applied here.  param_preset indexes the
      // Proteus patch table (PROTEUS_PATCH_COUNT entries), not the
      // SF2 preset list — clamping against SF2 presets silently
      // truncates the selected patch on every font load
      // (review #4).  params_set already clamps against the patch
      // table; the SF2-instrument-to-preset mapping happens in
      // s_load_patch via tsf_get_presetindex (patch_engine).
      break;
    }
    case SF_LOAD_TSF_SET: {
      tsf_set_output(soundfont, OUTPUT_MODE, 48000, 0.f);
      s_apply_params();
      break;
    }
    case SF_LOAD_FINISHED: {
      // Load is complete; transition back to idle so unit_render
      // can pass audio through.
      state.store(SF_LOAD_IDLE, std::memory_order_release);
      break;
    }
    default:
      state.store(SF_LOAD_IDLE, std::memory_order_relaxed);
  }

  uint32_t cur = state.load(std::memory_order_relaxed);
  if (cur != SF_LOAD_IDLE && cur != SF_LOAD_FINISHED) {
    state.store(cur + 1, std::memory_order_relaxed);
  }
}
