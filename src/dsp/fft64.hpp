#pragma once
#include <cstdint>

class FFT64 {
public:
  // input: PCM 64 samples
  // output: mag 32 bins
  static void mag64(const int16_t* pcm64, float* mag32);
};
