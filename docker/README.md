# Scribit Firmware Development Environment

This Docker environment provides a complete setup for compiling Scribit open firmware, including a Ubuntu desktop with VNC access and all necessary tools pre-configured.

## Features

- **Ubuntu Desktop with VNC**: Access a full Ubuntu desktop environment via web browser
- **Arduino Legacy IDE 1.8.19**: Pre-installed and configured
- **Board Support**: Briki MBC-WB v2.0.0 and ESP32 board definitions installed
- **Hardware Overrides**: Partition tables and SERCOM.cpp files properly configured
- **Libraries**: arduino-mqtt and StepperDriver libraries pre-copied
- **Configuration Files**: Example config files automatically copied to correct locations
- **Compilation Testing**: Built-in script to verify the setup works

## Quick Start

### 1. Build the Environment

```bash
# From the project root directory
./docker/build.sh
```

### 2. Start the Development Environment

```bash
cd docker
docker-compose up -d
```

### 3. Access the Desktop

Open your web browser and navigate to:
- **Web Interface**: http://localhost:6080
- **VNC Password**: `scribit123`

### 4. Test the Setup

Once inside the container, open a terminal and run:
```bash
/root/test_compilation.sh
```

This will verify that:
- All configuration files are in place
- Board definitions are installed
- Libraries are available
- Firmware can be compiled

## Using the Arduino IDE

1. **Access the IDE**: Double-click the Arduino IDE icon on the desktop
2. **Open Firmware Projects**:
   - ESP32 firmware: `/workspace/open-firmware/Firmware/ScribitESP/ScribitESP.ino`
   - SAMD firmware: `/workspace/open-firmware/Firmware/MK4duo/MK4duo.ino`

3. **Board Selection**:
   - For ESP32: `Tools` > `Board` > `Briki MBC-WB` > `Scribit ESP32`
   - For SAMD: `Tools` > `Board` > `Briki MBC-WB` > `Scribit SAMD`

## Configuration Files

The following configuration files are automatically set up:

- `Firmware/ScribitESP/SIConfig.hpp` (from `ExtraFile/SIConfig.hpp.example`)
- `Firmware/MK4duo/Mk4duoVersion.h` (from `ExtraFile/Mk4duoVersion.h.example`)
- `Firmware/ScribitESP/ScribitVersion.hpp` (from `ExtraFile/ScribitVersion.hpp.example`)

## Hardware Overrides

The following files are automatically copied to the correct Arduino15 locations:

- `8MB_ffat.csv` and `8MB_spiffs.csv` → partition tables
- `SERCOM.cpp` → SAMD core override

## Libraries

Pre-installed in the firmware directories:

- `arduino-mqtt` → `Firmware/ScribitESP/arduino-mqtt`
- `StepperDriver` → `Firmware/ScribitESP/StepperDriver`

## Troubleshooting

### Container Won't Start
```bash
# Check Docker is running
docker info

# Rebuild the image
docker-compose build --no-cache
```

### VNC Connection Issues
- Ensure port 6080 is not in use by another application
- Try accessing via direct VNC client on port 5900

### Compilation Errors
1. Run the test script: `/root/test_compilation.sh`
2. Check that all board definitions are installed:
   ```bash
   arduino-cli board listall | grep -i briki
   ```
3. Verify hardware override files are in place

### Permission Issues
The container runs as root to ensure all Arduino IDE operations work correctly.

## Development Workflow

1. **Start Environment**: `docker-compose up -d`
2. **Access via Browser**: http://localhost:6080
3. **Open Arduino IDE**: Desktop shortcut
4. **Load Firmware**: Open `.ino` files from `/workspace/open-firmware/Firmware/`
5. **Configure & Compile**: Select appropriate board and compile
6. **Stop Environment**: `docker-compose down`

## Advanced Usage

### Custom Resolution
```bash
docker-compose run -e RESOLUTION=1920x1200 scribit-firmware-dev
```

### USB Device Access (for flashing)
Add USB device mapping to docker-compose.yml:
```yaml
devices:
  - /dev/ttyUSB0:/dev/ttyUSB0
```

### Persistent Arduino Settings
Arduino preferences and installed libraries persist in the container.

## Files Created

This setup creates the following files in the `docker/` directory:

- `Dockerfile` - Main container definition
- `docker-compose.yml` - Service orchestration
- `build.sh` - Build script
- `README.md` - This documentation

## Based On

- Base Image: [wenoptics/ubuntu-desktop-novnc](https://github.com/wenoptics/ubuntu-desktop-novnc)
- Arduino Legacy IDE 1.8.19
- Briki MBC-WB board support v2.0.0
