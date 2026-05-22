# AGENTS.md

## Project Overview

This repository contains open firmware and local tooling for the Scribit drawing
robot. The hardware board is the Briki/MBC-WB board, which combines an ESP32 and
a SAMD21. Firmware is split by chip:

- `Firmware/ScribitESP/`: ESP32 firmware. Handles Wi-Fi, MQTT commands, OTA,
  file download, LED/status behavior, and forwarding G-code work to the motion
  side.
- `Firmware/MK4duo/`: SAMD21 firmware. This is MK4duo/Marlin-derived motion
  firmware with Scribit-specific G-code commands and hardware behavior.

The ESP32 firmware is currently expected to compile by following the documented
setup. The SAMD21/MK4duo firmware is documented as not currently compiling.

## Repository Map

- `README.md`: main setup, firmware build, device configuration, OTA, and
  troubleshooting notes.
- `Firmware/`: source code for both firmware targets.
  - `Firmware/ScribitESP/ScribitESP.ino`: ESP32 Arduino entry point.
  - `Firmware/MK4duo/MK4duo.ino`: SAMD21 Arduino entry point.
  - `Firmware/ScribitESP/data/`: sample G-code files and ESP data payloads.
- `ExtraFile/`: checked-in support files and vendored dependencies needed for
  local Arduino builds, including example config headers, board overrides,
  partition tables, `arduino-mqtt`, and `StepperDriver`.
- `docker/`: Arduino CLI build container and compose file. Prefer this for
  firmware compilation because it pins the Briki board package and hardware
  overrides.
- `tools/scribit-cmd/`: Python 3.13 Typer CLI for controlling a Scribit robot
  over MQTT and serving G-code over HTTP.
- `docs/`: device support archives, developer notes, MQTT command notes, and
  the MBC-WB manual PDF.
- `meteca_mirror/`: mirrored board package/index artifacts.

## Local Setup Files

Some firmware files are intentionally ignored and must be created from examples
before compiling:

```bash
cp ExtraFile/SIConfig.hpp.example Firmware/ScribitESP/SIConfig.hpp
cp ExtraFile/Mk4duoVersion.h.example Firmware/MK4duo/Mk4duoVersion.h
cp ExtraFile/ScribitVersion.hpp.example Firmware/ScribitESP/ScribitVersion.hpp
cp -r ExtraFile/arduino-mqtt Firmware/ScribitESP/
cp -r ExtraFile/StepperDriver Firmware/ScribitESP/
```

Do not commit local secrets or environment-specific values from generated config
headers. `Firmware/ScribitESP/SIConfig.hpp`,
`Firmware/ScribitESP/ScribitVersion.hpp`, and
`Firmware/MK4duo/Mk4duoVersion.h` are ignored by git.

## Firmware Build Commands

Run these from the project root after local setup.

Build ESP32 firmware:

```bash
docker-compose -f docker/docker-compose.yml run --rm scribit-firmware arduino-cli compile --fqbn briki:mbc-wb:mbc:mcu=esp --output-dir /workspace/builds /workspace/source/Firmware/ScribitESP/ScribitESP.ino
```

Build SAMD21 firmware:

```bash
docker-compose -f docker/docker-compose.yml run --rm scribit-firmware arduino-cli compile --fqbn briki:mbc-wb:mbc:mcu=samd --output-dir /workspace/builds /workspace/source/Firmware/MK4duo/MK4duo.ino
```

Build outputs are written to `docker/builds/`. Treat these outputs as generated
artifacts.

## Python Tooling

The `tools/scribit-cmd/` package provides command-line tools for MQTT control
and G-code serving. It uses `uv` and Python `>=3.13`.

Common commands from `tools/scribit-cmd/`:

```bash
uv sync
uv run python main.py --help
uv run pytest
```

The draw command serves files to firmware in the payload shape
`http://<host-ip>/<path>;<suffix>`. Current firmware rejects URLs containing a
port, so real draw jobs usually need the HTTP server on port `80`, which may
require elevated privileges outside normal tests.

## MQTT Notes

Inbound MQTT commands use this topic shape:

```text
tin/<robot-id>/<command>
```

See `docs/dev/mqtt-commands.md` for command details. Important commands include
`print`, `erase`, `status`, `reset`, `pause`, `manualMove`, `calibration`,
`update`, `wifiConfig`, and `resetConfig`.

For `manualMove`, payloads are short semicolon-delimited inline G-code strings.
The firmware payload limit is `SI_MQTT_MAX_PAYLOAD_LEN = 512`.

For `print`, payloads are URL-plus-suffix strings:

```text
http://<host-ip>/<path>;<suffix>
```

## Coding Guidance

- Keep changes scoped to the relevant target: ESP32 behavior in
  `Firmware/ScribitESP/`, SAMD21 motion behavior in `Firmware/MK4duo/`, and host
  tooling in `tools/scribit-cmd/`.
- Prefer the existing Arduino/C++ style in firmware. Much of `Firmware/MK4duo/`
  is vendored or forked firmware; avoid broad refactors there unless required.
- Be careful around MQTT command names and topic suffixes. Keep docs,
  `SIMQTT.hpp`, `SIMQTT.cpp`, and command-handling code in sync when changing
  MQTT behavior.
- Preserve generated/local-only config files. If a build requires missing local
  headers, recreate them from `ExtraFile/*.example` rather than committing
  machine-specific versions.
- For Python tool changes, add or update focused tests under
  `tools/scribit-cmd/tests/` and run `uv run pytest` from `tools/scribit-cmd/`.
- For firmware changes, use the Docker Arduino CLI workflow when practical and
  report whether ESP32 and/or SAMD21 compilation was run.

