// Minimal Logue SDK runtime stubs for native preset editor host
#pragma once

#include <stdint.h>

#include "../../../logue-sdk/platform/drumlogue/common/runtime.h"

#ifndef UNIT_API_VERSION
#define UNIT_API_VERSION 0x00020000U
#endif

#ifndef UNIT_TARGET_PLATFORM
#define UNIT_TARGET_PLATFORM 0x0400U
#endif

#ifdef __cplusplus
extern "C" {
#endif

typedef struct runtime_stub_state {
    unit_runtime_desc_t* runtime_desc;
} runtime_stub_state_t;

// Initialize stub runtime and allocate descriptor
int runtime_stubs_init(runtime_stub_state_t* state,
                       uint32_t sample_rate,
                       uint16_t frames_per_buffer,
                       uint8_t channels);

// Free allocated resources
void runtime_stubs_teardown(runtime_stub_state_t* state);

// Sample accessors used by runtime descriptor
uint8_t runtime_stub_get_num_sample_banks(void);
uint8_t runtime_stub_get_num_samples_for_bank(uint8_t bank);
const sample_wrapper_t* runtime_stub_get_sample(uint8_t bank, uint8_t sample);

#ifdef __cplusplus
}  // extern "C"
#endif
