#pragma once
#include <cstdint>
#include <functional>

class AudioEngine {
public:
  using FillFn = std::function<void(int16_t* dst, int n)>;

  void begin(uint32_t sample_rate, uint8_t channel);
  void pump(FillFn fill, bool heavy);

private:
  uint32_t sr_ = 44100;
  uint8_t ch_ = 0;

  uint32_t last_ms_ = 0;
  int32_t buffered_ms_ = 0;  // いま貯金してる再生時間(ms)の推定
  
  static constexpr int BUFS = 48;
  static constexpr int N = 1024;
  int16_t buf_[BUFS][N];
  int idx_ = 0;
};
