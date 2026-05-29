#include <iostream>
#include <vector>
#include <cmath>
#include <cstring>
#include <cstdlib>
#include <cstdint>

#include <sndfile.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846f
#endif

static void GenerateImpulse(const char* output, uint32_t sample_rate = 48000) {
  const size_t size = sample_rate;
  std::vector<float> buffer(size, 0.0f);
  buffer[0] = 1.0f;

  SF_INFO sfinfo = {};
  sfinfo.samplerate = sample_rate;
  sfinfo.channels = 2;
  sfinfo.format = SF_FORMAT_WAV | SF_FORMAT_PCM_16;

  SNDFILE* sf = sf_open(output, SFM_WRITE, &sfinfo);
  if (!sf) {
    std::cerr << "Error: Could not create " << output << std::endl;
    return;
  }
  std::vector<float> interleaved(size * 2, 0.0f);
  for (size_t i = 0; i < size; i++) {
    interleaved[i * 2] = buffer[i];
    interleaved[i * 2 + 1] = buffer[i];
  }
  sf_writef_float(sf, interleaved.data(), size);
  sf_close(sf);
  std::cout << "Generated impulse: " << output << std::endl;
}

static void GenerateSine(const char* output, float freq,
                         uint32_t sample_rate = 48000) {
  const size_t size = sample_rate;
  std::vector<float> buffer(size);

  for (size_t i = 0; i < size; i++) {
    float phase = 2.0f * M_PI * freq * i / sample_rate;
    buffer[i] = 0.5f * sinf(phase);
  }

  SF_INFO sfinfo = {};
  sfinfo.samplerate = sample_rate;
  sfinfo.channels = 2;
  sfinfo.format = SF_FORMAT_WAV | SF_FORMAT_PCM_16;

  SNDFILE* sf = sf_open(output, SFM_WRITE, &sfinfo);
  if (!sf) {
    std::cerr << "Error: Could not create " << output << std::endl;
    return;
  }
  std::vector<float> interleaved(size * 2, 0.0f);
  for (size_t i = 0; i < size; i++) {
    interleaved[i * 2] = buffer[i];
    interleaved[i * 2 + 1] = buffer[i];
  }
  sf_writef_float(sf, interleaved.data(), size);
  sf_close(sf);
  std::cout << "Generated sine " << freq << "Hz: " << output << std::endl;
}

static void GenerateNoise(const char* output, uint32_t sample_rate = 48000) {
  const size_t size = sample_rate;
  std::vector<float> buffer(size);

  for (size_t i = 0; i < size; i++) {
    buffer[i] = (rand() / float(RAND_MAX)) * 2.0f - 1.0f;
  }

  SF_INFO sfinfo = {};
  sfinfo.samplerate = sample_rate;
  sfinfo.channels = 2;
  sfinfo.format = SF_FORMAT_WAV | SF_FORMAT_PCM_16;

  SNDFILE* sf = sf_open(output, SFM_WRITE, &sfinfo);
  if (!sf) {
    std::cerr << "Error: Could not create " << output << std::endl;
    return;
  }
  std::vector<float> interleaved(size * 2, 0.0f);
  for (size_t i = 0; i < size; i++) {
    interleaved[i * 2] = buffer[i];
    interleaved[i * 2 + 1] = buffer[i];
  }
  sf_writef_float(sf, interleaved.data(), size);
  sf_close(sf);
  std::cout << "Generated white noise: " << output << std::endl;
}

static void ProcessWav(const char* input_file, const char* output_file) {
  SF_INFO sfinfo = {};
  SNDFILE* sf = sf_open(input_file, SFM_READ, &sfinfo);
  if (!sf) {
    std::cerr << "Error: Could not open " << input_file << std::endl;
    return;
  }

  std::cout << "Input: " << input_file << std::endl;
  std::cout << "  Samples: " << sfinfo.frames << std::endl;
  std::cout << "  Channels: " << sfinfo.channels << std::endl;
  std::cout << "  Sample rate: " << sfinfo.samplerate << std::endl;

  sf_count_t total_samples = sfinfo.frames * sfinfo.channels;
  std::vector<float> input(total_samples);
  sf_readf_float(sf, input.data(), sfinfo.frames);
  sf_close(sf);

  // Stereo pass-through (replace with unit DSP once implemented)
  std::vector<float> output = input;

  sfinfo.format = SF_FORMAT_WAV | SF_FORMAT_PCM_16;
  SNDFILE* sf_out = sf_open(output_file, SFM_WRITE, &sfinfo);
  if (!sf_out) {
    std::cerr << "Error: Could not create " << output_file << std::endl;
    return;
  }
  sf_writef_float(sf_out, output.data(), sfinfo.frames);
  sf_close(sf_out);

  std::cout << "Output: " << output_file << std::endl;
}

int main(int argc, char** argv) {
  if (argc < 2) {
    std::cerr << "Usage: " << argv[0] << " input.wav output.wav" << std::endl;
    std::cerr << "       " << argv[0] << " --generate-impulse out.wav" << std::endl;
    std::cerr << "       " << argv[0] << " --generate-sine out.wav <freq_hz>" << std::endl;
    std::cerr << "       " << argv[0] << " --generate-noise out.wav" << std::endl;
    return 1;
  }

  if (strcmp(argv[1], "--generate-impulse") == 0 && argc >= 3) {
    GenerateImpulse(argv[2]);
    return 0;
  }
  if (strcmp(argv[1], "--generate-sine") == 0 && argc >= 4) {
    GenerateSine(argv[2], atof(argv[3]));
    return 0;
  }
  if (strcmp(argv[1], "--generate-noise") == 0 && argc >= 3) {
    GenerateNoise(argv[2]);
    return 0;
  }

  if (argc < 3) {
    std::cerr << "Error: Missing output file argument" << std::endl;
    return 1;
  }

  ProcessWav(argv[1], argv[2]);
  return 0;
}
