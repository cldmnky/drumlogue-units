// Stub unit.h for local testing of elements-synth
// Provides the minimal types needed to compile for desktop testing
// This file shadows the real logue-sdk unit.h

#ifndef UNIT_H_
#define UNIT_H_

#include <cstdint>

// Error codes from logue SDK
enum {
  k_unit_err_none = 0,
  k_unit_err_undef = 1,
  k_unit_err_samplerate = 2,
  k_unit_err_memory = 3,
  k_unit_err_target = 4,
  k_unit_err_api_version = 5,
};

// Runtime descriptor passed to unit_init
typedef struct unit_runtime_desc {
  uint32_t target;           // Not used in test
  uint32_t api;              // Not used in test
  uint32_t samplerate;       // Sample rate (48000)
  uint32_t frames_per_buffer; // Frames per processing block
  uint8_t input_channels;    // Number of input channels
  uint8_t output_channels;   // Number of output channels
  uint8_t padding[2];        // Padding for alignment
} unit_runtime_desc_t;

// Parameter types (for header.c compatibility)
enum {
  k_unit_param_type_none = 0,
  k_unit_param_type_percent = 1,
  k_unit_param_type_db = 2,
  k_unit_param_type_cents = 3,
  k_unit_param_type_semi = 4,
  k_unit_param_type_oct = 5,
  k_unit_param_type_hertz = 6,
  k_unit_param_type_khertz = 7,
  k_unit_param_type_bpm = 8,
  k_unit_param_type_msec = 9,
  k_unit_param_type_sec = 10,
  k_unit_param_type_strings = 11,
  k_unit_param_type_bitmaps = 12,
  k_unit_param_type_drywet = 13,
  k_unit_param_type_pan = 14,
  k_unit_param_type_spread = 15,
  k_unit_param_type_onoff = 16,
  k_unit_param_type_midi_note = 17,
  k_unit_param_type_toggle = 18,
};

// Module types
enum {
  k_unit_module_synth = 0,
  k_unit_module_delfx = 1,
  k_unit_module_revfx = 2,
  k_unit_module_masterfx = 3,
};

// Preset handling
enum {
  k_num_unit_presets = 8,
};

// For header.c compatibility
#define UNIT_TARGET_PLATFORM 0
#define UNIT_API_VERSION 0
#define UNIT_TARGET_PLATFORM_DRUMLOGUE 0
#define __unit_header __attribute__((used, section(".unit_header")))
#define __unit_callback

// Parameter descriptor (matches logue SDK)
typedef struct unit_param {
  int16_t min;
  int16_t max;
  int16_t center;
  int16_t init;
  uint8_t type;
  uint8_t frac;
  uint8_t frac_mode;
  uint8_t reserved;
  char name[12];
} unit_param_t;

// Unit header (matches logue SDK structure)
typedef struct unit_header {
  uint32_t header_size;
  uint32_t target;
  uint32_t api;
  uint32_t dev_id;
  uint32_t unit_id;
  uint32_t version;
  char name[20];
  uint32_t num_presets;
  uint32_t num_params;
  unit_param_t params[24];
} unit_header_t;

// Macro to check API compatibility (always true in test)
#ifndef UNIT_API_IS_COMPAT
#define UNIT_API_IS_COMPAT(api) (true)
#endif

// External declaration for the unit header defined in header.c
extern const unit_header_t unit_header;

#endif  // UNIT_H_
