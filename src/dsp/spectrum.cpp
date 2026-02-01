#include "spectrum.hpp"
#include "fft64.hpp"
#include "../app_config.hpp"
#include <math.h>
#include <string.h>

static inline float clamp01(float x){ return x<0?0:(x>1?1:x); }
static inline float to_db(float x){ return 20.0f*log10f(x+1e-6f); }
static inline float db_norm(float db){
  if(db<-60) db=-60;
  if(db>0) db=0;
  return (db+60.0f)/60.0f;
}

void Spectrum::reset(){
  memset(ring_,0,sizeof(ring_));
  w_=0;
  memset(mag32_,0,sizeof(mag32_));
  st_ = {};
  memset(hold_ms_,0,sizeof(hold_ms_));
}

void Spectrum::set_bin_scale(float scale){
  if (scale < 0.1f) scale = 0.1f;
  if (scale > 1.0f) scale = 1.0f;
  bin_scale_ = scale;
}

void Spectrum::push_pcm_block(const int16_t* pcm, int n){
  for(int i=0;i<n;i++){
    ring_[w_ & 1023] = pcm[i];
    w_++;
  }
}

void Spectrum::compute_(){
  static int16_t win[64];  // ★スタック節約
  uint32_t start = w_ - 64;
  for(int i=0;i<64;i++){
    win[i] = ring_[(start+i)&1023];
  }
  FFT64::mag64(win, mag32_);
}

void Spectrum::update(uint32_t now_ms){
  // さらに軽くしたいなら更新頻度も落とせる（例: 120ms）
  //static uint32_t last=0; if(now_ms-last<66) return; last=now_ms;

  compute_();

  // 32列 = 32bin (0..31)。DC(0)は見た目いらないので飛ばして使う。
  for(int c=0;c<32;c++){
    int bin = (int)(c * bin_scale_);
    if (bin == 0) bin = 1;
    if (bin > 31) bin = 31;

    float t = clamp01(db_norm(to_db(mag32_[bin])));

    // きびきび（attack/release速め）
    float diff = t - st_.val[c];
    st_.val[c] += (diff>0 ? 0.70f : 0.25f) * diff;
    st_.val[c] = clamp01(st_.val[c]);

    float pd = st_.val[c] - st_.peak[c];
    st_.peak[c] += (pd>0 ? 0.60f : 0.25f) * pd;
    st_.peak[c] = clamp01(st_.peak[c]);

    if(st_.peak[c] > st_.hold[c]){
      st_.hold[c] = st_.peak[c];
      hold_ms_[c] = now_ms;
    } else if (now_ms - hold_ms_[c] > PEAK_HOLD_MS) {
      st_.hold[c] *= 0.90f;
      st_.hold[c] = clamp01(st_.hold[c]);
    }
  }
}
