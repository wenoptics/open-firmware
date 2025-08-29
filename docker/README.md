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
# Build the image first
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

# Build the image manually
docker build -f docker/Dockerfile -t scribit-firmware-builder .

# Compile ESP32 firmware
docker run --rm -v $(pwd):/workspace/source:ro -v $(pwd)/docker/builds:/workspace/builds scribit-firmware-builder arduino-cli compile --fqbn briki:mbc-wb:mbc:mcu=esp --output-dir /workspace/builds /workspace/source/Firmware/ScribitESP/ScribitESP.ino

# Compile SAMD firmware  
docker run --rm -v $(pwd):/workspace/source:ro -v $(pwd)/docker/builds:/workspace/builds scribit-firmware-builder arduino-cli compile --fqbn briki:mbc-wb:mbc:mcu=samd --output-dir /workspace/builds /workspace/source/Firmware/MK4duo/MK4duo.ino

# Interactive shell for debugging
docker run --rm -it -v $(pwd):/workspace/source:ro scribit-firmware-builder bash
```

## Output Files

Compiled firmware files are saved to `./docker/builds/` directory:

- **ESP32 Firmware:** `.bin`, `.hex`, `.elf` files
- **SAMD Firmware:** `.bin`, `.hex`, `.elf` files


## Board Configuration

The environment is pre-configured with:

- **ESP32 Board:** `briki:mbc-wb:mbc:mcu=esp`
- **SAMD Board:** `briki:mbc-wb:mbc:mcu=samd`
- **Libraries:** arduino-mqtt, StepperDriver
- **Hardware Overrides:** 8MB partition tables, SERCOM.cpp
- **Configuration Files:** SIConfig.hpp, Mk4duoVersion.h, ScribitVersion.hpp

## Troubleshooting

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

## Prerequisites

- Docker installed and running
- Run commands from the project root directory
- Ensure the following files exist:
  - `ExtraFile/SIConfig.hpp.example`
  - `ExtraFile/Mk4duoVersion.h.example` 
  - `ExtraFile/ScribitVersion.hpp.example`
  - `ExtraFile/arduino-mqtt/` directory
  - `ExtraFile/StepperDriver/` directory
