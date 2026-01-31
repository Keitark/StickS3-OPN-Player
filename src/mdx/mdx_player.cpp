#include "mdx_player.hpp"
#include "../opm/opm_state.hpp"
#include <Arduino.h>
#include <LittleFS.h>
#include <string.h>

static bool ieq_str(const std::string& a, const std::string& b) {
  return strcasecmp(a.c_str(), b.c_str()) == 0;
}

MDXPlayer::MDXPlayer() = default;

void MDXPlayer::reset_internal_() {
  if (pdx_data_) {
    ps_free_(pdx_data_);
    pdx_data_ = nullptr;
    pdx_size_ = 0;
  }
  pdx_loaded_ = false;
  data_ = nullptr;
  size_ = 0;
  title_.clear();
  playing_ = false;

  mdx_ = {};
  pdx_ = {};
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

void* MDXPlayer::ps_alloc_(size_t n) {
#if defined(ESP32)
  void* p = heap_caps_malloc(n, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
  if (!p) p = malloc(n);
  return p;
#else
  return malloc(n);
#endif
}

void MDXPlayer::ps_free_(void* p) {
#if defined(ESP32)
  free(p);
#else
  free(p);
#endif
}

std::string MDXPlayer::resolve_pdx_path_(const char* mdx_path) const {
  if (!mdx_.pdx_filename || mdx_.pdx_filename_len == 0 || !mdx_path) return {};

  std::string pdx_name(reinterpret_cast<const char*>(mdx_.pdx_filename), mdx_.pdx_filename_len);
  auto nul = pdx_name.find('\0');
  if (nul != std::string::npos) pdx_name.resize(nul);
  if (pdx_name.empty()) return {};

  std::string base = mdx_path;
  auto slash = base.find_last_of('/');
  std::string dir = (slash == std::string::npos) ? std::string("/") : base.substr(0, slash + 1);

  std::string pdx_with_ext = pdx_name;
  if (pdx_with_ext.find('.') == std::string::npos) {
    pdx_with_ext += ".PDX";
  }

  auto try_path = [&](const std::string& name) -> std::string {
    std::string path = dir + name;
    if (LittleFS.exists(path.c_str())) return path;
    return {};
  };

  std::string path = try_path(pdx_with_ext);
  if (!path.empty()) return path;
  path = try_path(pdx_name);
  if (!path.empty()) return path;

  if (pdx_name.find('.') == std::string::npos) {
    path = try_path(pdx_name + ".pdx");
    if (!path.empty()) return path;
  }

  File root = LittleFS.open(dir.c_str(), "r");
  if (!root) return {};
  File f = root.openNextFile();
  while (f) {
    if (!f.isDirectory()) {
      std::string name = f.name();
      std::string base_name = name;
      auto pos = base_name.find_last_of('/');
      if (pos != std::string::npos) base_name = base_name.substr(pos + 1);

      if (ieq_str(base_name, pdx_name) || ieq_str(base_name, pdx_with_ext) ||
          (pdx_name.find('.') == std::string::npos && ieq_str(base_name, pdx_name + ".pdx"))) {
        if (!name.empty() && name[0] != '/') {
          return dir + name;
        }
        return name;
      }
    }
    f = root.openNextFile();
  }

  return {};
}

bool MDXPlayer::load_pdx_(const char* mdx_path) {
  std::string pdx_path = resolve_pdx_path_(mdx_path);
  if (pdx_path.empty()) return false;

  File f = LittleFS.open(pdx_path.c_str(), "r");
  if (!f) return false;
  size_t sz = (size_t)f.size();
  if (sz == 0) return false;

  pdx_data_ = (uint8_t*)ps_alloc_(sz);
  if (!pdx_data_) return false;

  if (f.read(pdx_data_, sz) != (int)sz) {
    ps_free_(pdx_data_);
    pdx_data_ = nullptr;
    return false;
  }

  if (pdx_file_load(&pdx_, pdx_data_, (int)sz) != 0) {
    ps_free_(pdx_data_);
    pdx_data_ = nullptr;
    return false;
  }

  pdx_size_ = sz;
  pdx_loaded_ = true;
  return true;
}

bool MDXPlayer::load(uint8_t* data, size_t size, OPMState& state, const char* mdx_path) {
  reset_internal_();
  if (!data || size < 8) return false;

  data_ = data;
  size_ = size;

  int err = mdx_file_load(&mdx_, data_, (int)size_);
  if (err != MDX_SUCCESS) return false;

  title_.assign(reinterpret_cast<const char*>(mdx_.title), mdx_.title_len);

  if (mdx_.pdx_filename_len > 0) {
    load_pdx_(mdx_path);
  }

  adpcm_driver_init(&adpcm_);
  pcm_timer_driver_init(&timer_, MDX_RENDER_SR);

  opm_.state = &state;
  ym2151_init(&opm_.opm, OPM_CLOCK, MDX_RENDER_SR);
  ym2151_reset_chip(&opm_.opm);
  opm_.fm.write = opm_write_;
  fm_opm_driver_init(&opm_.fm, nullptr);

  mdx_driver_init(&driver_, &timer_.timer_driver, &opm_.fm.fm_driver, &adpcm_);
  struct pdx_file* pdx_ptr = pdx_loaded_ ? &pdx_ : nullptr;
  if (mdx_driver_load(&driver_, &mdx_, pdx_ptr) != 0) return false;

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

uint8_t MDXPlayer::pcm_mask() const {
  if (!pdx_loaded_) return 0;
  uint8_t mask = 0;
  for (int i = 8; i < 16; ++i) {
    if (driver_.tracks[i].used && driver_.tracks[i].note >= 0) {
      mask |= (uint8_t)(1u << (i - 8));
    }
  }
  return mask;
}
