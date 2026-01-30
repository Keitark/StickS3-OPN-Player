#include <M5Unified.h>
#include <LittleFS.h>
#include <string>

#include "app_config.hpp"

#include "vgm/track_manager.hpp"
#include "vgm/vgm_blob.hpp"
#include "vgm/vgm_player.hpp"

#include "opn/opn_state.hpp"
#include "dsp/spectrum.hpp"
#include "ui/ui_renderer.hpp"

#include "audio/audio_engine.hpp"
#include "ym2203_wrap.hpp"

// ===================== Globals =====================
static TrackManager tracks;
static VGMBlob blob;
static VGMPlayer player;

static OPNState opn_state;
static Spectrum spec;
static UIRenderer ui;

static AudioEngine audio;
static YM2203Wrap* chip = nullptr;

static uint32_t last_ui = 0;
static int volume = VOLUME_DEFAULT;

static uint32_t last_vol_tick = 0;
static uint32_t last_vol_show = 0;


// ===== resample state (chip_sr -> OUT_SR) =====
static uint32_t rs_step_fp = 0;   // 16.16 fixed: chip_sr/OUT_SR
static uint32_t rs_frac = 0;
static int16_t  rs_last = 0;

// ===================== Helpers =====================
static bool load_current_track() {
  if (tracks.empty()) return false;

  const std::string& path = tracks.current();
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

static void init_resampler() {
  uint32_t chip_sr = chip->sample_rate_native();
  rs_step_fp = (uint32_t)(((uint64_t)chip_sr << 16) / OUT_SR);
  rs_pos_fp  = 0;
  rs_s0 = chip->render_one_mono_i16_and_outputs();
  rs_s1 = chip->render_one_mono_i16_and_outputs();
}

static void fill_audio_block(int16_t* dst, int n) {
  if (!chip || !player.playing()) {
    for (int i=0;i<n;i++) dst[i]=0;
    uint32_t now = millis();
    opn_state.update(now);
    spec.push_pcm_block(dst, n);
    spec.update(now);
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

  uint32_t now = millis();
  opn_state.update(now);
  spec.push_pcm_block(dst, n);
  spec.update(now);
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

  tracks.scan();
  if (tracks.empty()) {
    Serial.println("No .vgm/.vgz in LittleFS root");
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
  audio.pump(fill_audio_block);

  // UI 30fps
  if (now - last_ui >= UI_FPS_MS) {
    last_ui = now;
    std::string title = blob.gd3_track_name_en();
    if (title.empty()) title = blob.gd3_track_name_jp();
    if (title.empty()) title = tracks.empty() ? std::string("(no track)") : tracks.current();  // fallback

    bool show_vol = (last_vol_show != 0) && ((now - last_vol_show) <= VOLUME_SHOW_MS);
    ui.draw(now,
            spec.state(),
            opn_state.meters(),
            title,
            player.writes(),
            player.position(),
            volume,
            show_vol);
  }
  delay(1);
}
