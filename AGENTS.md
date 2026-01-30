# Repository Guidelines

## Project Structure & Module Organization
- `src/` has the firmware entry (`main.cpp`) and modules under `audio/`, `dsp/`, `opn/`, `ui/`, and `vgm/`.
- `data/` stores LittleFS assets (for example `.vgm`/`.vgz` tracks) uploaded to the device filesystem.
- `lib/` is optional for local libraries; YMFM is pulled via `lib_deps` in `platformio.ini`.
- `platformio.ini` defines the `env:m5sticks3` PlatformIO config; `.pio/` and most `.vscode/` files are generated.

## Build, Test, and Development Commands
- `pio run` (or `pio run -e m5sticks3`) builds the firmware.
- `pio run -t upload` flashes firmware; `pio run -t uploadfs` flashes `data/` to LittleFS.
- `pio device monitor -b 115200` opens serial logs (matches `monitor_speed`).

## Coding Style & Naming Conventions
- C++17 is required (`-std=gnu++17`); keep allocations predictable for embedded targets.
- 2-space indentation, compact functions, lower_snake_case filenames; headers `.hpp`, sources `.cpp`.
- Prefer `constexpr`/`static` where practical for stability and clarity.

## Testing Guidelines
- No automated tests configured; validate by building, flashing, and checking serial output + UI behavior on-device.

## Task & Workflow (GitHub)
- No local git history exists here; if using GitHub, create/update an Issue for non-trivial changes.
- Issue titles: `[Bug][<Area>] <component>: <symptom>` / `[Feat][<Area>] <component>: <what>` / `[Chore] <component>: <what>`.
- Issue body: Background/Goal, Current behavior, Expected behavior, Steps to reproduce (bugs), Acceptance criteria, Notes/logs/links.
- Labels: `type: bug|feature|chore` required; optional `priority: P0|P1|P2`, `area: ui|backend|infra|docs`.
- Branches: `fix/<issue>-<slug>`, `feat/<issue>-<slug>`, `chore/<issue>-<slug>`.
- PR title: `<type>(<scope>): <summary> (#<issue>)`, type in `fix|feat|chore|refactor|docs|test`.
- PR description: link issue (`Fixes #<issue>` only when complete, otherwise `Refs #<issue>`), include checklist (manual test, automated tests, impact/risks); delete branches after merge.
- Commit messages: short, imperative subjects; add a brief body when behavior changes.

## Configuration & Data Notes
- Keep `data/` assets small; LittleFS size is limited by `default_8MB.csv`.
- Document board setting changes in `platformio.ini` comments.
