#pragma once
#include <cstdint>
#include "../common/meter_state.hpp"

class OPNState {
public:
  void reset();
  void on_write(uint8_t reg, uint8_t data);
  void update(uint32_t now_ms);

  const MeterState& meters() const { return meters_; }

private:
  uint8_t reg_[256]{};

  // KeyOn (0x28)
  uint8_t fm_keyon_mask_[3]{};
  uint8_t fm_keyon_prev_[3]{};
  uint32_t fm_kick_until_ms_[3]{};

  // SSG activity
  bool ssg_prev_active_[3]{};
  uint32_t ssg_kick_until_ms_[3]{};

  // meters
  MeterState meters_{};
  uint32_t hold_ms_[6]{};

  // helpers
  bool ssg_active_(int ch) const;
  float fm_target_(int ch, uint32_t now_ms) const;
  float ssg_target_(int ch, uint32_t now_ms);
};
