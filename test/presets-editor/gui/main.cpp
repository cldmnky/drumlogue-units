#include <cstdlib>
#include <cstring>
#include <iostream>
#include <string>

#include "imgui_app.h"

static void print_usage(const char* prog) {
  std::cerr << "Usage: " << prog << " --unit <path> [--frames N] [--rate Hz] [--channels C]\n";
}

int main(int argc, char** argv) {
  std::string unit_path;
  uint32_t sample_rate = 48000;
  uint16_t frames = 128;
  uint8_t channels = 2;

  for (int i = 1; i < argc; ++i) {
    if ((std::strcmp(argv[i], "--unit") == 0 || std::strcmp(argv[i], "-u") == 0) && i + 1 < argc) {
      unit_path = argv[++i];
    } else if (std::strcmp(argv[i], "--frames") == 0 && i + 1 < argc) {
      frames = static_cast<uint16_t>(std::atoi(argv[++i]));
    } else if (std::strcmp(argv[i], "--rate") == 0 && i + 1 < argc) {
      sample_rate = static_cast<uint32_t>(std::atoi(argv[++i]));
    } else if (std::strcmp(argv[i], "--channels") == 0 && i + 1 < argc) {
      channels = static_cast<uint8_t>(std::atoi(argv[++i]));
    } else if (std::strcmp(argv[i], "--help") == 0 || std::strcmp(argv[i], "-h") == 0) {
      print_usage(argv[0]);
      return 0;
    }
  }

  if (unit_path.empty()) {
    print_usage(argv[0]);
    return 1;
  }

  ImGuiApp app(unit_path, sample_rate, frames, channels);
  if (!app.Init()) {
    std::cerr << "Failed to initialize GUI app" << std::endl;
    return 1;
  }

  app.Run();
  return 0;
}
