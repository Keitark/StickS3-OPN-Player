#include "opm_state.hpp"
#include "../app_config.hpp"
#include <Arduino.h>
#include <string.h>
#include <math.h>

static inline float clamp01(float x){ return x<0?0:(x>1?1:x); }

void OPMState::reset() {
  memset(reg_, 0, sizeof(reg_));
  memset(keyon_mask_, 0, sizeof(keyon_mask_));
  memset(kick_until_ms_, 0, sizeof(kick_until_ms_));
  pcm_mask_ = 0;
  memset(pcm_kick_until_ms_, 0, sizeof(pcm_kick_until_ms_));
  pcm_enabled_ = false;
  meters_ = {};
  meters_.count = 8;
  memset(hold_ms_, 0, sizeof(hold_ms_));
}

void OPMState::on_write(uint8_t reg, uint8_t data) {
  reg_[reg] = data;
  if (reg == 0x08) {
    uint8_t ch = data & 0x07;
    uint8_t mask = (data >> 3) & 0x0F;
    keyon_mask_[ch] = mask;
    if (mask != 0) {
      kick_until_ms_[ch] = (uint32_t)(millis() + 80);
    }
  }
}

void OPMState::set_fm_keyon(uint8_t ch, bool keyon, bool kick) {
  if (ch >= 8) return;
  keyon_mask_[ch] = keyon ? 0x0F : 0x00;
  if (kick) {
    kick_until_ms_[ch] = (uint32_t)(millis() + 80);
  }
}

void OPMState::set_pcm_enabled(bool enabled) {
  pcm_enabled_ = enabled;
  meters_.count = enabled ? 16 : 8;
  if (!enabled) {
    pcm_mask_ = 0;
    memset(pcm_kick_until_ms_, 0, sizeof(pcm_kick_until_ms_));
  }
}

void OPMState::set_pcm_mask(uint8_t mask) {
  if (!pcm_enabled_) return;
  uint8_t rising = mask & ~pcm_mask_;
  if (rising) {
    uint32_t now = millis();
    for (int i = 0; i < 8; ++i) {
      if (rising & (1u << i)) {
        pcm_kick_until_ms_[i] = now + 80;
      }
    }
  }
  pcm_mask_ = mask;
}

void OPMState::update(uint32_t now_ms) {
  for (int i = 0; i < 8; ++i) {
    bool kicked = now_ms < kick_until_ms_[i];
    float lv = (keyon_mask_[i] || kicked) ? 0.65f : 0.0f;
    if (kicked) {
      lv = fminf(1.0f, lv + 0.20f);
    }

    float diff = lv - meters_.val[i];
    meters_.val[i] += (diff > 0 ? M_ATTACK : M_RELEASE) * diff;
    meters_.val[i] = clamp01(meters_.val[i]);

    float pd = meters_.val[i] - meters_.peak[i];
    meters_.peak[i] += (pd > 0 ? 0.55f : 0.20f) * pd;
    meters_.peak[i] = clamp01(meters_.peak[i]);

    if (meters_.peak[i] > meters_.hold[i]) {
      meters_.hold[i] = meters_.peak[i];
      hold_ms_[i] = now_ms;
    } else if (now_ms - hold_ms_[i] > PEAK_HOLD_MS) {
      meters_.hold[i] *= 0.90f;
      meters_.hold[i] = clamp01(meters_.hold[i]);
    }
  }

  if (!pcm_enabled_) return;

  for (int i = 0; i < 8; ++i) {
    int idx = 8 + i;
    float lv = (pcm_mask_ & (1u << i)) ? 0.65f : 0.0f;
    if ((pcm_mask_ & (1u << i)) && now_ms < pcm_kick_until_ms_[i]) {
      lv = fminf(1.0f, lv + 0.20f);
    }

    float diff = lv - meters_.val[idx];
    meters_.val[idx] += (diff > 0 ? M_ATTACK : M_RELEASE) * diff;
    meters_.val[idx] = clamp01(meters_.val[idx]);

    float pd = meters_.val[idx] - meters_.peak[idx];
    meters_.peak[idx] += (pd > 0 ? 0.55f : 0.20f) * pd;
    meters_.peak[idx] = clamp01(meters_.peak[idx]);

    if (meters_.peak[idx] > meters_.hold[idx]) {
      meters_.hold[idx] = meters_.peak[idx];
      hold_ms_[idx] = now_ms;
    } else if (now_ms - hold_ms_[idx] > PEAK_HOLD_MS) {
      meters_.hold[idx] *= 0.90f;
      meters_.hold[idx] = clamp01(meters_.hold[idx]);
    }
  }
}
