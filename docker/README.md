# Scribit Firmware Compilation Environment

A lightweight, headless Docker environment for compiling Scribit firmware using Arduino CLI.

## Quick Start

### Prerequisites
1. Prepare configuration files (one-time setup):
   ```bash
   # Copy example configuration files
   cp ExtraFile/SIConfig.hpp.example Firmware/ScribitESP/SIConfig.hpp
   cp ExtraFile/Mk4duoVersion.h.example Firmware/MK4duo/Mk4duoVersion.h  
   cp ExtraFile/ScribitVersion.hpp.example Firmware/ScribitESP/ScribitVersion.hpp
   
   # Copy required libraries
   cp -r ExtraFile/arduino-mqtt Firmware/ScribitESP/
   cp -r ExtraFile/StepperDriver Firmware/ScribitESP/
   ```

### Compile Firmware

**Using Docker Compose (recommended):**
```bash
# From project root directory:

# ESP32 firmware
docker-compose -f docker/docker-compose.yml run --rm scribit-firmware arduino-cli compile --fqbn briki:mbc-wb:mbc:mcu=esp --output-dir /workspace/builds /workspace/source/Firmware/ScribitESP/ScribitESP.ino

# SAMD21 firmware  
docker-compose -f docker/docker-compose.yml run --rm scribit-firmware arduino-cli compile --fqbn briki:mbc-wb:mbc:mcu=samd --output-dir /workspace/builds /workspace/source/Firmware/MK4duo/MK4duo.ino
```

**Using Direct Docker Commands:**
```bash
# Build the image
docker build -f docker/Dockerfile -t scribit-firmware-builder .

# ESP32 firmware
docker run --rm -v $(pwd):/workspace/source:ro -v $(pwd)/docker/builds:/workspace/builds scribit-firmware-builder arduino-cli compile --fqbn briki:mbc-wb:mbc:mcu=esp --output-dir /workspace/builds /workspace/source/Firmware/ScribitESP/ScribitESP.ino

# SAMD21 firmware
docker run --rm -v $(pwd):/workspace/source:ro -v $(pwd)/docker/builds:/workspace/builds scribit-firmware-builder arduino-cli compile --fqbn briki:mbc-wb:mbc:mcu=samd --output-dir /workspace/builds /workspace/source/Firmware/MK4duo/MK4duo.ino
```

## Usage Options

### Compilation with Verbose Output
```bash
# ESP32 with verbose output
docker-compose -f docker/docker-compose.yml run --rm scribit-firmware arduino-cli compile --fqbn briki:mbc-wb:mbc:mcu=esp --output-dir /workspace/builds --verbose /workspace/source/Firmware/ScribitESP/ScribitESP.ino

# SAMD with verbose output  
docker-compose -f docker/docker-compose.yml run --rm scribit-firmware arduino-cli compile --fqbn briki:mbc-wb:mbc:mcu=samd --output-dir /workspace/builds --verbose /workspace/source/Firmware/MK4duo/MK4duo.ino
```

### Verify Only (No Binary Output)
```bash
# ESP32 verification only
docker-compose -f docker/docker-compose.yml run --rm scribit-firmware arduino-cli compile --fqbn briki:mbc-wb:mbc:mcu=esp --verify /workspace/source/Firmware/ScribitESP/ScribitESP.ino

# SAMD verification only
docker-compose -f docker/docker-compose.yml run --rm scribit-firmware arduino-cli compile --fqbn briki:mbc-wb:mbc:mcu=samd --verify /workspace/source/Firmware/MK4duo/MK4duo.ino
```

### Direct Docker Commands

```bash
# Navigate to project root, then:

# Build the image manually (if not using ./docker/compile)
docker build -f docker/Dockerfile -t scribit-firmware-builder .

# Compile ESP32 firmware
docker run --rm -v $(pwd):/workspace/source:ro -v $(pwd)/docker/builds:/workspace/builds scribit-firmware-builder arduino-cli compile --fqbn briki:mbc-wb:mbc:mcu=esp --output-dir /workspace/builds /workspace/source/Firmware/ScribitESP/ScribitESP.ino

# Compile SAMD firmware  
docker run --rm -v $(pwd):/workspace/source:ro -v $(pwd)/docker/builds:/workspace/builds scribit-firmware-builder arduino-cli compile --fqbn briki:mbc-wb:mbc:mcu=samd --output-dir /workspace/builds /workspace/source/Firmware/MK4duo/MK4duo.ino

# Verify setup
docker run --rm -v $(pwd):/workspace/source:ro scribit-firmware-builder verify-setup

# Interactive shell for debugging
docker run --rm -it -v $(pwd):/workspace/source:ro scribit-firmware-builder bash
```

## Output Files

Compiled firmware files are saved to `./docker/builds/` directory:

- **ESP32 Firmware:** `.bin`, `.hex`, `.elf` files
- **SAMD Firmware:** `.bin`, `.hex`, `.elf` files

### Accessing Build Files

```bash
# Files are directly accessible in the host filesystem
ls -la ./docker/builds/

# Example output files:
# ScribitESP.ino.bin      - ESP32 binary for flashing
# ScribitESP.ino.hex      - ESP32 hex file
# MK4duo.ino.bin          - SAMD binary for flashing  
# MK4duo.ino.hex          - SAMD hex file
```

## Board Configuration

The environment is pre-configured with:

- **ESP32 Board:** `briki:mbc-wb:mbc:mcu=esp`
- **SAMD Board:** `briki:mbc-wb:mbc:mcu=samd`
- **Libraries:** arduino-mqtt, StepperDriver
- **Hardware Overrides:** 8MB partition tables, SERCOM.cpp
- **Configuration Files:** SIConfig.hpp, Mk4duoVersion.h, ScribitVersion.hpp

## Troubleshooting

### Verify Setup
```bash
# Using Docker Compose
docker-compose -f docker/docker-compose.yml run --rm scribit-firmware verify-setup

# Using Docker directly
docker run --rm -v $(pwd):/workspace/source:ro scribit-firmware-builder verify-setup
```

### Check Available Boards
```bash
# Using the Docker image directly  
docker run --rm scribit-firmware-builder arduino-cli board listall | grep briki
```

### Interactive Debugging
```bash
# Using the Docker image directly
docker run --rm -it -v $(pwd):/workspace/source:ro scribit-firmware-builder bash
```

## Architecture

- **Base Image:** Ubuntu 22.04 (lightweight, headless)
- **Arduino CLI:** Version 0.35.3 for compilation
- **Board Packages:** Briki MBC-WB v2.0.0
- **Size:** ~400MB (optimized for CI/CD)

## Prerequisites

- Docker installed and running
- Run commands from the project root directory
- Ensure the following files exist:
  - `ExtraFile/SIConfig.hpp.example`
  - `ExtraFile/Mk4duoVersion.h.example` 
  - `ExtraFile/ScribitVersion.hpp.example`
  - `ExtraFile/arduino-mqtt/` directory
  - `ExtraFile/StepperDriver/` directory

## Summary

✅ **Compilation Tested Successfully:**
- ESP32 firmware: `ScribitESP.ino` → `ScribitESP.ino.bin` (1.05MB)
- SAMD firmware: `MK4duo.ino` → `MK4duo.ino.bin` (101KB)

✅ **Both compilation methods work:**
- Docker Compose (recommended)
- Direct Docker commands

✅ **Output files:**
- `.bin` files for flashing
- `.hex` files for programming
- `.elf` files for debugging

## Files

- `Dockerfile` - Main Docker image definition
- `docker-compose.yml` - Docker Compose configuration  
- `verify-setup.sh` - Environment verification script
- `setup-arduino.sh` - Arduino CLI setup script
- `builds/` - Output directory for compiled firmware
- `.dockerignore` - Files excluded from Docker context