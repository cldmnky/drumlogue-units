#include "sf_loader.h"
#include "druteus_state.h"
#include "params.h"
#include "patch_engine.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h>

#define SOUNDFONT_PATH   "/var/lib/drumlogued/userfs/Programs"
#define CHUNK_SIZE       131072
#define OUTPUT_MODE      TSF_STEREO_INTERLEAVED
#define REQUIRED_PATCH_SF2 "Proteus1_Instruments.sf2"

const char *sf2_prefix = "";
const char *sf2_suffix = ".sf2";
fs_dir soundfont_list = fs_dir(SOUNDFONT_PATH, sf2_prefix, sf2_suffix);
tsf *soundfont = nullptr;
char * __attribute__((aligned(32))) soundfont_buf = nullptr;
volatile uint32_t state = SF_LOAD_IDLE;
volatile bool suspended = false;

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

void sf_load_step(uint32_t frames) {
  (void)frames;
  static FILE   *fp       = nullptr;
  static size_t  buf_size = 0;
  static size_t  buf_pos  = 0;

  switch (state) {
    case SF_LOAD_START: {
      char *path = (char *)malloc(PATH_MAX);
      if (path == nullptr) {
        state = SF_LOAD_IDLE;
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
        state = SF_LOAD_IDLE;
        break;
      }
      break;
    }
    case SF_LOAD_ALLOC: {
      struct stat st;
      if (fstat(fileno(fp), &st) != 0) {
        fclose(fp);
        fp = nullptr;
        state = SF_LOAD_IDLE;
        break;
      }
      buf_size = (size_t)st.st_size;
      free(soundfont_buf);
      soundfont_buf = (char *)malloc(buf_size);
      if (soundfont_buf == nullptr) {
        fclose(fp);
        fp = nullptr;
        buf_size = 0;
        state = SF_LOAD_IDLE;
        break;
      }
      buf_pos = 0;
      break;
    }
    case SF_LOAD_READ: {
      if (buf_pos >= buf_size)
        break;
      size_t remaining = buf_size - buf_pos;
      size_t chunk_size = remaining < CHUNK_SIZE ? remaining : CHUNK_SIZE;
      size_t n = fread(soundfont_buf + buf_pos, 1, chunk_size, fp);
      buf_pos += n;
      if (n < chunk_size || buf_pos >= buf_size) {
        break;
      }
      state--;
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
        state = SF_LOAD_IDLE;
        break;
      }
      int max_preset = tsf_get_presetcount(soundfont) - 1;
      if (Params[param_preset] > max_preset)
        Params[param_preset] = max_preset;
      break;
    }
    case SF_LOAD_TSF_SET: {
      tsf_set_output(soundfont, OUTPUT_MODE, 48000, 0.f);
      s_apply_params();
      break;
    }
    case SF_LOAD_FINISHED: {
      break;
    }
    default:
      state = SF_LOAD_IDLE;
  }

  if (state != SF_LOAD_IDLE) {
    state++;
  }
}
