#include "ui_renderer.hpp"
#include "../dsp/spectrum.hpp"
#include "../common/meter_state.hpp"
#include "../app_config.hpp"

static inline float clamp01(float x){ return x<0?0:(x>1?1:x); }

void UIRenderer::begin(M5GFX& display) {
  display_ = &display;

  canvas_.setColorDepth(16);
  canvas_.createSprite(display.width(), display.height());
  canvas_.setFont(&fonts::lgfxJapanGothic_12);
  canvas_.setTextSize(1);
  canvas_.fillScreen(COL_BG);

  // ★親を明示してpush
  canvas_.pushSprite(display_, 0, 0);
}


uint16_t UIRenderer::bar_grad_(float t) const {
  if(t<0.25f) return COL_BAR1;
  if(t<0.55f) return COL_BAR2;
  if(t<0.80f) return COL_BAR3;
  return COL_BAR4;
}
uint16_t UIRenderer::spec_grad_(float t) const {
  if(t<0.33f) return COL_S1;
  if(t<0.66f) return COL_S2;
  return COL_S3;
}

// 0/-20/-40/-60 の横線＋左にラベル
void UIRenderer::draw_db_grid_(int x,int y,int w,int h){
  canvas_.drawRect(x,y,w,h,COL_FRAME);

  struct Mark{ int db; };
  Mark marks[]={{0},{-20},{-40},{-60}};
  for(auto m: marks){
    float t = (m.db + 60.0f)/60.0f; // 0..1
    if(t<0) t=0; if(t>1) t=1;
    int yy = y + 1 + (int)((1.0f - t) * (h-2));
    canvas_.drawFastHLine(x+1, yy, w-2, (m.db==0)?COL_GRID2:COL_GRID);

    canvas_.setTextColor(COL_TXT2, COL_BG);
    canvas_.setCursor(x-26, yy-4);
    canvas_.printf("%2d", m.db);
  }
}

void UIRenderer::draw_segment_bar_v_(int x,int y,int w,int h,float v,float p,float hold){
  v=clamp01(v); p=clamp01(p); hold=clamp01(hold);
  const int SEGS=16;
  int segH=(h-2)/SEGS; if(segH<2) segH=2;

  canvas_.fillRect(x+1,y+1,w-2,h-2,COL_PANEL);

  int filled=(int)(v*SEGS+0.5f); if(filled<0)filled=0; if(filled>SEGS)filled=SEGS;
  for(int s=0;s<filled;s++){
    float t=(float)s/(float)SEGS;
    uint16_t c=bar_grad_(t);
    int yy=y+h-2-(s+1)*segH;
    canvas_.fillRect(x+2,yy+1,w-4,segH-1,c);
  }

  int pseg=(int)(p*SEGS+0.5f); if(pseg<0)pseg=0; if(pseg>SEGS)pseg=SEGS;
  int py=y+h-2-pseg*segH;
  canvas_.drawFastHLine(x+2,py,w-4,COL_PEAK);

  int hseg=(int)(hold*SEGS+0.5f); if(hseg<0)hseg=0; if(hseg>SEGS)hseg=SEGS;
  int hy=y+h-2-hseg*segH;
  canvas_.drawFastHLine(x+2,hy,w-4,COL_HOLD);
}

void UIRenderer::draw(uint32_t now_ms,
                      const SpectrumState& spec,
                      const MeterState& meters,
                      const std::string& track_name,
                      uint32_t wr_count,
                      uint32_t pos,
                      int volume,
                      bool show_volume)
{
  (void)now_ms;
  int W=canvas_.width();
  int H=canvas_.height();

  // layout固定（かぶり防止）
  const int headerH = UI_HEADER_H;
  const int gap = UI_GAP;
  int specH = UI_SPEC_H;
  int partsH = UI_PARTS_H;
  int sepH = UI_SEP_H;
  int parts = meters.count > 0 ? meters.count : 1;
  bool pcm = (parts == 16);
  if (pcm) {
    specH = UI_SPEC_H - 12;
    partsH = UI_PARTS_H + 16;
  }

  int specY  = headerH + gap;
  int sepY   = specY + specH;
  int partsY = sepY + sepH;

  // 画面が小さい場合に自動縮小
  int maxPartsY = H - partsH - 2;
  if(partsY > maxPartsY){
    int overflow = partsY - maxPartsY;
    specH = specH - overflow;
    if(specH < UI_MIN_SPEC_H) specH = UI_MIN_SPEC_H;
    sepY = specY + specH;
    partsY = sepY + sepH;
  }

  // clear
  canvas_.fillScreen(COL_BG);

// header
canvas_.fillRect(0, 0, W, headerH, COL_PANEL);
canvas_.drawRect(0, 0, W, headerH, COL_FRAME);

// wrapを切る（次の行に出るのを防止）
canvas_.setTextWrap(false, false);

// 左タイトル（末尾にスペースを入れる）
const char* app = "StickS3 FM Player ";   // ←スペース入り
canvas_.setTextColor(COL_TXT, COL_PANEL);
canvas_.setCursor(4, 3);
canvas_.print(app);

// 左タイトルの右端を見積もって、スクロール領域の開始位置にする
// textWidthが使えるならそれがベスト。無ければ 6px/文字で近似。
int leftW = 0;
#if defined(LGFX_VERSION_MAJOR) || defined(M5GFX_VERSION)
  leftW = canvas_.textWidth(app);   // M5GFX/LovyanGFXにあることが多い
#else
  leftW = (int)strlen(app) * 6;
#endif

int x0 = 4 + leftW;          // 左タイトルの直後
int statusW = 0;
std::string status;
if (show_volume) {
  status = "VOL " + std::to_string(volume);
  statusW = canvas_.textWidth(status.c_str()) + 8;
}
int x1 = W - 6 - statusW;
int areaW = x1 - x0;
if (areaW < 20) areaW = 20;  // 保険

// 右側領域だけ背景を塗り直す（左タイトルは保持）
canvas_.fillRect(x0, 1, areaW, headerH - 2, COL_PANEL);

// タイトル更新でスクロールリセット
if (track_name != last_title_) {
  last_title_ = track_name;
  title_start_ms_ = now_ms;
  title_scroll_px_ = 0;
}

// 文字幅の近似（等幅6px）
int textW = (int)track_name.size() * 6;
int drawX = x0;

// クリップ領域を設定して、右側以外には絶対描かせない
canvas_.setClipRect(x0, 0, areaW, headerH);

if (textW > areaW) {
  const int px_per_sec = UI_TITLE_SCROLL_PX_PER_SEC;
  const uint32_t WAIT_MS = UI_TITLE_SCROLL_WAIT_MS;   // ★最初と戻りで1秒待つ
  const int gapPx = UI_TITLE_SCROLL_GAP_PX;

  int loopW = textW + gapPx;
  uint32_t scroll_ms = (uint32_t)((1000LL * loopW) / px_per_sec);
  uint32_t cycle_ms  = WAIT_MS + scroll_ms + WAIT_MS;

  uint32_t elapsed = now_ms - title_start_ms_;
  uint32_t t = elapsed % cycle_ms;

  if (t < WAIT_MS) {
    // 初期待ち：初期位置で静止
    drawX = x0;
  } else if (t < WAIT_MS + scroll_ms) {
    // スクロール区間
    uint32_t tscroll = t - WAIT_MS;
    int ofs = (int)((tscroll * px_per_sec) / 1000);
    ofs %= loopW;
    drawX = x0 - ofs;
  } else {
    // 戻り待ち：初期位置で静止
    drawX = x0;
  }
}


canvas_.setTextColor(COL_TXT2, COL_PANEL);
canvas_.setCursor(drawX, 3);
canvas_.print(track_name.c_str());

if (textW > areaW) {
  canvas_.setCursor(drawX + textW + 24, 3);
  canvas_.print(track_name.c_str());
}

// クリップ解除
canvas_.clearClipRect();

  if (show_volume) {
    int sx = W - statusW - 2;
    int sy = 2;
    int sh = headerH - 4;
    canvas_.fillRoundRect(sx, sy, statusW, sh, 3, COL_GRID2);
    canvas_.setTextColor(COL_TXT, COL_GRID2);
    canvas_.setCursor(sx + 4, 3);
    canvas_.print(status.c_str());
  }
  // separator
  canvas_.fillRect(0, sepY, W, sepH, COL_GRID2);


  // ===== Spectrum panel =====
  //canvas_.setTextColor(COL_TXT, COL_BG);
  //canvas_.setCursor(4, specY-2);
  //canvas_.print("SPECTRUM");

  int specPadL = UI_SPEC_PAD_L;
  int specX = specPadL;
  int specW = W - specPadL - 4;
  int specBoxY = specY;
  int specBoxH = specH;

  //draw_db_grid_(specX, specBoxY, specW, specBoxH);

  int innerX = specX + 1;
  int innerY = specBoxY + 1;
  int innerW = specW - 2;
  int innerH = specBoxH - 2;

  // vertical grid every 4 cols
  int colW = innerW / SPEC_COLS;
  if(colW < 2) colW = 2;
  for(int c=0;c<=SPEC_COLS;c+=4){
    int gx = innerX + c*colW;
    canvas_.drawFastVLine(gx, innerY, innerH, COL_GRID);
  }

  // bars as segments
  const int SSEG=16;
  int segH = (innerH-2)/SSEG; if(segH<2) segH=2;

  for(int c=0;c<SPEC_COLS;c++){
    float v = spec.val[c];
    float p = spec.peak[c];
    float hld = spec.hold[c];

    int x = innerX + c*colW;
    int bw = colW-1; if(bw<1) bw=1;
    canvas_.fillRect(x, innerY, bw, innerH, COL_PANEL);

    int filled=(int)(v*SSEG+0.5f); if(filled<0)filled=0; if(filled>SSEG)filled=SSEG;
    for(int s=0;s<filled;s++){
      float t=(float)s/(float)SSEG;
      uint16_t cc = spec_grad_(t);
      int yy = innerY + innerH - 2 - (s+1)*segH;
      canvas_.fillRect(x, yy, bw, segH-1, cc);
    }

    int pseg=(int)(p*SSEG+0.5f); if(pseg<0)pseg=0; if(pseg>SSEG)pseg=SSEG;
    int py = innerY + innerH - 2 - pseg*segH;
    canvas_.drawFastHLine(x, py, bw, COL_PEAK);

    int hseg=(int)(hld*SSEG+0.5f); if(hseg<0)hseg=0; if(hseg>SSEG)hseg=SSEG;
    int hy = innerY + innerH - 2 - hseg*segH;
    canvas_.drawFastHLine(x, hy, bw, COL_HOLD);
  }

  // ===== Parts panel =====
  //canvas_.setTextColor(COL_TXT, COL_BG);
  //canvas_.setCursor(4, partsY-2);
  //canvas_.print("PART");

  int pPadL = UI_PARTS_PAD_L;
  int px = pPadL;
  int pw = W - pPadL - 4;
  int py = partsY;
  int ph = partsH;

  //draw_db_grid_(px, py, pw, ph);

  int inX = px + 1;
  int inY = py + 1;
  int inW = pw - 2;
  int inH = ph - 2;

  int gapX = 6;
  int rowCount = pcm ? 2 : 1;
  int barsPerRow = pcm ? 8 : parts;
  int rowGap = pcm ? 4 : 0;
  int rowH = (inH - rowGap) / rowCount;
  if (rowH < 18) rowH = 18;

  int barW = (inW - gapX * (barsPerRow - 1)) / barsPerRow;
  if (barW < 10) barW = 10;

  const char* lab6[6]={"FM1","FM2","FM3","SSG1","SSG2","SSG3"};
  const char* lab8[8]={"FM1","FM2","FM3","FM4","FM5","FM6","FM7","FM8"};
  const char* labpcm[8]={"P08","P09","P10","P11","P12","P13","P14","P15"};

  for (int row = 0; row < rowCount; ++row) {
    int base = row * 8;
    int by = inY + row * (rowH + rowGap);
    int bh = rowH - 12;
    if (bh < 8) bh = 8;

    for (int i = 0; i < barsPerRow; ++i) {
      int idx = base + i;
      if (idx >= parts) break;

      int bx = inX + i * (barW + gapX);
      draw_segment_bar_v_(bx, by, barW, bh, meters.val[idx], meters.peak[idx], meters.hold[idx]);

      const char* label = nullptr;
      if (pcm && row == 1) {
        label = labpcm[i];
      } else if (parts == 6) {
        label = lab6[i];
      } else {
        label = lab8[i];
      }
      if (label) {
        canvas_.setTextColor(COL_TXT, COL_BG);
        canvas_.setCursor(bx, by + bh + 2);
        canvas_.print(label);
      }
    }
  }

  canvas_.pushSprite(display_, 0, 0);

}
