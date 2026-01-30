#pragma once
#include <cstddef>
#include <cstdint>

constexpr uint32_t OUT_SR = 44100;
constexpr size_t   AUDIO_BLOCK_SAMPLES = 1024;
constexpr uint8_t  AUDIO_CHANNEL = 0;

// Audio buffering (ms / us)
constexpr int32_t  AUDIO_TARGET_BUFFER_MS = 400;
constexpr int32_t  AUDIO_MIN_BUFFER_MS    = 250;
constexpr uint32_t AUDIO_PUMP_BUDGET_US   = 4000;

// Speaker config
constexpr uint16_t SPEAKER_DMA_BUF_LEN   = 1024;
constexpr uint8_t  SPEAKER_DMA_BUF_COUNT = 8;
constexpr uint8_t  SPEAKER_TASK_PRIORITY = 3;
constexpr uint8_t  SPEAKER_TASK_CORE     = 1;

// Volume control
constexpr int      VOLUME_DEFAULT  = 200;
constexpr int      VOLUME_MIN      = 0;
constexpr int      VOLUME_MAX      = 255;
constexpr int      VOLUME_STEP     = 5;
constexpr uint32_t VOLUME_REPEAT_MS = 120;
constexpr uint32_t VOLUME_SHOW_MS   = 1500;

constexpr int PARTS = 6;     // FM1..3 + SSG A..C
constexpr int SPEC_COLS = 32;

// UI layout/timing
constexpr uint32_t UI_FPS_MS = 33;  // 10fps
constexpr int UI_HEADER_H = 16;
constexpr int UI_GAP = 4;
constexpr int UI_SPEC_H = 78;
constexpr int UI_PARTS_H = 44;
constexpr int UI_SEP_H = 2;
constexpr int UI_SPEC_PAD_L = 28;
constexpr int UI_PARTS_PAD_L = 28;
constexpr int UI_TITLE_SCROLL_PX_PER_SEC = 30;
constexpr uint32_t UI_TITLE_SCROLL_WAIT_MS = 1000;
constexpr int UI_TITLE_SCROLL_GAP_PX = 24;
constexpr int UI_MIN_SPEC_H = 54;

// “きびきび”メータ
constexpr float M_ATTACK  = 0.75f;
constexpr float M_RELEASE = 0.25f;   // 落ちも速く
constexpr uint32_t PEAK_HOLD_MS = 120;
