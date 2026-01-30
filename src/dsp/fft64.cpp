#include "fft64.hpp"
#include <math.h>

static float re_[64];
static float im_[64];

void FFT64::mag64(const int16_t* pcm64, float* mag32) {
  float* re = re_;
  float* im = im_;

  // window + copy
  for (int i = 0; i < 64; i++) {
    float x = (float)pcm64[i] / 32768.0f;
    // Hann
    float w = 0.5f - 0.5f * cosf(2.0f * (float)M_PI * i / 63.0f);
    re[i] = x * w;
    im[i] = 0.0f;
  }

  // bit-reverse
  int j = 0;
  for (int i = 0; i < 64; i++) {
    if (i < j) {
      float tr = re[i]; re[i] = re[j]; re[j] = tr;
      float ti = im[i]; im[i] = im[j]; im[j] = ti;
    }
    int m = 32;
    while (m >= 1 && j >= m) { j -= m; m >>= 1; }
    j += m;
  }

  // FFT
  for (int len = 2; len <= 64; len <<= 1) {
    float ang = -2.0f * (float)M_PI / (float)len;
    float wlen_r = cosf(ang);
    float wlen_i = sinf(ang);
    for (int i = 0; i < 64; i += len) {
      float wr = 1.0f, wi = 0.0f;
      for (int k = 0; k < len / 2; ++k) {
        int a = i + k;
        int b = i + k + len / 2;

        float ur = re[a], ui = im[a];
        float vr = re[b] * wr - im[b] * wi;
        float vi = re[b] * wi + im[b] * wr;

        re[a] = ur + vr;
        im[a] = ui + vi;
        re[b] = ur - vr;
        im[b] = ui - vi;

        float nwr = wr * wlen_r - wi * wlen_i;
        float nwi = wr * wlen_i + wi * wlen_r;
        wr = nwr; wi = nwi;
      }
    }
  }

  // magnitude: 0..31
  for (int i = 0; i < 32; i++) {
    float m = sqrtf(re[i] * re[i] + im[i] * im[i]);
    mag32[i] = m;
  }
}
