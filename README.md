# M5StickS3 FM Player (VGM)
![License](https://img.shields.io/github/license/Keitark/StickS3-OPN-Player)
![Platform](https://img.shields.io/badge/platform-PlatformIO-orange)
![Board](https://img.shields.io/badge/board-M5StickS3-blue)

## Overview
This repository contains firmware for an ESP32-S3 M5Stack device that plays VGM/VGZ (YM2203/OPN) and MDX (YM2151/OPM) tracks from LittleFS and renders a simple UI with spectrum and chip meters. The target PlatformIO environment is `m5sticks3` (board: `m5stack-stamps3`) using the Arduino framework. MDX playback supports PDX/ADPCM when the matching PDX file is present.

## Features
- YM2203 (OPN) playback via the YMFM emulator.
- YM2151 (OPM) MDX playback (PDX/ADPCM supported when PDX is available).
- LittleFS track browser for `.vgm`/`.vgz`/`.mdx` files.
- On-device UI: track title, spectrum, and chip activity meters.
- Button controls for previous/next and volume (on-screen volume indicator while adjusting).

## Hardware & Requirements
- ESP32-S3 M5Stack device matching `platformio.ini` (board `m5stack-stamps3`).
- PlatformIO CLI (`pio`).
- Python 3.x (required by PlatformIO and build helper scripts).

## Quick Start
1) Put VGM/VGZ/MDX files in `data/`.
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
- `src/audio`, `src/common`, `src/dsp`, `src/mdx`, `src/opm`, `src/opn`, `src/ui`, `src/vgm`: feature modules
- `data/`: LittleFS assets (tracks)
- `lib/`: optional local libraries (not required for YMFM; fetched via PlatformIO).

## License Notes (Assessment)
- YMFM is BSD 3-Clause (fetched via PlatformIO). Keep its copyright and license text in source/binary distributions.
- PlatformIO dependencies `M5Unified` and `M5GFX` are MIT-licensed; `M5GFX` bundles Adafruit GFX fonts under a BSD-style license. If you distribute binaries, include their notices.
- MDX playback pulls `portable_mdx` via PlatformIO. Yosshin’s code is Apache-2.0, while MXDRVg/X68Sound-derived parts follow their original terms. If you distribute firmware built with it, review and comply with those licenses (see the upstream `readme.md`).
- Track files in `data/` may be copyrighted. Only distribute audio assets you have rights to.
