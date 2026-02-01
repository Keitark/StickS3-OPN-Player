#pragma once
#include <cstddef>
#include <cstdint>
#include <string>
#include <array>

#include "../app_config.hpp"
#include "../common/meter_state.hpp"

class OPMState;

extern "C" {
#include <mdx_util.h>
#include <mxdrv.h>
#include <mxdrv_context.h>
}

class MDXPlayer {
public:
  MDXPlayer();

  bool load(uint8_t* data, size_t size, OPMState& state, const char* mdx_path);
  void stop();

  bool playing() const { return playing_; }
  bool pdx_loaded() const { return pdx_loaded_; }
  uint32_t render_sample_rate() const { return render_sr_; }
  uint8_t pcm_mask() const;
  void render_mono(int16_t* dst, int n);
  const std::string& title() const { return title_; }

private:
  bool playing_ = false;
  bool pdx_loaded_ = false;
  uint8_t pcm_mask_ = 0;
  std::string title_;
  OPMState* opm_state_ = nullptr;
  uint32_t render_sr_ = 0;

  MxdrvContext ctx_{};
  bool ctx_ready_ = false;
  uint32_t ctx_pool_size_ = 0;

  uint8_t* mdx_buffer_ = nullptr;
  uint32_t mdx_buffer_size_ = 0;
  uint8_t* pdx_buffer_ = nullptr;
  uint32_t pdx_buffer_size_ = 0;

  std::array<int16_t, MDX_RENDER_BLOCK_SAMPLES * 2> pcm_interleaved_{};

  void reset_internal_();
  bool ensure_context_(uint32_t mdx_buf_size, uint32_t pdx_buf_size, uint32_t render_sr);
  std::string resolve_pdx_path_(const char* mdx_path, const char* pdx_name) const;
  void poll_opm_regs_();
  void poll_pcm_keyon_();
  static void* ps_alloc_(size_t n);
  static void ps_free_(void* p);
};
