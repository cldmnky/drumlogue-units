/**
 * @file unit.h
 * @brief Minimal drumlogue SDK mock for desktop builds of the Pepege test
 *        harnesses (main.cc, perf_test.cc).
 *
 * pepege_synth.h includes "unit.h" only when TEST is not defined; under TEST
 * it still declares an Init() overload that takes a unit_runtime_desc_t, so
 * the type has to exist. Only the fields the synth reads are provided.
 */

#ifndef UNIT_H_
#define UNIT_H_

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

// Error codes
#define k_unit_err_none 0
#define k_unit_err_undef -1
#define k_unit_err_target -2
#define k_unit_err_api_version -3

// Unit runtime descriptor (only the fields the synth reads)
typedef struct {
    uint16_t target;
    uint32_t api;
    uint32_t samplerate;
    uint16_t frames_per_buffer;
    uint8_t input_channels;
    uint8_t output_channels;
} unit_runtime_desc_t;

#ifdef __cplusplus
}
#endif

#endif  // UNIT_H_
