// WAV file I/O wrapper using libsndfile
// For local DSP testing of elements-synth

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

 private:
  SNDFILE* file_;
  int sample_rate_;
  int channels_;
  size_t frames_;
};

#endif  // TEST_WAV_FILE_H_
