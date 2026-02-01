#include "mdx_player.hpp"
#include "../app_config.hpp"
#include "../opm/opm_state.hpp"
#include <Arduino.h>
#include <LittleFS.h>
#include <algorithm>
#include <limits>
#include <string.h>
#include <stdlib.h>

static bool ieq_str(const std::string& a, const std::string& b) {
  return strcasecmp(a.c_str(), b.c_str()) == 0;
}

namespace {
constexpr uint32_t kPoolMarginBytes = 512 * 1024;
constexpr uint32_t kPoolMinBytes = 2 * 1024 * 1024;
}  // namespace

MDXPlayer::MDXPlayer() = default;

void MDXPlayer::reset_internal_() {
  if (ctx_ready_) {
    MXDRV_Stop(&ctx_);
    MXDRV_End(&ctx_);
    MxdrvContext_Terminate(&ctx_);
    ctx_ready_ = false;
    ctx_pool_size_ = 0;
  }

  if (mdx_buffer_) {
    ps_free_(mdx_buffer_);
    mdx_buffer_ = nullptr;
    mdx_buffer_size_ = 0;
  }
  if (pdx_buffer_) {
    ps_free_(pdx_buffer_);
    pdx_buffer_ = nullptr;
    pdx_buffer_size_ = 0;
  }

  playing_ = false;
  pdx_loaded_ = false;
  pcm_mask_ = 0;
  title_.clear();
  opm_state_ = nullptr;
  render_sr_ = 0;
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

std::string MDXPlayer::resolve_pdx_path_(const char* mdx_path, const char* pdx_name) const {
  if (!mdx_path || !pdx_name || pdx_name[0] == '\0') return {};

  std::string pdx_name_str(pdx_name);
  if (pdx_name_str.empty()) return {};

  std::string base = mdx_path;
  auto slash = base.find_last_of('/');
  std::string dir = (slash == std::string::npos) ? std::string("/") : base.substr(0, slash + 1);

  File root = LittleFS.open(dir.c_str(), "r");
  if (root) {
    File f = root.openNextFile();
    while (f) {
      if (!f.isDirectory()) {
        std::string name = f.name();
        std::string base_name = name;
        auto pos = base_name.find_last_of('/');
        if (pos != std::string::npos) base_name = base_name.substr(pos + 1);

        if (ieq_str(base_name, pdx_name_str)) {
          if (!name.empty() && name[0] != '/') {
            return dir + name;
          }
          return name;
        }
      }
      f = root.openNextFile();
    }
  }

  std::string path = dir + pdx_name_str;
  if (LittleFS.exists(path.c_str())) return path;

  return {};
}

bool MDXPlayer::ensure_context_(uint32_t mdx_buf_size, uint32_t pdx_buf_size, uint32_t render_sr) {
  const uint64_t data_bytes = static_cast<uint64_t>(mdx_buf_size) + pdx_buf_size;
  uint64_t pool64 = data_bytes * 2 + kPoolMarginBytes;
  if (pool64 < kPoolMinBytes) pool64 = kPoolMinBytes;
  if (pool64 > static_cast<uint64_t>(std::numeric_limits<int>::max())) {
    pool64 = static_cast<uint64_t>(std::numeric_limits<int>::max());
  }
  const uint32_t pool = static_cast<uint32_t>(pool64);
  if (!MxdrvContext_Initialize(&ctx_, (int)pool)) return false;

  if (MXDRV_Start(&ctx_, (int)render_sr, 0, 0, 0, (int)mdx_buf_size, (int)pdx_buf_size, 0) != 0) {
    MxdrvContext_Terminate(&ctx_);
    return false;
  }

  MXDRV_PCM8Enable(&ctx_, 1);
  MXDRV_TotalVolume(&ctx_, 256);

  ctx_ready_ = true;
  ctx_pool_size_ = pool;
  render_sr_ = render_sr;
  return true;
}

bool MDXPlayer::load(uint8_t* data, size_t size, OPMState& state, const char* mdx_path) {
  reset_internal_();
  if (!data || size < 8) return false;

  opm_state_ = &state;

  char title_buf[128] = {};
  if (MdxGetTitle(data, (uint32_t)size, title_buf, sizeof(title_buf))) {
    title_ = title_buf;
  }

  bool has_pdx = false;
  if (!MdxHasPdxFileName(data, (uint32_t)size, &has_pdx)) return false;
  const uint32_t render_sr = has_pdx ? MDX_RENDER_SR_PCM : MDX_RENDER_SR_DEFAULT;

  uint8_t* pdx_image = nullptr;
  uint32_t pdx_image_size = 0;
  if (has_pdx) {
    char pdx_name[128] = {};
    if (!MdxGetPdxFileName(data, (uint32_t)size, pdx_name, sizeof(pdx_name))) return false;

    std::string pdx_path = resolve_pdx_path_(mdx_path, pdx_name);
    if (pdx_path.empty()) return false;

    File f = LittleFS.open(pdx_path.c_str(), "r");
    if (!f) return false;
    pdx_image_size = (uint32_t)f.size();
    if (pdx_image_size == 0) return false;

    pdx_image = (uint8_t*)ps_alloc_(pdx_image_size);
    if (!pdx_image) return false;
    if (f.read(pdx_image, pdx_image_size) != (int)pdx_image_size) {
      ps_free_(pdx_image);
      return false;
    }
  }

  uint32_t req_mdx = 0;
  uint32_t req_pdx = 0;
  if (!MdxGetRequiredBufferSize(data, (uint32_t)size, pdx_image_size, &req_mdx, &req_pdx)) {
    if (pdx_image) ps_free_(pdx_image);
    return false;
  }

  mdx_buffer_ = (uint8_t*)ps_alloc_(req_mdx);
  if (!mdx_buffer_) {
    if (pdx_image) ps_free_(pdx_image);
    return false;
  }
  mdx_buffer_size_ = req_mdx;

  if (req_pdx > 0) {
    pdx_buffer_ = (uint8_t*)ps_alloc_(req_pdx);
    if (!pdx_buffer_) {
      ps_free_(mdx_buffer_);
      mdx_buffer_ = nullptr;
      if (pdx_image) ps_free_(pdx_image);
      return false;
    }
    pdx_buffer_size_ = req_pdx;
  }

  if (!MdxUtilCreateMdxPdxBuffer(
        data, (uint32_t)size,
        pdx_image, pdx_image_size,
        mdx_buffer_, mdx_buffer_size_,
        pdx_buffer_, pdx_buffer_size_)) {
    if (pdx_image) ps_free_(pdx_image);
    return false;
  }

  if (pdx_image) {
    ps_free_(pdx_image);
    pdx_image = nullptr;
  }

  if (!ensure_context_(mdx_buffer_size_, pdx_buffer_size_, render_sr)) return false;

  if (MXDRV_SetData2(&ctx_, mdx_buffer_, mdx_buffer_size_, pdx_buffer_, pdx_buffer_size_) != 0) return false;

  const bool pdx_loaded = (pdx_buffer_size_ > 0);

  // MXDRV copies data into its own pool; free source buffers to save RAM.
  if (mdx_buffer_) {
    ps_free_(mdx_buffer_);
    mdx_buffer_ = nullptr;
    mdx_buffer_size_ = 0;
  }
  if (pdx_buffer_) {
    ps_free_(pdx_buffer_);
    pdx_buffer_ = nullptr;
    pdx_buffer_size_ = 0;
  }
  MXDRV_Play2(&ctx_);

  pdx_loaded_ = pdx_loaded;
  playing_ = true;
  return true;
}

void MDXPlayer::stop() {
  if (ctx_ready_) {
    MXDRV_Stop(&ctx_);
  }
  playing_ = false;
}

void MDXPlayer::poll_opm_regs_() {
  if (!opm_state_ || !ctx_ready_) return;
  for (int i = 0; i < 256; ++i) {
    uint8_t val = 0;
    bool updated = false;
    if (MxdrvContext_GetOpmReg(&ctx_, (uint8_t)i, &val, &updated) && updated) {
      opm_state_->on_write((uint8_t)i, val);
    }
  }
  for (int ch = 0; ch < 8; ++ch) {
    bool current = false;
    bool logical = false;
    if (MxdrvContext_GetFmKeyOn(&ctx_, (uint8_t)ch, &current, &logical)) {
      opm_state_->set_fm_keyon((uint8_t)ch, current, logical);
    }
  }
}

void MDXPlayer::poll_pcm_keyon_() {
  if (!ctx_ready_) return;
  uint8_t mask = 0;
  for (int ch = 0; ch < 8; ++ch) {
    bool keyon = false;
    if (MxdrvContext_GetPcmKeyOn(&ctx_, (uint8_t)ch, &keyon) && keyon) {
      mask |= (uint8_t)(1u << ch);
    }
  }
  pcm_mask_ = mask;
}

void MDXPlayer::render_mono(int16_t* dst, int n) {
  if (!dst || n <= 0) return;

  if (!playing_) {
    for (int i = 0; i < n; ++i) dst[i] = 0;
    return;
  }

  const int block_samples = (int)MDX_RENDER_BLOCK_SAMPLES;
  int remaining = n;
  int dst_idx = 0;

  while (remaining > 0) {
    int chunk = remaining > block_samples ? block_samples : remaining;
    MXDRV_GetPCM(&ctx_, pcm_interleaved_.data(), chunk);

    for (int i = 0; i < chunk; ++i) {
      int32_t l = pcm_interleaved_[i * 2];
      int32_t r = pcm_interleaved_[i * 2 + 1];
      int32_t mono = (l + r) / 2;
      if (mono > 32767) mono = 32767;
      if (mono < -32768) mono = -32768;
      dst[dst_idx + i] = (int16_t)mono;
    }

    dst_idx += chunk;
    remaining -= chunk;
  }

  poll_opm_regs_();
  poll_pcm_keyon_();

  if (MXDRV_GetTerminated(&ctx_)) {
    playing_ = false;
  }
}

uint8_t MDXPlayer::pcm_mask() const {
  return pdx_loaded_ ? pcm_mask_ : 0;
}
