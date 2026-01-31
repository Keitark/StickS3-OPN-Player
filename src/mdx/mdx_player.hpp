#pragma once
#include <cstddef>
#include <cstdint>
#include <string>
#include <array>

#include "../app_config.hpp"
#include "../common/meter_state.hpp"

class OPMState;

extern "C" {
#include <mamedef.h>
#include <mdx.h>
#include <mdx_driver.h>
#include <fm_opm_driver.h>
#include <pcm_timer_driver.h>
#include <adpcm_driver.h>
#include <ym2151.h>
}

class MDXPlayer {
public:
  MDXPlayer();

  bool load(uint8_t* data, size_t size, OPMState& state);
  void stop();

  bool playing() const { return playing_; }
  void render_mono(int16_t* dst, int n);
  const std::string& title() const { return title_; }

private:
  struct OPMEmuDriver {
    fm_opm_driver fm{};
    ym2151 opm{};
    OPMState* state = nullptr;
  };

  uint8_t* data_ = nullptr;
  size_t size_ = 0;
  bool playing_ = false;
  std::string title_;

  mdx_file mdx_{};
  mdx_driver driver_{};
  pcm_timer_driver timer_{};
  adpcm_driver adpcm_{};
  OPMEmuDriver opm_{};

  std::array<stream_sample_t, AUDIO_BLOCK_SAMPLES> bufL_{};
  std::array<stream_sample_t, AUDIO_BLOCK_SAMPLES> bufR_{};

  static void opm_write_(fm_opm_driver* driver, uint8_t reg, uint8_t val);
  void reset_internal_();
};
