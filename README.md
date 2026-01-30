# M5StickS3 FM Player (VGM)

## Overview
This repository contains firmware for an ESP32-S3 M5Stack device that plays VGM/VGZ tracks from LittleFS and renders a simple UI with spectrum and chip meters. The target PlatformIO environment is `m5sticks3` (board: `m5stack-stamps3`) using the Arduino framework. Only single-chip YM2203 (OPN) VGM/VGZ files are supported.

## Features
- YM2203 (OPN) playback via the YMFM emulator.
- LittleFS track browser for `.vgm`/`.vgz` files.
- On-device UI: track title, spectrum, and chip activity meters.
- Button controls for previous/next and volume (on-screen volume indicator while adjusting).

## Hardware & Requirements
- ESP32-S3 M5Stack device matching `platformio.ini` (board `m5stack-stamps3`).
- PlatformIO CLI (`pio`).

## Quick Start
1) Put VGM/VGZ files in `data/`.
2) Build and flash firmware:

```bash
pio run -e m5sticks3
pio run -t upload
```

3) Upload filesystem assets:

```bash
pio run -t uploadfs
```

4) Open the serial monitor:

```bash
pio device monitor -b 115200
```

## Build Process
- PlatformIO reads `platformio.ini` and fetches dependencies listed in `lib_deps` on the first build (into `.pio/libdeps`).
- `pio run -e m5sticks3` compiles the firmware using the configured Arduino/ESP32-S3 toolchain.
- `pio run -t upload` flashes the firmware; `pio run -t uploadfs` flashes LittleFS assets.

## Usage
- `BtnA` (short press): next track
- `BtnA` (long press): volume up (shows `VOL` while changing)
- `BtnB` (short press): previous track
- `BtnB` (long press): volume down (shows `VOL` while changing)

## Project Structure
- `src/`: firmware sources (entry: `main.cpp`)
- `src/audio`, `src/dsp`, `src/opn`, `src/ui`, `src/vgm`: feature modules
- `data/`: LittleFS assets (tracks)
- `lib/`: optional local libraries (not required for YMFM; fetched via PlatformIO).

## License Notes (Assessment)
- YMFM is BSD 3-Clause (fetched via PlatformIO). Keep its copyright and license text in source/binary distributions.
- PlatformIO dependencies `M5Unified` and `M5GFX` are MIT-licensed; `M5GFX` bundles Adafruit GFX fonts under a BSD-style license. If you distribute binaries, include their notices.
- Track files in `data/` may be copyrighted. Only distribute audio assets you have rights to.
