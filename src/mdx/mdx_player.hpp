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
#include <pdx.h>
#include <ym2151.h>
}

class MDXPlayer {
public:
  MDXPlayer();

  bool load(uint8_t* data, size_t size, OPMState& state, const char* mdx_path);
  void stop();

  bool playing() const { return playing_; }
  bool pdx_loaded() const { return pdx_loaded_; }
  uint8_t pcm_mask() const;
  void render_mono(int16_t* dst, int n);
  const std::string& title() const { return title_; }

private:
  struct OPMEmuDriver {
    fm_opm_driver fm{};
    ym2151 opm{};
    OPMState* state = nullptr;
  };

  struct ADPCMDriverCtx {
    adpcm_driver driver{};
    MDXPlayer* owner = nullptr;
  };

  struct PCMChannel {
    const int16_t* samples = nullptr;
    int length = 0;
    uint32_t pos_fp = 0;
    uint32_t step_fp = 0;
    uint8_t volume = 0;
    uint16_t volume_scale = 0;
    uint8_t freq = 4;
    bool active = false;
  };

  uint8_t* data_ = nullptr;
  size_t size_ = 0;
  bool playing_ = false;
  std::string title_;

  mdx_file mdx_{};
  pdx_file pdx_{};
  uint8_t* pdx_data_ = nullptr;
  size_t pdx_size_ = 0;
  bool pdx_loaded_ = false;
  mdx_driver driver_{};
  pcm_timer_driver timer_{};
  ADPCMDriverCtx adpcm_{};
  OPMEmuDriver opm_{};
  PCMChannel pcm_channels_[8]{};

  std::array<stream_sample_t, AUDIO_BLOCK_SAMPLES> bufL_{};
  std::array<stream_sample_t, AUDIO_BLOCK_SAMPLES> bufR_{};

  static void opm_write_(fm_opm_driver* driver, uint8_t reg, uint8_t val);
  static int adpcm_play_(adpcm_driver* d, uint8_t channel, uint8_t* data, int len, uint8_t freq, uint8_t vol);
  static int adpcm_stop_(adpcm_driver* d, uint8_t channel);
  static int adpcm_set_volume_(adpcm_driver* d, uint8_t channel, uint8_t vol);
  static int adpcm_set_freq_(adpcm_driver* d, uint8_t channel, uint8_t freq);
  static int adpcm_set_pan_(adpcm_driver* d, uint8_t pan);

  void reset_internal_();
  bool load_pdx_(const char* mdx_path);
  std::string resolve_pdx_path_(const char* mdx_path) const;
  static void* ps_alloc_(size_t n);
  static void ps_free_(void* p);

  void reset_pcm_();
  const pdx_sample* find_pdx_sample_(uint8_t* data, int len) const;
  uint32_t adpcm_step_from_freq_(uint8_t freq) const;
  uint8_t normalize_adpcm_volume_(uint8_t vol) const;
  int32_t mix_pcm_sample_();
};
