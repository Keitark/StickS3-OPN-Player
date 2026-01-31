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

void OPMState::update(uint32_t now_ms) {
  for (int i = 0; i < 8; ++i) {
    float lv = keyon_mask_[i] ? 0.65f : 0.0f;
    if (keyon_mask_[i] && now_ms < kick_until_ms_[i]) {
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
}
