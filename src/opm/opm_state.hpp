#pragma once
#include <cstdint>
#include "../common/meter_state.hpp"

class OPMState {
public:
  void reset();
  void on_write(uint8_t reg, uint8_t data);
  void update(uint32_t now_ms);

  const MeterState& meters() const { return meters_; }

private:
  uint8_t reg_[256]{};
  uint8_t keyon_mask_[8]{};
  uint32_t kick_until_ms_[8]{};

  MeterState meters_{};
  uint32_t hold_ms_[8]{};
};
