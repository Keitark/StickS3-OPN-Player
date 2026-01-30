# M5StickS3 FM Player (VGM)

## 概要
本リポジトリは ESP32-S3 の M5Stack デバイス向けファームウェアです。LittleFS に保存した VGM/VGZ トラックを再生し、簡易 UI（スペクトラム・チップメーターなど）を表示します。PlatformIO の環境は `m5sticks3`（board: `m5stack-stamps3`）で、Arduino フレームワークを使用します。対応フォーマットは単一チップの YM2203 (OPN) の VGM/VGZ のみです。

## 特長
- YMFM エミュレータによる YM2203 (OPN) 再生
- LittleFS の `.vgm` / `.vgz` をスキャンして再生
- 画面にトラック名、スペクトラム、チップ活動量を表示
- ボタン操作で前後・音量調整（調整中は音量を表示）

## ハードウェア / 必要環境
- `platformio.ini` に対応する ESP32-S3 M5Stack デバイス（board `m5stack-stamps3`）
- PlatformIO CLI (`pio`)

## クイックスタート
1) `data/` に VGM/VGZ を配置します。
2) ビルド & フラッシュ:

```bash
pio run -e m5sticks3
pio run -t upload
```

3) LittleFS をアップロード:

```bash
pio run -t uploadfs
```

4) シリアルモニタ:

```bash
pio device monitor -b 115200
```

## ビルド手順
- PlatformIO は `platformio.ini` を読み、`lib_deps` の依存関係を初回ビルド時に `.pio/libdeps` へ取得します。
- `pio run -e m5sticks3` で Arduino/ESP32-S3 用ツールチェーンでビルドします。
- `pio run -t upload` でファームを書き込み、`pio run -t uploadfs` で LittleFS を書き込みます。

## 使い方
- `BtnA`（短押し）: 次のトラック
- `BtnA`（長押し）: 音量アップ（変更中は `VOL` を表示）
- `BtnB`（短押し）: 前のトラック
- `BtnB`（長押し）: 音量ダウン（変更中は `VOL` を表示）

## プロジェクト構成
- `src/`: ファームのソース（エントリ: `main.cpp`）
- `src/audio`, `src/dsp`, `src/opn`, `src/ui`, `src/vgm`: 機能別モジュール
- `data/`: LittleFS 用データ（トラック）
- `lib/`: ローカルライブラリ（YMFM は PlatformIO で取得）

## ライセンスに関する注意
- YMFM は BSD 3-Clause（PlatformIO で取得）。ソース/バイナリ配布時はライセンス表記が必要です。
- `M5Unified` / `M5GFX` は MIT、`M5GFX` の一部フォントは BSD ライセンスです。バイナリ配布時はそれぞれの表記が必要です。
- `data/` 内の音源は著作権物の可能性があります。配布する場合は権利を確認してください。
