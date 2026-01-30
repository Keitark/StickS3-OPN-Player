#pragma once
#include <cstdint>
#include <cstddef>

class YM2203Wrap;
class OPNState;

class VGMPlayer {
public:
  bool load(const uint8_t* data, size_t size, YM2203Wrap& chip, OPNState& state);

  bool playing() const { return playing_; }
  uint32_t position() const { return pos_; }
  uint32_t writes() const { return wr_count_; }

  // OUT_SR 1サンプル進める（waitを含む）
  void step_one_sample();

private:
  const uint8_t* data_ = nullptr;
  size_t size_ = 0;

  YM2203Wrap* chip_ = nullptr;
  OPNState* state_ = nullptr;

  uint32_t pos_ = 0;
  uint32_t data_start_ = 0;
  uint32_t loop_pos_ = 0;
  uint32_t wait_ = 0;
  bool playing_ = false;

  uint32_t wr_count_ = 0;

  uint8_t  rd8_();
  uint32_t rd32le_at_(uint32_t off) const;
  void step_until_wait_();

  void reset_to_data_();
};
