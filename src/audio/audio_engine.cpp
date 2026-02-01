#include "audio_engine.hpp"
#include <M5Unified.h>
#include "../app_config.hpp"

void AudioEngine::begin(uint32_t sample_rate, uint8_t channel) {
  sr_ = sample_rate;
  ch_ = channel;
}

void AudioEngine::pump(FillFn fill, bool heavy) {
  const uint32_t now = millis();

  // 経過分だけ貯金を減らす（雑だけど効く）
  if (last_ms_ != 0) {
    int32_t dec = (int32_t)(now - last_ms_);
    buffered_ms_ -= dec;
    if (buffered_ms_ < 0) buffered_ms_ = 0;
  }
  last_ms_ = now;

  // 再生が止まってたら貯金0扱い
  if (M5.Speaker.isPlaying(ch_) == 0) {
    buffered_ms_ = 0;
  }

  // 目標貯金（ms）：重いときは多めが安定
  const int32_t TARGET_MS = heavy ? AUDIO_TARGET_BUFFER_MS_PCM : AUDIO_TARGET_BUFFER_MS;
  const int32_t MIN_MS    = heavy ? AUDIO_MIN_BUFFER_MS_PCM : AUDIO_MIN_BUFFER_MS;
  const int32_t CHUNK_MS  = (int32_t)((1000LL * AUDIO_BLOCK_SAMPLES) / sr_);

  // 1回のpumpで使う時間上限（UIを止めない）
  const uint32_t t0 = micros();
  const uint32_t BUDGET_US = heavy ? AUDIO_PUMP_BUDGET_US_PCM : AUDIO_PUMP_BUDGET_US;

  // 貯金が足りない時だけ詰める。詰めたらその分貯金を増やす。
  while (buffered_ms_ < TARGET_MS) {
    if (M5.Speaker.isPlaying(ch_) == 2) break; // キュー満杯なら終了

    // UI優先で時間切れなら一旦戻る
    if ((micros() - t0) > BUDGET_US && buffered_ms_ >= MIN_MS) break;

    int16_t* p = buf_[idx_];
    idx_ = (idx_ + 1) % BUFS;

    fill(p, (int)AUDIO_BLOCK_SAMPLES);
    M5.Speaker.playRaw(p, AUDIO_BLOCK_SAMPLES, sr_, false, 1, ch_, false);

    buffered_ms_ += CHUNK_MS;
  }
}
