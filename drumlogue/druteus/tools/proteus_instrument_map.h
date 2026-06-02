#pragma once

#include <cstddef>
#include <cstdint>

// Explicit Proteus/1 instrument ID -> SF2 preset index map for
// Proteus1_Instruments.sf2. IDs outside this table are treated as unmapped.
static constexpr int16_t kProteusInstrumentToSf2Preset[] = {
    0, 1, 2, 3, 4, 5, 6, 7, 8, 9,
    10, 11, 12, 13, 14, 15, 16, 17, 18, 19,
    20, 21, 22, 23, 24, 25, 26, 27, 28, 29,
    30, 31, 32, 33, 34, 35, 36, 37, 38, 39,
    40, 41, 42, 43, 44, 45, 46, 47, 48, 49,
    50, 51, 52, 53, 54, 55, 56, 57, 58, 59,
    60, 61, 62, 63, 64, 65, 66, 67, 68, 69,
    70, 71, 72, 73, 74, 75, 76, 77, 78, 79,
    80, 81, 82, 83, 84, 85, 86, 87, 88, 89,
    90, 91, 92, 93, 94, 95, 96, 97, 98, 99,
    100, 101, 102, 103, 104, 105, 106, 107, 108, 109,
    110, 111, 112, 113, 114, 115, 116, 117, 118, 119,
    120, 121, 122, 123, 124, 125,
};

static constexpr size_t kProteusInstrumentToSf2PresetCount =
    sizeof(kProteusInstrumentToSf2Preset) / sizeof(kProteusInstrumentToSf2Preset[0]);

static inline int resolve_proteus_instrument_to_sf2_preset(int proteus_instrument_id,
                                                            int sf2_preset_count) {
  if (sf2_preset_count <= 0) {
    return -1;
  }
  if (proteus_instrument_id < 0 ||
      proteus_instrument_id >= static_cast<int>(kProteusInstrumentToSf2PresetCount)) {
    return -1;
  }

  const int mapped = kProteusInstrumentToSf2Preset[proteus_instrument_id];
  if (mapped < 0 || mapped >= sf2_preset_count) {
    return -1;
  }
  return mapped;
}
