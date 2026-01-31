#pragma once
#include <cstdint>
#include <string>
#include <M5Unified.h>

struct SpectrumState;
#include "../common/meter_state.hpp"

inline constexpr uint16_t ui_rgb565(uint8_t r, uint8_t g, uint8_t b) {
  return (uint16_t)(((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3));
}


class UIRenderer {
public:
  void begin(M5GFX& display);

  void draw(uint32_t now_ms,
            const SpectrumState& spec,
            const MeterState& meters,
            const std::string& track_name,
            uint32_t wr_count,
            uint32_t pos,
            int volume,
            bool show_volume);

private:
  M5GFX* display_ = nullptr;
  M5Canvas canvas_;

  // theme


  static constexpr uint16_t COL_BG    = ui_rgb565(4,4,16);
  static constexpr uint16_t COL_PANEL = ui_rgb565(10,10,30);
  static constexpr uint16_t COL_FRAME = ui_rgb565(55,55,100);
  static constexpr uint16_t COL_GRID  = ui_rgb565(20,20,45);
  static constexpr uint16_t COL_GRID2 = ui_rgb565(30,30,70);
  static constexpr uint16_t COL_TXT   = ui_rgb565(210,210,235);
  static constexpr uint16_t COL_TXT2  = ui_rgb565(150,150,180);
  static constexpr uint16_t COL_PEAK  = ui_rgb565(210,210,245);
  static constexpr uint16_t COL_HOLD  = ui_rgb565(245,245,255);
  static constexpr uint16_t COL_BAR1  = ui_rgb565(45,30,110);
  static constexpr uint16_t COL_BAR2  = ui_rgb565(70,70,140);
  static constexpr uint16_t COL_BAR3  = ui_rgb565(110,110,180);
  static constexpr uint16_t COL_BAR4  = ui_rgb565(150,150,210);
  static constexpr uint16_t COL_S1    = ui_rgb565(70,80,150);
  static constexpr uint16_t COL_S2    = ui_rgb565(110,120,190);
  static constexpr uint16_t COL_S3    = ui_rgb565(150,170,230);

  uint16_t bar_grad_(float t) const;
  uint16_t spec_grad_(float t) const;

  uint32_t title_start_ms_ = 0;
  int title_scroll_px_ = 0;
  std::string last_title_;

  void draw_db_grid_(int x,int y,int w,int h);
  void draw_segment_bar_v_(int x,int y,int w,int h,float v,float p,float hold);
};
