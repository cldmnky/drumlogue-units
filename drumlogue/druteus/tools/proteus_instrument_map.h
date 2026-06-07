#pragma once

#include <cstddef>
#include <cstdint>

// Proteus/1 base instruments (1-125) + Plus Orchestral expansion (126-203)
// Maps 0-indexed instrument IDs to SF2 preset indices
// For base Proteus/1: IDs 0-124 map to SF2 presets 0-124
// For Plus Orchestral: IDs 125-202 are NOT in the base SF2

static constexpr int16_t kProteusInstrumentToSf2Preset[] = {
    // Base Proteus/1 instruments (0-124)
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
    120, 121, 122, 123, 124,
    // Plus Orchestral expansion (125-202) - NOT in base SF2
    // These return -1 to indicate unavailable
    -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,  // 125-134
    -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,  // 135-144
    -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,  // 145-154
    -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,  // 155-164
    -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,  // 165-174
    -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,  // 175-184
    -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,  // 185-194
    -1, -1, -1, -1, -1, -1, -1, -1,           // 195-202
};

static constexpr size_t kProteusInstrumentToSf2PresetCount =
    sizeof(kProteusInstrumentToSf2Preset) / sizeof(kProteusInstrumentToSf2Preset[0]);

// Instrument name lookup for debugging/display
static const char* GetProteusInstrumentName(int id) {
    static const char* names[] = {
        // Base Proteus/1 (0-124)
        "Piano", "Piano Pad", "Loose Piano", "Tight Piano",
        "Strings", "Long Strings", "Slow Strings", "Dark Strings",
        "Voices", "Slow Voices", "Dark Choir",
        "Synth Flute", "Soft Flute", "Alto Sax", "Tenor Sax",
        "BaritoneSax", "Dark Sax", "Trumpet Sft", "Dark TrumpS",
        "Trumpet Hrd", "Dark TrumpH", "Horn Falls",
        "Trombone 1", "Trombone 2", "French Horn",
        "Brass 1", "Brass 2", "Brass 3", "Trom / Sax",
        "Guitar Mute", "El. Guitar", "Ac. Guitar",
        "Rock Bass", "Stone Bass", "Flint Bass",
        "Funk Slap", "Funk Pop", "Harmonics", "Rock/Harms",
        "Stone/Harms", "Nose Bass", "Bass Syn 1", "Bass Syn 2",
        "Synth Pad", "Med Env Pad", "Lng Env Pad",
        "Dark Synth", "Perc. Organ", "Marimba", "Vibraphone",
        "All Perc. B", "All Perc.UB", "Standard 1", "Standard 2",
        "Standard 3",
        "Kicks", "Snares", "Toms", "Cymbals",
        "Latin Drums", "Latin Perc.", "Agogo", "Woodblock",
        "Conga", "Timbal", "Ride", "Perc FX 1",
        "Perc FX 2", "Metal",
        "Oct 1 Sine", "Oct 2 All", "Oct 3 All", "Oct 4 All",
        "Oct 5 All", "Oct 6 All", "Oct 7 All",
        "Oct 2 Odd", "Oct 3 Odd", "Oct 4 Odd",
        "Oct 5 Odd", "Oct 6 Odd", "Oct 7 Odd",
        "Oct 2 Even", "Oct 3 Even", "Oct 4 Even",
        "Oct 5 Even", "Oct 6 Even", "Oct 7 Even",
        "Low Odds", "Low Evens", "FourOctaves",
        "Synth Cyc 1", "Synth Cyc 2", "Synth Cyc 3", "Synth Cyc 4",
        "Fund Gone 1", "Fund Gone 2",
        "Bite Cyc", "Buzzy Cyc 1",
        "Metlphone 1", "Metlphone 2", "Metlphone 3", "Metlphone 4",
        "Duck Cyc 1", "Duck Cyc 2", "Duck Cyc 3",
        "Wind Cyc 1", "Wind Cyc 2", "Wind Cyc 3",
        "Wind Cyc 4", "Organ Cyc 1", "Organ Cyc 2",
        "Noise", "StrayVoices1", "StrayVoices2",
        "StrayVoices3", "StrayVoices4",
        "Synth String1", "Synth String2",
        "Animals", "Reed", "Pluck 1", "Pluck 2",
        "Mallet 1", "Mallet 2",
        // Plus Orchestral expansion (125-202)
        "Solo Cello", "Solo Viola", "Solo Violin",
        "Gambambo", "Quartet 1", "Quartet 2", "Quartet 3", "Quartet 4",
        "Pizz Basses", "Pizz Celli", "Pizz Violas", "Pizz Violin",
        "Pizzicombo", "Bass Clarinet", "Clarinet",
        "Bass Clar/Clar", "Contra Bassoon", "Bassoon",
        "English Horn", "Oboe", "Woodwinds",
        "Harmon Mute", "Tubular Bell", "Timpani",
        "Timpani/Tubular", "Tamborine", "Tam Tam",
        "Percussion 3", "Special Effects", "Oboe noVib",
        "Upright Pizz",
        "Sine Wave", "Triangle Wave", "Square Wave",
        "Pulse 33%", "Pulse 25%", "Pulse 10%",
        "Sawtooth", "Sawtooth Odd Gone", "Ramp",
        "Ramp Even Only", "Violin Essence", "Buzzoon",
        "Brassy Wave", "Reedy Buzz", "Growl Wave",
        "HarpsiWave", "Fuzzy Gruzz", "Power 5ths",
        "Filtered Saw", "Ice Bell", "Bronze Age",
        "Iron Plate", "Aluminum", "Lead Beam",
        "Steel Extract", "Winter Glass", "Town Bell Wash",
        "Orchestral Bells", "Tubular SE", "Soft Bell Wave",
        "Swirly", "Tack Attack", "Shimmer Wave",
        "Moog Lead", "B3 SE", "Mild Tone",
        "Piper", "Ah Wave", "Vocal Wave",
        "Fuzzy Clav", "Electrhode", "Whine 1",
        "Whine 2", "Metal Drone", "Silver Race",
        "Metal Attack", "Filter Bass",
    };
    
    if (id < 0 || id >= static_cast<int>(sizeof(names) / sizeof(names[0]))) {
        return "Unknown";
    }
    return names[id];
}

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
