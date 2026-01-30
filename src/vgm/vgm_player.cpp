#include "vgm_player.hpp"
#include "../opn/opn_state.hpp"
#include "../app_config.hpp"
#include "ym2203_wrap.hpp"
#include <string.h>

static inline uint32_t u32le(const uint8_t* p){
  return (uint32_t)p[0] | ((uint32_t)p[1]<<8) | ((uint32_t)p[2]<<16) | ((uint32_t)p[3]<<24);
}

uint32_t VGMPlayer::rd32le_at_(uint32_t off) const {
  if (!data_ || off + 4 > size_) return 0;
  return u32le(data_ + off);
}

uint8_t VGMPlayer::rd8_() {
  if (pos_ >= size_) return 0;
  return data_[pos_++];
}

void VGMPlayer::reset_to_data_() {
  pos_ = data_start_;
  wait_ = 0;
  playing_ = true;
  wr_count_ = 0;
}

bool VGMPlayer::load(const uint8_t* data, size_t size, YM2203Wrap& chip, OPNState& state) {
  data_ = data;
  size_ = size;
  chip_ = &chip;
  state_ = &state;

  if (!data_ || size_ < 0x100) return false;
  if (!(data_[0]=='V' && data_[1]=='g' && data_[2]=='m' && data_[3]==' ')) return false;

  // data offset (0x34 relative to 0x34, v1.50+). if 0 -> 0x40
  uint32_t rel = rd32le_at_(0x34);
  data_start_ = (rel == 0) ? 0x40 : (0x34 + rel);
  if (data_start_ >= size_) return false;

  // loop offset at 0x1C is relative to 0x1C, 0 if none
  uint32_t loop_rel = rd32le_at_(0x1C);
  loop_pos_ = (loop_rel == 0) ? 0 : (0x1C + loop_rel);

  reset_to_data_();
  return true;
}

void VGMPlayer::step_until_wait_() {
  while (playing_ && wait_ == 0 && pos_ < size_) {
    uint8_t cmd = rd8_();

    if (cmd == 0x55) {
      uint8_t aa = rd8_();
      uint8_t dd = rd8_();
      state_->on_write(aa, dd);
      chip_->write_reg(aa, dd);
      wr_count_++;
    }
    else if (cmd == 0x61) {
      uint16_t n = (uint16_t)rd8_();
      n |= (uint16_t)rd8_() << 8;
      wait_ = n;
    }
    else if (cmd == 0x62) wait_ = 735;
    else if (cmd == 0x63) wait_ = 882;
    else if ((cmd & 0xF0) == 0x70) wait_ = (cmd & 0x0F) + 1;
    else if (cmd == 0x66) {
      // end: loop if possible
      if (loop_pos_ != 0 && loop_pos_ < size_) {
        pos_ = loop_pos_;
      } else {
        playing_ = false;
      }
    }
    else if (cmd == 0x67) {
      // data block: 0x67 0x66 tt [size u32] [data...]
      (void)rd8_(); (void)rd8_();
      uint32_t sz = rd32le_at_(pos_);
      pos_ += 4;
      pos_ += sz;
      if (pos_ > size_) { playing_ = false; }
    }
    else {
      // unknown -> stop (必要なら増やす)
      playing_ = false;
    }
  }
}

void VGMPlayer::step_one_sample() {
  if (!playing_) return;
  if (wait_ == 0) step_until_wait_();
  if (wait_ > 0) wait_--;
}
