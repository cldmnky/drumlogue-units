// WAV file I/O wrapper using libsndfile
// For local DSP testing of clouds-revfx

#ifndef TEST_WAV_FILE_H_
#define TEST_WAV_FILE_H_

#include <sndfile.h>
#include <vector>
#include <string>
#include <cmath>
#include <cstdio>
#include <cstdint>

class WavFile {
 public:
  WavFile() : file_(nullptr), sample_rate_(48000), channels_(2), frames_(0) {}
  ~WavFile() { Close(); }

  // Open a WAV file for reading
  bool OpenRead(const std::string& path) {
    Close();
    SF_INFO info = {};
    file_ = sf_open(path.c_str(), SFM_READ, &info);
    if (!file_) {
      fprintf(stderr, "Error opening %s for reading: %s\n", 
              path.c_str(), sf_strerror(nullptr));
      return false;
    }
    sample_rate_ = info.samplerate;
    channels_ = info.channels;
    frames_ = info.frames;
    return true;
  }

  // Open a WAV file for writing
  bool OpenWrite(const std::string& path, int sample_rate = 48000, int channels = 2) {
    Close();
    SF_INFO info = {};
    info.samplerate = sample_rate;
    info.channels = channels;
    info.format = SF_FORMAT_WAV | SF_FORMAT_FLOAT;
    file_ = sf_open(path.c_str(), SFM_WRITE, &info);
    if (!file_) {
      fprintf(stderr, "Error opening %s for writing: %s\n", 
              path.c_str(), sf_strerror(nullptr));
      return false;
    }
    sample_rate_ = sample_rate;
    channels_ = channels;
    frames_ = 0;
    return true;
  }

  void Close() {
    if (file_) {
      sf_close(file_);
      file_ = nullptr;
    }
  }

  // Read interleaved float samples
  // Returns number of frames read
  size_t Read(std::vector<float>& buffer, size_t frames) {
    if (!file_) return 0;
    buffer.resize(frames * channels_);
    sf_count_t read = sf_readf_float(file_, buffer.data(), frames);
    buffer.resize(read * channels_);
    return static_cast<size_t>(read);
  }

  // Read entire file
  bool ReadAll(std::vector<float>& buffer) {
    if (!file_) return false;
    buffer.resize(frames_ * channels_);
    sf_count_t read = sf_readf_float(file_, buffer.data(), frames_);
    return read == static_cast<sf_count_t>(frames_);
  }

  // Write interleaved float samples
  // Returns number of frames written
  size_t Write(const float* data, size_t frames) {
    if (!file_) return 0;
    sf_count_t written = sf_writef_float(file_, data, frames);
    frames_ += written;
    return static_cast<size_t>(written);
  }

  size_t Write(const std::vector<float>& buffer) {
    return Write(buffer.data(), buffer.size() / channels_);
  }

  int sample_rate() const { return sample_rate_; }
  int channels() const { return channels_; }
  size_t frames() const { return frames_; }

  // Generate test signals
  static bool GenerateImpulse(const std::string& path, int sample_rate = 48000, 
                               float duration_sec = 1.0f) {
    WavFile wav;
    if (!wav.OpenWrite(path, sample_rate, 2)) return false;
    
    size_t frames = static_cast<size_t>(duration_sec * sample_rate);
    std::vector<float> buffer(frames * 2, 0.0f);
    
    // Single impulse at the start (both channels)
    buffer[0] = 1.0f;  // Left
    buffer[1] = 1.0f;  // Right
    
    wav.Write(buffer);
    return true;
  }

  static bool GenerateSine(const std::string& path, float frequency = 440.0f,
                           int sample_rate = 48000, float duration_sec = 1.0f,
                           float amplitude = 0.5f) {
    WavFile wav;
    if (!wav.OpenWrite(path, sample_rate, 2)) return false;
    
    size_t frames = static_cast<size_t>(duration_sec * sample_rate);
    std::vector<float> buffer(frames * 2);
    
    const float two_pi = 2.0f * 3.14159265358979323846f;
    for (size_t i = 0; i < frames; ++i) {
      float sample = amplitude * sinf(two_pi * frequency * i / sample_rate);
      buffer[i * 2 + 0] = sample;  // Left
      buffer[i * 2 + 1] = sample;  // Right
    }
    
    wav.Write(buffer);
    return true;
  }

  static bool GenerateNoise(const std::string& path, int sample_rate = 48000,
                            float duration_sec = 1.0f, float amplitude = 0.5f) {
    WavFile wav;
    if (!wav.OpenWrite(path, sample_rate, 2)) return false;
    
    size_t frames = static_cast<size_t>(duration_sec * sample_rate);
    std::vector<float> buffer(frames * 2);
    
    // Simple LCG random number generator
    uint32_t seed = 12345;
    for (size_t i = 0; i < frames; ++i) {
      seed = seed * 1664525 + 1013904223;
      float sample_l = amplitude * (static_cast<float>(seed) / 2147483648.0f - 1.0f);
      seed = seed * 1664525 + 1013904223;
      float sample_r = amplitude * (static_cast<float>(seed) / 2147483648.0f - 1.0f);
      buffer[i * 2 + 0] = sample_l;
      buffer[i * 2 + 1] = sample_r;
    }
    
    wav.Write(buffer);
    return true;
  }

  // Generate a stereo drum loop pattern for testing
  static bool GenerateDrumPattern(const std::string& path, int sample_rate = 48000,
                                   float duration_sec = 2.0f) {
    WavFile wav;
    if (!wav.OpenWrite(path, sample_rate, 2)) return false;
    
    size_t frames = static_cast<size_t>(duration_sec * sample_rate);
    std::vector<float> buffer(frames * 2, 0.0f);
    
    // Generate a simple beat pattern at 120 BPM
    // Beat interval = 0.5 seconds = sample_rate / 2 samples
    size_t beat_samples = sample_rate / 2;
    
    uint32_t seed = 54321;
    for (size_t beat = 0; beat < static_cast<size_t>(duration_sec * 2); ++beat) {
      size_t start = beat * beat_samples;
      if (start >= frames) break;
      
      // Kick on beats 0, 2
      if (beat % 2 == 0) {
        // Simple kick: decaying low freq sine
        for (size_t i = 0; i < 4800 && (start + i) < frames; ++i) {
          float t = static_cast<float>(i) / sample_rate;
          float env = expf(-t * 20.0f);
          float freq = 60.0f + 100.0f * expf(-t * 30.0f);
          float sample = env * sinf(2.0f * 3.14159f * freq * t);
          buffer[(start + i) * 2 + 0] += sample * 0.8f;
          buffer[(start + i) * 2 + 1] += sample * 0.8f;
        }
      }
      
      // Snare on beats 1, 3
      if (beat % 2 == 1) {
        // Simple snare: noise burst
        for (size_t i = 0; i < 3600 && (start + i) < frames; ++i) {
          float t = static_cast<float>(i) / sample_rate;
          float env = expf(-t * 15.0f);
          seed = seed * 1664525 + 1013904223;
          float noise = static_cast<float>(seed) / 2147483648.0f - 1.0f;
          buffer[(start + i) * 2 + 0] += env * noise * 0.4f;
          buffer[(start + i) * 2 + 1] += env * noise * 0.4f;
        }
      }
      
      // Hi-hat on every beat
      for (size_t i = 0; i < 2400 && (start + i) < frames; ++i) {
        float t = static_cast<float>(i) / sample_rate;
        float env = expf(-t * 30.0f);
        seed = seed * 1664525 + 1013904223;
        float noise = static_cast<float>(seed) / 2147483648.0f - 1.0f;
        // High-pass filtered noise
        buffer[(start + i) * 2 + 0] += env * noise * 0.15f;
        buffer[(start + i) * 2 + 1] += env * noise * 0.15f;
      }
    }
    
    // Soft clip
    for (size_t i = 0; i < frames * 2; ++i) {
      float x = buffer[i];
      if (x < -1.0f) x = -1.0f;
      if (x > 1.0f) x = 1.0f;
      buffer[i] = x;
    }
    
    wav.Write(buffer);
    return true;
  }

 private:
  SNDFILE* file_;
  int sample_rate_;
  int channels_;
  sf_count_t frames_;
};

#endif  // TEST_WAV_FILE_H_
