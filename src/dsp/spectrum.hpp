#pragma once
#include <array>
#include <cstdint>

struct SpectrumState {
  std::array<float, 32> val{};
  std::array<float, 32> peak{};
  std::array<float, 32> hold{};
};

class Spectrum {
public:
  void reset();
  void set_bin_scale(float scale);
  void push_pcm_block(const int16_t* pcm, int n);
  void update(uint32_t now_ms);

  const SpectrumState& state() const { return st_; }

private:
  int16_t ring_[1024]{};
  uint32_t w_ = 0;

  float mag32_[32]{};
  SpectrumState st_{};
  uint32_t hold_ms_[32]{};
  float bin_scale_ = 1.0f;

  void compute_();
};
