#include <M5Unified.h>
#include <LittleFS.h>
#include <array>
#include <string>
#include <string.h>

#include "app_config.hpp"

#include "vgm/track_manager.hpp"
#include "vgm/vgm_blob.hpp"
#include "vgm/vgm_player.hpp"

#include "opn/opn_state.hpp"
#include "opm/opm_state.hpp"
#include "dsp/spectrum.hpp"
#include "ui/ui_renderer.hpp"

#include "audio/audio_engine.hpp"
#include "ym2203_wrap.hpp"
#include "mdx/mdx_blob.hpp"
#include "mdx/mdx_player.hpp"

// ===================== Globals =====================
static TrackManager tracks;
static VGMBlob blob;
static VGMPlayer player;
static MDXBlob mdx_blob;
static MDXPlayer mdx_player;

static OPNState opn_state;
static OPMState opm_state;
static Spectrum spec;
static UIRenderer ui;

static AudioEngine audio;
static YM2203Wrap* chip = nullptr;
static bool is_mdx = false;

static uint32_t last_ui = 0;
static int volume = VOLUME_DEFAULT;

static uint32_t last_vol_tick = 0;
static uint32_t last_vol_show = 0;


// ===== resample state (chip_sr -> OUT_SR) =====
static uint32_t rs_step_fp = 0;   // 16.16 fixed: chip_sr/OUT_SR
static uint32_t rs_frac = 0;
static int16_t  rs_last = 0;

// ===== MDX render/downsample state (MDX_RENDER_SR -> OUT_SR) =====
static std::array<int16_t, MDX_RENDER_BLOCK_SAMPLES> mdx_buf{};
static size_t mdx_buf_pos = 0;
static size_t mdx_buf_len = 0;
static uint32_t mdx_rs_step_fp = 0;  // 16.16 fixed: mdx_sr/OUT_SR
static uint32_t mdx_rs_pos_fp = 0;
static int16_t mdx_rs_s_1 = 0;
static int16_t mdx_rs_s0 = 0;
static int16_t mdx_rs_s1 = 0;
static int16_t mdx_rs_s2 = 0;
static bool mdx_rs_ready = false;
static uint32_t mdx_render_sr = MDX_RENDER_SR_DEFAULT;
static int32_t mdx_lpf_y_q15 = 0;

// ===================== Helpers =====================
static bool ends_with_i(const std::string& s, const char* suf) {
  size_t a = s.size();
  size_t b = strlen(suf);
  if (a < b) return false;
  return strcasecmp(s.c_str() + (a - b), suf) == 0;
}

static bool load_current_track() {
  if (tracks.empty()) return false;

  const std::string& path = tracks.current();
  if (ends_with_i(path, ".mdx")) {
    is_mdx = true;
    blob.clear();
    delete chip;
    chip = nullptr;

    if (!mdx_blob.load_from_file(path.c_str())) return false;
    opm_state.reset();
    spec.reset();
    mdx_player.stop();
    if (!mdx_player.load(mdx_blob.data(), mdx_blob.size(), opm_state, path.c_str())) return false;
    mdx_buf_pos = 0;
    mdx_buf_len = 0;
    mdx_render_sr = mdx_player.render_sample_rate();
    if (mdx_render_sr == 0) mdx_render_sr = MDX_RENDER_SR_DEFAULT;
    mdx_rs_step_fp = (uint32_t)(((uint64_t)mdx_render_sr << 16) / OUT_SR);
    mdx_rs_pos_fp = 0;
    mdx_rs_s_1 = 0;
    mdx_rs_s0 = 0;
    mdx_rs_s1 = 0;
    mdx_rs_s2 = 0;
    mdx_rs_ready = false;
    mdx_lpf_y_q15 = 0;
    spec.set_bin_scale((float)mdx_render_sr / (float)OUT_SR);
    return true;
  }

  is_mdx = false;
  mdx_blob.clear();
  mdx_player.stop();
  if (!blob.load_from_file(path.c_str())) return false;

  const uint8_t* d = blob.data();
  if (!d || blob.size() < 0x100) return false;

  // YM2203 clock in VGM header (0x44)
  uint32_t clk = (uint32_t)d[0x44] | ((uint32_t)d[0x45] << 8) | ((uint32_t)d[0x46] << 16) | ((uint32_t)d[0x47] << 24);
  if (clk == 0) return false;

  delete chip;
  //chip = new YM2203Wrap(clk, ymfm::OPN_FIDELITY_MIN);
  chip = new YM2203Wrap(clk, ymfm::OPN_FIDELITY_MIN);

  opn_state.reset();
  spec.reset();
  spec.set_bin_scale(1.0f);

  if (!player.load(blob.data(), blob.size(), *chip, opn_state)) return false;

  // reset resampler for new track/clock
  rs_step_fp = 0;
  rs_frac = 0;
  rs_last = 0;

  return true;
}

// ===== resample state (chip_sr -> OUT_SR) =====
//static uint32_t rs_step_fp = 0;   // chip_sr/OUT_SR (16.16)
static uint32_t rs_pos_fp  = 0;   // 0..65535
static int16_t  rs_s0 = 0, rs_s1 = 0;

static inline int16_t lerp_i16(int16_t a, int16_t b, uint32_t t16) {
  int32_t da = (int32_t)b - (int32_t)a;
  int32_t v  = (int32_t)a + (da * (int32_t)t16 >> 16);
  if (v < -32768) v = -32768;
  if (v >  32767) v =  32767;
  return (int16_t)v;
}

static inline int16_t cubic_i16(int16_t s_1, int16_t s0, int16_t s1, int16_t s2, uint32_t t16) {
  int64_t t = t16;
  int64_t t2 = (t * t) >> 16;
  int64_t t3 = (t2 * t) >> 16;
  int64_t a0 = 2LL * s0;
  int64_t a1 = (int64_t)(-s_1 + s1);
  int64_t a2 = (int64_t)(2 * s_1 - 5 * s0 + 4 * s1 - s2);
  int64_t a3 = (int64_t)(-s_1 + 3 * s0 - 3 * s1 + s2);
  int64_t y = a0 + ((a1 * t) >> 16) + ((a2 * t2) >> 16) + ((a3 * t3) >> 16);
  y >>= 1;
  if (y < -32768) y = -32768;
  if (y > 32767) y = 32767;
  return (int16_t)y;
}

static inline int16_t mdx_lpf(int16_t x) {
  if (MDX_LPF_ALPHA_Q15 <= 0) return x;
  int32_t y = mdx_lpf_y_q15;
  int32_t xq = ((int32_t)x) << 15;
  y = y + (int32_t)(((int64_t)MDX_LPF_ALPHA_Q15 * (xq - y)) >> 15);
  mdx_lpf_y_q15 = y;
  int32_t out = y >> 15;
  if (out < -32768) out = -32768;
  if (out > 32767) out = 32767;
  return (int16_t)out;
}

static inline int16_t mdx_next_sample() {
  if (!mdx_player.playing()) return 0;
  if (mdx_buf_pos >= mdx_buf_len) {
    mdx_buf_len = MDX_RENDER_BLOCK_SAMPLES;
    mdx_player.render_mono(mdx_buf.data(), (int)mdx_buf_len);
    mdx_buf_pos = 0;
  }
  return mdx_lpf(mdx_buf[mdx_buf_pos++]);
}

static void init_resampler() {
  uint32_t chip_sr = chip->sample_rate_native();
  rs_step_fp = (uint32_t)(((uint64_t)chip_sr << 16) / OUT_SR);
  rs_pos_fp  = 0;
  rs_s0 = chip->render_one_mono_i16_and_outputs();
  rs_s1 = chip->render_one_mono_i16_and_outputs();
}

static void fill_audio_block(int16_t* dst, int n) {
  if (is_mdx) {
    if (!mdx_player.playing()) {
      for (int i=0;i<n;i++) dst[i]=0;
    } else {
      if (mdx_render_sr == OUT_SR) {
        mdx_player.render_mono(dst, n);
      } else {
        if (mdx_rs_step_fp == 0) {
          mdx_rs_step_fp = (uint32_t)(((uint64_t)mdx_render_sr << 16) / OUT_SR);
        }
        if (!mdx_rs_ready) {
          mdx_rs_s_1 = mdx_next_sample();
          mdx_rs_s0 = mdx_next_sample();
          mdx_rs_s1 = mdx_next_sample();
          mdx_rs_s2 = mdx_next_sample();
          mdx_rs_ready = true;
        }
        for (int i = 0; i < n; ++i) {
          mdx_rs_pos_fp += mdx_rs_step_fp;
          while (mdx_rs_pos_fp >= (1u << 16)) {
            mdx_rs_pos_fp -= (1u << 16);
            mdx_rs_s_1 = mdx_rs_s0;
            mdx_rs_s0 = mdx_rs_s1;
            mdx_rs_s1 = mdx_rs_s2;
            mdx_rs_s2 = mdx_next_sample();
          }
          dst[i] = cubic_i16(mdx_rs_s_1, mdx_rs_s0, mdx_rs_s1, mdx_rs_s2, mdx_rs_pos_fp);
        }
      }
    }
    spec.push_pcm_block(dst, n);
    return;
  }

  if (!chip || !player.playing()) {
    for (int i=0;i<n;i++) dst[i]=0;
    spec.push_pcm_block(dst, n);
    return;
  }

  if (rs_step_fp == 0) init_resampler();

  for (int i=0;i<n;i++) {
    player.step_one_sample(); // VGM時間は44100基準で進める

    // rs_pos_fp が 1.0(65536) 以上進む分だけ chip を進める
    rs_pos_fp += rs_step_fp;
    while (rs_pos_fp >= (1u << 16)) {
      rs_pos_fp -= (1u << 16);
      rs_s0 = rs_s1;
      rs_s1 = chip->render_one_mono_i16_and_outputs();
    }

    dst[i] = lerp_i16(rs_s0, rs_s1, rs_pos_fp);
  }

  spec.push_pcm_block(dst, n);
}


void setup() {
  Serial.begin(115200);
  uint32_t t0 = millis();
  while (!Serial && millis() - t0 < 600) delay(10);

  auto cfg = M5.config();
  M5.begin(cfg);

  M5.Display.setRotation(1);

  // Speaker
  auto spk = M5.Speaker.config();
  spk.sample_rate = OUT_SR;
  spk.dma_buf_len = SPEAKER_DMA_BUF_LEN;
  spk.dma_buf_count = SPEAKER_DMA_BUF_COUNT;
  spk.task_priority = SPEAKER_TASK_PRIORITY;
  spk.task_pinned_core = SPEAKER_TASK_CORE;
  M5.Speaker.config(spk);

  M5.Speaker.begin();
  M5.Speaker.setVolume(volume);

  if (!LittleFS.begin(true)) {
    Serial.println("LittleFS.begin failed");
  }

  opn_state.reset();
  opm_state.reset();

  tracks.scan();
  if (tracks.empty()) {
    Serial.println("No .vgm/.vgz/.mdx in LittleFS root");
  } else {
    bool ok = load_current_track();
    Serial.printf("load_current_track=%d (%s)\n", ok ? 1 : 0, tracks.current().c_str());
  }

  ui.begin(M5.Display);
  audio.begin(OUT_SR, AUDIO_CHANNEL);
}

void loop() {
  M5.update();
  uint32_t now = millis();

  // controls
  bool hold_a = M5.BtnA.isHolding();
  bool hold_b = M5.BtnB.isHolding();

  if (!(hold_a || hold_b)) last_vol_tick = 0;

  if (hold_b && !hold_a) {
    if (last_vol_tick == 0 || now - last_vol_tick >= VOLUME_REPEAT_MS) {
      int next = volume - VOLUME_STEP;
      if (next < VOLUME_MIN) next = VOLUME_MIN;
      if (next != volume) {
        volume = next;
        M5.Speaker.setVolume(volume);
        last_vol_show = now;
      }
      last_vol_tick = now;
    }
  } else if (hold_a && !hold_b) {
    if (last_vol_tick == 0 || now - last_vol_tick >= VOLUME_REPEAT_MS) {
      int next = volume + VOLUME_STEP;
      if (next > VOLUME_MAX) next = VOLUME_MAX;
      if (next != volume) {
        volume = next;
        M5.Speaker.setVolume(volume);
        last_vol_show = now;
      }
      last_vol_tick = now;
    }
  }

  if (M5.BtnA.wasClicked()) { tracks.next(); load_current_track(); }
  if (M5.BtnB.wasClicked()) { tracks.prev(); load_current_track(); }

  // audio pump
  const bool pcm_heavy = is_mdx && mdx_player.pdx_loaded();
  audio.pump(fill_audio_block, pcm_heavy);

  // UI 30fps
  const uint32_t ui_interval = pcm_heavy ? UI_FPS_MS_PCM : UI_FPS_MS;
  if (now - last_ui >= ui_interval) {
    last_ui = now;
    if (is_mdx) {
      bool pcm = mdx_player.pdx_loaded();
      opm_state.set_pcm_enabled(pcm);
      if (pcm) {
        opm_state.set_pcm_mask(mdx_player.pcm_mask());
      } else {
        opm_state.set_pcm_mask(0);
      }
      opm_state.update(now);
    } else {
      opn_state.update(now);
    }
    spec.update(now);
    std::string title;
    if (is_mdx) {
      title = mdx_player.title();
    } else {
      title = blob.gd3_track_name_jp();
      if (title.empty()) title = blob.gd3_track_name_en();
    }
    if (title.empty()) title = tracks.empty() ? std::string("(no track)") : tracks.current();

    const MeterState& meters = is_mdx ? opm_state.meters() : opn_state.meters();
    bool show_vol = (last_vol_show != 0) && ((now - last_vol_show) <= VOLUME_SHOW_MS);
    ui.draw(now,
            spec.state(),
            meters,
            title,
            player.writes(),
            player.position(),
            volume,
            show_vol);
  }
  delay(1);
}
