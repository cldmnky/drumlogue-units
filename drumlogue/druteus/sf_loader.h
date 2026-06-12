#pragma once

#include <stdint.h>
#include <sys/stat.h>
#include <atomic>

// Soundfont loading state machine.
// Exposed so callers can trigger a load (state = SF_LOAD_START) or check
// completion (state == SF_LOAD_IDLE) without magic numbers.
enum {
  SF_LOAD_IDLE = 0,
  SF_LOAD_START = 1,
  SF_LOAD_ALLOC,
  SF_LOAD_READ,
  SF_LOAD_CLOSE,
  SF_LOAD_TSF_LOAD,
  SF_LOAD_TSF_SET,
  SF_LOAD_FINISHED,
};

void sf_teardown();
void sf_reset();
int sf_find_index_by_name(const char* name);
void sf_load_step(uint32_t frames);

// Applies any deferred param changes (voices, volume, pan, fine-tune)
// raised by the control thread.  Called from `unit_render` before any
// audio work touches the soundfont.  Idempotent and cheap when nothing
// is pending.
void sf_apply_pending();
