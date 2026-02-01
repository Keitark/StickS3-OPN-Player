#pragma once
#include <cstddef>
#include <cstdint>

constexpr uint32_t OUT_SR = 44100;
constexpr size_t   AUDIO_BLOCK_SAMPLES = 1024;
constexpr uint8_t  AUDIO_CHANNEL = 0;
// MDX render rates (portable_mdx output rate).
constexpr uint32_t MDX_RENDER_SR_DEFAULT = 22050;
constexpr uint32_t MDX_RENDER_SR_PCM = 22050;
// MDX low-pass (Q15). 0 disables. Higher = brighter but more aliasing.
constexpr int32_t MDX_LPF_ALPHA_Q15 = 22938; // ~0.70
constexpr bool MDX_ENABLE_PCM = true;
// Temporary: set true to listen to PCM-only (mute FM/OPM output)
constexpr bool MDX_PCM_ONLY = false;
// OPM mix gain (Q15). X68Sound applies its own internal scaling.
constexpr int32_t MDX_OPM_GAIN_Q15 = 32768; // 1.0
// PCM mix gain (Q15). We upshift 12-bit to 16-bit (+4) for 1:1 scale.
constexpr int32_t MDX_PCM_GAIN_Q15 = 32768; // 1.0
constexpr int32_t MDX_PCM_GAIN_SHIFT = 4;   // +12-bit -> 16-bit
constexpr size_t   MDX_RENDER_BLOCK_SAMPLES = AUDIO_BLOCK_SAMPLES;

// Audio buffering (ms / us)
constexpr int32_t  AUDIO_TARGET_BUFFER_MS = 500;
constexpr int32_t  AUDIO_MIN_BUFFER_MS    = 300;
constexpr uint32_t AUDIO_PUMP_BUDGET_US   = 12000;
// Heavier PCM8 mixes need more buffering/budget
constexpr int32_t  AUDIO_TARGET_BUFFER_MS_PCM = 700;
constexpr int32_t  AUDIO_MIN_BUFFER_MS_PCM    = 450;
constexpr uint32_t AUDIO_PUMP_BUDGET_US_PCM   = 20000;

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

constexpr int SPEC_COLS = 32;

// UI layout/timing
constexpr uint32_t UI_FPS_MS = 33;  // 30fps
constexpr uint32_t UI_FPS_MS_PCM = 50;  // 20fps when PCM8 is active
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
