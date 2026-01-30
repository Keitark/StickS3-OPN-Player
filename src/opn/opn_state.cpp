#include "opn_state.hpp"
#include "../app_config.hpp"
#include <math.h>
#include <string.h>
#include <Arduino.h>

static inline float clamp01(float x){ return x<0?0:(x>1?1:x); }

// OPN系でよく使われるスロット並び
static constexpr uint8_t slot_ofs[4] = { 0x00, 0x08, 0x04, 0x0C };

void OPNState::reset() {
  memset(reg_, 0, sizeof(reg_));
  memset(fm_keyon_mask_, 0, sizeof(fm_keyon_mask_));
  memset(fm_keyon_prev_, 0, sizeof(fm_keyon_prev_));
  memset(fm_kick_until_ms_, 0, sizeof(fm_kick_until_ms_));
  memset(ssg_prev_active_, 0, sizeof(ssg_prev_active_));
  memset(ssg_kick_until_ms_, 0, sizeof(ssg_kick_until_ms_));
  meters_ = {};
  memset(hold_ms_, 0, sizeof(hold_ms_));
}

void OPNState::on_write(uint8_t reg, uint8_t data) {
  reg_[reg] = data;

  // KeyOn reg
  if (reg == 0x28) {
    uint8_t ch = data & 0x03; // 0..2
    if (ch < 3) {
      fm_keyon_prev_[ch] = fm_keyon_mask_[ch];
      fm_keyon_mask_[ch] = data & 0xF0; // slot bits
      if (fm_keyon_prev_[ch] == 0 && fm_keyon_mask_[ch] != 0) {
        fm_kick_until_ms_[ch] = (uint32_t)(millis() + 80); // ぴょんは短め
      }
    }
  }
}

bool OPNState::ssg_active_(int ch) const {
  // enable 0x07: bit0..2 tone disable, bit3..5 noise disable
  uint8_t en = reg_[0x07];
  bool tone_on  = ((en >> ch) & 1) == 0;
  bool noise_on = ((en >> (3 + ch)) & 1) == 0;
  uint8_t vol = reg_[0x08 + ch] & 0x0F;
  return (vol > 0) && (tone_on || noise_on);
}

float OPNState::fm_target_(int ch, uint32_t now_ms) const {
  if (fm_keyon_mask_[ch] == 0) return 0.0f;

  // “鳴ってる”同期を重視：ベース固定
  float base = 0.65f;

  // TLで軽く補正（強すぎるとズレるので控えめ）
  int min_tl = 127;
  for (int s = 0; s < 4; ++s) {
    uint8_t r = (uint8_t)(0x40 + slot_ofs[s] + ch);
    int tl = reg_[r] & 0x7F;
    if (tl < min_tl) min_tl = tl;
  }
  float tl_lv = 1.0f - (float)min_tl / 127.0f;   // 0..1
  float lv = base * (0.80f + 0.20f * tl_lv);      // 0.8..1.0倍

  // 短い“ぴょん”
  if (now_ms < fm_kick_until_ms_[ch]) lv = fminf(1.0f, lv + 0.20f);

  return clamp01(lv);
}

float OPNState::ssg_target_(int ch, uint32_t now_ms) {
  uint8_t v = reg_[0x08 + ch] & 0x0F;  // 0..15
  float lv = (float)v / 15.0f;

  bool active = ssg_active_(ch);
  if (!active) lv = 0.0f;

  if (!ssg_prev_active_[ch] && active) ssg_kick_until_ms_[ch] = now_ms + 60;
  ssg_prev_active_[ch] = active;

  if (now_ms < ssg_kick_until_ms_[ch]) lv = fminf(1.0f, lv + 0.15f);

  return clamp01(lv);
}

void OPNState::update(uint32_t now_ms) {
  float target[6];
  target[0] = fm_target_(0, now_ms);
  target[1] = fm_target_(1, now_ms);
  target[2] = fm_target_(2, now_ms);
  target[3] = ssg_target_(0, now_ms);
  target[4] = ssg_target_(1, now_ms);
  target[5] = ssg_target_(2, now_ms);

  for (int i = 0; i < 6; ++i) {
    float diff = target[i] - meters_.val[i];
    meters_.val[i] += (diff > 0 ? M_ATTACK : M_RELEASE) * diff;
    meters_.val[i] = clamp01(meters_.val[i]);

    float pd = meters_.val[i] - meters_.peak[i];
    meters_.peak[i] += (pd > 0 ? 0.55f : 0.20f) * pd;
    meters_.peak[i] = clamp01(meters_.peak[i]);

    if (meters_.peak[i] > meters_.hold[i]) {
      meters_.hold[i] = meters_.peak[i];
      hold_ms_[i] = now_ms;
    } else if (now_ms - hold_ms_[i] > PEAK_HOLD_MS) {
      meters_.hold[i] *= 0.90f;     // きびきび落とす
      meters_.hold[i] = clamp01(meters_.hold[i]);
    }
  }
}
