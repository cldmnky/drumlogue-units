// Test harness for Drumpler (JV-880 emulator)
// Renders a short clip using an embedded ROM file

#include <iostream>
#include <vector>
#include <string>
#include <fstream>
#include <cstring>
#include <cstdlib>

// Define TEST to enable desktop testing
#ifndef TEST
#define TEST
#endif

#include "wav_file.h"
#include "emulator/jv880_wrapper.h"

static void PrintUsage(const char* prog) {
  std::cerr << "Usage: " << prog << " --rom <path> --out <file> [options]\n";
  std::cerr << "Options:\n";
  std::cerr << "  --seconds <sec>      Render length in seconds (default 2.0)\n";
  std::cerr << "  --note <note>        MIDI note number (default 60)\n";
  std::cerr << "  --velocity <vel>     MIDI velocity 1-127 (default 100)\n";
  std::cerr << "  --program <prog>     Program change (default 0)\n";
  std::cerr << "  --channel <ch>       MIDI channel 0-15 (default 0)\n";
  std::cerr << "  --note-off-ms <ms>   Note-off time in ms (default 1000)\n";
}

static bool LoadRomFile(const std::string& path, std::vector<uint8_t>& data) {
  std::ifstream file(path, std::ios::binary | std::ios::ate);
  if (!file.is_open()) {
    std::cerr << "Failed to open ROM: " << path << std::endl;
    return false;
  }

  std::streamsize size = file.tellg();
  if (size <= 0) {
    std::cerr << "ROM file is empty: " << path << std::endl;
    return false;
  }

  data.resize(static_cast<size_t>(size));
  file.seekg(0, std::ios::beg);
  if (!file.read(reinterpret_cast<char*>(data.data()), size)) {
    std::cerr << "Failed to read ROM: " << path << std::endl;
    return false;
  }

  return true;
}

int main(int argc, char** argv) {
  std::string rom_path;
  std::string out_path = "output.wav";
  float seconds = 2.0f;
  int note = 60;
  int velocity = 100;
  int program = 0;
  int channel = 0;
  int note_off_ms = 1000;

  for (int i = 1; i < argc; ++i) {
    if (strcmp(argv[i], "--rom") == 0 && i + 1 < argc) {
      rom_path = argv[++i];
    } else if (strcmp(argv[i], "--out") == 0 && i + 1 < argc) {
      out_path = argv[++i];
    } else if (strcmp(argv[i], "--seconds") == 0 && i + 1 < argc) {
      seconds = static_cast<float>(atof(argv[++i]));
    } else if (strcmp(argv[i], "--note") == 0 && i + 1 < argc) {
      note = atoi(argv[++i]);
    } else if (strcmp(argv[i], "--velocity") == 0 && i + 1 < argc) {
      velocity = atoi(argv[++i]);
    } else if (strcmp(argv[i], "--program") == 0 && i + 1 < argc) {
      program = atoi(argv[++i]);
    } else if (strcmp(argv[i], "--channel") == 0 && i + 1 < argc) {
      channel = atoi(argv[++i]);
    } else if (strcmp(argv[i], "--note-off-ms") == 0 && i + 1 < argc) {
      note_off_ms = atoi(argv[++i]);
    } else if (strcmp(argv[i], "--help") == 0 || strcmp(argv[i], "-h") == 0) {
      PrintUsage(argv[0]);
      return 0;
    }
  }

  if (rom_path.empty()) {
    PrintUsage(argv[0]);
    return 1;
  }

  std::vector<uint8_t> rom_data;
  if (!LoadRomFile(rom_path, rom_data)) {
    return 1;
  }

  drumpler::JV880Emulator emulator;
  if (!emulator.Init(rom_data.data(), static_cast<uint32_t>(rom_data.size()))) {
    std::cerr << "Failed to initialize emulator with ROM." << std::endl;
    return 1;
  }

  // Print patch names from ROM
  std::cout << "Patch names from ROM:" << std::endl;
  char name_buf[16];
  for (int i = 0; i < 128; ++i) {
    if (emulator.GetPatchName(static_cast<uint8_t>(i), name_buf, sizeof(name_buf))) {
      std::cout << "  " << i << ": " << name_buf << std::endl;
    } else {
      std::cout << "  " << i << ": (not found)" << std::endl;
    }
    if (i >= 15 && i < 120) {
      // Skip middle for brevity
      if (i == 15) std::cout << "  ... (skipping 16-119)" << std::endl;
      i = 119;
    }
  }
  std::cout << std::endl;

  // Warmup: Give emulator time to boot (750 iterations ~= 1.25 seconds)
  std::cout << "Warming up emulator..." << std::endl;
  std::vector<float> warmup_l(128, 0.0f);
  std::vector<float> warmup_r(128, 0.0f);
  for (int i = 0; i < 750; ++i) {
    emulator.Render(warmup_l.data(), warmup_r.data(), 128);
    // Send test note halfway through warmup to initialize voice engine
    if (i == 375) {
      std::cout << "Sending test note during warmup..." << std::endl;
      emulator.NoteOn(0, 60, 64);
      emulator.Render(warmup_l.data(), warmup_r.data(), 128);
      emulator.NoteOff(0, 60);
    }
  }
  std::cout << "Warmup complete." << std::endl << std::endl;

  if (program >= 0) {
    emulator.ProgramChange(static_cast<uint8_t>(channel), static_cast<uint8_t>(program));
  }

  const int sample_rate = 48000;
  const int total_frames = static_cast<int>(seconds * sample_rate);
  const int note_off_frame = (note_off_ms * sample_rate) / 1000;

  std::vector<float> interleaved(static_cast<size_t>(total_frames) * 2, 0.0f);
  std::vector<float> block_l(128, 0.0f);
  std::vector<float> block_r(128, 0.0f);

  const int block_size = 128;
  int frame = 0;
  bool note_on_sent = false;
  bool note_off_sent = false;

  while (frame < total_frames) {
    if (!note_on_sent) {
      emulator.NoteOn(static_cast<uint8_t>(channel), static_cast<uint8_t>(note),
                      static_cast<uint8_t>(velocity));
      note_on_sent = true;
    }

    if (!note_off_sent && frame >= note_off_frame) {
      emulator.NoteOff(static_cast<uint8_t>(channel), static_cast<uint8_t>(note));
      note_off_sent = true;
    }

    int frames_this_block = block_size;
    if (frame + frames_this_block > total_frames) {
      frames_this_block = total_frames - frame;
    }

    emulator.Render(block_l.data(), block_r.data(), static_cast<uint32_t>(frames_this_block));

    for (int i = 0; i < frames_this_block; ++i) {
      interleaved[(frame + i) * 2 + 0] = block_l[i];
      interleaved[(frame + i) * 2 + 1] = block_r[i];
    }

    frame += frames_this_block;
  }

  WavFile wav;
  if (!wav.OpenWrite(out_path, sample_rate, 2)) {
    return 1;
  }
  wav.Write(interleaved);

  std::cout << "Wrote " << out_path << " (" << total_frames << " frames)" << std::endl;
  return 0;
}
