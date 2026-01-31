#include "mdx_player.hpp"
#include "../opm/opm_state.hpp"

MDXPlayer::MDXPlayer() = default;

void MDXPlayer::reset_internal_() {
  data_ = nullptr;
  size_ = 0;
  title_.clear();
  playing_ = false;

  mdx_ = {};
  driver_ = {};
  timer_ = {};
  adpcm_ = {};
  opm_ = {};
}

void MDXPlayer::opm_write_(fm_opm_driver* driver, uint8_t reg, uint8_t val) {
  auto* ctx = reinterpret_cast<OPMEmuDriver*>(driver);
  ym2151_write_reg(&ctx->opm, reg, val);
  if (ctx->state) {
    ctx->state->on_write(reg, val);
  }
}

bool MDXPlayer::load(uint8_t* data, size_t size, OPMState& state) {
  reset_internal_();
  if (!data || size < 8) return false;

  data_ = data;
  size_ = size;

  int err = mdx_file_load(&mdx_, data_, (int)size_);
  if (err != MDX_SUCCESS) return false;

  title_.assign(reinterpret_cast<const char*>(mdx_.title), mdx_.title_len);

  adpcm_driver_init(&adpcm_);
  pcm_timer_driver_init(&timer_, OUT_SR);

  opm_.state = &state;
  ym2151_init(&opm_.opm, OPM_CLOCK, OUT_SR);
  ym2151_reset_chip(&opm_.opm);
  opm_.fm.write = opm_write_;
  fm_opm_driver_init(&opm_.fm, nullptr);

  mdx_driver_init(&driver_, &timer_.timer_driver, &opm_.fm.fm_driver, &adpcm_);
  if (mdx_driver_load(&driver_, &mdx_, nullptr) != 0) return false;

  playing_ = true;
  return true;
}

void MDXPlayer::stop() {
  playing_ = false;
}

void MDXPlayer::render_mono(int16_t* dst, int n) {
  if (!dst || n <= 0) return;

  if (!playing_) {
    for (int i = 0; i < n; ++i) dst[i] = 0;
    return;
  }

  int remaining = n;
  int offset = 0;
  while (remaining > 0) {
    int chunk = pcm_timer_driver_estimate(&timer_, remaining);
    if (chunk <= 0 || chunk > remaining) chunk = remaining;

    stream_sample_t* bufs[2] = { bufL_.data(), bufR_.data() };
    ym2151_update_one(&opm_.opm, bufs, chunk);

    for (int i = 0; i < chunk; ++i) {
      int32_t mono = (bufL_[i] + bufR_[i]) / 2;
      if (mono > 32767) mono = 32767;
      if (mono < -32768) mono = -32768;
      dst[offset + i] = (int16_t)mono;
    }

    pcm_timer_driver_advance(&timer_, chunk);

    remaining -= chunk;
    offset += chunk;

    if (driver_.ended) {
      playing_ = false;
      break;
    }
  }

  for (int i = offset; i < n; ++i) dst[i] = 0;
}
